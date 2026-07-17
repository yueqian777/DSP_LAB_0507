from __future__ import annotations

import csv
import math
import pathlib
import re
import shlex
import subprocess
import tempfile
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[2]
BASH = pathlib.Path(r"C:\msys64\usr\bin\bash.exe")
INCLUDE = ROOT / "Code/User/user_dsp"
EQUALIZER = INCLUDE / "user_equalizer.c"
SMART_BASS = INCLUDE / "user_smart_bass.c"
SMART_HEADER = INCLUDE / "user_smart_bass.h"
FLOW = INCLUDE / "user_equalizer_flow.c"
FLOW_HEADER = INCLUDE / "user_equalizer_flow.h"
HARNESS = ROOT / "tools/tests/smart_bass_test.c"
EVALUATOR = ROOT / "tools/smart_bass_response_eval.c"


def msys_path(path: pathlib.Path) -> str:
    resolved = path.resolve()
    drive = resolved.drive[:1].lower()
    if not drive:
        raise AssertionError(f"expected absolute Windows path: {resolved}")
    return f"/{drive}/{resolved.as_posix()[3:]}"


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


class SmartBassTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.smart_source = SMART_BASS.read_text(encoding="utf-8")
        cls.smart_header = SMART_HEADER.read_text(encoding="utf-8")
        cls.flow_source = FLOW.read_text(encoding="utf-8")
        cls.flow_header = FLOW_HEADER.read_text(encoding="utf-8")

    def test_host_harness(self) -> None:
        self.assertTrue(BASH.is_file(), f"missing MSYS2 bash: {BASH}")
        with tempfile.TemporaryDirectory(prefix="smart_bass_host_") as temp:
            executable = pathlib.Path(temp) / "smart_bass_test.exe"
            command = " ".join(
                (
                    "gcc -std=c99 -Wall -Wextra -Werror",
                    "-DEQ_ALGO_ONLY",
                    "-DEQ_ENABLE_AUDIO_FEATURE_ANALYZER=1",
                    "-DEQ_ENABLE_SMART_BASS=1",
                    f"-I{shlex.quote(msys_path(INCLUDE))}",
                    shlex.quote(msys_path(EQUALIZER)),
                    shlex.quote(msys_path(SMART_BASS)),
                    shlex.quote(msys_path(HARNESS)),
                    "-lm -o",
                    shlex.quote(msys_path(executable)),
                    "&&",
                    shlex.quote(msys_path(executable)),
                )
            )
            result = run_bash(command)

        self.assertEqual(result.returncode, 0, result.stdout)
        self.assertIn("smart_bass: PASS", result.stdout)
        self.assertRegex(result.stdout, r"smart_bass failures=0(?:\s|$)")
        self.assertNotIn("FAIL:", result.stdout)

        def metric(name: str) -> float:
            match = re.search(
                rf"(?:^|\s){re.escape(name)}=(-?\d+(?:\.\d+)?)",
                result.stdout,
            )
            self.assertIsNotNone(match, result.stdout)
            assert match is not None
            return float(match.group(1))

        self.assertEqual(metric("transition_samples"), 4000.0)
        self.assertEqual(metric("identity_exact"), 1.0)
        self.assertEqual(metric("eq_off_chain_exact"), 1.0)
        self.assertEqual(metric("attack_step_max"), 1.0)
        self.assertEqual(metric("release_confirmations"), 2.0)
        self.assertEqual(metric("decision_duplicate_ignored"), 1.0)
        self.assertEqual(metric("release_causes_to_zero"), 3.0)
        self.assertEqual(metric("strength_ceiling_converged"), 1.0)
        self.assertEqual(metric("strength_queued_clamped"), 1.0)
        self.assertEqual(metric("sequence_epoch_reset"), 1.0)
        self.assertEqual(metric("queued_release_preserved"), 1.0)
        self.assertEqual(metric("latest_wins"), 1.0)
        self.assertEqual(metric("queued_applied"), 1.0)
        self.assertEqual(metric("transition_progress_monotonic"), 1.0)
        self.assertEqual(metric("transition_saturation"), 0.0)
        self.assertEqual(metric("impulse_09fs_saturation"), 0.0)
        self.assertAlmostEqual(metric("level6_50hz_db"), -3.0, delta=0.25)
        self.assertLess(metric("high_8khz_abs_max_db"), 0.01)
        self.assertLess(metric("response_positive_max_db"), 0.002)
        self.assertEqual(metric("deterministic"), 1.0)
        self.assertEqual(metric("input_immutable"), 1.0)

    def test_compile_gate_off_object_and_build_state(self) -> None:
        with tempfile.TemporaryDirectory(prefix="smart_bass_gate_") as temp:
            temp_path = pathlib.Path(temp)
            rejected = temp_path / "rejected.o"
            reject_result = run_bash(
                " ".join(
                    (
                        "gcc -std=c99 -Wall -Wextra -Werror -c",
                        "-DEQ_ALGO_ONLY",
                        "-DEQ_ENABLE_AUDIO_FEATURE_ANALYZER=0",
                        "-DEQ_ENABLE_SMART_BASS=1",
                        f"-I{shlex.quote(msys_path(INCLUDE))}",
                        shlex.quote(msys_path(SMART_BASS)),
                        "-o",
                        shlex.quote(msys_path(rejected)),
                    )
                )
            )
            self.assertNotEqual(reject_result.returncode, 0)
            self.assertIn(
                "Smart Bass requires the audio feature analyzer.",
                reject_result.stdout,
            )

            off_object = temp_path / "smart_bass_off.o"
            off_result = run_bash(
                " ".join(
                    (
                        "gcc -std=c99 -Wall -Wextra -Werror -c",
                        "-DEQ_ALGO_ONLY",
                        "-DEQ_ENABLE_AUDIO_FEATURE_ANALYZER=0",
                        "-DEQ_ENABLE_SMART_BASS=0",
                        f"-I{shlex.quote(msys_path(INCLUDE))}",
                        shlex.quote(msys_path(SMART_BASS)),
                        "-o",
                        shlex.quote(msys_path(off_object)),
                        "&& nm",
                        shlex.quote(msys_path(off_object)),
                    )
                )
            )
            self.assertEqual(off_result.returncode, 0, off_result.stdout)
            self.assertNotIn("SmartBass_", off_result.stdout)

            c89_object = temp_path / "smart_bass_c89.o"
            c89_result = run_bash(
                " ".join(
                    (
                        "gcc -std=c89 -pedantic -Wall -Wextra -Werror -c",
                        "-DEQ_ALGO_ONLY",
                        "-DEQ_ENABLE_AUDIO_FEATURE_ANALYZER=1",
                        "-DEQ_ENABLE_SMART_BASS=1",
                        f"-I{shlex.quote(msys_path(INCLUDE))}",
                        shlex.quote(msys_path(SMART_BASS)),
                        "-o",
                        shlex.quote(msys_path(c89_object)),
                    )
                )
            )
            self.assertEqual(c89_result.returncode, 0, c89_result.stdout)

            harness = temp_path / "compiled_state.c"
            harness.write_text(
                '#include "user_equalizer_flow.h"\n'
                "#ifndef EXPECTED\n#define EXPECTED 0\n#endif\n"
                "int main(void) { return "
                "(EQ_DebugSmartBassCompiled == EXPECTED) ? 0 : 1; }\n",
                encoding="ascii",
            )
            for enabled in (0, 1):
                executable = temp_path / f"compiled_state_{enabled}.exe"
                command = " ".join(
                    (
                        "gcc -std=c99 -Wall -Wextra -Werror",
                        "-DEQ_ALGO_ONLY",
                        "-DEQ_ENABLE_AUDIO_FEATURE_ANALYZER=1",
                        f"-DEQ_ENABLE_SMART_BASS={enabled}",
                        f"-DEXPECTED={enabled}",
                        f"-I{shlex.quote(msys_path(INCLUDE))}",
                        shlex.quote(msys_path(FLOW)),
                        shlex.quote(msys_path(harness)),
                        "-o",
                        shlex.quote(msys_path(executable)),
                        "&&",
                        shlex.quote(msys_path(executable)),
                    )
                )
                result = run_bash(command)
                self.assertEqual(result.returncode, 0, result.stdout)

    def test_module_and_flow_contracts(self) -> None:
        self.assertIn("#define EQ_ENABLE_SMART_BASS 0", self.smart_header)
        self.assertIn("#define EQ_ENABLE_SMART_BASS 0", self.flow_header)
        self.assertIn(
            "#error Smart Bass requires the audio feature analyzer.",
            self.smart_header,
        )
        self.assertIn(
            "volatile const unsigned int EQ_DebugSmartBassCompiled =\n"
            "    EQ_ENABLE_SMART_BASS;",
            self.flow_source,
        )
        self.assertIn(
            "extern volatile const unsigned int EQ_DebugSmartBassCompiled;",
            self.flow_header,
        )

        forbidden = (
            "EQ_ControlMailbox",
            "band_gain_db",
            "Builder",
            "ADC",
            "DAC",
            "UART",
            "LCD",
            "Touch",
            "printf(",
            "fopen(",
        )
        for token in forbidden:
            self.assertNotIn(token, self.smart_source)
        for math_call in ("sinf(", "cosf(", "powf(", "sqrtf("):
            self.assertNotIn(math_call, self.smart_source)
        self.assertEqual(
            self.smart_source.count("Equalizer_DesignRbjLowShelfAt("), 1
        )
        init_start = self.smart_source.index("void SmartBass_Init(")
        init_end = self.smart_source.index("void SmartBass_Reset(", init_start)
        self.assertIn(
            "Equalizer_DesignRbjLowShelfAt(",
            self.smart_source[init_start:init_end],
        )

        state_line = "static SMART_BASS_STATE EQ_SmartBassState;"
        state_index = self.flow_source.index(state_line)
        guard_index = self.flow_source.rfind(
            "#if EQ_ENABLE_SMART_BASS != 0", 0, state_index
        )
        end_index = self.flow_source.index("#endif", state_index)
        self.assertGreater(guard_index, 0)
        self.assertGreater(end_index, state_index)

        capture_start = self.flow_source.index(
            "static void EQ_CaptureAdcFrame(void)"
        )
        capture_end = self.flow_source.index(
            "static void EQ_FillDacPingBuffer(void)", capture_start
        )
        capture = self.flow_source[capture_start:capture_end]
        observe = capture.index("EQ_ObserveAnalyzerInputFrame();")
        equalizer = capture.index("Equalizer_ProcessFrame(")
        smart_bass = capture.index("SmartBass_ProcessFrame(")
        capture_output = capture.index("EqualizerCapture_OnFrame(")
        self.assertLess(observe, equalizer)
        self.assertLess(equalizer, smart_bass)
        self.assertLess(smart_bass, capture_output)
        self.assertIn(
            "&EQ_SmartBassState, EQ_DA_Buffer1, EQ_DA_Buffer1,",
            capture,
        )

        runtime_control = self.flow_source.index(
            "static void EQ_ServiceSmartBassRuntimeControl(void)"
        )
        runtime_control_end = self.flow_source.index(
            "static void EQ_UpdateSmartBassTiming", runtime_control
        )
        runtime_body = self.flow_source[runtime_control:runtime_control_end]
        self.assertIn("EQ_AnalyzerLastEnabled != 0U", runtime_body)
        self.assertIn("EQ_SmartBassAnalyzerFault == 0U", runtime_body)
        unavailable_start = self.flow_source.index(
            "static void EQ_MarkSmartBassAnalyzerUnavailable(void)"
        )
        unavailable_end = self.flow_source.index(
            "static void EQ_UpdateSmartBassTiming", unavailable_start
        )
        unavailable_body = self.flow_source[unavailable_start:unavailable_end]
        self.assertLess(
            unavailable_body.index("SmartBass_InvalidateAnalysisEpoch("),
            unavailable_body.index("SmartBass_SetEnabled("),
        )

        analyzer_publish = self.flow_source.index(
            "static void EQ_PublishAnalyzerSnapshot(void)"
        )
        analyzer_service = self.flow_source.index(
            "static int EQ_ServiceAnalyzer(void)", analyzer_publish
        )
        publish_body = self.flow_source[analyzer_publish:analyzer_service]
        self.assertLess(
            publish_body.index("EQ_SmartBassAnalyzerFault = 0U;"),
            publish_body.index("SmartBass_UpdateFromFeature("),
        )
        self.assertIn("SmartBass_UpdateFromFeature(", publish_body)
        self.assertNotIn("EqualizerControl_", publish_body)
        service_end = self.flow_source.index(
            "#endif /* EQ_ENABLE_AUDIO_FEATURE_ANALYZER */", analyzer_service
        )
        service_body = self.flow_source[analyzer_service:service_end]
        self.assertIn("EQ_MarkSmartBassAnalyzerUnavailable();", service_body)

    def test_response_and_control_evaluator(self) -> None:
        with tempfile.TemporaryDirectory(prefix="smart_bass_eval_") as temp:
            temp_path = pathlib.Path(temp)
            executable = temp_path / "smart_bass_response_eval.exe"
            response_csv = temp_path / "smart_bass_response.csv"
            control_csv = temp_path / "smart_bass_control.csv"
            command = " ".join(
                (
                    "gcc -std=c99 -Wall -Wextra -Werror",
                    "-DEQ_ALGO_ONLY",
                    "-DEQ_ENABLE_AUDIO_FEATURE_ANALYZER=1",
                    "-DEQ_ENABLE_SMART_BASS=1",
                    f"-I{shlex.quote(msys_path(INCLUDE))}",
                    shlex.quote(msys_path(EQUALIZER)),
                    shlex.quote(msys_path(SMART_BASS)),
                    shlex.quote(msys_path(EVALUATOR)),
                    "-lm -o",
                    shlex.quote(msys_path(executable)),
                    "&&",
                    shlex.quote(msys_path(executable)),
                    shlex.quote(msys_path(response_csv)),
                    shlex.quote(msys_path(control_csv)),
                )
            )
            result = run_bash(command)
            self.assertEqual(result.returncode, 0, result.stdout)
            self.assertIn("label=HOST_SIMULATED_CONTROL", result.stdout)

            with response_csv.open(newline="", encoding="ascii") as handle:
                response_rows = list(csv.DictReader(handle))
            with control_csv.open(newline="", encoding="ascii") as handle:
                control_rows = list(csv.DictReader(handle))

        self.assertEqual(len(response_rows), 7 * 129)
        self.assertEqual(
            set(response_rows[0]),
            {
                "evidence_label",
                "frequency_hz",
                "level",
                "gain_db",
                "phase_deg",
            },
        )
        self.assertEqual(
            {row["evidence_label"] for row in response_rows},
            {"HOST_SIMULATED_CONTROL"},
        )
        parsed_response = [
            (
                float(row["frequency_hz"]),
                int(row["level"]),
                float(row["gain_db"]),
                float(row["phase_deg"]),
            )
            for row in response_rows
        ]
        self.assertTrue(
            all(math.isfinite(value) for row in parsed_response for value in row)
        )
        self.assertEqual({row[1] for row in parsed_response}, set(range(7)))
        for level in range(7):
            level_rows = [row for row in parsed_response if row[1] == level]
            self.assertEqual(len(level_rows), 129)
            frequencies = [row[0] for row in level_rows]
            self.assertTrue(
                all(after > before for before, after in zip(frequencies, frequencies[1:]))
            )
        level_zero = [row for row in parsed_response if row[1] == 0]
        self.assertLess(max(abs(row[2]) for row in level_zero), 1.0e-5)
        self.assertLess(max(abs(row[3]) for row in level_zero), 1.0e-5)
        for reference_hz in (50.0, 125.0):
            gains = []
            for level in range(7):
                nearest = min(
                    (row for row in parsed_response if row[1] == level),
                    key=lambda row: abs(row[0] - reference_hz),
                )
                gains.append(nearest[2])
            self.assertTrue(
                all(after < before for before, after in zip(gains, gains[1:])),
                gains,
            )
        self.assertLess(max(row[2] for row in parsed_response), 0.002)
        level6_near_50 = min(
            (row for row in parsed_response if row[1] == 6),
            key=lambda row: abs(row[0] - 50.0),
        )
        self.assertAlmostEqual(level6_near_50[2], -3.0, delta=0.3)
        high_frequency = [row for row in parsed_response if row[0] >= 8000.0]
        self.assertLess(max(abs(row[2]) for row in high_frequency), 0.001)

        self.assertEqual(len(control_rows), 77)
        self.assertEqual(
            set(control_rows[0]),
            {
                "evidence_label",
                "analysis_index",
                "bass_relative_db",
                "rms_dbfs",
                "requested_level",
                "applied_level",
                "transition_active",
                "reason",
            },
        )
        self.assertEqual(
            {row["evidence_label"] for row in control_rows},
            {"HOST_SIMULATED_CONTROL"},
        )
        requested = [int(row["requested_level"]) for row in control_rows]
        applied = [int(row["applied_level"]) for row in control_rows]
        self.assertGreaterEqual(min(requested), 0)
        self.assertLessEqual(max(requested), 4)
        self.assertGreaterEqual(min(applied), 0)
        self.assertLessEqual(max(applied), 4)
        self.assertEqual(max(requested[:31]), 4)
        self.assertEqual(max(applied[:31]), 4)
        self.assertIn(1, [int(row["transition_active"]) for row in control_rows])
        self.assertTrue(
            all(abs(after - before) <= 1 for before, after in zip(applied, applied[1:]))
        )
        jitter_requested = requested[51:59]
        jitter_applied = applied[51:59]
        jitter_transition = [
            int(row["transition_active"]) for row in control_rows[51:59]
        ]
        self.assertEqual(set(jitter_requested), {0, 1})
        self.assertLessEqual(len(set(jitter_applied)), 2)
        self.assertLessEqual(
            sum(before != after for before, after in zip(jitter_applied, jitter_applied[1:])),
            1,
        )
        self.assertLessEqual(sum(jitter_transition), 1)
        self.assertEqual(max(applied[59:67]), 4)
        self.assertEqual(applied[67], 4)
        self.assertTrue(all(level == 0 for level in requested[67:]))
        self.assertEqual(applied[-1], 0)


if __name__ == "__main__":
    unittest.main()
