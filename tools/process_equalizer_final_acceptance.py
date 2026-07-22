#!/usr/bin/env python3
"""Convert raw Project 3.3 board-acceptance records into final evidence."""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import math
import re
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Iterable


CYCLE_HZ = 456_000_000.0
FRAME_SAMPLES = 1024
SAMPLE_RATE_HZ = 50_000.0
FRAME_BUDGET_CYCLES = 9_338_880
LCD_OVER_2MS_CYCLES = 912_000
LCD_OVER_5MS_CYCLES = 2_280_000
TIMING_SAMPLE_MINIMUM = 20
UINT32_MODULUS = 2**32

TIMING_CLASSES = (
    "preset",
    "analyzer_strip",
    "dynamic_enabled",
    "dynamic_strength",
    "dynamic_active",
    "editor_band",
    "editor_field",
    "page_sync",
    "page_swap",
)

REQUIRED_OUTPUTS = (
    "README.md",
    "evidence_manifest.json",
    "build_summary.json",
    "memory_sections.csv",
    "framebuffer_map.json",
    "production_symbol_audit.txt",
    "production_boot_summary.json",
    "page_switch_30_scripted.json",
    "operator_visual_observation.md",
    "lcd_job_timing.csv",
    "lcd_job_timing_summary.json",
    "final_non_touch_endurance_300s.json",
    "analyzer_reset_board_summary.json",
    "external_file_inventory.json",
    "final_acceptance_summary.csv",
    "sha256_manifest.txt",
)

TOUCH_WAIVER_REASON = "USER_REMOVED_TOUCH_RELIABILITY_FROM_ACCEPTANCE_SCOPE"

EXTERNAL_BOUNDARIES = {
    "EXTERNAL_ANALOG_THD": "NOT_MEASURED",
    "EXTERNAL_ANALOG_SNR": "NOT_MEASURED",
    "EXTERNAL_ANALOG_FREQUENCY_RESPONSE": "NOT_MEASURED",
    "SPL": "NOT_MEASURED",
    "ADC_ENOB": "NOT_MEASURED",
}

LCD_FAULT_COUNTERS = (
    "lcd_unexpected_full_redraw",
    "lcd_writeback_failures",
    "lcd_raster_faults",
    "lcd_fifo_underflows",
    "lcd_sync_errors",
    "lcd_frame_address_mismatches",
    "lcd_bounds_failures",
    "lcd_canary_failures",
    "lcd_raster_stop_timeouts",
)

ENDURANCE_SAFETY_COUNTERS = (
    "deadline_misses",
    "latency_misses",
    "frame_overlaps",
    "dropped_frames",
    "clip_count",
    "invalid_count",
    "smart_saturation",
    "smart_nonfinite",
    "clarity_saturation",
    "clarity_nonfinite",
    "guard_saturation",
    "guard_nonfinite",
    *LCD_FAULT_COUNTERS,
)

TIMING_COLUMNS = (
    "class_name",
    "sample_index",
    "cycles",
    "microseconds",
    "job_type",
    "job_count_delta",
    "over_2ms",
    "over_5ms",
    "deferred_by_audio_delta",
    "audio_arrived_during_draw_delta",
    "attribution",
)


def utc_now() -> str:
    return datetime.now(timezone.utc).isoformat().replace("+00:00", "Z")


def write_json(path: Path, value: Any) -> None:
    path.write_text(
        json.dumps(value, indent=2, ensure_ascii=True) + "\n",
        encoding="utf-8",
    )


def read_json(path: Path) -> tuple[Any | None, str | None]:
    if not path.is_file():
        return None, None
    try:
        return json.loads(path.read_text(encoding="utf-8-sig")), None
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        return None, f"{path.name}: {error}"


def read_jsonl(path: Path) -> tuple[list[dict[str, Any]], list[str]]:
    records: list[dict[str, Any]] = []
    errors: list[str] = []
    if not path.is_file():
        return records, errors
    try:
        lines = path.read_text(encoding="utf-8-sig").splitlines()
    except (OSError, UnicodeError) as error:
        return records, [f"{path.name}: {error}"]
    for line_number, line in enumerate(lines, start=1):
        if not line.strip():
            continue
        try:
            value = json.loads(line)
        except json.JSONDecodeError as error:
            errors.append(f"{path.name}:{line_number}: {error}")
            continue
        if isinstance(value, dict):
            records.append(value)
        else:
            errors.append(f"{path.name}:{line_number}: object required")
    return records, errors


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def pending(stage: str, reason: str = "raw board evidence is absent") -> dict[str, Any]:
    return {
        "schema_version": 1,
        "stage": stage,
        "status": "PENDING_HARDWARE",
        "result": "NOT_RUN",
        "reason": reason,
        "generated_at_utc": utc_now(),
    }


def exact_number(value: Any, expected: float) -> bool:
    return (
        not isinstance(value, bool)
        and isinstance(value, (int, float))
        and math.isfinite(float(value))
        and float(value) == expected
    )


def process_production_boot(raw_dir: Path, output_dir: Path) -> dict[str, Any]:
    summary, error = read_json(raw_dir / "production_boot_summary.json")
    if summary is None and error is None:
        evidence = pending("production_boot", "production boot stage has not run")
        write_json(output_dir / "production_boot_summary.json", evidence)
        return evidence

    failures: list[str] = []
    if error:
        failures.append(error)
    if not isinstance(summary, dict):
        summary = {}
        failures.append("production boot summary must be a JSON object")
    checks = (
        (summary.get("pass") is True, "pass is not true"),
        (exact_number(summary.get("init_stage"), 11.0), "init_stage is not 11"),
        (exact_number(summary.get("deadline"), 0.0), "deadline is not 0"),
        (
            exact_number(summary.get("latency_miss"), 0.0),
            "latency_miss is not 0",
        ),
        (exact_number(summary.get("overlap"), 0.0), "overlap is not 0"),
        (exact_number(summary.get("dropped"), 0.0), "dropped is not 0"),
        (exact_number(summary.get("clip"), 0.0), "clip is not 0"),
        (
            summary.get("final_target_state") == "RUNNING_DISCONNECTED",
            "final_target_state is not RUNNING_DISCONNECTED",
        ),
    )
    failures.extend(message for passed, message in checks if not passed)
    evidence = dict(summary)
    evidence.update(
        {
            "schema_version": 1,
            "stage": "production_boot",
            "status": "MEASURED",
            "measurement_label": "BOARD_BOOT_COUNTER",
            "result": "FAIL" if failures else "PASS",
            "stop_reasons": failures,
        }
    )
    write_json(output_dir / "production_boot_summary.json", evidence)
    return evidence


def number(value: Any) -> float | None:
    if isinstance(value, bool):
        return float(int(value))
    if isinstance(value, (int, float)) and math.isfinite(float(value)):
        return float(value)
    return None


def integer(value: Any) -> int | None:
    parsed = number(value)
    if parsed is None:
        return None
    return int(parsed)


def counter_delta(after: dict[str, Any], before: dict[str, Any], key: str) -> int | None:
    first = integer(before.get(key))
    second = integer(after.get(key))
    if first is None or second is None:
        return None
    value = second - first
    if value < 0:
        value += UINT32_MODULUS
    return value


def snapshot_deltas(
    before: dict[str, Any], after: dict[str, Any], keys: Iterable[str]
) -> dict[str, int | None]:
    return {key: counter_delta(after, before, key) for key in keys}


def waived_touch_stage(stage: str) -> dict[str, Any]:
    return {
        "schema_version": 1,
        "stage": stage,
        "status": "WAIVED",
        "result": "NOT_REQUIRED",
        "reason": TOUCH_WAIVER_REASON,
    }


def process_no_touch(_raw_dir: Path, _output_dir: Path) -> dict[str, Any]:
    return waived_touch_stage("no_touch_120s")


def write_csv(path: Path, columns: Iterable[str], rows: Iterable[dict[str, Any]]) -> None:
    with path.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(columns), extrasaction="ignore")
        writer.writeheader()
        for row in rows:
            writer.writerow({key: row.get(key, "") for key in writer.fieldnames})


def process_hitboxes(_raw_dir: Path, _output_dir: Path) -> dict[str, Any]:
    return waived_touch_stage("touch_hitbox_27")


PAGE_REQUEST_CONTRACT = "SCRIPTED_PAGE_REQUEST_API"

PAGE_COUNTERS = (
    "lcd_page_sync_jobs",
    "lcd_swap_requests",
    "lcd_swap_completes",
    "lcd_eof0",
    "lcd_eof1",
    "lcd_eof_ambiguous",
    "lcd_frame_address_mismatches",
    "lcd_unexpected_full_redraw",
    "lcd_writeback_count",
    "lcd_writeback_bytes",
    "lcd_writeback_failures",
    "lcd_raster_faults",
    "lcd_fifo_underflows",
    "lcd_sync_errors",
    "lcd_bounds_failures",
    "lcd_canary_failures",
    "lcd_raster_stop_timeouts",
)


def process_page_switch(raw_dir: Path, output_dir: Path) -> dict[str, Any]:
    summary, error = read_json(raw_dir / "page_switch_30_raw_summary.json")
    records, parse_errors = read_jsonl(raw_dir / "page_switch_30.jsonl")
    rounds = [record for record in records if record.get("record_type") == "page_round"]
    if summary is None and not rounds:
        evidence = pending("page_switch_30", error or "30-round page stage has not run")
        evidence["completed_round_trips"] = 0
        evidence["request_contract"] = PAGE_REQUEST_CONTRACT
        write_json(output_dir / "page_switch_30_scripted.json", evidence)
        return evidence
    summary = summary if isinstance(summary, dict) else {}
    before = summary.get("before") if isinstance(summary.get("before"), dict) else {}
    after = summary.get("after") if isinstance(summary.get("after"), dict) else {}
    deltas = snapshot_deltas(before, after, PAGE_COUNTERS)
    completed_rounds = integer(summary.get("completed_round_trips"))
    if completed_rounds is None:
        completed_rounds = len({integer(item.get("round")) for item in rounds})
    stop_reasons: list[str] = []
    round_ids = {integer(item.get("round")) for item in rounds}
    if len(rounds) != 30 or round_ids != set(range(1, 31)):
        stop_reasons.append(
            f"scripted page records={len(rounds)}, expected rounds 1..30"
        )
    scripted_record_errors = sum(
        1
        for item in rounds
        if item.get("status_to_editor_scripted") is not True
        or item.get("editor_to_status_scripted") is not True
    )
    if scripted_record_errors:
        stop_reasons.append(
            f"non-scripted-API page round records={scripted_record_errors}"
        )
    if completed_rounds < 30:
        stop_reasons.append(f"scripted page round trips={completed_rounds}, expected 30")
    for key, minimum in (
        ("lcd_page_sync_jobs", 1),
        ("lcd_swap_requests", 60),
        ("lcd_swap_completes", 60),
    ):
        value = deltas.get(key)
        if value is not None and value < minimum:
            stop_reasons.append(f"{key} delta={value}, expected at least {minimum}")
    for key in LCD_FAULT_COUNTERS:
        value = deltas.get(key)
        if value not in (None, 0):
            stop_reasons.append(f"{key} delta is {value}")
    if deltas.get("lcd_eof_ambiguous") not in (None, 0):
        stop_reasons.append("ambiguous EOF observed")
    if parse_errors:
        stop_reasons.extend(parse_errors)
    if error:
        stop_reasons.append(error)
    completed = summary.get("completed") is True
    if summary.get("completed") is False:
        stop_reasons.append(str(summary.get("error") or "DSS stage failed"))
    missing = [key for key, value in deltas.items() if value is None]
    if stop_reasons:
        result = "FAIL" if rounds or completed else "INCOMPLETE"
    elif not completed or missing:
        result = "INCOMPLETE"
    else:
        result = "PASS"
    evidence = {
        "schema_version": 1,
        "stage": "page_switch_30",
        "status": "MEASURED",
        "measurement_label": "BOARD_UI_COUNTER",
        "result": result,
        "request_contract": PAGE_REQUEST_CONTRACT,
        "scripted_record_error_count": scripted_record_errors,
        "completed_round_trips": completed_rounds,
        "before": before,
        "after": after,
        "deltas": deltas,
        "max_pending_dirty_regions": after.get("lcd_max_pending_dirty_regions"),
        "missing_counters": missing,
        "operator_visual_result": "PENDING_OPERATOR",
        "stop_reasons": stop_reasons,
    }
    write_json(output_dir / "page_switch_30_scripted.json", evidence)
    return evidence


def nearest_rank(values: list[int], fraction: float) -> int | None:
    if not values:
        return None
    ordered = sorted(values)
    rank = max(1, math.ceil(fraction * len(ordered)))
    return ordered[rank - 1]


def median_cycles(values: list[int]) -> int | float | None:
    if not values:
        return None
    ordered = sorted(values)
    middle = len(ordered) // 2
    if len(ordered) % 2:
        return ordered[middle]
    return (ordered[middle - 1] + ordered[middle]) / 2.0


def cycles_to_us(value: int | float | None) -> float | None:
    if value is None:
        return None
    return float(value) * 1_000_000.0 / CYCLE_HZ


def process_timing(raw_dir: Path, output_dir: Path) -> dict[str, Any]:
    records, parse_errors = read_jsonl(raw_dir / "lcd_job_timing.jsonl")
    summary, summary_error = read_json(raw_dir / "lcd_job_timing_raw_summary.json")
    samples = [record for record in records if record.get("record_type") == "timing_sample"]
    class_records = {
        str(record.get("class_name")): record
        for record in records
        if record.get("record_type") == "timing_class_summary"
    }
    csv_rows: list[dict[str, Any]] = []
    classes: list[dict[str, Any]] = []
    for class_name in TIMING_CLASSES:
        class_samples = [item for item in samples if item.get("class_name") == class_name]
        values = [
            value
            for value in (integer(item.get("cycles")) for item in class_samples)
            if value is not None and value >= 0
        ]
        for item in class_samples:
            value = integer(item.get("cycles"))
            row = dict(item)
            row["microseconds"] = cycles_to_us(value)
            row["over_2ms"] = value is not None and value > LCD_OVER_2MS_CYCLES
            row["over_5ms"] = value is not None and value > LCD_OVER_5MS_CYCLES
            csv_rows.append(row)
        sample_minimum = min(values) if values else None
        median = median_cycles(values)
        p95 = nearest_rank(values, 0.95)
        p99 = nearest_rank(values, 0.99)
        sample_maximum = max(values) if values else None
        board_summary = class_records.get(class_name, {})
        board_total = integer(board_summary.get("total_count"))
        board_dropped = integer(board_summary.get("dropped_count"))
        board_minimum = integer(board_summary.get("min_cycles"))
        board_maximum = integer(board_summary.get("max_cycles"))
        total = board_total if board_total is not None else len(values)
        dropped = board_dropped if board_dropped is not None else 0
        minimum = board_minimum if board_minimum is not None else sample_minimum
        maximum = board_maximum if board_maximum is not None else sample_maximum
        board_over_2ms = integer(board_summary.get("over_2ms_count"))
        board_over_5ms = integer(board_summary.get("over_5ms_count"))
        deferred = integer(board_summary.get("deferred_by_audio_count"))
        arrived = integer(board_summary.get("audio_arrived_during_draw_count"))
        classes.append(
            {
                "class_name": class_name,
                "count": total,
                "sample_count": len(values),
                "total_count": total,
                "dropped_count": dropped,
                "min_cycles": minimum,
                "median_cycles": median,
                "p95_cycles": p95,
                "p99_cycles": p99,
                "max_cycles": maximum,
                "min_us": cycles_to_us(minimum),
                "median_us": cycles_to_us(median),
                "p95_us": cycles_to_us(p95),
                "p99_us": cycles_to_us(p99),
                "max_us": cycles_to_us(maximum),
                "over_2ms_count": board_over_2ms
                if board_over_2ms is not None
                else sum(value > LCD_OVER_2MS_CYCLES for value in values),
                "over_5ms_count": board_over_5ms
                if board_over_5ms is not None
                else sum(value > LCD_OVER_5MS_CYCLES for value in values),
                "deferred_by_audio_count": deferred
                if deferred is not None
                else sum(
                    integer(item.get("deferred_by_audio_delta")) or 0
                    for item in class_samples
                ),
                "audio_arrived_during_draw_count": arrived
                if arrived is not None
                else sum(
                    integer(item.get("audio_arrived_during_draw_delta")) or 0
                    for item in class_samples
                ),
                "measurement_region": board_summary.get("measurement_region"),
            }
        )
    write_csv(output_dir / "lcd_job_timing.csv", TIMING_COLUMNS, csv_rows)
    if summary is None and not samples:
        evidence = pending("lcd_job_timing", summary_error or "timing stage has not run")
        evidence["classes"] = classes
        evidence["clock_hz"] = CYCLE_HZ
        write_json(output_dir / "lcd_job_timing_summary.json", evidence)
        return evidence

    summary = summary if isinstance(summary, dict) else {}
    stop_reasons = list(parse_errors)
    if summary_error:
        stop_reasons.append(summary_error)
    for item in classes:
        if item["over_5ms_count"]:
            stop_reasons.append(f"{item['class_name']} contains LCD jobs over 5 ms")
        if item["max_cycles"] is not None and item["max_cycles"] >= LCD_OVER_5MS_CYCLES:
            stop_reasons.append(f"{item['class_name']} maximum is not below 5 ms")
        if item["dropped_count"] != 0:
            stop_reasons.append(f"{item['class_name']} dropped timing samples")
        if item["total_count"] != item["sample_count"] + item["dropped_count"]:
            stop_reasons.append(
                f"{item['class_name']} total/sample/dropped counts disagree"
            )
    before = summary.get("before") if isinstance(summary.get("before"), dict) else {}
    after = summary.get("after") if isinstance(summary.get("after"), dict) else {}
    fault_deltas = snapshot_deltas(before, after, LCD_FAULT_COUNTERS)
    for key, value in fault_deltas.items():
        if value not in (None, 0):
            stop_reasons.append(f"{key} delta is {value}")
    completed = summary.get("completed") is True
    if summary.get("completed") is False:
        stop_reasons.append(str(summary.get("error") or "DSS timing stage failed"))
    missing_classes = [
        item["class_name"]
        for item in classes
        if item["count"] < TIMING_SAMPLE_MINIMUM
        or item["sample_count"] < TIMING_SAMPLE_MINIMUM
    ]
    if stop_reasons:
        result = "FAIL"
    elif not completed or missing_classes:
        result = "INCOMPLETE"
    else:
        result = "PASS"
    evidence = {
        "schema_version": 1,
        "stage": "lcd_job_timing",
        "status": "MEASURED",
        "measurement_label": "BOARD_UI_COUNTER",
        "result": result,
        "clock_hz": CYCLE_HZ,
        "thresholds": {
            "minimum_valid_samples_per_class": TIMING_SAMPLE_MINIMUM,
            "over_2ms_cycles": LCD_OVER_2MS_CYCLES,
            "over_5ms_cycles": LCD_OVER_5MS_CYCLES,
        },
        "classes": classes,
        "missing_classes": missing_classes,
        "fault_deltas": fault_deltas,
        "stop_reasons": stop_reasons,
        "measurement_note": (
            "Samples come from the bounded board capture. The measured "
            "region contains only drawing or descriptor work; PAGE_SWAP is the "
            "EOF descriptor-update interval."
        ),
    }
    write_json(output_dir / "lcd_job_timing_summary.json", evidence)
    return evidence


ENDURANCE_COUNTERS = (
    "process_frames",
    "ad_frames",
    "da_frames",
    "algorithm_frames",
    "analyzer_publications",
    "smart_decisions",
    "clarity_decisions",
    "guard_decisions",
    "lcd_swap_completes",
    "lcd_jobs",
    "static_dynamic_overlap_frames",
    *ENDURANCE_SAFETY_COUNTERS,
)


def process_endurance(raw_dir: Path, output_dir: Path) -> dict[str, Any]:
    summary, error = read_json(raw_dir / "endurance_300s_raw_summary.json")
    if summary is None:
        evidence = pending("endurance_300s", error or "300 s stage has not run")
        evidence["interaction_contract"] = "NON_TOUCH_SCRIPTED_API"
        write_json(output_dir / "final_non_touch_endurance_300s.json", evidence)
        return evidence
    summary = summary if isinstance(summary, dict) else {}
    before = summary.get("before") if isinstance(summary.get("before"), dict) else {}
    after = summary.get("after") if isinstance(summary.get("after"), dict) else {}
    deltas = snapshot_deltas(before, after, ENDURANCE_COUNTERS)
    process_delta = deltas.get("process_frames")
    dsp_audio_seconds = (
        process_delta * FRAME_SAMPLES / SAMPLE_RATE_HZ
        if process_delta is not None
        else None
    )
    elapsed = number(summary.get("host_elapsed_seconds"))
    stop_reasons: list[str] = []
    if elapsed is None or elapsed < 300.0:
        stop_reasons.append("host elapsed time is below 300 s")
    if dsp_audio_seconds is None or dsp_audio_seconds < 300.0:
        stop_reasons.append("DSP audio time is below 300 s")
    for key in ENDURANCE_SAFETY_COUNTERS:
        value = deltas.get(key)
        if value not in (None, 0):
            stop_reasons.append(f"{key} delta is {value}")
    for key in (
        "analyzer_publications",
        "smart_decisions",
        "clarity_decisions",
        "guard_decisions",
    ):
        value = deltas.get(key)
        if value is None or value <= 0:
            stop_reasons.append(f"{key} did not advance")
    max_latency = integer(after.get("frame_latency_max_cycles"))
    if max_latency is None:
        stop_reasons.append("maximum frame latency is unavailable")
    elif max_latency >= FRAME_BUDGET_CYCLES:
        stop_reasons.append(
            f"maximum frame latency {max_latency} is not below {FRAME_BUDGET_CYCLES}"
        )
    overlap_delta = deltas.get("static_dynamic_overlap_frames")
    if overlap_delta is None or overlap_delta < 1:
        stop_reasons.append("no static/dynamic transition overlap frame was measured")
    completed = summary.get("completed") is True
    if summary.get("continuous_window_halt_count") not in (None, 2):
        stop_reasons.append("300 s window did not use start/end halt only")
    if not completed:
        stop_reasons.append(str(summary.get("error") or "DSS endurance stage failed"))
    missing = [key for key, value in deltas.items() if value is None]
    if stop_reasons:
        result = "FAIL"
    elif missing:
        result = "INCOMPLETE"
    else:
        result = "PASS"
    evidence = {
        "schema_version": 1,
        "stage": "endurance_300s",
        "status": "MEASURED",
        "measurement_label": "BOARD_REALTIME_COUNTER",
        "result": result,
        "interaction_contract": "NON_TOUCH_SCRIPTED_API",
        "host_elapsed_seconds": elapsed,
        "dsp_audio_seconds": dsp_audio_seconds,
        "frame_samples": FRAME_SAMPLES,
        "sample_rate_hz": SAMPLE_RATE_HZ,
        "before": before,
        "after": after,
        "deltas": deltas,
        "maxima": {
            "algorithm_cycles": after.get("algorithm_max_cycles"),
            "frame_service_cycles": after.get("frame_service_max_cycles"),
            "frame_latency_cycles": max_latency,
            "lcd_job_cycles": after.get("lcd_max_job_cycles"),
        },
        "missing_counters": missing,
        "stop_reasons": stop_reasons,
        "window_contract": "one start snapshot, continuous run, one end snapshot",
    }
    write_json(output_dir / "final_non_touch_endurance_300s.json", evidence)
    return evidence


def process_analyzer_reset(raw_dir: Path, output_dir: Path) -> dict[str, Any]:
    summary, error = read_json(raw_dir / "analyzer_reset_raw_summary.json")
    if summary is None:
        evidence = pending("analyzer_reset", error or "Analyzer reset stage has not run")
        write_json(output_dir / "analyzer_reset_board_summary.json", evidence)
        return evidence
    summary = summary if isinstance(summary, dict) else {}
    before = summary.get("before") if isinstance(summary.get("before"), dict) else {}
    invalid = summary.get("invalid") if isinstance(summary.get("invalid"), dict) else {}
    recovered = summary.get("recovered") if isinstance(summary.get("recovered"), dict) else {}
    safety = snapshot_deltas(before, recovered, ENDURANCE_SAFETY_COUNTERS)
    stop_reasons: list[str] = []
    if invalid.get("analyzer_valid") not in (0, False):
        stop_reasons.append("Analyzer invalid phase was not observed")
    if recovered.get("analyzer_valid") not in (1, True):
        stop_reasons.append("Analyzer valid did not recover")
    if recovered.get("analyzer_warm") not in (1, True):
        stop_reasons.append("Analyzer warm state did not recover")
    publication_delta = counter_delta(recovered, before, "analyzer_publications")
    if publication_delta is None or publication_delta <= 0:
        stop_reasons.append("Analyzer publication count did not advance")
    for key, value in safety.items():
        if value not in (None, 0):
            stop_reasons.append(f"{key} delta is {value}")
    completed = summary.get("completed") is True
    if not completed:
        stop_reasons.append(str(summary.get("error") or "DSS reset stage failed"))
    epoch_before = before.get("analyzer_epoch")
    epoch_after = recovered.get("analyzer_epoch")
    epoch_verified = (
        integer(epoch_before) is not None
        and integer(epoch_after) is not None
        and counter_delta(recovered, before, "analyzer_epoch") not in (None, 0)
    )
    missing = [] if epoch_verified else ["analyzer_epoch"]
    if stop_reasons:
        result = "FAIL"
    elif missing:
        result = "INCOMPLETE"
    else:
        result = "PASS"
    evidence = {
        "schema_version": 1,
        "stage": "analyzer_reset",
        "status": "MEASURED",
        "measurement_label": "BOARD_REALTIME_COUNTER",
        "result": result,
        "before": before,
        "invalid": invalid,
        "recovered": recovered,
        "publication_delta": publication_delta,
        "epoch_verified": epoch_verified,
        "missing_contract_fields": missing,
        "safety_deltas": safety,
        "module_contract": (
            "Modules use their existing invalid/warmup release-or-hold behavior; "
            "this tool does not alter module state."
        ),
        "stop_reasons": stop_reasons,
    }
    write_json(output_dir / "analyzer_reset_board_summary.json", evidence)
    return evidence


def parse_nm(path: Path) -> list[dict[str, Any]]:
    if not path.is_file():
        return []
    pattern = re.compile(
        r"^\[\d+\]\s+\|0x([0-9a-fA-F]+)\|(\d+)\|([^|]*)\|([^|]*)\|"
        r"[^|]*\|[^|]*\|([^\r\n]+)$"
    )
    symbols: list[dict[str, Any]] = []
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        match = pattern.match(line.strip())
        if not match:
            continue
        symbols.append(
            {
                "address": int(match.group(1), 16),
                "size": int(match.group(2)),
                "binding": match.group(3).strip(),
                "type": match.group(4).strip(),
                "name": match.group(5).strip(),
            }
        )
    return symbols


def parse_memory_regions(path: Path) -> list[dict[str, Any]]:
    if not path.is_file():
        return []
    text = path.read_text(encoding="utf-8", errors="replace")
    start = text.find("MEMORY CONFIGURATION")
    end = text.find("SEGMENT ALLOCATION MAP", start + 1)
    if start < 0 or end < 0:
        return []
    pattern = re.compile(
        r"^\s*([A-Za-z0-9_]+)\s+([0-9a-fA-F]{8})\s+([0-9a-fA-F]{8})"
        r"\s+([0-9a-fA-F]{8})\s+([0-9a-fA-F]{8})\s+([RWIX]+)",
        re.MULTILINE,
    )
    regions = []
    for match in pattern.finditer(text[start:end]):
        regions.append(
            {
                "name": match.group(1),
                "origin": int(match.group(2), 16),
                "length": int(match.group(3), 16),
                "used": int(match.group(4), 16),
                "unused": int(match.group(5), 16),
                "attributes": match.group(6),
            }
        )
    return regions


def process_build(raw_dir: Path, output_dir: Path) -> dict[str, Any]:
    strict_outputs = (
        output_dir / "build_summary.json",
        output_dir / "memory_sections.csv",
        output_dir / "framebuffer_map.json",
        output_dir / "production_symbol_audit.txt",
    )
    if all(path.is_file() for path in strict_outputs):
        existing, existing_error = read_json(strict_outputs[0])
        if isinstance(existing, dict):
            if existing.get("result") != "PASS":
                return existing
            strict_contract = (
                existing.get("stage") == "build"
                and existing.get("status") == "MEASURED"
                and existing.get("measurement_label") == "BUILD_EVIDENCE"
                and existing.get("evidence_class") == "BUILD_EVIDENCE"
                and existing.get("profile") == "H_project33_full"
                and existing.get("warning_count") == 0
                and existing.get("error_count") == 0
                and existing.get("link_errors") == "0x0"
            )
            if strict_contract:
                return existing
            return {
                "stage": "build",
                "status": "MEASURED",
                "measurement_label": "BUILD_EVIDENCE",
                "result": "FAIL",
                "failures": ["strict build evidence contract is incomplete"],
            }
        return {
            "stage": "build",
            "status": "MEASURED",
            "measurement_label": "BUILD_EVIDENCE",
            "result": "FAIL",
            "failures": [existing_error or "build_summary.json is unreadable"],
        }

    build_dir = raw_dir / "build"
    matrix, matrix_error = read_json(build_dir / "build_matrix_summary.json")
    if isinstance(matrix, list) and len(matrix) == 1:
        profile = matrix[0]
    elif isinstance(matrix, dict):
        profile = matrix
    else:
        profile = None
    map_path = build_dir / "H_project33_full.map"
    nm_path = build_dir / "H_project33_full_nm.txt"
    symbols = parse_nm(nm_path)
    regions = parse_memory_regions(map_path)

    if not isinstance(profile, dict):
        evidence = pending("build", matrix_error or "clean H build has not run")
        evidence["measurement_label"] = "BUILD_EVIDENCE"
        write_json(output_dir / "build_summary.json", evidence)
        write_csv(
            output_dir / "memory_sections.csv",
            ("kind", "name", "address", "size_bytes", "used_bytes", "unused_bytes", "status"),
            [{"kind": "build", "name": "PENDING", "status": "NOT_MEASURED"}],
        )
        write_json(
            output_dir / "framebuffer_map.json",
            {**pending("framebuffer_map", "clean H build has not run"), "buffers": []},
        )
        (output_dir / "production_symbol_audit.txt").write_text(
            "status=NOT_MEASURED\nresult=PENDING_HARDWARE\nreason=clean H build has not run\n",
            encoding="ascii",
        )
        return evidence

    required_defines = (
        "DSP_LAB_PROJECT_SELECT=33",
        "EQ_ENABLE_AUDIO_FEATURE_ANALYZER=1",
        "EQ_ENABLE_SMART_BASS=1",
        "EQ_ENABLE_DYNAMIC_CLARITY=1",
        "EQ_ENABLE_HARSHNESS_GUARD=1",
        "EQ_ENABLE_LCD_DISPLAY=1",
        "EQ_ENABLE_PROJECT33_TOUCH=1",
        "EQ_ENABLE_TEN_BAND_EDITOR=1",
        "EQ_ENABLE_TEN_BAND_EDITOR_TOUCH=1",
    )
    defines = str(profile.get("defines") or "")
    missing_defines = [item for item in required_defines if item not in defines]
    build_failures: list[str] = []
    if integer(profile.get("warning_count")) != 0:
        build_failures.append("warning_count is not 0")
    if integer(profile.get("error_count")) != 0:
        build_failures.append("error_count is not 0")
    if str(profile.get("link_errors")) != "0x0":
        build_failures.append("link_errors is not 0x0")
    if missing_defines:
        build_failures.append("missing full-H defines: " + ", ".join(missing_defines))

    framebuffers = [
        symbol
        for symbol in symbols
        if symbol["name"] in {"Lcd_Buffer", "EQ_LcdEditorBuffer"}
    ]
    framebuffers.sort(key=lambda item: item["address"])
    overlap = False
    for first, second in zip(framebuffers, framebuffers[1:]):
        if first["address"] + first["size"] > second["address"]:
            overlap = True
    framebuffer_result = "PASS"
    if len(framebuffers) != 2 or overlap:
        framebuffer_result = "FAIL"
        build_failures.append("two non-overlapping permanent framebuffers were not proven")
    framebuffer_evidence = {
        "schema_version": 1,
        "status": "MEASURED",
        "measurement_label": "BUILD_EVIDENCE",
        "result": framebuffer_result,
        "buffers": [
            {
                "name": item["name"],
                "start_address": f"0x{item['address']:08x}",
                "end_address_inclusive": f"0x{item['address'] + item['size'] - 1:08x}",
                "size_bytes": item["size"],
            }
            for item in framebuffers
        ],
        "overlap": overlap,
    }
    write_json(output_dir / "framebuffer_map.json", framebuffer_evidence)

    state_names = {
        "EQ_AudioAnalyzerState": "analyzer_state",
        "EQ_SmartBassState": "smart_bass_state",
        "EQ_DynamicClarityState": "dynamic_clarity_state",
        "EQ_HarshnessGuardState": "hf_guard_state",
    }
    rows: list[dict[str, Any]] = []
    section_fields = {
        ".text": "text_bytes",
        ".const": "const_bytes",
        ".bss": "bss_bytes",
        ".subband_l2": "subband_l2_bytes",
    }
    for name, field in section_fields.items():
        rows.append(
            {
                "kind": "section",
                "name": name,
                "size_bytes": integer(profile.get(field)),
                "status": "MEASURED",
            }
        )
    for region in regions:
        if region["name"] in {"DDR2", "DSPL2RAM"}:
            rows.append(
                {
                    "kind": "memory_region",
                    "name": region["name"],
                    "address": f"0x{region['origin']:08x}",
                    "size_bytes": region["length"],
                    "used_bytes": region["used"],
                    "unused_bytes": region["unused"],
                    "status": "MEASURED",
                }
            )
    for symbol in symbols:
        if symbol["name"] in state_names:
            rows.append(
                {
                    "kind": "state",
                    "name": state_names[symbol["name"]],
                    "address": f"0x{symbol['address']:08x}",
                    "size_bytes": symbol["size"],
                    "status": "MEASURED",
                }
            )
    for name, field in (
        ("ui_state", "ui_state_bytes"),
        ("touch_state", "touch_state_bytes"),
        ("editor_state", "editor_state_bytes"),
    ):
        rows.append(
            {
                "kind": "state",
                "name": name,
                "size_bytes": integer(profile.get(field)),
                "status": "MEASURED",
            }
        )
    write_csv(
        output_dir / "memory_sections.csv",
        ("kind", "name", "address", "size_bytes", "used_bytes", "unused_bytes", "status"),
        rows,
    )

    forbidden_patterns = (
        r"^EQ_FinalMetrics",
        r"Benchmark",
        r"TransitionCapture",
        r"^EQ_Capture(?:Input|Output)$",
        r"^EQ_TriggerCapture(?:Input|Output)$",
        r"^EQ_DebugLcdTimingSamples$",
    )
    forbidden = sorted(
        symbol["name"]
        for symbol in symbols
        if any(re.search(pattern, symbol["name"]) for pattern in forbidden_patterns)
    )
    audit_result = "PASS" if symbols and not forbidden else "FAIL"
    if forbidden:
        build_failures.append("production capture/benchmark symbols are linked")
    if not symbols:
        build_failures.append("nm symbol evidence is absent")
    audit_lines = [
        "status=MEASURED",
        "measurement_label=BUILD_EVIDENCE",
        f"result={audit_result}",
        f"symbol_count={len(symbols)}",
        "forbidden_patterns=" + ";".join(forbidden_patterns),
        "forbidden_symbols=" + (",".join(forbidden) if forbidden else "NONE"),
        "note=large final-metric, benchmark, transition, and capture buffers must be absent",
    ]
    (output_dir / "production_symbol_audit.txt").write_text(
        "\n".join(audit_lines) + "\n", encoding="ascii"
    )

    ddr = next((item for item in regions if item["name"] == "DDR2"), None)
    evidence = {
        "schema_version": 1,
        "stage": "build",
        "status": "MEASURED",
        "measurement_label": "BUILD_EVIDENCE",
        "result": "FAIL" if build_failures else "PASS",
        "profile": profile.get("profile"),
        "warning_count": profile.get("warning_count"),
        "error_count": profile.get("error_count"),
        "link_errors": profile.get("link_errors"),
        "defines": defines,
        "missing_required_defines": missing_defines,
        "sections": {name: profile.get(field) for name, field in section_fields.items()},
        "ddr": ddr,
        "state_sizes": {
            row["name"]: row.get("size_bytes") for row in rows if row["kind"] == "state"
        },
        "framebuffer_result": framebuffer_result,
        "production_symbol_audit_result": audit_result,
        "out_sha256": profile.get("out_sha256"),
        "failures": build_failures,
    }
    write_json(output_dir / "build_summary.json", evidence)
    return evidence


VISUAL_ITEMS = (
    ("circular_offset", "No cyclic screen offset"),
    ("full_screen_white_flash", "No full-screen white flash"),
    ("old_page_ghosting", "No old-page ghosting"),
    ("element_misalignment", "No element misalignment"),
)


def process_visual(raw_dir: Path, output_dir: Path) -> dict[str, Any]:
    value, error = read_json(raw_dir / "operator_visual_observation_raw.json")
    valid = isinstance(value, dict) and value.get("result_label") == "OPERATOR_VISUAL_OBSERVATION"
    lines = ["# Operator Visual Observation", ""]
    if not valid:
        lines.extend(
            [
                "Status: `PENDING_OPERATOR`",
                "",
                "Automated counters are not visual evidence. No visual PASS is claimed.",
                "",
            ]
        )
        if error:
            lines.extend([f"Raw-file error: `{error}`", ""])
        for key, description in VISUAL_ITEMS:
            lines.append(f"- [PENDING] `{key}`: {description}")
        evidence = {"status": "PENDING_OPERATOR", "result": "NOT_RECORDED"}
    else:
        observations = value.get("observations") if isinstance(value.get("observations"), dict) else {}
        lines.extend(
            [
                "Status: `OPERATOR_VISUAL_OBSERVATION`",
                "",
                f"Operator: {value.get('operator', '')}",
                f"Observed at: {value.get('observed_at', '')}",
                "",
            ]
        )
        statuses = []
        for key, description in VISUAL_ITEMS:
            observed = observations.get(key, "PENDING")
            statuses.append(str(observed).upper())
            lines.append(f"- `{key}`: {observed} ({description})")
        notes = str(value.get("notes") or "")
        lines.extend(["", "Notes:", "", notes or "None supplied."])
        if any(item == "FAIL" for item in statuses):
            result = "FAIL"
        elif all(item == "PASS" for item in statuses):
            result = "PASS"
        else:
            result = "INCOMPLETE"
        evidence = {"status": "OPERATOR_VISUAL_OBSERVATION", "result": result}
    (output_dir / "operator_visual_observation.md").write_text(
        "\n".join(lines) + "\n", encoding="utf-8"
    )
    return evidence


def process_external_inventory(raw_dir: Path, output_dir: Path) -> dict[str, Any]:
    files = []
    if raw_dir.is_dir():
        for path in sorted(item for item in raw_dir.rglob("*") if item.is_file()):
            files.append(
                {
                    "file_name": path.name,
                    "logical_path": path.relative_to(raw_dir).as_posix(),
                    "bytes": path.stat().st_size,
                    "sha256": sha256_file(path),
                    "status": "EXTERNAL_RAW_EVIDENCE",
                }
            )
    evidence = {
        "schema_version": 1,
        "artifact_policy": "Raw board/build artifacts remain outside Git evidence source files",
        "raw_directory": str(raw_dir.resolve()),
        "files": files,
    }
    write_json(output_dir / "external_file_inventory.json", evidence)
    return evidence


def result_word(value: dict[str, Any]) -> str:
    return str(value.get("result") or value.get("status") or "UNKNOWN")


def write_readme(
    output_dir: Path,
    build: dict[str, Any],
    boot: dict[str, Any],
    no_touch: dict[str, Any],
    hitboxes: dict[str, Any],
    page: dict[str, Any],
    visual: dict[str, Any],
    timing: dict[str, Any],
    endurance: dict[str, Any],
    analyzer: dict[str, Any],
) -> None:
    rows = (
        ("Full H clean build", build.get("status"), result_word(build)),
        ("Production boot", boot.get("status"), result_word(boot)),
        ("120 s no-touch validation", no_touch.get("status"), result_word(no_touch)),
        ("27-hitbox validation", hitboxes.get("status"), result_word(hitboxes)),
        ("30 scripted API page round trips", page.get("status"), result_word(page)),
        ("Operator visual observation", visual.get("status"), result_word(visual)),
        ("Nine LCD job timing classes", timing.get("status"), result_word(timing)),
        ("300 s non-touch endurance", endurance.get("status"), result_word(endurance)),
        ("Analyzer reset recovery", analyzer.get("status"), result_word(analyzer)),
    )
    lines = [
        "# Project 3.3 Final Board Acceptance Evidence",
        "",
        "This directory is generated from raw DSS/build records. Missing stages remain pending; failures are retained.",
        "",
        "| Item | Evidence label | Result |",
        "|---|---|---|",
    ]
    for name, status, result in rows:
        lines.append(f"| {name} | `{status}` | `{result}` |")
    lines.extend(
        [
            "",
            "## Evidence Boundaries",
            "",
            "- `MEASURED`: board counters or build artifacts actually captured.",
            "- `OPERATOR_VISUAL_OBSERVATION`: human visual evidence only.",
            "- `HOST_ONLY`: offline processing or contract-test evidence only.",
            "- `NOT_MEASURED`: no calibrated external analog measurement was performed.",
            "- `PENDING_HARDWARE`: the corresponding board stage has not run.",
            "- `WAIVED`: explicitly removed from the acceptance scope by the user.",
            "",
        ]
    )
    for name, status in EXTERNAL_BOUNDARIES.items():
        lines.append(f"- `{name}`: `{status}`")
    lines.extend(
        [
            "",
            "Raw JSONL/JSON, complete build logs, map, link XML, nm output, and `.out` are listed in `external_file_inventory.json`.",
        ]
    )
    (output_dir / "README.md").write_text("\n".join(lines) + "\n", encoding="utf-8")


def write_final_acceptance_summary(
    output_dir: Path, stage_results: dict[str, dict[str, Any]]
) -> None:
    rows = [
        {
            "stage": name,
            "status": value.get("status"),
            "result": value.get("result"),
            "reason": value.get("reason", ""),
        }
        for name, value in stage_results.items()
    ]
    write_csv(
        output_dir / "final_acceptance_summary.csv",
        ("stage", "status", "result", "reason"),
        rows,
    )


def write_manifest(output_dir: Path, stage_results: dict[str, dict[str, Any]]) -> None:
    files = []
    for name in REQUIRED_OUTPUTS:
        if name in {"evidence_manifest.json", "sha256_manifest.txt"}:
            continue
        path = output_dir / name
        if path.is_file():
            files.append(
                {
                    "file_name": name,
                    "bytes": path.stat().st_size,
                    "sha256": sha256_file(path),
                }
            )
    failures = [name for name, value in stage_results.items() if value.get("result") == "FAIL"]
    waived_stages = [
        name
        for name, value in stage_results.items()
        if value.get("status") == "WAIVED"
        and value.get("result") == "NOT_REQUIRED"
    ]
    pending_stages = [
        name
        for name, value in stage_results.items()
        if name not in waived_stages
        and (
            value.get("status")
            in {"PENDING_HARDWARE", "PENDING_OPERATOR", "NOT_MEASURED"}
            or value.get("result")
            in {"NOT_RUN", "PENDING_OPERATOR", "INCOMPLETE", "NOT_RECORDED"}
        )
    ]
    manifest = {
        "schema_version": 1,
        "generated_at_utc": utc_now(),
        "overall_result": "FAIL" if failures else ("PENDING" if pending_stages else "PASS"),
        "failed_stages": failures,
        "pending_stages": pending_stages,
        "waived_stages": waived_stages,
        "measurement_boundaries": EXTERNAL_BOUNDARIES,
        "stage_results": {
            name: {
                "status": value.get("status"),
                "result": value.get("result"),
                "reason": value.get("reason"),
            }
            for name, value in stage_results.items()
        },
        "files": files,
    }
    write_json(output_dir / "evidence_manifest.json", manifest)


def write_checksum_manifest(output_dir: Path) -> None:
    lines = []
    for path in sorted(item for item in output_dir.iterdir() if item.is_file()):
        if path.name == "sha256_manifest.txt":
            continue
        lines.append(f"{sha256_file(path)}  {path.name}")
    (output_dir / "sha256_manifest.txt").write_text(
        "\n".join(lines) + "\n", encoding="ascii"
    )


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--raw-dir", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    raw_dir = args.raw_dir.resolve()
    output_dir = args.output_dir.resolve()
    output_dir.mkdir(parents=True, exist_ok=True)

    build = process_build(raw_dir, output_dir)
    boot = process_production_boot(raw_dir, output_dir)
    no_touch = process_no_touch(raw_dir, output_dir)
    hitboxes = process_hitboxes(raw_dir, output_dir)
    page = process_page_switch(raw_dir, output_dir)
    timing = process_timing(raw_dir, output_dir)
    endurance = process_endurance(raw_dir, output_dir)
    analyzer = process_analyzer_reset(raw_dir, output_dir)
    visual = process_visual(raw_dir, output_dir)
    process_external_inventory(raw_dir, output_dir)
    write_readme(
        output_dir,
        build,
        boot,
        no_touch,
        hitboxes,
        page,
        visual,
        timing,
        endurance,
        analyzer,
    )
    stage_results = {
        "build": build,
        "production_boot": boot,
        "no_touch_120s": no_touch,
        "touch_hitbox_27": hitboxes,
        "page_switch_30": page,
        "operator_visual": visual,
        "lcd_job_timing": timing,
        "endurance_300s": endurance,
        "analyzer_reset": analyzer,
    }
    write_final_acceptance_summary(output_dir, stage_results)
    write_manifest(output_dir, stage_results)
    write_checksum_manifest(output_dir)
    manifest = json.loads((output_dir / "evidence_manifest.json").read_text(encoding="utf-8"))
    print(f"EVIDENCE_DIR={output_dir}")
    print(f"OVERALL_RESULT={manifest['overall_result']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
