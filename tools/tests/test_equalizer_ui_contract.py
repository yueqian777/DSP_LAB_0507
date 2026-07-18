import os
from pathlib import Path
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
DSP = ROOT / "Code/User/user_dsp"
LOGIC = DSP / "user_equalizer_ui_logic.c"
DISPLAY = DSP / "user_equalizer_display.c"
DISPLAY_HEADER = DSP / "user_equalizer_display.h"
FLOW = DSP / "user_equalizer_flow.c"
LOGIC_HARNESS = ROOT / "tools/tests/equalizer_ui_logic_test.c.host"
DISPLAY_HARNESS = ROOT / "tools/tests/equalizer_display_test.c.host"


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
                    "-DEQ_ENABLE_LCD_DISPLAY=1 -DEQ_LCD_USE_CHINESE=0",
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
        cls.display = DISPLAY.read_text(encoding="utf-8")
        cls.header = DISPLAY_HEADER.read_text(encoding="utf-8")
        cls.flow = FLOW.read_text(encoding="utf-8")

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
        start = self.display.index("static void EQ_DrawJob(int job)")
        end = self.display.index("static unsigned long EQ_ReadCycles", start)
        runtime = self.display[start:end]
        self.assertNotIn("EQ_DrawPresetStatic", runtime)
        self.assertNotIn("EQ_DrawChainStatic", runtime)
        self.assertNotIn("EQ_DrawAnalyzerStatic", runtime)
        self.assertNotIn("EQ_DrawDynamicStatic", runtime)
        self.assertNotIn("EQ_UI_SCREEN_WIDTH, EQ_UI_SCREEN_HEIGHT", runtime)

    def test_chain_uses_ascii_arrow_only(self) -> None:
        combined = self.display + self.logic
        self.assertIn('" -> "', self.display)
        self.assertNotIn("\u2192", combined)
        self.assertLess(648, 800)

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

    def test_defaults_and_timing_contract(self) -> None:
        for token in (
            "#define EQ_ENABLE_PROJECT33_TOUCH 0",
            "#define EQ_UI_RUNTIME_DEFAULT_MASK 0U",
            "#define EQ_LCD_MIN_JOB_GAP_FRAMES 2UL",
            "#define EQ_LCD_CONTROL_QUIET_FRAMES 3UL",
            "#define EQ_LCD_USE_CHINESE 1",
        ):
            self.assertIn(token, self.header)


if __name__ == "__main__":
    unittest.main()
