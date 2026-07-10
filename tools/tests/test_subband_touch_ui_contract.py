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

    def test_ui_refresh_is_throttled_and_touch_redraws_mode_text(self) -> None:
        ui_header = (ROOT / "Code/User/user_dsp/user_subband_ui.h").read_text(encoding="utf-8")
        ui_source = (ROOT / "Code/User/user_dsp/user_subband_ui.c").read_text(encoding="utf-8")
        flow_source = (ROOT / "Code/User/user_dsp/user_subband_flow.c").read_text(encoding="utf-8")

        self.assertIn("#define SUBBAND_UI_SHOW_ALGO_LOAD 1", ui_header)
        self.assertIn("#define UI_COUNTDOWN_FRAME_INTERVAL 25UL", ui_source)
        self.assertIn("#define UI_LOAD_FRAME_INTERVAL 50UL", ui_source)
        self.assertIn("#define UI_MIN_DRAW_FRAME_INTERVAL 10UL", ui_source)
        self.assertIn("UI_LastDrawFrame", ui_source)
        self.assertIn("UI_LastDrawFrame = frame", ui_source)
        self.assertIn("UI_DrawModePhrasesOnly", ui_source)
        self.assertIn("UI_DrawModePhrasesOnly();", ui_source)

        self.assertIn("touch_serviced = 0U;", flow_source)
        self.assertIn("touch_serviced = 1U;", flow_source)
        self.assertIn("(touch_serviced == 0U)", flow_source)
        touch_call = flow_source.index("SubbandUI_ServiceTouch(force_touch_scan);")
        post_touch = flow_source[touch_call:flow_source.index("if ((FLAG_AD == 0)", touch_call)]
        self.assertNotIn("Subband_Service_Demo_Mode();", post_touch)


if __name__ == "__main__":
    unittest.main()
