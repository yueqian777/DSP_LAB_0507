#!/usr/bin/env python3
"""Measure pure THD, THD+N, and SINAD from mono PCM16 WAV files."""

from __future__ import annotations

import argparse
import csv
import json
import math
import re
import wave
from pathlib import Path
from typing import Any

import numpy as np
from PIL import Image, ImageDraw


EXPECTED_SAMPLE_RATE = 50_000
EPSILON = 1.0e-20


def read_pcm16_mono(path: Path, expected_sample_rate: int = EXPECTED_SAMPLE_RATE) -> tuple[int, np.ndarray, np.ndarray]:
    try:
        with wave.open(str(path), "rb") as handle:
            channels = handle.getnchannels()
            sample_width = handle.getsampwidth()
            sample_rate = handle.getframerate()
            compression = handle.getcomptype()
            sample_count = handle.getnframes()
            frames = handle.readframes(sample_count)
    except (wave.Error, EOFError) as error:
        raise ValueError(f"{path}: invalid PCM WAV: {error}") from error

    if channels != 1:
        raise ValueError(f"{path}: expected mono WAV, got {channels} channels")
    if sample_width != 2 or compression != "NONE":
        raise ValueError(f"{path}: expected uncompressed PCM16 WAV")
    if sample_rate != expected_sample_rate:
        raise ValueError(
            f"{path}: expected {expected_sample_rate} Hz, got {sample_rate} Hz"
        )
    if len(frames) != sample_count * 2:
        raise ValueError(f"{path}: truncated PCM data")

    pcm = np.frombuffer(frames, dtype="<i2").copy()
    normalized = pcm.astype(np.float64) / 32768.0
    validate_finite(normalized)
    return sample_rate, pcm, normalized


def write_pcm16_mono(path: Path, sample_rate: int, samples: np.ndarray) -> None:
    pcm = np.asarray(samples, dtype="<i2")
    path.parent.mkdir(parents=True, exist_ok=True)
    with wave.open(str(path), "wb") as handle:
        handle.setnchannels(1)
        handle.setsampwidth(2)
        handle.setframerate(sample_rate)
        handle.writeframes(pcm.tobytes())


def validate_finite(samples: np.ndarray) -> None:
    if not np.all(np.isfinite(samples)):
        raise ValueError("samples contain NaN or Inf")


def infer_fundamental(path: Path) -> float | None:
    match = re.search(r"(\d+(?:\.\d+)?)\s*Hz", path.name, re.IGNORECASE)
    return float(match.group(1)) if match else None


def select_segment(
    samples: np.ndarray,
    sample_rate: int,
    start_s: float,
    duration_s: float,
) -> tuple[np.ndarray, int, int]:
    validate_finite(samples)
    if start_s < 0.0 or duration_s <= 0.0:
        raise ValueError("analysis start must be nonnegative and duration must be positive")
    start = int(round(start_s * sample_rate))
    stop = min(len(samples), start + int(round(duration_s * sample_rate)))
    minimum = max(4096, int(round(0.2 * sample_rate)))
    if start >= len(samples) or stop - start < minimum:
        raise ValueError("analysis segment is too short")
    return samples[start:stop].copy(), start, stop


def _parabolic_frequency(
    samples: np.ndarray, sample_rate: int, expected_f0: float, search_fraction: float
) -> float:
    fft_size = 1 << int(math.floor(math.log2(min(len(samples), 524_288))))
    data = samples[:fft_size] - float(np.mean(samples[:fft_size]))
    window = np.blackman(fft_size)
    magnitude = np.abs(np.fft.rfft(data * window))
    frequencies = np.fft.rfftfreq(fft_size, 1.0 / sample_rate)
    low = max(1.0, expected_f0 * (1.0 - search_fraction))
    high = min(sample_rate / 2.0 - 1.0, expected_f0 * (1.0 + search_fraction))
    indices = np.flatnonzero((frequencies >= low) & (frequencies <= high))
    if indices.size < 3:
        return float(expected_f0)
    bin_index = int(indices[np.argmax(magnitude[indices])])
    delta = 0.0
    if 1 <= bin_index < len(magnitude) - 1:
        left = math.log(max(float(magnitude[bin_index - 1]), EPSILON))
        center = math.log(max(float(magnitude[bin_index]), EPSILON))
        right = math.log(max(float(magnitude[bin_index + 1]), EPSILON))
        denominator = left - 2.0 * center + right
        if abs(denominator) > EPSILON:
            delta = 0.5 * (left - right) / denominator
            delta = float(np.clip(delta, -0.5, 0.5))
    return (bin_index + delta) * sample_rate / fft_size


def refine_fundamental(
    samples: np.ndarray,
    sample_rate: int,
    expected_f0: float,
    search_fraction: float = 0.03,
) -> float:
    if not 0.0 < expected_f0 < sample_rate / 2.0:
        raise ValueError("expected fundamental is outside (0, Nyquist)")
    coarse = _parabolic_frequency(samples, sample_rate, expected_f0, search_fraction)
    block = min(len(samples) // 4, max(int(round(0.25 * sample_rate)), 4096))
    hop = max(block // 2, 1)
    if block < 1024 or len(samples) < 3 * block:
        return coarse

    local_time = np.arange(block, dtype=np.float64) / sample_rate
    oscillator = np.exp(-2j * math.pi * coarse * local_time)
    window = np.hanning(block)
    centers: list[float] = []
    phases: list[float] = []
    for start in range(0, len(samples) - block + 1, hop):
        section = samples[start : start + block]
        section = section - float(np.mean(section))
        projection = np.sum(section * window * oscillator)
        projection *= np.exp(-2j * math.pi * coarse * start / sample_rate)
        if abs(projection) > 1.0e-12:
            centers.append((start + 0.5 * (block - 1)) / sample_rate)
            phases.append(float(np.angle(projection)))
    if len(phases) < 4:
        return coarse

    slope, _ = np.polyfit(
        np.asarray(centers, dtype=np.float64),
        np.unwrap(np.asarray(phases, dtype=np.float64)),
        1,
    )
    refined = coarse + float(slope) / (2.0 * math.pi)
    low = expected_f0 * (1.0 - search_fraction)
    high = expected_f0 * (1.0 + search_fraction)
    if not math.isfinite(refined) or not low <= refined <= high:
        return coarse
    return refined


def _normal_equations(
    samples: np.ndarray,
    sample_rate: int,
    fundamental_hz: float,
    max_harmonic: int,
    chunk_size: int = 65_536,
) -> tuple[np.ndarray, np.ndarray]:
    columns = 1 + 2 * max_harmonic
    gram = np.zeros((columns, columns), dtype=np.float64)
    right = np.zeros(columns, dtype=np.float64)
    for start in range(0, len(samples), chunk_size):
        stop = min(len(samples), start + chunk_size)
        time = np.arange(start, stop, dtype=np.float64) / sample_rate
        matrix = np.empty((stop - start, columns), dtype=np.float64)
        matrix[:, 0] = 1.0
        column = 1
        for order in range(1, max_harmonic + 1):
            phase = 2.0 * math.pi * order * fundamental_hz * time
            matrix[:, column] = np.cos(phase)
            matrix[:, column + 1] = np.sin(phase)
            column += 2
        gram += matrix.T @ matrix
        right += matrix.T @ samples[start:stop]
    return gram, right


def fit_metrics(
    segment: np.ndarray,
    sample_rate: int,
    fundamental_hz: float,
    requested_harmonics: int = 5,
) -> dict[str, Any]:
    validate_finite(segment)
    if requested_harmonics < 2:
        raise ValueError("at least the second harmonic must be requested")
    max_harmonic = min(
        requested_harmonics,
        int((sample_rate / 2.0 - 1.0) // fundamental_hz),
    )
    if max_harmonic < 2:
        raise ValueError("fundamental is too high to measure H2")

    gram, right = _normal_equations(
        segment, sample_rate, fundamental_hz, max_harmonic
    )
    coefficients = np.linalg.lstsq(gram, right, rcond=None)[0]
    harmonics: list[dict[str, float | int]] = []
    for order in range(1, max_harmonic + 1):
        cosine = float(coefficients[1 + 2 * (order - 1)])
        sine = float(coefficients[2 + 2 * (order - 1)])
        peak = math.hypot(cosine, sine)
        rms = peak / math.sqrt(2.0)
        harmonics.append(
            {
                "order": order,
                "frequency_hz": order * fundamental_hz,
                "peak": peak,
                "rms": rms,
                "dbfs_rms": 20.0 * math.log10(max(rms, EPSILON)),
            }
        )

    fundamental_rms = float(harmonics[0]["rms"])
    harmonic_rms = math.sqrt(
        sum(float(item["rms"]) ** 2 for item in harmonics[1:])
    )
    thd_ratio = harmonic_rms / max(fundamental_rms, EPSILON)

    time = np.arange(len(segment), dtype=np.float64) / sample_rate
    fundamental_model = (
        float(coefficients[0])
        + float(coefficients[1]) * np.cos(2.0 * math.pi * fundamental_hz * time)
        + float(coefficients[2]) * np.sin(2.0 * math.pi * fundamental_hz * time)
    )
    residual_rms = float(np.sqrt(np.mean((segment - fundamental_model) ** 2)))
    thdn_ratio = residual_rms / max(fundamental_rms, EPSILON)

    return {
        "dc": float(coefficients[0]),
        "fundamental_est_hz": fundamental_hz,
        "max_harmonic_used": max_harmonic,
        "harmonics": harmonics,
        "fundamental_rms": fundamental_rms,
        "harmonic_rms": harmonic_rms,
        "residual_rms": residual_rms,
        "thd_ratio": thd_ratio,
        "thd_percent": 100.0 * thd_ratio,
        "thd_db": 20.0 * math.log10(max(thd_ratio, EPSILON)),
        "thdn_ratio": thdn_ratio,
        "thdn_percent": 100.0 * thdn_ratio,
        "thdn_db": 20.0 * math.log10(max(thdn_ratio, EPSILON)),
        "sinad_db": -20.0 * math.log10(max(thdn_ratio, EPSILON)),
    }


def analyze_wav(
    path: Path,
    expected_f0: float,
    start_s: float = 1.0,
    duration_s: float = 8.0,
    requested_harmonics: int = 5,
) -> tuple[dict[str, Any], np.ndarray]:
    sample_rate, pcm, normalized = read_pcm16_mono(path)
    segment, start, stop = select_segment(normalized, sample_rate, start_s, duration_s)
    fundamental_hz = refine_fundamental(segment, sample_rate, expected_f0)
    result = fit_metrics(segment, sample_rate, fundamental_hz, requested_harmonics)
    peak = float(np.max(np.abs(pcm.astype(np.int32))))
    result.update(
        {
            "file": str(path.resolve()),
            "sample_rate_hz": sample_rate,
            "samples_total": len(pcm),
            "expected_f0_hz": expected_f0,
            "analysis_start_s": start / sample_rate,
            "analysis_end_s": stop / sample_rate,
            "analysis_samples": len(segment),
            "peak_dbfs": 20.0 * math.log10(max(peak / 32768.0, EPSILON)),
            "clip_count": int(np.count_nonzero(np.abs(pcm.astype(np.int32)) >= 32767)),
            "format": "mono PCM16",
        }
    )
    return result, segment


def write_spectrum_png(
    segment: np.ndarray,
    sample_rate: int,
    result: dict[str, Any],
    output_path: Path,
) -> None:
    fft_size = 1 << int(math.floor(math.log2(min(len(segment), 524_288))))
    window = np.blackman(fft_size)
    spectrum = np.fft.rfft((segment[:fft_size] - np.mean(segment[:fft_size])) * window)
    peak_amplitude = 2.0 * np.abs(spectrum) / max(float(np.sum(window)), EPSILON)
    spectrum_db = 20.0 * np.log10(np.maximum(peak_amplitude, 1.0e-8))

    width, height = 1400, 700
    left, top, right, bottom = 90, 55, 35, 70
    plot_width = width - left - right
    plot_height = height - top - bottom
    image = Image.new("RGB", (width, height), "white")
    draw = ImageDraw.Draw(image)
    draw.rectangle(
        (left, top, left + plot_width, top + plot_height), outline=(30, 30, 30)
    )
    for level_db in range(-160, 1, 20):
        y = top + int(round((-level_db / 160.0) * plot_height))
        draw.line((left, y, left + plot_width, y), fill=(225, 225, 225))
        draw.text((8, y - 7), f"{level_db} dB", fill=(30, 30, 30))
    for frequency_hz in range(0, sample_rate // 2 + 1, 5000):
        x = left + int(round(frequency_hz / (sample_rate / 2.0) * plot_width))
        draw.line((x, top, x, top + plot_height), fill=(235, 235, 235))
        draw.text((x - 18, top + plot_height + 10), f"{frequency_hz // 1000}k", fill=(30, 30, 30))

    points: list[tuple[int, int]] = []
    bins_per_pixel = max(1, int(math.ceil(len(spectrum_db) / plot_width)))
    for pixel in range(plot_width):
        start = pixel * bins_per_pixel
        stop = min(len(spectrum_db), start + bins_per_pixel)
        if start >= stop:
            break
        value_db = float(np.max(spectrum_db[start:stop]))
        value_db = min(0.0, max(-160.0, value_db))
        y = top + int(round((-value_db / 160.0) * plot_height))
        points.append((left + pixel, y))
    if len(points) > 1:
        draw.line(points, fill=(20, 85, 180), width=2)

    for harmonic in result["harmonics"]:
        frequency_hz = float(harmonic["frequency_hz"])
        x = left + int(round(frequency_hz / (sample_rate / 2.0) * plot_width))
        draw.line((x, top, x, top + plot_height), fill=(200, 70, 45), width=1)
        draw.text((x + 2, top + 4), f"H{harmonic['order']}", fill=(150, 35, 20))

    title = (
        f"{Path(result['file']).name}  THD={result['thd_percent']:.7g}%  "
        f"THD+N={result['thdn_percent']:.7g}%"
    )
    draw.text((left, 15), title, fill=(20, 20, 20))
    draw.text((left + plot_width // 2 - 55, height - 30), "Frequency (Hz)", fill=(20, 20, 20))
    output_path.parent.mkdir(parents=True, exist_ok=True)
    image.save(output_path, format="PNG")


def write_artifacts(
    result: dict[str, Any],
    segment: np.ndarray,
    output_dir: Path,
    prefix: str,
) -> None:
    output_dir.mkdir(parents=True, exist_ok=True)
    with (output_dir / f"{prefix}_metrics.json").open("w", encoding="utf-8") as handle:
        json.dump(result, handle, ensure_ascii=False, indent=2)
    with (output_dir / f"{prefix}_harmonics.csv").open(
        "w", newline="", encoding="utf-8-sig"
    ) as handle:
        writer = csv.DictWriter(
            handle, fieldnames=["order", "frequency_hz", "peak", "rms", "dbfs_rms"]
        )
        writer.writeheader()
        writer.writerows(result["harmonics"])
    write_spectrum_png(
        segment,
        int(result["sample_rate_hz"]),
        result,
        output_dir / f"{prefix}_spectrum.png",
    )


def _summary_row(result: dict[str, Any]) -> dict[str, Any]:
    return {
        "file": result["file"],
        "sample_rate_hz": result["sample_rate_hz"],
        "expected_f0_hz": result["expected_f0_hz"],
        "fundamental_est_hz": result["fundamental_est_hz"],
        "max_harmonic_used": result["max_harmonic_used"],
        "fundamental_rms": result["fundamental_rms"],
        "thd_ratio": result["thd_ratio"],
        "thd_percent": result["thd_percent"],
        "thd_db": result["thd_db"],
        "thdn_ratio": result["thdn_ratio"],
        "thdn_percent": result["thdn_percent"],
        "thdn_db": result["thdn_db"],
        "sinad_db": result["sinad_db"],
        "peak_dbfs": result["peak_dbfs"],
        "clip_count": result["clip_count"],
        "analysis_start_s": result["analysis_start_s"],
        "analysis_end_s": result["analysis_end_s"],
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("wav", nargs="+", type=Path)
    parser.add_argument("--f0", type=float)
    parser.add_argument("--start", type=float, default=1.0)
    parser.add_argument("--duration", type=float, default=8.0)
    parser.add_argument("--harmonics", type=int, default=5)
    parser.add_argument("--out-dir", type=Path, default=Path("results/thd/analysis"))
    arguments = parser.parse_args()

    rows: list[dict[str, Any]] = []
    for path in arguments.wav:
        expected_f0 = arguments.f0 if arguments.f0 is not None else infer_fundamental(path)
        if expected_f0 is None:
            raise ValueError(f"{path}: cannot infer f0; pass --f0")
        result, segment = analyze_wav(
            path,
            expected_f0,
            arguments.start,
            arguments.duration,
            arguments.harmonics,
        )
        write_artifacts(result, segment, arguments.out_dir, path.stem)
        rows.append(_summary_row(result))
        print(
            f"{path.name}: f0={result['fundamental_est_hz']:.7f} Hz "
            f"THD={result['thd_percent']:.9g}% ({result['thd_db']:.3f} dB) "
            f"THD+N={result['thdn_percent']:.9g}% "
            f"SINAD={result['sinad_db']:.3f} dB clips={result['clip_count']}"
        )

    with (arguments.out_dir / "thd_summary.csv").open(
        "w", newline="", encoding="utf-8-sig"
    ) as handle:
        writer = csv.DictWriter(handle, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
