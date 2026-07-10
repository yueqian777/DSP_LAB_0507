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


if __name__ == "__main__":
    unittest.main()
