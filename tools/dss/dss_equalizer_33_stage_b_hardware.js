/* Bounded Project 3.3 Stage B validation for TMS320C6748/XDS100v3. */
importPackage(Packages.com.ti.debug.engine.scripting);
importPackage(Packages.com.ti.ccstudio.scripting.environment);
importPackage(Packages.java.lang);
importPackage(Packages.java.io);

var root = env("DSP_TEST_ROOT", String(new File(".").getCanonicalPath()).replace(/\\/g, "/"));
var ccxml = env("DSP_TEST_CCXML", root + "/TargetConfig/C6748.ccxml");
var program = env("DSP_TEST_PROGRAM", root + "/Debug/DSP_LAB_0507.out");
var resultDir = env("DSP_TEST_RESULT_DIR", String(System.getProperty("java.io.tmpdir")));
var audioSyncDir = env("DSP_TEST_AUDIO_SYNC_DIR", resultDir + "/audio_sync");
var expectedSha = env("DSP_TEST_EXPECTED_SHA", "UNKNOWN");
var operatorStatus = env("DSP_TEST_OPERATOR_STATUS", "NOT_OBSERVED");
var operatorNotes = env("DSP_TEST_OPERATOR_NOTES", "");
var testStage = env("DSP_TEST_STAGE", "CrossfadeA");
var resultPath = resultDir + "/stage_results.jsonl";
var summaryPath = resultDir + "/summary.json";
var script = null;
var debugServer = null;
var debugSession = null;
var results = null;
var completed = false;
var FRAME_BUDGET_CYCLES = 9338880;
var LCD_HARD_CYCLES = 2280000;
var LABEL_BOARD = "MEASURED_ON_CURRENT_BOARD";
var LABEL_DEBUG = "DEBUG_CONTROL_FUNCTIONAL";
var LABEL_NOT_OBSERVED = "NOT_OBSERVED";
var LABEL_PENDING = "PENDING_HARDWARE";
var LABEL_SUBJECTIVE = "SUBJECTIVE_SPEAKER_OBSERVATION";
var LABEL_FAIL = "FAIL";

var symbols = {
    ad_frames: "EQ_DebugAdFrames",
    da_frames: "EQ_DebugDaFrames",
    process_frames: "EQ_DebugProcessFrames",
    init_stage: "EQ_DebugInitStage",
    init_flag_ad_done: "EQ_DebugFlagAdDone",
    algo_last_cycles: "EQ_DebugAlgoLastCycles",
    algo_max_cycles: "EQ_DebugAlgoMaxCycles",
    mode_service_last_cycles: "EQ_DebugModeServiceLastCycles",
    mode_service_max_cycles: "EQ_DebugModeServiceMaxCycles",
    frame_service_last_cycles: "EQ_DebugFrameServiceLastCycles",
    frame_service_max_cycles: "EQ_DebugFrameServiceMaxCycles",
    frame_latency_last_cycles: "EQ_DebugFrameLatencyLastCycles",
    frame_latency_max_cycles: "EQ_DebugFrameLatencyMaxCycles",
    deadline_miss: "EQ_DebugDeadlineMissCount",
    latency_deadline_miss: "EQ_DebugFrameLatencyDeadlineMissCount",
    service_overlap: "EQ_DebugFrameServiceOverlapCount",
    service_dropped: "EQ_DebugFrameServiceDroppedCount",
    clip_count: "EQ_DebugClipCount",
    raw_mismatch: "EQ_DebugRawCopyMismatchCount",
    float_copy_max_error: "EQ_DebugFloatCopyMaxError",
    input_peak: "EQ_DebugInputPeak",
    output_peak: "EQ_DebugOutputPeak",
    predicted_peak_db: "EQ_DebugPredictedPeakDb",
    preamp_db: "EQ_DebugPreampDb",
    headroom_scan_count: "EQ_DebugHeadroomScanCount",
    requested_mode: "EQ_DebugRequestedMode",
    transition_target_mode: "EQ_DebugTransitionTargetMode",
    applied_mode: "EQ_DebugAppliedMode",
    mode_change_count: "EQ_DebugModeChangeCount",
    request_token: "EQ_DebugControlRequestToken",
    observed_token: "EQ_DebugControlObservedToken",
    accepted_token: "EQ_DebugControlAcceptedToken",
    target_token: "EQ_DebugControlTargetToken",
    prepared_token: "EQ_DebugControlPreparedToken",
    ready_valid: "EQ_DebugControlReadyValid",
    installed_token: "EQ_DebugControlInstalledToken",
    applied_token: "EQ_DebugControlAppliedToken",
    installed_pair_valid: "EQ_DebugControlInstalledPairValid",
    rejected_count: "EQ_DebugControlRejectedCount",
    coalesced_count: "EQ_DebugControlCoalescedCount",
    control_last_error: "EQ_DebugControlLastError",
    builder_state: "EQ_DebugBuilderState",
    builder_section_index: "EQ_DebugBuilderSectionIndex",
    builder_scan_index: "EQ_DebugBuilderScanIndex",
    builder_generation: "EQ_DebugBuilderGeneration",
    builder_request_token: "EQ_DebugBuilderRequestToken",
    builder_slice_count: "EQ_DebugBuilderSliceCount",
    builder_cancel_count: "EQ_DebugBuilderCancelCount",
    builder_restart_count: "EQ_DebugBuilderRestartCount",
    builder_last_error: "EQ_DebugBuilderLastError",
    builder_last_cycles: "EQ_DebugBuilderLastCycles",
    builder_max_cycles: "EQ_DebugBuilderMaxCycles",
    builder_deferred_audio: "EQ_DebugBuilderDeferredAudioCount",
    builder_audio_during_slice: "EQ_DebugBuilderAudioArrivedDuringSliceCount",
    response_active_generation: "EQ_DebugResponseActiveGeneration",
    response_target_generation: "EQ_DebugResponseTargetGeneration",
    response_transition_active: "EQ_DebugResponseTransitionActive",
    response_transition_progress: "EQ_DebugResponseTransitionProgress",
    response_active_path: "EQ_DebugResponseActivePathType",
    response_target_valid: "EQ_DebugResponseTargetValid",
    lcd_enabled: "EQ_DebugLcdEnabled",
    lcd_runtime_mask: "EQ_DebugLcdRuntimeMask",
    lcd_pending_mask: "EQ_DebugLcdPendingMask",
    lcd_job_count: "EQ_DebugLcdJobCount",
    lcd_deferred_audio: "EQ_DebugLcdDeferredAudioCount",
    lcd_audio_during_draw: "EQ_DebugLcdAudioArrivedDuringDrawCount",
    lcd_unexpected_full_redraw: "EQ_DebugLcdUnexpectedFullRedrawCount",
    lcd_budget_exceeded: "EQ_DebugLcdBudgetExceededCount",
    lcd_last_job_cycles: "EQ_DebugLcdLastJobCycles",
    lcd_max_job_cycles: "EQ_DebugLcdMaxJobCycles",
    lcd_auto_disabled: "EQ_DebugLcdAutoDisabledCount",
    lcd_auto_disable_reason: "EQ_DebugLcdAutoDisableReason"
};

function env(name, fallback) {
    var value = String(System.getenv(name));
    return (value == "null" || value == "") ? fallback : value;
}

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

function evaluate(expression) {
    return String(debugSession.expression.evaluate(expression));
}

function numberValue(expression) {
    var text = evaluate(expression);
    var hex = text.match(/-?0x[0-9a-fA-F]+/);
    var value = hex ? parseInt(hex[0], 16) : parseFloat(text);
    if (!isFinite(value)) throw "Unreadable diagnostic " + expression + "=" + text;
    return value;
}

function optionalNumberValue(expression) {
    try {
        return numberValue(expression);
    } catch (error) {
        return null;
    }
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
    var state = {}, key;
    for (key in symbols) {
        state[key] = key.indexOf("lcd_") == 0 && testStage == "CrossfadeA" ?
            null : numberValue(symbols[key]);
    }
    state.lcd_category_count = [];
    state.lcd_category_max_cycles = [];
    for (key = 0; key < 6; key++) {
        state.lcd_category_count.push(testStage == "CrossfadeA" ? null :
            numberValue("EQ_DebugLcdCategoryCount[" + key + "]"));
        state.lcd_category_max_cycles.push(testStage == "CrossfadeA" ? null :
            numberValue("EQ_DebugLcdCategoryMaxCycles[" + key + "]"));
    }
    state.flag_ad = optionalNumberValue("FLAG_AD");
    state.flag_da = optionalNumberValue("FLAG_DA");
    state.cpu_ier = optionalNumberValue("IER");
    state.edma_event_enable = optionalNumberValue(
        "*((unsigned int *)0x01C01020)");
    state.edma_interrupt_enable = optionalNumberValue(
        "*((unsigned int *)0x01C01050)");
    return state;
}

function delta(after, before, name) {
    return after[name] - before[name];
}

function writeResult(name, label, durationMs, before, after, detail) {
    var metrics = null;
    if (after != null && before != null &&
        typeof after.ad_frames != "undefined" &&
        typeof before.ad_frames != "undefined") {
        metrics = {
            algo_max_cycles: after.algo_max_cycles,
            frame_service_max_cycles: after.frame_service_max_cycles,
            frame_latency_max_cycles: after.frame_latency_max_cycles,
            deadline_delta: delta(after, before, "deadline_miss"),
            latency_deadline_delta: delta(after, before, "latency_deadline_miss"),
            overlap_delta: delta(after, before, "service_overlap"),
            dropped_delta: delta(after, before, "service_dropped"),
            clip_delta: delta(after, before, "clip_count"),
            ad_delta: delta(after, before, "ad_frames"),
            da_delta: delta(after, before, "da_frames"),
            process_delta: delta(after, before, "process_frames")
        };
    }
    results.write(jsonStringify({
        stage: name,
        result_label: label,
        duration_ms: durationMs,
        before: before,
        after: after,
        metrics: metrics,
        detail: detail || ""
    }) + "\n");
    results.flush();
}

function checkCommon(name, before, after) {
    var ad = delta(after, before, "ad_frames");
    var da = delta(after, before, "da_frames");
    var process = delta(after, before, "process_frames");
    requireCondition(ad > 0 && da > 0 && process > 0, name + ": frame counters did not grow");
    requireCondition(Math.abs(ad - da) <= 1 && Math.abs(ad - process) <= 1,
                     name + ": AD/DA/process frame mismatch");
    requireCondition(delta(after, before, "deadline_miss") == 0,
                     name + ": active deadline miss increased");
    requireCondition(delta(after, before, "latency_deadline_miss") == 0,
                     name + ": latency deadline miss increased");
    requireCondition(delta(after, before, "service_overlap") == 0,
                     name + ": service overlap increased");
    requireCondition(delta(after, before, "service_dropped") == 0,
                     name + ": service dropped increased");
    requireCondition(delta(after, before, "clip_count") == 0 && after.clip_count == 0,
                     name + ": clip count is nonzero");
    requireCondition(delta(after, before, "raw_mismatch") == 0,
                     name + ": RAW mismatch increased");
    requireCondition(delta(after, before, "lcd_auto_disabled") == 0,
                     name + ": LCD auto-disable increased");
    requireCondition(delta(after, before, "lcd_unexpected_full_redraw") == 0,
                     name + ": unexpected LCD full redraw increased");
    requireCondition(delta(after, before, "lcd_budget_exceeded") == 0,
                     name + ": LCD budget-exceeded count increased");
    requireCondition(after.algo_max_cycles < FRAME_BUDGET_CYCLES,
                     name + ": algorithm maximum exceeds budget");
    requireCondition(after.frame_service_max_cycles < FRAME_BUDGET_CYCLES,
                     name + ": frame service maximum exceeds budget");
    requireCondition(after.frame_latency_max_cycles < FRAME_BUDGET_CYCLES,
                     name + ": frame latency maximum exceeds budget");
}

function clearDiagnosticPeaks() {
    write("EQ_DebugAlgoMaxCycles = 0");
    write("EQ_DebugFrameServiceMaxCycles = 0");
    write("EQ_DebugFrameLatencyMaxCycles = 0");
}

function runFor(milliseconds) {
    debugSession.target.runAsynch();
    Thread.sleep(milliseconds);
    debugSession.target.halt();
}

function runWindow(name, durationMs, label, extraCheck, detail) {
    var before = snapshot();
    runFor(durationMs);
    var after = snapshot();
    try {
        checkCommon(name, before, after);
        if (extraCheck != null) extraCheck(before, after);
    } catch (error) {
        writeResult(name, LABEL_FAIL, durationMs, before, after, String(error));
        throw error;
    }
    writeResult(name, label, durationMs, before, after, detail);
    return after;
}

function runMeasuredWindow(name, durationMs, label, extraCheck, detail) {
    clearDiagnosticPeaks();
    return runWindow(name, durationMs, label, extraCheck, detail);
}

function setPath(path, mode) {
    if (path != 0 && path != 1) write("EQ_DebugMode = " + mode);
    write("EQ_DebugDiagPath = " + path);
}

function publishAll(gains) {
    var current = numberValue("EQ_ControlMailbox.sequence");
    var odd, even, band;
    requireCondition((current % 2) == 0, "mailbox sequence is odd while halted");
    odd = current >= 4294967294 ? 1 : current + 1;
    even = current >= 4294967294 ? 2 : current + 2;
    write("EQ_ControlMailbox.sequence = " + odd);
    write("EQ_ControlMailbox.command = 4");
    write("EQ_ControlMailbox.band = 0");
    write("EQ_ControlMailbox.preset = 5");
    write("EQ_ControlMailbox.value_db = 0.0");
    write("EQ_ControlMailbox.step_db = 0.0");
    for (band = 0; band < 10; band++) {
        write("EQ_ControlMailbox.shadow_gain_db[" + band + "] = " + gains[band]);
    }
    write("EQ_ControlMailbox.sequence = " + even);
    return even;
}

function requestAudio(kind) {
    var ready = new File(audioSyncDir + "/" + kind + ".ready");
    var playing = new File(audioSyncDir + "/" + kind + ".playing");
    var writer, waited = 0;
    if (ready.exists()) ready["delete"]();
    if (playing.exists()) playing["delete"]();
    writer = new FileWriter(ready, false);
    writer.write("requested=" + kind + "\n");
    writer.close();
    while (!playing.exists() && waited < 5000) {
        Thread.sleep(50);
        waited += 50;
    }
    requireCondition(playing.exists(), "audio playback acknowledgement timeout: " + kind);
    Thread.sleep(250);
}

function verifyBuildIdentity() {
    var magic = numberValue("EQ_DebugBuildMagic");
    var dirty = numberValue("EQ_DebugBuildDirty");
    var buildId = readCString("EQ_DebugBuildId", 32);
    var version = readCString("EQ_DebugBuildVersion", 32);
    var sha = readCString("EQ_DebugBuildGitSha", 16);
    requireCondition(magic == 0x33030003, "build magic mismatch: " + magic);
    requireCondition(dirty == 0, "build dirty is not zero: " + dirty);
    requireCondition(sha == expectedSha, "build SHA mismatch: " + sha);
    requireCondition(buildId == "P33_FIX_V5" && version == "P33_FIX_V5",
                     "build version mismatch: " + buildId + "/" + version);
    writeResult("build_identity", LABEL_BOARD, 0, {}, {},
                "magic=0x33030003;id=" + buildId + ";sha=" + sha + ";dirty=0");
}

function checkExpectedPath(expectedPath) {
    return function(before, after) {
        requireCondition(after.response_active_path == expectedPath,
                         "response path mismatch expected=" + expectedPath +
                         " actual=" + after.response_active_path);
        requireCondition(after.response_transition_active == 0,
                         "transition did not settle");
    };
}

function checkSettledRbj(before, after) {
    requireCondition(after.response_active_path == 3 ||
                     after.response_active_path == 5,
                     "settled RBJ response path is neither identity nor active bank");
    requireCondition(after.response_transition_active == 0,
                     "RBJ return transition did not settle");
}

function checkPresetSettled(expectedMode) {
    return function(before, after) {
        var expectedPath = expectedMode == 0 ? 3 : 5;
        requireCondition(after.applied_mode == expectedMode,
                         "applied preset mismatch expected=" + expectedMode +
                         " actual=" + after.applied_mode);
        requireCondition(after.response_active_path == expectedPath,
                         "settled preset path mismatch expected=" + expectedPath +
                         " actual=" + after.response_active_path);
        requireCondition(after.response_transition_active == 0,
                         "preset transition did not settle");
        requireCondition(after.applied_token == after.target_token &&
                         after.applied_token == after.installed_token,
                         "applied/target/installed token mismatch");
    };
}

function runInitializationDiagnostic() {
    var before = snapshot();
    var after;
    var detail = "milestones:1=flow,2=ADC init returned,3=DAC init returned," +
        "4=equalizer/cache ready,5=runtime ready,6=ADC started," +
        "7=DAC started,8=loop,9=first AD,10=first process,11=first DA";
    runFor(2000);
    after = snapshot();
    try {
        requireCondition(after.init_stage == 11,
                         "initialization milestone stopped at " + after.init_stage);
        requireCondition(delta(after, before, "ad_frames") > 0 &&
                         delta(after, before, "da_frames") > 0 &&
                         delta(after, before, "process_frames") > 0,
                         "initialization frame counters did not grow");
        requireCondition(Math.abs(delta(after, before, "ad_frames") -
                                  delta(after, before, "da_frames")) <= 1 &&
                         Math.abs(delta(after, before, "ad_frames") -
                                  delta(after, before, "process_frames")) <= 1,
                         "initialization AD/DA/process frame mismatch");
    } catch (error) {
        writeResult("target_initialization", LABEL_FAIL, 2000,
                    before, after, String(error));
        throw error;
    }
    writeResult("target_initialization", LABEL_BOARD, 2000,
                before, after, detail);
    return after;
}

function runCrossfadeA() {
    var cycle;
    write("EQ_DebugLcdRuntimeMask = 0");
    requestAudio("tone");
    setPath(4, 0);
    for (cycle = 1; cycle <= 3; cycle++) {
        write("EQ_DebugMode = 0");
        runMeasuredWindow("CrossfadeA_" + cycle + "_FLAT_before", 20000,
                          LABEL_BOARD, checkPresetSettled(0),
                          "cycle=" + cycle + ";FLAT stable");
        write("EQ_DebugMode = 1");
        runMeasuredWindow("CrossfadeA_" + cycle + "_BASS", 20000,
                          LABEL_BOARD, checkPresetSettled(1),
                          "cycle=" + cycle + ";FLAT>BASS");
        write("EQ_DebugMode = 0");
        runMeasuredWindow("CrossfadeA_" + cycle + "_FLAT_after", 20000,
                          LABEL_BOARD, checkPresetSettled(0),
                          "cycle=" + cycle + ";BASS>FLAT");
    }
}

function runLcdStatusOnly() {
    write("EQ_DebugLcdRuntimeMask = 1");
    requestAudio("music");
    runMeasuredWindow("F_LCD_STATUS", 60000, LABEL_BOARD,
        function(before, after) {
            requireCondition(after.lcd_enabled == 1 && after.lcd_runtime_mask == 1,
                             "LCD STATUS runtime mask mismatch");
            requireCondition(delta(after, before, "lcd_job_count") > 0,
                             "LCD STATUS jobs did not run");
            requireCondition(after.lcd_max_job_cycles <= LCD_HARD_CYCLES,
                             "LCD STATUS job exceeded hard 5 ms limit");
        }, "STATUS only;no builder or additional LCD load");
}

function runFullValidation() {
    requestAudio("tone");
    runAudioMatrix("TONE_1K_-18DBFS");
    requireCondition(numberValue("EQ_DebugInputPeak") > 0 &&
                     numberValue("EQ_DebugOutputPeak") > 0,
                     "audio input/output peak was not observed");
    requestAudio("music");
    runAudioMatrix("MUSIC");
    runFixedPresetCache();
    runCustomBuilder();
    runLatestWins();
    runIdentityReturns();
    runLcdSharedBudget();
    runSwitching("G_mode_switch_5s", 5000);
    runSwitching("G_mode_switch_2s", 2000);
    runWindow("H_stability_5min", 300000, LABEL_BOARD, function(before, after) {
        requireCondition(after.lcd_runtime_mask == 3 &&
                         delta(after, before, "lcd_job_count") > 0,
                         "stability LCD BOTH did not remain active");
    }, "music;LCD BOTH;no control writes during window");
}

function runAudioMatrix(prefix) {
    var custom = [6.0, 3.0, 0.0, -2.0, -4.0, -4.0, -2.0, 0.0, 3.0, 6.0];
    var token;
    setPath(0, 0);
    runWindow("A_" + prefix + "_RAW", 20000, LABEL_BOARD, checkExpectedPath(0), "RAW_COPY");
    setPath(1, 0);
    runWindow("A_" + prefix + "_FLOAT", 20000, LABEL_BOARD, function(before, after) {
        checkExpectedPath(1)(before, after);
        requireCondition(after.float_copy_max_error <= 1.0,
                         "FLOAT_COPY error exceeds one LSB");
    }, "FLOAT_COPY");
    setPath(2, 0);
    runWindow("A_" + prefix + "_FLAT", 20000, LABEL_BOARD, checkExpectedPath(3), "FLAT");
    setPath(4, 1);
    runWindow("A_" + prefix + "_BASS", 20000, LABEL_BOARD, checkExpectedPath(5), "BASS");
    setPath(4, 2);
    runWindow("A_" + prefix + "_VOCAL", 20000, LABEL_BOARD, checkExpectedPath(5), "VOCAL");
    token = publishAll(custom);
    runWindow("A_" + prefix + "_CUSTOM", 20000, LABEL_BOARD, function(before, after) {
        checkExpectedPath(5)(before, after);
        requireCondition(after.applied_token == token && after.target_token == token,
                         "custom token did not reach applied state");
    }, "SET_ALL token=" + token);
}

function runFixedPresetCache() {
    var modes = [0, 1, 2, 3, 4, 0];
    var before = snapshot(), after, index;
    setPath(4, 0);
    for (index = 0; index < modes.length; index++) {
        write("EQ_DebugMode = " + modes[index]);
        runFor(400);
    }
    after = snapshot();
    checkCommon("B_fixed_preset_cache", before, after);
    requireCondition(delta(after, before, "builder_slice_count") == 0,
                     "fixed preset switch invoked builder");
    requireCondition(delta(after, before, "headroom_scan_count") == 0,
                     "fixed preset switch invoked headroom scan");
    requireCondition(after.applied_mode == 0 && after.response_active_path == 3,
                     "fixed preset sequence did not end at FLAT");
    writeResult("B_fixed_preset_cache", LABEL_DEBUG, 2400, before, after,
                "FLAT>BASS>VOCAL>TREBLE>V_SHAPE>FLAT;halted debug writes");
}

function runCustomBuilder() {
    var gains = [-3.0, 1.0, 4.0, 2.0, -1.0, -4.0, -2.0, 2.0, 5.0, 1.0];
    var token = publishAll(gains);
    runWindow("C_custom_43_slice_builder", 2000, LABEL_BOARD, function(before, after) {
        requireCondition(delta(after, before, "builder_slice_count") == 43,
                         "custom builder did not execute exactly 43 slices");
        requireCondition(after.prepared_token == token && after.installed_token == token &&
                         after.applied_token == token && after.installed_pair_valid == 1,
                         "custom token lifecycle incomplete");
        requireCondition(after.builder_last_error == 0 && after.control_last_error == 0,
                         "builder/control error is nonzero");
        requireCondition(after.builder_max_cycles < FRAME_BUDGET_CYCLES,
                         "builder maximum exceeds frame budget");
    }, "SET_ALL token=" + token + ";finalize/install observed at final boundary");
    writeResult("C_finalize_install_exact_boundary", LABEL_PENDING, 0, {}, {},
                "Host verified; exact board boundary requires an intrusive breakpoint");
}

function runLatestWins() {
    var a = [5.0, 4.0, 3.0, 2.0, 1.0, 0.0, -1.0, -2.0, -3.0, -4.0];
    var b = [-4.0, -2.0, 0.0, 2.0, 4.0, 4.0, 2.0, 0.0, -2.0, -4.0];
    var c = [1.0, -1.0, 2.0, -2.0, 3.0, -3.0, 4.0, -4.0, 5.0, -5.0];
    var before = snapshot(), tokenA, tokenB, tokenC, after;
    tokenA = publishAll(a); runFor(100);
    tokenB = publishAll(b); runFor(100);
    tokenC = publishAll(c); runFor(2000);
    after = snapshot();
    checkCommon("D_latest_wins", before, after);
    requireCondition(after.target_token == tokenC && after.prepared_token == tokenC &&
                     after.installed_token == tokenC && after.applied_token == tokenC,
                     "latest request C was not the applied target");
    requireCondition(after.builder_cancel_count > before.builder_cancel_count ||
                     after.builder_restart_count > before.builder_restart_count,
                     "rapid requests did not cancel or restart stale builder work");
    requireCondition(after.control_last_error == 0 && after.builder_last_error == 0,
                     "latest-wins ended with control/builder error");
    writeResult("D_latest_wins", LABEL_DEBUG, 2200, before, after,
                "A=" + tokenA + ";B=" + tokenB + ";C=" + tokenC + ";halted debug writes");
}

function runIdentityReturns() {
    var before, after;
    setPath(0, 0);
    runWindow("E_RAW_enter", 300, LABEL_DEBUG, checkExpectedPath(0), "RAW_COPY");
    setPath(4, 1);
    before = snapshot(); runFor(60); after = snapshot(); checkCommon("E_RAW_return_transition", before, after);
    requireCondition(after.response_transition_active == 1 && after.response_active_path == 4,
                     "RAW return did not enter dry-to-bank transition");
    writeResult("E_RAW_return_transition", LABEL_DEBUG, 60, before, after,
                "dry-to-bank transition;exact identity_hold transient not sampled");
    runWindow("E_RAW_return_settle", 200, LABEL_DEBUG, checkSettledRbj,
              "settled RBJ return");

    setPath(1, 0);
    runWindow("E_FLOAT_enter", 300, LABEL_DEBUG, checkExpectedPath(1), "FLOAT_COPY");
    setPath(4, 2);
    before = snapshot(); runFor(60); after = snapshot(); checkCommon("E_FLOAT_return_transition", before, after);
    requireCondition(after.response_transition_active == 1 && after.response_active_path == 4,
                     "FLOAT return did not enter dry-to-bank transition");
    writeResult("E_FLOAT_return_transition", LABEL_DEBUG, 60, before, after,
                "dry-to-bank transition;exact identity_hold transient not sampled");
    runWindow("E_FLOAT_return_settle", 200, LABEL_DEBUG, checkSettledRbj,
              "settled RBJ return");
    writeResult("E_exact_identity_hold_path", LABEL_NOT_OBSERVED, 0, {}, {},
                "identity_hold is shorter than a debugger-safe fixed observation window");
}

function runLcdSharedBudget() {
    var masks = [1, 2, 3];
    var names = ["STATUS", "GAINS", "BOTH"];
    var curves = [
        [2.0, 1.0, 0.0, -1.0, -2.0, -2.0, -1.0, 0.0, 1.0, 2.0],
        [-2.0, -1.0, 0.0, 1.0, 2.0, 2.0, 1.0, 0.0, -1.0, -2.0],
        [4.0, 2.0, 0.0, -2.0, -4.0, -4.0, -2.0, 0.0, 2.0, 4.0]
    ];
    var index, token;
    for (index = 0; index < masks.length; index++) {
        write("EQ_DebugLcdRuntimeMask = " + masks[index]);
        token = publishAll(curves[index]);
        runWindow("F_LCD_" + names[index], 60000, LABEL_BOARD, function(before, after) {
            requireCondition(after.lcd_enabled == 1 && after.lcd_runtime_mask == masks[index],
                             "LCD runtime mask mismatch");
            requireCondition(delta(after, before, "lcd_job_count") > 0,
                             "LCD jobs did not run");
            requireCondition(after.lcd_max_job_cycles <= LCD_HARD_CYCLES,
                             "LCD job exceeded hard 5 ms limit");
            requireCondition(delta(after, before, "builder_slice_count") == 43,
                             "custom builder did not receive shared background budget");
            requireCondition(after.applied_token == token,
                             "LCD/custom token did not apply");
        }, "mask=" + masks[index] + ";SET_ALL token=" + token);
    }
}

function runSwitching(name, intervalMs) {
    var sequence = [0, 1, 2, 3, 4, 0];
    var before = snapshot(), after, index;
    setPath(4, sequence[0]);
    for (index = 0; index < sequence.length; index++) {
        write("EQ_DebugMode = " + sequence[index]);
        runFor(intervalMs);
    }
    after = snapshot();
    checkCommon(name, before, after);
    requireCondition(after.applied_mode == 0 && after.response_transition_active == 0,
                     name + ": final FLAT did not settle");
    writeResult(name, LABEL_DEBUG, intervalMs * sequence.length, before, after,
                "interval_ms=" + intervalMs + ";halted debugger writes");
}

try {
    requireCondition(new File(ccxml).exists(), "missing CCXML");
    requireCondition(new File(program).exists(), "missing program");
    new File(resultDir).mkdirs();
    results = new FileWriter(resultPath, false);
    script = ScriptingEnvironment.instance();
    script.setScriptTimeout(20 * 60 * 1000);
    script.traceSetConsoleLevel(TraceLevel.INFO);
    debugServer = script.getServer("DebugServer.1");
    debugServer.setConfig(ccxml);
    debugSession = debugServer.openSession(".*C674X_0");
    debugSession.target.connect();
    debugSession.target.reset();
    debugSession.memory.loadProgram(program);
    verifyBuildIdentity();
    runInitializationDiagnostic();
    if (testStage == "CrossfadeA") {
        runCrossfadeA();
    } else if (testStage == "LcdStatus") {
        runLcdStatusOnly();
    } else if (testStage == "Full") {
        runFullValidation();
    } else {
        throw "unsupported validation stage: " + testStage;
    }
    writeResult("subjective_speaker_observation",
                operatorStatus == LABEL_SUBJECTIVE ? LABEL_SUBJECTIVE :
                                                     LABEL_NOT_OBSERVED,
                0, {}, {}, operatorNotes);
    var finalState = snapshot();
    var summary = new FileWriter(summaryPath, false);
    summary.write(jsonStringify({
        result_label: LABEL_BOARD,
        commit_sha: expectedSha,
        stage: testStage,
        dirty: 0,
        operator_status: operatorStatus,
        operator_notes: operatorNotes,
        final_state: finalState
    }) + "\n");
    summary.close();
    completed = true;
} catch (error) {
    try {
        var failureState = {};
        try { failureState = snapshot(); } catch (snapshotError) {}
        if (results != null) writeResult("hardware_validation", LABEL_FAIL, 0, {}, {}, String(error));
        var failed = new FileWriter(summaryPath, false);
        failed.write(jsonStringify({result_label: LABEL_FAIL, error: String(error),
                                     commit_sha: expectedSha, stage: testStage,
                                     final_state: failureState}) + "\n");
        failed.close();
    } catch (writeError) {}
    System.err.println("DSS_ERROR=" + error);
    throw error;
} finally {
    try { if (debugSession != null) debugSession.target.halt(); } catch (ignored) {}
    try { if (debugSession != null) debugSession.target.disconnect(); } catch (ignored2) {}
    try { if (debugSession != null) debugSession.terminate(); } catch (ignored3) {}
    try { if (debugServer != null) debugServer.stop(); } catch (ignored4) {}
    if (results != null) results.close();
    if (!completed) System.err.println("Stage B hardware validation stopped before completion.");
}
