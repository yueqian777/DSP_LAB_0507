import pathlib
import re
import shutil
import subprocess
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[2]
RUNNER = ROOT / "tools" / "run_equalizer_ten_band_editor_hardware.ps1"
DSS = ROOT / "tools" / "dss" / "dss_equalizer_ten_band_editor.js"
FLOW = ROOT / "Code" / "User" / "user_dsp" / "user_equalizer_flow.c"
FLOW_HEADER = ROOT / "Code" / "User" / "user_dsp" / "user_equalizer_flow.h"


def powershell_executable():
    executable = shutil.which("powershell.exe") or shutil.which("pwsh.exe")
    if executable is None:
        raise unittest.SkipTest("PowerShell is unavailable")
    return executable


def run_runner(*arguments):
    return subprocess.run(
        [
            powershell_executable(),
            "-NoProfile",
            "-ExecutionPolicy",
            "Bypass",
            "-File",
            str(RUNNER),
            *arguments,
        ],
        cwd=ROOT,
        text=True,
        capture_output=True,
        timeout=20,
        check=False,
    )


class EqualizerEditorHardwareToolingTest(unittest.TestCase):
    def test_tool_files_exist(self):
        self.assertTrue(RUNNER.is_file())
        self.assertTrue(DSS.is_file())

    def test_default_and_dry_run_are_hardware_inert(self):
        invocations = [(), ("-DryRun",), ("-Mode", "DryRun")]
        for arguments in invocations:
            with self.subTest(arguments=arguments):
                result = run_runner(*arguments)
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertEqual(result.stdout.strip(), "NOT_RUN_NO_HARDWARE")
                self.assertNotIn("PASS", result.stdout.upper())

    def test_powershell_syntax_and_guard_order(self):
        source = RUNNER.read_text(encoding="utf-8")
        quoted = str(RUNNER).replace("'", "''")
        command = (
            "$tokens=$null;$errors=$null;"
            f"[System.Management.Automation.Language.Parser]::ParseFile('{quoted}',"
            "[ref]$tokens,[ref]$errors)|Out-Null;"
            "if($errors.Count){$errors|ForEach-Object{$_.Message};exit 1}"
        )
        result = subprocess.run(
            [powershell_executable(), "-NoProfile", "-Command", command],
            cwd=ROOT,
            text=True,
            capture_output=True,
            timeout=20,
            check=False,
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        guard = source.index('if ($DryRun.IsPresent -or $Mode -ne "Hardware")')
        self.assertLess(guard, source.index("Resolve-Path"))
        self.assertLess(guard, source.index("Start-Process"))
        self.assertEqual(
            source.count('Write-Output "NOT_RUN_NO_HARDWARE"'), 1
        )

    def test_hardware_mode_requires_exact_provenance_and_profile(self):
        source = RUNNER.read_text(encoding="utf-8")
        required = [
            "^[0-9a-fA-F]{40}$",
            'branch -ne "main"',
            "rev-parse HEAD",
            "status --porcelain --untracked-files=all",
            "EQ_BUILD_DIRTY 0",
            "<link_errors>0x0</link_errors>",
            "run_equalizer_ui_build_matrix.ps1",
            '-ProfileNames "H_project33_full"',
            'CLEAN_BUILD_PROFILE=H_project33_full',
            "warning_count",
            "error_count",
            "out_sha256",
            "LastWriteTimeUtc",
            'Assert-ExactGitState "pre-load"',
            'Assert-BuildIdentity "pre-load"',
            "C6748_CONNECTED_AND_AUDIO_LOOP_READY",
            "EQ_DebugUiEditorStateBytes",
            "EQ_DebugTouchActionCount",
            "EQ_DebugLcdFramebufferCanaryFailureCount",
            'ValidateRange(10, 30)',
        ]
        for contract in required:
            with self.subTest(contract=contract):
                self.assertIn(contract, source)

    def test_h_profile_contract_is_checked_before_dss(self):
        source = RUNNER.read_text(encoding="utf-8")
        for token in (
            "--define=EQ_ENABLE_LCD_DISPLAY=1",
            "--define=EQ_ENABLE_PROJECT33_TOUCH=1",
            "--define=EQ_ENABLE_TEN_BAND_EDITOR=1",
            "--define=EQ_ENABLE_TEN_BAND_EDITOR_TOUCH=1",
            "--define=EQ_UI_RUNTIME_DEFAULT_MASK=31",
            "job_count -ne 25",
            "dynamic_hitbox_count -ne 12",
            "editor_hitbox_count -ne 15",
            "framebuffer_symbol_count -ne 2",
            "second_framebuffer_symbol_hits -ne 1",
            "offscreen_buffer_object_count -ne 2",
            "offscreen_buffer_bytes -ne $buildResult.framebuffer_bytes",
            "editor_state_bytes -le 0",
        ):
            with self.subTest(token=token):
                self.assertIn(token, source)
        self.assertLess(
            source.index("CLEAN_BUILD_PROFILE=H_project33_full"),
            source.index("Start-Process"),
        )

    def test_h_profile_requires_double_buffer_debug_symbols_in_result(self):
        source = RUNNER.read_text(encoding="utf-8")
        symbols = (
            "EQ_DebugLcdDoubleBufferEnabled",
            "EQ_DebugLcdFrontPage",
            "EQ_DebugLcdSwapTargetPage",
            "EQ_DebugLcdSwapPending",
            "EQ_DebugLcdSwapDescriptorMask",
            "EQ_DebugLcdEofCount",
            "EQ_DebugLcdEofAmbiguousCount",
            "EQ_DebugLcdSwapRequestCount",
            "EQ_DebugLcdSwapCompleteCount",
            "EQ_DebugLcdCurrentFrame1Base",
            "EQ_DebugLcdCurrentFrame1End",
            "EQ_DebugLcdRasterStopTimeoutCount",
        )
        required_start = source.index("$lcdDoubleBufferDebugSymbols = @(")
        required_end = source.index(")\n$requiredSymbols = @(", required_start)
        required_block = source[required_start:required_end]
        for symbol in symbols:
            with self.subTest(symbol=symbol):
                self.assertIn(symbol, required_block)
        self.assertIn(
            ") + $lcdDoubleBufferDebugSymbols", source
        )
        self.assertIn(
            "required_symbol_presence = $requiredSymbolPresence", source
        )
        self.assertIn(
            "lcd_double_buffer_debug_symbols = $lcdDoubleBufferDebugSymbols",
            source,
        )
        self.assertIn(
            "framebuffer_symbols = $buildResult.framebuffer_symbol_names",
            source,
        )

    def test_operator_visual_templates_remain_operator_owned(self):
        source = RUNNER.read_text(encoding="utf-8")
        self.assertIn("operator_visual_checklist.txt", source)
        self.assertIn("operator_visual_summary.json", source)
        self.assertIn('result_label = "PENDING_OPERATOR"', source)
        self.assertIn("automated_counters_are_not_visual_evidence", source)
        item_ids = re.findall(
            r'\bid = "([a-z0-9_]+)"; description =', source
        )
        self.assertEqual(len(item_ids), 15)
        self.assertEqual(len(set(item_ids)), 15)
        for field in (
            "file_name", "captured_at", "size_bytes", "sha256",
            "stage", "user_conclusion",
        ):
            self.assertIn(field, source)

    def test_dss_contract_covers_deferred_board_acceptance(self):
        source = DSS.read_text(encoding="utf-8")
        auth_guard = source.index(
            'hardwareAuthorization != "C6748_CONNECTED_AND_AUDIO_LOOP_READY"'
        )
        self.assertLess(auth_guard, source.index("ScriptingEnvironment.instance()"))
        self.assertIn('System.out.println("NOT_RUN_NO_HARDWARE")', source)
        required = [
            "EQ_DebugBuildDirty",
            "EQ_DebugBuildGitSha",
            "EQ_DebugInitStage",
            "ACTION_EDITOR_BAND_0 + band",
            "ACTION_EDITOR_PLUS",
            "ACTION_EDITOR_MINUS",
            "ACTION_EDITOR_APPLY",
            "ACTION_EDITOR_RESET_FLAT",
            "PRESET_CUSTOM",
            "RESET FLAT applied",
            "stale_latest_wins",
            "PAGE_TO_EDITOR",
            "PAGE_TO_DYNAMIC",
            "dynamic_status",
            "EQ_DebugLcdMaxJobCycles",
            "EQ_DebugFrameLatencyMaxCycles",
            "EQ_DebugLcdRasterFaultCount",
            "EQ_DebugLcdFifoUnderflowCount",
            "EQ_DebugLcdFramebufferCanaryFailureCount",
            "EQ_DebugLcdRasterStopTimeoutCount",
            "EQ_DebugDeadlineMissCount",
            "EQ_DebugFrameServiceOverlapCount",
            "EQ_DebugFrameServiceDroppedCount",
            "EQ_DebugClipCount",
            "FRAME_LATENCY_ACCEPTANCE_CYCLES = 8405000",
            "LCD_NORMAL_JOB_CYCLES = 912000",
            "MAX_ANALYZER_STRIP_HEIGHT = 16",
            "MAX_LCD_JOBS_PER_SECOND = 8",
            "enduranceMinutes * 60 * 1000",
            "enduranceMinutes >= 10",
        ]
        for contract in required:
            with self.subTest(contract=contract):
                self.assertIn(contract, source)

    def test_dss_reads_target_sizes_before_dynamic_arrays(self):
        source = DSS.read_text(encoding="utf-8")
        for symbol in (
            "EQ_DebugLcdCategoryCountSize",
            "EQ_DebugLcdJobTypeCountSize",
            "EQ_DebugUiJobCountSize",
            "EQ_DebugDynamicHitboxCountSize",
            "EQ_DebugEditorHitboxCountSize",
        ):
            self.assertIn(symbol, source)
        self.assertIn("initializeArraySizes();", source)
        self.assertIn("arraySizes.lcd_category_count", source)
        self.assertIn("arraySizes.lcd_job_type_count", source)
        self.assertNotRegex(source, r"index\s*<\s*(?:6|27)\b")
        self.assertNotIn("target_sizeof_fallback", source)
        self.assertNotIn("sizeof(EQ_DebugLcdCategoryCount)", source)
        self.assertIn("missing retained size symbols", RUNNER.read_text(
            encoding="utf-8"))

    def test_dss_uses_exported_editor_debug_mirrors(self):
        dss = DSS.read_text(encoding="utf-8")
        runner = RUNNER.read_text(encoding="utf-8")
        flow = FLOW.read_text(encoding="utf-8")
        header = FLOW_HEADER.read_text(encoding="utf-8")
        symbols = (
            "EQ_DebugUiEditorSelectedBand",
            "EQ_DebugUiEditorDraftDirty",
            "EQ_DebugUiEditorSubmittedValid",
            "EQ_DebugUiEditorApplyStatus",
            "EQ_DebugUiEditorSubmittedSequence",
            "EQ_DebugUiEditorAppliedSequence",
            "EQ_DebugUiEditorDraftGainHalfDb",
            "EQ_DebugUiEditorSubmittedGainHalfDb",
            "EQ_DebugUiEditorAppliedGainHalfDb",
        )
        for symbol in symbols:
            with self.subTest(symbol=symbol):
                self.assertIn(symbol, dss)
                self.assertIn(symbol, runner)
                self.assertIn(symbol, flow)
                self.assertIn(symbol, header)
        self.assertNotIn("EQ_UiEditorState", dss)
        self.assertNotIn('"EQ_UiEditorState"', runner)
        self.assertIn("static void EQ_SyncUiEditorDebug(void)", flow)
        self.assertGreaterEqual(flow.count("EQ_SyncUiEditorDebug();"), 3)

    def test_full_staged_hardware_sequence_is_present(self):
        source = DSS.read_text(encoding="utf-8")
        ordered_calls = (
            "runTouchCalibration();",
            "runPhysicalDynamicSequence();",
            "runPhysicalEditorSequence();",
            "runPhysicalMultiBandCustom();",
            "runPageSwitchStress();",
            "runStaleRequestCheck();",
            "runDynamicStatusCheck();",
            "runCombinedInteractive();",
            "runEndurance().after",
        )
        offsets = [source.index(call) for call in ordered_calls]
        self.assertEqual(offsets, sorted(offsets))
        for token in (
            "physical_dynamic_12_actions",
            "physical_multi_band_custom",
            "20 round trips;5 reversals",
            "combined_interactive_10min",
            "uninterrupted_endurance_10min",
            "MEASURED_ON_CURRENT_BOARD_UNINTERRUPTED_ENDURANCE",
            "transition_ms=",
        ):
            self.assertIn(token, source)

    def test_touch_calibration_waits_for_stable_release(self):
        source = DSS.read_text(encoding="utf-8")
        helper_start = source.index(
            "function waitForStableCalibrationRelease(")
        capture_start = source.index(
            "function captureCalibrationPoint(", helper_start)
        capture_end = source.index(
            "function averagePair(", capture_start)
        helper = source[helper_start:capture_start]
        capture = source[capture_start:capture_end]
        self.assertIn("stableMilliseconds", helper)
        self.assertIn("state.touch_pressed == 0", helper)
        self.assertIn("stable = 0", helper)
        self.assertIn("interactionTimeoutSeconds", helper)
        stable_call = capture.index(
            "waitForStableCalibrationRelease(label, 500)")
        record_write = capture.index("touchCalibration.push(record)")
        self.assertLess(stable_call, record_write)

    def test_dss_snapshot_covers_lcd_audio_analyzer_dynamics_and_touch(self):
        source = DSS.read_text(encoding="utf-8")
        required = (
            "EQ_DebugLcdOver1msCount",
            "EQ_DebugLcdOver2msCount",
            "EQ_DebugLcdOver5msCount",
            "EQ_DebugLcdAnalyzerMaxStripHeight",
            "EQ_DebugLcdAnalyzerStripCount",
            "EQ_DebugLcdDoubleBufferEnabled",
            "EQ_DebugLcdSwapDescriptorMask",
            "EQ_DebugLcdEofAmbiguousCount",
            "EQ_DebugLcdCurrentFrame1Base",
            "EQ_DebugLcdCurrentFrame1End",
            "EQ_DebugLcdCategoryCount[",
            "EQ_DebugLcdCategoryMaxCycles[",
            "EQ_DebugLcdJobTypeCount[",
            "EQ_DebugLcdJobTypeMaxCycles[",
            "EQ_DebugAlgoMaxCycles",
            "EQ_DebugFrameServiceMaxCycles",
            "EQ_DebugFrameLatencyMaxCycles",
            "EQ_DebugAnalyzerEnabled",
            "EQ_DebugAnalyzerRunCount",
            "EQ_DebugAnalyzerAnalysisCount",
            "EQ_DebugAnalyzerDeferredCount",
            "EQ_DebugAnalyzerMaxCycles",
            "EQ_DebugSmartBassDecisionCount",
            "EQ_DebugSmartBassLevelChangeCount",
            "EQ_DebugSmartBassSaturationCount",
            "EQ_DebugSmartBassNonFiniteCount",
            "EQ_DebugDynamicClarityDecisionCount",
            "EQ_DebugHarshnessGuardDecisionCount",
            "EQ_DebugTouchRawX",
            "EQ_DebugTouchRawY",
            "EQ_DebugTouchScreenX",
            "EQ_DebugTouchScreenY",
            "EQ_DebugTouchPressed",
        )
        for token in required:
            with self.subTest(token=token):
                self.assertIn(token, source)
        self.assertNotIn("EQ_DebugLcdAnalyzerValueCount", source)

    def test_touch_calibration_and_automated_operator_boundary(self):
        source = DSS.read_text(encoding="utf-8")
        for label in (
            "LEFT_TOP", "RIGHT_TOP", "LEFT_BOTTOM", "RIGHT_BOTTOM",
            "CENTER",
        ):
            self.assertIn(label, source)
        for token in (
            "raw_x_min_observed", "raw_x_max_observed",
            "raw_y_min_observed", "raw_y_max_observed",
            "swap_xy_derived", "flip_x_derived", "flip_y_derived",
            'result_label: "AUTOMATED_BOARD_VALIDATION_COMPLETE"',
            'operator_visual_result: "PENDING_OPERATOR"',
            "automated_counters_are_not_visual_evidence",
        ):
            self.assertIn(token, source)


if __name__ == "__main__":
    unittest.main()
