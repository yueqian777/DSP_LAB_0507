from __future__ import annotations

import pathlib
import subprocess
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[2]
CORE = ROOT / "Code/User/user_dsp/user_equalizer.c"
CONTROL = ROOT / "Code/User/user_dsp/user_equalizer_control.c"
FLOW = ROOT / "Code/User/user_dsp/user_equalizer_flow.c"
HEADER = ROOT / "Code/User/user_dsp/user_equalizer_control.h"
RESPONSE = ROOT / "Code/User/user_dsp/user_equalizer_response.c"
EVALUATOR = ROOT / "Code/User/user_dsp/user_equalizer_eval.c"
BASH = pathlib.Path(r"C:\msys64\usr\bin\bash.exe")


def run_msys(command: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [str(BASH), "-lc", f"export PATH=/mingw64/bin:/usr/bin:$PATH; "
         f"cd /c/Users/zhangyueqian/lab8/DSP_LAB_0507; {command}"],
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        check=False,
    )


class EqualizerControlTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.core = CORE.read_text(encoding="utf-8")
        cls.control = CONTROL.read_text(encoding="utf-8")
        cls.flow = FLOW.read_text(encoding="utf-8")
        cls.header = HEADER.read_text(encoding="utf-8")
        cls.evaluator = EVALUATOR.read_text(encoding="utf-8")

    def test_host_safe_boundary_defers_finalize_install(self) -> None:
        start = self.evaluator.index("static void EQ_EvalControlService(")
        end = self.evaluator.index("static void EQ_EvalControlSettle", start)
        service = self.evaluator[start:end]
        self.assertEqual(service.count(
            "EqualizerControl_TryInstallReady("), 1)
        self.assertLess(service.index("EqualizerControl_TryInstallReady("),
                        service.index(
                            "EqualizerControl_ServiceOneBuilderSlice("))

    def test_host_harness(self) -> None:
        result = run_msys(
            "gcc -std=c99 -Wall -Wextra -Werror -DEQ_ALGO_ONLY "
            "-ICode/User/user_dsp "
            "Code/User/user_dsp/user_equalizer.c "
            "Code/User/user_dsp/user_equalizer_control.c "
            "Code/User/user_dsp/user_equalizer_response.c "
            "Code/User/user_dsp/user_equalizer_flow.c "
            "tools/tests/equalizer_control_test.c -lm "
            "-o /tmp/equalizer_control_test.exe && "
            "/tmp/equalizer_control_test.exe"
        )
        self.assertEqual(result.returncode, 0, result.stdout)
        self.assertIn("failures=0", result.stdout)

    def test_host_harness_analyzer_compiled_on(self) -> None:
        result = run_msys(
            "gcc -std=c99 -Wall -Wextra -Werror -DEQ_ALGO_ONLY "
            "-DEQ_ENABLE_AUDIO_FEATURE_ANALYZER=1 "
            "-ICode/User/user_dsp "
            "Code/User/user_dsp/user_equalizer.c "
            "Code/User/user_dsp/user_equalizer_control.c "
            "Code/User/user_dsp/user_equalizer_response.c "
            "Code/User/user_dsp/user_equalizer_flow.c "
            "tools/tests/equalizer_control_test.c -lm "
            "-o /tmp/equalizer_control_analyzer_on_test.exe && "
            "/tmp/equalizer_control_analyzer_on_test.exe"
        )
        self.assertEqual(result.returncode, 0, result.stdout)
        self.assertIn("failures=0", result.stdout)

    def test_mailbox_layout_and_single_attempt_reader(self) -> None:
        mailbox = self.header[self.header.index("typedef struct\n{"):]
        self.assertLess(mailbox.index("volatile EQ_CONTROL_SEQUENCE sequence"),
                        mailbox.index("volatile int command"))
        service_start = self.control.index("int EqualizerControl_ServiceMailbox(")
        service_end = self.control.index(
            "void EqualizerControl_InvalidateStaleWork", service_start)
        service = self.control[service_start:service_end]
        self.assertEqual(service.count("seq_before = mailbox->sequence;"), 1)
        self.assertEqual(service.count("seq_after = mailbox->sequence;"), 1)
        self.assertNotIn("while (", service)

    def test_builder_is_exactly_bounded(self) -> None:
        start = self.control.index("int EqualizerControl_ServiceOneBuilderSlice(")
        end = self.control.index("int EqualizerControl_TryInstallReady", start)
        builder = self.control[start:end]
        self.assertIn("if (points > 16)", builder)
        self.assertIn("points = 16;", builder)
        self.assertNotIn("while (", builder)
        self.assertEqual(builder.count("payload_slice_count++"), 3)

    def test_final_audio_state_is_rechecked_by_shared_decision(self) -> None:
        header = (ROOT / "Code/User/user_dsp/user_equalizer_flow.h").read_text(
            encoding="utf-8")
        signature = header[header.index("int EqualizerBackgroundService_Decide("):]
        for field in (
            "int final_flag_ad",
            "int final_flag_da",
            "int final_flag_ad_done",
            "int final_frame_service_pending",
        ):
            self.assertIn(field, signature)
        loop = self.flow[self.flow.index("while (1)"):]
        decide = loop.index("EqualizerBackgroundService_Decide(")
        builder = loop.index("EqualizerControl_ServiceOneBuilderSlice(", decide)
        lcd = loop.index("EqualizerDisplay_ServiceOneJob();", decide)
        decision_call = loop[decide:builder]
        for field in ("FLAG_AD", "FLAG_DA", "flag_ad_done",
                      "EQ_FrameServicePending"):
            self.assertIn(field, decision_call)
        self.assertLess(decide, builder)
        self.assertLess(decide, lcd)
        self.assertIn("EQ_DebugBuilderDeferredAudioCount++", loop)
        self.assertIn("EQ_DebugBuilderAudioArrivedDuringSliceCount++", loop)

    def test_sample_path_cannot_start_or_build_target(self) -> None:
        start = self.core.index("static float EQ_ProcessRbjSample(")
        end = self.core.index("static short EQ_SaturateToShort", start)
        sample_path = self.core[start:end]
        for forbidden in (
            "EQ_StartCurrentTarget",
            "EQ_BuildRbjBank",
            "EqualizerControl_",
            "EqualizerResponse_",
        ):
            self.assertNotIn(forbidden, sample_path)

    def test_safe_boundary_order_and_shared_budget(self) -> None:
        loop = self.flow[self.flow.index("while (1)"):]
        observe = loop.index("EqualizerControl_ObserveFrameBoundary(")
        mailbox = loop.index("EqualizerControl_ServiceMailbox(", observe)
        stale = loop.index("EqualizerControl_InvalidateStaleWork(", mailbox)
        install = loop.index("EqualizerControl_TryInstallReady(", stale)
        decide = loop.index("EqualizerBackgroundService_Decide(", install)
        self.assertLess(observe, mailbox)
        self.assertLess(mailbox, stale)
        self.assertLess(stale, install)
        self.assertLess(install, decide)
        self.assertEqual(loop.count(
            "EqualizerControl_ServiceOneBuilderSlice("), 1)
        self.assertEqual(loop.count("EqualizerDisplay_ServiceOneJob();"), 1)
        self.assertLess(decide, loop.index(
            "EqualizerControl_ServiceOneBuilderSlice(", decide))
        self.assertLess(decide, loop.index(
            "EqualizerDisplay_ServiceOneJob();", decide))

    def test_post_start_has_no_direct_rbj_target_setter(self) -> None:
        post_start = self.flow[self.flow.index("Adc_Start();"):]
        for forbidden in (
            "Equalizer_ApplyPreset(",
            "Equalizer_ApplySingleBand1kPlus3Db(",
            "Equalizer_SetBandGainDb(",
            "Equalizer_SetAllGainsDb(",
        ):
            self.assertNotIn(forbidden, post_start)

    def test_cache_prewarm_precedes_audio_start(self) -> None:
        init = self.flow.index("Equalizer_Init(&EQ_BoardState);")
        adc_start = self.flow.index("Adc_Start();", init)
        dac_start = self.flow.index("Dac_Start();", init)
        self.assertLess(init, adc_start)
        self.assertLess(init, dac_start)


if __name__ == "__main__":
    unittest.main()
