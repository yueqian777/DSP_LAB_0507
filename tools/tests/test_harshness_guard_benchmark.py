from __future__ import annotations

import json
import shlex
import struct
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
BENCHMARK_C = ROOT / "Code" / "User" / "user_dsp" / (
    "user_dynamic_clarity_benchmark.c"
)
BENCHMARK_H = ROOT / "Code" / "User" / "user_dsp" / (
    "user_dynamic_clarity_benchmark.h"
)
DSS = ROOT / "tools" / "dss" / "dss_harshness_guard_benchmark.js"
ANALYZER = ROOT / "tools" / "analyze_harshness_guard_benchmark.py"
RUNNER = ROOT / "tools" / "run_harshness_guard_benchmark.ps1"
FLOW = ROOT / "Code" / "User" / "user_dsp" / "user_equalizer_flow.c"
MSYS_BASH = Path(r"C:\msys64\usr\bin\bash.exe")

MODULES = ("dynamic_clarity", "smart_bass", "harshness_guard")
INPUTS = (
    "tone_400hz",
    "dual_400hz_1953_125hz",
    "music_like_multitone",
    "deterministic_pseudorandom",
)
CASES = (
    "level_0_identity",
    "level_1_stable",
    "level_medium_max_stable",
    "transition_0_to_1",
    "transition_1_to_2",
    "transition_medium_to_previous",
    "transition_1_to_0",
)
MEASURED_COUNT = 4096
JOB_COUNT = len(MODULES) * len(INPUTS) * len(CASES)


def msys_path(path: Path) -> str:
    absolute = path.resolve().as_posix()
    return "/" + absolute[0].lower() + absolute[2:]


class HarshnessGuardBenchmarkTest(unittest.TestCase):
    def run_msys(self, command: str, *, check: bool = True) -> subprocess.CompletedProcess:
        if not MSYS_BASH.exists():
            self.skipTest("MSYS2 bash is not installed")
        script = (
            "export PATH=/mingw64/bin:/usr/bin:$PATH; "
            f"cd {shlex.quote(msys_path(ROOT))}; {command}"
        )
        return subprocess.run(
            [str(MSYS_BASH), "-lc", script],
            check=check,
            text=True,
            capture_output=True,
        )

    def test_compile_gates_and_production_symbols(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            temp = Path(directory)
            fake_include = temp / "include"
            fake_include.mkdir()
            (fake_include / "c6x.h").write_text(
                "#ifndef TEST_C6X_H\n"
                "#define TEST_C6X_H\n"
                "extern volatile unsigned int test_tscl;\n"
                "#define TSCL test_tscl\n"
                "#define asm(text)\n"
                "#endif\n",
                encoding="ascii",
            )
            contract = temp / "contract.c"
            contract.write_text(
                '#include "user_dynamic_clarity_benchmark.h"\n'
                "typedef char check_warmup[\n"
                "    (EQ_DYNAMIC_CLARITY_BENCHMARK_WARMUP_COUNT == 64) ? 1 : -1];\n"
                "typedef char check_measure[\n"
                "    (EQ_DYNAMIC_CLARITY_BENCHMARK_MEASURED_COUNT == 4096) ? 1 : -1];\n"
                "typedef char check_modules[\n"
                "    (EQ_DYNAMIC_CLARITY_BENCHMARK_MODULE_COUNT == EXPECTED_MODULES)\n"
                "    ? 1 : -1];\n"
                "typedef char check_jobs[\n"
                "    (EQ_DYNAMIC_CLARITY_BENCHMARK_JOB_COUNT == EXPECTED_JOBS)\n"
                "    ? 1 : -1];\n"
                "int main(void) { return 0; }\n",
                encoding="ascii",
            )
            default_contract = temp / "default_contract.c"
            default_contract.write_text(
                '#include "user_dynamic_clarity_benchmark.h"\n'
                "#if EQ_ENABLE_HARSHNESS_GUARD_BENCHMARK != 0\n"
                '#error "Harshness Guard benchmark default must be zero"\n'
                "#endif\n"
                "int main(void) { return 0; }\n",
                encoding="ascii",
            )

            include = shlex.quote(msys_path(fake_include))
            contract_path = shlex.quote(msys_path(contract))
            default_path = shlex.quote(msys_path(default_contract))
            temp_path = msys_path(temp)
            common = (
                "-std=gnu89 -Wall -Wextra -Werror -Wno-unknown-pragmas "
                "-D__TMS320C6X__ -DEQ_ALGO_ONLY "
                "-DEQ_ENABLE_AUDIO_FEATURE_ANALYZER=1 "
                "-DEQ_ENABLE_SMART_BASS=1 -DEQ_ENABLE_DYNAMIC_CLARITY=1 "
                f"-I{include} -ICode/User/user_dsp"
            )
            old_defines = (
                "-DEQ_ENABLE_HARSHNESS_GUARD=0 "
                "-DEQ_ENABLE_DYNAMIC_CLARITY_BENCHMARK=1 "
                "-DEQ_ENABLE_HARSHNESS_GUARD_BENCHMARK=0"
            )
            extended_defines = (
                "-DEQ_ENABLE_HARSHNESS_GUARD=1 "
                "-DEQ_ENABLE_DYNAMIC_CLARITY_BENCHMARK=1 "
                "-DEQ_ENABLE_HARSHNESS_GUARD_BENCHMARK=1"
            )

            self.run_msys(
                "gcc -std=c89 -Wall -Wextra -Werror -DEQ_ALGO_ONLY "
                f"-ICode/User/user_dsp {shlex.quote(msys_path(BENCHMARK_C))} "
                f"-c -o {shlex.quote(temp_path + '/production.o')} && "
                f"! nm {shlex.quote(temp_path + '/production.o')} | "
                "grep -q EQ_DebugDynamicClarityBenchmark"
            )
            self.run_msys(
                f"gcc -std=c89 -Wall -Wextra -Werror -DEQ_ALGO_ONLY "
                f"-ICode/User/user_dsp {default_path} "
                f"-o {shlex.quote(temp_path + '/default.exe')}"
            )
            self.run_msys(
                f"gcc {common} {old_defines} -DEXPECTED_MODULES=2 "
                f"-DEXPECTED_JOBS=56 {contract_path} "
                f"-o {shlex.quote(temp_path + '/old_contract.exe')}"
            )
            self.run_msys(
                f"gcc {common} {extended_defines} -DEXPECTED_MODULES=3 "
                f"-DEXPECTED_JOBS=84 {contract_path} "
                f"-o {shlex.quote(temp_path + '/extended_contract.exe')}"
            )
            self.run_msys(
                f"gcc {common} {old_defines} "
                f"{shlex.quote(msys_path(BENCHMARK_C))} -c "
                f"-o {shlex.quote(temp_path + '/old.o')} && "
                f"! nm {shlex.quote(temp_path + '/old.o')} | "
                "grep -q Harshness"
            )
            self.run_msys(
                f"gcc {common} {extended_defines} "
                f"{shlex.quote(msys_path(BENCHMARK_C))} -c "
                f"-o {shlex.quote(temp_path + '/extended.o')} && "
                f"nm {shlex.quote(temp_path + '/extended.o')} | "
                "grep -q EQ_DebugHarshnessGuardBenchmarkCompiled && "
                f"nm {shlex.quote(temp_path + '/extended.o')} | "
                "grep -q EQ_DebugDynamicClarityBenchmarkHarshnessSaturationCount"
            )

            invalid = self.run_msys(
                f"gcc {common} -DEQ_ENABLE_HARSHNESS_GUARD=1 "
                "-DEQ_ENABLE_DYNAMIC_CLARITY_BENCHMARK=0 "
                "-DEQ_ENABLE_HARSHNESS_GUARD_BENCHMARK=1 "
                f"{default_path} -fsyntax-only",
                check=False,
            )
            self.assertNotEqual(invalid.returncode, 0)
            self.assertIn(
                "Harshness Guard benchmark requires the Dynamic Clarity benchmark",
                invalid.stderr,
            )

    def test_source_contract_and_pre_audio_entry(self) -> None:
        header = BENCHMARK_H.read_text(encoding="ascii")
        source = BENCHMARK_C.read_text(encoding="ascii")
        flow = FLOW.read_text(encoding="ascii")
        self.assertIn("#define EQ_ENABLE_HARSHNESS_GUARD_BENCHMARK 0", header)
        self.assertIn("EQ_DYNAMIC_CLARITY_BENCHMARK_WARMUP_COUNT    64", header)
        self.assertIn("EQ_DYNAMIC_CLARITY_BENCHMARK_MEASURED_COUNT  4096", header)
        self.assertIn("EQ_BENCHMARK_MODULE_HARSHNESS", source)
        self.assertIn("medium_level = 3", source)
        self.assertIn("HarshnessGuard_ProcessFrame", source)
        self.assertIn(
            "EQ_DebugDynamicClarityBenchmarkHarshnessSaturationCount",
            source,
        )
        self.assertIn(
            "EQ_DebugDynamicClarityBenchmarkHarshnessNonFiniteCount",
            source,
        )
        self.assertLess(
            flow.index("DynamicClarityBenchmark_Run();"),
            flow.index("Adc_Init(ADC_50KHZ"),
        )

    def test_dss_runner_contract(self) -> None:
        dss = DSS.read_text(encoding="ascii")
        runner = RUNNER.read_text(encoding="ascii")
        for symbol in (
            "EQ_DebugHarshnessGuardBenchmarkCompiled",
            "EQ_DebugDynamicClarityBenchmarkHarshnessSaturationCount",
            "EQ_DebugDynamicClarityBenchmarkHarshnessNonFiniteCount",
            "EQ_DebugDynamicClarityBenchmarkCycles",
        ):
            self.assertIn(symbol, dss)
        self.assertIn("WARMUP_COUNT = 64", dss)
        self.assertIn("MEASURED_COUNT = 4096", dss)
        self.assertIn("debugSession.target.run();", dss)
        self.assertNotIn("runFor(", dss)
        self.assertIn('evaluate("AD_En = 0")', dss)
        self.assertIn('evaluate("DA_En = 0")', dss)
        self.assertIn('evaluate("FLAG_AD = 0")', dss)
        self.assertIn('evaluate("FLAG_DA = 0")', dss)
        self.assertIn("Unable to clear stale audio service flags", dss)
        self.assertIn("state.init_stage == 0", dss)
        self.assertIn("state.ad_en == 0", dss)
        self.assertIn("state.da_en == 0", dss)
        self.assertIn("state.flag_ad == 0", dss)
        self.assertIn("state.flag_da == 0", dss)
        self.assertIn("harshness_guard_benchmark_cycles.raw", dss)
        self.assertIn("EQ_ENABLE_DYNAMIC_CLARITY_BENCHMARK=1", runner)
        self.assertIn("EQ_ENABLE_HARSHNESS_GUARD_BENCHMARK=1", runner)
        self.assertIn("EQ_ENABLE_HARSHNESS_GUARD=1", runner)
        self.assertIn("ResultDir must stay under the current TEMP directory", runner)

        command = (
            "$tokens=$null; $errors=$null; "
            "[System.Management.Automation.Language.Parser]::ParseFile("
            f"'{str(RUNNER).replace(chr(39), chr(39) * 2)}',"
            "[ref]$tokens,[ref]$errors) | Out-Null; "
            "if($errors.Count -ne 0){$errors | ForEach-Object {$_.Message}; exit 1}"
        )
        subprocess.run(
            ["powershell.exe", "-NoProfile", "-Command", command],
            check=True,
            text=True,
            capture_output=True,
        )

    def test_analyzer_statistics_ratios_and_safety_contract(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            result_dir = Path(directory)
            first_calls: list[int] = []
            warm_maxes: list[int] = []
            checksums: list[int] = []
            raw_path = result_dir / "harshness_guard_benchmark_cycles.raw"
            with raw_path.open("wb") as stream:
                job = 0
                for module_index, _module in enumerate(MODULES):
                    for input_index, _input in enumerate(INPUTS):
                        for case_index, _case in enumerate(CASES):
                            base = (
                                (100, 80, 130)[module_index]
                                + input_index * 7
                                + case_index * 3
                            )
                            values = [
                                base + (iteration % 17)
                                for iteration in range(MEASURED_COUNT)
                            ]
                            stream.write(
                                struct.pack(f"<{MEASURED_COUNT}I", *values)
                            )
                            first_calls.append(base + 70)
                            warm_maxes.append(base + 40)
                            checksums.append(job + 1)
                            job += 1

            state = {
                "dynamic_saturation": 0,
                "dynamic_nonfinite": 0,
                "smart_saturation": 0,
                "smart_nonfinite": 0,
                "harshness_saturation": 0,
                "harshness_nonfinite": 0,
            }
            board = {
                "pass": True,
                "modules": list(MODULES),
                "inputs": list(INPUTS),
                "cases": list(CASES),
                "medium_max_level": {
                    "dynamic_clarity": 4,
                    "smart_bass": 4,
                    "harshness_guard": 3,
                },
                "job_count": JOB_COUNT,
                "measured_count": MEASURED_COUNT,
                "warmup_count": 64,
                "first_call": first_calls,
                "warm_max": warm_maxes,
                "checksum": checksums,
                "state": state,
            }
            (result_dir / "harshness_guard_benchmark_board.json").write_text(
                json.dumps(board), encoding="ascii"
            )

            completed = subprocess.run(
                [sys.executable, "-B", str(ANALYZER), str(result_dir)],
                cwd=ROOT,
                check=True,
                text=True,
                capture_output=True,
            )
            self.assertIn('"pass": true', completed.stdout.lower())
            report = json.loads(
                (result_dir / "harshness_guard_benchmark.json").read_text(
                    encoding="ascii"
                )
            )
            board["state"]["harshness_saturation"] = 1
            (result_dir / "harshness_guard_benchmark_board.json").write_text(
                json.dumps(board), encoding="ascii"
            )
            safety_failure = subprocess.run(
                [sys.executable, "-B", str(ANALYZER), str(result_dir)],
                cwd=ROOT,
                check=False,
                text=True,
                capture_output=True,
            )

        self.assertTrue(report["pass"])
        self.assertEqual(len(report["jobs"]), JOB_COUNT)
        self.assertEqual(len(report["comparisons"]), len(INPUTS) * len(CASES))
        self.assertIsNone(report["performance_acceptance_threshold"])
        self.assertEqual(
            report["guard_safety_counters"],
            {"saturation": 0, "nonfinite": 0},
        )
        for row in report["jobs"]:
            for metric in (
                "min",
                "median",
                "p95",
                "p99",
                "max",
                "first",
                "warm_max",
            ):
                self.assertIn(metric, row)
        for comparison in report["comparisons"]:
            self.assertGreater(comparison["guard_to_dynamic_p99_ratio"], 0.0)
            self.assertGreater(comparison["guard_to_smart_p99_ratio"], 0.0)
            self.assertIn(
                comparison["guard_vs_dynamic_p99_significance"],
                {
                    "GUARD_HIGHER_APPROX_95_PERCENT",
                    "GUARD_LOWER_APPROX_95_PERCENT",
                    "NOT_SEPARATED_APPROX_95_PERCENT",
                },
            )
            self.assertIn(
                comparison["guard_vs_smart_p99_significance"],
                {
                    "GUARD_HIGHER_APPROX_95_PERCENT",
                    "GUARD_LOWER_APPROX_95_PERCENT",
                    "NOT_SEPARATED_APPROX_95_PERCENT",
                },
            )
        self.assertNotEqual(safety_failure.returncode, 0)
        self.assertIn(
            "non-zero benchmark safety counter: harshness_saturation",
            safety_failure.stderr,
        )


if __name__ == "__main__":
    unittest.main()
