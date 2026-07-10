#!/usr/bin/env python3
"""Summarize the focused C6748 touch-UI board test CSV."""

from __future__ import annotations

import argparse
import csv
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_CSV = ROOT / "docs/eval_outputs/subband_touch_ui_test.csv"
DEFAULT_SUMMARY = ROOT / "docs/eval_outputs/subband_touch_ui_test_summary.md"


def as_int(row: dict[str, str], key: str) -> int:
    return int(row[key])


def find_row(rows: list[dict[str, str]], case: str, phase: str) -> dict[str, str]:
    for row in rows:
        if row["case_name"] == case and row["phase"] == phase:
            return row
    raise ValueError(f"missing row: {case}/{phase}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--csv", type=Path, default=DEFAULT_CSV)
    parser.add_argument("--summary", type=Path, default=DEFAULT_SUMMARY)
    args = parser.parse_args()

    with args.csv.open(newline="", encoding="utf-8") as handle:
        rows = list(csv.DictReader(handle))
    if not rows:
        raise ValueError("board test CSV is empty")

    modes = [0, 1, 2, 4, 6, 7, 8, 11]
    learning_modes = {2, 4, 6, 7, 8}
    mode_lines: list[str] = []
    for mode in modes:
        phase = "learning_complete" if mode in learning_modes else "entered"
        row = find_row(rows, f"mode_{mode}", phase)
        mode_lines.append(
            f"| {mode} | {row['applied_mode']} | {row['displayed_mode']} | "
            f"{row['learn_hops']}/{row['target_hops']} | {row['ready']} | "
            f"{'PASS' if row['pass'] == '1' else 'FAIL'} |"
        )

    rate_lines: list[str] = []
    for mode in (11, 8):
        for rate in (160, 240, 320):
            row = find_row(rows, f"mode_{mode}_rate_{rate}", "dynamic_rate")
            rate_lines.append(
                f"| {mode} | {rate} | {row['target_kbps']} | "
                f"{float(row['estimated_bitrate_kbps']):.4f} | "
                f"{'PASS' if row['pass'] == '1' else 'FAIL'} |"
            )

    realtime = find_row(rows, "mode_8_320_realtime", "300_seconds")
    stress_mode = find_row(rows, "mode_switch_stress", "50_switches")
    stress_rate = find_row(rows, "rate_switch_stress", "50_switches")
    duplicate = find_row(rows, "duplicate_current_mode", "ten_requests")
    failed = [row for row in rows if row["pass"] != "1"]
    commit_shas = {row["commit_sha"] for row in rows}
    if len(commit_shas) != 1:
        raise ValueError(f"mixed commit SHA values: {sorted(commit_shas)}")

    lines = [
        "# Subband touch UI board-test summary",
        "",
        f"- Tested commit: `{next(iter(commit_shas))}`",
        f"- DSS rows: {len(rows)}; PASS: {len(rows) - len(failed)}; FAIL: {len(failed)}",
        "- Build: CCS Debug with `-O3`; link result is reported separately from the build log.",
        "",
        "## Mode results",
        "",
        "| Requested | Applied | Displayed | Learn hops | Ready | Result |",
        "|---:|---:|---:|---:|---:|---|",
        *mode_lines,
        "",
        "## Dynamic codec-rate results",
        "",
        "| Mode | Selected kbps | Applied kbps | Estimated kbps | Result |",
        "|---:|---:|---:|---:|---|",
        *rate_lines,
        "",
        "## Stress and real-time results",
        "",
        f"- 50 mode switches: {'PASS' if stress_mode['pass'] == '1' else 'FAIL'}; "
        f"full redraw count `{stress_mode['ui_full_redraw_count']}`.",
        f"- 50 codec-rate switches: {'PASS' if stress_rate['pass'] == '1' else 'FAIL'}; "
        f"invalid count `{stress_rate['invalid_count']}`.",
        f"- Repeated current-mode request guard: "
        f"{'PASS' if duplicate['pass'] == '1' else 'FAIL'}.",
        f"- Mode 8 / 320 kbps / UI, 300 s: "
        f"{'PASS' if realtime['pass'] == '1' else 'FAIL'}.",
        f"- Frame deltas: AD `{realtime['ad_delta']}`, DA `{realtime['da_delta']}`, "
        f"algorithm `{realtime['algo_delta']}`.",
        f"- Algorithm MaxMs: `{float(realtime['algo_max_ms']):.6f}`; "
        f"UI MaxDrawMs: `{float(realtime['ui_max_draw_ms']):.6f}`; "
        f"UI MaxTouchMs: `{float(realtime['ui_max_touch_ms']):.6f}`.",
        f"- UI refresh delta: `{realtime['refresh_delta']}`; "
        f"full redraw count: `{realtime['ui_full_redraw_count']}`; "
        f"font bitmap bytes: `{realtime['ui_font_bytes']}`.",
        "",
        "## Measurement boundaries",
        "",
        "- DSS verifies board state, frame continuity, mode/rate application, learning counters, "
        "invalid count, and cycle counters.",
        "- Physical long-press feel, visible coordinate drift/flicker, and subjective audio "
        "continuity require operator observation and are not inferred from Watch values.",
    ]
    if failed:
        lines.extend(
            [
                "",
                "## Failed rows",
                "",
                *[f"- `{row['case_name']}/{row['phase']}`" for row in failed],
            ]
        )
    args.summary.parent.mkdir(parents=True, exist_ok=True)
    args.summary.write_text("\n".join(lines) + "\n", encoding="utf-8")
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
