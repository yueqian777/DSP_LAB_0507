import os
from pathlib import Path
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
DSP = ROOT / "Code/User/user_dsp"
LOGIC = DSP / "user_equalizer_ui_logic.c"
LOGIC_HEADER = DSP / "user_equalizer_ui_logic.h"
DISPLAY = DSP / "user_equalizer_display.c"
DISPLAY_HEADER = DSP / "user_equalizer_display.h"
FLOW = DSP / "user_equalizer_flow.c"
FLOW_HEADER = DSP / "user_equalizer_flow.h"
LOGIC_HARNESS = ROOT / "tools/tests/equalizer_ui_logic_test.c.host"
DISPLAY_HARNESS = ROOT / "tools/tests/equalizer_display_test.c.host"
ALIGNMENT_HARNESS = ROOT / "tools/tests/equalizer_lcd_alignment_test.c.host"
ALIGNMENT_DSS = ROOT / "tools/dss/dss_equalizer_lcd_alignment.js"
ALIGNMENT_RUNNER = ROOT / "tools/run_equalizer_lcd_alignment_hardware.ps1"
BUILD_MATRIX = ROOT / "tools/run_equalizer_ui_build_matrix.ps1"


def msys_path(path: Path) -> str:
    value = path.resolve().as_posix()
    return f"/{value[0].lower()}{value[2:]}"


class EqualizerUiHostTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.bash = Path(os.environ.get(
            "MSYS2_BASH", r"C:\msys64\usr\bin\bash.exe"))
        if not cls.bash.exists():
            raise unittest.SkipTest(f"MSYS2 bash not found: {cls.bash}")

    def run_msys(self, command: str) -> subprocess.CompletedProcess[str]:
        return subprocess.run(
            [str(self.bash), "-lc",
             "export PATH=/mingw64/bin:/usr/bin:$PATH; " + command],
            cwd=ROOT,
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            check=False,
        )

    def test_logic_and_both_text_renderers_are_c89_clean(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)
            common = (
                "gcc -std=c89 -pedantic -Wall -Wextra -Werror "
                "-DEQ_ALGO_ONLY "
                f"-I{msys_path(DSP)} -x c "
            )
            cases = (
                (
                    "logic",
                    f"{msys_path(LOGIC_HARNESS)} {msys_path(LOGIC)}",
                    "",
                    "equalizer_ui_logic failures=0",
                ),
                (
                    "display_cn",
                    f"{msys_path(DISPLAY_HARNESS)} {msys_path(LOGIC)} "
                    f"{msys_path(DISPLAY)}",
                    "-DEQ_ENABLE_LCD_DISPLAY=1",
                    "equalizer_display failures=0",
                ),
                (
                    "display_en",
                    f"{msys_path(DISPLAY_HARNESS)} {msys_path(LOGIC)} "
                    f"{msys_path(DISPLAY)}",
                    "-DEQ_ENABLE_LCD_DISPLAY=1 -DEQ_LCD_USE_CHINESE=0 "
                    "-Wno-unused-function",
                    "equalizer_display failures=0",
                ),
                (
                    "display_alignment",
                    f"{msys_path(ALIGNMENT_HARNESS)} {msys_path(LOGIC)} "
                    f"{msys_path(DISPLAY)}",
                    "-DEQ_ENABLE_LCD_DISPLAY=1 "
                    "-DEQ_LCD_DIAGNOSTIC_ALIGNMENT_PATTERN=1",
                    "equalizer_lcd_alignment failures=0",
                ),
                (
                    "display_timing",
                    f"{msys_path(DISPLAY_HARNESS)} {msys_path(LOGIC)} "
                    f"{msys_path(DISPLAY)}",
                    "-DEQ_ENABLE_LCD_DISPLAY=1 "
                    "-DEQ_ENABLE_TEN_BAND_EDITOR=1 "
                    "-DEQ_ENABLE_LCD_JOB_TIMING_CAPTURE=1",
                    "equalizer_display failures=0",
                ),
            )
            for name, sources, defines, expected in cases:
                exe = msys_path(output / f"{name}.exe")
                result = self.run_msys(
                    f"{common} {defines} {sources} -o {exe} && {exe}")
                self.assertEqual(
                    result.returncode, 0,
                    f"{name} failed\nstdout:\n{result.stdout}"
                    f"\nstderr:\n{result.stderr}")
                self.assertIn(expected, result.stdout)

    def test_lcd_off_has_no_runtime_ui_diagnostics(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            obj = Path(directory) / "display_off.o"
            result = self.run_msys(
                "gcc -std=c89 -pedantic -Wall -Wextra -Werror "
                "-DEQ_FRAME_LEN=1024 "
                f"-I{msys_path(DSP)} -c {msys_path(DISPLAY)} "
                f"-o {msys_path(obj)} && nm {msys_path(obj)}")
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertNotIn("EQ_DebugLcd", result.stdout)
            self.assertNotIn("EQ_DebugTouch", result.stdout)
            self.assertNotIn("s_ui_state", result.stdout)

    def test_explicit_project32_has_no_ui_logic_symbols(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            obj = Path(directory) / "logic_p32.o"
            result = self.run_msys(
                "gcc -std=c89 -pedantic -Wall -Wextra -Werror "
                "-DEQ_FRAME_LEN=1024 -DDSP_LAB_PROJECT_SELECT=32 "
                f"-I{msys_path(DSP)} -c {msys_path(LOGIC)} "
                f"-o {msys_path(obj)} && nm {msys_path(obj)}")
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertNotIn("EqualizerUi", result.stdout)

    def test_touch_build_is_rejected_when_lcd_is_off(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            obj = Path(directory) / "invalid.o"
            result = self.run_msys(
                "gcc -std=c89 -pedantic -Wall -Wextra -Werror "
                "-DEQ_FRAME_LEN=1024 -DEQ_ENABLE_PROJECT33_TOUCH=1 "
                f"-I{msys_path(DSP)} -c {msys_path(DISPLAY)} "
                f"-o {msys_path(obj)}")
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("Touch requires", result.stderr)


class EqualizerUiSourceContractTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.logic = LOGIC.read_text(encoding="utf-8")
        cls.logic_header = LOGIC_HEADER.read_text(encoding="utf-8")
        cls.display = DISPLAY.read_text(encoding="utf-8")
        cls.header = DISPLAY_HEADER.read_text(encoding="utf-8")
        cls.flow = FLOW.read_text(encoding="utf-8")
        cls.flow_header = FLOW_HEADER.read_text(encoding="utf-8")

    def test_final_acceptance_diagnostics_are_bounded_and_noninvasive(
            self) -> None:
        touch_counters = (
            "EQ_DebugTouchInvalidCoordinateCount",
            "EQ_DebugTouchPresetRequestCount",
            "EQ_DebugTouchDynamicEnableRequestCount",
            "EQ_DebugTouchDynamicStrengthRequestCount",
            "EQ_DebugTouchEditorActionCount",
            "EQ_DebugTouchDuplicateActionCount",
        )
        for counter in touch_counters:
            self.assertIn(f"extern volatile unsigned long {counter};",
                          self.flow_header)
            self.assertIn(f"volatile unsigned long {counter} = 0UL;",
                          self.flow)
            self.assertIn(f"{counter} = 0UL;", self.flow)

        touch_start = self.flow.index("static int EQ_ServiceUiTouch(")
        touch_end = self.flow.index("static void EQ_FillControlRequest(",
                                    touch_start)
        touch = self.flow[touch_start:touch_end]
        self.assertIn("EQ_RecordAcceptedTouchAction(action);", touch)
        self.assertIn("EQ_TouchAcceptedThisPress = 0U;", touch)
        self.assertIn("EQ_DebugTouchInvalidCoordinateCount++", touch)
        self.assertNotIn("EQ_UI_DYNAMIC_HITBOXES", touch)
        self.assertNotIn("EQ_UI_EDITOR_HITBOXES", touch)
        self.assertIn("#define EQ_TOUCH_ACTION_DIAGNOSTIC_COUNT",
                      self.flow_header)
        for token in (
            "EQ_DebugTouchActionHistogramSize",
            "EQ_DebugTouchActionHistogram[EQ_TOUCH_ACTION_DIAGNOSTIC_COUNT]",
        ):
            self.assertIn(token, self.flow_header)
            self.assertIn(token.split("[")[0], self.flow)
        self.assertIn("EQ_DebugTouchActionHistogram[action]++;", touch)

        for token in (
            "#define EQ_ENABLE_LCD_JOB_TIMING_CAPTURE 0",
            "#define EQ_LCD_TIMING_CLASS_COUNT 9U",
            "#define EQ_LCD_TIMING_SAMPLE_CAPACITY 256U",
            "EQ_DebugLcdPendingDirtyRegionCount",
            "EQ_DebugLcdMaxPendingDirtyRegionCount",
            "EQ_DebugLcdTimingCaptureCompiled",
        ):
            self.assertIn(token, self.header)
        timing_arrays = (
            "EQ_DebugLcdTimingSamples",
            "EQ_DebugLcdTimingTotalCount",
            "EQ_DebugLcdTimingSampleCount",
            "EQ_DebugLcdTimingDroppedCount",
            "EQ_DebugLcdTimingMinCycles",
            "EQ_DebugLcdTimingMaxCycles",
            "EQ_DebugLcdTimingOver2msCount",
            "EQ_DebugLcdTimingOver5msCount",
            "EQ_DebugLcdTimingDeferredByAudioCount",
            "EQ_DebugLcdTimingAudioArrivedDuringDrawCount",
        )
        for array in timing_arrays:
            self.assertIn(array, self.header)
            self.assertIn(array, self.display)
        self.assertIn("#if EQ_ENABLE_LCD_JOB_TIMING_CAPTURE != 0",
                      self.header)
        self.assertIn("EQ_RecordLcdTimingSample", self.display)
        self.assertIn("EQ_LcdTimingClassForJob", self.display)
        self.assertIn("EqualizerDisplay_RecordDeferredByAudio(", self.display)
        self.assertIn("EqualizerDisplay_RecordAudioArrivalDuringDraw(",
                      self.display)

    def test_request_is_pure_and_service_is_single_job(self) -> None:
        start = self.display.index("void EqualizerDisplay_RequestSnapshot(")
        end = self.display.index("int EqualizerDisplay_HasPendingJob", start)
        request = self.display[start:end]
        for forbidden in (
            "EQ_Lcd", "GrContext", "GrRect", "GrLine", "GrString",
            "EQ_Draw", "Lcd_",
        ):
            self.assertNotIn(forbidden, request)
        loop = self.flow[self.flow.index("while (1)"):]
        self.assertEqual(loop.count("EqualizerDisplay_ServiceOneJob("), 1)

    def test_runtime_renderer_has_no_full_layout_path(self) -> None:
        start = self.display.index("static unsigned int EQ_DrawJob(int job)")
        end = self.display.index("static unsigned long EQ_ReadCycles", start)
        runtime = self.display[start:end]
        self.assertNotIn("EQ_DrawPresetStatic", runtime)
        self.assertNotIn("EQ_DrawChainStatic", runtime)
        self.assertNotIn("EQ_DrawAnalyzerStatic", runtime)
        self.assertNotIn("EQ_DrawDynamicStatic", runtime)
        self.assertNotIn("EQ_UI_SCREEN_WIDTH, EQ_UI_SCREEN_HEIGHT", runtime)

    def test_analyzer_renderer_is_one_bounded_differential_field(self) -> None:
        start = self.display.index(
            "static unsigned int EQ_DrawAnalyzerJob(int band)")
        end = self.display.index("static int EQ_DynamicEnabled", start)
        analyzer = self.display[start:end]
        for token in (
            "EqualizerUiLogic_AnalyzerNextField",
            "EqualizerUiLogic_AnalyzerNextPixel",
            "EQ_DebugLcdAnalyzerLastStripHeight",
            "EQ_LCD_ANALYZER_STRIP_CLEAR",
            "EQ_LCD_ANALYZER_STRIP_POSITIVE_FILL",
            "EQ_LCD_ANALYZER_STRIP_NEGATIVE_FILL",
        ):
            self.assertIn(token, analyzer)
        self.assertNotIn(
            "bar_rect.x, bar_rect.y, bar_rect.w, bar_rect.h",
            analyzer,
        )
        self.assertIn("EQ_UI_ANALYZER_MAX_STRIP_HEIGHT 16",
                      self.logic_header)
        self.assertIn("EQ_UI_ANALYZER_MAX_AGE_FRAMES 50UL",
                      self.logic_header)

    def test_dynamic_runtime_clear_is_bounded_to_value_footprint(self) -> None:
        start = self.display.index("static unsigned int EQ_DrawDynamicJob(")
        end = self.display.index("#if EQ_ENABLE_TEN_BAND_EDITOR", start)
        dynamic = self.display[start:end]
        self.assertEqual(dynamic.count("EQ_ClearDynamicValue("), 3)
        self.assertNotIn("EQ_ClearInside(", dynamic)
        self.assertIn("#define EQ_UI_DYNAMIC_VALUE_CLEAR_W 44",
                      self.display)
        self.assertIn("#define EQ_UI_DYNAMIC_VALUE_CLEAR_H 20",
                      self.display)

    def test_ui_snapshot_separates_user_enabled_from_activity(self) -> None:
        start = self.flow.index("static void EQ_BuildUiSnapshot(")
        end = self.flow.index("#endif", start)
        snapshot = self.flow[start:end]
        for token in (
            "EQ_DebugSmartBassEnabled",
            "EQ_DebugDynamicClarityEnabled",
            "EQ_DebugHarshnessGuardEnabled",
            "EQ_DebugSmartBassProcessingActive",
            "EQ_DebugDynamicClarityProcessingActive",
            "EQ_DebugHarshnessGuardProcessingActive",
            "snapshot->smart_active",
            "snapshot->clarity_active",
            "snapshot->guard_active",
        ):
            self.assertIn(token, snapshot)
        self.assertIn("EqualizerDisplay_HasEligibleJob(", self.flow)

    def test_renderer_uses_bounded_local_rectangles(self) -> None:
        self.assertNotIn("Lcd_Rectangle", self.display)
        start = self.display.index("static void EQ_LcdFillRect(")
        end = self.display.index("\n}\n", start)
        fill = self.display[start:end]
        self.assertIn("if (EQ_CheckRect(x, y, w, h) == 0)", fill)
        self.assertIn("EQ_LcdFillRect16(", fill)
        self.assertNotIn("GrRectFill", fill)

        start = self.display.index("static void EQ_LcdFillRect16(")
        end = self.display.index("\n}\n#endif", start)
        packed = self.display[start:end]
        self.assertIn("EQ_LCD_PALETTE_OFFSET", packed)
        self.assertIn("EQ_LCD_PALETTE_SIZE", packed)
        self.assertIn("(unsigned int)pixel_value << 16", packed)
        self.assertIn("(y + row) * EQ_UI_SCREEN_WIDTH + x", packed)

        start = self.display.index("static void EQ_LcdFillRectStartup(")
        end = self.display.index("\n}\n", start)
        startup_fill = self.display[start:end]
        self.assertIn("GrRectFill", startup_fill)
        static_start = self.display.index(
            "int EqualizerDisplay_DrawStaticLayout(void)")
        static_end = self.display.index(
            "void EqualizerDisplay_BeginRuntime(void)", static_start)
        static_layout = self.display[static_start:static_end]
        self.assertIn("EQ_LcdFillRectStartup(", static_layout)
        self.assertNotIn(
            "EQ_LcdFillRect(0, 0, EQ_UI_SCREEN_WIDTH, EQ_UI_SCREEN_HEIGHT",
            static_layout,
        )

        start = self.display.index("static void EQ_LcdDrawRect(")
        end = self.display.index("\n}\n", start)
        outline = self.display[start:end]
        self.assertIn("if (EQ_CheckRect(x, y, w, h) == 0)", outline)
        self.assertIn("tRectangle rect;", outline)
        self.assertIn("rect.sXMin = x;", outline)
        self.assertIn("rect.sYMin = y;", outline)
        self.assertIn("rect.sXMax = x + w - 1;", outline)
        self.assertIn("rect.sYMax = y + h - 1;", outline)

    def test_lcd_hardware_audit_is_read_only_and_ui_fail_closed(self) -> None:
        start = self.display.index(
            "static void EQ_ReadHardwareSnapshot(")
        end = self.display.index("static int EQ_ClampInt", start)
        audit = self.display[start:end]
        for required in (
            "LCDC_LCDDMA_FB0_BASE",
            "LCDC_LCDDMA_FB0_CEILING",
            "LCDC_RASTER_CTRL",
            "LCDC_LCD_STAT",
            "LCDC_LCDDMA_CTRL",
            "EQ_LCD_STATUS_SYNC_MASK",
            "EQ_LCD_STATUS_FIFO_UNDERFLOW_MASK",
            "EqualizerDisplay_AutoDisable(reason)",
        ):
            self.assertIn(required, audit)
        for forbidden in (
            "LCDC_IRQSTATUS",
            "Lcd_Buffer +\n                                      EQ_LCD_BUFFER_TOTAL_BYTES",
            "Lcd_Init(", "RasterEnable(", "RasterDisable(",
            "RasterDMAFBConfig(", "Adc_Stop(", "Dac_Stop(",
        ):
            self.assertNotIn(forbidden, audit)
        clear_start = self.display.index(
            "static void EQ_ClearStartupFaultStatus(")
        clear_end = self.display.index(
            "static void EQ_CaptureFramebufferCanary", clear_start)
        startup_clear = self.display[clear_start:clear_end]
        self.assertIn("RasterClearGetIntStatus(", startup_clear)
        self.assertIn("EQ_LCD_CLEARABLE_FAULT_MASK", startup_clear)
        self.assertNotIn("RasterEnable(", startup_clear)
        self.assertNotIn("RasterDisable(", startup_clear)
        self.assertEqual(self.display.count("Lcd_Init();"), 1)
        init_start = self.display.index("void EqualizerDisplay_Init(void)")
        init_end = self.display.index(
            "int EqualizerDisplay_DrawStaticLayout(void)", init_start)
        display_init = self.display[init_start:init_end]
        self.assertIn("Lcd_Init();", display_init)
        self.assertIn("RasterDisable(SOC_LCDC_0_REGS);", display_init)
        draw_start = init_end
        draw_end = self.display.index(
            "void EqualizerDisplay_BeginRuntime(void)", draw_start)
        static_draw = self.display[draw_start:draw_end]
        self.assertIn("RasterEnable(SOC_LCDC_0_REGS);", static_draw)
        self.assertLess(
            static_draw.index("RasterEnable(SOC_LCDC_0_REGS);"),
            static_draw.index("EQ_ClearStartupFaultStatus();"),
        )
        loop = self.flow[self.flow.index("while (1)"):]
        self.assertNotIn("EqualizerDisplay_Init(", loop)

    def test_removed_chain_and_analyzer_value_paths_do_not_return(self) -> None:
        combined = self.display + self.header + self.logic + self.logic_header
        for forbidden in (
            "EQ_DrawChain", "EQ_UI_CHAIN", "EQ_UI_RUNTIME_CHAIN",
            "band_value_db", "EQ_UI_ANALYZER_FIELD_VALUE",
            "EQ_DebugLcdAnalyzerValueCount", "EQ_FormatSignedDb",
            "EQ_UI_DYNAMIC_FIELD_LEVEL", "smart_level",
            "clarity_level", "guard_level",
        ):
            self.assertNotIn(forbidden, combined)
        self.assertIn("EQ_UI_ANALYZER_FIELD_BAR", self.logic_header)
        self.assertIn("EQ_UI_DYNAMIC_FIELD_ACTIVE", self.logic_header)

    def test_cache_debug_and_fixed_swap_ring_contract(self) -> None:
        for token in (
            "EQ_DebugLcdCacheMode",
            "EQ_DebugLcdWritebackCount",
            "EQ_DebugLcdWritebackBytes",
            "EQ_DebugLcdWritebackFailureCount",
            "#define EQ_LCD_SWAP_TRACE_DEPTH   64U",
            "EQ_DebugLcdSwapTrace[EQ_LCD_SWAP_TRACE_DEPTH]",
            "EQ_DebugLcdSwapTraceWriteIndex",
            "EQ_DebugLcdSwapTraceCount",
            "EQ_DebugLcdSwapTraceWrapCount",
        ):
            self.assertIn(token, self.header)
        detect_start = self.display.index(
            "static void EQ_DetectFramebufferCacheMode(void)")
        detect_end = self.display.index(
            "static void EQ_ResetDirtyBounds", detect_start)
        detect = self.display[detect_start:detect_end]
        self.assertIn("DSPCACHE_MAR(", detect)
        self.assertIn("DSPCACHE_MAR_PC", detect)

        flush_start = self.display.index(
            "static void EQ_WritebackRange(")
        flush_end = self.display.index("static int EQ_BeginDraw", flush_start)
        flush = self.display[flush_start:flush_end]
        self.assertIn("EQ_LCD_CACHE_CACHEABLE", flush)
        self.assertIn("CacheWB(", flush)
        self.assertIn("EQ_DebugLcdWritebackBytes += bytes", flush)
        self.assertIn("s_dirty_x0", flush)
        self.assertIn("s_dirty_y1", flush)

        trace_start = self.display.index("static void EQ_RecordSwapTrace(")
        trace_end = self.display.index(
            "static void EQ_ClearStartupFaultStatus", trace_start)
        trace = self.display[trace_start:trace_end]
        self.assertIn("EQ_DebugLcdSwapTraceWriteIndex", trace)
        self.assertIn("EQ_LCD_SWAP_TRACE_DEPTH", trace)
        self.assertNotIn("printf(", trace)
        self.assertNotIn("malloc(", trace)

    def test_touch_rejects_every_page_transition_phase(self) -> None:
        start = self.logic.index("int EqualizerUiTouch_Process(")
        end = self.logic.index("int EqualizerUi_ActionToPreset", start)
        touch = self.logic[start:end]
        self.assertIn("if (page_building != 0)", touch)
        reject = touch[touch.index("if (page_building != 0)"):]
        self.assertIn("action = EQ_UI_ACTION_NONE", reject)
        self.assertNotIn("EQ_UI_ACTION_PAGE_SWITCH)", reject)

    def test_touch_mapping_and_actions_are_centralized(self) -> None:
        self.assertEqual(self.logic.count(
            "int EqualizerUi_MapTouchRawToScreen("), 1)
        touch_start = self.flow.index("static int EQ_ServiceUiTouch(")
        touch_end = self.flow.index("static void EQ_FillControlRequest(",
                                    touch_start)
        touch = self.flow[touch_start:touch_end]
        self.assertIn("Touch_ScanRaw()", touch)
        self.assertNotIn("Touch_Scan()", touch)
        self.assertIn("EqualizerUi_MapTouchRawToScreen(", touch)
        self.assertIn("EqualizerUiTouch_Process(", touch)

    def test_touch_writes_requests_not_filter_state(self) -> None:
        start = self.flow.index("static int EQ_ApplyUiAction(int action)")
        end = self.flow.index("static int EQ_ServiceUiTouch(", start)
        action = self.flow[start:end]
        for required in (
            "EQ_DebugMode = preset",
            "EQ_DebugSmartBassEnabled",
            "EQ_DebugSmartBassStrength",
            "EQ_DebugDynamicClarityEnabled",
            "EQ_DebugDynamicClarityStrength",
            "EQ_DebugHarshnessGuardEnabled",
            "EQ_DebugHarshnessGuardStrength",
        ):
            self.assertIn(required, action)
        for forbidden in (
            "active_bank", "pending_bank", "active_level",
            "pending_level", "transition_remaining", "request_sequence",
        ):
            self.assertNotIn(forbidden, action)

    def test_alignment_tooling_preserves_objective_operator_boundary(self) -> None:
        dss = ALIGNMENT_DSS.read_text(encoding="utf-8")
        runner = ALIGNMENT_RUNNER.read_text(encoding="utf-8")
        for token in (
            "EQ_DebugLcdAlignmentPatternEnabled",
            "EQ_DebugLcdStartupRasterStatus",
            "EQ_DebugLcdExpectedFrameBase",
            "EQ_DebugLcdFramebufferCanaryFailureCount",
            '"PENDING_OPERATOR_VISUAL_OBSERVATION"',
            '"RUNNING_DISCONNECTED"',
        ):
            self.assertIn(token, dss)
        self.assertNotIn("EQ_DebugTouchActionCount", dss)
        for token in (
            "Alignment hardware validation requires a clean worktree",
            "--define=DSP_LAB_PROJECT_SELECT=33",
            "--define=EQ_LCD_DIAGNOSTIC_ALIGNMENT_PATTERN=1",
            "--define=EQ_ENABLE_PROJECT33_TOUCH=0",
            "GEN_OPTS__FLAG=$defines",
            "<link_errors>0x0</link_errors>",
            "OPERATOR_VISUAL_OBSERVATION=PENDING",
        ):
            self.assertIn(token, runner)
        self.assertNotIn("SoundPlayer", runner)
        self.assertIn('if (-not $result.pass)', runner)
        self.assertIn('dss_launcher_exit_code', runner)
        self.assertNotIn('$process.ExitCode -ne 0 -or', runner)

    def test_build_matrix_names_match_actual_lcd_profiles(self) -> None:
        matrix = BUILD_MATRIX.read_text(encoding="utf-8")
        static_start = matrix.index('name = "C_project33_static"')
        dynamic_start = matrix.index('name = "D_project33_dynamic"')
        touch_start = matrix.index('name = "E_project33_touch"')
        self.assertIn(
            '--define=EQ_LCD_DIAGNOSTIC_ALIGNMENT_PATTERN=1',
            matrix[static_start:dynamic_start],
        )
        self.assertIn(
            '--define=EQ_LCD_DIAGNOSTIC_ALIGNMENT_PATTERN=0',
            matrix[dynamic_start:touch_start],
        )

    def test_defaults_and_timing_contract(self) -> None:
        for token in (
            "#define EQ_ENABLE_PROJECT33_TOUCH 0",
            "#define EQ_UI_RUNTIME_DEFAULT_MASK 0U",
            "#define EQ_LCD_MIN_JOB_GAP_FRAMES 2UL",
            "#define EQ_LCD_CONTROL_QUIET_FRAMES 3UL",
            "#define EQ_LCD_USE_CHINESE 1",
        ):
            self.assertIn(token, self.header)
        for token in (
            "#define EQ_UI_PRESET_MIN_GAP_FRAMES  2UL",
            "#define EQ_UI_DYNAMIC_MIN_GAP_FRAMES 4UL",
            "#define EQ_UI_ANALYZER_MIN_GAP_FRAMES 8UL",
            "#define EQ_UI_STEADY_MIN_GAP_FRAMES  7UL",
        ):
            self.assertIn(token, self.logic_header)


if __name__ == "__main__":
    unittest.main()
