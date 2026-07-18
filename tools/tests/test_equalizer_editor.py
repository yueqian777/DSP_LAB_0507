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

    def test_page_build_is_tiled_without_runtime_full_clear(self) -> None:
        start = self.display.index("static void EQ_DrawPageTile(void)")
        end = self.display.index("static unsigned int EQ_DrawJob", start)
        page_tile = self.display[start:end]
        self.assertNotIn("Lcd_Clear", page_tile)
        self.assertNotIn("EQ_UI_SCREEN_HEIGHT", page_tile)
        self.assertIn("EqualizerUiLogic_GetPageTileIndex", page_tile)


if __name__ == "__main__":
    unittest.main()
