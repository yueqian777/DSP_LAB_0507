/* Objective Project 3.3 LCD/Touch load validation for C6748 DSS. */
importPackage(Packages.com.ti.debug.engine.scripting);
importPackage(Packages.com.ti.ccstudio.scripting.environment);
importPackage(Packages.java.lang);
importPackage(Packages.java.io);

function env(name, fallback) {
    var value = String(System.getenv(name));
    return (value == "null" || value == "") ? fallback : value;
}

var root = env("DSP_TEST_ROOT",
    String(new File(".").getCanonicalPath()).replace(/\\/g, "/"));
var ccxml = env("DSP_TEST_CCXML", root + "/TargetConfig/C6748.ccxml");
var program = env("DSP_TEST_PROGRAM", root + "/Debug/DSP_LAB_0507.out");
var resultDir = env("DSP_TEST_RESULT_DIR",
    String(System.getProperty("java.io.tmpdir")));
var expectedSha = env("DSP_TEST_EXPECTED_SHA", "UNKNOWN");
var summaryPath = resultDir + "/summary.json";
var resultPath = resultDir + "/ui_stress_result.json";
var script = null;
var debugServer = null;
var debugSession = null;
var FRAME_BUDGET_CYCLES = 9338880;
var LCD_HARD_CYCLES = 2280000;

var symbols = {
    init_stage: "EQ_DebugInitStage",
    ad_frames: "EQ_DebugAdFrames",
    da_frames: "EQ_DebugDaFrames",
    process_frames: "EQ_DebugProcessFrames",
    algo_max_cycles: "EQ_DebugAlgoMaxCycles",
    frame_service_max_cycles: "EQ_DebugFrameServiceMaxCycles",
    frame_latency_max_cycles: "EQ_DebugFrameLatencyMaxCycles",
    deadline: "EQ_DebugDeadlineMissCount",
    latency: "EQ_DebugFrameLatencyDeadlineMissCount",
    overlap: "EQ_DebugFrameServiceOverlapCount",
    dropped: "EQ_DebugFrameServiceDroppedCount",
    clip: "EQ_DebugClipCount",
    input_peak: "EQ_DebugInputPeak",
    output_peak: "EQ_DebugOutputPeak",
    analyzer_compiled: "EQ_DebugAnalyzerCompiled",
    analyzer_valid: "EQ_DebugAnalyzerValid",
    analyzer_warm: "EQ_DebugAnalyzerWarmup",
    analyzer_runs: "EQ_DebugAnalyzerRunCount",
    analyzer_analyses: "EQ_DebugAnalyzerAnalysisCount",
    smart_compiled: "EQ_DebugSmartBassCompiled",
    clarity_compiled: "EQ_DebugDynamicClarityCompiled",
    guard_compiled: "EQ_DebugHarshnessGuardCompiled",
    smart_saturation: "EQ_DebugSmartBassSaturationCount",
    smart_nonfinite: "EQ_DebugSmartBassNonFiniteCount",
    clarity_saturation: "EQ_DebugDynamicClaritySaturationCount",
    clarity_nonfinite: "EQ_DebugDynamicClarityNonFiniteCount",
    guard_saturation: "EQ_DebugHarshnessGuardSaturationCount",
    guard_nonfinite: "EQ_DebugHarshnessGuardNonFiniteCount",
    lcd_enabled: "EQ_DebugLcdEnabled",
    lcd_mask: "EQ_DebugLcdRuntimeMask",
    lcd_pending: "EQ_DebugLcdPendingMask",
    lcd_jobs: "EQ_DebugLcdJobCount",
    lcd_deferred: "EQ_DebugLcdDeferredAudioCount",
    lcd_audio_arrived: "EQ_DebugLcdAudioArrivedDuringDrawCount",
    lcd_unexpected: "EQ_DebugLcdUnexpectedFullRedrawCount",
    lcd_bounds: "EQ_DebugLcdBoundsFailureCount",
    lcd_budget_2ms: "EQ_DebugLcdBudgetExceededCount",
    lcd_budget_5ms: "EQ_DebugLcdHardBudgetExceededCount",
    lcd_max_cycles: "EQ_DebugLcdMaxJobCycles",
    lcd_auto_disabled: "EQ_DebugLcdAutoDisabledCount",
    lcd_auto_reason: "EQ_DebugLcdAutoDisableReason",
    ui_state_bytes: "EQ_DebugUiStateBytes",
    touch_state_bytes: "EQ_DebugTouchStateBytes",
    touch_actions: "EQ_DebugTouchActionCount",
    touch_rejected: "EQ_DebugTouchRejectedCount",
    touch_max_cycles: "EQ_DebugTouchMaxCycles"
};

function jsonQuote(value) {
    return "\"" + String(value).replace(/\\/g, "\\\\")
        .replace(/\"/g, "\\\"").replace(/\r/g, "\\r")
        .replace(/\n/g, "\\n").replace(/\t/g, "\\t") + "\"";
}

function jsonStringify(value) {
    var kind, parts, key, index;
    if (value === null || typeof value == "undefined") return "null";
    kind = typeof value;
    if (kind == "string") return jsonQuote(value);
    if (kind == "number") return isFinite(value) ? String(value) : "null";
    if (kind == "boolean") return value ? "true" : "false";
    parts = [];
    if (value instanceof Array) {
        for (index = 0; index < value.length; index++) {
            parts.push(jsonStringify(value[index]));
        }
        return "[" + parts.join(",") + "]";
    }
    for (key in value) {
        parts.push(jsonQuote(key) + ":" + jsonStringify(value[key]));
    }
    return "{" + parts.join(",") + "}";
}

function writeJson(path, value) {
    var writer = new FileWriter(path, false);
    writer.write(jsonStringify(value) + "\n");
    writer.close();
}

function evaluate(expression) {
    return String(debugSession.expression.evaluate(expression));
}

function numberValue(expression) {
    var text = evaluate(expression);
    var hex = text.match(/-?0x[0-9a-fA-F]+/);
    var value = hex ? parseInt(hex[0], 16) : parseFloat(text);
    if (!isFinite(value)) throw "unreadable diagnostic " + expression + "=" + text;
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

function snapshot() {
    var state = {}, key, index;
    for (key in symbols) state[key] = numberValue(symbols[key]);
    state.lcd_category_count = [];
    state.lcd_category_max_cycles = [];
    for (index = 0; index < 4; index++) {
        state.lcd_category_count.push(numberValue(
            "EQ_DebugLcdCategoryCount[" + index + "]"));
        state.lcd_category_max_cycles.push(numberValue(
            "EQ_DebugLcdCategoryMaxCycles[" + index + "]"));
    }
    state.lcd_job_type_count = [];
    state.lcd_job_type_max_cycles = [];
    for (index = 0; index < 15; index++) {
        state.lcd_job_type_count.push(numberValue(
            "EQ_DebugLcdJobTypeCount[" + index + "]"));
        state.lcd_job_type_max_cycles.push(numberValue(
            "EQ_DebugLcdJobTypeMaxCycles[" + index + "]"));
    }
    return state;
}

function delta(after, before, name) {
    return after[name] - before[name];
}

function runFor(milliseconds) {
    debugSession.target.runAsynch();
    Thread.sleep(milliseconds);
    debugSession.target.halt();
}

function configureStep(index) {
    write("EQ_DebugDiagPath = 4");
    write("EQ_DebugMode = " + (index % 5));
    write("EQ_DebugSmartBassEnabled = " + ((index % 4) == 0 ? 0 : 1));
    write("EQ_DebugDynamicClarityEnabled = " + ((index % 5) == 0 ? 0 : 1));
    write("EQ_DebugHarshnessGuardEnabled = " + ((index % 6) == 0 ? 0 : 1));
    write("EQ_DebugSmartBassStrength = " + (1 + (index % 3)));
    write("EQ_DebugDynamicClarityStrength = " + (1 + ((index + 1) % 3)));
    write("EQ_DebugHarshnessGuardStrength = " + (1 + ((index + 2) % 3)));
}

function verifyBuild() {
    requireCondition(numberValue("EQ_DebugBuildMagic") == 0x33030003,
        "build magic mismatch");
    requireCondition(numberValue("EQ_DebugBuildDirty") == 0,
        "build is dirty");
    requireCondition(readCString("EQ_DebugBuildGitSha", 16) == expectedSha,
        "build SHA mismatch");
}

function verifyResult(before, after) {
    var index;
    var ad = delta(after, before, "ad_frames");
    var da = delta(after, before, "da_frames");
    var process = delta(after, before, "process_frames");
    requireCondition(after.init_stage == 11, "INIT did not reach 11");
    requireCondition(ad > 0 && da > 0 && process > 0, "frame counters did not grow");
    requireCondition(Math.abs(ad - da) <= 1 && Math.abs(ad - process) <= 1,
        "AD/DA/process frame mismatch");
    requireCondition(after.deadline == 0 && after.latency == 0 &&
        after.overlap == 0 && after.dropped == 0, "audio safety counter nonzero");
    requireCondition(after.clip == 0, "clip count nonzero");
    requireCondition(after.smart_saturation == 0 && after.smart_nonfinite == 0 &&
        after.clarity_saturation == 0 && after.clarity_nonfinite == 0 &&
        after.guard_saturation == 0 && after.guard_nonfinite == 0,
        "dynamic saturation/nonfinite counter nonzero");
    requireCondition(after.algo_max_cycles < FRAME_BUDGET_CYCLES &&
        after.frame_service_max_cycles < FRAME_BUDGET_CYCLES &&
        after.frame_latency_max_cycles < FRAME_BUDGET_CYCLES,
        "frame budget exceeded");
    requireCondition(after.input_peak > 0 && after.output_peak > 0,
        "PC line-out signal was not observed on ADC/DAC CH1");
    requireCondition(after.analyzer_compiled == 1 && after.analyzer_valid == 1 &&
        after.analyzer_warm == 1 && delta(after, before, "analyzer_analyses") > 0,
        "Analyzer did not publish during stress");
    requireCondition(after.smart_compiled == 1 && after.clarity_compiled == 1 &&
        after.guard_compiled == 1, "dynamic module build state mismatch");
    requireCondition(after.lcd_enabled == 1 && after.lcd_mask == 15,
        "LCD runtime mask changed");
    requireCondition(delta(after, before, "lcd_jobs") > 0,
        "no runtime LCD jobs completed");
    requireCondition(after.lcd_unexpected == 0 && after.lcd_bounds == 0,
        "unexpected or out-of-bounds LCD draw");
    requireCondition(after.lcd_budget_5ms == 0 && after.lcd_auto_disabled == 0 &&
        after.lcd_max_cycles <= LCD_HARD_CYCLES,
        "LCD job exceeded hard budget");
    requireCondition(after.ui_state_bytes > 0 && after.touch_state_bytes > 0,
        "UI/Touch state size diagnostic missing");
    for (index = 0; index < 4; index++) {
        requireCondition(after.lcd_category_count[index] >
            before.lcd_category_count[index],
            "LCD category did not run: " + index);
        requireCondition(after.lcd_category_max_cycles[index] <= LCD_HARD_CYCLES,
            "LCD category exceeded hard budget: " + index);
    }
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
    Thread.sleep(500);
    runFor(3000);
    verifyBuild();
    requireCondition(numberValue("EQ_DebugInitStage") == 11,
        "initialization failed");
    write("EQ_DebugLcdRuntimeMask = 15");
    write("EQ_DebugAnalyzerEnabled = 1");
    write("EQ_DebugAnalyzerResetRequest = 1");
    runFor(2000);
    before = snapshot();

    for (var step = 0; step < 15; step++) {
        configureStep(step);
        runFor(2000);
    }
    write("EQ_DebugMode = 4");
    write("EQ_DebugSmartBassEnabled = 1");
    write("EQ_DebugDynamicClarityEnabled = 1");
    write("EQ_DebugHarshnessGuardEnabled = 1");
    runFor(30000);
    after = snapshot();
    verifyResult(before, after);
    summary = {
        result_label: "MEASURED_ON_CURRENT_BOARD",
        pass: true,
        duration_ms: 60000,
        build_sha: expectedSha,
        before: before,
        after: after,
        notes: "Objective DSS Watch; no visual, touch, or listening claim"
    };
    writeJson(resultPath, summary);
    writeJson(summaryPath, summary);
} catch (error) {
    summary = {
        result_label: "FAIL",
        pass: false,
        duration_ms: 60000,
        build_sha: expectedSha,
        error: String(error),
        before: before,
        after: after
    };
    writeJson(summaryPath, summary);
    throw error;
} finally {
    try {
        if (debugSession != null) debugSession.target.disconnect();
    } catch (ignore1) {}
    try {
        if (debugSession != null) debugSession.terminate();
    } catch (ignore2) {}
    try {
        if (debugServer != null) debugServer.stop();
    } catch (ignore3) {}
}
