"""Summarize the C6748 board missing-tests rerun without mixing history."""

from __future__ import annotations

import csv
import math
from collections import defaultdict
from pathlib import Path
from statistics import median

from PIL import Image, ImageDraw, ImageFont


ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "docs" / "eval_outputs"
CSV_PATH = OUT / "board_missing_tests_rerun.csv"
SUMMARY_PATH = OUT / "board_missing_tests_rerun_summary.md"
MAP_SUMMARY_PATH = OUT / "board_missing_tests_map_summary.md"
PLOTS = OUT / "plots"
RUNTIME_PLOT = PLOTS / "board_runtime_compare_rerun.png"
BITRATE_PLOT = PLOTS / "codec_bitrate_compare_rerun.png"
BAND_BITS_PLOT = PLOTS / "codec_band_bits_compare_rerun.png"
MAP_PATH = ROOT / "Debug" / "DSP_LAB_0507.map"
LINK_INFO_PATH = ROOT / "Debug" / "DSP_LAB_0507_linkInfo.xml"
FRAME_BUDGET_MS = 20.48
PREVIOUS_POLYPHASE_MAX_MS = 17.53


def number(row: dict[str, str], key: str, fallback: float = 0.0) -> float:
    try:
        value = float(row.get(key, ""))
    except ValueError:
        return fallback
    return value if math.isfinite(value) else fallback


def whole(row: dict[str, str], key: str, fallback: int = 0) -> int:
    return int(number(row, key, float(fallback)))


def valid(row: dict[str, str]) -> bool:
    return whole(row, "valid_repeat") == 1


def rows_for(rows: list[dict[str, str]], **criteria: object) -> list[dict[str, str]]:
    result = []
    for row in rows:
        matched = True
        for key, value in criteria.items():
            if str(row.get(key, "")) != str(value):
                matched = False
                break
        if matched:
            result.append(row)
    return result


def metric_values(rows: list[dict[str, str]], key: str) -> list[float]:
    return [number(row, key) for row in rows if valid(row)]


def median_or_na(values: list[float], digits: int = 4) -> str:
    return f"{median(values):.{digits}f}" if values else "NOT_MEASURED"


def max_or_na(values: list[float], digits: int = 4) -> str:
    return f"{max(values):.{digits}f}" if values else "NOT_MEASURED"


def font(size: int, bold: bool = False):
    candidates = [
        r"C:\Windows\Fonts\arialbd.ttf" if bold else r"C:\Windows\Fonts\arial.ttf",
        r"C:\Windows\Fonts\segoeuib.ttf" if bold else r"C:\Windows\Fonts\segoeui.ttf",
    ]
    for candidate in candidates:
        if Path(candidate).exists():
            return ImageFont.truetype(candidate, size)
    return ImageFont.load_default()


TITLE = font(32, True)
LABEL = font(19, True)
TEXT = font(17)
SMALL = font(14)


def canvas(title: str, subtitle: str) -> tuple[Image.Image, ImageDraw.ImageDraw]:
    image = Image.new("RGB", (1400, 820), "white")
    draw = ImageDraw.Draw(image)
    draw.text((55, 35), title, font=TITLE, fill="#10213f")
    draw.text((55, 84), subtitle, font=TEXT, fill="#5b6780")
    return image, draw


def method_plot(rows: list[dict[str, str]]) -> None:
    labels = [
        ("legacy_fir", "Legacy FIR", "#2563eb"),
        ("polyphase_project1", "Project1 polyphase", "#dc2626"),
        ("wola", "WOLA-DFT", "#16a34a"),
    ]
    image, draw = canvas(
        "C6748 analysis/synthesis runtime rerun",
        "This plot uses only docs/eval_outputs/board_missing_tests_rerun.csv.",
    )
    left, top, right, bottom = 120, 170, 1320, 610
    draw.line((left, bottom, right, bottom), fill="#10213f", width=2)
    draw.line((left, top, left, bottom), fill="#10213f", width=2)
    ymax = 22.0
    for tick in range(0, 23, 2):
        y = bottom - (bottom - top) * tick / ymax
        draw.line((left, y, right, y), fill="#e5ebf3")
        draw.text((55, y - 8), f"{tick} ms", font=SMALL, fill="#5b6780")
    budget_y = bottom - (bottom - top) * FRAME_BUDGET_MS / ymax
    draw.line((left, budget_y, right, budget_y), fill="#d97706", width=3)
    draw.text((right - 220, budget_y - 28), "budget 20.48 ms", font=TEXT, fill="#9a5a00")
    for index, (key, label, color) in enumerate(labels):
        samples = rows_for(rows, test_name="method_compare", backend=key)
        valid_samples = [row for row in samples if valid(row)]
        values = [number(row, "max_ms") for row in valid_samples]
        x = 330 + index * 370
        value = median(values) if values else 0.0
        height = (bottom - top) * value / ymax
        draw.rectangle((x - 85, bottom - height, x + 85, bottom), fill=color)
        draw.text((x - 80, bottom - height - 60), f"median {median_or_na(values)} ms", font=TEXT, fill="#10213f")
        draw.text((x - 80, bottom - height - 35), f"worst {max_or_na(values)} ms", font=SMALL, fill="#10213f")
        draw.text((x - 85, bottom + 18), f"valid {len(valid_samples)}/3", font=SMALL, fill=color)
        draw.text((x - 85, bottom + 48), label, font=LABEL, fill="#10213f")
    image.save(RUNTIME_PLOT)


def bitrate_plot(rows: list[dict[str, str]]) -> None:
    image, draw = canvas(
        "C6748 codec-only bitrate rerun",
        "WOLA backend, mode 11, denoise off; only current rerun CSV is plotted.",
    )
    left, top, right, bottom = 120, 180, 1320, 670
    draw.line((left, bottom, right, bottom), fill="#10213f", width=2)
    draw.line((left, top, left, bottom), fill="#10213f", width=2)
    ymax = 360.0
    for tick in range(0, 361, 60):
        y = bottom - (bottom - top) * tick / ymax
        draw.line((left, y, right, y), fill="#e5ebf3")
        draw.text((55, y - 8), str(tick), font=SMALL, fill="#5b6780")
    for target, x in zip((160, 240, 320), (350, 700, 1050)):
        samples = [row for row in rows_for(rows, test_name="codec_only", target_kbps=target) if valid(row)]
        actual_values = [number(row, "estimated_bitrate_kbps") for row in samples]
        actual = median(actual_values) if actual_values else 0.0
        target_h = (bottom - top) * target / ymax
        actual_h = (bottom - top) * actual / ymax
        draw.rectangle((x - 95, bottom - target_h, x - 12, bottom), fill="#93c5fd")
        draw.rectangle((x + 12, bottom - actual_h, x + 95, bottom), fill="#2563eb")
        draw.text((x - 90, bottom + 20), f"target {target}", font=TEXT, fill="#10213f")
        draw.text((x - 90, bottom + 48), f"actual {median_or_na(actual_values)}", font=TEXT, fill="#10213f")
        draw.text((x - 90, bottom + 76), f"valid {len(samples)}/3", font=SMALL, fill="#5b6780")
    image.save(BITRATE_PLOT)


def band_plot(rows: list[dict[str, str]]) -> None:
    image, draw = canvas(
        "C6748 codec-only BandBits rerun",
        "Median BandBits0..7 from current valid rerun rows.",
    )
    left, top, right, bottom = 130, 190, 1330, 670
    draw.line((left, bottom, right, bottom), fill="#10213f", width=2)
    draw.line((left, top, left, bottom), fill="#10213f", width=2)
    for target, color, x in zip((160, 240, 320), ("#2563eb", "#16a34a", "#f59e0b"), (320, 700, 1080)):
        samples = [row for row in rows_for(rows, test_name="codec_only", target_kbps=target) if valid(row)]
        for band in range(8):
            values = [number(row, f"band_bits_{band}") for row in samples]
            value = median(values) if values else 0.0
            height = (bottom - top) * value / 7.0
            bx = x - 170 + band * 44
            draw.rectangle((bx, bottom - height, bx + 38, bottom), fill=color)
            draw.text((bx + 5, bottom + 12), str(band), font=SMALL, fill="#5b6780")
        draw.text((x - 90, bottom + 48), f"{target} kbps", font=LABEL, fill="#10213f")
    image.save(BAND_BITS_PLOT)


def map_summary() -> str:
    if not MAP_PATH.exists():
        text = "# Board Missing Tests Map Summary\n\n`Debug/DSP_LAB_0507.map` was not found.\n"
        MAP_SUMMARY_PATH.write_text(text, encoding="utf-8")
        return text

    lines = MAP_PATH.read_text(encoding="utf-8", errors="ignore").splitlines()
    selected: list[str] = []
    seen: set[str] = set()
    capture = False

    def add(line: str) -> None:
        text = line.rstrip()
        if text and text not in seen:
            selected.append(text)
            seen.add(text)

    for line in lines:
        if "DSPL2RAM" in line or ".subband_l2" in line:
            add(line)
        if line.startswith(".subband_l2"):
            capture = True
            continue
        if capture:
            if line.startswith(".") and not line.startswith(".subband_l2"):
                capture = False
            elif line.strip():
                add(line)
        if "user_subband_polyphase.obj" in line and (
            "SubbandPolyphase" in line or ".subband_l2" in line or ".far" in line or ".const" in line
        ):
            add(line)

    link_errors = "NOT_MEASURED"
    if LINK_INFO_PATH.exists():
        link_text = LINK_INFO_PATH.read_text(encoding="utf-8", errors="ignore")
        link_errors = "0x0" if "<link_errors>0x0</link_errors>" in link_text else "NONZERO_OR_UNKNOWN"

    text_lines = [
        "# Board Missing Tests Map Summary",
        "",
        f"- Map file: `{MAP_PATH}`",
        f"- Link errors: `{link_errors}`",
        "- Selected `.subband_l2` / backend memory lines:",
        "",
        "```text",
        *selected[:120],
        "```",
    ]
    text = "\n".join(text_lines) + "\n"
    MAP_SUMMARY_PATH.write_text(text, encoding="utf-8")
    return text


def summarize(rows: list[dict[str, str]]) -> str:
    commit_values = sorted({row.get("commit_sha", "") for row in rows})
    grouped_status = defaultdict(int)
    for row in rows:
        grouped_status[row.get("measurement_status", "UNKNOWN")] += 1

    lines = [
        "# Project 3.2 C6748 Board Missing Tests Rerun",
        "",
        "## Traceability",
        "",
        f"- Source CSV: `{CSV_PATH}`",
        f"- Commit SHA(s): {', '.join(f'`{item}`' for item in commit_values)}",
        "- Hardware/config: C6748, 456 MHz, 50 kHz, 1024 samples/frame, CH1, Debug/O3.",
        "- Timing is derived from cycle counts: `ms = cycles / 456000.0`, `CPU = MaxMs / 20.48 * 100%`.",
        "- This summary and all plots use only `board_missing_tests_rerun.csv`.",
        "",
        "## Method Comparison",
        "",
        "| Backend | Valid repeats | Median MaxMs | Worst MaxMs | Median CPU | Peak ratio median | Statuses |",
        "|---|---:|---:|---:|---:|---:|---|",
    ]

    for backend, label in (("legacy_fir", "legacy FIR"),
                           ("polyphase_project1", "project1 polyphase"),
                           ("wola", "WOLA-DFT")):
        samples = rows_for(rows, test_name="method_compare", backend=backend)
        valid_samples = [row for row in samples if valid(row)]
        max_ms = metric_values(samples, "max_ms")
        cpu = metric_values(samples, "cpu_usage_percent")
        ratio = metric_values(samples, "output_input_peak_ratio")
        statuses = ", ".join(sorted({row["measurement_status"] for row in samples}))
        lines.append(
            f"| {label} | {len(valid_samples)}/3 | {median_or_na(max_ms)} | "
            f"{max_or_na(max_ms)} | {median_or_na(cpu)}% | "
            f"{median_or_na(ratio, 6)} | {statuses} |"
        )

    poly_values = metric_values(rows_for(rows, test_name="method_compare", backend="polyphase_project1"), "max_ms")
    if poly_values:
        poly_worst = max(poly_values)
        delta = PREVIOUS_POLYPHASE_MAX_MS - poly_worst
        lines.append("")
        lines.append(
            f"- Project1 polyphase worst MaxMs versus previous 17.53 ms: "
            f"{poly_worst:.4f} ms, delta {delta:.4f} ms."
        )

    lines += [
        "",
        "## Codec Only",
        "",
        "| Target kbps | Valid repeats | Actual target | Estimated kbps median | BandBits median | Worst MaxMs | Median CPU | Invalid max |",
        "|---:|---:|---:|---:|---|---:|---:|---:|",
    ]
    for target in (160, 240, 320):
        samples = rows_for(rows, test_name="codec_only", target_kbps=target)
        valid_samples = [row for row in samples if valid(row)]
        bitrates = metric_values(samples, "estimated_bitrate_kbps")
        max_ms = metric_values(samples, "max_ms")
        cpu = metric_values(samples, "cpu_usage_percent")
        actual_targets = sorted({row.get("actual_target_kbps", "") for row in samples})
        bands = []
        for band in range(8):
            values = [number(row, f"band_bits_{band}") for row in valid_samples]
            bands.append(f"{median(values):.2f}" if values else "NA")
        invalid_max = max([whole(row, "invalid_count", -1) for row in samples], default=-1)
        lines.append(
            f"| {target} | {len(valid_samples)}/3 | {','.join(actual_targets)} | "
            f"{median_or_na(bitrates)} | [{', '.join(bands)}] | "
            f"{max_or_na(max_ms)} | {median_or_na(cpu)}% | {invalid_max} |"
        )

    full = rows_for(rows, test_name="full_chain_240k")
    full_valid = [row for row in full if valid(row)]
    full_max_ms = metric_values(full, "max_ms")
    full_cpu = metric_values(full, "cpu_usage_percent")
    full_learn = sorted({f"{row.get('learning_hops')}/{row.get('target_learning_hops')}" for row in full})
    full_ready = sorted({row.get("denoise_ready", "") for row in full})
    lines += [
        "",
        "## Full Chain",
        "",
        f"- Valid repeats: {len(full_valid)}/3.",
        f"- Learning: {', '.join(full_learn) if full_learn else 'NOT_MEASURED'}; Ready values: {', '.join(full_ready) if full_ready else 'NOT_MEASURED'}.",
        f"- Mode 8 / 240 kbps MaxMs median/worst: {median_or_na(full_max_ms)} / {max_or_na(full_max_ms)} ms.",
        f"- Mode 8 / 240 kbps CPU median/worst: {median_or_na(full_cpu)}% / {max_or_na(full_cpu)}%.",
        "",
        "## Data Quality Flags",
        "",
        f"- Measurement statuses: {dict(sorted(grouped_status.items()))}",
        f"- Input silent rows: {sum(1 for row in rows if row.get('measurement_status') == 'NOT_MEASURED_INPUT_SILENT')}",
        f"- No-frame rows: {sum(1 for row in rows if row.get('measurement_status') == 'NOT_MEASURED_NO_FRAMES')}",
        f"- Rows with input clipping: {sum(1 for row in rows if whole(row, 'input_clip_frames') > 0)}",
        f"- Rows with output clipping: {sum(1 for row in rows if whole(row, 'output_clip_frames') > 0)}",
        f"- Rows with invalid codec count: {sum(1 for row in rows if whole(row, 'invalid_count') > 0)}",
        "",
        "## Artifacts",
        "",
        f"- `{CSV_PATH}`",
        f"- `{SUMMARY_PATH}`",
        f"- `{MAP_SUMMARY_PATH}`",
        f"- `{RUNTIME_PLOT}`",
        f"- `{BITRATE_PLOT}`",
        f"- `{BAND_BITS_PLOT}`",
        f"- `{OUT / 'dss_board_missing_tests_rerun.log'}`",
    ]
    return "\n".join(lines) + "\n"


def main() -> None:
    if not CSV_PATH.exists():
        raise SystemExit(f"missing rerun CSV: {CSV_PATH}")
    with CSV_PATH.open(newline="", encoding="utf-8-sig") as handle:
        rows = list(csv.DictReader(handle))
    if len(rows) != 21:
        raise SystemExit(f"expected 21 rerun rows, got {len(rows)}")

    PLOTS.mkdir(parents=True, exist_ok=True)
    method_plot(rows)
    bitrate_plot(rows)
    band_plot(rows)
    map_summary()
    SUMMARY_PATH.write_text(summarize(rows), encoding="utf-8")
    print(f"rows={len(rows)} summary={SUMMARY_PATH}")


if __name__ == "__main__":
    main()
