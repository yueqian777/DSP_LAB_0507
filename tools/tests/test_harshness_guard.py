from __future__ import annotations

import csv
import hashlib
import json
import math
import pathlib
import re
import shlex
import subprocess
import sys
import tempfile
import unittest
import wave


ROOT = pathlib.Path(__file__).resolve().parents[2]
BASH = pathlib.Path(r"C:\msys64\usr\bin\bash.exe")
INCLUDE = ROOT / "Code/User/user_dsp"
EQUALIZER = INCLUDE / "user_equalizer.c"
SMART_BASS = INCLUDE / "user_smart_bass.c"
DYNAMIC_CLARITY = INCLUDE / "user_dynamic_clarity.c"
HARSHNESS_GUARD = INCLUDE / "user_harshness_guard.c"
HARSHNESS_HEADER = INCLUDE / "user_harshness_guard.h"
FLOW = INCLUDE / "user_equalizer_flow.c"
FLOW_HEADER = INCLUDE / "user_equalizer_flow.h"
HARNESS = ROOT / "tools/tests/harshness_guard_test.c"
EVALUATOR = ROOT / "tools/harshness_guard_response_eval.c"
AUDIO_GENERATOR = ROOT / "tools/generate_harshness_guard_audio.py"


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


def parse_key_value_line(line: str) -> dict[str, str]:
    parsed: dict[str, str] = {}
    for token in line.split():
        if "=" in token:
            key, value = token.split("=", 1)
            parsed[key] = value
    return parsed


class HarshnessGuardTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.guard_source = HARSHNESS_GUARD.read_text(encoding="utf-8")
        cls.guard_header = HARSHNESS_HEADER.read_text(encoding="utf-8")
        cls.flow_source = FLOW.read_text(encoding="utf-8")
        cls.flow_header = FLOW_HEADER.read_text(encoding="utf-8")

    def test_host_harness_metrics_and_regressions(self) -> None:
        self.assertTrue(BASH.is_file(), f"missing MSYS2 bash: {BASH}")
        with tempfile.TemporaryDirectory(prefix="harshness_guard_host_") as temp:
            executable = pathlib.Path(temp) / "harshness_guard_test.exe"
            command = " ".join(
                (
                    "gcc -std=c99 -Wall -Wextra -Werror",
                    "-DEQ_ALGO_ONLY",
                    "-DEQ_ENABLE_AUDIO_FEATURE_ANALYZER=1",
                    "-DEQ_ENABLE_SMART_BASS=1",
                    "-DEQ_ENABLE_DYNAMIC_CLARITY=1",
                    "-DEQ_ENABLE_HARSHNESS_GUARD=1",
                    f"-I{shlex.quote(msys_path(INCLUDE))}",
                    shlex.quote(msys_path(EQUALIZER)),
                    shlex.quote(msys_path(SMART_BASS)),
                    shlex.quote(msys_path(DYNAMIC_CLARITY)),
                    shlex.quote(msys_path(HARSHNESS_GUARD)),
                    shlex.quote(msys_path(HARNESS)),
                    "-lm -o",
                    shlex.quote(msys_path(executable)),
                    "&&",
                    shlex.quote(msys_path(executable)),
                )
            )
            result = run_bash(command)

        self.assertEqual(result.returncode, 0, result.stdout)
        self.assertIn("harshness_guard: PASS", result.stdout)
        self.assertRegex(result.stdout, r"harshness_guard failures=0(?:\s|$)")
        self.assertNotIn("FAIL:", result.stdout)

        def metric(name: str) -> float:
            match = re.search(
                rf"(?:^|\s){re.escape(name)}=(-?\d+(?:\.\d+)?)",
                result.stdout,
            )
            self.assertIsNotNone(match, result.stdout)
            assert match is not None
            return float(match.group(1))

        self.assertEqual(metric("identity_exact"), 1.0)
        self.assertEqual(metric("default_enabled"), 0.0)
        self.assertEqual(metric("default_strength"), 2.0)
        self.assertEqual(metric("transition_samples"), 4000.0)
        self.assertGreater(metric("guard_state_bytes"), 0.0)
        self.assertLess(metric("guard_state_bytes"), 512.0)
        self.assertEqual(metric("harshness_formula_exact"), 1.0)
        self.assertEqual(metric("snapshot_immutable"), 1.0)
        self.assertEqual(metric("attack_step_max"), 1.0)
        self.assertEqual(metric("presence_min_gate"), 0.0)
        self.assertEqual(metric("release_confirmations"), 2.0)
        self.assertEqual(metric("gate_causes_to_zero"), 5.0)
        self.assertEqual(metric("debounce_transitions"), 0.0)
        self.assertEqual(metric("disable_identity_exact"), 1.0)
        self.assertEqual(metric("strength_high_to_low_final_level"), 2.0)
        self.assertEqual(metric("strength_off_identity_exact"), 1.0)
        self.assertEqual(metric("pending_preserved"), 1.0)
        self.assertEqual(metric("queued_applied"), 1.0)
        self.assertEqual(metric("latest_queue_wins"), 1.0)
        self.assertEqual(metric("transition_progress_monotonic"), 1.0)
        self.assertEqual(metric("sequence_epoch_reset"), 1.0)
        self.assertEqual(metric("impulse_09fs_saturation"), 0.0)
        self.assertEqual(metric("deterministic"), 1.0)
        self.assertEqual(metric("input_immutable"), 1.0)
        self.assertEqual(metric("illegal_strength_clamped"), 1.0)
        self.assertEqual(metric("guard_off_existing_chain_exact"), 1.0)
        self.assertEqual(metric("all_dynamic_off_static_eq_exact"), 1.0)

        mapping = [
            parse_key_value_line(line)
            for line in result.stdout.splitlines()
            if line.startswith("mapping_excess_db=")
        ]
        self.assertEqual(len(mapping), 7, result.stdout)
        self.assertEqual(
            [float(row["mapping_excess_db"]) for row in mapping],
            [6.0, 8.0, 10.0, 12.0, 14.0, 16.0, 18.0],
        )
        self.assertEqual(
            [int(row["mapping_level"]) for row in mapping],
            [0, 1, 1, 2, 2, 3, 3],
        )
        self.assertTrue(
            all(float(row["mapping_gain_db"]) <= 0.0 for row in mapping)
        )
        self.assertTrue(
            all(float(row["mapping_gain_db"]) >= -2.0 for row in mapping)
        )

        responses = [
            parse_key_value_line(line)
            for line in result.stdout.splitlines()
            if line.startswith("response_frequency_hz=")
        ]
        expected_frequencies = [
            1000.0,
            1953.125,
            4003.90625,
            5859.375,
            8007.8125,
            12011.71875,
            16015.625,
        ]
        self.assertEqual(len(responses), 7 * 5, result.stdout)
        parsed_responses = [
            (
                float(row["response_frequency_hz"]),
                int(row["response_level"]),
                float(row["response_gain_db"]),
            )
            for row in responses
        ]
        self.assertTrue(
            all(math.isfinite(value) for row in parsed_responses for value in row)
        )
        self.assertEqual(
            sorted({row[0] for row in parsed_responses}), expected_frequencies
        )
        self.assertEqual({row[1] for row in parsed_responses}, set(range(5)))
        for frequency in expected_frequencies:
            rows = [row for row in parsed_responses if row[0] == frequency]
            self.assertEqual(len(rows), 5)
            self.assertAlmostEqual(rows[0][2], 0.0, delta=1.0e-5)
        center = [row[2] for row in parsed_responses if row[0] == 5859.375]
        self.assertTrue(
            all(after < before for before, after in zip(center, center[1:])),
            center,
        )
        self.assertAlmostEqual(center[4], -2.0, delta=0.08)
        self.assertLess(metric("low_1khz_abs_max_db"), 0.10)
        self.assertLess(metric("high_16khz_abs_max_db"), 0.30)
        self.assertLess(metric("response_positive_max_db"), 0.002)

        pcm_rows = [
            parse_key_value_line(line)
            for line in result.stdout.splitlines()
            if line.startswith("pcm_chain=")
        ]
        self.assertEqual(len(pcm_rows), 7, result.stdout)
        self.assertEqual([row["pcm_chain"] for row in pcm_rows], list("ABCDEFG"))
        for row in pcm_rows:
            for field in (
                "max_error",
                "rms_error",
                "residual_dbfs",
                "dc_error",
                "clipped_samples",
                "saturation",
                "nonfinite",
            ):
                self.assertTrue(math.isfinite(float(row[field])), row)
            self.assertEqual(row["evidence_label"], "HOST_DIGITAL_SIMULATION")
            self.assertEqual(float(row["clipped_samples"]), 0.0)
            self.assertEqual(float(row["saturation"]), 0.0)
            self.assertEqual(float(row["nonfinite"]), 0.0)
        chain = {row["pcm_chain"]: row for row in pcm_rows}
        self.assertEqual(float(chain["A"]["max_error"]), 0.0)
        self.assertEqual(float(chain["B"]["max_error"]), 0.0)
        for name in "CDE":
            self.assertLessEqual(float(chain[name]["max_error"]), 0.501)
        self.assertLessEqual(float(chain["F"]["max_error"]), 1.5)
        self.assertLessEqual(float(chain["G"]["max_error"]), 2.5)
        self.assertLessEqual(float(chain["G"]["rms_error"]), 0.85)
        self.assertLessEqual(float(chain["G"]["dc_error"]), 0.15)
        self.assertEqual(metric("pcm_vectors"), 5.0)
        self.assertEqual(metric("pcm_chain_B_exact_A"), 1.0)
        self.assertEqual(metric("pcm_clip_count"), 0.0)
        self.assertEqual(metric("pcm_saturation_count"), 0.0)
        self.assertEqual(metric("pcm_nonfinite_count"), 0.0)

    def test_compile_gate_off_nm_c89_and_feature_independence(self) -> None:
        with tempfile.TemporaryDirectory(prefix="harshness_guard_gate_") as temp:
            temp_path = pathlib.Path(temp)

            rejected = temp_path / "guard_rejected.o"
            reject_result = run_bash(
                " ".join(
                    (
                        "gcc -std=c99 -Wall -Wextra -Werror -c",
                        "-DEQ_ALGO_ONLY",
                        "-DEQ_ENABLE_AUDIO_FEATURE_ANALYZER=0",
                        "-DEQ_ENABLE_SMART_BASS=0",
                        "-DEQ_ENABLE_DYNAMIC_CLARITY=0",
                        "-DEQ_ENABLE_HARSHNESS_GUARD=1",
                        f"-I{shlex.quote(msys_path(INCLUDE))}",
                        shlex.quote(msys_path(HARSHNESS_GUARD)),
                        "-o",
                        shlex.quote(msys_path(rejected)),
                    )
                )
            )
            self.assertNotEqual(reject_result.returncode, 0)
            self.assertIn(
                "Harshness Guard requires the audio feature analyzer.",
                reject_result.stdout,
            )

            independent = temp_path / "guard_independent.o"
            independent_result = run_bash(
                " ".join(
                    (
                        "gcc -std=c99 -Wall -Wextra -Werror -c",
                        "-DEQ_ALGO_ONLY",
                        "-DEQ_ENABLE_AUDIO_FEATURE_ANALYZER=1",
                        "-DEQ_ENABLE_SMART_BASS=0",
                        "-DEQ_ENABLE_DYNAMIC_CLARITY=0",
                        "-DEQ_ENABLE_HARSHNESS_GUARD=1",
                        f"-I{shlex.quote(msys_path(INCLUDE))}",
                        shlex.quote(msys_path(HARSHNESS_GUARD)),
                        "-o",
                        shlex.quote(msys_path(independent)),
                    )
                )
            )
            self.assertEqual(
                independent_result.returncode, 0, independent_result.stdout
            )

            off_object = temp_path / "guard_off.o"
            off_result = run_bash(
                " ".join(
                    (
                        "gcc -std=c99 -Wall -Wextra -Werror -c",
                        "-DEQ_ALGO_ONLY",
                        "-DEQ_ENABLE_AUDIO_FEATURE_ANALYZER=0",
                        "-DEQ_ENABLE_SMART_BASS=0",
                        "-DEQ_ENABLE_DYNAMIC_CLARITY=0",
                        "-DEQ_ENABLE_HARSHNESS_GUARD=0",
                        f"-I{shlex.quote(msys_path(INCLUDE))}",
                        shlex.quote(msys_path(HARSHNESS_GUARD)),
                        "-o",
                        shlex.quote(msys_path(off_object)),
                        "&& nm",
                        shlex.quote(msys_path(off_object)),
                    )
                )
            )
            self.assertEqual(off_result.returncode, 0, off_result.stdout)
            self.assertNotRegex(off_result.stdout, r"\bHarshnessGuard_")

            c89_object = temp_path / "guard_c89.o"
            c89_result = run_bash(
                " ".join(
                    (
                        "gcc -std=c89 -pedantic -Wall -Wextra -Werror -c",
                        "-DEQ_ALGO_ONLY",
                        "-DEQ_ENABLE_AUDIO_FEATURE_ANALYZER=1",
                        "-DEQ_ENABLE_SMART_BASS=0",
                        "-DEQ_ENABLE_DYNAMIC_CLARITY=0",
                        "-DEQ_ENABLE_HARSHNESS_GUARD=1",
                        f"-I{shlex.quote(msys_path(INCLUDE))}",
                        shlex.quote(msys_path(HARSHNESS_GUARD)),
                        "-o",
                        shlex.quote(msys_path(c89_object)),
                    )
                )
            )
            self.assertEqual(c89_result.returncode, 0, c89_result.stdout)

            flow_off_object = temp_path / "flow_guard_off.o"
            flow_off_result = run_bash(
                " ".join(
                    (
                        "gcc -std=c99 -Wall -Wextra -Werror -c",
                        "-DEQ_ALGO_ONLY",
                        "-DEQ_ENABLE_AUDIO_FEATURE_ANALYZER=0",
                        "-DEQ_ENABLE_SMART_BASS=0",
                        "-DEQ_ENABLE_DYNAMIC_CLARITY=0",
                        "-DEQ_ENABLE_HARSHNESS_GUARD=0",
                        f"-I{shlex.quote(msys_path(INCLUDE))}",
                        shlex.quote(msys_path(FLOW)),
                        "-o",
                        shlex.quote(msys_path(flow_off_object)),
                        "&& nm",
                        shlex.quote(msys_path(flow_off_object)),
                    )
                )
            )
            self.assertEqual(
                flow_off_result.returncode, 0, flow_off_result.stdout
            )
            self.assertNotIn("EQ_HarshnessGuardState", flow_off_result.stdout)
            self.assertNotRegex(flow_off_result.stdout, r"\bHarshnessGuard_")

    def test_response_and_control_evaluator(self) -> None:
        with tempfile.TemporaryDirectory(prefix="harshness_guard_eval_") as temp:
            temp_path = pathlib.Path(temp)
            executable = temp_path / "harshness_guard_response_eval.exe"
            response_path = temp_path / "harshness_guard_response.csv"
            control_path = temp_path / "harshness_guard_control.csv"
            command = " ".join(
                (
                    "gcc -std=c99 -Wall -Wextra -Werror",
                    "-DEQ_ALGO_ONLY",
                    "-DEQ_ENABLE_AUDIO_FEATURE_ANALYZER=1",
                    "-DEQ_ENABLE_HARSHNESS_GUARD=1",
                    f"-I{shlex.quote(msys_path(INCLUDE))}",
                    shlex.quote(msys_path(EQUALIZER)),
                    shlex.quote(msys_path(HARSHNESS_GUARD)),
                    shlex.quote(msys_path(EVALUATOR)),
                    "-lm -o",
                    shlex.quote(msys_path(executable)),
                    "&&",
                    shlex.quote(msys_path(executable)),
                    shlex.quote(msys_path(response_path)),
                    shlex.quote(msys_path(control_path)),
                )
            )
            result = run_bash(command)
            self.assertEqual(result.returncode, 0, result.stdout)

            with response_path.open(newline="", encoding="ascii") as source:
                response = list(csv.DictReader(source))
            with control_path.open(newline="", encoding="ascii") as source:
                control = list(csv.DictReader(source))

        self.assertEqual(len(response), 5 * 129)
        self.assertEqual(len(control), 128)
        self.assertTrue(
            all(row["evidence_label"] == "HOST_DIGITAL_RESPONSE" for row in response)
        )
        self.assertTrue(
            all(row["evidence_label"] == "HOST_SIMULATED_CONTROL" for row in control)
        )
        for row in response:
            for field in ("frequency_hz", "level", "gain_db", "phase_deg"):
                self.assertTrue(math.isfinite(float(row[field])), row)
        level_zero = [float(row["gain_db"]) for row in response if row["level"] == "0"]
        self.assertLess(max(abs(value) for value in level_zero), 1.0e-5)
        self.assertLessEqual(max(float(row["gain_db"]) for row in response), 0.002)

        def nearest(frequency: float, level: int) -> dict[str, str]:
            rows = [row for row in response if int(row["level"]) == level]
            return min(rows, key=lambda row: abs(float(row["frequency_hz"]) - frequency))

        center = nearest(6000.0, 4)
        self.assertAlmostEqual(float(center["gain_db"]), -2.0, delta=0.02)
        self.assertLess(
            max(abs(float(nearest(1000.0, level)["gain_db"])) for level in range(5)),
            0.04,
        )
        self.assertLess(
            max(abs(float(nearest(16015.625, level)["gain_db"])) for level in range(5)),
            0.10,
        )

        for index, row in enumerate(control):
            self.assertEqual(int(row["analysis_index"]), index)
            self.assertAlmostEqual(
                float(row["harshness_db"]),
                float(row["brightness_db"]) - float(row["presence_db"]),
                delta=1.0e-6,
            )
        ramp_levels = [int(row["requested_level"]) for row in control[:23]]
        self.assertEqual(ramp_levels, sorted(ramp_levels))
        self.assertEqual(max(ramp_levels), 3)
        self.assertEqual(int(control[62]["reason"]), 4)
        self.assertTrue(
            all(int(row["reason"]) in (4, 7) for row in control[62:68])
        )
        self.assertEqual(int(control[67]["applied_level"]), 0)
        self.assertTrue(all(int(row["reason"]) == 3 for row in control[68:74]))
        self.assertTrue(all(int(row["reason"]) == 2 for row in control[74:80]))
        self.assertEqual(max(int(row["applied_level"]) for row in control[80:96]), 4)
        self.assertEqual(int(control[111]["applied_level"]), 2)
        self.assertEqual(int(control[-1]["applied_level"]), 0)
        self.assertEqual(int(control[-1]["transition_active"]), 0)
        jitter_levels = [int(row["applied_level"]) for row in control[54:62]]
        jitter_changes = sum(
            before != after for before, after in zip(jitter_levels, jitter_levels[1:])
        )
        self.assertLessEqual(jitter_changes, 1)

    def test_generated_audio_matches_analyzer_contract(self) -> None:
        with tempfile.TemporaryDirectory(prefix="harshness_guard_audio_") as temp:
            result = subprocess.run(
                [sys.executable, "-B", str(AUDIO_GENERATOR), temp],
                cwd=ROOT,
                text=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                check=False,
            )
            self.assertEqual(result.returncode, 0, result.stdout)
            temp_path = pathlib.Path(temp)
            manifest = json.loads(
                (temp_path / "audio_manifest.json").read_text(encoding="ascii")
            )
            files = manifest["files"]
            self.assertEqual(len(files), 14)
            for entry in files:
                path = temp_path / entry["file_name"]
                self.assertTrue(path.is_file())
                self.assertEqual(
                    hashlib.sha256(path.read_bytes()).hexdigest().upper(),
                    entry["sha256"],
                )
                self.assertLess(float(entry["peak"]), 0.98)
                with wave.open(str(path), "rb") as source:
                    self.assertEqual(source.getnchannels(), 1)
                    self.assertEqual(source.getsampwidth(), 2)
                    self.assertEqual(source.getframerate(), 50_000)
            calibrated = [
                entry for entry in files if "target_harshness_db" in entry
            ]
            self.assertEqual(len(calibrated), 6)
            self.assertLess(
                max(
                    abs(
                        float(entry["target_harshness_db"])
                        - float(entry["harshness_db"])
                    )
                    for entry in calibrated
                ),
                0.01,
            )
            combined = next(
                entry for entry in files
                if entry["file_name"] == "all_dynamic_trigger.wav"
            )
            noise = next(
                entry for entry in files
                if entry["file_name"] == "transition_noise.wav"
            )
            self.assertEqual(manifest["transition_noise_period_samples"], 1024)
            self.assertEqual(noise["period_samples"], 1024)
            with wave.open(str(temp_path / noise["file_name"]), "rb") as source:
                noise_pcm = source.readframes(source.getnframes())
            period_bytes = 1024 * 2
            first_period = noise_pcm[:period_bytes]
            self.assertTrue(first_period)
            self.assertTrue(
                all(
                    noise_pcm[offset : offset + period_bytes] == first_period
                    for offset in range(0, len(noise_pcm), period_bytes)
                )
            )

        bass_mud = combined["bass_mud_phase"]
        brightness = combined["brightness_phase"]
        overlap = combined["overlap_phase"]
        self.assertGreaterEqual(float(bass_mud["bass_db"]), 16.0)
        self.assertGreaterEqual(float(bass_mud["mud_db"]), 0.0)
        self.assertGreaterEqual(float(bass_mud["presence_db"]), -12.0)
        self.assertAlmostEqual(float(bass_mud["masking_db"]), 14.0, delta=0.01)
        self.assertGreaterEqual(float(brightness["brightness_db"]), 0.0)
        self.assertAlmostEqual(float(brightness["harshness_db"]), 18.0, delta=0.01)
        self.assertGreaterEqual(float(overlap["bass_db"]), 5.5)
        self.assertGreaterEqual(float(overlap["mud_db"]), 0.0)
        self.assertGreaterEqual(float(overlap["brightness_db"]), 0.0)
        self.assertGreaterEqual(float(overlap["masking_db"]), 5.25)
        self.assertGreaterEqual(float(overlap["harshness_db"]), 8.0)

    def test_module_contracts(self) -> None:
        self.assertIn(
            "#define EQ_ENABLE_HARSHNESS_GUARD 0", self.guard_header
        )
        self.assertIn(
            "#error Harshness Guard requires the audio feature analyzer.",
            self.guard_header,
        )
        for macro, value in (
            ("HARSHNESS_GUARD_CENTER_HZ", "6000.0f"),
            ("HARSHNESS_GUARD_BANDWIDTH_OCTAVES", "1.0f"),
            ("HARSHNESS_GUARD_LEVEL_COUNT", "5"),
            ("HARSHNESS_GUARD_LEVEL_STEP_DB", "0.5f"),
            ("HARSHNESS_GUARD_TRANSITION_MS", "80.0f"),
            ("HARSHNESS_GUARD_START_DB", "6.0f"),
            ("HARSHNESS_GUARD_FULL_DB", "18.0f"),
        ):
            self.assertRegex(
                self.guard_header,
                rf"#define\s+{re.escape(macro)}\s+{re.escape(value)}",
            )

        forbidden = (
            "SmartBass_",
            "DynamicClarity_",
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
            self.assertNotIn(token, self.guard_source)
        for runtime_math in (
            "sinf(",
            "cosf(",
            "powf(",
            "sinhf(",
            "sqrtf(",
        ):
            self.assertNotIn(runtime_math, self.guard_source)
        self.assertEqual(
            self.guard_source.count("Equalizer_DesignRbjPeakingAt("), 1
        )
        init_start = self.guard_source.index("void HarshnessGuard_Init(")
        init_end = self.guard_source.index(
            "void HarshnessGuard_Reset(", init_start
        )
        self.assertIn(
            "Equalizer_DesignRbjPeakingAt(",
            self.guard_source[init_start:init_end],
        )
        update_start = self.guard_source.index(
            "int HarshnessGuard_UpdateFromFeature("
        )
        update_end = self.guard_source.index(
            "int HarshnessGuard_ProcessFrame(", update_start
        )
        update_body = self.guard_source[update_start:update_end]
        self.assertIn("harshness_db = brightness_db - presence_db;", update_body)
        self.assertNotRegex(update_body, r"snapshot->\w+\s*=(?!=)")
        self.assertIn("state->release_confirmation_count >= 2", update_body)
        self.assertIn("state->queued_level", update_body)

    def test_flow_default_macro_compile_state_and_order(self) -> None:
        self.assertIn(
            "#define EQ_ENABLE_HARSHNESS_GUARD 0", self.flow_header
        )
        self.assertIn(
            "#error Harshness Guard requires the audio feature analyzer.",
            self.flow_header,
        )
        self.assertIn(
            "volatile const unsigned int EQ_DebugHarshnessGuardCompiled =\n"
            "    EQ_ENABLE_HARSHNESS_GUARD;",
            self.flow_source,
        )
        self.assertIn(
            "extern volatile const unsigned int "
            "EQ_DebugHarshnessGuardCompiled;",
            self.flow_header,
        )

        state_line = "static HARSHNESS_GUARD_STATE EQ_HarshnessGuardState;"
        state_index = self.flow_source.index(state_line)
        guard_index = self.flow_source.rfind(
            "#if EQ_ENABLE_HARSHNESS_GUARD != 0", 0, state_index
        )
        end_index = self.flow_source.index("#endif", state_index)
        self.assertGreater(guard_index, 0)
        self.assertGreater(end_index, state_index)
        self.assertIn(
            '#pragma DATA_SECTION(EQ_HarshnessGuardState, ".subband_l2")',
            self.flow_source,
        )

        publish_start = self.flow_source.index(
            "static void EQ_PublishAnalyzerSnapshot(void)"
        )
        analyzer_service = self.flow_source.index(
            "static int EQ_ServiceAnalyzer(void)", publish_start
        )
        publish_body = self.flow_source[publish_start:analyzer_service]
        self.assertIn(
            "HarshnessGuard_UpdateFromFeature(\n"
            "        &EQ_HarshnessGuardState, &snapshot);",
            publish_body,
        )
        self.assertNotIn("EqualizerControl_", publish_body)

        capture_start = self.flow_source.index(
            "static void EQ_CaptureAdcFrame(void)"
        )
        capture_end = self.flow_source.index(
            "static void EQ_FillDacPingBuffer(void)", capture_start
        )
        capture = self.flow_source[capture_start:capture_end]
        equalizer_index = capture.index("Equalizer_ProcessFrame(")
        smart_index = capture.index("SmartBass_ProcessFrame(")
        clarity_index = capture.index("DynamicClarity_ProcessFrame(")
        guard_process_index = capture.index("HarshnessGuard_ProcessFrame(")
        capture_output_index = capture.index("EqualizerCapture_OnFrame(")
        self.assertLess(equalizer_index, smart_index)
        self.assertLess(smart_index, clarity_index)
        self.assertLess(clarity_index, guard_process_index)
        self.assertLess(guard_process_index, capture_output_index)
        self.assertIn(
            "&EQ_HarshnessGuardState, EQ_DA_Buffer1, EQ_DA_Buffer1,",
            capture,
        )
        self.assertIn(
            "HarshnessGuard_InvalidateAnalysisEpoch(", self.flow_source
        )
        self.assertIn("EQ_MarkHarshnessGuardAnalyzerUnavailable();", self.flow_source)

        runtime_start = self.flow_source.index(
            "static void EQ_ServiceHarshnessGuardRuntimeControl(void)"
        )
        unavailable_start = self.flow_source.index(
            "static void EQ_MarkHarshnessGuardAnalyzerUnavailable(void)",
            runtime_start,
        )
        runtime_body = self.flow_source[runtime_start:unavailable_start]
        self.assertIn(
            "(EQ_HarshnessGuardState.requested_enabled != 0)",
            runtime_body,
        )

        timing_start = self.flow_source.index(
            "static void EQ_UpdateHarshnessGuardTiming", unavailable_start
        )
        unavailable_body = self.flow_source[unavailable_start:timing_start]
        self.assertIn(
            "HarshnessGuard_SetEnabled(\n"
            "            &EQ_HarshnessGuardState, 0);",
            unavailable_body,
        )
        reset_start = self.flow_source.index(
            "static void EQ_MarkHarshnessGuardAnalyzerReset(void)",
            unavailable_start,
        )
        reset_body = self.flow_source[reset_start:timing_start]
        self.assertIn("HarshnessGuard_InvalidateAnalysisEpoch(", reset_body)
        self.assertNotIn("HarshnessGuard_SetEnabled(", reset_body)
        clear_start = self.flow_source.index(
            "static void EQ_ClearPublishedAnalyzerState(", reset_start
        )
        reset_runtime = self.flow_source.index(
            "static void EQ_ResetAnalyzerRuntime(void)", clear_start
        )
        clear_body = self.flow_source[clear_start:reset_runtime]
        self.assertIn("EQ_MarkHarshnessGuardAnalyzerReset();", clear_body)
        self.assertIn("EQ_MarkHarshnessGuardAnalyzerUnavailable();", clear_body)
        self.assertIn(
            "EQ_HarshnessGuardAnalyzerFault =\n"
            "        ((snapshot.valid != 0) &&\n"
            "         (snapshot.warmup_complete != 0)) ? 0U : 1U;",
            publish_body,
        )

        with tempfile.TemporaryDirectory(
            prefix="harshness_guard_compiled_state_"
        ) as temp:
            temp_path = pathlib.Path(temp)
            compiled_state = temp_path / "compiled_state.c"
            compiled_state.write_text(
                '#include "user_equalizer_flow.h"\n'
                "#ifndef EXPECTED\n#define EXPECTED 0\n#endif\n"
                "int main(void) { return "
                "(EQ_DebugHarshnessGuardCompiled == EXPECTED) ? 0 : 1; }\n",
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
                        "-DEQ_ENABLE_DYNAMIC_CLARITY=0",
                        f"-DEQ_ENABLE_HARSHNESS_GUARD={enabled}",
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


if __name__ == "__main__":
    unittest.main()
