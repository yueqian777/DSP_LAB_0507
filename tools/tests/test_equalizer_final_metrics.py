"""Contracts for the Project 3.3 final internal-digital metric harness."""

from __future__ import annotations

import importlib.util
import json
import math
from pathlib import Path
import subprocess
import tempfile
import unittest

import numpy as np


ROOT = Path(__file__).resolve().parents[2]
BASH = Path(r"C:\msys64\usr\bin\bash.exe")
ANALYZER_PATH = ROOT / "tools/analyze_equalizer_final_metrics.py"
SPEC = importlib.util.spec_from_file_location("equalizer_final_metrics", ANALYZER_PATH)
assert SPEC and SPEC.loader
ANALYZER = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(ANALYZER)


def msys_path(path: Path) -> str:
    resolved = path.resolve()
    return f"/{resolved.drive[0].lower()}{resolved.as_posix()[2:]}"


def run_msys(command: str) -> subprocess.CompletedProcess[str]:
    full = ("export PATH=/mingw64/bin:/usr/bin:$PATH; "
            f"cd {msys_path(ROOT)}; {command}")
    return subprocess.run([str(BASH), "-lc", full], check=True,
                          capture_output=True, text=True)


class FinalMetricsHarnessTests(unittest.TestCase):
    def test_default_off_and_production_object_has_no_buffers(self) -> None:
        header = (ROOT / "Code/User/user_dsp/user_equalizer_eval.h").read_text(
            encoding="utf-8")
        flow = (ROOT / "Code/User/user_dsp/user_equalizer_flow.c").read_text(
            encoding="utf-8")
        self.assertIn("#define EQ_ENABLE_FINAL_METRICS_BOARD_TEST 0", header)
        self.assertIn("EqualizerEval_BoardFinalMetrics();", flow)
        self.assertIn("#pragma diag_suppress 179", flow)
        with tempfile.TemporaryDirectory(prefix="eq_final_metrics_obj_") as temp:
            object_path = Path(temp) / "production.o"
            run_msys(
                "gcc -std=c89 -Wall -Wextra -Werror -Wno-unknown-pragmas "
                "-DEQ_FRAME_LEN=1024 -ICode/User/user_dsp "
                "-c Code/User/user_dsp/user_equalizer_eval.c "
                f"-o {msys_path(object_path)}")
            symbols = run_msys(f"nm {msys_path(object_path)}").stdout
            self.assertNotIn("EQ_FinalMetrics", symbols)
            self.assertNotIn("EQ_DebugFinalMetrics", symbols)

    def test_host_executes_all_dedicated_cases(self) -> None:
        with tempfile.TemporaryDirectory(prefix="eq_final_metrics_run_") as temp:
            executable = Path(temp) / "equalizer_final_metrics_test.exe"
            result = run_msys(
                "gcc -std=c99 -Wall -Wextra -Werror -Wno-unknown-pragmas "
                "-DEQ_FRAME_LEN=1024 "
                "-DEQ_ENABLE_FINAL_METRICS_BOARD_TEST=1 "
                "-DEQ_USE_GENERATED_BUILD_ID -ICode/User/user_dsp "
                "Code/User/user_dsp/user_equalizer.c "
                "Code/User/user_dsp/user_equalizer_eval.c "
                "tools/tests/equalizer_final_metrics_test.c -lm "
                f"-o {msys_path(executable)}; {msys_path(executable)}")
            self.assertIn("failures=0 cases=60", result.stdout)

    def test_board_runner_requires_clean_identity_and_restores_full_image(self) -> None:
        runner = (ROOT / "tools/run_equalizer_final_metrics.ps1").read_text(
            encoding="utf-8")
        dss = (ROOT / "tools/dss/dss_equalizer_final_metrics.js").read_text(
            encoding="utf-8")

        for contract in (
            "Close CCS before exclusive DSS access.",
            "Formal metrics run requires clean main",
            "generate_equalizer_build_id.ps1",
            "--define=EQ_ENABLE_FINAL_METRICS_BOARD_TEST=1",
            "--define=EQ_ENABLE_AUDIO_FEATURE_ANALYZER=0",
            "--define=EQ_ENABLE_LCD_DISPLAY=0",
            "<link_errors>0x0</link_errors>",
            '$ErrorActionPreference = "Continue"',
            "buildExitCode",
            "warningCount -ne 0",
            "H_project33_full",
            "dss_project33_leave_running.js",
            'restoreResult = "RUNNING_DISCONNECTED"',
        ):
            self.assertIn(contract, runner)

        for contract in (
            "debugSession.memory.loadProgram(program)",
            "EQ_DebugFinalMetricsCompiled",
            "EQ_DebugFinalMetricsBuildGitSha",
            "EQ_DebugFinalMetricsBuildDirty",
            "EQ_DebugFinalMetricsCompletedCases",
            "EQ_DebugFinalMetricsMaxFrameCycles",
            "EQ_FinalMetricsResponsePacked",
            "EQ_FinalMetricsThdInputPacked",
            "EQ_FinalMetricsThdOutputPacked",
            "EQ_FinalMetricsSnrInputPacked",
            "EQ_FinalMetricsSnrOutputPacked",
            "Normal metrics case reported clipping",
            "debugSession.target.disconnect()",
        ):
            self.assertIn(contract, dss)


class FinalMetricsAnalyzerTests(unittest.TestCase):
    @staticmethod
    def _write_raw(path: Path, samples: np.ndarray) -> None:
        np.asarray(samples, dtype="<i2").tofile(path)

    def test_synthetic_capture_contract(self) -> None:
        with tempfile.TemporaryDirectory(prefix="eq_final_metrics_analysis_") as temp:
            raw_dir = Path(temp) / "raw"
            output_dir = Path(temp) / "output"
            raw_dir.mkdir()

            impulse = np.zeros(ANALYZER.CAPTURE_SAMPLES, dtype=np.int16)
            impulse[0] = int(ANALYZER.IMPULSE_PEAK)
            responses = np.stack([
                ANALYZER.high_precision_reference(impulse, model_name)
                for model_name in ANALYZER.MODEL_NAMES
            ])

            sample_index = np.arange(ANALYZER.CAPTURE_SAMPLES,
                                     dtype=np.float64)
            thd_inputs = np.stack([
                ANALYZER._round_pcm16(
                    4125.0 * np.sin(2.0 * math.pi * frequency * sample_index /
                                    ANALYZER.SAMPLE_RATE))
                for frequency in ANALYZER.THD_FREQUENCIES_HZ
            ])
            thd_outputs = np.repeat(
                thd_inputs[np.newaxis, :, :], len(ANALYZER.CASE_NAMES), axis=0)

            total = ANALYZER.PREROLL_SAMPLES + ANALYZER.CAPTURE_SAMPLES
            full_index = np.arange(total, dtype=np.float64)
            rng = np.random.default_rng(0x13579BDF)
            snr_inputs = np.stack((
                rng.integers(-1000, 1001, size=total, dtype=np.int16),
                ANALYZER._round_pcm16(
                    700.0 * np.sin(2.0 * math.pi * 390.625 * full_index /
                                   ANALYZER.SAMPLE_RATE) +
                    500.0 * np.sin(2.0 * math.pi * 1953.125 * full_index /
                                   ANALYZER.SAMPLE_RATE)),
                ANALYZER._round_pcm16(
                    450.0 * np.sin(2.0 * math.pi * 976.5625 * full_index /
                                   ANALYZER.SAMPLE_RATE) +
                    450.0 * np.sin(2.0 * math.pi * 8007.8125 * full_index /
                                   ANALYZER.SAMPLE_RATE)),
            ))
            snr_outputs = np.empty(
                (len(ANALYZER.CASE_NAMES), len(ANALYZER.SIGNAL_NAMES),
                 ANALYZER.CAPTURE_SAMPLES), dtype=np.int16)
            for case_index, model_name in enumerate(ANALYZER.MODEL_NAMES):
                for signal_index in range(len(ANALYZER.SIGNAL_NAMES)):
                    reference = ANALYZER.high_precision_reference(
                        snr_inputs[signal_index], model_name)
                    snr_outputs[case_index, signal_index] = reference[
                        ANALYZER.PREROLL_SAMPLES:]

            self._write_raw(raw_dir / "response_packed.raw", responses)
            self._write_raw(raw_dir / "thd_input_packed.raw", thd_inputs)
            self._write_raw(raw_dir / "thd_output_packed.raw", thd_outputs)
            self._write_raw(raw_dir / "snr_input_packed.raw", snr_inputs)
            self._write_raw(raw_dir / "snr_output_packed.raw", snr_outputs)
            dss = {
                "pass": True,
                "commit_sha": "0" * 40,
                "output_sha256": "1" * 64,
                "response_clip_count": [0] * len(ANALYZER.CASE_NAMES),
                "thd_clip_count": [0] * (
                    len(ANALYZER.CASE_NAMES) *
                    len(ANALYZER.THD_FREQUENCIES_HZ)),
                "snr_clip_count": [0] * (
                    len(ANALYZER.CASE_NAMES) * len(ANALYZER.SIGNAL_NAMES)),
            }
            (raw_dir / "dss_summary.json").write_text(
                json.dumps(dss), encoding="utf-8")

            summary = ANALYZER.analyze(raw_dir, output_dir)
            self.assertTrue(summary["pass"])
            self.assertEqual(summary["thd"]["thd_limit_db"], -60.0)
            self.assertEqual(summary["thd"]["normal_mode_clip_count"], 0)
            self.assertTrue(summary["reference_snr"]["flat_all_byte_exact"])
            for name in (
                "static_eq_frequency_response.csv",
                "static_eq_group_delay.csv",
                "static_eq_response_summary.json",
                "static_eq_thd_cases.csv",
                "static_eq_thd_summary.json",
                "static_eq_reference_snr.csv",
                "static_eq_reference_snr_summary.json",
                "final_metrics_analysis_summary.json",
            ):
                self.assertTrue((output_dir / name).is_file(), name)


if __name__ == "__main__":
    unittest.main()
