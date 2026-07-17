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
        self.assertEqual(manifest["sample_rate"], 50_000)
        self.assertEqual(manifest["sample_count"], 512_000)
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
