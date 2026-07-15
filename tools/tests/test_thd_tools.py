from __future__ import annotations

import math
import sys
import tempfile
import unittest
import wave
from pathlib import Path

import numpy as np


THD_DIR = Path(__file__).resolve().parents[1] / "thd"
sys.path.insert(0, str(THD_DIR))

from analyze_thd import (  # noqa: E402
    analyze_wav,
    fit_metrics,
    read_pcm16_mono,
    refine_fundamental,
    select_segment,
    validate_finite,
)
from generate_thd_tones import SAMPLE_RATE, generate_suite  # noqa: E402


def quantized_signal(components: list[tuple[float, float]], seconds: float = 2.0) -> np.ndarray:
    count = int(round(SAMPLE_RATE * seconds))
    time = np.arange(count, dtype=np.float64) / SAMPLE_RATE
    signal = np.zeros(count, dtype=np.float64)
    for frequency_hz, peak in components:
        signal += peak * np.sin(2.0 * math.pi * frequency_hz * time)
    pcm = np.clip(np.rint(signal * 32767.0), -32768, 32767).astype("<i2")
    return pcm.astype(np.float64) / 32768.0


def write_wav(path: Path, sample_rate: int, samples: np.ndarray) -> None:
    pcm = np.clip(np.rint(samples * 32767.0), -32768, 32767).astype("<i2")
    with wave.open(str(path), "wb") as handle:
        handle.setnchannels(1)
        handle.setsampwidth(2)
        handle.setframerate(sample_rate)
        handle.writeframes(pcm.tobytes())


class ThdAnalyzerTests(unittest.TestCase):
    def metrics(self, samples: np.ndarray, expected_f0: float) -> dict:
        segment, _, _ = select_segment(samples, SAMPLE_RATE, 0.1, 1.5)
        actual_f0 = refine_fundamental(segment, SAMPLE_RATE, expected_f0)
        return fit_metrics(segment, SAMPLE_RATE, actual_f0, 5)

    def test_quantized_ideal_sine_has_low_thd(self) -> None:
        result = self.metrics(quantized_signal([(1000.0, 0.25)]), 1000.0)
        self.assertLess(result["thd_percent"], 0.002)

    def test_one_percent_second_harmonic_is_recovered(self) -> None:
        result = self.metrics(
            quantized_signal([(1000.0, 0.25), (2000.0, 0.0025)]), 1000.0
        )
        self.assertAlmostEqual(result["thd_percent"], 1.0, delta=0.02)
        self.assertAlmostEqual(
            result["harmonics"][1]["rms"] / result["harmonics"][0]["rms"],
            0.01,
            delta=0.0002,
        )

    def test_known_h2_h3_rows_are_recovered(self) -> None:
        result = self.metrics(
            quantized_signal(
                [(1000.0, 0.25), (2000.0, 0.00125), (3000.0, 0.000625)]
            ),
            1000.0,
        )
        fundamental = result["harmonics"][0]["rms"]
        self.assertAlmostEqual(result["harmonics"][1]["rms"] / fundamental, 0.005, delta=0.0002)
        self.assertAlmostEqual(result["harmonics"][2]["rms"] / fundamental, 0.0025, delta=0.0002)

    def test_8khz_is_limited_to_h3(self) -> None:
        result = self.metrics(quantized_signal([(8000.0, 0.25)]), 8000.0)
        self.assertEqual(result["max_harmonic_used"], 3)
        self.assertEqual([item["order"] for item in result["harmonics"]], [1, 2, 3])

    def test_wrong_sample_rate_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "wrong_rate.wav"
            write_wav(path, 48_000, np.zeros(48_000, dtype=np.float64))
            with self.assertRaisesRegex(ValueError, "expected 50000 Hz"):
                read_pcm16_mono(path)

    def test_short_segment_is_rejected(self) -> None:
        with self.assertRaisesRegex(ValueError, "too short"):
            select_segment(np.zeros(1000), SAMPLE_RATE, 0.0, 0.01)

    def test_nan_and_inf_are_rejected(self) -> None:
        for value in (math.nan, math.inf, -math.inf):
            with self.subTest(value=value):
                samples = np.zeros(20_000, dtype=np.float64)
                samples[100] = value
                with self.assertRaisesRegex(ValueError, "NaN or Inf"):
                    validate_finite(samples)

    def test_generated_suite_format_and_analysis_interval(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)
            rows = generate_suite(output)
            self.assertEqual(len(rows), 5)
            path = output / "thd_1000Hz_m12dBFS_50k_mono16_10s.wav"
            sample_rate, pcm, _ = read_pcm16_mono(path)
            self.assertEqual(sample_rate, SAMPLE_RATE)
            self.assertEqual(len(pcm), 500_000)
            result, _ = analyze_wav(path, 1000.0)
            self.assertEqual(result["analysis_start_s"], 1.0)
            self.assertEqual(result["analysis_end_s"], 9.0)


if __name__ == "__main__":
    unittest.main()
