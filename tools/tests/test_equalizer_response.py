from __future__ import annotations

import importlib.util
import math
import pathlib
import subprocess
import tempfile
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[2]
HEADER = ROOT / "Code/User/user_dsp/user_equalizer_response.h"
SOURCE = ROOT / "Code/User/user_dsp/user_equalizer_response.c"
CORE = ROOT / "Code/User/user_dsp/user_equalizer.c"
BASH = pathlib.Path(r"C:\msys64\usr\bin\bash.exe")
CENTERS = [31.25, 62.5, 125.0, 250.0, 500.0,
           1000.0, 2000.0, 4000.0, 8000.0, 16000.0]
REPORT_PATH = ROOT / "tools/equalizer_33_response_report.py"
REPORT_SPEC = importlib.util.spec_from_file_location(
    "equalizer_33_response_report", REPORT_PATH)
assert REPORT_SPEC is not None and REPORT_SPEC.loader is not None
REPORT = importlib.util.module_from_spec(REPORT_SPEC)
REPORT_SPEC.loader.exec_module(REPORT)


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
        end = self.header.index("} EQ_RESPONSE_SNAPSHOT;")
        start = self.header.rfind("typedef struct\n{", 0, end)
        snapshot = self.header[start:end]
        for forbidden in ("s1", "s2", "clip_count", "audio"):
            self.assertNotIn(forbidden, snapshot)

    def test_all_path_and_role_kinds_are_explicit(self) -> None:
        for name in (
            "IDENTITY_RAW_COPY", "IDENTITY_FLOAT_COPY",
            "IDENTITY_HARD_BYPASS", "IDENTITY_RBJ_FLAT_COPY",
            "IDENTITY_RETURN_HOLD",
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

    def test_flat_headroom_and_response_are_identity(self) -> None:
        gains = REPORT.PRESETS["flat"]
        bank = REPORT.design_bank(gains)
        peak_db, preamp_db, preamp_gain = REPORT.headroom(bank, gains)
        self.assertEqual(peak_db, 0.0)
        self.assertEqual(preamp_db, 0.0)
        self.assertEqual(preamp_gain, 1.0)
        for frequency in REPORT.log_grid(129):
            shape = REPORT.magnitude_db(
                REPORT.cascade_response(bank, frequency))
            self.assertAlmostEqual(shape, 0.0, places=12)
            self.assertAlmostEqual(shape + preamp_db, 0.0, places=12)

    def test_interaction_uses_one_db_shape_only_without_mutation(self) -> None:
        flat_before = tuple(REPORT.PRESETS["flat"])
        matrix = REPORT.interaction_matrix()
        self.assertEqual(REPORT.INTERACTION_PERTURBATION_DB, 1.0)
        self.assertEqual(len(matrix), 10)
        self.assertTrue(all(len(row) == 10 for row in matrix))
        self.assertEqual(sum(math.isfinite(value)
                             for row in matrix for value in row), 100)
        self.assertEqual(tuple(REPORT.PRESETS["flat"]), flat_before)
        start = REPORT_PATH.read_text(encoding="utf-8").index(
            "def interaction_matrix(")
        end = REPORT_PATH.read_text(encoding="utf-8").index(
            "\ndef _write_csv", start)
        implementation = REPORT_PATH.read_text(encoding="utf-8")[start:end]
        self.assertNotIn("headroom(", implementation)
        self.assertNotIn("preamp", implementation)

    def test_board_response_module_excludes_host_math(self) -> None:
        math_include = self.source.index('#include "math.h"')
        host_guard = self.source.rfind("#ifdef EQ_ALGO_ONLY", 0, math_include)
        self.assertGreaterEqual(host_guard, 0)
        magnitude = self.source.index("int EqualizerResponse_GetMagnitudeDb(")
        host_guard = self.source.rfind("#ifdef EQ_ALGO_ONLY", 0, magnitude)
        self.assertGreaterEqual(host_guard, 0)

    def test_independent_report_and_c_reference_metrics(self) -> None:
        compile_result = subprocess.run(
            [str(BASH), "-lc",
             "export PATH=/mingw64/bin:/usr/bin:$PATH; "
             "cd /c/Users/zhangyueqian/lab8/DSP_LAB_0507; "
             "gcc -std=c99 -Wall -Wextra -Werror -DEQ_ALGO_ONLY "
             "-ICode/User/user_dsp Code/User/user_dsp/user_equalizer.c "
             "Code/User/user_dsp/user_equalizer_control.c "
             "Code/User/user_dsp/user_equalizer_response.c "
             "tools/tests/equalizer_response_test.c -lm "
             "-o /tmp/equalizer_response_test.exe"],
            text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
            check=False)
        self.assertEqual(compile_result.returncode, 0, compile_result.stdout)
        with tempfile.TemporaryDirectory(prefix="equalizer_33_response_") as temp:
            root = pathlib.Path(temp)
            output = root / "report"
            export_result = subprocess.run(
                [r"C:\msys64\tmp\equalizer_response_test.exe",
                 "--export", str(root)],
                text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                check=False)
            self.assertEqual(export_result.returncode, 0, export_result.stdout)
            report_result = subprocess.run(
                [r"D:\SoftwareDownload\python.exe", "-B",
                 str(ROOT / "tools/equalizer_33_response_report.py"),
                 "--output-dir", str(output),
                 "--c-reference-dir", str(root)],
                text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                check=False)
            self.assertEqual(report_result.returncode, 0, report_result.stdout)
            metrics = __import__("json").loads(report_result.stdout.strip())
            self.assertLess(metrics["c_python_shape_max_error_db"], 0.01)
            self.assertLess(metrics["c_python_total_max_error_db"], 0.01)
            self.assertLess(metrics["builder_sync_coefficient_max_diff"], 1e-6)
            self.assertLess(metrics["builder_sync_peak_diff_db"], 1e-4)
            self.assertLess(metrics["builder_sync_preamp_diff_db"], 1e-4)
            self.assertEqual(metrics["python_flat_peak_db"], 0.0)
            self.assertEqual(metrics["python_flat_preamp_db"], 0.0)
            self.assertEqual(metrics["python_flat_preamp_gain"], 1.0)
            self.assertEqual(metrics["c_flat_peak_db"], 0.0)
            self.assertEqual(metrics["c_flat_preamp_db"], 0.0)
            self.assertEqual(metrics["c_flat_preamp_gain"], 1.0)
            self.assertLess(metrics["c_python_flat_shape_max_error_db"],
                            1e-6)
            self.assertLess(metrics["c_python_flat_total_max_error_db"],
                            1e-6)
            self.assertEqual(metrics["interaction_rows"], 10.0)
            self.assertEqual(metrics["interaction_columns"], 10.0)
            self.assertEqual(metrics["interaction_finite_count"], 100.0)
            self.assertEqual(metrics["interaction_perturbation_db"], 1.0)
            for name in (
                "interaction_diagonal_mean_db",
                "interaction_off_diagonal_abs_max_db",
                "interaction_off_diagonal_rms_db",
                "interaction_ratio",
                "c_python_phase_max_error_rad",
                "c_python_group_delay_max_error_samples",
            ):
                self.assertTrue(math.isfinite(metrics[name]), name)
            self.assertGreater(metrics["group_delay_valid_point_count"], 0.0)
            self.assertLess(metrics["c_python_phase_max_error_rad"], 1e-6)
            self.assertLess(
                metrics["c_python_group_delay_max_error_samples"], 1e-4)
            self.assertLess(metrics["flat_group_delay_abs_max_samples"],
                            1e-9)
            expected_png = {
                "target_vs_actual_flat.png", "target_vs_actual_bass.png",
                "target_vs_actual_vocal.png", "target_vs_actual_treble.png",
                "target_vs_actual_v_shape.png", "target_vs_actual_custom.png",
                "rbj_individual_sections.png",
                "interaction_matrix_heatmap.png",
                "group_delay_comparison.png",
            }
            self.assertEqual({path.name for path in output.glob("*.png")},
                             expected_png)
            for name in ("response_curves.csv", "section_response.csv",
                         "group_delay.csv", "interaction_matrix.csv",
                         "metrics.csv"):
                self.assertTrue((output / name).is_file(), name)


if __name__ == "__main__":
    unittest.main()
