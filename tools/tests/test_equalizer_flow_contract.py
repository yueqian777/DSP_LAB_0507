from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[2]
FLOW = ROOT / "Code/User/user_dsp/user_equalizer_flow.c"


class EqualizerFlowContractTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.source = FLOW.read_text(encoding="utf-8")

    def test_audio_work_precedes_low_priority_service(self) -> None:
        ad = self.source.index("if (FLAG_AD == 1)")
        da = self.source.index("if ((FLAG_DA == 1) && (flag_ad_done == 1))", ad)
        idle = self.source.index(
            "if ((FLAG_AD == 0) && (FLAG_DA == 0) && (flag_ad_done == 0))",
            da,
        )
        service = self.source.index("EQ_ServiceModeTimed();", idle)
        self.assertLess(ad, da)
        self.assertLess(da, idle)
        self.assertLess(idle, service)

    def test_frame_service_spans_ad_and_dac_active_segments(self) -> None:
        ad = self.source.index("if (FLAG_AD == 1)")
        da = self.source.index("if ((FLAG_DA == 1) && (flag_ad_done == 1))", ad)
        idle = self.source.index(
            "if ((FLAG_AD == 0) && (FLAG_DA == 0) && (flag_ad_done == 0))",
            da,
        )
        ad_body = self.source[ad:da]
        da_body = self.source[da:idle]
        self.assertIn("EQ_BeginFrameService();", ad_body)
        self.assertIn("EQ_CaptureAdcFrame();", ad_body)
        self.assertIn("EQ_FillDacInactiveBuffer();", da_body)
        self.assertIn("EQ_EndFrameService();", da_body)

    def test_deadline_uses_active_frame_service_budget(self) -> None:
        start = self.source.index("static void EQ_EndFrameService(void)")
        end = self.source.index("static void EQ_CaptureAdcFrame(void)", start)
        body = self.source[start:end]
        self.assertIn(
            "EQ_FrameActiveServiceCycles > EQ_FRAME_SERVICE_BUDGET_CYCLES",
            body,
        )
        self.assertNotIn("latency_cycles > EQ_FRAME_SERVICE_BUDGET_CYCLES", body)

    def test_ti_timer_uses_unsigned_tscl_delta_only(self) -> None:
        self.assertIn("TSCL = 0;", self.source)
        self.assertNotIn("TSCH =", self.source)


if __name__ == "__main__":
    unittest.main()
