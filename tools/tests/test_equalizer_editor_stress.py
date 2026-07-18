from __future__ import annotations

import pathlib
import re
import subprocess
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[2]
BASH = pathlib.Path(r"C:\msys64\usr\bin\bash.exe")
HARNESS = ROOT / "tools/tests/equalizer_editor_stress_test.c"


def run_msys(command: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [
            str(BASH),
            "-lc",
            "export PATH=/mingw64/bin:/usr/bin:$PATH; "
            "cd /c/Users/zhangyueqian/lab8/DSP_LAB_0507; " + command,
        ],
        text=True,
        encoding="utf-8",
        errors="replace",
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        check=False,
    )


class EqualizerEditorStressTest(unittest.TestCase):
    def test_deterministic_stress_and_scheduler(self) -> None:
        command = (
            "gcc -std=c99 -Wall -Wextra -Werror "
            "-DEQ_ALGO_ONLY -DEQ_ENABLE_LCD_DISPLAY=1 "
            "-DEQ_ENABLE_PROJECT33_TOUCH=1 "
            "-DEQ_ENABLE_TEN_BAND_EDITOR=1 "
            "-DEQ_ENABLE_TEN_BAND_EDITOR_TOUCH=1 "
            "-DEQ_ENABLE_AUDIO_FEATURE_ANALYZER=1 "
            "-ICode/User/user_dsp "
            "Code/User/user_dsp/user_equalizer.c "
            "Code/User/user_dsp/user_equalizer_control.c "
            "Code/User/user_dsp/user_equalizer_response.c "
            "Code/User/user_dsp/user_equalizer_ui_logic.c "
            "Code/User/user_dsp/user_equalizer_display.c "
            "Code/User/user_dsp/user_equalizer_flow.c "
            "tools/tests/equalizer_editor_stress_test.c -lm "
            "-o /tmp/equalizer_editor_stress_test.exe && "
            "/tmp/equalizer_editor_stress_test.exe"
        )
        result = run_msys(command)
        self.assertEqual(result.returncode, 0, result.stdout)
        repeated = run_msys("/tmp/equalizer_editor_stress_test.exe")
        self.assertEqual(repeated.returncode, 0, repeated.stdout)
        self.assertEqual(repeated.stdout, result.stdout)
        summary = re.search(
            r"seed=(0x[0-9A-F]{8}) actions=(\d+) stale=(\d+) "
            r"stale_install=(\d+) cancel=(\d+) applied=(\d+) builder=(\d+) "
            r"analyzer=(\d+) lcd=(\d+) wrap=(\d+) "
            r"nonfinite=(\d+) failures=(\d+)",
            result.stdout,
        )
        self.assertIsNotNone(summary, result.stdout)
        assert summary is not None
        values = [int(value) for value in summary.groups()[1:]]
        actions, stale, stale_install, cancel, applied, builder = values[:6]
        analyzer, lcd, wrap, nonfinite, failures = values[6:]
        self.assertEqual(summary.group(1), "0x33E71801")
        self.assertGreaterEqual(actions, 10_000)
        for value in (stale, cancel, applied, builder, analyzer, lcd):
            self.assertGreater(value, 0, result.stdout)
        self.assertEqual(stale_install, 0)
        self.assertEqual(wrap, 1)
        self.assertEqual(nonfinite, 0)
        self.assertEqual(failures, 0)
        print(result.stdout.strip())

    def test_harness_uses_real_project_services(self) -> None:
        source = HARNESS.read_text(encoding="utf-8")
        for call in (
            "EqualizerControl_SubmitRequest(",
            "EqualizerControl_ServiceMailbox(",
            "EqualizerControl_ServiceOneBuilderSlice(",
            "EqualizerControl_TryInstallReady(",
            "EqualizerUiEditor_StepSelected(",
            "EqualizerUiTouch_Process(",
            "EqualizerDisplay_RequestSnapshot(",
            "EqualizerDisplay_ServiceOneJob(",
            "EqualizerBackgroundService_Decide(",
            "EqualizerBackgroundService_Record(",
            "EqualizerAnalyzer_CanService(",
        ):
            self.assertIn(call, source)
        self.assertIn("#define STRESS_ACTIONS 12000UL", source)
        self.assertNotIn("system(", source)
        self.assertNotIn("Dac_Start", source)
        self.assertNotIn("Adc_Start", source)


if __name__ == "__main__":
    unittest.main()
