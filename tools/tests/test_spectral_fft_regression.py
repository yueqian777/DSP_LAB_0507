from __future__ import annotations

import pathlib
import re
import shlex
import subprocess
import tempfile
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[2]
BASH = pathlib.Path(r"C:\msys64\usr\bin\bash.exe")
SOURCE = ROOT / "Code/User/user_dsp/user_spectral_fft.c"
HARNESS = ROOT / "tools/tests/spectral_fft_regression_test.c"
EXPECTED_BIT_EXACT_CASES = 8
EXPECTED_INVALID_CHECKS = 18
EXPECTED_WOLA_BIT_EXACT_HOPS = 12


def msys_path(path: pathlib.Path) -> str:
    resolved = path.resolve()
    drive = resolved.drive[:1].lower()
    if not drive:
        raise AssertionError(f"expected an absolute Windows path: {resolved}")
    return f"/{drive}/{resolved.as_posix()[3:]}"


class SpectralFFTRegressionTest(unittest.TestCase):
    def test_shared_fft_matches_frozen_project32_kernel(self) -> None:
        self.assertTrue(BASH.is_file(), f"missing MSYS2 bash: {BASH}")
        self.assertTrue(HARNESS.is_file(), f"missing C harness: {HARNESS}")

        with tempfile.TemporaryDirectory(
            prefix="spectral_fft_regression_"
        ) as temp_dir:
            executable = pathlib.Path(temp_dir) / "spectral_fft_test.exe"
            command = " ".join(
                (
                    "gcc",
                    "-std=c99",
                    "-Wall",
                    "-Wextra",
                    "-Werror",
                    "-ICode/User/user_dsp",
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
                timeout=30,
            )

        self.assertEqual(result.returncode, 0, result.stdout)
        self.assertNotIn("FAIL:", result.stdout)
        self.assertIn("spectral fft regression: PASS", result.stdout)

        summary = re.search(
            r"failures=(\d+)\s+bit_exact_cases=(\d+)\s+"
            r"wola_bit_exact_hops=(\d+)\s+invalid_checks=(\d+)\s+"
            r"sanity_1024=(\d+)",
            result.stdout,
        )
        self.assertIsNotNone(summary, result.stdout)
        assert summary is not None
        self.assertEqual(int(summary.group(1)), 0, result.stdout)
        self.assertEqual(
            int(summary.group(2)), EXPECTED_BIT_EXACT_CASES, result.stdout
        )
        self.assertEqual(
            int(summary.group(3)), EXPECTED_WOLA_BIT_EXACT_HOPS,
            result.stdout,
        )
        self.assertEqual(
            int(summary.group(4)), EXPECTED_INVALID_CHECKS, result.stdout
        )
        self.assertEqual(int(summary.group(5)), 1, result.stdout)


if __name__ == "__main__":
    unittest.main()
