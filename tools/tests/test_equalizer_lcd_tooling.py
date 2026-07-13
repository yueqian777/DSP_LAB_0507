import csv
import json
import math
import struct
import subprocess
import sys
import tempfile
import unittest
import wave
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
ANALYZER = ROOT / "tools/equalizer_33_capture.py"
DSS = ROOT / "tools/dss/dss_equalizer_lcd_audio_safe.js"
WRAPPER = ROOT / "tools/run_equalizer_lcd_audio_safe_test.ps1"


def write_i16(path: Path, samples: list[int]) -> None:
    path.write_bytes(struct.pack("<" + "h" * len(samples), *samples))


class EqualizerCaptureAnalyzerTests(unittest.TestCase):
    def run_analyzer(self, *arguments: str) -> subprocess.CompletedProcess[str]:
        return subprocess.run(
            [sys.executable, "-B", str(ANALYZER), *arguments],
            cwd=ROOT,
            text=True,
            encoding="utf-8",
            errors="replace",
            capture_output=True,
            check=False,
        )

    def test_manual_capture_is_little_endian_raw_copy_at_50khz(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)
            frame = [-32000, -1234, 0, 1234, 32000]
            source = frame * 8
            write_i16(output / "input.raw", source)
            write_i16(output / "output.raw", source)
            result = self.run_analyzer(
                "manual", "--input", str(output / "input.raw"),
                "--output", str(output / "output.raw"),
                "--out-dir", str(output), "--mode", "raw",
                "--frame-len", "5",
            )
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            with wave.open(str(output / "input.wav"), "rb") as wav_file:
                self.assertEqual(wav_file.getframerate(), 50000)
                self.assertEqual(wav_file.getnchannels(), 1)
                self.assertEqual(wav_file.getsampwidth(), 2)
                self.assertEqual(wav_file.readframes(40), (output / "input.raw").read_bytes())
            report = json.loads((output / "capture_report.json").read_text(encoding="utf-8"))
            self.assertEqual(report["raw_copy_mismatch_count"], 0)
            self.assertEqual(report["input_peak"], 32000)
            self.assertEqual(report["output_peak"], 32000)
            self.assertEqual(report["input_clipping_count"], 0)
            self.assertAlmostEqual(report["correlation"], 1.0, places=9)
            self.assertEqual(report["snr_db"], "INF")

    def test_trigger_rotates_four_preframes_then_appends_postframes(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)
            frame_len = 3
            pre_ring = [10, 11, 12, 13]
            prewrite = 2
            chronological = pre_ring[prewrite:] + pre_ring[:prewrite]
            physical = [value for frame in pre_ring for value in [frame] * frame_len]
            post = [value for frame in range(20, 28) for value in [frame] * frame_len]
            write_i16(output / "input.raw", physical + post)
            write_i16(output / "output.raw", physical + post)
            result = self.run_analyzer(
                "trigger", "--input", str(output / "input.raw"),
                "--output", str(output / "output.raw"),
                "--out-dir", str(output), "--mode", "processed",
                "--frame-len", str(frame_len), "--prewrite-index", str(prewrite),
            )
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            expected = chronological + list(range(20, 28))
            raw = (output / "input_chronological.raw").read_bytes()
            actual = list(struct.unpack("<" + "h" * (len(raw) // 2), raw))[::frame_len]
            self.assertEqual(actual, expected)
            report = json.loads((output / "capture_report.json").read_text(encoding="utf-8"))
            self.assertIsNone(report["raw_copy_mismatch_count"])
            self.assertIn("frame_boundary_delta_max", report)
            self.assertTrue((output / "spectrum.csv").exists())
            with (output / "spectrum.csv").open(newline="", encoding="utf-8") as csv_file:
                self.assertEqual(next(csv.reader(csv_file)), ["frequency_hz", "input_magnitude", "output_magnitude"])

    def test_report_has_required_signal_metrics(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)
            samples = [int(1000 * math.sin(2 * math.pi * index / 20)) for index in range(160)]
            write_i16(output / "input.raw", samples)
            write_i16(output / "output.raw", [sample // 2 for sample in samples])
            result = self.run_analyzer(
                "manual", "--input", str(output / "input.raw"),
                "--output", str(output / "output.raw"),
                "--out-dir", str(output), "--mode", "processed", "--frame-len", "20",
            )
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            report = json.loads((output / "capture_report.json").read_text(encoding="utf-8"))
            required = {
                "raw_copy_mismatch_count", "input_peak", "output_peak",
                "input_clipping_count", "output_clipping_count", "correlation",
                "snr_db", "frame_boundary_delta_max", "input_frame_boundary_delta_max",
                "output_frame_boundary_delta_max", "raw_max_abs_error",
                "raw_max_abs_error_index",
            }
            self.assertTrue(required.issubset(report))

    def test_rejects_wrong_frame_counts_and_unequal_channels(self) -> None:
        cases = (("manual", 7), ("trigger", 11))
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)
            for kind, frames in cases:
                write_i16(output / "input.raw", [1] * frames * 4)
                write_i16(output / "output.raw", [1] * frames * 4)
                arguments = [
                    kind, "--input", str(output / "input.raw"), "--output",
                    str(output / "output.raw"), "--out-dir", str(output / kind),
                    "--mode", "processed", "--frame-len", "4",
                ]
                if kind == "trigger":
                    arguments.extend(("--prewrite-index", "0"))
                result = self.run_analyzer(*arguments)
                self.assertNotEqual(result.returncode, 0)
            write_i16(output / "input.raw", [1] * 8 * 4)
            write_i16(output / "output.raw", [1] * (8 * 4 - 1))
            result = self.run_analyzer(
                "manual", "--input", str(output / "input.raw"), "--output",
                str(output / "output.raw"), "--out-dir", str(output / "unequal"),
                "--mode", "processed", "--frame-len", "4",
            )
            self.assertNotEqual(result.returncode, 0)

    def test_raw_capture_returns_failure_for_mismatch_or_output_clipping(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)
            clean = [100] * 8 * 4
            write_i16(output / "input.raw", clean)
            mismatched = clean.copy()
            mismatched[7] = 101
            write_i16(output / "output.raw", mismatched)
            mismatch = self.run_analyzer(
                "manual", "--input", str(output / "input.raw"), "--output",
                str(output / "output.raw"), "--out-dir", str(output / "mismatch"),
                "--mode", "raw", "--frame-len", "4",
            )
            self.assertNotEqual(mismatch.returncode, 0)
            report = json.loads((output / "mismatch/capture_report.json").read_text(encoding="utf-8"))
            self.assertFalse(report["raw_copy_pass"])
            clipped = clean.copy()
            clipped[3] = 32767
            write_i16(output / "output.raw", clipped)
            clipping = self.run_analyzer(
                "manual", "--input", str(output / "input.raw"), "--output",
                str(output / "output.raw"), "--out-dir", str(output / "clipping"),
                "--mode", "raw", "--frame-len", "4",
            )
            self.assertNotEqual(clipping.returncode, 0)

    def test_processed_capture_rejects_silence_and_output_clipping(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)
            silence = [0] * 8 * 4
            write_i16(output / "input.raw", silence)
            write_i16(output / "output.raw", silence)
            silent = self.run_analyzer(
                "manual", "--input", str(output / "input.raw"), "--output",
                str(output / "output.raw"), "--out-dir", str(output / "silent"),
                "--mode", "processed", "--frame-len", "4",
            )
            self.assertNotEqual(silent.returncode, 0)
            signal = [100] * 8 * 4
            clipped = signal.copy()
            clipped[5] = -32768
            write_i16(output / "input.raw", signal)
            write_i16(output / "output.raw", clipped)
            clipping = self.run_analyzer(
                "manual", "--input", str(output / "input.raw"), "--output",
                str(output / "output.raw"), "--out-dir", str(output / "processed_clip"),
                "--mode", "processed", "--frame-len", "4",
            )
            self.assertNotEqual(clipping.returncode, 0)


class EqualizerDssContractTests(unittest.TestCase):
    def test_dss_has_masks_diagnostics_provenance_and_timeout(self) -> None:
        source = DSS.read_text(encoding="utf-8")
        for token in (
            "EQ_LCD_RUNTIME_STATUS", "EQ_LCD_RUNTIME_GAINS", "STATUS",
            "GAINS", "BOTH", "EQ_DebugProcessFrames",
            "EQ_DebugFrameLatencyDeadlineMissCount", "EQ_DebugFrameServiceOverlapCount",
            "EQ_DebugFrameServiceDroppedCount", "EQ_DebugLcdJobCount",
            "EQ_DebugLcdAutoDisableCount", "EQ_DebugLcdAutoDisableReason",
            "EQ_DebugClipCount", "commit_sha", "build_config", "dirty_state",
            "measurement_status", "NOT_OBSERVED", "MEASURED_DSS_WATCH_PLUS_OPERATOR",
            "DEBUG_CAPTURE", "60000", "300000",
        ):
            self.assertIn(token, source)

    def test_dss_connects_exact_cpu_and_disconnects_cleanly(self) -> None:
        source = DSS.read_text(encoding="utf-8")
        self.assertIn('openSession(".*C674X_0")', source)
        self.assertIn("debugSession.target.connect()", source)
        self.assertIn("debugSession.target.disconnect()", source)
        self.assertLess(source.index("target.disconnect()"), source.index("debugSession.terminate()"))

    def test_dss_uses_real_symbol_addresses_and_program_memory(self) -> None:
        source = DSS.read_text(encoding="utf-8")
        self.assertIn("debugSession.symbol.getAddress(symbol)", source)
        self.assertIn("Memory.Page.PROGRAM", source)
        self.assertNotIn('evaluate("&" + symbol)', source)

    def test_dss_rows_are_full_duration_and_track_baseline_deltas(self) -> None:
        source = DSS.read_text(encoding="utf-8")
        self.assertIn('recordManualSwitchingTest("mode_switch_5s", measurementMs, 5000)', source)
        self.assertIn('recordManualSwitchingTest("mode_switch_2s", measurementMs, 2000)', source)
        self.assertIn('mode = (mode + 1) % EQ_PRESET_COUNT', source)
        self.assertIn('"MANUAL_HARDWARE_TEST"', source)
        self.assertNotIn("function measureSwitchingRow", source)
        for token in (
            "EQ_DebugDeadlineMissCount", "EQ_DebugLcdUnexpectedFullRedrawCount",
            "baseline_", "delta_", "actual_runtime_mask", "INCOMPLETE_READ_ERROR",
            "INCOMPLETE_AUTO_DISABLED", "INCOMPLETE_MASK_MISMATCH",
            "INCOMPLETE_SAFETY_COUNTER_INCREASED",
        ):
            self.assertIn(token, source)

    def test_dss_pass_criteria_cover_audio_lcd_and_operator_requirements(self) -> None:
        source = DSS.read_text(encoding="utf-8")
        for token in (
            '"technical_pass"', '"pass"', '"pass_reasons"', "clip_count", "2280000",
            "delta_lcd_job_count", "Math.abs(processDelta - adDelta) <= 1",
            "Math.abs(processDelta - daDelta) <= 1", "activeMissDelta == 0",
            "latencyMissDelta == 0", "overlapDelta == 0", "droppedDelta == 0",
            "redrawDelta == 0", "disabledDelta == 0", "operatorStatus == \"PASS\"",
            "MEASURED_DSS_ONLY", "FINAL_PASS", "FINAL_INCOMPLETE",
        ):
            self.assertIn(token, source)

    def test_dss_records_capture_analyzer_failure_before_aborting(self) -> None:
        source = DSS.read_text(encoding="utf-8")
        self.assertIn("CAPTURE_ANALYZER_FAILED", source)
        self.assertIn("captureOverallPass = false", source)
        self.assertIn("analyzer_exit_code", source)
        analyzer_start = source.index("function runAnalyzer")
        analyzer_end = source.index("function captureManual", analyzer_start)
        body = source[analyzer_start:analyzer_end]
        self.assertIn("return exitCode", body)
        self.assertNotIn('throw "capture analyzer failed"', body)

    def test_dss_validates_trigger_metadata_before_export_and_analysis(self) -> None:
        source = DSS.read_text(encoding="utf-8")
        for token in (
            "actualSource != source", "postCount != 8", "readyValue != 1",
            "prewrite < 0", "prewrite > 3", "armedReady != 1",
            "armedSource != source", "INCOMPLETE_TRIGGER_METADATA",
            "captureOverallPass = false",
        ):
            self.assertIn(token, source)
        validation = source.index("actualSource != source")
        export_call = source.index('saveRawI16LE("EQ_TriggerCaptureInput"', validation)
        analyzer_call = source.index('runAnalyzer("trigger"', validation)
        self.assertLess(validation, export_call)
        self.assertLess(validation, analyzer_call)

    def test_dss_trigger_sources_have_real_preconditions_and_state_sequence(self) -> None:
        source = DSS.read_text(encoding="utf-8")
        trigger_start = source.index("function captureTrigger")
        trigger_end = source.index("try {", trigger_start)
        body = source[trigger_start:trigger_end]
        for token in (
            "if (source == 1)", "EQ_DebugLcdRuntimeMask = " , "MASK_BOTH",
            "EQ_DebugMode = 1", "if (source == 2)",
            "EQ_DebugLcdPendingMask", "NOT_OBSERVED",
        ):
            self.assertIn(token, body)
        armed_wait = body.index('read("EQ_TriggerCaptureArmedReady"')
        mode_write = body.index('write("EQ_DebugMode = 1"')
        ready_wait = body.index('read("EQ_TriggerCaptureReady"')
        self.assertLess(armed_wait, mode_write)
        self.assertLess(mode_write, ready_wait)

    def test_debug_capture_does_not_poll_or_fail_unobserved_audio_event(self) -> None:
        source = DSS.read_text(encoding="utf-8")
        self.assertNotIn("function waitFor", source)
        self.assertNotIn("pollMs", source)
        self.assertIn('capture_scope: "DEBUG_CAPTURE"', source)
        trigger_start = source.index("function captureTrigger")
        trigger_end = source.index("try {", trigger_start)
        body = source[trigger_start:trigger_end]
        self.assertIn("if (source != 4) captureOverallPass = false", body)
        self.assertIn('source == 4 ? "NOT_OBSERVED"', body)

    def test_dss_matrix_sets_diag_path_and_gates_stability(self) -> None:
        source = DSS.read_text(encoding="utf-8")
        for token in (
            "EQ_DebugDiagPath", "EQ_DebugMode", "RAW_COPY", "FLAT", "PRESET",
            '"--mode", analyzerMode', "allSixtySecondRowsComplete",
            "rowPassedSixtySecondCriteria", "delta_process_frames", "delta_ad_frames",
            "delta_da_frames",
        ):
            self.assertIn(token, source)
        self.assertLess(source.index("allSixtySecondRowsComplete"),
                        source.index('measureRow("stability_five_minutes"'))

    def test_dss_operator_status_requires_external_evidence(self) -> None:
        source = DSS.read_text(encoding="utf-8")
        for token in (
            "DSP_TEST_OPERATOR_STATUS", "DSP_TEST_OPERATOR_NOTES", "PASS", "FAIL",
            "NOT_OBSERVED", "MEASURED_DSS_WATCH_PLUS_OPERATOR",
        ):
            self.assertIn(token, source)
        self.assertNotIn('operatorMode ? "MEASURED_DSS_WATCH_PLUS_OPERATOR"', source)
        self.assertIn('measurement_status = "MANUAL_HARDWARE_TEST"', source)

    def test_dss_exports_only_ch1_after_halt_with_metadata(self) -> None:
        source = DSS.read_text(encoding="utf-8")
        for token in (
            "EQ_CaptureInput", "EQ_CaptureOutput", "EQ_TriggerCaptureInput",
            "EQ_TriggerCaptureOutput", "pre_write_index", "frame_count",
            "trigger_source", "trigger_post_count", "armed_ready", "ready",
            "target.halt", "saveRawI16LE", "DEBUG_CAPTURE",
            "EqualizerCapture_AcknowledgeManual",
            "EqualizerCapture_AcknowledgeTrigger", "EqualizerCapture_Reset",
        ):
            self.assertIn(token, source)
        self.assertNotIn("EQ_AD_Buffer2", source)
        self.assertNotIn("EQ_DA_Buffer2", source)
        self.assertNotIn("CaptureInput2", source)
        self.assertNotIn("CaptureOutput2", source)


class EqualizerPowerShellContractTests(unittest.TestCase):
    def test_wrapper_uses_output_only_playback_and_always_stops(self) -> None:
        source = WRAPPER.read_text(encoding="utf-8")
        lowered = source.lower()
        for token in (
            "50000", "1000", "-18", "music-like", "default pc line-output",
            "finally", "stop", "actual_project_macro", "DSP_LAB_PROJECT_SELECT",
            "subjective", "60", "300", "DssTimeoutMinutes", "TIMEOUT",
            "Start-Process", "WaitForExit", "taskkill.exe",
        ):
            self.assertIn(token.lower(), lowered)
        for forbidden in (
            "wavein", "wasapicapture", "microphone", "recordingdevice",
            "audioinput", "start-recording", "ffmpeg -f dshow",
        ):
            self.assertNotIn(forbidden, lowered)

    def test_wrapper_builds_and_validates_project33_lcd_artifacts_before_dss(self) -> None:
        source = WRAPPER.read_text(encoding="utf-8")
        for token in (
            "GmakePath", "-B", "GEN_OPTS__FLAG", "--define=DSP_LAB_PROJECT_SELECT=33",
            "--define=EQ_ENABLE_LCD_DISPLAY=1",
            "DSP_LAB_0507_linkInfo.xml", "<link_errors>0x0</link_errors>",
            "DSP_LAB_0507.out", "DSP_LAB_0507.map", "LastWriteTimeUtc",
            "project33_lcd_on_commands.log", "Expanded compiler commands",
            "--define=DSP_LAB_PROJECT_SELECT=33", "--define=EQ_ENABLE_LCD_DISPLAY=1",
            "OperatorStatus", "OperatorNotes", "DSP_TEST_OPERATOR_STATUS",
            "DSP_TEST_OPERATOR_NOTES",
        ):
            self.assertIn(token, source)
        self.assertLess(source.index("& $gmake"), source.index("$player.PlayLooping()"))

    def test_paths_are_parameterized_and_main_source_is_not_required_to_be_33(self) -> None:
        dss_source = DSS.read_text(encoding="utf-8")
        wrapper = WRAPPER.read_text(encoding="utf-8")
        for forbidden in ("C:/Users/", "D:/SoftwareDownload/"):
            self.assertNotIn(forbidden, dss_source)
            self.assertNotIn(forbidden, wrapper)
        for token in ("RepoRoot", "CcsRoot", "PythonPath", "DssPath", "GmakePath"):
            self.assertIn(token, wrapper)
        self.assertNotIn("Code/main.c must select Project 33", wrapper)
        self.assertNotIn("Get-Content -Raw (Join-Path $root \"Code\\main.c\")", wrapper)


if __name__ == "__main__":
    unittest.main()
