import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
FLOW = (ROOT / "Code/User/user_dsp/user_subband_flow.c").read_text(
    encoding="utf-8"
)
DENOISE_H = (ROOT / "Code/User/user_dsp/user_subband_denoise.h").read_text(
    encoding="utf-8"
)
DENOISE_C = (ROOT / "Code/User/user_dsp/user_subband_denoise.c").read_text(
    encoding="utf-8"
)
APPLY_MODE = re.search(
    r"static void Subband_Apply_Demo_Mode\(int mode\)\s*\{(.*?)\n\}",
    FLOW,
    re.DOTALL,
)
if APPLY_MODE is None:
    raise AssertionError("missing Subband_Apply_Demo_Mode")
APPLY_MODE_BODY = APPLY_MODE.group(1)


def case_body(case_name: str, next_case_name: str) -> str:
    match = re.search(
        rf"case\s+{case_name}\s*:(.*?)(?=case\s+{next_case_name}\s*:)",
        APPLY_MODE_BODY,
        re.DOTALL,
    )
    if match is None:
        raise AssertionError(f"missing switch range for {case_name}")
    return match.group(1)


class McraTonalGuardContractTests(unittest.TestCase):
    def test_mode6_applies_only_requested_overrides_after_reset(self):
        body = case_body(
            "SUBBAND_DEMO_MODE_MCRA_DENOISE",
            "SUBBAND_DEMO_MODE_STRONG_MCRA_DENOISE",
        )
        reset = body.index("SubbandDenoise_Reset();")
        smooth = body.index(
            "SubbandDenoise_SetParams(0.96f, 0.15f, 0.85f, 0.80f);"
        )
        tonal = body.index(
            "SubbandDenoise_SetMcraTonalGuardParams(5.0f, 1.80f, 0.28f);"
        )
        self.assertLess(reset, smooth)
        self.assertLess(smooth, tonal)
        self.assertRegex(
            body,
            r"SubbandDenoise_SetMcraParams\(1\.5f,\s*4\.0f,\s*"
            r"0\.85f,\s*0\.998f,\s*1\.10f,\s*1\.70f,\s*"
            r"1\.40f,\s*0\);",
        )

    def test_mode8_relies_on_reset_defaults(self):
        body = case_body(
            "SUBBAND_DEMO_MODE_MCRA_CODEC_LOOPBACK",
            "SUBBAND_DEMO_MODE_STRONG_MCRA_CODEC_LOOPBACK",
        )
        self.assertIn("SubbandDenoise_Reset();", body)
        self.assertNotIn("SubbandDenoise_SetParams(", body)
        self.assertNotIn("SubbandDenoise_SetMcraTonalGuardParams(", body)
        self.assertRegex(
            body,
            r"SubbandDenoise_SetMcraParams\(1\.5f,\s*4\.0f,\s*"
            r"0\.85f,\s*0\.998f,\s*1\.10f,\s*1\.70f,\s*"
            r"1\.40f,\s*0\);",
        )

    def test_public_diagnostics_and_setter_are_declared(self):
        self.assertIn("SubbandDenoise_SetMcraTonalGuardParams", DENOISE_H)
        self.assertIn("SUBBAND_DENOISE_DebugMcraTonalGuardHits", DENOISE_H)
        self.assertIn(
            "SUBBAND_DENOISE_DebugMcraTonalGuardBinsLastFrame", DENOISE_H
        )

    def test_reset_restores_legacy_defaults_and_clears_counters(self):
        self.assertIn("#define SUBBAND_DENOISE_GAIN_SMOOTH_DOWN 0.60f", DENOISE_H)
        self.assertIn("mcra_tonal_snr_min = 3.0f;", DENOISE_C)
        self.assertIn("mcra_tonal_neighbor_ratio = 1.35f;", DENOISE_C)
        self.assertIn("mcra_tonal_floor = 0.45f;", DENOISE_C)
        reset = re.search(
            r"void SubbandDenoise_Reset\(void\)\s*\{(.*?)\n\}",
            DENOISE_C,
            re.DOTALL,
        )
        self.assertIsNotNone(reset)
        reset_body = reset.group(1)
        self.assertIn("SUBBAND_DENOISE_DebugMcraTonalGuardHits = 0UL;", reset_body)
        self.assertIn(
            "SUBBAND_DENOISE_DebugMcraTonalGuardBinsLastFrame = 0UL;",
            reset_body,
        )
        self.assertLess(
            reset_body.index("SUBBAND_DENOISE_DebugMcraTonalGuardHits = 0UL;"),
            reset_body.index("SubbandDenoise_Init();"),
        )

    def test_counter_lifecycle_is_diagnostic_only(self):
        self.assertEqual(
            DENOISE_C.count("SUBBAND_DENOISE_DebugMcraTonalGuardHits++;"), 1
        )
        self.assertEqual(
            DENOISE_C.count(
                "SUBBAND_DENOISE_DebugMcraTonalGuardBinsLastFrame++;"
            ),
            1,
        )
        process = re.search(
            r"void SubbandDenoise_ProcessSpectrum\(float \*re, float \*im\)"
            r"\s*\{(.*)",
            DENOISE_C,
            re.DOTALL,
        )
        self.assertIsNotNone(process)
        process_body = process.group(1)
        self.assertLess(
            process_body.index(
                "SUBBAND_DENOISE_DebugMcraTonalGuardBinsLastFrame = 0UL;"
            ),
            process_body.index("SubbandDenoise_Init();"),
        )


if __name__ == "__main__":
    unittest.main()
