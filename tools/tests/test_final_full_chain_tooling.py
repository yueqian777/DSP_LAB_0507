"""Static contract tests for the final C6748 full-chain board rerun tooling."""

from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[2]


class FinalFullChainToolingContractTest(unittest.TestCase):
    def test_dss_script_records_exact_integer_sources(self) -> None:
        script = (ROOT / "tools" / "dss" / "dss_board_missing_tests.js").read_text(
            encoding="utf-8"
        )
        for token in (
            "SUBBAND_CODEC_LOOP_DebugPayloadBitsPerHop",
            "SUBBAND_CODEC_LOOP_DebugScalarBitsPerHop",
            "SUBBAND_CODEC_LOOP_DebugScalarCountPerHop",
            "SUBBAND_CODEC_LOOP_DebugQuantizerClampCount",
            "SUBBAND_CODEC_LOOP_DebugTotalScalarCount",
            "SUBBAND_DENOISE_DebugGainAvgX1000000",
            "SUBBAND_DENOISE_DebugMcraSpeechProbAvgX1000000",
            "SUBBAND_DebugInputMeanSquareAvg",
            "SUBBAND_DebugOutputMeanSquareAvg",
            "board_missing_tests_rerun.csv",
        ):
            self.assertIn(token, script)
        self.assertNotIn(".delete()", script)
        self.assertIn('file["delete"]()', script)

    def test_c_headers_expose_read_only_diagnostic_mirrors(self) -> None:
        denoise_header = (ROOT / "Code" / "User" / "user_dsp" / "user_subband_denoise.h").read_text(
            encoding="utf-8"
        )
        codec_header = (ROOT / "Code" / "User" / "user_dsp" / "user_subband_codec_loopback.h").read_text(
            encoding="utf-8"
        )
        flow_header = (ROOT / "Code" / "User" / "user_dsp" / "user_subband_flow.h").read_text(
            encoding="utf-8"
        )
        self.assertIn("SUBBAND_DENOISE_DebugGainAvgX1000000", denoise_header)
        self.assertIn("SUBBAND_DENOISE_DebugMcraFloorAvgX1000000", denoise_header)
        self.assertIn("SUBBAND_CODEC_LOOP_DebugPayloadBitsPerHop", codec_header)
        self.assertIn("SUBBAND_DebugInputMeanSquareAvg", flow_header)

    def test_report_processor_refuses_invalid_repeats(self) -> None:
        processor = ROOT / "tools" / "final_full_chain_240_report.py"
        self.assertTrue(processor.exists())
        source = processor.read_text(encoding="utf-8")
        self.assertIn("NOT_MEASURED_AUDIO_CAPTURE_UNAVAILABLE", source)
        self.assertIn('CSV_PATH = OUT / "board_missing_tests_rerun.csv"', source)
        self.assertNotIn('CSV_PATH = OUT / "final_full_chain_240_rerun.csv"', source)
        self.assertIn("valid_repeat", source)
        self.assertIn("final_full_chain_240_diagnostics.csv", source)
        self.assertIn("Input-level diagnosis", source)
        self.assertIn("Audio-quality status: pending", source)


if __name__ == "__main__":
    unittest.main()
