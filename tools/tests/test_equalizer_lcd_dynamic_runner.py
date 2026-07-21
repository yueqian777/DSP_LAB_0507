from pathlib import Path
import subprocess
import unittest


ROOT = Path(__file__).resolve().parents[2]
DSS = ROOT / "tools/dss/dss_equalizer_lcd_dynamic.js"
RUNNER = ROOT / "tools/run_equalizer_lcd_dynamic_hardware.ps1"


class EqualizerLcdDynamicDssContractTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.source = DSS.read_text(encoding="utf-8")

    def test_uses_one_load_and_leaves_target_running_disconnected(self) -> None:
        self.assertEqual(self.source.count("debugSession.target.connect();"), 1)
        self.assertEqual(self.source.count("debugSession.target.reset();"), 1)
        self.assertEqual(self.source.count("debugSession.memory.loadProgram(program);"), 1)
        self.assertIn('openSession(".*C674X_0")', self.source)
        self.assertIn("debugSession.target.runAsynch();", self.source)
        self.assertIn("debugSession.target.disconnect();", self.source)
        self.assertIn('"RUNNING_DISCONNECTED"', self.source)

    def test_is_ten_minute_no_touch_no_preset_dynamic_profile(self) -> None:
        for token in (
            'env("DSP_TEST_DURATION_MS", "600000")',
            "write(\"EQ_DebugAnalyzerEnabled = 1\")",
            "write(\"EQ_DebugSmartBassEnabled = 1\")",
            "write(\"EQ_DebugDynamicClarityEnabled = 1\")",
            "write(\"EQ_DebugHarshnessGuardEnabled = 1\")",
            "runFor(durationMs)",
            "EQ_DebugLcdRuntimeMask",
        ):
            self.assertIn(token, self.source)
        for forbidden in (
            "EQ_DebugMode =", "EQ_DebugRequestedMode =",
            "EQ_DebugTouch", "Touch_Scan", "Touch_Init",
        ):
            self.assertNotIn(forbidden, self.source)

    def test_gates_audio_analyzer_lcd_timing_and_raster_state(self) -> None:
        for token in (
            "EQ_DebugAdFrames", "EQ_DebugDaFrames", "EQ_DebugProcessFrames",
            "EQ_DebugInputPeak", "EQ_DebugOutputPeak", "EQ_DebugClipCount",
            "EQ_DebugAnalyzerAnalysisCount", "EQ_DebugAnalyzerValid",
            "EQ_DebugAnalyzerWarmup", "EQ_DebugLcdAnalyzerStripCount",
            "EQ_DebugLcdAnalyzerMaxStripHeight", "MAX_ANALYZER_STRIP_HEIGHT = 16",
            "LCD_ACCEPTANCE_CYCLES = 912000",
            "MAX_LCD_JOBS_PER_SECOND = 8.0",
            "EQ_DebugLcdOver2msCount", "EQ_DebugLcdOver5msCount",
            "EQ_DebugLcdBudgetExceededCount",
            "EQ_DebugLcdHardBudgetExceededCount",
            "EQ_DebugLcdUnexpectedFullRedrawCount",
            "EQ_DebugLcdBoundsFailureCount", "EQ_DebugLcdAutoDisabledCount",
            "EQ_DebugLcdExpectedFrameBase", "EQ_DebugLcdCurrentFrameBase",
            "EQ_DebugLcdExpectedFrameEnd", "EQ_DebugLcdCurrentFrameEnd",
            "EQ_DebugLcdRasterFaultCount", "EQ_DebugLcdSyncLostCount",
            "EQ_DebugLcdFifoUnderflowCount",
            "EQ_DebugLcdFrameAddressMismatchCount",
            "EQ_DebugLcdFramebufferCanaryFailureCount",
            "EQ_DebugDeadlineMissCount",
            "EQ_DebugFrameLatencyDeadlineMissCount",
            "EQ_DebugFrameServiceOverlapCount",
            "EQ_DebugFrameServiceDroppedCount",
        ):
            self.assertIn(token, self.source)
        self.assertIn("lcd_category_count: readArray", self.source)
        self.assertIn("lcd_job_type_max_cycles: readArray", self.source)

    def test_keeps_objective_and_operator_visual_evidence_separate(self) -> None:
        self.assertIn('"MEASURED_ON_CURRENT_BOARD_OBJECTIVE_ONLY"', self.source)
        self.assertIn('"PENDING_OPERATOR_VISUAL_OBSERVATION"', self.source)
        self.assertNotIn('screen_circular_shift: "PASS"', self.source)


class EqualizerLcdDynamicRunnerContractTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.source = RUNNER.read_text(encoding="utf-8")

    def test_powershell_syntax_is_valid(self) -> None:
        command = (
            "$tokens=$null; $errors=$null; "
            f"[System.Management.Automation.Language.Parser]::ParseFile('{RUNNER}', "
            "[ref]$tokens, [ref]$errors) | Out-Null; "
            "if ($errors.Count -ne 0) { $errors | ForEach-Object { $_.Message }; exit 1 }"
        )
        result = subprocess.run(
            ["powershell.exe", "-NoProfile", "-Command", command],
            cwd=ROOT,
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            check=False,
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_forces_exact_clean_project33_dynamic_build(self) -> None:
        for token in (
            "Dynamic LCD hardware validation requires a clean worktree",
            "generate_equalizer_build_id.ps1", "-B", "GEN_OPTS__FLAG=$defines",
            "--define=DSP_LAB_PROJECT_SELECT=33",
            "--define=EQ_USE_GENERATED_BUILD_ID=1",
            "--define=EQ_ENABLE_AUDIO_FEATURE_ANALYZER=1",
            "--define=EQ_ENABLE_SMART_BASS=1",
            "--define=EQ_ENABLE_DYNAMIC_CLARITY=1",
            "--define=EQ_ENABLE_HARSHNESS_GUARD=1",
            "--define=EQ_ENABLE_LCD_DISPLAY=1",
            "--define=EQ_ENABLE_PROJECT33_TOUCH=0",
            "--define=EQ_UI_RUNTIME_DEFAULT_MASK=7",
            "--define=EQ_LCD_DIAGNOSTIC_ALIGNMENT_PATTERN=0",
            "<link_errors>0x0</link_errors>",
            "warning_count", "error_count", "build_dirty = 0",
        ):
            self.assertIn(token, self.source)

    def test_uses_temp_output_only_audio_and_no_capture(self) -> None:
        for token in (
            "$env:TEMP", "50000", "music_like_50khz_-18dbfs.wav",
            "SoundPlayer", "default PC line-output", "PlayLooping",
            ".Stop()", "finally",
        ):
            self.assertIn(token, self.source)
        lowered = self.source.lower()
        for forbidden in (
            "microphone", "wavein", "wasapicapture", "recordingdevice",
            "ffmpeg -f dshow", "docs\\eval_outputs",
        ):
            self.assertNotIn(forbidden, lowered)

    def test_result_json_is_authoritative_and_visual_gate_stays_pending(self) -> None:
        for token in (
            "if (-not $result.pass)", "dss_launcher_exit_code",
            "$substantiveDssFailure", "DSS_ERROR", "Exception in thread",
            "OPERATOR_VISUAL_OBSERVATION=PENDING",
            "DSP_STATE=RUNNING_DISCONNECTED",
        ):
            self.assertIn(token, self.source)
        self.assertNotIn('$process.ExitCode -ne 0 -or', self.source)


if __name__ == "__main__":
    unittest.main()
