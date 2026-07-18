from __future__ import annotations

import csv
import hashlib
import json
import math
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
import wave


ROOT = Path(__file__).resolve().parents[2]
RUNNER = ROOT / "tools/equalizer_33_editor_offline.py"
HARNESS = ROOT / "tools/tests/equalizer_editor_pcm16_test.c"


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


class EqualizerEditorPcm16Test(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.temporary = tempfile.TemporaryDirectory(
            prefix="equalizer_editor_pcm16_unittest_"
        )
        cls.output = Path(cls.temporary.name) / "artifacts"
        cls.result = subprocess.run(
            [
                sys.executable,
                "-B",
                str(RUNNER),
                "--output-dir",
                str(cls.output),
            ],
            cwd=ROOT,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            check=False,
        )
        if cls.result.returncode != 0:
            raise RuntimeError(cls.result.stdout)
        cls.summary = json.loads(
            (cls.output / "equalizer_editor_pcm16_summary.json").read_text(
                encoding="utf-8"
            )
        )
        cls.manifest = json.loads(
            (cls.output / "manifest.json").read_text(encoding="utf-8")
        )

    @classmethod
    def tearDownClass(cls) -> None:
        cls.temporary.cleanup()

    def test_runner_and_summary_contract(self) -> None:
        self.assertIn("equalizer_33_editor_offline PASS", self.result.stdout)
        expected = {
            "sample_rate_hz": 50_000,
            "channels": 1,
            "sample_format": "PCM_S16LE",
            "deterministic": True,
            "pattern_count": 6,
            "signal_count": 16,
            "static_case_count": 96,
            "response_point_count": 60,
            "transition_case_count": 4,
            "total_clip_count": 0,
            "total_nonfinite_count": 0,
            "identity_mismatch_count": 0,
            "unexpected_repeated_frames": 0,
            "transition_repeated_frames": 0,
            "transition_sequence_early_updates": 0,
            "failures": 0,
        }
        for key, value in expected.items():
            self.assertEqual(self.summary[key], value, key)
        self.assertLessEqual(
            self.summary["max_center_error_db"],
            self.summary["center_error_limit_db"],
        )
        self.assertLessEqual(
            self.summary["max_complex_error"],
            self.summary["complex_error_limit"],
        )

    def test_static_pcm_metrics(self) -> None:
        path = self.output / "equalizer_editor_pcm16_cases.csv"
        with path.open(encoding="utf-8", newline="") as source:
            rows = list(csv.DictReader(source))
        self.assertEqual(len(rows), 96)
        self.assertEqual(len({row["pattern"] for row in rows}), 6)
        self.assertEqual(len({row["signal"] for row in rows}), 16)
        self.assertEqual(sum(int(row["clip_count"]) for row in rows), 0)
        self.assertEqual(sum(int(row["nonfinite_count"]) for row in rows), 0)
        identity_rows = [
            row for row in rows if row["pattern"] == "E_all_zero"
        ]
        self.assertEqual(len(identity_rows), 16)
        self.assertTrue(all(row["identity_checked"] == "1" for row in identity_rows))
        self.assertEqual(
            sum(int(row["identity_mismatch_count"]) for row in identity_rows),
            0,
        )
        near_full = [
            row for row in rows if row["signal"] == "near_full_multitone"
        ]
        self.assertEqual(len(near_full), 6)
        for row in near_full:
            self.assertAlmostEqual(float(row["input_peak_fs"]), 0.891250938,
                                   delta=2.0 / 32768.0)
            self.assertLessEqual(float(row["output_peak_fs"]), 1.0)
        for row in rows:
            repeated = int(row["repeated_adjacent_frames"])
            expected = int(row["repeated_frames_expected"])
            if not expected:
                self.assertEqual(repeated, 0, (row["pattern"], row["signal"]))

    def test_theoretical_complex_and_pcm_response(self) -> None:
        path = self.output / "equalizer_editor_pcm16_response.csv"
        with path.open(encoding="utf-8", newline="") as source:
            rows = list(csv.DictReader(source))
        self.assertEqual(len(rows), 60)
        self.assertEqual(
            {(row["pattern"], int(row["band"])) for row in rows},
            {
                (pattern, band)
                for pattern in {
                    "A_125_plus3",
                    "B_1k_minus3",
                    "C_8k_plus3",
                    "D_v_like",
                    "E_all_zero",
                    "F_limits",
                }
                for band in range(10)
            },
        )
        numeric_fields = (
            "frequency_hz",
            "theory_real",
            "theory_imag",
            "measured_real",
            "measured_imag",
            "theory_magnitude_db",
            "measured_magnitude_db",
            "magnitude_error_db",
            "complex_abs_error",
        )
        for row in rows:
            self.assertTrue(
                all(math.isfinite(float(row[field])) for field in numeric_fields)
            )
            self.assertLessEqual(
                abs(float(row["magnitude_error_db"])), 0.15,
                (row["pattern"], row["band"]),
            )
            self.assertLessEqual(
                float(row["complex_abs_error"]), 0.025,
                (row["pattern"], row["band"]),
            )
            self.assertEqual(int(row["clip_count"]), 0)
            self.assertEqual(int(row["nonfinite_count"]), 0)

    def test_transition_contract(self) -> None:
        path = self.output / "equalizer_editor_pcm16_transitions.csv"
        with path.open(encoding="utf-8", newline="") as source:
            rows = list(csv.DictReader(source))
        self.assertEqual(
            {row["name"] for row in rows},
            {
                "flat_to_custom_a",
                "custom_a_to_b",
                "custom_to_vocal",
                "vocal_to_reset_flat",
            },
        )
        for row in rows:
            self.assertEqual(int(row["transition_total_samples"]), 6000)
            self.assertEqual(int(row["processed_samples"]), 6000)
            self.assertAlmostEqual(float(row["duration_ms"]), 120.0, places=6)
            self.assertGreater(int(row["progress_checks"]), 1)
            self.assertEqual(row["progress_monotonic"], "1")
            self.assertEqual(row["sequence_held_until_complete"], "1")
            self.assertEqual(row["applied_after_boundary"], "1")
            self.assertNotEqual(row["token"], row["previous_applied_sequence"])
            self.assertEqual(row["clip_count"], "0")
            self.assertEqual(row["nonfinite_count"], "0")
            self.assertEqual(row["repeated_adjacent_frames"], "0")
            for key in ("input_wav", "output_wav"):
                with wave.open(str(self.output / row[key]), "rb") as wav:
                    self.assertEqual(wav.getnchannels(), 1)
                    self.assertEqual(wav.getsampwidth(), 2)
                    self.assertEqual(wav.getframerate(), 50_000)
                    self.assertEqual(wav.getnframes(), 6000)

    def test_all_wavs_are_mono_pcm16_50k(self) -> None:
        paths = sorted(self.output.glob("*.wav"))
        self.assertEqual(len(paths), 120)
        for path in paths:
            with wave.open(str(path), "rb") as wav:
                self.assertEqual(wav.getnchannels(), 1, path.name)
                self.assertEqual(wav.getsampwidth(), 2, path.name)
                self.assertEqual(wav.getframerate(), 50_000, path.name)
                expected_frames = (
                    6000 if path.name.startswith("transition_") else 50_000
                )
                self.assertEqual(wav.getnframes(), expected_frames, path.name)

    def test_manifest_and_sha256_inventory(self) -> None:
        self.assertFalse(self.manifest["hardware_or_dss_used"])
        self.assertFalse(self.manifest["sound_card_used"])
        self.assertEqual(
            set(self.manifest["compiled_sources"]),
            {
                "Code/User/user_dsp/user_equalizer.c",
                "Code/User/user_dsp/user_equalizer_control.c",
                "tools/tests/equalizer_editor_pcm16_test.c",
            },
        )
        listed = {
            artifact["path"]: artifact["sha256"]
            for artifact in self.manifest["artifacts"]
        }
        for relative, digest in listed.items():
            self.assertEqual(sha256_file(self.output / relative), digest)

        sum_lines = (
            self.output / "SHA256SUMS.txt"
        ).read_text(encoding="ascii").splitlines()
        sums = dict(line.split("  ", 1)[::-1] for line in sum_lines)
        expected_paths = {
            path.relative_to(self.output).as_posix()
            for path in self.output.iterdir()
            if path.is_file() and path.name != "SHA256SUMS.txt"
        }
        self.assertEqual(set(sums), expected_paths)
        for relative, digest in sums.items():
            self.assertEqual(sha256_file(self.output / relative), digest)

    def test_deterministic_payload_hashes_across_two_runs(self) -> None:
        first = {
            artifact["path"]: artifact["sha256"]
            for artifact in self.manifest["artifacts"]
        }
        with tempfile.TemporaryDirectory(
            prefix="equalizer_editor_pcm16_repeat_"
        ) as directory:
            output = Path(directory) / "artifacts"
            result = subprocess.run(
                [
                    sys.executable,
                    "-B",
                    str(RUNNER),
                    "--output-dir",
                    str(output),
                ],
                cwd=ROOT,
                text=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                check=False,
            )
            self.assertEqual(result.returncode, 0, result.stdout)
            manifest = json.loads(
                (output / "manifest.json").read_text(encoding="utf-8")
            )
            second = {
                artifact["path"]: artifact["sha256"]
                for artifact in manifest["artifacts"]
            }
            self.assertEqual(second, first)

    def test_actual_equalizer_and_control_are_called(self) -> None:
        harness = HARNESS.read_text(encoding="utf-8")
        runner = RUNNER.read_text(encoding="utf-8")
        for call in (
            "Equalizer_ProcessFrame(",
            "Equalizer_GetSystemResponse(",
            "EqualizerControl_SubmitRequest(",
            "EqualizerControl_ServiceMailbox(",
            "EqualizerControl_ServiceOneBuilderSlice(",
            "EqualizerControl_TryInstallReady(",
            "EqualizerControl_ObserveFrameBoundary(",
        ):
            self.assertIn(call, harness)
        self.assertIn("Code/User/user_dsp/user_equalizer.c", runner)
        self.assertIn("Code/User/user_dsp/user_equalizer_control.c", runner)
        self.assertIn("output directory must be outside the repository", runner)

    def test_runner_rejects_repository_output_directory(self) -> None:
        forbidden = ROOT / "equalizer_editor_pcm16_generated_artifacts"
        self.assertFalse(forbidden.exists())
        result = subprocess.run(
            [
                sys.executable,
                "-B",
                str(RUNNER),
                "--output-dir",
                str(forbidden),
            ],
            cwd=ROOT,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            check=False,
        )
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("outside the repository", result.stdout)
        self.assertFalse(forbidden.exists())


if __name__ == "__main__":
    unittest.main()
