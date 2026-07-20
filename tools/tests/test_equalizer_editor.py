from __future__ import annotations

import pathlib
import subprocess
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[2]
BASH = pathlib.Path(r"C:\msys64\usr\bin\bash.exe")


def run_msys(command: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [str(BASH), "-lc", "export PATH=/mingw64/bin:/usr/bin:$PATH; "
         "cd /c/Users/zhangyueqian/lab8/DSP_LAB_0507; " + command],
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        check=False,
    )


class EqualizerEditorTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.ui_header = (ROOT / "Code/User/user_dsp/"
                         "user_equalizer_ui_logic.h").read_text(
                             encoding="utf-8")
        cls.ui_source = (ROOT / "Code/User/user_dsp/"
                         "user_equalizer_ui_logic.c").read_text(
                             encoding="utf-8")
        cls.flow = (ROOT / "Code/User/user_dsp/"
                    "user_equalizer_flow.c").read_text(encoding="utf-8")
        cls.display = (ROOT / "Code/User/user_dsp/"
                       "user_equalizer_display.c").read_text(
                           encoding="utf-8")
        cls.display_header = (ROOT / "Code/User/user_dsp/"
                              "user_equalizer_display.h").read_text(
                                  encoding="utf-8")

    def test_host_editor_control_chain(self) -> None:
        result = run_msys(
            "gcc -std=c99 -Wall -Wextra -Werror -DEQ_ALGO_ONLY "
            "-DEQ_ENABLE_TEN_BAND_EDITOR=1 -ICode/User/user_dsp "
            "Code/User/user_dsp/user_equalizer.c "
            "Code/User/user_dsp/user_equalizer_control.c "
            "Code/User/user_dsp/user_equalizer_ui_logic.c "
            "tools/tests/equalizer_editor_control_test.c -lm "
            "-o /tmp/equalizer_editor_control_test.exe && "
            "/tmp/equalizer_editor_control_test.exe"
        )
        self.assertEqual(result.returncode, 0, result.stdout)
        self.assertIn("failures=0", result.stdout)

    def test_host_editor_ui_logic_c89(self) -> None:
        result = run_msys(
            "gcc -std=c89 -pedantic -Wall -Wextra -Werror -DEQ_ALGO_ONLY "
            "-DEQ_ENABLE_TEN_BAND_EDITOR=1 -ICode/User/user_dsp -x c "
            "tools/tests/equalizer_ui_logic_test.c.host "
            "Code/User/user_dsp/user_equalizer_ui_logic.c "
            "-o /tmp/equalizer_ui_logic_editor_test.exe && "
            "/tmp/equalizer_ui_logic_editor_test.exe"
        )
        self.assertEqual(result.returncode, 0, result.stdout)
        self.assertIn("failures=0", result.stdout)

    def test_host_editor_renderer_c89(self) -> None:
        result = run_msys(
            "gcc -std=c89 -pedantic -Wall -Wextra -Werror -DEQ_ALGO_ONLY "
            "-DEQ_ENABLE_LCD_DISPLAY=1 -DEQ_ENABLE_TEN_BAND_EDITOR=1 "
            "-ICode/User/user_dsp -x c "
            "tools/tests/equalizer_display_test.c.host "
            "Code/User/user_dsp/user_equalizer_ui_logic.c "
            "Code/User/user_dsp/user_equalizer_display.c "
            "-o /tmp/equalizer_display_editor_test.exe && "
            "/tmp/equalizer_display_editor_test.exe"
        )
        self.assertEqual(result.returncode, 0, result.stdout)
        self.assertIn("failures=0", result.stdout)

    def test_default_is_disabled_and_mask_fits(self) -> None:
        self.assertIn("#define EQ_ENABLE_TEN_BAND_EDITOR 0", self.ui_header)
        self.assertIn("#define EQ_UI_JOB_COUNT         27", self.ui_header)
        self.assertIn("#define EQ_UI_CATEGORY_COUNT    6", self.ui_header)
        self.assertIn("(EQ_UI_JOB_COUNT <= 32) ? 1 : -1", self.ui_header)
        self.assertIn("typedef signed char EQ_UI_GAIN_HALF_DB", self.ui_header)

    def test_plus_minus_are_local_only(self) -> None:
        start = self.ui_source.index("int EqualizerUiEditor_StepSelected(")
        end = self.ui_source.index(
            "int EqualizerUiEditor_SetDraftFlat", start)
        step = self.ui_source[start:end]
        self.assertIn("draft_gain_half_db[band]", step)
        self.assertNotIn("EqualizerControl_", step)
        self.assertNotIn("EQ_STATE", step)

    def test_apply_and_reset_use_mailbox(self) -> None:
        start = self.flow.index("static int EQ_SubmitUiEditorRequest(",
                                self.flow.index("static void EQ_FillControlRequest"))
        end = self.flow.index("static int EQ_ServiceMode", start)
        submit = self.flow[start:end]
        self.assertIn("request.command = EQ_CONTROL_SET_ALL", submit)
        self.assertIn("request.preset = EQ_PRESET_CUSTOM", submit)
        self.assertIn("request.command = EQ_CONTROL_RESET_FLAT", submit)
        self.assertEqual(submit.count("EqualizerControl_SubmitRequest("), 1)
        for forbidden in ("EqualizerControl_ServiceOneBuilderSlice(",
                          "Equalizer_SetAllGainsDb(",
                          "active_gain_db", "pending_bank"):
            self.assertNotIn(forbidden, submit)

    def test_page_build_refreshes_persistent_hidden_regions(self) -> None:
        start = self.display.index(
            "static unsigned int EQ_DrawPageTile(void)")
        end = self.display.index("static unsigned int EQ_DrawJob", start)
        page_tile = self.display[start:end]
        self.assertNotIn("Lcd_Clear", page_tile)
        self.assertNotIn("EQ_UI_SCREEN_HEIGHT", page_tile)
        self.assertIn("EqualizerUiLogic_GetPageTileIndex", page_tile)
        self.assertIn("tile == EQ_UI_PAGE_TILE_SWITCH", page_tile)
        self.assertNotIn("EQ_DrawPageSwitch", page_tile)
        self.assertNotIn("EQ_DrawPageTitle", page_tile)
        self.assertNotIn("EQ_DrawPresetStatic", page_tile)
        self.assertIn("EQ_DrawPresetJob", page_tile)
        self.assertIn("EQ_DrawEditorBandFull", page_tile)
        self.assertNotIn("EQ_DrawEditorControlsFull", page_tile)
        self.assertNotIn("EQ_DrawEditorFieldFull", page_tile)
        self.assertIn("EQ_DrawEditorFieldValue", page_tile)
        self.assertIn("EQ_DrawAnalyzerTileFull", page_tile)
        self.assertNotIn("EQ_DrawChainStatic", page_tile)
        self.assertNotIn("EQ_ClearPageTitleStrip", page_tile)
        self.assertNotIn("EQ_ClearPageContentStrip", page_tile)

        editor_start = self.display.index(
            "static void EQ_DrawEditorBandFull(int band)")
        editor_end = self.display.index("#endif", editor_start)
        editor_band = self.display[editor_start:editor_end]
        self.assertNotIn(
            "EQ_LcdFillRect(rect->x, rect->y, rect->w, rect->h",
            editor_band)
        self.assertIn("EQ_UI_EDITOR_BAR_TOP + 1", editor_band)

        analyzer_start = self.display.index(
            "static void EQ_DrawAnalyzerTileFull(int band)")
        analyzer_end = self.display.index(
            "static void EQ_DrawDynamicTileFull", analyzer_start)
        analyzer = self.display[analyzer_start:analyzer_end]
        self.assertNotIn("rect->w, 224", analyzer)
        self.assertIn("EQ_UI_ANALYZER_BAR_TOP + 1", analyzer)

        dynamic_start = analyzer_end
        dynamic_end = self.display.index(
            "static const char *EQ_EditorPresetText", dynamic_start)
        dynamic = self.display[dynamic_start:dynamic_end]
        self.assertNotIn("EQ_UI_DYNAMIC_RECTS[index].w", dynamic)
        self.assertEqual(dynamic.count("EQ_ClearDynamicValue("), 3)
        chain_start = self.display.index("static void EQ_DrawChainJob")
        chain_end = self.display.index(
            "static unsigned int EQ_DrawAnalyzerJob", chain_start)
        self.assertNotIn("EQ_ClearInside", self.display[chain_start:chain_end])
        self.assertIn("#define EQ_UI_EDITOR_VALUE_CLEAR_W 96", self.display)
        self.assertIn("#define EQ_UI_EDITOR_VALUE_CLEAR_H 22", self.display)

    def test_page_build_has_explicit_region_counts(self) -> None:
        self.assertIn("#define EQ_UI_PAGE_TILE_DYNAMIC_COUNT           16U",
                      self.ui_header)
        self.assertIn("#define EQ_UI_PAGE_TILE_EDITOR_COUNT            24U",
                      self.ui_header)
        self.assertIn("EQ_UI_PAGE_TILE_EDITOR_FIELD_LAST       22U",
                      self.ui_header)
        self.assertIn("EQ_UI_PAGE_TILE_DYNAMIC_ROW_LAST        14U",
                      self.ui_header)
        self.assertNotIn("EQ_UI_PAGE_CLEAR_STRIP_HEIGHT", self.display)

    def test_page_tile_forces_lcdc_audit(self) -> None:
        self.assertIn(
            "if (job == EQ_UI_JOB_PAGE_TILE)",
            self.display)
        self.assertIn("force_hardware_audit = 1", self.display)

    def test_page_build_uses_double_buffer_and_eof_swap(self) -> None:
        self.assertIn("EQ_LcdEditorBuffer", self.display)
        self.assertIn("return s_draw_page;", self.display)
        self.assertIn("EQ_LcdEndOfFrameIsr", self.display)
        self.assertIn("RasterDMAFBConfig", self.display)
        self.assertIn("RASTER_DOUBLE_FRAME_BUFFER", self.display)
        self.assertIn("LCD_FRAME_1", self.display)
        self.assertIn("IntEventClear(SYS_INT_LCDC_INT)", self.display)
        self.assertIn(
            "memcpy(EQ_LcdEditorBuffer + EQ_LCD_PALETTE_OFFSET",
            self.display)
        self.assertIn("C674X_MASK_INT14", self.display)
        self.assertIn("EQ_DebugLcdDoubleBufferEnabled",
                      self.display_header)
        self.assertIn("EQ_DebugLcdSwapCompleteCount",
                      self.display_header)
        self.assertNotIn("EQ_PageRasterPauseBegin", self.display)
        self.assertNotIn("EQ_PageRasterPauseEnd", self.display)

    def test_raster_reconfiguration_waits_for_done(self) -> None:
        stop_start = self.display.index(
            "static int EQ_StopRasterForReconfigure(")
        stop_end = self.display.index("static void EQ_PageSwapInit", stop_start)
        stop = self.display[stop_start:stop_end]
        self.assertLess(stop.index("RasterDisable(SOC_LCDC_0_REGS);"),
                        stop.index("LCDC_LCD_STAT"))
        self.assertIn("EQ_LCD_STATUS_DONE_MASK", stop)
        self.assertIn("EQ_DebugLcdRasterStopTimeoutCount++", stop)

        init_start = self.display.index("void EqualizerDisplay_Init(void)")
        init_end = self.display.index(
            "int EqualizerDisplay_DrawStaticLayout(void)", init_start)
        init = self.display[init_start:init_end]
        self.assertLess(
            init.index("EQ_StopRasterForReconfigure("),
            init.index("EQ_PageSwapInit();"))

        force_start = self.display.index("static void EQ_ForceStopPageSwap")
        force_end = self.display.index(
            "static int EQ_ServicePageSwap", force_start)
        force = self.display[force_start:force_end]
        self.assertLess(force.index("EQ_StopRasterForReconfigure("),
                        force.index("EQ_ConfigFrameDescriptorRaw("))
        self.assertIn("if (raster_stopped != 0)", force)
        self.assertIn("EQ_DebugLcdRasterStopTimeoutCount",
                      self.display_header)

    def test_page_swap_state_is_published_inside_int14_guards(self) -> None:
        service_start = self.display.index(
            "static int EQ_ServicePageSwap(int page)")
        service_end = self.display.index(
            "static void EQ_ApplyDeferredPageRequest", service_start)
        service = self.display[service_start:service_end]
        self.assertLess(service.index("EQ_SwapStateLock();"),
                        service.index("s_swap_descriptor_mask = 0U;"))
        self.assertLess(service.index("s_swap_descriptor_mask = 0U;"),
                        service.index("s_swap_pending = 1U;"))
        self.assertLess(service.index("s_swap_pending = 1U;"),
                        service.index("EQ_SwapStateUnlock();"))

        request_start = self.display.index(
            "void EqualizerDisplay_RequestSnapshot(")
        request_end = self.display.index(
            "int EqualizerDisplay_GetDisplayedPage", request_start)
        request = self.display[request_start:request_end]
        self.assertLess(request.index("EQ_SwapStateLock();"),
                        request.index("EQ_CompleteAcknowledgedPageSwap("))
        self.assertLess(request.index("EQ_CancelPageSwap();"),
                        request.index("EQ_SwapStateUnlock();"))

        cancel_start = self.display.index(
            "void EqualizerDisplay_CancelRuntimeJobs(void)")
        cancel_end = self.display.index(
            "void EqualizerDisplay_AutoDisable", cancel_start)
        cancel = self.display[cancel_start:cancel_end]
        self.assertLess(cancel.index("EQ_SwapStateLock();"),
                        cancel.index("s_swap_descriptor_mask != 0U"))

        audit_start = self.display.index(
            "void EqualizerDisplay_AuditHardware(")
        audit_end = self.display.index(
            "static int EQ_ClampInt", audit_start)
        audit = self.display[audit_start:audit_end]
        self.assertLess(audit.index("EQ_SwapStateLock();"),
                        audit.index("EQ_ReadHardwareSnapshot(&snapshot);"))
        self.assertLess(audit.index("frame_mismatch ="),
                        audit.index("EQ_SwapStateUnlock();"))
        self.assertIn("EQ_ForceStopPageSwap();", self.display)

    def test_snapshot_request_is_limited_to_one_per_frame(self) -> None:
        start = self.flow.index(
            "static void EQ_RequestUiSnapshotIfChanged(")
        end = self.flow.index("#endif", start)
        service = self.flow[start:end]
        guard = service.index(
            "EQ_UiSnapshotLastRequestFrame == process_frame")
        request = service.index("EqualizerDisplay_RequestSnapshot(")
        record = service.index(
            "EQ_UiSnapshotLastRequestFrame = process_frame")
        self.assertLess(guard, request)
        self.assertLess(request, record)
        self.assertIn("EQ_DebugUiSnapshotSkippedCount++", service)


if __name__ == "__main__":
    unittest.main()
