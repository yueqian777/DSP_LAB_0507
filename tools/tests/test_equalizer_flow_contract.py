from pathlib import Path
import os
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
FLOW = ROOT / "Code/User/user_dsp/user_equalizer_flow.c"
HEADER = ROOT / "Code/User/user_dsp/user_equalizer_flow.h"


POLICY_HARNESS = r'''
#include "user_equalizer_flow.h"
#include <limits.h>

static int require_true(int condition, int code)
{
    return condition ? 0 : code;
}

int main(void)
{
    EQ_LCD_SERVICE_POLICY policy;
    EQ_LCD_FAULT_POLICY fault_policy;
    unsigned long reason;
    int rc;

    EqualizerLcdPolicy_Init(&policy);

    /* The first safe frame may service one pending job. */
    rc = require_true(EqualizerLcdPolicy_CanService(
        &policy, 0UL, 0, 0, 0, 0, 0, 0, 0, 0, 1), 1);
    if (rc != 0) return rc;

    /* Every audio/background guard and an empty queue reject service. */
    rc = require_true(!EqualizerLcdPolicy_CanService(
        &policy, 1UL, 1, 0, 0, 0, 0, 0, 0, 0, 1), 2);
    if (rc != 0) return rc;
    rc = require_true(!EqualizerLcdPolicy_CanService(
        &policy, 1UL, 0, 1, 0, 0, 0, 0, 0, 0, 1), 3);
    if (rc != 0) return rc;
    rc = require_true(!EqualizerLcdPolicy_CanService(
        &policy, 1UL, 0, 0, 1, 0, 0, 0, 0, 0, 1), 4);
    if (rc != 0) return rc;
    rc = require_true(!EqualizerLcdPolicy_CanService(
        &policy, 1UL, 0, 0, 0, 1, 0, 0, 0, 0, 1), 5);
    if (rc != 0) return rc;
    rc = require_true(!EqualizerLcdPolicy_CanService(
        &policy, 1UL, 0, 0, 0, 0, 1, 0, 0, 0, 1), 6);
    if (rc != 0) return rc;
    rc = require_true(!EqualizerLcdPolicy_CanService(
        &policy, 1UL, 0, 0, 0, 0, 0, 1, 0, 0, 1), 7);
    if (rc != 0) return rc;
    rc = require_true(!EqualizerLcdPolicy_CanService(
        &policy, 1UL, 0, 0, 0, 0, 0, 0, 1, 0, 1), 8);
    if (rc != 0) return rc;
    rc = require_true(!EqualizerLcdPolicy_CanService(
        &policy, 1UL, 0, 0, 0, 0, 0, 0, 0, 1, 1), 9);
    if (rc != 0) return rc;
    rc = require_true(!EqualizerLcdPolicy_CanService(
        &policy, 1UL, 0, 0, 0, 0, 0, 0, 0, 0, 0), 10);
    if (rc != 0) return rc;

    /* A final pre-draw audio snapshot can still defer an eligible job. */
    rc = require_true(EqualizerLcdPolicy_CanService(
        &policy, 1UL, 0, 0, 0, 0, 0, 0, 0, 0, 1), 11);
    if (rc != 0) return rc;
    rc = require_true(EqualizerLcdPolicy_Decide(
        &policy, 1UL,
        0, 0, 0, 0,
        1, 0, 0, 0,
        0, 0, 0, 0,
        1) == EQ_LCD_POLICY_DEFER, 12);
    if (rc != 0) return rc;
    rc = require_true(EqualizerLcdPolicy_Decide(
        &policy, 1UL,
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0,
        1) == EQ_LCD_POLICY_SERVICE, 13);
    if (rc != 0) return rc;

    /* A completed job leaves one whole processed frame between jobs. */
    EqualizerLcdPolicy_RecordService(&policy, 1UL, 1);
    rc = require_true(!EqualizerLcdPolicy_CanService(
        &policy, 1UL, 0, 0, 0, 0, 0, 0, 0, 0, 1), 14);
    if (rc != 0) return rc;
    rc = require_true(!EqualizerLcdPolicy_CanService(
        &policy, 2UL, 0, 0, 0, 0, 0, 0, 0, 0, 1), 15);
    if (rc != 0) return rc;
    rc = require_true(EqualizerLcdPolicy_CanService(
        &policy, 3UL, 0, 0, 0, 0, 0, 0, 0, 0, 1), 16);
    if (rc != 0) return rc;

    /* Deferred accounting is latched once per processed frame. */
    rc = require_true(EqualizerLcdPolicy_RecordDeferred(&policy, 3UL), 17);
    if (rc != 0) return rc;
    rc = require_true(!EqualizerLcdPolicy_RecordDeferred(&policy, 3UL), 18);
    if (rc != 0) return rc;
    rc = require_true(EqualizerLcdPolicy_RecordDeferred(&policy, 4UL), 19);
    if (rc != 0) return rc;

    /* A touch/control action reserves three complete quiet frames. */
    EqualizerLcdPolicy_RecordControlChange(&policy, 10UL);
    rc = require_true(!EqualizerLcdPolicy_CanService(
        &policy, 10UL, 0, 0, 0, 0, 0, 0, 0, 0, 1), 20);
    if (rc != 0) return rc;
    rc = require_true(!EqualizerLcdPolicy_CanService(
        &policy, 11UL, 0, 0, 0, 0, 0, 0, 0, 0, 1), 21);
    if (rc != 0) return rc;
    rc = require_true(!EqualizerLcdPolicy_CanService(
        &policy, 12UL, 0, 0, 0, 0, 0, 0, 0, 0, 1), 22);
    if (rc != 0) return rc;
    rc = require_true(EqualizerLcdPolicy_CanService(
        &policy, 13UL, 0, 0, 0, 0, 0, 0, 0, 0, 1), 23);
    if (rc != 0) return rc;

    /* Fault decisions refresh all baselines on every call. */
    EqualizerLcdFaultPolicy_Init(&fault_policy);
    reason = EqualizerLcdFaultPolicy_Monitor(&fault_policy, 1, 1UL, 0UL, 0UL);
    rc = require_true(reason == EQ_LCD_FAULT_LATENCY_MISS, 50);
    if (rc != 0) return rc;
    reason = EqualizerLcdFaultPolicy_Monitor(&fault_policy, 1, 1UL, 1UL, 0UL);
    rc = require_true(reason == EQ_LCD_FAULT_OVERLAP, 51);
    if (rc != 0) return rc;
    reason = EqualizerLcdFaultPolicy_Monitor(&fault_policy, 1, 2UL, 2UL, 1UL);
    rc = require_true(reason == (EQ_LCD_FAULT_LATENCY_MISS |
                                 EQ_LCD_FAULT_OVERLAP |
                                 EQ_LCD_FAULT_DROPPED), 52);
    if (rc != 0) return rc;
    rc = require_true((fault_policy.previous_latency_misses == 2UL) &&
                      (fault_policy.previous_overlaps == 2UL) &&
                      (fault_policy.previous_dropped == 1UL), 53);
    if (rc != 0) return rc;

    /* Disabled-time faults become the baseline and cannot fire after enable. */
    reason = EqualizerLcdFaultPolicy_Monitor(&fault_policy, 0, 7UL, 8UL, 9UL);
    rc = require_true(reason == 0UL, 54);
    if (rc != 0) return rc;
    rc = require_true((fault_policy.previous_latency_misses == 7UL) &&
                      (fault_policy.previous_overlaps == 8UL) &&
                      (fault_policy.previous_dropped == 9UL), 55);
    if (rc != 0) return rc;
    reason = EqualizerLcdFaultPolicy_Monitor(&fault_policy, 1, 7UL, 8UL, 9UL);
    rc = require_true(reason == 0UL, 56);
    if (rc != 0) return rc;
    reason = EqualizerLcdFaultPolicy_Monitor(&fault_policy, 1, 7UL, 8UL, 10UL);
    rc = require_true(reason == EQ_LCD_FAULT_DROPPED, 57);
    if (rc != 0) return rc;

    /* Unsigned counter wrap is a new event, and the new zero is the baseline. */
    EqualizerLcdFaultPolicy_Init(&fault_policy);
    reason = EqualizerLcdFaultPolicy_Monitor(
        &fault_policy, 0, ULONG_MAX, ULONG_MAX, ULONG_MAX);
    rc = require_true(reason == 0UL, 58);
    if (rc != 0) return rc;
    reason = EqualizerLcdFaultPolicy_Monitor(&fault_policy, 1, 0UL, 0UL, 0UL);
    rc = require_true(reason == (EQ_LCD_FAULT_LATENCY_MISS |
                                 EQ_LCD_FAULT_OVERLAP |
                                 EQ_LCD_FAULT_DROPPED), 59);
    if (rc != 0) return rc;
    reason = EqualizerLcdFaultPolicy_Monitor(&fault_policy, 1, 0UL, 0UL, 0UL);
    return require_true(reason == 0UL, 60);
}
'''


class EqualizerFlowContractTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.source = FLOW.read_text(encoding="utf-8")
        cls.header = HEADER.read_text(encoding="utf-8")

    def _runtime_loop(self) -> str:
        start = self.source.index("while (1)")
        return self.source[start:]

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

    def test_uart_diagnostics_follow_guide_without_frame_printf(self) -> None:
        self.assertIn("#define EQ_ENABLE_UART_DIAGNOSTICS 1", self.source)
        self.assertIn("Uart2_Init_Lite(UART_BAUD_115200);", self.source)
        self.assertIn(
            'UARTPuts("P33 UART 115200 8N1\\r\\n", -1);',
            self.source,
        )
        self.assertNotIn("UARTprintf(", self.source)

        loop = self._runtime_loop()
        end_frame = loop.index("EQ_EndFrameService();")
        stage_11 = loop.index("EQ_UartReportStage(11UL);", end_frame)
        idle_service = loop.index(
            "if ((FLAG_AD == 0) && (FLAG_DA == 0) && (flag_ad_done == 0))"
        )
        self.assertLess(end_frame, stage_11)
        self.assertLess(stage_11, idle_service)

        for stage in range(1, 9):
            self.assertIn(f"EQ_UartReportStage({stage}UL);", self.source)

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

    def test_capture_is_ch1_only_and_inside_active_service(self) -> None:
        start = self.source.index("static void EQ_CaptureAdcFrame(void)")
        end = self.source.index("static void EQ_FillDacPingBuffer(void)", start)
        body = self.source[start:end]
        process = body.index("Equalizer_ProcessFrame(")
        timing_end = body.index("EQ_UpdateAlgoTiming", process)
        timing_branch_end = body.index("#endif", timing_end)
        capture = body.index(
            "EqualizerCapture_OnFrame(EQ_AD_Buffer1, EQ_DA_Buffer1);",
            timing_branch_end,
        )
        self.assertLess(process, timing_end)
        self.assertLess(timing_branch_end, capture)
        capture_call = body[capture:]
        for channel in range(2, 9):
            self.assertNotIn(f"EQ_AD_Buffer{channel}", capture_call)

    def test_capture_request_acceptance_uses_stable_snapshots(self) -> None:
        start = self.source.index("static void EQ_CaptureAcceptRequest(void)")
        end = self.source.index("void EqualizerCapture_OnFrame", start)
        body = self.source[start:end]
        self.assertIn(
            "manual_request = EQ_CaptureManualRequest;", body
        )
        self.assertIn(
            "trigger_request = EQ_TriggerCaptureRequest;", body
        )
        self.assertIn(
            "EQ_TriggerCaptureArmedSource = trigger_request;", body
        )
        self.assertEqual(body.count("EQ_TriggerCaptureRequest"), 4)

    def test_capture_events_precede_the_next_processed_frame(self) -> None:
        loop = self._runtime_loop()
        lcd_service = loop.index("EqualizerDisplay_ServiceOneJob(")
        lcd_notify = loop.index(
            "EQ_CAPTURE_TRIGGER_LCD_JOB", lcd_service
        )
        audio_notify = loop.index(
            "EQ_CAPTURE_TRIGGER_AUDIO_DURING_LCD", lcd_service
        )
        self.assertGreater(lcd_notify, lcd_service)
        self.assertGreater(audio_notify, lcd_service)
        process = self.source.index("Equalizer_ProcessFrame(&EQ_BoardState")
        mode_before = self.source.rfind("mode_change_before =", 0, process)
        mode_notify = self.source.index(
            "EqualizerCapture_NotifyModeChange(", process
        )
        on_frame = self.source.index("EqualizerCapture_OnFrame(", mode_notify)
        self.assertLess(mode_before, process)
        self.assertLess(process, mode_notify)
        self.assertLess(mode_notify, on_frame)
        self.assertNotIn("EQ_ApplyPresetAndNotify", self.source)

    def test_deadline_uses_active_frame_service_budget(self) -> None:
        start = self.source.index("static void EQ_EndFrameService(void)")
        end = self.source.index("static void EQ_CaptureAdcFrame(void)", start)
        body = self.source[start:end]
        self.assertIn(
            "EQ_FrameActiveServiceCycles > EQ_FRAME_SERVICE_BUDGET_CYCLES",
            body,
        )
        active_check = body.index(
            "EQ_FrameActiveServiceCycles > EQ_FRAME_SERVICE_BUDGET_CYCLES"
        )
        active_counter = body.index("EQ_DebugDeadlineMissCount++", active_check)
        latency_check = body.index(
            "latency_cycles > EQ_FRAME_SERVICE_BUDGET_CYCLES", active_counter
        )
        self.assertLess(active_counter, latency_check)

    def test_latency_has_its_own_deadline_counter(self) -> None:
        self.assertIn("EQ_DebugFrameLatencyDeadlineMissCount", self.header)
        start = self.source.index("static void EQ_EndFrameService(void)")
        end = self.source.index("static void EQ_CaptureAdcFrame(void)", start)
        body = self.source[start:end]
        self.assertIn("latency_cycles > EQ_FRAME_SERVICE_BUDGET_CYCLES", body)
        self.assertIn("EQ_DebugFrameLatencyDeadlineMissCount++", body)

    def test_static_page_is_completed_before_audio_starts(self) -> None:
        init = self.source.index("EqualizerDisplay_Init();")
        static = self.source.index("EqualizerDisplay_DrawStaticLayout();", init)
        runtime = self.source.index("EqualizerDisplay_BeginRuntime();", static)
        adc_start = self.source.index("Adc_Start();", runtime)
        dac_start = self.source.index("Dac_Start();", runtime)
        self.assertLess(init, static)
        self.assertLess(static, runtime)
        self.assertLess(runtime, adc_start)
        self.assertLess(runtime, dac_start)

    def test_startup_seeds_snapshots_request_only_before_audio(self) -> None:
        init = self.source.index("EqualizerDisplay_Init();")
        runtime = self.source.index("EqualizerDisplay_BeginRuntime();", init)
        adc_start = self.source.index("Adc_Start();", runtime)
        startup = self.source[init:adc_start]
        snapshot = startup.index("EQ_BuildUiSnapshot(&lcd_snapshot);")
        request = startup.index("EqualizerDisplay_RequestSnapshot(", snapshot)
        runtime_rel = startup.index("EqualizerDisplay_BeginRuntime();", request)
        self.assertLess(snapshot, request)
        self.assertLess(request, runtime_rel)
        self.assertNotIn("EqualizerDisplay_ServiceOneJob", startup)
        self.assertNotIn("EqualizerDisplay_Update", startup)

    def test_runtime_uses_requests_not_direct_redraws(self) -> None:
        loop = self._runtime_loop()
        self.assertIn("EQ_BuildUiSnapshot(&lcd_snapshot);", loop)
        self.assertIn("EqualizerDisplay_RequestSnapshot(", loop)
        for forbidden in (
            "EqualizerDisplay_UpdateAll(",
            "EqualizerDisplay_UpdateGains(",
            "EqualizerDisplay_UpdateStatus(",
            "EqualizerDisplay_DrawStaticLayout(",
        ):
            self.assertNotIn(forbidden, loop)

    def test_service_block_is_guarded_and_returns_to_audio_top(self) -> None:
        loop = self._runtime_loop()
        service = loop.index("EqualizerDisplay_ServiceOneJob(")
        block_start = loop.rfind("if (EqualizerLcdPolicy_CanService(", 0, service)
        block_end = loop.index("continue;", service) + len("continue;")
        block = loop[block_start:block_end]
        for guard in (
            "FLAG_AD, FLAG_DA, flag_ad_done",
            "EQ_FrameServicePending",
            "audio_serviced",
            "builder_serviced, analyzer_serviced",
            "lcd_has_eligible_job",
        ):
            self.assertIn(guard, block)
        self.assertEqual(loop.count("EqualizerDisplay_ServiceOneJob("), 1)
        self.assertIn("if (lcd_job != EQ_LCD_JOB_NONE)", block)
        self.assertIn("EqualizerLcdPolicy_RecordService(", block)
        self.assertIn("lcd_audio_before", block)
        self.assertIn("lcd_audio_after", block)

    def test_predraw_snapshot_reuses_policy_and_defers_if_audio_arrives(self) -> None:
        loop = self._runtime_loop()
        service = loop.index("EqualizerDisplay_ServiceOneJob(")
        before = loop.rfind("lcd_audio_before =", 0, service)
        predraw = loop[before:service]
        self.assertIn("EqualizerLcdPolicy_Decide(", predraw)
        self.assertIn("EQ_LCD_POLICY_DEFER", predraw)
        self.assertIn("EQ_LCD_POLICY_SERVICE", predraw)
        self.assertIn("lcd_audio_before", predraw)
        self.assertIn("EqualizerLcdPolicy_RecordDeferred(", predraw)
        self.assertIn("EQ_DebugLcdDeferredAudioCount++", predraw)
        self.assertEqual(loop.count("EqualizerLcdPolicy_CanService("), 1)
        self.assertEqual(loop.count("EqualizerLcdPolicy_Decide("), 1)

    def test_deferred_count_requires_audio_busy_and_is_frame_latched(self) -> None:
        loop = self._runtime_loop()
        self.assertGreaterEqual(
            loop.count("EqualizerLcdPolicy_RecordDeferred("), 2
        )
        deferred = loop.rindex("EQ_DebugLcdDeferredAudioCount++;")
        block_start = loop.rfind("else if (", 0, deferred)
        block = loop[block_start:deferred]
        self.assertIn("EqualizerDisplay_HasEligibleJob(", block)
        self.assertNotIn("EqualizerDisplay_HasPendingJob()", block)
        self.assertIn("FLAG_AD != 0", block)
        self.assertIn("FLAG_DA != 0", block)
        self.assertIn("flag_ad_done != 0", block)
        self.assertIn("EQ_FrameServicePending != 0U", block)
        self.assertIn("EqualizerLcdPolicy_RecordDeferred(", block)

    def test_fault_monitor_auto_disables_without_stopping_audio(self) -> None:
        loop = self._runtime_loop()
        self.assertIn("EqualizerDisplay_AutoDisable(lcd_disable_reason);", loop)
        self.assertIn("EqualizerLcdFaultPolicy_Monitor(", loop)
        self.assertEqual(
            loop.count("EqualizerDisplay_AutoDisable(lcd_disable_reason);"), 1
        )
        monitor_start = loop.index("lcd_disable_reason = EqualizerLcdFaultPolicy_Monitor(")
        monitor = loop[monitor_start:loop.index("#endif", monitor_start)]
        self.assertIn("EQ_DebugLcdRuntimeMask != 0U", monitor)
        self.assertNotIn("Adc_Stop();", monitor)
        self.assertNotIn("Dac_Stop();", monitor)

    def test_policy_is_host_testable_and_enforces_frame_rules(self) -> None:
        bash = Path(os.environ.get("MSYS2_BASH", r"C:\msys64\usr\bin\bash.exe"))
        self.assertTrue(bash.exists(), f"MSYS2 bash not found: {bash}")
        with tempfile.TemporaryDirectory() as temp_dir:
            harness = Path(temp_dir) / "equalizer_flow_policy_test.c"
            exe = Path(temp_dir) / "equalizer_flow_policy_test.exe"
            harness.write_text(POLICY_HARNESS, encoding="ascii")

            def msys_path(path: Path) -> str:
                value = path.resolve().as_posix()
                return f"/{value[0].lower()}{value[2:]}"

            command = (
                "export PATH=/mingw64/bin:/usr/bin:$PATH; "
                "gcc -std=c99 -Wall -Wextra -Werror -DEQ_ALGO_ONLY "
                f"-I{msys_path(ROOT / 'Code/User/user_dsp')} "
                f"{msys_path(FLOW)} {msys_path(harness)} "
                f"-o {msys_path(exe)} && {msys_path(exe)}"
            )
            result = subprocess.run(
                [str(bash), "-lc", command],
                capture_output=True,
                text=True,
                encoding="utf-8",
                errors="replace",
                check=False,
            )
            self.assertEqual(
                result.returncode,
                0,
                f"host policy test failed\nstdout:\n{result.stdout}\nstderr:\n{result.stderr}",
            )

    def test_frame_limit_uses_policy_state_not_shadow_variable(self) -> None:
        self.assertNotIn("EQ_LCD_STATUS_PERIOD_FRAMES", self.header)
        self.assertNotIn("EQ_LastLcdServiceFrame", self.source)
        loop = self._runtime_loop()
        self.assertIn("EqualizerLcdPolicy_RecordService(", loop)

    def test_ti_timer_uses_unsigned_tscl_delta_only(self) -> None:
        self.assertIn("TSCL = 0;", self.source)
        self.assertNotIn("TSCH =", self.source)

    def test_analyzer_runtime_default_follows_ui_mask(self) -> None:
        self.assertIn("#define EQ_ENABLE_AUDIO_FEATURE_ANALYZER 0", self.header)
        self.assertIn(
            "volatile const unsigned int EQ_DebugAnalyzerCompiled =\n"
            "    EQ_ENABLE_AUDIO_FEATURE_ANALYZER;",
            self.source,
        )
        self.assertIn(
            "extern volatile const unsigned int EQ_DebugAnalyzerCompiled;",
            self.header,
        )
        self.assertEqual(self.source.count("EQ_DebugAnalyzerCompiled"), 1)
        self.assertIn(
            "((EQ_UI_RUNTIME_DEFAULT_MASK & EQ_UI_RUNTIME_ANALYZER) != 0U)",
            self.source,
        )
        self.assertIn(
            "#define EQ_ANALYZER_RUNTIME_DEFAULT_ENABLED 1U",
            self.source,
        )
        self.assertIn(
            "#define EQ_ANALYZER_RUNTIME_DEFAULT_ENABLED 0U",
            self.source,
        )
        self.assertIn(
            "volatile unsigned int EQ_DebugAnalyzerEnabled =\n"
            "    EQ_ANALYZER_RUNTIME_DEFAULT_ENABLED;",
            self.source,
        )
        self.assertIn(
            "EQ_DebugAnalyzerEnabled = EQ_ANALYZER_RUNTIME_DEFAULT_ENABLED;",
            self.source,
        )
        for name in (
            "EQ_DebugAnalyzerResetRequest",
            "EQ_DebugAnalyzerPending",
            "EQ_DebugAnalyzerValid",
            "EQ_DebugAnalyzerWarmup",
            "EQ_DebugAnalyzerRunCount",
            "EQ_DebugAnalyzerAnalysisCount",
            "EQ_DebugAnalyzerDeferredCount",
            "EQ_DebugAnalyzerPendingOverwriteCount",
            "EQ_DebugAnalyzerAudioArrivedCount",
            "EQ_DebugAnalyzerLastCycles",
            "EQ_DebugAnalyzerMaxCycles",
            "EQ_DebugAnalyzerPeakDbfs",
            "EQ_DebugAnalyzerRmsDbfs",
            "EQ_DebugAnalyzerBassDb",
            "EQ_DebugAnalyzerMudDb",
            "EQ_DebugAnalyzerPresenceDb",
            "EQ_DebugAnalyzerBrightnessDb",
            "EQ_DebugUartFeatureAuditPending",
            "EQ_DebugUartFeatureAuditComplete",
            "EQ_DebugUartFeatureAudioArrived",
            "EQ_DebugUartFeatureBaselineFrame",
        ):
            self.assertIn(name, self.source)
            self.assertIn(name, self.header)

    def test_compile_time_analyzer_gate_covers_board_integration(self) -> None:
        self.assertIn(
            "#if EQ_ENABLE_AUDIO_FEATURE_ANALYZER != 0\n"
            "#include \"user_audio_feature_analyzer.h\"",
            self.source,
        )
        guarded_symbols = (
            "static AUDIO_FEATURE_ANALYZER EQ_AudioAnalyzerState;",
            "static short EQ_AnalyzerInput[AUDIO_FEATURE_FRAME_LEN];",
            "AudioFeatureAnalyzer_Init(&EQ_AudioAnalyzerState);",
            "EQ_ObserveAnalyzerInputFrame();",
            "EQ_ServiceAnalyzer();",
            "EQ_UartReportFeatureOnce(flag_ad_done);",
        )
        guard_stack: list[str] = []
        guarded_lines: dict[str, bool] = {symbol: False for symbol in guarded_symbols}
        for line in self.source.splitlines():
            stripped = line.strip()
            if stripped.startswith("#if"):
                guard_stack.append(stripped)
            elif stripped.startswith("#endif"):
                if guard_stack:
                    guard_stack.pop()
            for symbol in guarded_symbols:
                if symbol in line:
                    guarded_lines[symbol] = any(
                        "EQ_ENABLE_AUDIO_FEATURE_ANALYZER != 0" in guard
                        for guard in guard_stack
                    )
        for symbol in guarded_symbols:
            self.assertTrue(guarded_lines[symbol], symbol)

        uart_start = self.source.index(
            "static void EQ_UartReportFeatureOnce(unsigned char flag_ad_done)"
        )
        uart_end = self.source.index(
            "static void EQ_CompleteUartFeatureAudit(void)", uart_start
        )
        uart = self.source[uart_start:uart_end]
        self.assertNotIn(
            "EQ_DebugUartFeatureDeadlineDelta =", uart
        )

    def test_uart_requires_enabled_valid_warmup_and_request(self) -> None:
        loop = self._runtime_loop()
        start = loop.index("uart_eligible =")
        end = loop.index(";", start)
        eligibility = loop[start:end]
        for condition in (
            "EQ_DebugAnalyzerEnabled != 0U",
            "EQ_DebugAnalyzerValid != 0U",
            "EQ_DebugAnalyzerWarmup != 0U",
            "EQ_DebugUartFeatureRequest != 0U",
            "EQ_DebugUartFeatureAuditPending == 0U",
        ):
            self.assertIn(condition, eligibility)

        service_start = loop.index(
            "if (background_kind == EQ_BACKGROUND_UART)"
        )
        service_end = loop.index("continue;", service_start)
        service_guard = loop[service_start:service_end]
        self.assertIn(
            "EQ_DebugUartFeatureAuditPending != 0U", service_guard
        )

    def test_analyzer_disable_and_reset_clear_stale_publication(self) -> None:
        start = self.source.index(
            "static void EQ_ClearPublishedAnalyzerState("
        )
        end = self.source.index(
            "static void EQ_ResetAnalyzerRuntime(void)", start
        )
        clear = self.source[start:end]
        for assignment in (
            "EQ_DebugAnalyzerPending = 0U;",
            "EQ_DebugAnalyzerValid = 0U;",
            "EQ_DebugAnalyzerWarmup = 0U;",
            "EQ_DebugAnalyzerAnalysisCount = 0UL;",
            "EQ_DebugUartFeatureRequest = 0U;",
            "EQ_AnalyzerLastDeferredFrame = ~0UL;",
            "EqualizerUartFeatureAudit_Init(&EQ_UartFeatureAudit);",
            "EQ_SyncUartFeatureAuditDebug();",
        ):
            self.assertIn(assignment, clear)

        service_start = self.source.index(
            "static int EQ_ServiceAnalyzerControl("
        )
        service_end = self.source.index(
            "static void EQ_RecordAnalyzerDeferred(void)", service_start
        )
        service = self.source[service_start:service_end]
        self.assertIn("EQ_ANALYZER_ACTION_DISABLE", service)
        self.assertIn("EQ_ANALYZER_ACTION_ENABLE_RESET", service)
        self.assertIn("EQ_ANALYZER_ACTION_MANUAL_RESET", service)
        self.assertIn("EQ_ClearPublishedAnalyzerState(0);", service)
        self.assertNotIn("EQ_BoardState", service)

        reset_start = self.source.index(
            "static void EQ_ResetAnalyzerRuntime(void)", start
        )
        reset_end = self.source.index(
            "static int EQ_ServiceAnalyzerControl(", reset_start
        )
        reset = self.source[reset_start:reset_end]
        self.assertIn("EQ_ClearPublishedAnalyzerState(1);", reset)
        self.assertNotIn("EQ_ClearPublishedAnalyzerState();", self.source)
        self.assertNotIn("Equalizer_Set", service)
        self.assertNotIn("Equalizer_Apply", service)

    def test_uart_audit_completes_after_frame_service(self) -> None:
        da_start = self.source.index(
            "if ((FLAG_DA == 1) && (flag_ad_done == 1))"
        )
        da_end = self.source.index("if ((FLAG_AD == 0)", da_start)
        da_block = self.source[da_start:da_end]
        self.assertLess(
            da_block.index("EQ_EndFrameService();"),
            da_block.index("EQ_CompleteUartFeatureAudit();"),
        )

    def test_analyzer_cadence_observation_precedes_eq_but_fft_is_background(self) -> None:
        start = self.source.index("static void EQ_CaptureAdcFrame(void)")
        end = self.source.index("static void EQ_FillDacPingBuffer(void)", start)
        capture = self.source[start:end]
        observe = capture.index("EQ_ObserveAnalyzerInputFrame();")
        process = capture.index("Equalizer_ProcessFrame(")
        self.assertLess(observe, process)
        self.assertNotIn("AudioFeatureAnalyzer_AnalyzeObservedFrame", capture)

        service_start = self.source.index("static int EQ_ServiceAnalyzer(void)")
        service_end = self.source.index("static void EQ_CaptureAdcFrame(void)", service_start)
        service = self.source[service_start:service_end]
        self.assertIn("AudioFeatureAnalyzer_AnalyzeObservedFrame(", service)
        self.assertIn("EQ_AnalyzerInput", service)

    def test_background_priority_is_builder_analyzer_uart_lcd(self) -> None:
        self.assertIn("EQ_BACKGROUND_ANALYZER", self.header)
        self.assertIn("EQ_BACKGROUND_UART", self.header)
        start = self.source.index("int EqualizerBackgroundService_Decide(")
        end = self.source.index("void EqualizerBackgroundService_Record(", start)
        decide = self.source[start:end]
        builder = decide.index("return EQ_BACKGROUND_BUILDER;")
        analyzer = decide.index("return EQ_BACKGROUND_ANALYZER;")
        uart = decide.index("return EQ_BACKGROUND_UART;")
        lcd = decide.index("return EQ_BACKGROUND_LCD;")
        self.assertLess(builder, analyzer)
        self.assertLess(analyzer, uart)
        self.assertLess(uart, lcd)

    def test_analyzer_service_rechecks_audio_and_builder(self) -> None:
        loop = self._runtime_loop()
        service = loop.index("EQ_ServiceAnalyzer();")
        block_start = loop.rfind(
            "if (background_kind == EQ_BACKGROUND_ANALYZER)", 0, service
        )
        block_end = loop.index("continue;", service)
        block = loop[block_start:block_end]
        self.assertIn("EqualizerAnalyzer_CanService(", block)
        self.assertIn("builder_eligible", block)
        for flag in ("FLAG_AD", "FLAG_DA", "flag_ad_done", "EQ_FrameServicePending"):
            self.assertIn(flag, block)

    def test_uart_feature_report_is_single_bounded_integer_line(self) -> None:
        self.assertIn("EQ_DebugUartFeatureRequest = 0U", self.source)
        self.assertIn("P33 FEAT,", self.source)
        self.assertIn("EQ_UART_FEATURE_LINE_CAPACITY 72", self.source)
        self.assertNotIn("sprintf(", self.source)
        self.assertNotIn("UARTprintf(", self.source)
        self.assertEqual(
            self.source.count("EQ_UartReportFeatureOnce(flag_ad_done);"), 1
        )


if __name__ == "__main__":
    unittest.main()
