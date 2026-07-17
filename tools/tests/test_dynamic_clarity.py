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
DYNAMIC_CLARITY = INCLUDE / "user_dynamic_clarity.c"
DYNAMIC_HEADER = INCLUDE / "user_dynamic_clarity.h"
FLOW = INCLUDE / "user_equalizer_flow.c"
FLOW_HEADER = INCLUDE / "user_equalizer_flow.h"
HARNESS = ROOT / "tools/tests/dynamic_clarity_test.c"
EVALUATOR = ROOT / "tools/dynamic_clarity_response_eval.c"


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


class DynamicClarityTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.dynamic_source = DYNAMIC_CLARITY.read_text(encoding="utf-8")
        cls.dynamic_header = DYNAMIC_HEADER.read_text(encoding="utf-8")
        cls.flow_source = FLOW.read_text(encoding="utf-8")
        cls.flow_header = FLOW_HEADER.read_text(encoding="utf-8")

    def test_host_harness(self) -> None:
        self.assertTrue(BASH.is_file(), f"missing MSYS2 bash: {BASH}")
        with tempfile.TemporaryDirectory(prefix="dynamic_clarity_host_") as temp:
            executable = pathlib.Path(temp) / "dynamic_clarity_test.exe"
            command = " ".join(
                (
                    "gcc -std=c99 -Wall -Wextra -Werror",
                    "-DEQ_ALGO_ONLY",
                    "-DEQ_ENABLE_AUDIO_FEATURE_ANALYZER=1",
                    "-DEQ_ENABLE_SMART_BASS=1",
                    "-DEQ_ENABLE_DYNAMIC_CLARITY=1",
                    f"-I{shlex.quote(msys_path(INCLUDE))}",
                    shlex.quote(msys_path(EQUALIZER)),
                    shlex.quote(msys_path(SMART_BASS)),
                    shlex.quote(msys_path(DYNAMIC_CLARITY)),
                    shlex.quote(msys_path(HARNESS)),
                    "-lm -o",
                    shlex.quote(msys_path(executable)),
                    "&&",
                    shlex.quote(msys_path(executable)),
                )
            )
            result = run_bash(command)

        self.assertEqual(result.returncode, 0, result.stdout)
        self.assertIn("dynamic_clarity: PASS", result.stdout)
        self.assertRegex(result.stdout, r"dynamic_clarity failures=0(?:\s|$)")
        self.assertNotIn("FAIL:", result.stdout)
        self.assertIn(
            "chain_evidence_label=HOST_DIGITAL_SIMULATION", result.stdout
        )
        self.assertIn(
            "quant_evidence_label=HOST_DIGITAL_SIMULATION", result.stdout
        )

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
        self.assertEqual(metric("peaking_static_bit_exact"), 1.0)
        self.assertEqual(metric("peaking_static_bit_exact_cases"), 40.0)
        self.assertEqual(metric("masking_formula_exact"), 1.0)
        self.assertEqual(metric("snapshot_immutable"), 1.0)
        self.assertEqual(metric("attack_step_max"), 1.0)
        self.assertEqual(metric("release_confirmations"), 2.0)
        self.assertEqual(metric("gate_causes_to_zero"), 6.0)
        self.assertEqual(metric("debounce_transitions"), 0.0)
        self.assertEqual(metric("disable_identity_exact"), 1.0)
        self.assertEqual(metric("latest_wins"), 1.0)
        self.assertEqual(metric("queued_applied"), 1.0)
        self.assertEqual(metric("queued_release_preserved"), 1.0)
        self.assertEqual(metric("transition_progress_monotonic"), 1.0)
        self.assertEqual(metric("sequence_epoch_reset"), 1.0)
        self.assertEqual(metric("transition_saturation"), 0.0)
        self.assertEqual(metric("impulse_09fs_saturation"), 0.0)
        self.assertAlmostEqual(metric("level6_400hz_db"), -3.0, delta=0.08)
        self.assertLess(metric("low_100hz_abs_max_db"), 0.20)
        self.assertLess(metric("high_8khz_abs_max_db"), 0.01)
        self.assertLess(metric("response_positive_max_db"), 0.002)
        self.assertEqual(metric("deterministic"), 1.0)
        self.assertEqual(metric("input_immutable"), 1.0)
        self.assertEqual(metric("eq_off_chain_exact"), 1.0)
        self.assertEqual(metric("smart_bass_off_chain_exact"), 1.0)
        self.assertEqual(metric("quant_vectors"), 4.0)
        self.assertEqual(metric("quant_chain_b_exact_a"), 1.0)
        self.assertEqual(metric("quant_chain_b_max_error"), 0.0)
        self.assertLessEqual(metric("quant_chain_c_max_error"), 0.501)
        self.assertLessEqual(metric("quant_chain_d_max_error"), 0.501)
        self.assertLessEqual(metric("quant_chain_e_max_error"), 1.5)
        self.assertLessEqual(metric("quant_chain_e_rms_error"), 0.6)
        self.assertLessEqual(metric("quant_chain_e_dc_error"), 0.1)
        self.assertEqual(metric("quant_clip_count"), 0.0)
        self.assertTrue(math.isfinite(metric("quant_chain_e_residual_dbfs")))

    def test_compile_gate_off_object_c89_and_build_state(self) -> None:
        with tempfile.TemporaryDirectory(prefix="dynamic_clarity_gate_") as temp:
            temp_path = pathlib.Path(temp)
            rejected = temp_path / "rejected.o"
            reject_result = run_bash(
                " ".join(
                    (
                        "gcc -std=c99 -Wall -Wextra -Werror -c",
                        "-DEQ_ALGO_ONLY",
                        "-DEQ_ENABLE_AUDIO_FEATURE_ANALYZER=0",
                        "-DEQ_ENABLE_SMART_BASS=0",
                        "-DEQ_ENABLE_DYNAMIC_CLARITY=1",
                        f"-I{shlex.quote(msys_path(INCLUDE))}",
                        shlex.quote(msys_path(DYNAMIC_CLARITY)),
                        "-o",
                        shlex.quote(msys_path(rejected)),
                    )
                )
            )
            self.assertNotEqual(reject_result.returncode, 0)
            self.assertIn(
                "Dynamic Clarity requires the audio feature analyzer.",
                reject_result.stdout,
            )

            off_object = temp_path / "dynamic_clarity_off.o"
            off_result = run_bash(
                " ".join(
                    (
                        "gcc -std=c99 -Wall -Wextra -Werror -c",
                        "-DEQ_ALGO_ONLY",
                        "-DEQ_ENABLE_AUDIO_FEATURE_ANALYZER=0",
                        "-DEQ_ENABLE_SMART_BASS=0",
                        "-DEQ_ENABLE_DYNAMIC_CLARITY=0",
                        f"-I{shlex.quote(msys_path(INCLUDE))}",
                        shlex.quote(msys_path(DYNAMIC_CLARITY)),
                        "-o",
                        shlex.quote(msys_path(off_object)),
                        "&& nm",
                        shlex.quote(msys_path(off_object)),
                    )
                )
            )
            self.assertEqual(off_result.returncode, 0, off_result.stdout)
            self.assertNotIn("DynamicClarity_", off_result.stdout)

            c89_object = temp_path / "dynamic_clarity_c89.o"
            c89_result = run_bash(
                " ".join(
                    (
                        "gcc -std=c89 -pedantic -Wall -Wextra -Werror -c",
                        "-DEQ_ALGO_ONLY",
                        "-DEQ_ENABLE_AUDIO_FEATURE_ANALYZER=1",
                        "-DEQ_ENABLE_SMART_BASS=0",
                        "-DEQ_ENABLE_DYNAMIC_CLARITY=1",
                        f"-I{shlex.quote(msys_path(INCLUDE))}",
                        shlex.quote(msys_path(DYNAMIC_CLARITY)),
                        "-o",
                        shlex.quote(msys_path(c89_object)),
                    )
                )
            )
            self.assertEqual(c89_result.returncode, 0, c89_result.stdout)

            compiled_state = temp_path / "compiled_state.c"
            compiled_state.write_text(
                '#include "user_equalizer_flow.h"\n'
                "#ifndef EXPECTED\n#define EXPECTED 0\n#endif\n"
                "int main(void) { return "
                "(EQ_DebugDynamicClarityCompiled == EXPECTED) ? 0 : 1; }\n",
                encoding="ascii",
            )
            for enabled in (0, 1):
                executable = temp_path / f"compiled_state_{enabled}.exe"
                command = " ".join(
                    (
                        "gcc -std=c99 -Wall -Wextra -Werror",
                        "-DEQ_ALGO_ONLY",
                        "-DEQ_ENABLE_AUDIO_FEATURE_ANALYZER=1",
                        "-DEQ_ENABLE_SMART_BASS=0",
                        f"-DEQ_ENABLE_DYNAMIC_CLARITY={enabled}",
                        f"-DEXPECTED={enabled}",
                        f"-I{shlex.quote(msys_path(INCLUDE))}",
                        shlex.quote(msys_path(FLOW)),
                        shlex.quote(msys_path(compiled_state)),
                        "-o",
                        shlex.quote(msys_path(executable)),
                        "&&",
                        shlex.quote(msys_path(executable)),
                    )
                )
                result = run_bash(command)
                self.assertEqual(result.returncode, 0, result.stdout)

    def test_module_and_flow_contracts(self) -> None:
        self.assertIn("#define EQ_ENABLE_DYNAMIC_CLARITY 0", self.dynamic_header)
        self.assertIn("#define EQ_ENABLE_DYNAMIC_CLARITY 0", self.flow_header)
        self.assertIn(
            "#error Dynamic Clarity requires the audio feature analyzer.",
            self.dynamic_header,
        )
        self.assertIn(
            "volatile const unsigned int EQ_DebugDynamicClarityCompiled =\n"
            "    EQ_ENABLE_DYNAMIC_CLARITY;",
            self.flow_source,
        )
        self.assertIn(
            "extern volatile const unsigned int "
            "EQ_DebugDynamicClarityCompiled;",
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
            "malloc(",
        )
        for token in forbidden:
            self.assertNotIn(token, self.dynamic_source)
        for math_call in ("sinf(", "cosf(", "powf(", "sinhf(", "sqrtf("):
            self.assertNotIn(math_call, self.dynamic_source)
        self.assertEqual(
            self.dynamic_source.count("Equalizer_DesignRbjPeakingAt("), 1
        )
        init_start = self.dynamic_source.index("void DynamicClarity_Init(")
        init_end = self.dynamic_source.index(
            "void DynamicClarity_Reset(", init_start
        )
        self.assertIn(
            "Equalizer_DesignRbjPeakingAt(",
            self.dynamic_source[init_start:init_end],
        )
        source_lower = (self.dynamic_source + self.dynamic_header).lower()
        for unsupported_claim in (
            "voice_found",
            "speech_found",
            "vocal_detected",
        ):
            self.assertNotIn(unsupported_claim, source_lower)

        state_line = "static DYNAMIC_CLARITY_STATE EQ_DynamicClarityState;"
        state_index = self.flow_source.index(state_line)
        guard_index = self.flow_source.rfind(
            "#if EQ_ENABLE_DYNAMIC_CLARITY != 0", 0, state_index
        )
        end_index = self.flow_source.index("#endif", state_index)
        self.assertGreater(guard_index, 0)
        self.assertGreater(end_index, state_index)
        self.assertIn(
            '#pragma DATA_SECTION(EQ_DynamicClarityState, ".subband_l2")',
            self.flow_source,
        )

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
        clarity = capture.index("DynamicClarity_ProcessFrame(")
        capture_output = capture.index("EqualizerCapture_OnFrame(")
        self.assertLess(observe, equalizer)
        self.assertLess(equalizer, smart_bass)
        self.assertLess(smart_bass, clarity)
        self.assertLess(clarity, capture_output)
        self.assertIn(
            "&EQ_DynamicClarityState, EQ_DA_Buffer1, EQ_DA_Buffer1,",
            capture,
        )

        self.assertIn(
            "DynamicClarity_InvalidateAnalysisEpoch(", self.flow_source
        )
        self.assertIn("DynamicClarity_UpdateFromFeature(", self.flow_source)
        self.assertIn(
            "EQ_MarkDynamicClarityAnalyzerUnavailable();", self.flow_source
        )
        publish_start = self.flow_source.index(
            "static void EQ_PublishAnalyzerSnapshot(void)"
        )
        analyzer_service = self.flow_source.index(
            "static int EQ_ServiceAnalyzer(void)", publish_start
        )
        publish_body = self.flow_source[publish_start:analyzer_service]
        self.assertIn("DynamicClarity_UpdateFromFeature(", publish_body)
        self.assertNotIn("EqualizerControl_", publish_body)

    def test_response_and_control_evaluator(self) -> None:
        with tempfile.TemporaryDirectory(prefix="dynamic_clarity_eval_") as temp:
            temp_path = pathlib.Path(temp)
            executable = temp_path / "dynamic_clarity_response_eval.exe"
            response_csv = temp_path / "dynamic_clarity_response.csv"
            control_csv = temp_path / "dynamic_clarity_control.csv"
            command = " ".join(
                (
                    "gcc -std=c99 -Wall -Wextra -Werror",
                    "-DEQ_ALGO_ONLY",
                    "-DEQ_ENABLE_AUDIO_FEATURE_ANALYZER=1",
                    "-DEQ_ENABLE_SMART_BASS=0",
                    "-DEQ_ENABLE_DYNAMIC_CLARITY=1",
                    f"-I{shlex.quote(msys_path(INCLUDE))}",
                    shlex.quote(msys_path(EQUALIZER)),
                    shlex.quote(msys_path(DYNAMIC_CLARITY)),
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
                all(
                    after > before
                    for before, after in zip(frequencies, frequencies[1:])
                )
            )
        level_zero = [row for row in parsed_response if row[1] == 0]
        self.assertLess(max(abs(row[2]) for row in level_zero), 1.0e-5)
        self.assertLess(max(abs(row[3]) for row in level_zero), 1.0e-5)

        gains_at_center = []
        for level in range(7):
            nearest = min(
                (row for row in parsed_response if row[1] == level),
                key=lambda row: abs(row[0] - 400.0),
            )
            gains_at_center.append(nearest[2])
        self.assertTrue(
            all(
                after < before
                for before, after in zip(gains_at_center, gains_at_center[1:])
            ),
            gains_at_center,
        )
        self.assertAlmostEqual(gains_at_center[6], -3.0, delta=0.10)
        low_frequency = [
            min(
                (row for row in parsed_response if row[1] == level),
                key=lambda row: abs(row[0] - 100.0),
            )
            for level in range(7)
        ]
        self.assertLess(max(abs(row[2]) for row in low_frequency), 0.20)
        high_frequency = [row for row in parsed_response if row[0] >= 8000.0]
        self.assertLess(max(abs(row[2]) for row in high_frequency), 0.01)
        self.assertLess(max(row[2] for row in parsed_response), 0.002)

        self.assertEqual(len(control_rows), 82)
        self.assertEqual(
            set(control_rows[0]),
            {
                "evidence_label",
                "analysis_index",
                "mud_db",
                "presence_db",
                "masking_db",
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
        for row in control_rows:
            numeric = [
                float(row["mud_db"]),
                float(row["presence_db"]),
                float(row["masking_db"]),
                float(row["rms_dbfs"]),
            ]
            self.assertTrue(all(math.isfinite(value) for value in numeric))
            self.assertAlmostEqual(
                float(row["masking_db"]),
                float(row["mud_db"]) - float(row["presence_db"]),
                delta=1.0e-6,
            )

        requested = [int(row["requested_level"]) for row in control_rows]
        applied = [int(row["applied_level"]) for row in control_rows]
        transitions = [int(row["transition_active"]) for row in control_rows]
        reasons = [int(row["reason"]) for row in control_rows]
        self.assertGreaterEqual(min(requested), 0)
        self.assertLessEqual(max(requested), 4)
        self.assertGreaterEqual(min(applied), 0)
        self.assertLessEqual(max(applied), 4)
        self.assertIn(1, transitions)
        self.assertTrue(
            all(
                abs(after - before) <= 1
                for before, after in zip(applied, applied[1:])
            )
        )
        self.assertTrue(
            all(after >= before for before, after in zip(requested[:19], requested[1:19]))
        )
        self.assertEqual(max(requested[:27]), 4)
        self.assertTrue(all(level == 4 for level in requested[19:27]))
        self.assertTrue(
            all(
                after <= before
                for before, after in zip(requested[27:46], requested[28:46])
            )
        )
        self.assertEqual(set(requested[46:54]), {0, 1})
        self.assertLessEqual(
            sum(
                before != after
                for before, after in zip(applied[46:54], applied[47:54])
            ),
            1,
        )
        for segment, gate_reason in (
            (reasons[54:60], 4),
            (reasons[60:66], 5),
            (reasons[66:74], 3),
            (reasons[74:82], 2),
        ):
            self.assertIn(gate_reason, segment)
            self.assertTrue(
                all(reason in {gate_reason, 8} for reason in segment), segment
            )
        self.assertEqual(applied[-1], 0)


if __name__ == "__main__":
    unittest.main()
