#!/usr/bin/env python3
"""Render deterministic Project 3.3 WAV A/B cases through the host C core.

The renderer is compiled into the temporary directory from the current
user_equalizer.c, so these WAV files exercise the C implementation rather
than a Python approximation.  The script never records from a microphone and
rejects any input that is not mono 16-bit PCM at 50 kHz.
"""

from __future__ import annotations

import argparse
import csv
import math
import os
import subprocess
import tempfile
import wave
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw


FS = 50000
FRAME = 1024
DEFAULT_DURATION_SECONDS = 2.0
TARGET_PEAK = 10.0 ** (-18.0 / 20.0)
ROOT = Path(__file__).resolve().parents[1]
DEFAULT_OUTPUT = ROOT / "docs" / "eval_outputs" / "equalizer_33_offline"

MODES = (
    ("raw_copy", 0, 0, "flat"),
    ("float_copy", 1, 0, "flat"),
    ("legacy_flat", 2, 0, "flat"),
    ("legacy_bass", 2, 1, "bass"),
    ("rbj_flat", 3, 0, "flat"),
    ("rbj_bass", 3, 1, "bass"),
    ("rbj_vocal", 3, 2, "vocal"),
    ("rbj_treble", 3, 3, "treble"),
    ("rbj_v_shape", 3, 4, "v_shape"),
)

RENDERER_SOURCE = r'''
#include <stdio.h>
#include <stdlib.h>
#include "user_equalizer.h"

int main(int argc, char **argv)
{
    FILE *input_file;
    FILE *output_file;
    EQ_STATE state;
    short input[1024];
    short output[1024];
    float zero = 0.0f;
    float discard;
    int core;
    int preset;
    int i;
    size_t count;

    if (argc != 5)
    {
        return 2;
    }
    core = atoi(argv[3]);
    preset = atoi(argv[4]);
    input_file = fopen(argv[1], "rb");
    output_file = fopen(argv[2], "wb");
    if ((input_file == NULL) || (output_file == NULL))
    {
        return 3;
    }
    Equalizer_Init(&state);
    Equalizer_SetCoreMode(&state, core);
    Equalizer_ApplyPreset(&state, preset);
    for (i = 0; i < 7000; i++)
    {
        Equalizer_ProcessFrameFloat(&state, &zero, &discard, 1);
    }
    while ((count = fread(input, sizeof(short), 1024, input_file)) > 0)
    {
        Equalizer_ProcessFrame(&state, input, output, (int)count);
        if (fwrite(output, sizeof(short), count, output_file) != count)
        {
            fclose(input_file);
            fclose(output_file);
            return 4;
        }
    }
    fclose(input_file);
    fclose(output_file);
    return 0;
}
'''


def require_wav_format(path: Path) -> np.ndarray:
    with wave.open(str(path), "rb") as handle:
        if handle.getnchannels() != 1:
            raise ValueError(f"{path}: expected mono WAV")
        if handle.getsampwidth() != 2:
            raise ValueError(f"{path}: expected 16-bit PCM WAV")
        if handle.getframerate() != FS:
            raise ValueError(f"{path}: expected {FS} Hz; resampling is not performed")
        if handle.getcomptype() != "NONE":
            raise ValueError(f"{path}: expected uncompressed PCM WAV")
        return np.frombuffer(handle.readframes(handle.getnframes()), dtype="<i2").copy()


def write_wav(path: Path, samples: np.ndarray) -> None:
    samples = np.asarray(np.rint(np.clip(samples, -32768, 32767)), dtype="<i2")
    path.parent.mkdir(parents=True, exist_ok=True)
    with wave.open(str(path), "wb") as handle:
        handle.setnchannels(1)
        handle.setsampwidth(2)
        handle.setframerate(FS)
        handle.setcomptype("NONE", "not compressed")
        handle.writeframes(samples.tobytes())


def normalized(signal: np.ndarray) -> np.ndarray:
    peak = float(np.max(np.abs(signal))) if signal.size else 0.0
    if peak == 0.0:
        return signal.astype(np.float64)
    return signal.astype(np.float64) * (TARGET_PEAK / peak)


def make_vectors(output_dir: Path, duration_seconds: float) -> list[Path]:
    count = int(round(FS * duration_seconds))
    time = np.arange(count, dtype=np.float64) / FS
    rng = np.random.default_rng(33030003)
    white = rng.standard_normal(count)
    pink = np.empty(count, dtype=np.float64)
    state = 0.0
    for index, value in enumerate(white):
        state = 0.985 * state + 0.015 * value
        pink[index] = state
    envelope_music = 0.35 + 0.65 * (0.5 + 0.5 * np.sin(2.0 * math.pi * 0.7 * time))
    envelope_speech = 0.20 + 0.80 * (0.5 + 0.5 * np.sin(2.0 * math.pi * 2.4 * time))
    sweep = np.sin(2.0 * math.pi * 20.0 * duration_seconds /
                   math.log(20000.0 / 20.0) *
                   (np.exp(time / duration_seconds * math.log(20000.0 / 20.0)) - 1.0))
    signals = {
        "sine_1khz_minus18dbfs": TARGET_PEAK * np.sin(2.0 * math.pi * 1000.0 * time),
        "multitone_62p5_1k_8k_minus18dbfs": normalized(
            np.sin(2.0 * math.pi * 62.5 * time) +
            np.sin(2.0 * math.pi * 1000.0 * time) +
            np.sin(2.0 * math.pi * 8000.0 * time)
        ),
        "log_sweep_20hz_20khz": normalized(sweep),
        "impulse": np.concatenate(([TARGET_PEAK], np.zeros(count - 1))),
        "white_noise": normalized(white),
        "pink_noise": normalized(pink),
        "music_like": normalized(envelope_music * (
            np.sin(2.0 * math.pi * 110.0 * time) +
            0.7 * np.sin(2.0 * math.pi * 220.0 * time) +
            0.4 * np.sin(2.0 * math.pi * 659.25 * time)
        )),
        "speech_like": normalized(envelope_speech * (
            np.sin(2.0 * math.pi * 125.0 * time) +
            0.55 * np.sin(2.0 * math.pi * 250.0 * time) +
            0.25 * np.sin(2.0 * math.pi * 375.0 * time)
        )),
    }
    paths: list[Path] = []
    output_dir.mkdir(parents=True, exist_ok=True)
    for name, signal in signals.items():
        path = output_dir / f"{name}.wav"
        write_wav(path, signal * 32767.0)
        paths.append(path)
    return paths


def find_gcc() -> Path:
    for candidate in (
        Path(r"C:\msys64\mingw64\bin\gcc.exe"),
        Path(r"C:\msys64\ucrt64\bin\gcc.exe"),
    ):
        if candidate.exists():
            return candidate
    raise RuntimeError("MSYS2 MinGW gcc.exe was not found")


def build_renderer() -> Path:
    temp_dir = Path(tempfile.gettempdir()) / "equalizer_33_renderer"
    temp_dir.mkdir(parents=True, exist_ok=True)
    source_path = temp_dir / "equalizer_33_renderer.c"
    executable_path = temp_dir / "equalizer_33_renderer.exe"
    source_path.write_text(RENDERER_SOURCE, encoding="ascii")
    gcc = find_gcc()
    command = [
        str(gcc), "-std=c99", "-O2", "-DEQ_ALGO_ONLY",
        "-I", str(ROOT / "Code" / "User" / "user_dsp"),
        str(source_path),
        str(ROOT / "Code" / "User" / "user_dsp" / "user_equalizer.c"),
        "-lm", "-o", str(executable_path),
    ]
    environment = os.environ.copy()
    environment["PATH"] = str(gcc.parent) + os.pathsep + environment.get("PATH", "")
    completed = subprocess.run(command, text=True, capture_output=True, env=environment)
    if completed.returncode != 0:
        raise RuntimeError(f"host renderer compile failed:\n{completed.stdout}\n{completed.stderr}")
    return executable_path


def render_with_c(renderer: Path, samples: np.ndarray, core: int, preset: int, work_dir: Path) -> np.ndarray:
    raw_input = work_dir / "input.pcm"
    raw_output = work_dir / "output.pcm"
    np.asarray(samples, dtype="<i2").tofile(raw_input)
    environment = os.environ.copy()
    environment["PATH"] = str(renderer.parent) + os.pathsep + environment.get("PATH", "")
    completed = subprocess.run(
        [str(renderer), str(raw_input), str(raw_output), str(core), str(preset)],
        text=True,
        capture_output=True,
        env=environment,
    )
    if completed.returncode != 0:
        raise RuntimeError(f"host renderer failed ({core}/{preset}): {completed.stdout}\n{completed.stderr}")
    output = np.fromfile(raw_output, dtype="<i2")
    if output.size != samples.size:
        raise RuntimeError(f"renderer output has {output.size} samples, expected {samples.size}")
    return output


def best_alignment(input_float: np.ndarray, output_float: np.ndarray, limit: int = 32) -> tuple[int, np.ndarray, np.ndarray]:
    probe = min(4096, input_float.size, output_float.size)
    best_lag = 0
    best_score = -math.inf
    for lag in range(-limit, limit + 1):
        if lag >= 0:
            left = input_float[:probe - lag]
            right = output_float[lag:probe]
        else:
            left = input_float[-lag:probe]
            right = output_float[:probe + lag]
        if left.size == 0:
            continue
        score = float(np.dot(left, right))
        if score > best_score:
            best_score = score
            best_lag = lag
    if best_lag >= 0:
        return best_lag, input_float[:input_float.size - best_lag], output_float[best_lag:]
    return best_lag, input_float[-best_lag:], output_float[:output_float.size + best_lag]


def tone_thdn(samples: np.ndarray) -> float:
    length = min(5000, samples.size)
    if length < 5000:
        return math.nan
    block = samples[:length] / 32768.0
    spectrum = np.fft.rfft(block)
    powers = (np.abs(spectrum) ** 2) * 2.0 / (length * length)
    fundamental = float(powers[100])
    total = float(np.mean(block * block))
    if fundamental <= 0.0:
        return math.nan
    return math.sqrt(max(0.0, total - fundamental) / fundamental)


def metrics_for(input_samples: np.ndarray, output_samples: np.ndarray, is_tone: bool) -> dict[str, float | int]:
    input_float = input_samples.astype(np.float64) / 32768.0
    output_float = output_samples.astype(np.float64) / 32768.0
    lag, aligned_input, aligned_output = best_alignment(input_float, output_float)
    error = aligned_output - aligned_input
    input_rms = float(np.sqrt(np.mean(input_float * input_float)))
    output_rms = float(np.sqrt(np.mean(output_float * output_float)))
    error_rms = float(np.sqrt(np.mean(error * error)))
    correlation = 1.0
    if np.std(aligned_input) > 0.0 and np.std(aligned_output) > 0.0:
        correlation = float(np.corrcoef(aligned_input, aligned_output)[0, 1])
    spectrum_length = min(4096, aligned_input.size)
    input_spectrum = np.abs(np.fft.rfft(aligned_input[:spectrum_length]))
    output_spectrum = np.abs(np.fft.rfft(aligned_output[:spectrum_length]))
    spectral_error = float(np.mean(np.abs(
        20.0 * np.log10(np.maximum(output_spectrum, 1.0e-12)) -
        20.0 * np.log10(np.maximum(input_spectrum, 1.0e-12))
    )))
    frame_boundaries = output_float[FRAME::FRAME] - output_float[FRAME - 1::FRAME]
    return {
        "alignment_samples": lag,
        "max_abs_error": float(np.max(np.abs(error))),
        "rms_error": error_rms,
        "snr_db": 300.0 if error_rms == 0.0 else 20.0 * math.log10(max(input_rms, 1.0e-20) / error_rms),
        "correlation": correlation,
        "input_peak": float(np.max(np.abs(input_float))),
        "output_peak": float(np.max(np.abs(output_float))),
        "input_rms": input_rms,
        "output_rms": output_rms,
        "dc_offset": float(np.mean(output_float)),
        "clip_count": int(np.count_nonzero((output_samples == 32767) | (output_samples == -32768))),
        "nan_inf": int(not np.isfinite(output_float).all()),
        "crest_factor": float(np.max(np.abs(output_float)) / max(output_rms, 1.0e-20)),
        "max_first_difference": float(np.max(np.abs(np.diff(output_float)))) if output_float.size > 1 else 0.0,
        "frame_boundary_max_delta": float(np.max(np.abs(frame_boundaries))) if frame_boundaries.size else 0.0,
        "spectral_magnitude_error_db": spectral_error,
        "thdn_db": 20.0 * math.log10(tone_thdn(output_samples)) if is_tone else math.nan,
    }


def read_headroom() -> dict[str, tuple[float, float]]:
    path = ROOT / "equalizer_headroom_prediction.csv"
    if not path.exists():
        return {}
    result: dict[str, tuple[float, float]] = {}
    with path.open("r", newline="", encoding="utf-8") as handle:
        for row in csv.DictReader(handle):
            result[row["preset"]] = (float(row["predicted_peak_db"]), float(row["applied_preamp_db"]))
    return result


def draw_plot(path: Path, title: str, series: list[tuple[str, np.ndarray, np.ndarray, tuple[int, int, int]]], log_x: bool = False) -> None:
    width, height = 1100, 620
    left, top, right, bottom = 90, 55, 1030, 520
    image = Image.new("RGB", (width, height), "white")
    draw = ImageDraw.Draw(image)
    draw.rectangle((left, top, right, bottom), outline=(40, 40, 40), width=2)
    draw.text((left, 18), title, fill=(0, 0, 0))
    valid = [(x, y) for _, x, y, _ in series if x.size and y.size]
    if not valid:
        image.save(path)
        return
    xs = np.concatenate([x for x, _ in valid])
    ys = np.concatenate([y[np.isfinite(y)] for _, y in valid])
    if log_x:
        xs = np.log10(np.maximum(xs, 1.0e-12))
    x_min, x_max = float(np.min(xs)), float(np.max(xs))
    y_min, y_max = float(np.min(ys)), float(np.max(ys))
    if y_max - y_min < 1.0e-9:
        y_min -= 1.0
        y_max += 1.0
    margin = 0.08 * (y_max - y_min)
    y_min -= margin
    y_max += margin
    for series_index, (label, x_values, y_values, color) in enumerate(series):
        x_plot = np.log10(np.maximum(x_values, 1.0e-12)) if log_x else x_values
        points = []
        for x_value, y_value in zip(x_plot, y_values):
            if not math.isfinite(float(y_value)):
                continue
            px = left + int((float(x_value) - x_min) / max(x_max - x_min, 1.0e-20) * (right - left))
            py = bottom - int((float(y_value) - y_min) / max(y_max - y_min, 1.0e-20) * (bottom - top))
            points.append((px, py))
        if len(points) > 1:
            draw.line(points, fill=color, width=2)
        legend_y = 545 + series_index * 18
        draw.rectangle((left, legend_y, left + 16, legend_y + 12), fill=color)
        draw.text((left + 24, legend_y), label, fill=(0, 0, 0))
    image.save(path)


def draw_bar_plot(path: Path, title: str, labels: list[str], values: list[float]) -> None:
    width, height = 700, 420
    image = Image.new("RGB", (width, height), "white")
    draw = ImageDraw.Draw(image)
    draw.text((40, 18), title, fill=(0, 0, 0))
    top, bottom, left, right = 70, 350, 70, 650
    maximum = max(max(values), 1.0e-12)
    bar_width = (right - left) // max(len(values), 1)
    for index, (label, value) in enumerate(zip(labels, values)):
        x0 = left + index * bar_width + 15
        x1 = left + (index + 1) * bar_width - 15
        y0 = bottom - int(value / maximum * (bottom - top))
        draw.rectangle((x0, y0, x1, bottom), fill=(45, 122, 190))
        draw.text((x0, bottom + 8), label, fill=(0, 0, 0))
        draw.text((x0, y0 - 15), f"{value:.4g}", fill=(0, 0, 0))
    draw.rectangle((left, top, right, bottom), outline=(40, 40, 40), width=2)
    image.save(path)


def build_plots(output_dir: Path, first_case: dict[str, np.ndarray]) -> None:
    response_path = ROOT / "equalizer_system_response.csv"
    series_by_key: dict[tuple[str, str], list[tuple[float, float]]] = {}
    if response_path.exists():
        with response_path.open("r", newline="", encoding="utf-8") as handle:
            for row in csv.DictReader(handle):
                series_by_key.setdefault((row["core"], row["preset"]), []).append(
                    (float(row["freq_hz"]), float(row["magnitude_db"]))
                )
    def response_series(core: str, preset: str, color: tuple[int, int, int]) -> tuple[str, np.ndarray, np.ndarray, tuple[int, int, int]]:
        values = series_by_key.get((core, preset), [])
        return (f"{core} {preset}", np.array([v[0] for v in values]), np.array([v[1] for v in values]), color)

    draw_plot(output_dir / "equalizer_frequency_response.png", "Legacy and RBJ bass response", [
        response_series("legacy", "bass", (194, 59, 42)),
        response_series("rbj_cascade", "bass", (40, 118, 184)),
    ], log_x=True)
    colors = [(50, 50, 50), (35, 132, 67), (231, 120, 23), (120, 64, 180), (194, 59, 42)]
    draw_plot(output_dir / "equalizer_preset_comparison.png", "RBJ preset comparison", [
        response_series("rbj_cascade", preset, color)
        for preset, color in zip(("flat", "bass", "vocal", "treble", "v_shape"), colors)
    ], log_x=True)
    sample_count = min(2000, first_case["input"].size)
    x_values = np.arange(sample_count)
    draw_plot(output_dir / "equalizer_waveform_comparison.png", "Waveform comparison", [
        ("input", x_values, first_case["input"][:sample_count] / 32768.0, (50, 50, 50)),
        ("legacy_bass", x_values, first_case["legacy_bass"][:sample_count] / 32768.0, (194, 59, 42)),
        ("rbj_bass", x_values, first_case["rbj_bass"][:sample_count] / 32768.0, (40, 118, 184)),
    ])
    fft_count = min(4096, first_case["input"].size)
    frequency = np.fft.rfftfreq(fft_count, 1.0 / FS)
    def spectrum(values: np.ndarray) -> np.ndarray:
        return 20.0 * np.log10(np.maximum(np.abs(np.fft.rfft(values[:fft_count] / 32768.0)), 1.0e-12))
    draw_plot(output_dir / "equalizer_spectrum_comparison.png", "Spectrum comparison", [
        ("input", frequency[1:], spectrum(first_case["input"])[1:], (50, 50, 50)),
        ("legacy_bass", frequency[1:], spectrum(first_case["legacy_bass"])[1:], (194, 59, 42)),
        ("rbj_bass", frequency[1:], spectrum(first_case["rbj_bass"])[1:], (40, 118, 184)),
    ], log_x=True)
    transition_path = ROOT / "equalizer_transition_report.csv"
    values = [0.0, 0.0]
    if transition_path.exists():
        with transition_path.open("r", newline="", encoding="utf-8") as handle:
            row = next(csv.DictReader(handle))
            values = [float(row["baseline_max_sample_delta"]), float(row["switch_max_sample_delta"])]
    draw_bar_plot(output_dir / "equalizer_transition_comparison.png", "Transition delta comparison", ["baseline", "switch"], values)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", action="append", type=Path, help="mono 16-bit 50 kHz WAV input; may be repeated")
    parser.add_argument("--output-dir", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument("--duration", type=float, default=DEFAULT_DURATION_SECONDS)
    parser.add_argument("--generate-vectors", action="store_true", help="also generate deterministic test inputs")
    args = parser.parse_args()
    output_dir = args.output_dir.resolve()
    output_dir.mkdir(parents=True, exist_ok=True)
    input_paths = list(args.input or [])
    if args.generate_vectors or not input_paths:
        input_paths.extend(make_vectors(output_dir / "inputs", args.duration))
    renderer = build_renderer()
    headroom = read_headroom()
    metric_rows: list[dict[str, object]] = []
    first_case: dict[str, np.ndarray] | None = None
    for input_path in input_paths:
        input_samples = require_wav_format(input_path)
        case_dir = output_dir / input_path.stem
        case_dir.mkdir(parents=True, exist_ok=True)
        write_wav(case_dir / "input_copy.wav", input_samples)
        rendered: dict[str, np.ndarray] = {"input": input_samples}
        for mode_name, core, preset, headroom_preset in MODES:
            work_dir = Path(tempfile.mkdtemp(prefix="eq33_wav_"))
            output_samples = render_with_c(renderer, input_samples, core, preset, work_dir)
            rendered[mode_name] = output_samples
            write_wav(case_dir / f"{mode_name}.wav", output_samples)
            metric = metrics_for(input_samples, output_samples, input_path.stem.startswith("sine_1khz"))
            predicted_peak, applied_preamp = headroom.get(headroom_preset, (0.0, 0.0))
            exact_copy = mode_name in {"raw_copy", "float_copy", "legacy_flat", "rbj_flat"}
            passed = metric["clip_count"] == 0 and metric["nan_inf"] == 0
            if exact_copy:
                passed = passed and metric["max_abs_error"] == 0.0 and metric["alignment_samples"] == 0
            metric_rows.append({
                "input": input_path.name,
                "mode": mode_name,
                "sample_count": input_samples.size,
                **metric,
                "predicted_peak_db": predicted_peak,
                "applied_preamp_db": applied_preamp,
                "pass": "PASS" if passed else "FAIL",
            })
        if first_case is None:
            first_case = rendered
    fieldnames = [
        "input", "mode", "sample_count", "alignment_samples", "max_abs_error", "rms_error", "snr_db",
        "correlation", "input_peak", "output_peak", "input_rms", "output_rms", "dc_offset",
        "clip_count", "nan_inf", "crest_factor", "max_first_difference", "frame_boundary_max_delta",
        "spectral_magnitude_error_db", "thdn_db", "predicted_peak_db", "applied_preamp_db", "pass",
    ]
    with (output_dir / "equalizer_wav_metrics.csv").open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(metric_rows)
    failures = sum(row["pass"] != "PASS" for row in metric_rows)
    with (output_dir / "equalizer_wav_summary.md").open("w", encoding="utf-8") as handle:
        handle.write("# Project 3.3 Offline WAV A/B Summary\n\n")
        handle.write(f"C host renderer: `{renderer}`\n\n")
        handle.write(f"Inputs: {len(input_paths)}; rendered rows: {len(metric_rows)}; failures: {failures}.\n\n")
        handle.write("| Input | Mode | Peak | Clip | SNR dB | Pass |\n|---|---|---:|---:|---:|---|\n")
        for row in metric_rows:
            handle.write(
                f"| {row['input']} | {row['mode']} | {row['output_peak']:.6f} | "
                f"{row['clip_count']} | {row['snr_db']:.3f} | {row['pass']} |\n"
            )
        handle.write("\nTHD+N is reported only for the coherent 1 kHz input; other signals have no single fundamental.\n")
    if first_case is not None:
        build_plots(output_dir, first_case)
    print(f"equalizer_33_process_wav rows={len(metric_rows)} failures={failures} output={output_dir}")
    return 0 if failures == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
