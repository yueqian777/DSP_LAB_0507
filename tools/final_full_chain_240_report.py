"""Validate and summarize the final C6748 mode-8 / 240-kbps board rerun."""

from __future__ import annotations

import csv
import math
from pathlib import Path
from statistics import median

from PIL import Image, ImageDraw, ImageFont


ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "docs" / "eval_outputs"
CSV_PATH = OUT / "final_full_chain_240_rerun.csv"
DIAGNOSTICS_PATH = OUT / "final_full_chain_240_diagnostics.csv"
AUDIO_PATH = OUT / "final_full_chain_240_audio_metrics.csv"
SUMMARY_PATH = OUT / "final_full_chain_240_summary.md"
PLOTS = OUT / "plots"
BUDGET_MS = 20.48


def value(row: dict[str, str], key: str) -> float:
    try:
        return float(row.get(key, "nan"))
    except ValueError:
        return float("nan")


def count(row: dict[str, str], key: str) -> int:
    number = value(row, key)
    return 0 if math.isnan(number) else int(number)


def is_valid(row: dict[str, str]) -> bool:
    return (
        count(row, "valid_repeat") == 1
        and count(row, "ad_frames") >= 470
        and count(row, "da_frames") >= 470
        and count(row, "algo_frames") >= 470
        and count(row, "input_nonzero_frames") > 0
        and count(row, "learning_hops") == count(row, "target_learning_hops")
        and value(row, "learning_progress") >= 0.9999
        and count(row, "denoise_ready") == 1
        and count(row, "codec_frames") > 0
        and 200.0 < value(row, "estimated_bitrate_kbps") < 280.0
        and count(row, "invalid_count") == 0
        and value(row, "max_ms") < BUDGET_MS
    )


def select(rows: list[dict[str, str]], *keys: str) -> list[dict[str, str]]:
    return [{key: row.get(key, "NOT_MEASURED") for key in keys} for row in rows]


def write_csv(path: Path, rows: list[dict[str, str]]) -> None:
    with path.open("w", newline="", encoding="utf-8") as output:
        writer = csv.DictWriter(output, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)


def get_font(size: int, bold: bool = False):
    path = r"C:\Windows\Fonts\arialbd.ttf" if bold else r"C:\Windows\Fonts\arial.ttf"
    return ImageFont.truetype(path, size) if Path(path).exists() else ImageFont.load_default()


TITLE, TEXT, SMALL = get_font(31, True), get_font(18), get_font(14)


def canvas(title: str, subtitle: str):
    image = Image.new("RGB", (1400, 820), "white")
    draw = ImageDraw.Draw(image)
    draw.text((50, 35), title, fill="#10213f", font=TITLE)
    draw.text((50, 86), subtitle, fill="#5b6780", font=TEXT)
    return image, draw


def bar_plot(path: Path, title: str, subtitle: str, labels: list[str], values: list[float], unit: str, maximum: float, color: str) -> None:
    image, draw = canvas(title, subtitle)
    left, top, right, bottom = 130, 170, 1320, 630
    draw.line((left, bottom, right, bottom), fill="#10213f", width=2)
    draw.line((left, top, left, bottom), fill="#10213f", width=2)
    for tick in range(0, 6):
        y = bottom - (bottom - top) * tick / 5
        draw.line((left, y, right, y), fill="#e5ebf3")
        draw.text((55, y - 8), f"{maximum * tick / 5:.2f}", fill="#5b6780", font=SMALL)
    step = (right - left) / (len(values) + 1)
    for index, (label, item) in enumerate(zip(labels, values), start=1):
        x = left + index * step
        height = (bottom - top) * item / maximum if maximum else 0
        draw.rectangle((x - 75, bottom - height, x + 75, bottom), fill=color)
        draw.text((x - 65, bottom - height - 28), f"{item:.4f} {unit}", fill="#10213f", font=TEXT)
        draw.text((x - 35, bottom + 20), label, fill="#10213f", font=TEXT)
    image.save(path)


def audio_plot(path: Path) -> None:
    image, draw = canvas("Audio capture metrics", "No controllable board-output capture device was exposed to this session.")
    draw.rectangle((70, 180, 1330, 570), outline="#b91c1c", width=3)
    draw.text((115, 300), "NOT_MEASURED_AUDIO_CAPTURE_UNAVAILABLE", fill="#b91c1c", font=get_font(32, True))
    draw.text((115, 365), "No aligned SNR, correlation, spectrum error, or clipping conclusion is reported.", fill="#5b6780", font=TEXT)
    image.save(path)


def fmt(items: list[dict[str, str]], key: str, kind: str = ".4f") -> str:
    values = [value(item, key) for item in items]
    return f"median {median(values):{kind}}, worst {max(values):{kind}}"


def main() -> None:
    with CSV_PATH.open(newline="", encoding="utf-8-sig") as source:
        rows = list(csv.DictReader(source))
    if len(rows) != 3:
        raise SystemExit(f"expected exactly three final repeats, got {len(rows)}")
    if not all(is_valid(row) for row in rows):
        raise SystemExit("one or more rows do not satisfy final-chain validity criteria")
    if len({row["commit_sha"] for row in rows}) != 1:
        raise SystemExit("commit_sha differs across repeats")

    diagnostics = select(rows,
        "repeat_index", "valid_repeat", "ad_frames", "da_frames", "algo_frames", "codec_frames",
        "last_cycles", "max_cycles", "last_ms", "max_ms", "cpu_usage_percent",
        "learning_hops", "target_learning_hops", "learning_progress", "denoise_ready",
        "estimated_bitrate_kbps", "compression_ratio", "avg_bits_per_scalar",
        "quantizer_clamp_count", "total_scalar_count", "quantizer_clamp_ratio", "invalid_count",
        "input_power_avg", "output_power_avg", "gain_avg", "min_gain", "max_gain", "noise_psd_avg",
        "mcra_speech_probability_avg", "mcra_overdrive_avg", "mcra_floor_avg",
        "noise_phase_speech_probability_avg", "noise_phase_gain_avg", "speech_phase_speech_probability_avg", "speech_phase_gain_avg",
        "input_peak_max", "output_peak_max", "output_input_peak_ratio",
        "input_mean_square_avg", "output_mean_square_avg", "output_input_energy_ratio", "output_input_rms_ratio",
        "input_clip_frames", "output_clip_frames", "band_bits_0", "band_bits_1", "band_bits_2", "band_bits_3", "band_bits_4", "band_bits_5", "band_bits_6", "band_bits_7")
    write_csv(DIAGNOSTICS_PATH, diagnostics)
    audio_rows = []
    for row in rows:
        audio_rows.append({
            "repeat_index": row["repeat_index"],
            "audio_capture_status": "NOT_MEASURED_AUDIO_CAPTURE_UNAVAILABLE",
            "aligned_snr_db": "NOT_MEASURED_AUDIO_CAPTURE_UNAVAILABLE",
            "input_snr_db": "NOT_MEASURED_AUDIO_CAPTURE_UNAVAILABLE",
            "output_snr_db": "NOT_MEASURED_AUDIO_CAPTURE_UNAVAILABLE",
            "snr_improvement_db": "NOT_MEASURED_AUDIO_CAPTURE_UNAVAILABLE",
            "clean_output_correlation": "NOT_MEASURED_AUDIO_CAPTURE_UNAVAILABLE",
            "speech_preservation_ratio": "NOT_MEASURED_AUDIO_CAPTURE_UNAVAILABLE",
            "input_output_energy_ratio": "NOT_MEASURED_AUDIO_CAPTURE_UNAVAILABLE",
            "spectrum_error_0_8k": "NOT_MEASURED_AUDIO_CAPTURE_UNAVAILABLE",
            "waveform_clipping": "NOT_MEASURED_AUDIO_CAPTURE_UNAVAILABLE",
        })
    write_csv(AUDIO_PATH, audio_rows)

    PLOTS.mkdir(parents=True, exist_ok=True)
    labels = [f"run {row['repeat_index']}" for row in rows]
    bar_plot(PLOTS / "final_full_chain_runtime.png", "Mode 8 runtime", "C6748 hardware max frame time; budget is 20.48 ms.", labels, [value(row, "max_ms") for row in rows], "ms", 20.48, "#2563eb")
    bar_plot(PLOTS / "final_full_chain_gain.png", "MCRA gain average", "Integer-scaled Watch mirror, exact to 1e-6.", labels, [value(row, "gain_avg") for row in rows], "gain", 1.0, "#16a34a")
    bar_plot(PLOTS / "final_full_chain_input_output_level.png", "Input/output RMS ratio", "Derived from board-side integer frame mean-square averages.", labels, [value(row, "output_input_rms_ratio") for row in rows], "ratio", 1.0, "#f59e0b")
    audio_plot(PLOTS / "final_full_chain_audio_metrics.png")

    speech_changed = any(abs(value(row, "speech_phase_speech_probability_avg") - value(row, "noise_phase_speech_probability_avg")) >= 0.02 for row in rows)
    gain_near_floor = median(value(row, "gain_avg") for row in rows) <= median(value(row, "mcra_floor_avg") for row in rows) + 0.03
    summary = [
        "# Final C6748 Full-Chain 240 kbps Rerun",
        "",
        f"- Measured commit: `{rows[0]['commit_sha']}`.",
        "- Three repeats are valid according to frame, learning, codec, invalid-count, and real-time criteria.",
        f"- MaxMs: {fmt(rows, 'max_ms')} ms; CPU: {fmt(rows, 'cpu_usage_percent')}%.",
        f"- Estimated bitrate: {fmt(rows, 'estimated_bitrate_kbps')} kbps; compression ratio: {fmt(rows, 'compression_ratio')}x.",
        f"- Clamp ratio: {fmt(rows, 'quantizer_clamp_ratio', '.6f')}; invalid count worst: {max(count(row, 'invalid_count') for row in rows)}.",
        f"- Peak ratio: {fmt(rows, 'output_input_peak_ratio', '.6f')}; RMS ratio: {fmt(rows, 'output_input_rms_ratio', '.6f')}.",
        f"- Speech-probability change from the noise checkpoint: {'observed' if speech_changed else 'not observed at the 0.02 threshold'}.",
        f"- Gain near MCRA floor: {'yes' if gain_near_floor else 'no'}.",
        "- Audio recording status: `NOT_MEASURED_AUDIO_CAPTURE_UNAVAILABLE`; no audio-quality conclusion is made.",
        "",
        "## Interpretation",
        "",
        "A low output level together with gain near the configured MCRA floor indicates denoise attenuation. This diagnostic alone cannot establish that MCRA is the sole cause: the board input RMS and source metadata must also be considered. Codec-only historical tests are not mixed into this result.",
        "",
        "## Artifacts",
        "",
        "- `docs/eval_outputs/final_full_chain_240_rerun.csv`",
        "- `docs/eval_outputs/final_full_chain_240_diagnostics.csv`",
        "- `docs/eval_outputs/final_full_chain_240_audio_metrics.csv`",
        "- `docs/eval_outputs/dss_final_full_chain.log`",
        "- `docs/eval_outputs/plots/final_full_chain_runtime.png`",
        "- `docs/eval_outputs/plots/final_full_chain_gain.png`",
        "- `docs/eval_outputs/plots/final_full_chain_input_output_level.png`",
        "- `docs/eval_outputs/plots/final_full_chain_audio_metrics.png`",
    ]
    SUMMARY_PATH.write_text("\n".join(summary) + "\n", encoding="utf-8")
    print(SUMMARY_PATH)


if __name__ == "__main__":
    main()
