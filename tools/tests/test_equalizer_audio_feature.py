from __future__ import annotations

import pathlib
import re
import shlex
import subprocess
import tempfile
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[2]
BASH = pathlib.Path(r"C:\msys64\usr\bin\bash.exe")
SOURCE = ROOT / "Code/User/user_dsp/user_audio_feature_analyzer.c"
SPECTRAL_FFT = ROOT / "Code/User/user_dsp/user_spectral_fft.c"
HARNESS = ROOT / "tools/tests/equalizer_audio_feature_test.c"


def msys_path(path: pathlib.Path) -> str:
    resolved = path.resolve()
    drive = resolved.drive[:1].lower()
    if not drive:
        raise AssertionError(f"expected an absolute Windows path: {resolved}")
    return f"/{drive}/{resolved.as_posix()[3:]}"


class EqualizerAudioFeatureTest(unittest.TestCase):
    def test_host_harness(self) -> None:
        self.assertTrue(BASH.is_file(), f"missing MSYS2 bash: {BASH}")
        self.assertTrue(SOURCE.is_file(), f"missing analyzer source: {SOURCE}")
        self.assertTrue(
            SPECTRAL_FFT.is_file(), f"missing spectral FFT source: {SPECTRAL_FFT}"
        )
        self.assertTrue(HARNESS.is_file(), f"missing C harness: {HARNESS}")

        with tempfile.TemporaryDirectory(
            prefix="equalizer_audio_feature_"
        ) as temp_dir:
            executable = pathlib.Path(temp_dir) / "audio_feature_test.exe"
            command = " ".join(
                (
                    "gcc",
                    "-std=c99",
                    "-Wall",
                    "-Wextra",
                    "-Werror",
                    "-DEQ_ALGO_ONLY",
                    "-ICode/User/user_dsp",
                    shlex.quote(msys_path(SPECTRAL_FFT)),
                    shlex.quote(msys_path(SOURCE)),
                    shlex.quote(msys_path(HARNESS)),
                    "-lm",
                    "-o",
                    shlex.quote(msys_path(executable)),
                    "&&",
                    shlex.quote(msys_path(executable)),
                )
            )
            result = subprocess.run(
                [
                    str(BASH),
                    "-lc",
                    "export PATH=/mingw64/bin:/usr/bin:$PATH; "
                    f"cd {shlex.quote(msys_path(ROOT))}; {command}",
                ],
                text=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                check=False,
            )

        self.assertEqual(result.returncode, 0, result.stdout)
        self.assertIn("equalizer audio feature: PASS", result.stdout)
        self.assertNotIn("FAIL:", result.stdout)

        def metric(name: str) -> float:
            match = re.search(
                rf"(?:^|\s){re.escape(name)}=(-?\d+(?:\.\d+)?)",
                result.stdout,
            )
            self.assertIsNotNone(match, result.stdout)
            assert match is not None
            return float(match.group(1))

        self.assertEqual(metric("silence_valid"), 0.0)
        self.assertAlmostEqual(metric("tone_bass_hz"), 97.65625, delta=0.001)
        self.assertAlmostEqual(metric("tone_mud_hz"), 390.625, delta=0.001)
        self.assertAlmostEqual(
            metric("tone_presence_hz"), 1953.125, delta=0.001
        )
        self.assertAlmostEqual(
            metric("tone_brightness_hz"), 8007.8125, delta=0.001
        )
        self.assertGreaterEqual(metric("tone_dominance_min_db"), 12.0)
        self.assertAlmostEqual(metric("bass_boost_db"), 12.041, delta=0.01)
        self.assertAlmostEqual(metric("rms_delta_db"), 6.0206, delta=0.15)
        self.assertLess(metric("relative_invariance_max_db"), 0.25)
        self.assertGreaterEqual(metric("noise_analyses"), 64.0)
        self.assertLess(metric("noise_density_spread_db"), 1.5)
        self.assertGreaterEqual(metric("deterministic_snapshots"), 64.0)
        self.assertGreater(metric("attack_progress"),
                           metric("release_progress"))
        self.assertGreaterEqual(metric("invalid_call_checks"), 10.0)
        self.assertGreaterEqual(metric("input_immutable_checks"), 5.0)
        self.assertRegex(
            result.stdout,
            r"cadence_updates=10\s+cadence_frames=80\s+cadence_period=8",
        )
        self.assertRegex(
            result.stdout,
            r"split_due=10\s+split_analyses=10\s+split_frames=80\s+"
            r"split_skipped=70",
        )
        self.assertRegex(result.stdout, r"failures=0(?:\s|$)")


if __name__ == "__main__":
    unittest.main()
