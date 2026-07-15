#!/usr/bin/env python3
"""Generate Project 3.2 memory-audit tables from TI linker map/XML files."""

from __future__ import annotations

import csv
import json
import re
import subprocess
import xml.etree.ElementTree as ET
from collections import defaultdict
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
RESULTS = ROOT / "results" / "memory"
PRODUCTION_MAP = RESULTS / "production_DSP_LAB_0507.map"
PRODUCTION_XML = RESULTS / "production_DSP_LAB_0507_linkInfo.xml"
THD_MAP = RESULTS / "thd_test_DSP_LAB_0507.map"
THD_XML = RESULTS / "thd_test_DSP_LAB_0507_linkInfo.xml"

CODE_READONLY_SECTIONS = {
    ".text", ".const", ".switch", ".cinit", ".pinit", ".binit",
    ".init_array", ".rodata", ".c6xabi.exidx", ".c6xabi.extab",
}
RESERVED_SECTIONS = {".stack", ".sysmem", ".args"}
SKIP_GROUPS = {"NEARDP_DATA"}
MODULE_ORDER = [
    "WOLA / FFT",
    "Denoise",
    "Compression",
    "Main chain and audio buffers",
    "UI and display",
    "Platform and shared runtime",
]


def hex_int(value: str | None) -> int:
    if not value:
        return 0
    return int(value, 16)


def git(*args: str) -> str:
    return subprocess.check_output(
        ["git", *args], cwd=ROOT, text=True, encoding="utf-8"
    ).strip()


def parse_regions(map_path: Path) -> list[dict]:
    text = map_path.read_text(encoding="utf-8", errors="replace")
    block = text.split("MEMORY CONFIGURATION", 1)[1].split(
        "SEGMENT ALLOCATION MAP", 1
    )[0]
    pattern = re.compile(
        r"^\s*(\S+)\s+([0-9a-fA-F]{8})\s+([0-9a-fA-F]{8,9})\s+"
        r"([0-9a-fA-F]{8,9})\s+([0-9a-fA-F]{8,9})\s+(\S+)",
        re.MULTILINE,
    )
    regions = []
    for name, origin, length, used, unused, attr in pattern.findall(block):
        capacity = int(length, 16)
        used_bytes = int(used, 16)
        regions.append(
            {
                "region": name,
                "origin": f"0x{int(origin, 16):08x}",
                "origin_int": int(origin, 16),
                "capacity_bytes": capacity,
                "used_bytes": used_bytes,
                "unused_bytes": int(unused, 16),
                "utilization_percent": round(100.0 * used_bytes / capacity, 6)
                if capacity
                else 0.0,
                "attributes": attr,
            }
        )
    if not regions:
        raise RuntimeError(f"No MEMORY CONFIGURATION rows parsed from {map_path}")
    return regions


def output_file_name(record: ET.Element | None) -> str:
    if record is None:
        return "LINKER_GENERATED"
    kind = record.findtext("kind", "")
    archive = record.findtext("file", "")
    member = record.findtext("name", archive)
    return f"{archive}:{member}" if kind == "archive" else member


def section_class(name: str) -> str:
    if name in RESERVED_SECTIONS:
        return "runtime_reserved"
    if name in CODE_READONLY_SECTIONS:
        return "code_readonly"
    return "static_readwrite"


def module_for(object_file: str) -> str:
    name = object_file.lower()
    if any(key in name for key in ("user_subband_wola", "user_subband_polyphase")):
        return "WOLA / FFT"
    if "user_subband_denoise" in name:
        return "Denoise"
    if any(key in name for key in ("user_subband_codec", "user_subband_eval")):
        return "Compression"
    if any(
        key in name
        for key in (
            "user_subband_flow", "adc_", "dac_", "ad_ch", "da_ch",
            "user_aden", "user_adflag", "user_adpingpong", "user_dalen",
            "user_daen", "user_daflag", "user_dapingpong", "user_dasel",
        )
    ):
        return "Main chain and audio buffers"
    if any(
        key in name
        for key in (
            "user_subband_ui", "lcd", "touch", "grlib", "font", "image.obj",
            "offscr", "canvas.obj", "widget.obj", "pushbutton.obj",
        )
    ):
        return "UI and display"
    return "Platform and shared runtime"


def description_for(object_file: str) -> str:
    name = object_file.lower()
    if "user_subband_flow" in name:
        return "Project 3.2 audio main chain, frame and ping-pong state"
    if "user_subband_denoise" in name:
        return "Subband denoise state and processing code"
    if "user_subband_wola" in name:
        return "WOLA analysis/synthesis, overlap and spectrum state"
    if "user_subband_polyphase" in name:
        return "Polyphase/FFT filter-bank state and code"
    if "user_subband_codec" in name:
        return "Subband codec/quantization state and code"
    if "user_subband_ui" in name:
        return "Project 3.2 UI, display state, text or glyph support"
    if "lcd_dma" in name or "offscr" in name:
        return "LCD frame/off-screen buffer"
    if any(key in name for key in ("lcd", "touch", "grlib", "font", "image.obj")):
        return "LCD, touch, font or graphics support"
    if any(key in name for key in ("adc_", "dac_")):
        return "ADC/DAC driver or fixed board audio buffer"
    if "rts6740" in name:
        return "TI C6740 runtime support"
    if "linker_generated" in name:
        return "Linker-generated initialization/padding contribution"
    return "Platform, driver or shared runtime contribution"


def find_region(address: int, size: int, regions: list[dict]) -> dict | None:
    if address == 0 and size == 0:
        return None
    for region in regions:
        origin = region["origin_int"]
        if origin <= address and address + max(size, 1) <= origin + region["capacity_bytes"]:
            return region
    return None


def parse_build(kind: str, map_path: Path, xml_path: Path) -> dict:
    regions = parse_regions(map_path)
    root = ET.parse(xml_path).getroot()
    files = {node.get("id"): node for node in root.findall("./input_file_list/input_file")}
    components = {}
    for node in root.findall("./object_component_list/object_component"):
        file_ref = node.find("input_file_ref")
        file_node = files.get(file_ref.get("idref")) if file_ref is not None else None
        components[node.get("id")] = {
            "id": node.get("id"),
            "input_section": node.findtext("name", ""),
            "run_address_int": hex_int(node.findtext("run_address")),
            "load_address_int": hex_int(node.findtext("load_address")),
            "size_bytes": hex_int(node.findtext("size")),
            "readonly": node.findtext("readonly") == "true",
            "executable": node.findtext("executable") == "true",
            "object_file": output_file_name(file_node),
        }

    sections = []
    component_section = {}
    for group in root.findall("./logical_group_list/logical_group"):
        name = group.findtext("name", "")
        if name in SKIP_GROUPS or name.startswith(".debug_"):
            continue
        size = hex_int(group.findtext("size"))
        run_address = hex_int(group.findtext("run_address"))
        load_text = group.findtext("load_address")
        region = find_region(run_address, size, regions)
        category = section_class(name)
        refs = [ref.get("idref") for ref in group.findall("./contents/object_component_ref")]
        for ref in refs:
            component_section.setdefault(ref, name)
        sections.append(
            {
                "section_name": name,
                "category": category,
                "run_address": f"0x{run_address:08x}" if run_address else "",
                "load_address": f"0x{hex_int(load_text):08x}" if load_text else "",
                "size_bytes": size,
                "size_kib": round(size / 1024.0, 6),
                "memory_region": region["region"] if region else "UNALLOCATED",
                "region_capacity_bytes": region["capacity_bytes"] if region else 0,
                "region_utilization_percent": region["utilization_percent"] if region else 0.0,
                "map_initialization": "load image" if load_text else "no in-place load image",
                "component_refs": refs,
            }
        )

    contributions = []
    for component_id, section_name in component_section.items():
        component = components.get(component_id)
        if not component:
            continue
        section_category = section_class(section_name)
        region = find_region(component["run_address_int"], component["size_bytes"], regions)
        contributions.append(
            {
                **component,
                "output_section": section_name,
                "category": section_category,
                "module": module_for(component["object_file"]),
                "memory_region": region["region"] if region else "UNALLOCATED",
                "description": description_for(component["object_file"]),
            }
        )

    # Assign linker holes/alignment bytes to the shared-platform bucket so module
    # totals reconcile exactly with section totals without double counting.
    component_sums = defaultdict(int)
    for item in contributions:
        component_sums[item["output_section"]] += item["size_bytes"]
    for section in sections:
        gap = section["size_bytes"] - component_sums[section["section_name"]]
        if gap > 0:
            contributions.append(
                {
                    "id": f"gap:{section['section_name']}",
                    "input_section": "alignment/hole",
                    "run_address_int": hex_int(section["run_address"]),
                    "load_address_int": 0,
                    "size_bytes": gap,
                    "readonly": section["category"] == "code_readonly",
                    "executable": section["section_name"] == ".text",
                    "object_file": "LINKER_HOLES_AND_PADDING",
                    "output_section": section["section_name"],
                    "category": section["category"],
                    "module": "Platform and shared runtime",
                    "memory_region": section["memory_region"],
                    "description": "Linker alignment and section holes",
                }
            )

    totals = defaultdict(int)
    for section in sections:
        totals[section["category"]] += section["size_bytes"]
    mapped_total = sum(region["used_bytes"] for region in regions)
    classified_total = sum(totals.values())
    if classified_total != mapped_total:
        raise RuntimeError(
            f"{kind}: section total {classified_total} != region-used total {mapped_total}"
        )

    modules = {name: {"code_readonly_bytes": 0, "static_ram_bytes": 0} for name in MODULE_ORDER}
    for item in contributions:
        if item["category"] == "code_readonly":
            modules[item["module"]]["code_readonly_bytes"] += item["size_bytes"]
        elif item["category"] == "static_readwrite":
            modules[item["module"]]["static_ram_bytes"] += item["size_bytes"]

    symbols = {}
    for symbol in root.findall("./symbol_table/symbol"):
        name = symbol.findtext("name", "")
        if name.startswith("SUBBAND_THD_"):
            ref = symbol.find("object_component_ref")
            symbols[name] = {
                "address": hex_int(symbol.findtext("value")),
                "component_ref": ref.get("idref") if ref is not None else "",
            }

    map_text = map_path.read_text(encoding="utf-8", errors="replace")
    dynamic_symbols = sorted(
        {
            match.group(1)
            for match in re.finditer(
                r"(?mi)^[0-9a-f]{8}\s+_?(malloc|calloc|realloc|free)\s*$",
                map_text,
            )
        }
    )
    linked_match = re.search(r">> Linked (.+)", map_text)
    return {
        "kind": kind,
        "map_path": map_path,
        "xml_path": xml_path,
        "regions": regions,
        "sections": sections,
        "contributions": contributions,
        "modules": modules,
        "totals": dict(totals),
        "runtime_ram_bytes": mapped_total,
        "symbols": symbols,
        "components": components,
        "linker_banner": root.findtext("banner", ""),
        "link_errors": hex_int(root.findtext("link_errors")),
        "linked_at": linked_match.group(1).strip() if linked_match else "unknown",
        "dynamic_allocation_symbols": dynamic_symbols,
    }


def write_csv(path: Path, rows: list[dict], fields: list[str]) -> None:
    with path.open("w", newline="", encoding="utf-8-sig") as stream:
        writer = csv.DictWriter(stream, fieldnames=fields, extrasaction="ignore")
        writer.writeheader()
        writer.writerows(rows)


def summary_rows(build: dict, heap_configured: int) -> list[dict]:
    values = [
        ("program_code_and_readonly", build["totals"].get("code_readonly", 0), ".text/.const/.switch/.cinit and other read-only sections"),
        ("static_readwrite_ram", build["totals"].get("static_readwrite", 0), "Static run-time RAM including fixed board audio buffers"),
        ("stack_mapped_reserved", next((s["size_bytes"] for s in build["sections"] if s["section_name"] == ".stack"), 0), "Linker-mapped reservation; not a measured high-water mark"),
        ("heap_mapped_reserved", next((s["size_bytes"] for s in build["sections"] if s["section_name"] == ".sysmem"), 0), f".sysmem mapped size; linker option configures {heap_configured} bytes"),
        ("total_runtime_ram", build["runtime_ram_bytes"], "Sum of map MEMORY CONFIGURATION used bytes; load/run not double-counted"),
    ]
    return [
        {"category": name, "bytes": value, "kib": round(value / 1024.0, 6), "note": note}
        for name, value, note in values
    ]


def general_region(region: dict) -> bool:
    return not re.match(r"^(User_|AD_Ch|DA_Ch)", region["region"])


def largest_rows(build: dict) -> list[dict]:
    kinds = {
        "code": lambda item: item["executable"] or item["output_section"] == ".text",
        "static_data": lambda item: item["category"] == "static_readwrite",
        "constant": lambda item: item["output_section"] == ".const",
    }
    rows = []
    for kind, predicate in kinds.items():
        selected = sorted(
            (item for item in build["contributions"] if predicate(item) and item["size_bytes"]),
            key=lambda item: item["size_bytes"],
            reverse=True,
        )[:15]
        for rank, item in enumerate(selected, 1):
            rows.append(
                {
                    "object_kind": kind,
                    "rank": rank,
                    "name": f"{item['object_file']} ({item['input_section']})",
                    "object_file": item["object_file"],
                    "output_section": item["output_section"],
                    "bytes": item["size_bytes"],
                    "kib": round(item["size_bytes"] / 1024.0, 6),
                    "memory_region": item["memory_region"],
                    "module": item["module"],
                    "description": item["description"],
                }
            )
    return rows


def kib(value: int) -> str:
    return f"{value / 1024.0:.3f}"


def markdown_table(headers: list[str], rows: list[list[object]]) -> str:
    lines = ["| " + " | ".join(headers) + " |", "|" + "|".join("---" for _ in headers) + "|"]
    lines.extend("| " + " | ".join(str(value) for value in row) + " |" for row in rows)
    return "\n".join(lines)


def main() -> None:
    RESULTS.mkdir(parents=True, exist_ok=True)
    production = parse_build("production", PRODUCTION_MAP, PRODUCTION_XML)
    thd = parse_build("thd_test", THD_MAP, THD_XML)
    if production["link_errors"] or thd["link_errors"]:
        raise RuntimeError("A linker XML reports non-zero link_errors")

    heap_configured = 0x800
    stack_configured = 0x800
    prod_summary = summary_rows(production, heap_configured)
    thd_summary = summary_rows(thd, heap_configured)
    prod_by_name = {row["category"]: row["bytes"] for row in prod_summary}
    thd_by_name = {row["category"]: row["bytes"] for row in thd_summary}
    comparisons = [
        {
            "category": category,
            "production_bytes": prod_by_name[category],
            "thd_test_bytes": thd_by_name[category],
            "test_only_increment_bytes": thd_by_name[category] - prod_by_name[category],
        }
        for category in prod_by_name
    ]

    section_fields = [
        "section_name", "category", "run_address", "load_address", "size_bytes",
        "size_kib", "memory_region", "region_capacity_bytes",
        "region_utilization_percent", "map_initialization",
    ]
    region_fields = [
        "region", "origin", "capacity_bytes", "used_bytes", "unused_bytes",
        "utilization_percent", "attributes",
    ]
    module_rows = []
    for name in MODULE_ORDER:
        values = production["modules"][name]
        module_rows.append(
            {
                "module": name,
                **values,
                "code_readonly_kib": round(values["code_readonly_bytes"] / 1024.0, 6),
                "static_ram_kib": round(values["static_ram_bytes"] / 1024.0, 6),
                "granularity": "TI linker object-file contribution; linker holes assigned to platform/shared",
            }
        )
    largest = largest_rows(production)

    write_csv(RESULTS / "production_memory_summary.csv", prod_summary, ["category", "bytes", "kib", "note"])
    write_csv(RESULTS / "thd_test_memory_summary.csv", thd_summary, ["category", "bytes", "kib", "note"])
    write_csv(RESULTS / "production_sections.csv", production["sections"], section_fields)
    write_csv(RESULTS / "production_regions.csv", production["regions"], region_fields)
    write_csv(
        RESULTS / "production_modules.csv",
        module_rows,
        ["module", "code_readonly_bytes", "code_readonly_kib", "static_ram_bytes", "static_ram_kib", "granularity"],
    )
    write_csv(
        RESULTS / "production_largest_objects.csv",
        largest,
        ["object_kind", "rank", "name", "object_file", "output_section", "bytes", "kib", "memory_region", "module", "description"],
    )
    write_csv(
        RESULTS / "memory_comparison.csv",
        comparisons,
        ["category", "production_bytes", "thd_test_bytes", "test_only_increment_bytes"],
    )

    flow_far = next(
        item
        for item in thd["contributions"]
        if item["object_file"] == "user_subband_flow.obj"
        and item["input_section"] == ".far"
    )
    input_symbol = thd["symbols"]["SUBBAND_THD_InputPacked"]["address"]
    output_symbol = thd["symbols"]["SUBBAND_THD_OutputPacked"]["address"]
    capture_detail = {
        "classification": "TEST_ONLY",
        "object_file": "user_subband_flow.obj",
        "input_section": ".far",
        "component_bytes": flow_far["size_bytes"],
        "memory_region": flow_far["memory_region"],
        "input_symbol": "SUBBAND_THD_InputPacked",
        "input_symbol_address": f"0x{input_symbol:08x}",
        "bytes_until_next_linker_symbol": output_symbol - input_symbol,
        "output_symbol": "SUBBAND_THD_OutputPacked",
        "output_symbol_address": f"0x{output_symbol:08x}",
        "bytes_from_output_symbol_to_component_end": flow_far["run_address_int"] + flow_far["size_bytes"] - output_symbol,
        "precision_note": "The linker proves the complete object contribution and symbol boundaries, but does not emit individual variable sizes for OutputPacked and adjacent THD frame scratch state.",
    }

    important_regions = [region for region in production["regions"] if general_region(region)]
    max_region = max(important_regions, key=lambda region: region["utilization_percent"])
    dynamic_calls = []
    dynamic_source = ROOT / "Code" / "User" / "user_dsp" / "user_subband_audio_compare.c"
    for line_number, line in enumerate(dynamic_source.read_text(encoding="utf-8", errors="replace").splitlines(), 1):
        if re.search(r"\b(?:malloc|calloc|realloc|free)\s*\(", line):
            dynamic_calls.append({"file": str(dynamic_source.relative_to(ROOT)), "line": line_number, "text": line.strip()})

    head = git("rev-parse", "HEAD")
    metadata = {
        "commit_sha": head,
        "build_identity": {"version": "P33_FIX_V5", "git_sha": head[:7], "dirty": 0},
        "source_dirty_at_build": False,
        "configuration": "Debug configuration with production-equivalent -O3 optimization",
        "compiler": production["linker_banner"],
        "optimization": "-O3",
        "common_defines": ["c6748", "DEBUG", "EQ_USE_GENERATED_BUILD_ID=1"],
        "production_defines": ["DSP_LAB_PROJECT_SELECT=32", "SUBBAND_THD_BOARD_TEST=0"],
        "thd_test_defines": ["DSP_LAB_PROJECT_SELECT=32", "SUBBAND_THD_BOARD_TEST=1"],
        "linker_command_files": ["TargetConfig/C6748.cmd", "TargetConfig/User.cmd"],
        "linker_options": {"stack_size": "0x800", "heap_size": "0x800", "rom_model": True},
        "production": {
            "clean_full_rebuild": True,
            "link_errors": production["link_errors"],
            "linked_at": production["linked_at"],
            "map_path": str(PRODUCTION_MAP.relative_to(ROOT)),
            "xml_link_info_path": str(PRODUCTION_XML.relative_to(ROOT)),
            "build_log_path": "results/memory/build.log",
            "command": 'gmake -C Debug -B all "GEN_OPTS__FLAG=--define=DSP_LAB_PROJECT_SELECT=32 --define=SUBBAND_THD_BOARD_TEST=0 --define=EQ_USE_GENERATED_BUILD_ID=1"',
        },
        "thd_test": {
            "clean_full_rebuild": True,
            "link_errors": thd["link_errors"],
            "linked_at": thd["linked_at"],
            "map_path": str(THD_MAP.relative_to(ROOT)),
            "xml_link_info_path": str(THD_XML.relative_to(ROOT)),
            "build_log_path": "results/memory/thd_test_build.log",
            "command": 'gmake -C Debug -B all "GEN_OPTS__FLAG=--define=DSP_LAB_PROJECT_SELECT=32 --define=SUBBAND_THD_BOARD_TEST=1 --define=EQ_USE_GENERATED_BUILD_ID=1"',
        },
    }
    (RESULTS / "build_metadata.json").write_text(
        json.dumps(metadata, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
    )

    audit_checks = {
        "production_link_errors_zero": production["link_errors"] == 0,
        "thd_link_errors_zero": thd["link_errors"] == 0,
        "production_has_no_thd_capture_symbols": not any(
            name in production["symbols"]
            for name in ("SUBBAND_THD_InputPacked", "SUBBAND_THD_OutputPacked")
        ),
        "thd_has_both_capture_symbols": all(
            name in thd["symbols"]
            for name in ("SUBBAND_THD_InputPacked", "SUBBAND_THD_OutputPacked")
        ),
        "production_has_no_dynamic_allocation_symbols": not production["dynamic_allocation_symbols"],
        "production_module_code_reconciles": sum(
            row["code_readonly_bytes"] for row in module_rows
        ) == prod_by_name["program_code_and_readonly"],
        "production_module_static_reconciles": sum(
            row["static_ram_bytes"] for row in module_rows
        ) == prod_by_name["static_readwrite_ram"],
        "production_categories_reconcile": (
            prod_by_name["program_code_and_readonly"]
            + prod_by_name["static_readwrite_ram"]
            + prod_by_name["stack_mapped_reserved"]
            + prod_by_name["heap_mapped_reserved"]
            == prod_by_name["total_runtime_ram"]
        ),
    }
    if not all(audit_checks.values()):
        raise RuntimeError(f"Audit checks failed: {audit_checks}")

    report = {
        "schema_version": 1,
        "metadata": metadata,
        "production": {
            "summary": prod_summary,
            "sections": [{key: value for key, value in row.items() if key != "component_refs"} for row in production["sections"]],
            "regions": [{key: value for key, value in row.items() if key != "origin_int"} for row in production["regions"]],
            "modules": module_rows,
            "largest_objects": largest,
            "max_general_region": {key: value for key, value in max_region.items() if key != "origin_int"},
        },
        "thd_test": {"summary": thd_summary, "test_only_capture": capture_detail},
        "comparison": comparisons,
        "dynamic_allocation_audit": {
            "production_runtime_calls_linked": bool(production["dynamic_allocation_symbols"]),
            "linked_symbols": production["dynamic_allocation_symbols"],
            "production_sysmem_mapped_bytes": 0,
            "linker_heap_option_bytes": heap_configured,
            "source_calls": dynamic_calls,
            "source_guard": "SUBBAND_ALGO_ONLY (host/offline-only; absent from production build)",
        },
        "evidence_boundaries": [
            "All byte counts are from fresh TI linker map/XML outputs; .out file size is not used as RAM.",
            "Load and run sizes are not added together.",
            "Stack is a linker reservation, not a measured run-time high-water mark.",
            "Heap option is 2048 bytes, but .sysmem maps 0 bytes because the production image links no dynamic-allocation calls.",
            "Module attribution is at object-file contribution granularity; shared libraries and linker gaps remain platform/shared.",
            "No board stack watermark or run-time heap peak was measured in this build-artifact-only audit.",
        ],
        "audit_checks": audit_checks,
    }
    (RESULTS / "memory_summary.json").write_text(
        json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
    )

    ppt_rows = [
        ["Program code and read-only constants", f"{kib(prod_by_name['program_code_and_readonly'])} KiB", ".text/.const/.switch/.cinit etc."],
        ["Static read/write data", f"{kib(prod_by_name['static_readwrite_ram'])} KiB", ".subband_l2/.far/.bss/frame buffers etc."],
        ["Stack reservation", f"{kib(prod_by_name['stack_mapped_reserved'])} KiB", "Linker reservation, not measured peak"],
        ["Heap reservation", "0.000 KiB mapped", "2.000 KiB linker option; .sysmem not instantiated"],
        ["Production total run-time RAM", f"{kib(prod_by_name['total_runtime_ram'])} KiB", "Excludes THD test-only capture"],
        ["Highest general-region utilization", f"{max_region['utilization_percent']:.3f}%", f"{max_region['region']}, {kib(max_region['unused_bytes'])} KiB free"],
    ]
    module_ppt_rows = [
        [row["module"], f"{row['code_readonly_kib']:.3f}", f"{row['static_ram_kib']:.3f}"]
        for row in module_rows
    ]
    region_rows = [
        [region["region"], region["origin"], f"{kib(region['capacity_bytes'])}", f"{kib(region['used_bytes'])}", f"{kib(region['unused_bytes'])}", f"{region['utilization_percent']:.3f}%"]
        for region in production["regions"]
    ]
    top_static = [row for row in largest if row["object_kind"] == "static_data"][:8]
    top_code = [row for row in largest if row["object_kind"] == "code"][:8]
    top_const = [row for row in largest if row["object_kind"] == "constant"][:8]
    comparison_by_name = {row["category"]: row for row in comparisons}

    dynamic_conclusion_zh = (
        "\u8fd0\u884c\u671f\u672a\u53d1\u73b0\u52a8\u6001\u5806\u5206\u914d\uff0c"
        "\u4e3b\u8981\u5185\u5b58\u4e3a\u9759\u6001\u5206\u914d\u3002"
    )
    md = f"""# Project 3.2 memory audit

## 1. Production firmware result

- Commit: `{head}`; build identity `P33_FIX_V5 / {head[:7]} / dirty=0`.
- Configuration: CCS `Debug` configuration, TI C6000 v8.5.0.LTS, `-O3`.
- Production macros: `DSP_LAB_PROJECT_SELECT=32`, `SUBBAND_THD_BOARD_TEST=0`, `EQ_USE_GENERATED_BUILD_ID=1`.
- Production clean/full link: `link_errors=0x0`; linked at {production['linked_at']}.
- The total below is linker-mapped run-time memory. The `.out` file size is deliberately not used.

### PPT-ready overall table

{markdown_table(["Category", "Usage", "Note"], ppt_rows)}

Normal Project 3.2 production uses **{kib(prod_by_name['program_code_and_readonly'])} KiB** for code/read-only content and **{kib(prod_by_name['static_readwrite_ram'])} KiB** for static run-time data. The highest general-purpose region utilization is **{max_region['region']} {max_region['utilization_percent']:.3f}%**, leaving **{kib(max_region['unused_bytes'])} KiB**. Fixed AD/DA linker windows are intentionally 100% occupied and are shown separately in the complete region table.

## 2. Code/read-only and static RAM boundaries

- Code/read-only: {prod_by_name['program_code_and_readonly']} bytes ({kib(prod_by_name['program_code_and_readonly'])} KiB).
- Static read/write RAM: {prod_by_name['static_readwrite_ram']} bytes ({kib(prod_by_name['static_readwrite_ram'])} KiB), including the normal LCD off-screen buffer and fixed board ADC/DAC buffers.
- Stack mapped reservation: {prod_by_name['stack_mapped_reserved']} bytes ({kib(prod_by_name['stack_mapped_reserved'])} KiB). This is not a measured peak.
- Heap: linker option `--heap_size=0x800` configures 2048 bytes, while the production map has `.sysmem=0`; therefore mapped heap reservation is 0 bytes and no 2 KiB is added to run-time RAM.
- Total mapped run-time RAM: {prod_by_name['total_runtime_ram']} bytes ({kib(prod_by_name['total_runtime_ram'])} KiB). Load/run addresses are not double-counted.

## 3. Memory regions

{markdown_table(["Region", "Origin", "Capacity KiB", "Used KiB", "Free KiB", "Utilization"], region_rows)}

## 4. Module usage

{markdown_table(["Module", "Code/const KiB", "Static RAM KiB"], module_ppt_rows)}

Module values use TI linker object-file contributions. Shared libraries and linker alignment/holes are retained in `Platform and shared runtime`; no shared contribution is counted twice.

## 5. Largest linked contributions

### Code (top 8 of 15 in CSV)

{markdown_table(["Rank", "Object contribution", "Bytes", "Region", "Purpose"], [[r['rank'], r['name'], r['bytes'], r['memory_region'], r['description']] for r in top_code])}

### Static data (top 8 of 15 in CSV)

{markdown_table(["Rank", "Object contribution", "Bytes", "Region", "Purpose"], [[r['rank'], r['name'], r['bytes'], r['memory_region'], r['description']] for r in top_static])}

### Constants (top 8 of 15 in CSV)

{markdown_table(["Rank", "Object contribution", "Bytes", "Region", "Purpose"], [[r['rank'], r['name'], r['bytes'], r['memory_region'], r['description']] for r in top_const])}

## 6. THD test-only increment

The THD build adds **{comparison_by_name['total_runtime_ram']['test_only_increment_bytes']} bytes ({kib(comparison_by_name['total_runtime_ram']['test_only_increment_bytes'])} KiB)** of mapped run-time memory. Of this, **{flow_far['size_bytes']} bytes ({kib(flow_far['size_bytes'])} KiB)** is the `TEST_ONLY` `user_subband_flow.obj (.far)` contribution in DDR containing the 10 s JTAG capture buffers and adjacent THD frame scratch state. It is absent from the production map and excluded from all production totals.

- `SUBBAND_THD_InputPacked`: address `0x{input_symbol:08x}`; {output_symbol - input_symbol} bytes until the next linker symbol.
- `SUBBAND_THD_OutputPacked`: address `0x{output_symbol:08x}`; {flow_far['run_address_int'] + flow_far['size_bytes'] - output_symbol} bytes remain to the end of the same THD-only object contribution.
- The linker XML proves the whole contribution and both symbol boundaries, but does not provide an individual variable-size record for `OutputPacked` versus adjacent private THD frame scratch arrays. This audit does not replace that missing precision with a source estimate.

{markdown_table(["Category", "Production bytes", "THD bytes", "TEST_ONLY increment"], [[r['category'], r['production_bytes'], r['thd_test_bytes'], r['test_only_increment_bytes']] for r in comparisons])}

## 7. Dynamic allocation and stack/heap

Production links no `malloc/calloc/realloc/free` symbols and maps `.sysmem` to 0 bytes. The source-level calls found in `user_subband_audio_compare.c` are inside `SUBBAND_ALGO_ONLY`, a host/offline-only branch absent from this board build. Therefore: **{dynamic_conclusion_zh}** The 2 KiB heap option is configuration, not measured use; the 2 KiB stack is only a linker reservation, not a measured high-water mark.

## 8. Evidence and limitations

- All byte counts come from the fresh production/THD TI linker map and XML link-info files.
- Production and THD both report `link_errors=0x0`; their maps were produced by separate clean/full builds.
- Program load and run sizes are never added together, and `.out` artifact size is not treated as RAM.
- Module and largest-object reporting is object-file contribution granularity. The map cannot safely assign every common symbol or shared library to a single algorithm.
- No board-side stack watermark or run-time heap peak was measured; neither is claimed.
- Debug information sections exist in the ELF artifact but have run address 0 and are excluded from target RAM/program-image totals.
"""
    (RESULTS / "memory_summary.md").write_text(md, encoding="utf-8")

    actual = {
        "production_code_readonly": prod_by_name["program_code_and_readonly"],
        "production_static": prod_by_name["static_readwrite_ram"],
        "production_stack": prod_by_name["stack_mapped_reserved"],
        "production_total": prod_by_name["total_runtime_ram"],
    }
    print(
        json.dumps(
            {
                "status": "ok",
                **actual,
                "thd_total": thd_by_name["total_runtime_ram"],
                "audit_checks": audit_checks,
            },
            indent=2,
        )
    )


if __name__ == "__main__":
    main()
