from __future__ import annotations

import pathlib
import shlex
import subprocess
import tempfile
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[2]
BASH = pathlib.Path(r"C:\msys64\usr\bin\bash.exe")
INCLUDE = ROOT / "Code/User/user_dsp"
FLOW_HEADER = INCLUDE / "user_equalizer_flow.h"
FLOW_SOURCE = INCLUDE / "user_equalizer_flow.c"
GUARD_SOURCE = INCLUDE / "user_harshness_guard.c"
EQUALIZER_SOURCE = INCLUDE / "user_equalizer.c"
BOARD_DSS = ROOT / "tools/dss/dss_harshness_guard_board.js"
BOARD_RUNNER = ROOT / "tools/run_harshness_guard_board.ps1"
RAW_ANALYZER = ROOT / "tools/analyze_harshness_guard_raw.py"
TRANSITION_DSS = ROOT / "tools/dss/dss_harshness_guard_transition.js"
TRANSITION_RUNNER = ROOT / "tools/run_harshness_guard_transition.ps1"
TRANSITION_ANALYZER = ROOT / "tools/analyze_harshness_guard_transition.py"


def msys_path(path: pathlib.Path) -> str:
    resolved = path.resolve()
    return f"/{resolved.drive[0].lower()}/{resolved.as_posix()[3:]}"


def run_bash(command: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [
            str(BASH),
            "-lc",
            "export PATH=/mingw64/bin:/usr/bin:$PATH; "
            f"cd {shlex.quote(msys_path(ROOT))}; {command}",
        ],
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        check=False,
    )


class HarshnessGuardHardwareRunnerTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.flow_header = FLOW_HEADER.read_text(encoding="utf-8")
        cls.flow_source = FLOW_SOURCE.read_text(encoding="utf-8")
        cls.board_dss = BOARD_DSS.read_text(encoding="utf-8")
        cls.board_runner = BOARD_RUNNER.read_text(encoding="utf-8")
        cls.raw_analyzer = RAW_ANALYZER.read_text(encoding="utf-8")
        cls.transition_dss = TRANSITION_DSS.read_text(encoding="utf-8")
        cls.transition_runner = TRANSITION_RUNNER.read_text(encoding="utf-8")
        cls.transition_analyzer = TRANSITION_ANALYZER.read_text(encoding="utf-8")

    def test_transition_diagnostic_api_is_compile_gated_and_adjacent(self) -> None:
        self.assertIn(
            "#define EQ_ENABLE_HARSHNESS_GUARD_TRANSITION_CAPTURE 0",
            self.flow_header,
        )
        self.assertIn(
            "#error Harshness Guard transition capture requires Harshness Guard.",
            self.flow_header,
        )
        with tempfile.TemporaryDirectory(prefix="guard_diag_api_") as temp:
            test_source = pathlib.Path(temp) / "diag_api.c"
            executable = pathlib.Path(temp) / "diag_api.exe"
            test_source.write_text(
                '#include "user_harshness_guard.h"\n'
                "int main(void) { HARSHNESS_GUARD_STATE state;\n"
                "HarshnessGuard_Init(&state);\n"
                "if (HarshnessGuard_DiagnosticForceStableLevel(&state, 1) < 0) return 1;\n"
                "if (state.active_level != 1 || state.transition_active != 0) return 2;\n"
                "if (HarshnessGuard_DiagnosticRequestLevel(&state, 2) < 0) return 3;\n"
                "if (state.transition_active == 0 || state.pending_level != 2) return 4;\n"
                "if (HarshnessGuard_DiagnosticRequestLevel(&state, 4) >= 0) return 5;\n"
                "return 0; }\n",
                encoding="ascii",
            )
            command = " ".join(
                (
                    "gcc -std=c99 -Wall -Wextra -Werror",
                    "-DEQ_ALGO_ONLY",
                    "-DEQ_ENABLE_AUDIO_FEATURE_ANALYZER=1",
                    "-DEQ_ENABLE_HARSHNESS_GUARD=1",
                    "-DEQ_ENABLE_HARSHNESS_GUARD_TRANSITION_CAPTURE=1",
                    f"-I{shlex.quote(msys_path(INCLUDE))}",
                    shlex.quote(msys_path(EQUALIZER_SOURCE)),
                    shlex.quote(msys_path(GUARD_SOURCE)),
                    shlex.quote(msys_path(test_source)),
                    "-lm -o",
                    shlex.quote(msys_path(executable)),
                    "&&",
                    shlex.quote(msys_path(executable)),
                )
            )
            result = run_bash(command)
        self.assertEqual(result.returncode, 0, result.stdout)

    def test_four_way_overlap_is_a_same_frame_latch(self) -> None:
        self.assertIn(
            "#define EQ_ENABLE_FOUR_WAY_TRANSITION_DIAGNOSTICS 0",
            self.flow_header,
        )
        start = self.flow_source.index(
            "static void EQ_UpdateFourWayTransitionDiagnostics(void)"
        )
        end = self.flow_source.index(
            "static void EQ_ServiceHarshnessGuardTransitionCapture", start
        )
        body = self.flow_source[start:end]
        self.assertIn("if (mask == 0x0fU)", body)
        self.assertIn("((mask & 0x0eU) == 0x0eU)", body)
        self.assertIn("EQ_DebugMode = armed_mode;", body)
        self.assertNotIn("|= EQ_DebugFourWayTransitionMask", body)
        self.assertLess(
            self.flow_source.index("HarshnessGuard_ProcessFrame("),
            self.flow_source.index("EQ_UpdateFourWayTransitionDiagnostics();"),
        )

    def test_transition_runner_is_isolated_and_objective_only(self) -> None:
        for token in (
            "EQ_ENABLE_HARSHNESS_GUARD_TRANSITION_CAPTURE=1",
            "EQ_ENABLE_DYNAMIC_CLARITY_TRANSITION_CAPTURE=0",
            "EQ_ENABLE_HARSHNESS_GUARD_BENCHMARK=0",
            "generate_harshness_guard_audio.py",
            "analyze_harshness_guard_transition.py",
            "requires a clean worktree",
            "EQ_USE_GENERATED_BUILD_ID=1",
            "UART capture is empty",
            "[IO.File]::ReadAllText",
            "P33 BUILD $expectedSha",
        ):
            self.assertIn(token, self.transition_runner)
        for token in (
            "EQ_TriggerCaptureSource\") == 16",
            "EQ_DebugHarshnessGuardTransitionCaptureRequest",
            "EQ_DebugSmartBassEnabled = 0",
            "EQ_DebugDynamicClarityEnabled = 0",
            "EQ_DebugAppliedMode\") == 0",
            "EQ_DebugBandGainDb[",
            "Build identity is not the expected clean commit",
            "BOARD_INTERNAL_PCM_OBJECTIVE_TRANSIENT",
            "NOT_PERFORMED",
        ):
            self.assertIn(token, self.transition_dss)
        self.assertIn("hf_boundary_to_internal_ratio", self.transition_analyzer)
        self.assertNotIn("inaudible", self.transition_analyzer.lower())
        self.assertNotIn("no audible click", self.transition_analyzer.lower())

    def test_board_runner_covers_a_to_g_and_uninterrupted_i(self) -> None:
        for token in (
            "EQ_ENABLE_FOUR_WAY_TRANSITION_DIAGNOSTICS=1",
            "EQ_ENABLE_HARSHNESS_GUARD_TRANSITION_CAPTURE=0",
            "EQ_ENABLE_HARSHNESS_GUARD_BENCHMARK=0",
            "generate_harshness_guard_audio.py",
            "analyze_harshness_guard_raw.py",
            "requires a clean worktree",
            "EQ_USE_GENERATED_BUILD_ID=1",
            "UART capture is empty",
            "[IO.File]::ReadAllText",
        ):
            self.assertIn(token, self.board_runner)
        for token in (
            'runFor(60000);',
            'requestAudio("presence_tone")',
            'requestAudio("brightness_tone")',
            '"harshness_sweep_2db"',
            '"harshness_sweep_18db"',
            'requestAudio("silence")',
            'requestAudio("all_dynamic_trigger")',
            'EQ_DebugFourWayTransitionArmMode = 1',
            'EQ_DebugFourWayTransitionArmMode = 2',
            'state.four_way_overlap_count >',
            'access_during_window: "NONE"',
            'Thread.sleep(301000);',
            'measured_dsp_audio_seconds',
            'debugger_access_during_window: "NONE"',
            'dsp_left_running = true',
        ):
            self.assertIn(token, self.board_dss)
        continuous_start = self.board_dss.index("hostStartNs = System.nanoTime();")
        continuous_end = self.board_dss.index(
            "continuousFinal = boardSnapshot();", continuous_start
        )
        continuous = self.board_dss[continuous_start:continuous_end]
        self.assertNotIn("numberValue(", continuous)
        self.assertNotIn("memory.saveRaw", continuous)
        self.assertNotIn("runFor(", continuous)
        release_start = self.board_dss.index(
            "function requireGuardReleaseConfirmations"
        )
        release_end = self.board_dss.index(
            "function expectedGuardLevel", release_start
        )
        release_contract = self.board_dss[release_start:release_end]
        self.assertIn("start.guard_transitions", release_contract)
        self.assertIn("state.guard_transitions", release_contract)
        self.assertIn("2 * releaseSteps", release_contract)
        self.assertNotIn("start.guard_applied", release_contract)
        self.assertNotIn("start.guard_requested", release_contract)

    def test_raw_analyzer_uses_measured_guard_center_delta(self) -> None:
        for token in (
            '"runtime_off_identity"',
            '"presence_identity"',
            '"brightness_off"',
            '"brightness_on"',
            '"guard_center_transfer_db"',
            "-1.7 <= guard_center_delta <= -1.3",
            '"NOT_PERFORMED"',
            '"UNMEASURED"',
        ):
            self.assertIn(token, self.raw_analyzer)

    def test_powershell_and_python_syntax(self) -> None:
        for path in (BOARD_RUNNER, TRANSITION_RUNNER):
            command = (
                "$errors=$null; [void][System.Management.Automation.Language.Parser]::"
                f"ParseFile('{path}',[ref]$null,[ref]$errors); "
                "if($errors.Count -ne 0){$errors | Out-String; exit 1}"
            )
            result = subprocess.run(
                ["powershell", "-NoProfile", "-Command", command],
                text=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                check=False,
            )
            self.assertEqual(result.returncode, 0, result.stdout)
        result = subprocess.run(
            [
                "python",
                "-B",
                "-m",
                "py_compile",
                str(RAW_ANALYZER),
                str(TRANSITION_ANALYZER),
            ],
            cwd=ROOT,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            check=False,
        )
        self.assertEqual(result.returncode, 0, result.stdout)


if __name__ == "__main__":
    unittest.main()
