from __future__ import annotations

import json
import subprocess
import sys
import tempfile
import unittest
import wave
from pathlib import Path

import numpy as np


ROOT = Path(__file__).resolve().parents[2]
GENERATOR = ROOT / "tools" / "generate_dynamic_clarity_audio.py"
DSS = ROOT / "tools" / "dss" / "dss_dynamic_clarity_board.js"
RUNNER = ROOT / "tools" / "run_dynamic_clarity_board.ps1"
ANALYZER = ROOT / "tools" / "analyze_dynamic_clarity_raw.py"
BENCHMARK_DSS = ROOT / "tools" / "dss" / "dss_dynamic_clarity_benchmark.js"
BENCHMARK_RUNNER = ROOT / "tools" / "run_dynamic_clarity_benchmark.ps1"
BENCHMARK_ANALYZER = ROOT / "tools" / "analyze_dynamic_clarity_benchmark.py"
TRANSITION_DSS = ROOT / "tools" / "dss" / "dss_dynamic_clarity_transition.js"
TRANSITION_RUNNER = ROOT / "tools" / "run_dynamic_clarity_transition.ps1"
TRANSITION_ANALYZER = ROOT / "tools" / "analyze_dynamic_clarity_transition.py"
CONTINUOUS_DSS = ROOT / "tools" / "dss" / "dss_dynamic_clarity_continuous.js"
CONTINUOUS_RUNNER = ROOT / "tools" / "run_dynamic_clarity_continuous.ps1"


class DynamicClarityHardwareRunnerTest(unittest.TestCase):
    @staticmethod
    def _masking_db(path: Path) -> float:
        with wave.open(str(path), "rb") as source:
            samples = np.frombuffer(source.readframes(1024), dtype="<i2").astype(
                np.float64
            )
        power = np.abs(np.fft.rfft(samples * np.hanning(1024))) ** 2
        frequencies = np.fft.rfftfreq(1024, 1.0 / 50_000.0)
        mud = np.mean(power[(frequencies >= 200.0) & (frequencies < 800.0)])
        presence = np.mean(
            power[(frequencies >= 800.0) & (frequencies < 4000.0)]
        )
        return float(10.0 * np.log10(mud / presence))

    def test_fixed_stimuli_are_headroom_safe(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            subprocess.run(
                [sys.executable, str(GENERATOR), directory],
                check=True,
                cwd=ROOT,
            )
            manifest = json.loads(
                (Path(directory) / "audio_manifest.json").read_text(
                    encoding="ascii"
                )
            )
            for target in manifest["masking_targets_db"]:
                measured = self._masking_db(
                    Path(directory) / f"mask{int(target):02d}.wav"
                )
                self.assertAlmostEqual(measured, target, delta=0.05)
            with wave.open(
                str(Path(directory) / "transition_noise.wav"), "rb"
            ) as source:
                noise = np.frombuffer(
                    source.readframes(source.getnframes()), dtype="<i2"
                )
        self.assertEqual(manifest["sample_rate"], 50_000)
        self.assertEqual(manifest["sample_count"], 512_000)
        self.assertEqual(manifest["transition_noise_period_samples"], 1024)
        np.testing.assert_array_equal(noise[:-1024], noise[1024:])
        self.assertEqual(manifest["masking_targets_db"], [2, 5, 8, 12, 16])
        self.assertTrue(all(entry["peak"] < 0.98 for entry in manifest["files"]))
        self.assertEqual(
            {entry["name"] for entry in manifest["files"]},
            {
                "presence",
                "mud",
                "mask02",
                "mask05",
                "mask08",
                "mask12",
                "mask16",
                "weak_mud",
                "weak_presence",
                "combined",
                "combined_transition",
                "probe100",
                "probe8k",
                "music_like",
                "transition_dual",
                "transition_music",
                "transition_noise",
            },
        )

    def test_dss_contract_is_objective_only(self) -> None:
        source = DSS.read_text(encoding="ascii")
        for symbol in (
            "EQ_DebugDynamicClarityCompiled",
            "EQ_DebugDynamicClarityEnabled",
            "EQ_DebugDynamicClarityMaskingDb",
            "EQ_DebugDynamicClarityAppliedLevel",
            "EQ_DebugDynamicClarityMaxCycles",
            "EQ_DebugDynamicClaritySaturationCount",
            "EQ_DebugDynamicClarityNonFiniteCount",
        ):
            self.assertIn(symbol, source)
        self.assertIn('subjective_observation = "NOT_PERFORMED"', source)
        self.assertIn('label: "CONTINUOUS_300S"', source)
        self.assertIn("Thread.sleep(300000)", source)
        self.assertNotIn("PENDING_OPERATOR", source)
        self.assertNotIn("D_LISTEN", source)

    def test_isolated_benchmark_contract(self) -> None:
        dss = BENCHMARK_DSS.read_text(encoding="ascii")
        runner = BENCHMARK_RUNNER.read_text(encoding="ascii")
        analyzer = BENCHMARK_ANALYZER.read_text(encoding="utf-8")
        self.assertIn("EQ_DebugDynamicClarityBenchmarkReady", dss)
        self.assertIn("debugSession.target.run();", dss)
        self.assertNotIn("runFor(", dss)
        self.assertIn("dynamic_clarity_benchmark_cycles.raw", dss)
        self.assertIn("MEASURED_COUNT = 4096", analyzer)
        self.assertIn('"first_call"', analyzer)
        self.assertIn('"warm_max"', analyzer)
        self.assertIn("EQ_ENABLE_DYNAMIC_CLARITY_BENCHMARK=1", runner)
        self.assertIn('subjective_observation: "NOT_PERFORMED"', dss)

    def test_transition_capture_contract(self) -> None:
        dss = TRANSITION_DSS.read_text(encoding="ascii")
        runner = TRANSITION_RUNNER.read_text(encoding="ascii")
        analyzer = TRANSITION_ANALYZER.read_text(encoding="utf-8")
        for transition in (
            "transition_0_to_1",
            "transition_1_to_2",
            "transition_1_to_0",
        ):
            self.assertIn(transition, dss)
        self.assertIn("EQ_TriggerCapturePreWriteIndex", dss)
        self.assertIn("inputPath, 6144, 16, false", dss)
        self.assertIn("EQ_ENABLE_DYNAMIC_CLARITY_TIMING_DIAGNOSTICS=1", runner)
        self.assertIn("EQ_ENABLE_DYNAMIC_CLARITY_TRANSITION_CAPTURE=1", runner)
        self.assertIn("BOARD_INTERNAL_PCM_OBJECTIVE_TRANSIENT", analyzer)
        self.assertIn("hf_boundary_to_internal_ratio", analyzer)
        self.assertIn("repeated_adjacent_frame_count", analyzer)
        self.assertIn('"NOT_PERFORMED"', analyzer)
        for forbidden in (
            "inaudible",
            "no audible click",
            "sounds smooth",
            "perceptually transparent",
        ):
            self.assertNotIn(forbidden, analyzer.lower())

    def test_continuous_runner_has_uninterrupted_300s_contract(self) -> None:
        dss = CONTINUOUS_DSS.read_text(encoding="ascii")
        runner = CONTINUOUS_RUNNER.read_text(encoding="ascii")
        self.assertIn("RUN_WINDOW_MILLISECONDS = 303000", dss)
        self.assertIn("REQUIRED_DSP_SECONDS = 300.0", dss)
        self.assertIn("debugger_access_during_window: \"NONE\"", dss)
        self.assertIn("watch_reads_during_window: 0", dss)
        self.assertIn("raw_reads_during_window: 0", dss)
        self.assertIn("intermediate_dss_access_during_window: 0", dss)
        self.assertIn("frameDelta * FRAME_LEN / SAMPLE_RATE_HZ", dss)
        self.assertIn("System.nanoTime()", dss)
        for field in (
            "host_monotonic_start",
            "host_monotonic_end",
            "host_elapsed_seconds",
            "dsp_start_frame",
            "dsp_end_frame",
            "dsp_frame_delta",
            "dsp_elapsed_seconds",
            "target_seconds",
            "start_uart_timestamp",
            "end_uart_timestamp",
            "TARGET_WALL_TIME_SECONDS",
            "MEASURED_HOST_ELAPSED_SECONDS",
            "MEASURED_DSP_AUDIO_SECONDS",
        ):
            self.assertIn(field, dss)
        self.assertIn("Analyzer did not advance during the window", dss)
        self.assertIn("Smart Bass did not advance during the window", dss)
        self.assertIn("Dynamic Clarity did not advance during the window", dss)
        self.assertIn("EQ_ENABLE_LCD_DISPLAY=0", runner)
        self.assertIn("EQ_ENABLE_AUDIO_FEATURE_ANALYZER=1", runner)
        self.assertIn("EQ_ENABLE_SMART_BASS=1", runner)
        self.assertIn("EQ_ENABLE_DYNAMIC_CLARITY=1", runner)
        measurement = dss.split(
            "/* Measurement window: intentionally no expression, Watch, or RAW access. */",
            1,
        )[1].split("finalMonotonic =", 1)[0]
        self.assertEqual(measurement.count("target.runAsynch()"), 1)
        self.assertEqual(measurement.count("target.halt()"), 1)
        self.assertNotIn("numberValue(", measurement)
        self.assertNotIn("evaluate(", measurement)

    def test_runner_requires_exact_sha_and_temp_evidence(self) -> None:
        source = RUNNER.read_text(encoding="ascii")
        analyzer = ANALYZER.read_text(encoding="ascii")
        self.assertIn('[string]$ExpectedSha = "47337a0"', source)
        self.assertIn("ResultDir must stay under the current TEMP directory", source)
        self.assertIn("[switch]$AnalyzeOnly", source)
        self.assertIn("analyze_dynamic_clarity_raw.py", source)
        self.assertIn("Runtime OFF capture is not byte-exact", analyzer)
        self.assertIn("Presence identity capture is not byte-exact", analyzer)
        self.assertIn('subjective_observation = "NOT_PERFORMED"', source)


if __name__ == "__main__":
    unittest.main()
