#!/usr/bin/env python3
"""Analyze Project 3.3 CH1 digital captures without altering sample values."""

from __future__ import annotations

import argparse
import cmath
import csv
import json
import math
import struct
import wave
from pathlib import Path


SAMPLE_RATE = 50000
PRE_FRAMES = 4


def read_i16le(path: Path) -> list[int]:
    data = path.read_bytes()
    if len(data) % 2:
        raise ValueError(f"{path} has an odd byte count")
    return list(struct.unpack("<" + "h" * (len(data) // 2), data))


def pack_i16le(samples: list[int]) -> bytes:
    return struct.pack("<" + "h" * len(samples), *samples)


def rotate_trigger(samples: list[int], frame_len: int, prewrite: int) -> list[int]:
    pre_samples = PRE_FRAMES * frame_len
    if frame_len <= 0 or len(samples) < pre_samples:
        raise ValueError("trigger capture does not contain four complete pre-frames")
    if not 0 <= prewrite < PRE_FRAMES:
        raise ValueError("pre-write index must be in [0, 3]")
    frames = [samples[index * frame_len:(index + 1) * frame_len] for index in range(PRE_FRAMES)]
    return sum(frames[prewrite:] + frames[:prewrite], []) + samples[pre_samples:]


def write_wav(path: Path, samples: list[int]) -> None:
    with wave.open(str(path), "wb") as wav_file:
        wav_file.setnchannels(1)
        wav_file.setsampwidth(2)
        wav_file.setframerate(SAMPLE_RATE)
        wav_file.writeframes(pack_i16le(samples))


def peak(samples: list[int]) -> int:
    return max((abs(value) for value in samples), default=0)


def correlation(first: list[int], second: list[int]) -> float | None:
    count = min(len(first), len(second))
    if count == 0:
        return None
    x = first[:count]
    y = second[:count]
    x_mean = sum(x) / count
    y_mean = sum(y) / count
    numerator = sum((a - x_mean) * (b - y_mean) for a, b in zip(x, y))
    x_energy = sum((a - x_mean) ** 2 for a in x)
    y_energy = sum((b - y_mean) ** 2 for b in y)
    denominator = math.sqrt(x_energy * y_energy)
    if denominator == 0:
        return 1.0 if x == y else None
    return numerator / denominator


def snr_db(reference: list[int], observed: list[int]) -> float | str | None:
    count = min(len(reference), len(observed))
    if count == 0:
        return None
    signal = sum(value * value for value in reference[:count])
    noise = sum((a - b) ** 2 for a, b in zip(reference[:count], observed[:count]))
    if noise == 0:
        return "INF"
    if signal == 0:
        return None
    return 10.0 * math.log10(signal / noise)


def boundary_delta(samples: list[int], frame_len: int) -> int:
    if frame_len <= 0:
        return 0
    boundaries = range(frame_len, len(samples), frame_len)
    return max((abs(samples[index] - samples[index - 1]) for index in boundaries), default=0)


def spectrum(samples: list[int], bins: int = 256) -> list[float]:
    count = min(len(samples), max(2, bins * 2))
    if count == 0:
        return []
    result = []
    for k in range(count // 2 + 1):
        value = sum(samples[n] * cmath.exp(-2j * math.pi * k * n / count) for n in range(count))
        result.append(abs(value) / count)
    return result


def analyze(arguments: argparse.Namespace) -> int:
    input_samples = read_i16le(arguments.input)
    output_samples = read_i16le(arguments.output)
    expected_frames = 8 if arguments.capture_kind == "manual" else 12
    expected_samples = expected_frames * arguments.frame_len
    if arguments.frame_len <= 0:
        raise ValueError("frame length must be positive")
    if len(input_samples) != len(output_samples):
        raise ValueError("input and output capture lengths differ")
    if len(input_samples) != expected_samples:
        raise ValueError(
            f"{arguments.capture_kind} capture requires exactly "
            f"{expected_frames} frames ({expected_samples} samples)"
        )
    if arguments.capture_kind == "trigger":
        input_samples = rotate_trigger(input_samples, arguments.frame_len, arguments.prewrite_index)
        output_samples = rotate_trigger(output_samples, arguments.frame_len, arguments.prewrite_index)

    arguments.out_dir.mkdir(parents=True, exist_ok=True)
    input_raw = arguments.out_dir / "input_chronological.raw"
    output_raw = arguments.out_dir / "output_chronological.raw"
    input_raw.write_bytes(pack_i16le(input_samples))
    output_raw.write_bytes(pack_i16le(output_samples))
    write_wav(arguments.out_dir / "input.wav", input_samples)
    write_wav(arguments.out_dir / "output.wav", output_samples)

    count = min(len(input_samples), len(output_samples))
    mismatch = sum(a != b for a, b in zip(input_samples[:count], output_samples[:count]))
    mismatch += abs(len(input_samples) - len(output_samples))
    errors = [abs(a - b) for a, b in zip(input_samples, output_samples)]
    max_error = max(errors, default=0)
    max_error_index = errors.index(max_error) if errors else None
    input_boundary_delta = boundary_delta(input_samples, arguments.frame_len)
    output_boundary_delta = boundary_delta(output_samples, arguments.frame_len)
    report = {
        "capture_kind": arguments.capture_kind,
        "mode": arguments.mode,
        "sample_rate_hz": SAMPLE_RATE,
        "sample_format": "signed_i16_little_endian_mono",
        "input_samples": len(input_samples),
        "output_samples": len(output_samples),
        "raw_copy_mismatch_count": mismatch if arguments.mode == "raw" else None,
        "raw_copy_pass": (mismatch == 0) if arguments.mode == "raw" else None,
        "input_peak": peak(input_samples),
        "output_peak": peak(output_samples),
        "input_clipping_count": sum(value in (-32768, 32767) for value in input_samples),
        "output_clipping_count": sum(value in (-32768, 32767) for value in output_samples),
        "correlation": correlation(input_samples, output_samples),
        "snr_db": snr_db(input_samples, output_samples),
        "input_frame_boundary_delta_max": input_boundary_delta,
        "output_frame_boundary_delta_max": output_boundary_delta,
        "frame_boundary_delta_max": max(input_boundary_delta, output_boundary_delta),
        "raw_max_abs_error": max_error if arguments.mode == "raw" else None,
        "raw_max_abs_error_index": max_error_index if arguments.mode == "raw" else None,
    }
    (arguments.out_dir / "capture_report.json").write_text(
        json.dumps(report, indent=2, ensure_ascii=True) + "\n", encoding="utf-8"
    )

    input_spectrum = spectrum(input_samples)
    output_spectrum = spectrum(output_samples)
    spectrum_count = min(len(input_spectrum), len(output_spectrum))
    fft_size = max(1, (len(input_spectrum) - 1) * 2)
    with (arguments.out_dir / "spectrum.csv").open("w", newline="", encoding="utf-8") as csv_file:
        writer = csv.writer(csv_file)
        writer.writerow(["frequency_hz", "input_magnitude", "output_magnitude"])
        for index in range(spectrum_count):
            writer.writerow([index * SAMPLE_RATE / fft_size, input_spectrum[index], output_spectrum[index]])
    print(json.dumps(report, ensure_ascii=True))
    if arguments.mode == "raw" and (
        mismatch != 0 or report["output_clipping_count"] != 0
    ):
        return 2
    if arguments.mode == "processed" and (
        report["input_peak"] == 0 or report["output_clipping_count"] != 0
    ):
        return 3
    return 0


def parser() -> argparse.ArgumentParser:
    result = argparse.ArgumentParser()
    subparsers = result.add_subparsers(dest="capture_kind", required=True)
    for name in ("manual", "trigger"):
        command = subparsers.add_parser(name)
        command.add_argument("--input", type=Path, required=True)
        command.add_argument("--output", type=Path, required=True)
        command.add_argument("--out-dir", type=Path, required=True)
        command.add_argument("--mode", choices=("raw", "processed"), required=True)
        command.add_argument("--frame-len", type=int, default=1024)
        if name == "trigger":
            command.add_argument("--prewrite-index", type=int, required=True)
    return result


if __name__ == "__main__":
    raise SystemExit(analyze(parser().parse_args()))
