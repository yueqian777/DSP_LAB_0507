from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[2]


class SubbandTouchUIContractTest(unittest.TestCase):
    def test_flow_owns_persistent_codec_target(self) -> None:
        header = (ROOT / "Code/User/user_dsp/user_subband_flow.h").read_text(encoding="utf-8")
        source = (ROOT / "Code/User/user_dsp/user_subband_flow.c").read_text(encoding="utf-8")
        self.assertIn("SUBBAND_DebugPersistentCodecKbps", header)
        self.assertIn("Subband_Request_Codec_Target", header)
        self.assertIn("SUBBAND_DebugPersistentCodecKbps", source)
        self.assertIn("SUBBAND_CODEC_LOOP_DebugRequestedTargetKbps", source)

    def test_ui_uses_fixed_regions_and_nonblocking_services(self) -> None:
        header = (ROOT / "Code/User/user_dsp/user_subband_ui.h").read_text(encoding="utf-8")
        source = (ROOT / "Code/User/user_dsp/user_subband_ui.c").read_text(encoding="utf-8")
        for token in (
            "SUBBAND_UI_LCD_WIDTH 800",
            "SUBBAND_UI_LCD_HEIGHT 480",
            "SubbandUI_ServiceTouch",
            "SubbandUI_ServiceDisplay",
            "UI_DIRTY_MODE",
            "UI_DIRTY_STATUS",
            "UI_DIRTY_COUNTDOWN",
            "UI_DIRTY_RATE",
            "UI_DIRTY_LOAD",
            "SUBBAND_UI_DebugMaxDrawCycles",
            "SUBBAND_UI_DebugMaxTouchCycles",
        ):
            self.assertIn(token, header + source)
        self.assertNotIn("malloc(", source)
        self.assertNotIn("free(", source)
        self.assertNotIn("sprintf(", source)
        self.assertNotIn("stdio.h", source)
        self.assertNotIn("delay(", source)
        self.assertNotIn("x +=", source)
        self.assertNotIn("y +=", source)

    def test_flow_services_ui_outside_audio_processing(self) -> None:
        source = (ROOT / "Code/User/user_dsp/user_subband_flow.c").read_text(encoding="utf-8")
        process_start = source.index("void Subband_Process_1024")
        process_end = source.index("#if SUBBAND_USE_LEGACY_FIR", process_start)
        process_body = source[process_start:process_end]
        self.assertNotIn("SubbandUI_ServiceDisplay", process_body)
        self.assertNotIn("SubbandUI_ServiceTouch", process_body)
        self.assertIn("SubbandUI_ServiceTouch", source)
        self.assertIn("SubbandUI_ServiceDisplay", source)

    def test_touch_pinmux_and_font_draw_are_board_safe(self) -> None:
        touch_source = (ROOT / "Code/Driver/12_touch/touch_pin.c").read_text(encoding="utf-8")
        font_source = (ROOT / "Code/User/user_dsp/user_subband_ui_font.c").read_text(encoding="utf-8")
        self.assertIn("SYSCFG0_PINMUX(18)", touch_source)
        self.assertNotIn("SYSCFG0_PINMUX(0)", touch_source)
        self.assertIn("GrLineDrawH", font_source)
        self.assertNotIn("GrPixelDraw", font_source)

    def test_gt1151_scan_has_explicit_states_and_no_null_ack(self) -> None:
        scan_header = (ROOT / "Code/Driver/12_touch/touch_scan.h").read_text(encoding="utf-8")
        scan_source = (ROOT / "Code/Driver/12_touch/touch_scan.c").read_text(encoding="utf-8")
        combined = scan_header + scan_source
        self.assertIn("TouchScanResult", combined)
        for token in (
            "TOUCH_SCAN_NO_DATA",
            "TOUCH_SCAN_DOWN",
            "TOUCH_SCAN_RELEASE",
            "TOUCH_SCAN_ERROR",
            "Touch_DebugReadyCount",
            "Touch_DebugNoDataCount",
            "Touch_DebugDownCount",
            "Touch_DebugReleaseCount",
            "Touch_DebugI2cErrorCount",
            "clear_value",
            "GT1151_WR_Reg(GT_GSTID_REG, &clear_value, 1U)",
        ):
            self.assertIn(token, combined)
        self.assertNotIn("GT1151_WR_Reg(GT_GSTID_REG, 0, 1)", scan_source)
        self.assertNotIn("delay(", scan_source)
        self.assertNotIn("State == 0x81", scan_source)
        self.assertNotIn("State == 0x80", scan_source)

    def test_touch_isr_and_product_id_are_watchable(self) -> None:
        pin_source = (ROOT / "Code/Driver/12_touch/touch_pin.c").read_text(encoding="utf-8")
        drv_source = (ROOT / "Code/Driver/12_touch/touch_drv.c").read_text(encoding="utf-8")
        api_header = (ROOT / "Code/Driver/12_touch/touch_api.h").read_text(encoding="utf-8")
        self.assertIn("Touch_DebugInterruptCount++", pin_source)
        isr_body = pin_source[pin_source.index("static void TouchIsr(void)"):]
        self.assertNotIn("GT1151_RD_Reg", isr_body)
        self.assertNotIn("GT1151_WR_Reg", isr_body)
        self.assertNotIn("printf(", isr_body)
        self.assertIn("Touch_DebugProductId", drv_source + api_header)
        self.assertIn("unsigned char id[5]", drv_source)
        self.assertIn("id[4] = 0U", drv_source)

    def test_subband_touch_uses_low_frequency_poll_and_check_entry(self) -> None:
        flow_source = (ROOT / "Code/User/user_dsp/user_subband_flow.c").read_text(encoding="utf-8")
        ui_header = (ROOT / "Code/User/user_dsp/user_subband_ui.h").read_text(encoding="utf-8")
        main_source = (ROOT / "Code/main.c").read_text(encoding="utf-8")
        touch_check_source = ROOT / "Code/User/user_dsp/user_subband_touch_check.c"
        touch_check_header = ROOT / "Code/User/user_dsp/user_subband_touch_check.h"
        self.assertIn("Tim2_Init(TIMER_20HZ)", flow_source)
        self.assertIn("FLAG_TIM2", flow_source)
        self.assertIn("SubbandUI_ServiceTouch", flow_source)
        self.assertIn("SubbandUI_ServiceTouch(unsigned char force_scan)", ui_header + flow_source)
        self.assertIn("SUBBAND_TOUCH_CHECK_ONLY", main_source)
        self.assertTrue(touch_check_source.exists())
        self.assertTrue(touch_check_header.exists())
        source = touch_check_source.read_text(encoding="utf-8")
        self.assertIn("Subband_Touch_Check_Example", source)
        self.assertIn("Touch Check", source)
        self.assertIn("Tim2_Init(TIMER_20HZ)", source)
        self.assertNotIn("Adc_Init", source)
        self.assertNotIn("Dac_Init", source)

    def test_touch_scan_raw_keeps_widget_compatibility(self) -> None:
        scan_header = (ROOT / "Code/Driver/12_touch/touch_scan.h").read_text(encoding="utf-8")
        api_header = (ROOT / "Code/Driver/12_touch/touch_api.h").read_text(encoding="utf-8")
        scan_source = (ROOT / "Code/Driver/12_touch/touch_scan.c").read_text(encoding="utf-8")

        self.assertIn("TouchScanResult Touch_ScanRaw(void);", scan_header)
        self.assertIn("TouchScanResult Touch_ScanRaw(void);", api_header)
        self.assertIn("TouchScanResult Touch_ScanRaw(void)", scan_source)
        raw_start = scan_source.index("TouchScanResult Touch_ScanRaw(void)")
        scan_start = scan_source.index("TouchScanResult Touch_Scan(void)")
        raw_body = scan_source[raw_start:scan_start]
        self.assertIn("GT1151_Scan()", raw_body)
        self.assertNotIn("Widget_Btn_App", raw_body)
        scan_body = scan_source[scan_start:scan_source.index("TouchScanResult GT1151_Scan(void)")]
        self.assertIn("result = Touch_ScanRaw();", scan_body)
        self.assertIn("Widget_Btn_App(Touch_Sta, Touch_X, Touch_Y);", scan_body)

    def test_subband_page_owns_buttons_without_widget_autodraw(self) -> None:
        source = (ROOT / "Code/User/user_dsp/user_subband_ui.c").read_text(encoding="utf-8")

        for token in (
            "RectangularButton(",
            "UI_ModeButton0",
            "WidgetAdd(",
            "WidgetPaint(",
            "UI_DrawModePhrasesOnly",
            "PushButtonFillColorSet",
            "SubbandUI_OnModeButton",
        ):
            self.assertNotIn(token, source)
        for token in (
            "SubbandUIButtonDef",
            "static int UI_HitModeButton(unsigned int x, unsigned int y)",
            "static int UI_HitRateButton(unsigned int x, unsigned int y)",
            "static void UI_DrawModeButton(unsigned int index, int selected)",
            "static void UI_DrawRateButton(unsigned int index, int selected",
        ):
            self.assertIn(token, source)

    def test_subband_touch_uses_raw_press_release_fsm(self) -> None:
        source = (ROOT / "Code/User/user_dsp/user_subband_ui.c").read_text(encoding="utf-8")
        touch_start = source.index("void SubbandUI_ServiceTouch(unsigned char force_scan)")
        touch_end = source.index("void SubbandUI_ServiceDisplay(void)", touch_start)
        touch_body = source[touch_start:touch_end]

        for token in (
            "SubbandUITouchState",
            "UI_TOUCH_IDLE",
            "UI_TOUCH_PRESSED",
            "UI_PressedButtonType",
            "UI_PressedButtonIndex",
            "TOUCH_SCAN_DOWN",
            "TOUCH_SCAN_RELEASE",
        ):
            self.assertIn(token, source)
        self.assertIn("Touch_ScanRaw()", touch_body)
        self.assertNotIn("Touch_Scan()", touch_body)
        self.assertIn("release_button", touch_body)
        self.assertIn("UI_TOUCH_PRESSED", touch_body)

    def test_ui_refresh_is_incremental_and_budgeted(self) -> None:
        ui_header = (ROOT / "Code/User/user_dsp/user_subband_ui.h").read_text(encoding="utf-8")
        ui_source = (ROOT / "Code/User/user_dsp/user_subband_ui.c").read_text(encoding="utf-8")

        self.assertIn("#define SUBBAND_UI_SHOW_ALGO_LOAD 1", ui_header)
        self.assertIn("#define UI_COUNTDOWN_FRAME_INTERVAL 10UL", ui_source)
        self.assertIn("#define UI_LOAD_FRAME_INTERVAL 50UL", ui_source)
        self.assertIn("#define SUBBAND_UI_RUNTIME_DRAW_BUDGET_CYCLES 912000UL", ui_header + ui_source)
        self.assertIn("UI_DIRTY_PROGRESS", ui_header)
        for token in (
            "SUBBAND_UI_DebugDrawOverBudgetCount",
            "SUBBAND_UI_DebugLastDrawJob",
            "SUBBAND_UI_DebugMaxProgressDrawCycles",
            "SUBBAND_UI_DebugMaxButtonDrawCycles",
            "SUBBAND_UI_DebugMaxTextDrawCycles",
            "UI_DisplayedProgressStep",
            "UI_TargetProgressStep",
            "UI_DrawProgressBlock",
        ):
            self.assertIn(token, ui_header + ui_source)
        update_start = ui_source.index("static void UI_UpdateDirtyState(void)")
        update_end = ui_source.index("#if SUBBAND_UI_SHOW_ALGO_LOAD", update_start)
        learning_block = ui_source[update_start:update_end]
        self.assertIn("UI_DIRTY_PROGRESS", learning_block)
        self.assertNotIn("UI_DIRTY_COUNTDOWN | UI_DIRTY_STATUS", learning_block)
        self.assertNotIn("UI_FillRect(17, 317, 782, 382", ui_source)
        self.assertNotIn("UI_FillRect(24, 390, 775, 409", ui_source)
        self.assertNotIn("UI_FillRect(242, 296, 640, 314", ui_source)
        self.assertIn("UI_FillRect(690, 252, 783, 312", ui_source)

        update_start = ui_source.index("static void UI_UpdateDirtyState(void)")
        update_end = ui_source.index("static void UI_UpdateLoadDirtyState(void)", update_start)
        update_body = ui_source[update_start:update_end]
        self.assertNotIn("UI_ComputeAlgoLoad", update_body)
        self.assertIn("static void UI_UpdateLoadDirtyState(void)", ui_source)
        load_update_start = ui_source.index("static void UI_UpdateLoadDirtyState(void)")
        load_update_end = ui_source.index("static unsigned long UI_DrawNextModeButton", load_update_start)
        load_update_body = ui_source[load_update_start:load_update_end]
        self.assertIn("UI_ComputeAlgoLoad", load_update_body)
        service_start = ui_source.index("void SubbandUI_ServiceDisplay(void)")
        service_body = ui_source[service_start:]
        countdown_block = service_body[
            service_body.index("UI_LastCountdownFrame = frame;"):
            service_body.index("#if SUBBAND_UI_SHOW_ALGO_LOAD")
        ]
        self.assertNotIn("UI_UpdateLoadDirtyState", countdown_block)
        self.assertIn("UI_UpdateLoadDirtyState();", service_body)

    def test_ui_load_uses_rolling_current_cycles_not_historical_max(self) -> None:
        ui_header = (ROOT / "Code/User/user_dsp/user_subband_ui.h").read_text(encoding="utf-8")
        ui_source = (ROOT / "Code/User/user_dsp/user_subband_ui.c").read_text(encoding="utf-8")
        flow_source = (ROOT / "Code/User/user_dsp/user_subband_flow.c").read_text(encoding="utf-8")
        combined = ui_header + ui_source

        for token in (
            "SUBBAND_UI_DebugRollingCycles",
            "SUBBAND_UI_DebugRollingLoadPercent",
            "SUBBAND_UI_DebugLoadSampleCount",
            "void SubbandUI_RecordAlgoCycles(unsigned long cycles);",
            "void SubbandUI_ResetLoadWindow(void);",
            "rolling = (rolling * 7UL + cycles + 4UL) / 8UL;",
        ):
            self.assertIn(token, combined)
        compute_start = ui_source.index("static int UI_ComputeAlgoLoad(void)")
        compute_end = ui_source.index("static void UI_DrawLoad", compute_start)
        compute_body = ui_source[compute_start:compute_end]
        self.assertNotIn("SUBBAND_DebugMaxCycles", compute_body)
        process_start = flow_source.index("void Subband_Process_1024")
        process_end = flow_source.index("#if SUBBAND_USE_LEGACY_FIR", process_start)
        process_body = flow_source[process_start:process_end]
        self.assertIn("SubbandUI_RecordAlgoCycles(cycle_delta);", process_body)
        apply_start = flow_source.index("if (mode != SUBBAND_DebugAppliedDemoMode)")
        apply_body = flow_source[apply_start:flow_source.index("}", apply_start) + 1]
        self.assertIn("SubbandUI_ResetLoadWindow();", apply_body)

    def test_ui_processing_chain_replaces_visible_debug_fields(self) -> None:
        ui_header = (ROOT / "Code/User/user_dsp/user_subband_ui.h").read_text(encoding="utf-8")
        ui_source = (ROOT / "Code/User/user_dsp/user_subband_ui.c").read_text(encoding="utf-8")
        combined = ui_header + ui_source

        for token in (
            "UI_DIRTY_CHAIN",
            "SUBBAND_UI_DebugDisplayedChainMode",
            "SUBBAND_UI_DebugDisplayedChainKbps",
            "SUBBAND_UI_DebugChainRefreshCount",
            "SUBBAND_UI_DebugLastChainDrawCycles",
            "SUBBAND_UI_DebugMaxChainDrawCycles",
            "SubbandUIChainDef",
            "UI_ChainTable",
            "static void UI_UpdateChainDirtyState(void)",
            "static void UI_DrawProcessingChain(void)",
        ):
            self.assertIn(token, combined)

        status_start = ui_source.index("static void UI_DrawStatus(void)")
        status_end = ui_source.index("static void UI_DrawStaticBackground(void)", status_start)
        status_body = ui_source[status_start:status_end]
        self.assertNotIn('"REQ "', status_body)
        self.assertNotIn('"  APPLIED "', status_body)

        rate_start = ui_source.index("static void UI_DrawRateText(void)")
        rate_end = ui_source.index("static int UI_ComputeAlgoLoad(void)", rate_start)
        rate_body = ui_source[rate_start:rate_end]
        self.assertNotIn('"SEL "', rate_body)
        self.assertNotIn('"TGT "', rate_body)
        self.assertNotIn('"EST "', rate_body)
        self.assertNotIn("UI_CurrentEstimatedKbps", rate_body)

    def test_ui_processing_chain_uses_applied_mode_and_target_kbps_only(self) -> None:
        ui_source = (ROOT / "Code/User/user_dsp/user_subband_ui.c").read_text(encoding="utf-8")

        chain_start = ui_source.index("static void UI_DrawProcessingChain(void)")
        chain_end = ui_source.index("static void UI_DrawStaticBackground(void)", chain_start)
        chain_body = ui_source[chain_start:chain_end]
        self.assertIn("applied_mode = SUBBAND_DebugAppliedDemoMode;", chain_body)
        self.assertIn("target_kbps = UI_CurrentChainKbps(applied_mode);", chain_body)
        self.assertNotIn("SUBBAND_DebugDemoMode", chain_body)
        self.assertNotIn("SUBBAND_CODEC_LOOP_DebugEstimatedBitrateKbps", chain_body)

        dirty_start = ui_source.index("static void UI_UpdateChainDirtyState(void)")
        dirty_end = ui_source.index("static void UI_UpdateDirtyState(void)", dirty_start)
        dirty_body = ui_source[dirty_start:dirty_end]
        self.assertIn("SUBBAND_DebugAppliedDemoMode", dirty_body)
        self.assertIn("SUBBAND_CODEC_LOOP_DebugTargetKbps", dirty_body)
        for token in (
            "SUBBAND_DENOISE_DebugLearnHops",
            "SUBBAND_DENOISE_DebugTargetHops",
            "SUBBAND_DENOISE_DebugLearning",
            "SUBBAND_UI_DebugAlgoLoadPercent",
            "SUBBAND_CODEC_LOOP_DebugEstimatedBitrateKbps",
        ):
            self.assertNotIn(token, dirty_body)

    def test_flow_services_ui_without_touch_display_back_to_back(self) -> None:
        flow_source = (ROOT / "Code/User/user_dsp/user_subband_flow.c").read_text(encoding="utf-8")
        self.assertIn("touch_serviced = 0U;", flow_source)
        self.assertIn("touch_serviced = 1U;", flow_source)
        self.assertIn("(touch_serviced == 0U)", flow_source)
        touch_call = flow_source.index("SubbandUI_ServiceTouch(force_touch_scan);")
        post_touch = flow_source[touch_call:flow_source.index("if ((FLAG_AD == 0)", touch_call)]
        self.assertNotIn("Subband_Service_Demo_Mode();", post_touch)

    def test_flow_display_slot_ignores_ad_done_but_preserves_io_priority(self) -> None:
        flow_source = (ROOT / "Code/User/user_dsp/user_subband_flow.c").read_text(encoding="utf-8")
        self.assertIn("unsigned char audio_serviced;", flow_source)
        self.assertIn("audio_serviced = 0U;", flow_source)

        ad_start = flow_source.index("if (FLAG_AD == 1)")
        ad_end = flow_source.index("if (FLAG_DA == 1 && FLAG_AD_DONE == 1)", ad_start)
        self.assertIn("audio_serviced = 1U;", flow_source[ad_start:ad_end])

        da_start = flow_source.index("if (FLAG_DA == 1 && FLAG_AD_DONE == 1)")
        da_end = flow_source.index("SUBBAND_DebugFrameReady = FLAG_AD_DONE;", da_start)
        self.assertIn("audio_serviced = 1U;", flow_source[da_start:da_end])

        display_call = flow_source.index("SubbandUI_ServiceDisplay();")
        display_guard = flow_source[flow_source.rfind("if (", 0, display_call):display_call]

        self.assertIn("(FLAG_AD == 0)", display_guard)
        self.assertIn("(FLAG_DA == 0)", display_guard)
        self.assertIn("(audio_serviced == 0U)", display_guard)
        self.assertIn("(touch_serviced == 0U)", display_guard)
        self.assertNotIn("FLAG_AD_DONE == 0", display_guard)


if __name__ == "__main__":
    unittest.main()
