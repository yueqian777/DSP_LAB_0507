#!/usr/bin/env python3
"""Generate deterministic mono PCM16 tones for the WOLA THD suite."""

from __future__ import annotations

import argparse
import csv
import math
import wave
from pathlib import Path

import numpy as np


SAMPLE_RATE = 50_000
DURATION_S = 10.0
AMPLITUDE_DBFS = -12.0
FADE_S = 0.05
FREQUENCIES_HZ = (500, 1000, 3000, 4000, 8000)


def tone_filename(frequency_hz: int) -> str:
    return f"thd_{frequency_hz}Hz_m12dBFS_50k_mono16_10s.wav"


def generate_tone(frequency_hz: float) -> np.ndarray:
    sample_count = int(round(SAMPLE_RATE * DURATION_S))
    index = np.arange(sample_count, dtype=np.float64)
    amplitude = 10.0 ** (AMPLITUDE_DBFS / 20.0)
    signal = amplitude * np.sin(2.0 * math.pi * frequency_hz * index / SAMPLE_RATE)

    fade_count = int(round(SAMPLE_RATE * FADE_S))
    ramp = 0.5 - 0.5 * np.cos(
        math.pi * np.arange(fade_count, dtype=np.float64) / fade_count
    )
    signal[:fade_count] *= ramp
    signal[-fade_count:] *= ramp[::-1]
    return np.clip(np.rint(signal * 32767.0), -32768, 32767).astype("<i2")


def write_pcm16_mono(path: Path, samples: np.ndarray) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with wave.open(str(path), "wb") as handle:
        handle.setnchannels(1)
        handle.setsampwidth(2)
        handle.setframerate(SAMPLE_RATE)
        handle.writeframes(np.asarray(samples, dtype="<i2").tobytes())


def generate_suite(output_dir: Path) -> list[dict[str, object]]:
    output_dir.mkdir(parents=True, exist_ok=True)
    rows: list[dict[str, object]] = []
    for frequency_hz in FREQUENCIES_HZ:
        filename = tone_filename(frequency_hz)
        samples = generate_tone(frequency_hz)
        write_pcm16_mono(output_dir / filename, samples)
        rows.append(
            {
                "filename": filename,
                "sample_rate_hz": SAMPLE_RATE,
                "channels": 1,
                "pcm_bits": 16,
                "sample_count": len(samples),
                "duration_s": DURATION_S,
                "amplitude_dbfs_peak": AMPLITUDE_DBFS,
                "fade_in_s": FADE_S,
                "fade_out_s": FADE_S,
                "fundamental_hz": frequency_hz,
                "noise": "none",
                "generation": "deterministic",
            }
        )

    with (output_dir / "manifest.csv").open("w", newline="", encoding="utf-8-sig") as handle:
        writer = csv.DictWriter(handle, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)
    return rows


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out-dir", type=Path, default=Path("results/thd/tones"))
    arguments = parser.parse_args()
    rows = generate_suite(arguments.out_dir)
    print(f"generated={len(rows)} output_dir={arguments.out_dir.resolve()}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
