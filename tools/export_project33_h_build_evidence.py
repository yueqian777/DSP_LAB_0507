#!/usr/bin/env python3
"""Export verified Project 3.3 H production-build evidence.

The exporter is read-only with respect to the build inputs.  It accepts a
completed H build and derives evidence from the TI map, link-info XML, and
nm6x symbol table instead of estimating target memory from source code.
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import re
import subprocess
import sys
import xml.etree.ElementTree as ET
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


PROFILE_NAME = "H_project33_full"
FRAMEBUFFER_NAMES = ("Lcd_Buffer", "EQ_LcdEditorBuffer")
GENERIC_CAPTURE_NAMES = (
    "EQ_CaptureInput",
    "EQ_CaptureOutput",
    "EQ_TriggerCaptureInput",
    "EQ_TriggerCaptureOutput",
)
LCD_TIMING_COMPILED_SYMBOL = "EQ_DebugLcdTimingCaptureCompiled"
KEY_SECTIONS = (".text", ".const", ".bss", ".subband_l2")

REQUIRED_MACROS = {
    "DSP_LAB_PROJECT_SELECT": 33,
    "EQ_ENABLE_AUDIO_FEATURE_ANALYZER": 1,
    "EQ_ENABLE_SMART_BASS": 1,
    "EQ_ENABLE_DYNAMIC_CLARITY": 1,
    "EQ_ENABLE_HARSHNESS_GUARD": 1,
    "EQ_ENABLE_LCD_DISPLAY": 1,
    "EQ_ENABLE_PROJECT33_TOUCH": 1,
    "EQ_ENABLE_TEN_BAND_EDITOR": 1,
    "EQ_ENABLE_TEN_BAND_EDITOR_TOUCH": 1,
}

# These gates allocate board-test or measurement-only state when enabled.  A
# final production evidence build must state every value explicitly.
REQUIRED_OFF_MACROS = (
    "EQ_ENABLE_FINAL_METRICS_BOARD_TEST",
    "EQ_ENABLE_DYNAMIC_CLARITY_TIMING_DIAGNOSTICS",
    "EQ_ENABLE_DYNAMIC_CLARITY_TRANSITION_CAPTURE",
    "EQ_ENABLE_HARSHNESS_GUARD_TRANSITION_CAPTURE",
    "EQ_ENABLE_HARSHNESS_GUARD_KERNEL_DIAGNOSTICS",
    "EQ_ENABLE_FOUR_WAY_TRANSITION_DIAGNOSTICS",
    "EQ_ENABLE_DYNAMIC_CLARITY_BENCHMARK",
    "EQ_ENABLE_HARSHNESS_GUARD_BENCHMARK",
    "EQ_ENABLE_LCD_JOB_TIMING_CAPTURE",
)

# These macros use #ifdef branches; defining either one to zero still selects
# host/offline code and is therefore forbidden in a production build.
FORBIDDEN_DEFINED_MACROS = ("EQ_ALGO_ONLY", "EQUALIZER_TEST_MAIN")

STATE_SYMBOLS = {
    "analyzer_state": ("EQ_AudioAnalyzerState",),
    "analyzer_input": ("EQ_AnalyzerInput",),
    "smart_bass": ("EQ_SmartBassState",),
    "dynamic_clarity": ("EQ_DynamicClarityState",),
    "harshness_guard": ("EQ_HarshnessGuardState",),
    "ui": ("s_ui_state",),
    "touch_total": ("EQ_TouchState", "EQ_TouchTransform"),
    "editor": ("EQ_UiEditorState",),
}

FORBIDDEN_SYMBOL_RULES = (
    ("final_metrics", re.compile(r"^EQ_FinalMetrics")),
    (
        "benchmark",
        re.compile(
            r"(?:^EQ_Benchmark|Benchmark(?:FirstCall|WarmMax|Checksum|Cycles))"
        ),
    ),
    (
        "transition_capture",
        re.compile(
            r"(?:TransitionCapture|^EQ_DebugFourWayTransition).*"
            r"(?:Samples|Input|Output|Buffer|Trace|Frames|Cycles)$"
        ),
    ),
    (
        "timing_capture",
        re.compile(
            r"(?:^EQ_DebugLcdTiming(?!CaptureCompiled$)|"
            r"TimingSamples|LcdJobTimingSamples|"
            r"^EQ_DebugDynamicClarityTiming)"
        ),
    ),
)

WARNING_RE = re.compile(
    r"warning\s+#\d+|:\s*warning(?:\s|:)", re.IGNORECASE
)
ERROR_RE = re.compile(
    r"error\s+#\d+|:\s*error(?:\s|:)|undefined\s+symbol",
    re.IGNORECASE,
)


class EvidenceError(RuntimeError):
    """Raised when the supplied build cannot support a PASS evidence set."""


def _require_file(path: Path | str, label: str) -> Path:
    resolved = Path(path).expanduser().resolve()
    if not resolved.is_file():
        raise EvidenceError(f"{label} not found: {resolved}")
    return resolved


def _hex_int(value: str | None, label: str) -> int:
    if value is None or not value.strip():
        raise EvidenceError(f"Missing hexadecimal value for {label}")
    try:
        return int(value.strip(), 0)
    except ValueError as exc:
        raise EvidenceError(f"Invalid hexadecimal value for {label}: {value}") from exc


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _artifact_record(path: Path) -> dict[str, Any]:
    return {
        "path": str(path),
        "size_bytes": path.stat().st_size,
        "sha256": _sha256(path),
    }


def _load_h_profile(path: Path) -> dict[str, Any]:
    try:
        payload = json.loads(path.read_text(encoding="utf-8-sig"))
    except (OSError, json.JSONDecodeError) as exc:
        raise EvidenceError(f"Cannot parse build matrix JSON: {path}: {exc}") from exc

    if isinstance(payload, list):
        profiles = payload
    elif isinstance(payload, dict) and isinstance(payload.get("profiles"), list):
        profiles = payload["profiles"]
    elif isinstance(payload, dict):
        profiles = [payload]
    else:
        raise EvidenceError("Build matrix JSON must be an object or array")

    matches = [
        item
        for item in profiles
        if isinstance(item, dict)
        and item.get("profile", item.get("name")) == PROFILE_NAME
    ]
    if len(matches) != 1:
        raise EvidenceError(
            f"Expected exactly one {PROFILE_NAME} profile, found {len(matches)}"
        )
    return matches[0]


def _integer_field(record: dict[str, Any], names: tuple[str, ...]) -> int:
    for name in names:
        if name in record:
            try:
                return int(record[name])
            except (TypeError, ValueError) as exc:
                raise EvidenceError(f"Invalid build matrix field {name}") from exc
    raise EvidenceError(f"Build matrix is missing {names[0]}")


def _parse_defines(raw: Any) -> dict[str, str]:
    if not isinstance(raw, str):
        raise EvidenceError("H profile defines must be a string")
    pattern = re.compile(
        r"(?<!\S)(?:--define(?:=|\s+)|-D)"
        r"([A-Za-z_][A-Za-z0-9_]*)(?:=([^\s]+))?"
    )
    values: dict[str, str] = {}
    for name, value in pattern.findall(raw):
        normalized = value if value else "1"
        if name in values and values[name] != normalized:
            raise EvidenceError(
                f"Conflicting definitions for {name}: {values[name]} and {normalized}"
            )
        values[name] = normalized
    if not values:
        raise EvidenceError("No --define options found in H profile")
    return values


def _macro_integer(macros: dict[str, str], name: str) -> int:
    if name not in macros:
        raise EvidenceError(f"Required production macro is not explicit: {name}")
    raw = macros[name].strip()
    try:
        return int(raw, 0)
    except ValueError as exc:
        raise EvidenceError(f"Production macro {name} is not numeric: {raw}") from exc


def _validate_macros(profile: dict[str, Any]) -> dict[str, Any]:
    macros = _parse_defines(profile.get("defines"))
    for name, expected in REQUIRED_MACROS.items():
        actual = _macro_integer(macros, name)
        if actual != expected:
            raise EvidenceError(
                f"Production macro {name}={actual}, expected {expected}"
            )
    for name in REQUIRED_OFF_MACROS:
        actual = _macro_integer(macros, name)
        if actual != 0:
            raise EvidenceError(f"Production measurement macro {name} must be OFF")
    for name in FORBIDDEN_DEFINED_MACROS:
        if name in macros:
            raise EvidenceError(
                f"Offline-only macro must not be defined in production: {name}"
            )
    return {
        "required_values": {**REQUIRED_MACROS},
        "required_off": {name: 0 for name in REQUIRED_OFF_MACROS},
        "forbidden_defined": list(FORBIDDEN_DEFINED_MACROS),
        "observed_defines": macros,
        "result": "PASS",
    }


def _diagnostics_from_log(path: Path) -> dict[str, Any]:
    text = path.read_text(encoding="utf-8-sig", errors="replace")
    warning_lines = [line for line in text.splitlines() if WARNING_RE.search(line)]
    error_lines = [line for line in text.splitlines() if ERROR_RE.search(line)]
    return {
        "warning_count": len(warning_lines),
        "error_count": len(error_lines),
        "warning_lines": warning_lines,
        "error_lines": error_lines,
    }


def _parse_memory_regions(path: Path) -> list[dict[str, Any]]:
    text = path.read_text(encoding="utf-8-sig", errors="replace")
    try:
        block = text.split("MEMORY CONFIGURATION", 1)[1].split(
            "SEGMENT ALLOCATION MAP", 1
        )[0]
    except IndexError as exc:
        raise EvidenceError(f"Map is missing MEMORY CONFIGURATION: {path}") from exc
    pattern = re.compile(
        r"^\s*(\S+)\s+([0-9a-fA-F]{8})\s+([0-9a-fA-F]{8,9})\s+"
        r"([0-9a-fA-F]{8,9})\s+([0-9a-fA-F]{8,9})\s+(\S+)",
        re.MULTILINE,
    )
    regions: list[dict[str, Any]] = []
    for name, origin, length, used, unused, attributes in pattern.findall(block):
        capacity = int(length, 16)
        used_bytes = int(used, 16)
        unused_bytes = int(unused, 16)
        if used_bytes + unused_bytes != capacity:
            raise EvidenceError(f"Map region {name} used + unused does not equal length")
        origin_int = int(origin, 16)
        regions.append(
            {
                "name": name,
                "origin_int": origin_int,
                "origin": f"0x{origin_int:08x}",
                "capacity_bytes": capacity,
                "used_bytes": used_bytes,
                "unused_bytes": unused_bytes,
                "utilization_percent": round(
                    100.0 * used_bytes / capacity, 9
                )
                if capacity
                else 0.0,
                "attributes": attributes,
            }
        )
    if not regions:
        raise EvidenceError(f"No memory regions parsed from map: {path}")
    return regions


def _parse_map_sections(path: Path) -> dict[str, dict[str, int]]:
    text = path.read_text(encoding="utf-8-sig", errors="replace")
    try:
        block = text.split("SEGMENT ALLOCATION MAP", 1)[1].split(
            "SECTION ALLOCATION MAP", 1
        )[0]
    except IndexError as exc:
        raise EvidenceError(f"Map is missing SEGMENT ALLOCATION MAP: {path}") from exc
    pattern = re.compile(
        r"^\s*([0-9a-fA-F]+)\s+([0-9a-fA-F]+)\s+"
        r"([0-9a-fA-F]+)\s+([0-9a-fA-F]+)\s+\S+\s+(\S+)\s*$",
        re.MULTILINE,
    )
    result: dict[str, dict[str, int]] = {}
    for run, _load, size, _init_size, name in pattern.findall(block):
        if name in result:
            raise EvidenceError(f"Duplicate segment section in map: {name}")
        result[name] = {"run_address": int(run, 16), "size_bytes": int(size, 16)}
    for name in KEY_SECTIONS:
        if name not in result:
            raise EvidenceError(f"Map segment section not found: {name}")
    return result


def _find_region(
    address: int, size: int, regions: list[dict[str, Any]]
) -> dict[str, Any] | None:
    for region in regions:
        start = region["origin_int"]
        end = start + region["capacity_bytes"]
        if start <= address and address + max(size, 1) <= end:
            return region
    return None


def _parse_link_info(
    path: Path, regions: list[dict[str, Any]]
) -> dict[str, Any]:
    try:
        root = ET.parse(path).getroot()
    except (ET.ParseError, OSError) as exc:
        raise EvidenceError(f"Cannot parse TI link-info XML: {path}: {exc}") from exc

    link_errors = _hex_int(root.findtext("link_errors"), "link_errors")
    object_components = {
        component.get("id"): component
        for component in root.findall(".//object_component")
        if component.get("id")
    }
    sections: list[dict[str, Any]] = []
    for group in root.findall("./logical_group_list/logical_group"):
        name = (group.findtext("name") or "").strip()
        size_text = group.findtext("size")
        run_text = group.findtext("run_address")
        if (
            not name
            or name == "NEARDP_DATA"
            or name.startswith(".debug_")
            or not size_text
            or not run_text
        ):
            continue
        size = _hex_int(size_text, f"{name} size")
        address = _hex_int(run_text, f"{name} run_address")
        region = _find_region(address, size, regions)
        if size == 0 or region is None:
            continue
        load_text = group.findtext("load_address")
        component_refs = [
            reference.get("idref")
            for reference in group.findall("./contents/object_component_ref")
            if reference.get("idref")
        ]
        components = [
            object_components[reference]
            for reference in component_refs
            if reference in object_components
        ]
        if component_refs and len(components) != len(component_refs):
            raise EvidenceError(
                f"Unresolved object-component reference in logical group {name}"
            )
        if components:
            readonly = all(
                component.findtext("readonly") == "true"
                for component in components
            )
            executable = any(
                component.findtext("executable") == "true"
                for component in components
            )
        else:
            readonly = group.findtext("readonly") == "true"
            executable = group.findtext("executable") == "true"
        sections.append(
            {
                "section_name": name,
                "run_address_int": address,
                "run_address": f"0x{address:08x}",
                "end_address_exclusive": f"0x{address + size:08x}",
                "load_address": (
                    f"0x{_hex_int(load_text, f'{name} load_address'):08x}"
                    if load_text
                    else ""
                ),
                "size_bytes": size,
                "size_kib": round(size / 1024.0, 6),
                "memory_region": region["name"],
                "region_origin": region["origin"],
                "region_capacity_bytes": region["capacity_bytes"],
                "region_used_bytes": region["used_bytes"],
                "region_unused_bytes": region["unused_bytes"],
                "region_utilization_percent": region["utilization_percent"],
                "readonly": readonly,
                "executable": executable,
                "evidence_source": "TI_LINK_INFO_XML_AND_MAP_REGION",
            }
        )

    if not sections:
        raise EvidenceError("No allocated logical groups parsed from link-info XML")
    names: dict[str, list[dict[str, Any]]] = {}
    for section in sections:
        names.setdefault(section["section_name"], []).append(section)
    for name in KEY_SECTIONS:
        if len(names.get(name, [])) != 1:
            raise EvidenceError(
                f"Expected one allocated XML logical group {name}, "
                f"found {len(names.get(name, []))}"
            )

    xml_symbols: dict[str, list[int]] = {}
    for symbol in root.findall("./symbol_table/symbol"):
        name = (symbol.findtext("name") or "").strip()
        value = symbol.findtext("value")
        if name and value:
            xml_symbols.setdefault(name, []).append(
                _hex_int(value, f"symbol {name}")
            )
    return {
        "link_errors": link_errors,
        "linker_banner": root.findtext("banner", ""),
        "linked_output_file": root.findtext("output_file", ""),
        "sections": sorted(
            sections, key=lambda item: (item["run_address_int"], item["section_name"])
        ),
        "sections_by_name": names,
        "symbols": xml_symbols,
    }


def _run_nm6x(nm6x_path: Path, out_path: Path) -> tuple[str, list[dict[str, Any]]]:
    command = [str(nm6x_path), "-l", str(out_path)]
    try:
        completed = subprocess.run(
            command,
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            check=False,
        )
    except OSError as exc:
        raise EvidenceError(f"Cannot execute nm6x: {nm6x_path}: {exc}") from exc
    if completed.returncode != 0:
        detail = completed.stderr.strip() or completed.stdout.strip()
        raise EvidenceError(
            f"nm6x failed with exit code {completed.returncode}: {detail}"
        )
    pattern = re.compile(
        r"^\[\d+\]\s+\|0x([0-9a-fA-F]+)\|(\d+)\|([^|]+)\|"
        r"([^|]+)\|[^|]*\|[^|]*\|(.+?)\s*$"
    )
    symbols: list[dict[str, Any]] = []
    for line in completed.stdout.splitlines():
        match = pattern.match(line)
        if not match:
            continue
        symbols.append(
            {
                "address_int": int(match.group(1), 16),
                "address": f"0x{int(match.group(1), 16):08x}",
                "size_bytes": int(match.group(2), 10),
                "bind": match.group(3).strip(),
                "type": match.group(4).strip(),
                "name": match.group(5).strip(),
            }
        )
    if not symbols:
        raise EvidenceError("nm6x returned no parseable long-format symbols")
    return " ".join(command), symbols


def _object_symbols(symbols: list[dict[str, Any]]) -> list[dict[str, Any]]:
    return [item for item in symbols if item["type"] in {"OBJT", "COMN"}]


def _one_object_symbol(
    symbols: list[dict[str, Any]], name: str, required: bool = True
) -> dict[str, Any] | None:
    matches = [item for item in _object_symbols(symbols) if item["name"] == name]
    if not matches and not required:
        return None
    if len(matches) != 1:
        raise EvidenceError(
            f"Expected one object symbol {name}, found {len(matches)}"
        )
    return matches[0]


def _state_sizes(symbols: list[dict[str, Any]]) -> dict[str, int]:
    result: dict[str, int] = {}
    for label, names in STATE_SYMBOLS.items():
        result[label] = sum(
            int(_one_object_symbol(symbols, name)["size_bytes"]) for name in names
        )
    result["analyzer_total"] = (
        result["analyzer_state"] + result["analyzer_input"]
    )
    result["ui_touch_editor_total"] = (
        result["ui"] + result["touch_total"] + result["editor"]
    )
    return result


def _framebuffer_evidence(
    symbols: list[dict[str, Any]],
    link_info: dict[str, Any],
    regions: list[dict[str, Any]],
    profile: dict[str, Any],
) -> dict[str, Any]:
    buffers = []
    for name in FRAMEBUFFER_NAMES:
        symbol = _one_object_symbol(symbols, name)
        xml_addresses = link_info["symbols"].get(name, [])
        if len(xml_addresses) != 1:
            raise EvidenceError(
                f"Expected one XML symbol {name}, found {len(xml_addresses)}"
            )
        if xml_addresses[0] != symbol["address_int"]:
            raise EvidenceError(f"XML/nm framebuffer address mismatch for {name}")
        region = _find_region(symbol["address_int"], symbol["size_bytes"], regions)
        if region is None:
            raise EvidenceError(f"Framebuffer {name} is outside mapped memory")
        buffers.append(
            {
                "name": name,
                "start_address": symbol["address"],
                "start_address_int": symbol["address_int"],
                "end_address_inclusive": (
                    f"0x{symbol['address_int'] + symbol['size_bytes'] - 1:08x}"
                ),
                "end_address_exclusive": (
                    f"0x{symbol['address_int'] + symbol['size_bytes']:08x}"
                ),
                "size_bytes": symbol["size_bytes"],
                "memory_region": region["name"],
                "nm_type": symbol["type"],
                "nm_bind": symbol["bind"],
                "xml_symbol_address": f"0x{xml_addresses[0]:08x}",
            }
        )
    buffers.sort(key=lambda item: item["start_address_int"])
    first_end = buffers[0]["start_address_int"] + buffers[0]["size_bytes"]
    second_start = buffers[1]["start_address_int"]
    if first_end > second_start:
        raise EvidenceError(
            f"Framebuffer overlap: {buffers[0]['name']} ends at 0x{first_end:08x}, "
            f"{buffers[1]['name']} starts at 0x{second_start:08x}"
        )

    offscreen_groups = link_info["sections_by_name"].get("offscreen_buffer", [])
    if len(offscreen_groups) != 1:
        raise EvidenceError(
            "Expected one offscreen_buffer logical group in link-info XML"
        )
    offscreen = offscreen_groups[0]
    offscreen_start = offscreen["run_address_int"]
    offscreen_end = offscreen_start + offscreen["size_bytes"]
    for buffer in buffers:
        start = buffer["start_address_int"]
        end = start + buffer["size_bytes"]
        if start < offscreen_start or end > offscreen_end:
            raise EvidenceError(
                f"Framebuffer {buffer['name']} is outside offscreen_buffer section"
            )

    expected_count = profile.get("framebuffer_symbol_count")
    if expected_count is not None and int(expected_count) != len(buffers):
        raise EvidenceError("Build matrix framebuffer count does not match nm6x")
    expected_names = profile.get("framebuffer_symbol_names")
    if expected_names is not None and sorted(expected_names) != sorted(
        FRAMEBUFFER_NAMES
    ):
        raise EvidenceError("Build matrix framebuffer names do not match H profile")

    for buffer in buffers:
        buffer.pop("start_address_int")
    return {
        "schema_version": 1,
        "evidence_class": "BUILD_EVIDENCE",
        "result": "PASS",
        "framebuffer_count": len(buffers),
        "non_overlapping": True,
        "gap_bytes": second_start - first_end,
        "total_framebuffer_bytes": sum(item["size_bytes"] for item in buffers),
        "offscreen_buffer_section": {
            "start_address": offscreen["run_address"],
            "end_address_exclusive": offscreen["end_address_exclusive"],
            "size_bytes": offscreen["size_bytes"],
            "memory_region": offscreen["memory_region"],
        },
        "framebuffers": buffers,
        "address_evidence": "nm6x sizes/addresses cross-checked with linkInfo.xml symbols",
    }


def _symbol_audit(
    symbols: list[dict[str, Any]],
    nm_command: str,
    link_info: dict[str, Any],
) -> tuple[str, dict[str, Any]]:
    object_symbols = _object_symbols(symbols)
    compiled = _one_object_symbol(symbols, LCD_TIMING_COMPILED_SYMBOL)
    if compiled["size_bytes"] != 4:
        raise EvidenceError(
            f"{LCD_TIMING_COMPILED_SYMBOL} must be a 4-byte unsigned int"
        )
    compiled_sections = [
        section
        for section in link_info["sections"]
        if section["run_address_int"] <= compiled["address_int"]
        and compiled["address_int"] + compiled["size_bytes"]
        <= section["run_address_int"] + section["size_bytes"]
    ]
    if len(compiled_sections) != 1 or not compiled_sections[0]["readonly"]:
        raise EvidenceError(
            f"{LCD_TIMING_COMPILED_SYMBOL} is not proven to be in one read-only "
            "linker section"
        )
    compiled_section = compiled_sections[0]
    generic = []
    for name in GENERIC_CAPTURE_NAMES:
        symbol = _one_object_symbol(symbols, name, required=False)
        if symbol is not None:
            generic.append(symbol)

    forbidden = []
    generic_names = {item["name"] for item in generic}
    for symbol in object_symbols:
        if symbol["name"] in generic_names:
            continue
        for category, pattern in FORBIDDEN_SYMBOL_RULES:
            if pattern.search(symbol["name"]):
                forbidden.append({"category": category, **symbol})
                break
    forbidden.sort(key=lambda item: (item["category"], item["name"]))
    if forbidden:
        names = ", ".join(item["name"] for item in forbidden)
        raise EvidenceError(f"Forbidden production measurement symbols: {names}")

    lines = [
        "Project 3.3 H production symbol audit",
        "result=PASS",
        f"nm6x_command={nm_command}",
        f"parsed_symbol_count={len(symbols)}",
        f"parsed_object_symbol_count={len(object_symbols)}",
        "forbidden_measurement_symbols=NONE",
        "lcd_timing_compiled_state=PASS",
        f"{LCD_TIMING_COMPILED_SYMBOL} address={compiled['address']} "
        f"size_bytes={compiled['size_bytes']} "
        f"readonly_section={compiled_section['section_name']} "
        "macro=EQ_ENABLE_LCD_JOB_TIMING_CAPTURE=0",
        "generic_capture_buffers=DISCLOSED_NOT_FAILURE",
    ]
    for symbol in generic:
        lines.append(
            f"{symbol['name']} address={symbol['address']} "
            f"size_bytes={symbol['size_bytes']}"
        )
    generic_total = sum(item["size_bytes"] for item in generic)
    lines.extend(
        (
            f"generic_capture_total_bytes={generic_total}",
            "generic_capture_note=These general diagnostic capture buffers are "
            "reported separately and are not final-metrics, benchmark, transition-"
            "capture, or timing-capture arrays.",
        )
    )
    return "\n".join(lines) + "\n", {
        "forbidden_measurement_symbol_count": 0,
        "lcd_timing_compiled_state": {
            "name": LCD_TIMING_COMPILED_SYMBOL,
            "address": compiled["address"],
            "size_bytes": compiled["size_bytes"],
            "readonly_section": compiled_section["section_name"],
            "macro": "EQ_ENABLE_LCD_JOB_TIMING_CAPTURE=0",
        },
        "generic_capture_buffer_count": len(generic),
        "generic_capture_total_bytes": generic_total,
        "generic_capture_buffers": [
            {
                "name": item["name"],
                "address": item["address"],
                "size_bytes": item["size_bytes"],
            }
            for item in generic
        ],
    }


def _validate_section_cross_checks(
    profile: dict[str, Any],
    map_sections: dict[str, dict[str, int]],
    link_info: dict[str, Any],
) -> dict[str, int]:
    result: dict[str, int] = {}
    for section_name in KEY_SECTIONS:
        xml_section = link_info["sections_by_name"][section_name][0]
        map_section = map_sections[section_name]
        if xml_section["size_bytes"] != map_section["size_bytes"]:
            raise EvidenceError(f"Map/XML size mismatch for {section_name}")
        if xml_section["run_address_int"] != map_section["run_address"]:
            raise EvidenceError(f"Map/XML address mismatch for {section_name}")
        matrix_field = section_name.lstrip(".") + "_bytes"
        matrix_size = _integer_field(profile, (matrix_field,))
        if matrix_size != xml_section["size_bytes"]:
            raise EvidenceError(
                f"Build matrix {matrix_field} does not match map/XML"
            )
        result[section_name] = xml_section["size_bytes"]
    return result


def _write_json(path: Path, payload: Any) -> None:
    path.write_text(
        json.dumps(payload, indent=2, ensure_ascii=False) + "\n",
        encoding="utf-8",
    )


def _write_memory_sections(path: Path, sections: list[dict[str, Any]]) -> None:
    fields = (
        "section_name",
        "run_address",
        "end_address_exclusive",
        "load_address",
        "size_bytes",
        "size_kib",
        "memory_region",
        "region_origin",
        "region_capacity_bytes",
        "region_used_bytes",
        "region_unused_bytes",
        "region_utilization_percent",
        "readonly",
        "executable",
        "evidence_source",
    )
    with path.open("w", newline="", encoding="utf-8-sig") as stream:
        writer = csv.DictWriter(stream, fieldnames=fields)
        writer.writeheader()
        for section in sections:
            writer.writerow({name: section[name] for name in fields})


def export_evidence(
    *,
    build_matrix_path: Path | str,
    map_path: Path | str,
    link_info_path: Path | str,
    out_path: Path | str,
    build_log_path: Path | str,
    nm6x_path: Path | str,
    output_dir: Path | str,
) -> dict[str, Any]:
    matrix = _require_file(build_matrix_path, "build matrix summary")
    map_file = _require_file(map_path, "H map")
    link_file = _require_file(link_info_path, "linkInfo.xml")
    output_file = _require_file(out_path, ".out")
    build_log = _require_file(build_log_path, "build log")
    nm_tool = _require_file(nm6x_path, "nm6x")
    destination = Path(output_dir).expanduser().resolve()
    if destination.exists() and not destination.is_dir():
        raise EvidenceError(f"Output path is not a directory: {destination}")

    profile = _load_h_profile(matrix)
    if _integer_field(profile, ("exit_code",)) != 0:
        raise EvidenceError("H build exit_code is nonzero")
    matrix_warnings = _integer_field(
        profile, ("warning_count", "warnings", "warning")
    )
    matrix_errors = _integer_field(profile, ("error_count", "errors", "error"))
    if matrix_warnings != 0:
        raise EvidenceError(f"H build warning_count is {matrix_warnings}, expected 0")
    if matrix_errors != 0:
        raise EvidenceError(f"H build error_count is {matrix_errors}, expected 0")
    matrix_link_errors = _hex_int(
        str(profile.get("link_errors", "")), "build matrix link_errors"
    )
    if matrix_link_errors != 0:
        raise EvidenceError("H build matrix link_errors is nonzero")

    log_diagnostics = _diagnostics_from_log(build_log)
    if log_diagnostics["warning_count"] != 0:
        raise EvidenceError(
            f"Build log contains {log_diagnostics['warning_count']} warning diagnostic(s)"
        )
    if log_diagnostics["error_count"] != 0:
        raise EvidenceError(
            f"Build log contains {log_diagnostics['error_count']} error diagnostic(s)"
        )

    macro_audit = _validate_macros(profile)
    regions = _parse_memory_regions(map_file)
    map_sections = _parse_map_sections(map_file)
    link_info = _parse_link_info(link_file, regions)
    if link_info["link_errors"] != 0:
        raise EvidenceError(
            f"linkInfo.xml link_errors=0x{link_info['link_errors']:x}, expected 0x0"
        )
    section_sizes = _validate_section_cross_checks(
        profile, map_sections, link_info
    )

    out_size = output_file.stat().st_size
    if "out_size" in profile and int(profile["out_size"]) != out_size:
        raise EvidenceError("Build matrix out_size does not match supplied .out")
    out_hash = _sha256(output_file)
    if "out_sha256" in profile and str(profile["out_sha256"]).lower() != out_hash:
        raise EvidenceError("Build matrix out_sha256 does not match supplied .out")

    nm_command, symbols = _run_nm6x(nm_tool, output_file)
    states = _state_sizes(symbols)
    framebuffers = _framebuffer_evidence(
        symbols, link_info, regions, profile
    )
    symbol_text, symbol_summary = _symbol_audit(
        symbols, nm_command, link_info
    )

    ddr_matches = [region for region in regions if region["name"] == "DDR2"]
    if len(ddr_matches) != 1:
        raise EvidenceError(f"Expected one DDR2 map region, found {len(ddr_matches)}")
    ddr = ddr_matches[0]
    ddr_summary = {
        key: ddr[key]
        for key in (
            "origin",
            "capacity_bytes",
            "used_bytes",
            "unused_bytes",
            "utilization_percent",
        )
    }

    inputs = {
        "build_matrix_summary": _artifact_record(matrix),
        "map": _artifact_record(map_file),
        "link_info_xml": _artifact_record(link_file),
        "out": _artifact_record(output_file),
        "build_log": _artifact_record(build_log),
        "nm6x": _artifact_record(nm_tool),
    }
    summary = {
        "schema_version": 1,
        "generated_at_utc": datetime.now(timezone.utc).isoformat(),
        "stage": "build",
        "status": "MEASURED",
        "measurement_label": "BUILD_EVIDENCE",
        "evidence_class": "BUILD_EVIDENCE",
        "result": "PASS",
        "profile": PROFILE_NAME,
        "warning_count": 0,
        "error_count": 0,
        "link_errors": "0x0",
        "diagnostic_evidence": {
            "build_matrix_warning_count": matrix_warnings,
            "build_matrix_error_count": matrix_errors,
            "build_log_warning_count": log_diagnostics["warning_count"],
            "build_log_error_count": log_diagnostics["error_count"],
        },
        "macro_audit": macro_audit,
        "sections_bytes": section_sizes,
        "ddr": ddr_summary,
        "state_sizes_bytes": states,
        "framebuffer_summary": {
            "count": framebuffers["framebuffer_count"],
            "non_overlapping": framebuffers["non_overlapping"],
            "gap_bytes": framebuffers["gap_bytes"],
            "total_bytes": framebuffers["total_framebuffer_bytes"],
        },
        "production_symbol_audit": symbol_summary,
        "output": {"size_bytes": out_size, "sha256": out_hash},
        "linker_banner": link_info["linker_banner"],
        "linked_output_file_recorded_by_linker": link_info["linked_output_file"],
        "inputs": inputs,
        "memory_note": (
            "Target memory values come from TI map/XML run-time allocations; "
            ".out file size is not used as RAM."
        ),
    }

    destination.mkdir(parents=True, exist_ok=True)
    _write_json(destination / "build_summary.json", summary)
    _write_memory_sections(
        destination / "memory_sections.csv", link_info["sections"]
    )
    _write_json(destination / "framebuffer_map.json", framebuffers)
    (destination / "production_symbol_audit.txt").write_text(
        symbol_text, encoding="utf-8"
    )
    return summary


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Export verified Project 3.3 H production-build evidence"
    )
    parser.add_argument("--build-matrix", required=True, type=Path)
    parser.add_argument("--map", required=True, dest="map_path", type=Path)
    parser.add_argument("--link-info", required=True, type=Path)
    parser.add_argument("--out", required=True, type=Path)
    parser.add_argument("--build-log", required=True, type=Path)
    parser.add_argument("--nm6x", required=True, type=Path)
    parser.add_argument("--output-dir", required=True, type=Path)
    return parser


def main(argv: list[str] | None = None) -> int:
    args = _parser().parse_args(argv)
    try:
        summary = export_evidence(
            build_matrix_path=args.build_matrix,
            map_path=args.map_path,
            link_info_path=args.link_info,
            out_path=args.out,
            build_log_path=args.build_log,
            nm6x_path=args.nm6x,
            output_dir=args.output_dir,
        )
    except EvidenceError as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 1
    print(
        f"PASS profile={summary['profile']} warnings=0 link_errors=0x0 "
        f"output_dir={Path(args.output_dir).resolve()}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
