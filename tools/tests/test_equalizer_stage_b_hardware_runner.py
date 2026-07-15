from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[2]
WRAPPER = ROOT / "tools/run_equalizer_33_stage_b_hardware.ps1"
DSS = ROOT / "tools/dss/dss_equalizer_33_stage_b_hardware.js"
FLOW = ROOT / "Code/User/user_dsp/user_equalizer_flow.c"
HEADER = ROOT / "Code/User/user_dsp/user_equalizer_flow.h"


class EqualizerStageBHardwareRunnerTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.wrapper = WRAPPER.read_text(encoding="utf-8")
        cls.dss = DSS.read_text(encoding="utf-8")
        cls.flow = FLOW.read_text(encoding="utf-8")
        cls.header = HEADER.read_text(encoding="utf-8")

    def _function(self, name: str, next_name: str) -> str:
        start = self.dss.index(f"function {name}(")
        end = self.dss.index(f"function {next_name}(", start)
        return self.dss[start:end]

    def test_wrapper_defaults_to_bounded_crossfade_stage(self) -> None:
        self.assertIn(
            '[ValidateSet("CrossfadeA", "LcdStatus", "Full")]',
            self.wrapper,
        )
        self.assertIn('[string]$Stage = "CrossfadeA"', self.wrapper)
        self.assertIn('$env:DSP_TEST_STAGE = $Stage', self.wrapper)

    def test_crossfade_stage_has_only_three_flat_bass_flat_cycles(self) -> None:
        body = self._function("runCrossfadeA", "runLcdStatusOnly")
        self.assertIn("cycle <= 3", body)
        self.assertEqual(body.count("runMeasuredWindow("), 3)
        self.assertIn('write("EQ_DebugLcdRuntimeMask = 0")', body)
        self.assertIn('requestAudio("tone")', body)
        for forbidden in (
            "runCustomBuilder", "runLatestWins", "runLcdSharedBudget",
            "runSwitching", "H_stability_5min", 'requestAudio("music")',
        ):
            self.assertNotIn(forbidden, body)

    def test_dispatch_requires_explicit_lcd_or_full_stage(self) -> None:
        dispatch = self.dss[self.dss.index(
            'if (testStage == "CrossfadeA")'
        ):]
        self.assertLess(
            dispatch.index("runCrossfadeA()"),
            dispatch.index('testStage == "LcdStatus"'),
        )
        self.assertIn("runLcdStatusOnly()", dispatch)
        self.assertIn("runFullValidation()", dispatch)

    def test_all_common_windows_enforce_algorithm_budget(self) -> None:
        body = self._function("checkCommon", "clearDiagnosticPeaks")
        self.assertIn(
            "after.algo_max_cycles < FRAME_BUDGET_CYCLES", body
        )
        self.assertIn(
            "after.frame_service_max_cycles < FRAME_BUDGET_CYCLES", body
        )
        self.assertIn(
            "after.frame_latency_max_cycles < FRAME_BUDGET_CYCLES", body
        )

    def test_measured_windows_clear_only_diagnostic_peaks(self) -> None:
        body = self._function("clearDiagnosticPeaks", "runFor")
        self.assertEqual(body.count("MaxCycles = 0"), 3)
        for forbidden in (
            "EQ_BoardState", "EQ_ControlMailbox", "Frames = 0",
            "MissCount = 0", "Generation = 0", "Token = 0",
        ):
            self.assertNotIn(forbidden, body)

    def test_initialization_snapshot_is_bounded_and_records_hardware_state(self) -> None:
        body = self._function("runInitializationDiagnostic", "runCrossfadeA")
        self.assertIn("runFor(2000)", body)
        self.assertIn('writeResult("target_initialization"', body)
        self.assertIn("after.init_stage == 11", body)
        for token in (
            'init_stage: "EQ_DebugInitStage"',
            'init_flag_ad_done: "EQ_DebugFlagAdDone"',
            'optionalNumberValue("FLAG_AD")',
            'optionalNumberValue("FLAG_DA")',
            'optionalNumberValue("IER")',
            "0x01C01020", "0x01C01050",
        ):
            self.assertIn(token, self.dss)

    def test_lcd_off_snapshot_does_not_evaluate_lcd_symbols(self) -> None:
        body = self._function("snapshot", "delta")
        self.assertIn(
            'key.indexOf("lcd_") == 0 && testStage == "CrossfadeA"', body
        )
        self.assertIn("null : numberValue(symbols[key])", body)
        self.assertIn(
            'testStage == "CrossfadeA" ? null :\n'
            '            numberValue("EQ_DebugLcdCategoryCount["', body
        )

    def test_initialization_accepts_frames_before_steady_state_checks(self) -> None:
        body = self._function("runInitializationDiagnostic", "runCrossfadeA")
        self.assertIn("after.init_stage == 11", body)
        self.assertIn('delta(after, before, "ad_frames") > 0', body)
        self.assertNotIn("checkCommon", body)

    def test_flow_exports_monotonic_first_frame_milestones(self) -> None:
        self.assertIn(
            "extern volatile unsigned long EQ_DebugInitStage;", self.header
        )
        self.assertIn(
            "extern volatile unsigned int EQ_DebugFlagAdDone;", self.header
        )
        for value in range(1, 12):
            self.assertIn(f"EQ_DebugInitStage = {value}UL;", self.flow)
        self.assertLess(
            self.flow.index("EQ_DebugInitStage = 6UL;"),
            self.flow.index("EQ_DebugInitStage = 7UL;"),
        )
        self.assertLess(
            self.flow.index("EQ_DebugInitStage = 8UL;"),
            self.flow.index("while (1)"),
        )

    def test_each_result_contains_explicit_window_metrics(self) -> None:
        for token in (
            "algo_max_cycles", "frame_service_max_cycles",
            "frame_latency_max_cycles", "deadline_delta",
            "latency_deadline_delta", "overlap_delta", "dropped_delta",
            "clip_delta", "ad_delta", "da_delta", "process_delta",
        ):
            self.assertIn(token, self.dss)


if __name__ == "__main__":
    unittest.main()
