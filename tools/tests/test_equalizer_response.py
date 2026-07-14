from __future__ import annotations

import math
import pathlib
import subprocess
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[2]
HEADER = ROOT / "Code/User/user_dsp/user_equalizer_response.h"
SOURCE = ROOT / "Code/User/user_dsp/user_equalizer_response.c"
CORE = ROOT / "Code/User/user_dsp/user_equalizer.c"
BASH = pathlib.Path(r"C:\msys64\usr\bin\bash.exe")
CENTERS = [31.25, 62.5, 125.0, 250.0, 500.0,
           1000.0, 2000.0, 4000.0, 8000.0, 16000.0]


def desired_curve(gains: list[float], frequency_hz: float) -> float:
    if frequency_hz <= CENTERS[0]:
        return gains[0]
    if frequency_hz >= CENTERS[-1]:
        return gains[-1]
    log_frequency = math.log2(frequency_hz)
    for band in range(len(CENTERS) - 1):
        if frequency_hz <= CENTERS[band + 1]:
            mix = ((log_frequency - math.log2(CENTERS[band])) /
                   (math.log2(CENTERS[band + 1]) -
                    math.log2(CENTERS[band])))
            return gains[band] + mix * (gains[band + 1] - gains[band])
    raise AssertionError("unreachable")


class EqualizerResponseTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.header = HEADER.read_text(encoding="utf-8")
        cls.source = SOURCE.read_text(encoding="utf-8")
        cls.core = CORE.read_text(encoding="utf-8")

    def test_host_harness(self) -> None:
        command = (
            "export PATH=/mingw64/bin:/usr/bin:$PATH; "
            "cd /c/Users/zhangyueqian/lab8/DSP_LAB_0507; "
            "gcc -std=c99 -Wall -Wextra -Werror -DEQ_ALGO_ONLY "
            "-ICode/User/user_dsp Code/User/user_dsp/user_equalizer.c "
            "Code/User/user_dsp/user_equalizer_control.c "
            "Code/User/user_dsp/user_equalizer_response.c "
            "tools/tests/equalizer_response_test.c -lm "
            "-o /tmp/equalizer_response_test.exe && "
            "/tmp/equalizer_response_test.exe"
        )
        result = subprocess.run(
            [str(BASH), "-lc", command], text=True,
            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, check=False)
        self.assertEqual(result.returncode, 0, result.stdout)
        self.assertIn("failures=0", result.stdout)

    def test_snapshot_has_no_runtime_filter_state(self) -> None:
        start = self.header.index("typedef struct\n{", self.header.index(
            "EQ_RESPONSE_COMPLEX"))
        end = self.header.index("} EQ_RESPONSE_SNAPSHOT;", start)
        snapshot = self.header[start:end]
        for forbidden in ("s1", "s2", "clip_count", "audio"):
            self.assertNotIn(forbidden, snapshot)

    def test_all_path_and_role_kinds_are_explicit(self) -> None:
        for name in (
            "IDENTITY_RAW_COPY", "IDENTITY_FLOAT_COPY",
            "IDENTITY_HARD_BYPASS", "IDENTITY_RBJ_FLAT_COPY",
            "DRY_TO_BANK_TRANSITION", "ACTIVE_BANK",
            "BANK_TO_BANK_TRANSITION", "UNSUPPORTED_LEGACY",
            "ROLE_ACTIVE", "ROLE_PENDING", "ROLE_PREPARED_TARGET",
            "ROLE_LOGICAL_TARGET",
        ):
            self.assertIn(name, self.header)

    def test_response_uses_core_canonical_biquad_math(self) -> None:
        section_start = self.source.index(
            "int EqualizerResponse_GetSectionComplex(")
        section_end = self.source.index(
            "int EqualizerResponse_GetCascadeComplex", section_start)
        section = self.source[section_start:section_end]
        self.assertIn("Equalizer_GetBiquadResponseComplex(", section)
        self.assertNotIn("numerator_real", section)
        self.assertEqual(self.core.count(
            "int Equalizer_GetBiquadResponseComplex("), 1)

    def test_independent_log_interpolation(self) -> None:
        gains = [-3.0, -1.0, 1.0, 3.0, 4.0,
                 2.0, 0.0, -2.0, 1.0, 3.0]
        for center, gain in zip(CENTERS, gains):
            self.assertAlmostEqual(desired_curve(gains, center), gain, places=12)
        self.assertAlmostEqual(desired_curve(gains, math.sqrt(500.0 * 1000.0)),
                               3.0, places=12)


if __name__ == "__main__":
    unittest.main()
