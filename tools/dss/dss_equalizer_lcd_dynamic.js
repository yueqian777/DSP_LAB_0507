/* Ten-minute Project 3.3 dynamic LCD stability test; leaves DSP running. */
importPackage(Packages.com.ti.debug.engine.scripting);
importPackage(Packages.com.ti.ccstudio.scripting.environment);
importPackage(Packages.java.lang);
importPackage(Packages.java.io);

function env(name, fallback) {
    var value = String(System.getenv(name));
    return (value == "null" || value == "") ? fallback : value;
}

var root = env("DSP_TEST_ROOT", ".");
var ccxml = env("DSP_TEST_CCXML", root + "/TargetConfig/C6748.ccxml");
var program = env("DSP_TEST_PROGRAM", root + "/Debug/DSP_LAB_0507.out");
var resultPath = env("DSP_TEST_RESULT_PATH", "lcd_dynamic_result.json");
var expectedSha = env("DSP_TEST_EXPECTED_SHA", "UNKNOWN");
var expectedDirty = parseInt(env("DSP_TEST_EXPECTED_DIRTY", "0"), 10);
var durationMs = parseInt(env("DSP_TEST_DURATION_MS", "600000"), 10);
var auditMs = 250;
var script = null;
var debugServer = null;
var debugSession = null;
var targetLoaded = false;
var targetRunning = false;
var LCD_ACCEPTANCE_CYCLES = 912000;
var MAX_LCD_JOBS_PER_SECOND = 8.0;
var MAX_ANALYZER_STRIP_HEIGHT = 16;

function quote(value) {
    return "\"" + String(value).replace(/\\/g, "\\\\")
        .replace(/\"/g, "\\\"").replace(/\r/g, "\\r")
        .replace(/\n/g, "\\n").replace(/\t/g, "\\t") + "\"";
}

function stringify(value) {
    var kind, parts, key, index;
    if (value === null || typeof value == "undefined") return "null";
    kind = typeof value;
    if (kind == "string") return quote(value);
    if (kind == "number") return isFinite(value) ? String(value) : "null";
    if (kind == "boolean") return value ? "true" : "false";
    parts = [];
    if (value instanceof Array) {
        for (index = 0; index < value.length; index++) {
            parts.push(stringify(value[index]));
        }
        return "[" + parts.join(",") + "]";
    }
    for (key in value) parts.push(quote(key) + ":" + stringify(value[key]));
    return "{" + parts.join(",") + "}";
}

function save(value) {
    var writer = new FileWriter(resultPath, false);
    writer.write(stringify(value) + "\n");
    writer.close();
}

function evaluate(expression) {
    return String(debugSession.expression.evaluate(expression));
}

function numberValue(expression) {
    var text = evaluate(expression);
    var hex = text.match(/-?0x[0-9a-fA-F]+/);
    var value = hex ? parseInt(hex[0], 16) : parseFloat(text);
    if (!isFinite(value)) throw "Unreadable " + expression + "=" + text;
    return value;
}

function readCString(symbol, maximum) {
    var text = "", index, code;
    for (index = 0; index < maximum; index++) {
        code = numberValue("(int)" + symbol + "[" + index + "]");
        if (code == 0) break;
        text += String.fromCharCode(code & 0xff);
    }
    return text;
}

function write(expression) {
    evaluate(expression);
}

function requireCondition(condition, message) {
    if (!condition) throw message;
}

function runFor(milliseconds) {
    debugSession.target.runAsynch();
    targetRunning = true;
    Thread.sleep(milliseconds);
    debugSession.target.halt();
    targetRunning = false;
}

function readArray(symbol, count) {
    var values = [], index;
    for (index = 0; index < count; index++) {
        values.push(numberValue(symbol + "[" + index + "]"));
    }
    return values;
}

function snapshot() {
    return {
        init_stage: numberValue("EQ_DebugInitStage"),
        ad_frames: numberValue("EQ_DebugAdFrames"),
        da_frames: numberValue("EQ_DebugDaFrames"),
        process_frames: numberValue("EQ_DebugProcessFrames"),
        input_peak: numberValue("EQ_DebugInputPeak"),
        output_peak: numberValue("EQ_DebugOutputPeak"),
        clip: numberValue("EQ_DebugClipCount"),
        deadline_miss: numberValue("EQ_DebugDeadlineMissCount"),
        latency_miss: numberValue("EQ_DebugFrameLatencyDeadlineMissCount"),
        overlap: numberValue("EQ_DebugFrameServiceOverlapCount"),
        dropped: numberValue("EQ_DebugFrameServiceDroppedCount"),
        algo_max_cycles: numberValue("EQ_DebugAlgoMaxCycles"),
        frame_service_max_cycles: numberValue("EQ_DebugFrameServiceMaxCycles"),
        frame_latency_max_cycles: numberValue("EQ_DebugFrameLatencyMaxCycles"),

        analyzer_compiled: numberValue("EQ_DebugAnalyzerCompiled"),
        analyzer_enabled: numberValue("EQ_DebugAnalyzerEnabled"),
        analyzer_valid: numberValue("EQ_DebugAnalyzerValid"),
        analyzer_warm: numberValue("EQ_DebugAnalyzerWarmup"),
        analyzer_runs: numberValue("EQ_DebugAnalyzerRunCount"),
        analyzer_analyses: numberValue("EQ_DebugAnalyzerAnalysisCount"),
        analyzer_deferred: numberValue("EQ_DebugAnalyzerDeferredCount"),
        analyzer_max_cycles: numberValue("EQ_DebugAnalyzerMaxCycles"),
        analyzer_peak_dbfs: numberValue("EQ_DebugAnalyzerPeakDbfs"),
        analyzer_rms_dbfs: numberValue("EQ_DebugAnalyzerRmsDbfs"),

        smart_compiled: numberValue("EQ_DebugSmartBassCompiled"),
        smart_enabled: numberValue("EQ_DebugSmartBassEnabled"),
        smart_decisions: numberValue("EQ_DebugSmartBassDecisionCount"),
        smart_max_cycles: numberValue("EQ_DebugSmartBassMaxCycles"),
        smart_saturation: numberValue("EQ_DebugSmartBassSaturationCount"),
        smart_nonfinite: numberValue("EQ_DebugSmartBassNonFiniteCount"),
        clarity_compiled: numberValue("EQ_DebugDynamicClarityCompiled"),
        clarity_enabled: numberValue("EQ_DebugDynamicClarityEnabled"),
        clarity_decisions: numberValue("EQ_DebugDynamicClarityDecisionCount"),
        clarity_max_cycles: numberValue("EQ_DebugDynamicClarityMaxCycles"),
        clarity_saturation: numberValue("EQ_DebugDynamicClaritySaturationCount"),
        clarity_nonfinite: numberValue("EQ_DebugDynamicClarityNonFiniteCount"),
        guard_compiled: numberValue("EQ_DebugHarshnessGuardCompiled"),
        guard_enabled: numberValue("EQ_DebugHarshnessGuardEnabled"),
        guard_decisions: numberValue("EQ_DebugHarshnessGuardDecisionCount"),
        guard_max_cycles: numberValue("EQ_DebugHarshnessGuardMaxCycles"),
        guard_saturation: numberValue("EQ_DebugHarshnessGuardSaturationCount"),
        guard_nonfinite: numberValue("EQ_DebugHarshnessGuardNonFiniteCount"),

        lcd_alignment_enabled: numberValue(
            "EQ_DebugLcdAlignmentPatternEnabled"),
        lcd_enabled: numberValue("EQ_DebugLcdEnabled"),
        lcd_runtime_mask: numberValue("EQ_DebugLcdRuntimeMask"),
        lcd_pending_mask: numberValue("EQ_DebugLcdPendingMask"),
        lcd_static_draws: numberValue("EQ_DebugLcdStaticDrawCount"),
        lcd_jobs: numberValue("EQ_DebugLcdJobCount"),
        lcd_deferred_audio: numberValue("EQ_DebugLcdDeferredAudioCount"),
        lcd_audio_during_draw: numberValue(
            "EQ_DebugLcdAudioArrivedDuringDrawCount"),
        lcd_unexpected_full_redraw: numberValue(
            "EQ_DebugLcdUnexpectedFullRedrawCount"),
        lcd_bounds_failures: numberValue("EQ_DebugLcdBoundsFailureCount"),
        lcd_budget_exceeded: numberValue("EQ_DebugLcdBudgetExceededCount"),
        lcd_hard_budget_exceeded: numberValue(
            "EQ_DebugLcdHardBudgetExceededCount"),
        lcd_over_1ms: numberValue("EQ_DebugLcdOver1msCount"),
        lcd_over_2ms: numberValue("EQ_DebugLcdOver2msCount"),
        lcd_over_5ms: numberValue("EQ_DebugLcdOver5msCount"),
        lcd_last_job_cycles: numberValue("EQ_DebugLcdLastJobCycles"),
        lcd_max_job_cycles: numberValue("EQ_DebugLcdMaxJobCycles"),
        lcd_auto_disabled: numberValue("EQ_DebugLcdAutoDisabledCount"),
        lcd_auto_disable_reason: numberValue("EQ_DebugLcdAutoDisableReason"),
        lcd_analyzer_last_strip_height: numberValue(
            "EQ_DebugLcdAnalyzerLastStripHeight"),
        lcd_analyzer_max_strip_height: numberValue(
            "EQ_DebugLcdAnalyzerMaxStripHeight"),
        lcd_analyzer_strips: numberValue("EQ_DebugLcdAnalyzerStripCount"),
        lcd_category_count: readArray("EQ_DebugLcdCategoryCount", 3),
        lcd_category_max_cycles: readArray(
            "EQ_DebugLcdCategoryMaxCycles", 4),
        lcd_job_type_count: readArray("EQ_DebugLcdJobTypeCount", 12),
        lcd_job_type_max_cycles: readArray(
            "EQ_DebugLcdJobTypeMaxCycles", 15),

        lcd_expected_frame_base: numberValue(
            "EQ_DebugLcdExpectedFrameBase"),
        lcd_expected_frame_end: numberValue(
            "EQ_DebugLcdExpectedFrameEnd"),
        lcd_buffer_address: numberValue("EQ_DebugLcdBufferAddress"),
        lcd_buffer_end_address: numberValue("EQ_DebugLcdBufferEndAddress"),
        lcd_current_frame_base: numberValue(
            "EQ_DebugLcdCurrentFrameBase"),
        lcd_current_frame_end: numberValue(
            "EQ_DebugLcdCurrentFrameEnd"),
        lcd_raster_control: numberValue(
            "EQ_DebugLcdHwSnapshot.raster_control"),
        lcd_raster_status: numberValue("EQ_DebugLcdLastRasterStatus"),
        lcd_startup_raster_status: numberValue(
            "EQ_DebugLcdStartupRasterStatus"),
        lcd_startup_status_after_clear: numberValue(
            "EQ_DebugLcdStartupStatusAfterClear"),
        lcd_startup_fault_clear_count: numberValue(
            "EQ_DebugLcdStartupFaultClearCount"),
        lcd_dma_control: numberValue("EQ_DebugLcdHwSnapshot.dma_control"),
        lcd_irq_status: numberValue("EQ_DebugLcdLastIrqStatus"),
        lcd_fault_latched: numberValue("EQ_DebugLcdFaultLatched"),
        lcd_raster_faults: numberValue("EQ_DebugLcdRasterFaultCount"),
        lcd_sync_lost: numberValue("EQ_DebugLcdSyncLostCount"),
        lcd_fifo_underflow: numberValue("EQ_DebugLcdFifoUnderflowCount"),
        lcd_frame_mismatch: numberValue(
            "EQ_DebugLcdFrameAddressMismatchCount"),
        lcd_canary_checks: numberValue(
            "EQ_DebugLcdFramebufferCanaryCheckCount"),
        lcd_canary_failures: numberValue(
            "EQ_DebugLcdFramebufferCanaryFailureCount")
    };
}

function verifyBuild() {
    requireCondition(numberValue("EQ_DebugBuildMagic") == 0x33030003,
                     "build magic mismatch");
    requireCondition(numberValue("EQ_DebugBuildDirty") == expectedDirty,
                     "build dirty mismatch");
    requireCondition(readCString("EQ_DebugBuildGitSha", 16) == expectedSha,
                     "build SHA mismatch");
    requireCondition(numberValue("EQ_DebugAnalyzerCompiled") == 1,
                     "analyzer is not compiled");
    requireCondition(numberValue("EQ_DebugSmartBassCompiled") == 1 &&
                     numberValue("EQ_DebugDynamicClarityCompiled") == 1 &&
                     numberValue("EQ_DebugHarshnessGuardCompiled") == 1,
                     "dynamic feature is not compiled");
    requireCondition(numberValue("EQ_DebugLcdAlignmentPatternEnabled") == 0,
                     "dynamic build unexpectedly uses alignment page");
}

function delta(after, before, name) {
    return after[name] - before[name];
}

function verifyCycleArray(values, label) {
    var index;
    for (index = 0; index < values.length; index++) {
        requireCondition(values[index] <= LCD_ACCEPTANCE_CYCLES,
                         label + "[" + index + "] exceeded 2 ms");
    }
}

function verify(before, after) {
    var adDelta = delta(after, before, "ad_frames");
    var daDelta = delta(after, before, "da_frames");
    var processDelta = delta(after, before, "process_frames");
    var jobDelta = delta(after, before, "lcd_jobs");
    var elapsedSeconds = (durationMs + auditMs) / 1000.0;
    var jobsPerSecond = jobDelta / elapsedSeconds;

    requireCondition(before.init_stage == 11 && after.init_stage == 11,
                     "initialization did not reach stage 11");
    requireCondition(adDelta > 0 && daDelta > 0 && processDelta > 0,
                     "audio frame counters did not advance");
    requireCondition(Math.abs(adDelta - daDelta) <= 1 &&
                     Math.abs(adDelta - processDelta) <= 1,
                     "AD/DA/process frame mismatch");
    requireCondition(after.input_peak > 0 && after.output_peak > 0,
                     "music-like signal was not observed");
    requireCondition(after.analyzer_enabled == 1 &&
                     after.analyzer_valid == 1 && after.analyzer_warm == 1,
                     "analyzer did not become valid and warm");
    requireCondition(delta(after, before, "analyzer_analyses") > 0 &&
                     delta(after, before, "analyzer_runs") > 0,
                     "analyzer did not continue running");
    requireCondition(isFinite(after.analyzer_peak_dbfs) &&
                     isFinite(after.analyzer_rms_dbfs),
                     "analyzer levels are non-finite");

    requireCondition(after.smart_enabled == 1 && after.clarity_enabled == 1 &&
                     after.guard_enabled == 1,
                     "dynamic feature enable request was not retained");
    requireCondition(delta(after, before, "smart_decisions") > 0 &&
                     delta(after, before, "clarity_decisions") > 0 &&
                     delta(after, before, "guard_decisions") > 0,
                     "dynamic decisions did not advance");
    requireCondition(after.smart_saturation == 0 &&
                     after.clarity_saturation == 0 &&
                     after.guard_saturation == 0 &&
                     after.smart_nonfinite == 0 &&
                     after.clarity_nonfinite == 0 &&
                     after.guard_nonfinite == 0,
                     "dynamic safety counter is nonzero");

    requireCondition(after.lcd_enabled == 1 && after.lcd_runtime_mask == 15,
                     "dynamic LCD runtime profile is not active");
    requireCondition(after.lcd_alignment_enabled == 0,
                     "alignment page is active in dynamic test");
    requireCondition(jobDelta > 0 &&
                     delta(after, before, "lcd_analyzer_strips") > 0,
                     "dynamic analyzer LCD work did not advance");
    requireCondition(after.lcd_category_count[1] >
                     before.lcd_category_count[1],
                     "dynamic status fields were not exercised");
    requireCondition(after.lcd_analyzer_max_strip_height <=
                     MAX_ANALYZER_STRIP_HEIGHT,
                     "analyzer strip exceeded 16 pixels");
    requireCondition(jobsPerSecond <= MAX_LCD_JOBS_PER_SECOND,
                     "LCD job rate exceeded 8 jobs/s");
    requireCondition(after.lcd_max_job_cycles <= LCD_ACCEPTANCE_CYCLES,
                     "LCD maximum job exceeded 2 ms");
    verifyCycleArray(after.lcd_category_max_cycles, "LCD category");
    verifyCycleArray(after.lcd_job_type_max_cycles, "LCD job type");
    requireCondition(after.lcd_budget_exceeded == 0 &&
                     after.lcd_hard_budget_exceeded == 0 &&
                     after.lcd_over_2ms == 0 && after.lcd_over_5ms == 0,
                     "LCD timing counter is nonzero");
    requireCondition(after.lcd_auto_disabled == 0 &&
                     after.lcd_auto_disable_reason == 0,
                     "LCD was automatically disabled");

    requireCondition(after.deadline_miss == 0 && after.latency_miss == 0 &&
                     after.overlap == 0 && after.dropped == 0 &&
                     after.clip == 0,
                     "audio safety counter is nonzero");
    requireCondition(after.lcd_unexpected_full_redraw == 0 &&
                     after.lcd_bounds_failures == 0 &&
                     after.lcd_fault_latched == 0 &&
                     after.lcd_raster_faults == 0 &&
                     after.lcd_sync_lost == 0 &&
                     after.lcd_fifo_underflow == 0 &&
                     after.lcd_frame_mismatch == 0 &&
                     after.lcd_canary_failures == 0,
                     "LCD raster/framebuffer safety counter is nonzero");
    requireCondition(after.lcd_expected_frame_base ==
                     after.lcd_current_frame_base &&
                     after.lcd_expected_frame_end == after.lcd_current_frame_end,
                     "FB0 address does not match initialized address");
    requireCondition(before.lcd_expected_frame_base ==
                     after.lcd_expected_frame_base &&
                     before.lcd_expected_frame_end == after.lcd_expected_frame_end,
                     "FB0 range changed during dynamic rendering");
    requireCondition(after.lcd_expected_frame_base ==
                     after.lcd_buffer_address + 4 &&
                     after.lcd_buffer_end_address -
                     after.lcd_expected_frame_end == 3,
                     "framebuffer symbol bounds do not match FB0 range");
    requireCondition((after.lcd_raster_control & 1) != 0,
                     "LCD raster is disabled");
    requireCondition(after.lcd_canary_checks > before.lcd_canary_checks,
                     "framebuffer canary audit did not advance");

    return {
        ad_delta: adDelta,
        da_delta: daDelta,
        process_delta: processDelta,
        lcd_job_delta: jobDelta,
        lcd_jobs_per_second: jobsPerSecond,
        analyzer_analysis_delta: delta(
            after, before, "analyzer_analyses"),
        analyzer_strip_delta: delta(
            after, before, "lcd_analyzer_strips"),
        dynamic_status_job_delta:
            after.lcd_category_count[1] - before.lcd_category_count[1]
    };
}

var before = null;
var after = null;
var metrics = null;
var summary = null;
try {
    script = ScriptingEnvironment.instance();
    debugServer = script.getServer("DebugServer.1");
    debugServer.setConfig(ccxml);
    debugSession = debugServer.openSession(".*C674X_0");
    debugSession.target.connect();
    debugSession.target.reset();
    Thread.sleep(500);
    debugSession.memory.loadProgram(program);
    targetLoaded = true;
    Thread.sleep(500);
    runFor(5000);
    verifyBuild();

    write("EQ_DebugAnalyzerEnabled = 1");
    runFor(8000);
    requireCondition(numberValue("EQ_DebugAnalyzerValid") == 1 &&
                     numberValue("EQ_DebugAnalyzerWarmup") == 1,
                     "analyzer warmup failed");
    before = snapshot();

    write("EQ_DebugSmartBassEnabled = 1");
    write("EQ_DebugDynamicClarityEnabled = 1");
    write("EQ_DebugHarshnessGuardEnabled = 1");
    runFor(durationMs);
    write("EQ_DebugLcdHardwareAuditRequest = 1");
    runFor(auditMs);
    after = snapshot();
    metrics = verify(before, after);
    summary = {
        pass: true,
        evidence_label: "MEASURED_ON_CURRENT_BOARD_OBJECTIVE_ONLY",
        expected_sha: expectedSha,
        expected_dirty: expectedDirty,
        duration_ms: durationMs,
        audit_ms: auditMs,
        lcd_acceptance_cycles: LCD_ACCEPTANCE_CYCLES,
        lcd_max_jobs_per_second: MAX_LCD_JOBS_PER_SECOND,
        before: before,
        after: after,
        metrics: metrics,
        screen_circular_shift: "PENDING_OPERATOR_VISUAL_OBSERVATION",
        final_target_state: "RUNNING_DISCONNECTED"
    };
    save(summary);
} catch (error) {
    summary = {
        pass: false,
        error: String(error),
        expected_sha: expectedSha,
        expected_dirty: expectedDirty,
        duration_ms: durationMs,
        before: before,
        after: after,
        metrics: metrics,
        screen_circular_shift: "PENDING_OPERATOR_VISUAL_OBSERVATION",
        final_target_state: targetLoaded ? "RUNNING_DISCONNECTED" : "NOT_LOADED"
    };
    save(summary);
    throw error;
} finally {
    if (targetLoaded && debugSession !== null) {
        try {
            if (!targetRunning) debugSession.target.runAsynch();
            Thread.sleep(250);
            debugSession.target.disconnect();
        } catch (ignored0) {}
    }
    try { if (debugSession !== null) debugSession.terminate(); } catch (ignored1) {}
    try { if (debugServer !== null) debugServer.stop(); } catch (ignored2) {}
}
