/* Static LCD alignment/raster stability test; leaves Project 3.3 running. */
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
var resultPath = env("DSP_TEST_RESULT_PATH", "lcd_alignment_result.json");
var expectedSha = env("DSP_TEST_EXPECTED_SHA", "UNKNOWN");
var expectedDirty = parseInt(env("DSP_TEST_EXPECTED_DIRTY", "0"), 10);
var durationMs = parseInt(env("DSP_TEST_DURATION_MS", "600000"), 10);
var script = null;
var debugServer = null;
var debugSession = null;
var targetLoaded = false;
var targetRunning = false;

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

function runFor(milliseconds) {
    debugSession.target.runAsynch();
    targetRunning = true;
    Thread.sleep(milliseconds);
    debugSession.target.halt();
    targetRunning = false;
}

function snapshot() {
    return {
        init_stage: numberValue("EQ_DebugInitStage"),
        ad_frames: numberValue("EQ_DebugAdFrames"),
        da_frames: numberValue("EQ_DebugDaFrames"),
        process_frames: numberValue("EQ_DebugProcessFrames"),
        deadline_miss: numberValue("EQ_DebugDeadlineMissCount"),
        latency_miss: numberValue("EQ_DebugFrameLatencyDeadlineMissCount"),
        overlap: numberValue("EQ_DebugFrameServiceOverlapCount"),
        dropped: numberValue("EQ_DebugFrameServiceDroppedCount"),
        clip: numberValue("EQ_DebugClipCount"),
        frame_service_max_cycles: numberValue("EQ_DebugFrameServiceMaxCycles"),
        frame_latency_max_cycles: numberValue("EQ_DebugFrameLatencyMaxCycles"),
        lcd_alignment_enabled: numberValue(
            "EQ_DebugLcdAlignmentPatternEnabled"),
        lcd_enabled: numberValue("EQ_DebugLcdEnabled"),
        lcd_runtime_mask: numberValue("EQ_DebugLcdRuntimeMask"),
        lcd_static_draws: numberValue("EQ_DebugLcdStaticDrawCount"),
        lcd_jobs: numberValue("EQ_DebugLcdJobCount"),
        lcd_bounds_failures: numberValue("EQ_DebugLcdBoundsFailureCount"),
        lcd_expected_frame_base: numberValue(
            "EQ_DebugLcdExpectedFrameBase"),
        lcd_expected_frame_end: numberValue(
            "EQ_DebugLcdExpectedFrameEnd"),
        lcd_buffer_address: numberValue("EQ_DebugLcdBufferAddress"),
        lcd_buffer_end_address: numberValue(
            "EQ_DebugLcdBufferEndAddress"),
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
            "EQ_DebugLcdFramebufferCanaryFailureCount"),
        lcd_first_fault_frame: numberValue("EQ_DebugLcdFirstFaultFrame"),
        lcd_first_fault_job: numberValue("EQ_DebugLcdFirstFaultJob")
    };
}

function verifyBuild() {
    if (numberValue("EQ_DebugBuildMagic") != 0x33030003)
        throw "build magic mismatch";
    if (numberValue("EQ_DebugBuildDirty") != expectedDirty)
        throw "build dirty mismatch";
    if (readCString("EQ_DebugBuildGitSha", 16) != expectedSha)
        throw "build SHA mismatch";
}

function verify(before, after) {
    if (before.init_stage != 11 || after.init_stage != 11)
        throw "initialization did not reach stage 11";
    if (before.lcd_alignment_enabled != 1 || after.lcd_alignment_enabled != 1)
        throw "not an alignment diagnostic build";
    if (before.lcd_runtime_mask != 0 || after.lcd_runtime_mask != 0 ||
        before.lcd_jobs != 0 || after.lcd_jobs != 0)
        throw "alignment page generated runtime LCD work";
    if (after.process_frames <= before.process_frames ||
        after.ad_frames <= before.ad_frames || after.da_frames <= before.da_frames)
        throw "audio frame counters did not advance";
    if (after.deadline_miss != 0 || after.latency_miss != 0 ||
        after.overlap != 0 || after.dropped != 0 || after.clip != 0)
        throw "audio safety counter nonzero";
    if (after.lcd_bounds_failures != 0 || after.lcd_fault_latched != 0 ||
        after.lcd_raster_faults != 0 || after.lcd_sync_lost != 0 ||
        after.lcd_fifo_underflow != 0 || after.lcd_frame_mismatch != 0 ||
        after.lcd_canary_failures != 0)
        throw "LCD raster/framebuffer diagnostic fault";
    if (after.lcd_expected_frame_base != after.lcd_current_frame_base ||
        after.lcd_expected_frame_end != after.lcd_current_frame_end)
        throw "FB0 address does not match initialized address";
    if (after.lcd_expected_frame_base != after.lcd_buffer_address + 4 ||
        after.lcd_buffer_end_address - after.lcd_expected_frame_end != 3)
        throw "framebuffer symbol bounds do not match FB0 range";
    if ((after.lcd_raster_control & 1) == 0)
        throw "raster disabled";
    if (after.lcd_canary_checks <= before.lcd_canary_checks)
        throw "periodic framebuffer guard did not run";
}

var before = null;
var after = null;
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
    before = snapshot();
    runFor(durationMs);
    write("EQ_DebugLcdHardwareAuditRequest = 1");
    runFor(250);
    after = snapshot();
    verify(before, after);
    summary = {
        pass: true,
        evidence_label: "MEASURED_ON_CURRENT_BOARD_OBJECTIVE_ONLY",
        expected_sha: expectedSha,
        expected_dirty: expectedDirty,
        duration_ms: durationMs,
        before: before,
        after: after,
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
