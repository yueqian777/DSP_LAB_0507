/* Project 3.3 final board-acceptance collector for TI C6748 DSS. */
importPackage(Packages.com.ti.debug.engine.scripting);
importPackage(Packages.com.ti.ccstudio.scripting.environment);
importPackage(Packages.java.lang);
importPackage(Packages.java.io);

function env(name, fallback) {
    var value = String(System.getenv(name));
    return (value == "null" || value == "") ? fallback : value;
}

var hardwareAuthorization = env(
    "DSP_FINAL_ACCEPTANCE_HARDWARE_AUTH", "NOT_AUTHORIZED");
if (hardwareAuthorization != "C6748_CONNECTED_FINAL_ACCEPTANCE") {
    System.out.println("NOT_RUN_NO_HARDWARE");
    System.exit(0);
}

var root = env("DSP_TEST_ROOT",
    String(new File(".").getCanonicalPath()).replace(/\\/g, "/"));
var ccxml = env("DSP_TEST_CCXML", root + "/TargetConfig/C6748.ccxml");
var program = env("DSP_TEST_PROGRAM", root + "/Debug/DSP_LAB_0507.out");
var resultDir = env("DSP_TEST_RAW_DIR",
    String(System.getProperty("java.io.tmpdir")));
var expectedSha = env("DSP_TEST_EXPECTED_SHA", "").toLowerCase();
var expectedProgramSha = env("DSP_TEST_PROGRAM_SHA256", "").toLowerCase();
var stage = env("DSP_FINAL_ACCEPTANCE_STAGE", "");
var interactionTimeoutSeconds = parseInt(
    env("DSP_TEST_INTERACTION_TIMEOUT_SECONDS", "180"), 10);
var timingSamplesPerClass = parseInt(
    env("DSP_TEST_TIMING_SAMPLES_PER_CLASS", "8"), 10);
var enduranceStartDelaySeconds = parseInt(
    env("DSP_TEST_ENDURANCE_START_DELAY_SECONDS", "15"), 10);

var STAGE_FILES = {
    page_switch_30: "page_switch_30",
    lcd_job_timing: "lcd_job_timing",
    endurance_300s: "endurance_300s",
    analyzer_reset: "analyzer_reset"
};

var PAGE_STATUS = 0;
var PAGE_EDITOR = 1;
var PAGE_ROUND_TRIPS = 30;
var ENDURANCE_WINDOW_MILLISECONDS = 302000;
var LCD_TIMING_CLASS_COUNT = 9;
var LCD_TIMING_SAMPLE_CAPACITY = 256;
var LCD_TIMING_CLASSES = [
    "preset",
    "analyzer_strip",
    "dynamic_enabled",
    "dynamic_strength",
    "dynamic_active",
    "editor_band",
    "editor_field",
    "page_sync",
    "page_swap"
];

var ACTION_NAMES = {
    1: "PRESET_FLAT",
    2: "PRESET_BASS",
    3: "PRESET_VOCAL",
    4: "PRESET_TREBLE",
    5: "PRESET_V_SHAPE",
    6: "SMART_TOGGLE",
    7: "SMART_STRENGTH",
    8: "CLARITY_TOGGLE",
    9: "CLARITY_STRENGTH",
    10: "GUARD_TOGGLE",
    11: "GUARD_STRENGTH",
    12: "PAGE_SWITCH",
    13: "EDITOR_BAND_31",
    14: "EDITOR_BAND_63",
    15: "EDITOR_BAND_125",
    16: "EDITOR_BAND_250",
    17: "EDITOR_BAND_500",
    18: "EDITOR_BAND_1K",
    19: "EDITOR_BAND_2K",
    20: "EDITOR_BAND_4K",
    21: "EDITOR_BAND_8K",
    22: "EDITOR_BAND_16K",
    23: "EDITOR_MINUS",
    24: "EDITOR_PLUS",
    25: "EDITOR_APPLY",
    26: "EDITOR_FLAT"
};

var HITBOXES = [
    {id: "status_preset_flat", page: PAGE_STATUS, action: 1,
     pointHint: "edge"},
    {id: "status_preset_bass", page: PAGE_STATUS, action: 2},
    {id: "status_preset_vocal", page: PAGE_STATUS, action: 3},
    {id: "status_preset_treble", page: PAGE_STATUS, action: 4},
    {id: "status_preset_v_shape", page: PAGE_STATUS, action: 5},
    {id: "status_smart_toggle", page: PAGE_STATUS, action: 6},
    {id: "status_smart_strength", page: PAGE_STATUS, action: 7,
     longHold: true},
    {id: "status_clarity_toggle", page: PAGE_STATUS, action: 8},
    {id: "status_clarity_strength", page: PAGE_STATUS, action: 9},
    {id: "status_guard_toggle", page: PAGE_STATUS, action: 10},
    {id: "status_guard_strength", page: PAGE_STATUS, action: 11},
    {id: "status_editor", page: PAGE_STATUS, action: 12},
    {id: "editor_band_31", page: PAGE_EDITOR, action: 13,
     pointHint: "edge"},
    {id: "editor_band_63", page: PAGE_EDITOR, action: 14},
    {id: "editor_band_125", page: PAGE_EDITOR, action: 15},
    {id: "editor_band_250", page: PAGE_EDITOR, action: 16},
    {id: "editor_band_500", page: PAGE_EDITOR, action: 17},
    {id: "editor_band_1k", page: PAGE_EDITOR, action: 18},
    {id: "editor_band_2k", page: PAGE_EDITOR, action: 19},
    {id: "editor_band_4k", page: PAGE_EDITOR, action: 20},
    {id: "editor_band_8k", page: PAGE_EDITOR, action: 21},
    {id: "editor_band_16k", page: PAGE_EDITOR, action: 22},
    {id: "editor_minus", page: PAGE_EDITOR, action: 23},
    {id: "editor_plus", page: PAGE_EDITOR, action: 24},
    {id: "editor_apply", page: PAGE_EDITOR, action: 25},
    {id: "editor_flat", page: PAGE_EDITOR, action: 26},
    {id: "editor_status", page: PAGE_EDITOR, action: 12}
];

var REQUIRED_SYMBOLS = {
    build_dirty: "EQ_DebugBuildDirty",
    init_stage: "EQ_DebugInitStage",
    ad_frames: "EQ_DebugAdFrames",
    da_frames: "EQ_DebugDaFrames",
    process_frames: "EQ_DebugProcessFrames",
    algorithm_frames: "EQ_DebugProcessFrames",
    algorithm_max_cycles: "EQ_DebugAlgoMaxCycles",
    frame_service_max_cycles: "EQ_DebugFrameServiceMaxCycles",
    frame_latency_max_cycles: "EQ_DebugFrameLatencyMaxCycles",
    deadline_misses: "EQ_DebugDeadlineMissCount",
    latency_misses: "EQ_DebugFrameLatencyDeadlineMissCount",
    frame_overlaps: "EQ_DebugFrameServiceOverlapCount",
    dropped_frames: "EQ_DebugFrameServiceDroppedCount",
    clip_count: "EQ_DebugClipCount",
    invalid_count: "EQ_DebugControlRejectedCount",
    static_dynamic_overlap_frames:
        "EQ_DebugStaticDynamicTransitionOverlapFrameCount",
    applied_mode: "EQ_DebugAppliedMode",
    control_request_token: "EQ_DebugControlRequestToken",
    control_applied_token: "EQ_DebugControlAppliedToken",

    touch_down: "Touch_DebugDownCount",
    touch_release: "Touch_DebugReleaseCount",
    touch_i2c_errors: "Touch_DebugI2cErrorCount",
    touch_actions: "EQ_DebugTouchActionCount",
    touch_rejected: "EQ_DebugTouchRejectedCount",
    touch_invalid_coordinates: "EQ_DebugTouchInvalidCoordinateCount",
    touch_preset_requests: "EQ_DebugTouchPresetRequestCount",
    touch_dynamic_enable_requests:
        "EQ_DebugTouchDynamicEnableRequestCount",
    touch_dynamic_strength_requests:
        "EQ_DebugTouchDynamicStrengthRequestCount",
    touch_editor_actions: "EQ_DebugTouchEditorActionCount",
    touch_duplicate_actions: "EQ_DebugTouchDuplicateActionCount",
    touch_page_requests: "EQ_DebugUiPageSwitchCount",
    touch_raw_x: "EQ_DebugTouchRawX",
    touch_raw_y: "EQ_DebugTouchRawY",
    touch_screen_x: "EQ_DebugTouchScreenX",
    touch_screen_y: "EQ_DebugTouchScreenY",
    touch_pressed: "EQ_DebugTouchPressed",
    touch_last_action: "EQ_DebugTouchLastAction",

    requested_page: "EQ_DebugUiRequestedPage",
    displayed_page: "EQ_DebugUiDisplayedPage",
    page_building: "EQ_DebugUiPageBuilding",

    analyzer_compiled: "EQ_DebugAnalyzerCompiled",
    analyzer_enabled: "EQ_DebugAnalyzerEnabled",
    analyzer_valid: "EQ_DebugAnalyzerValid",
    analyzer_warm: "EQ_DebugAnalyzerWarmup",
    analyzer_epoch: "EQ_DebugAnalyzerEpoch",
    analyzer_publications: "EQ_DebugAnalyzerPublicationCount",
    analyzer_runs: "EQ_DebugAnalyzerRunCount",

    smart_enabled: "EQ_DebugSmartBassEnabled",
    smart_strength: "EQ_DebugSmartBassStrength",
    smart_active: "EQ_DebugSmartBassProcessingActive",
    smart_requested_level: "EQ_DebugSmartBassRequestedLevel",
    smart_applied_level: "EQ_DebugSmartBassAppliedLevel",
    smart_transition: "EQ_DebugSmartBassTransitionActive",
    smart_decisions: "EQ_DebugSmartBassDecisionCount",
    smart_saturation: "EQ_DebugSmartBassSaturationCount",
    smart_nonfinite: "EQ_DebugSmartBassNonFiniteCount",

    clarity_enabled: "EQ_DebugDynamicClarityEnabled",
    clarity_strength: "EQ_DebugDynamicClarityStrength",
    clarity_active: "EQ_DebugDynamicClarityProcessingActive",
    clarity_requested_level: "EQ_DebugDynamicClarityRequestedLevel",
    clarity_applied_level: "EQ_DebugDynamicClarityAppliedLevel",
    clarity_transition: "EQ_DebugDynamicClarityTransitionActive",
    clarity_decisions: "EQ_DebugDynamicClarityDecisionCount",
    clarity_saturation: "EQ_DebugDynamicClaritySaturationCount",
    clarity_nonfinite: "EQ_DebugDynamicClarityNonFiniteCount",

    guard_enabled: "EQ_DebugHarshnessGuardEnabled",
    guard_strength: "EQ_DebugHarshnessGuardStrength",
    guard_active: "EQ_DebugHarshnessGuardProcessingActive",
    guard_requested_level: "EQ_DebugHarshnessGuardRequestedLevel",
    guard_applied_level: "EQ_DebugHarshnessGuardAppliedLevel",
    guard_transition: "EQ_DebugHarshnessGuardTransitionActive",
    guard_decisions: "EQ_DebugHarshnessGuardDecisionCount",
    guard_saturation: "EQ_DebugHarshnessGuardSaturationCount",
    guard_nonfinite: "EQ_DebugHarshnessGuardNonFiniteCount",

    lcd_enabled: "EQ_DebugLcdEnabled",
    lcd_mask: "EQ_DebugLcdRuntimeMask",
    lcd_jobs: "EQ_DebugLcdJobCount",
    lcd_max_job_cycles: "EQ_DebugLcdMaxJobCycles",
    lcd_deferred_audio: "EQ_DebugLcdDeferredAudioCount",
    lcd_audio_arrived: "EQ_DebugLcdAudioArrivedDuringDrawCount",
    lcd_unexpected_full_redraw: "EQ_DebugLcdUnexpectedFullRedrawCount",
    lcd_bounds_failures: "EQ_DebugLcdBoundsFailureCount",
    lcd_raster_faults: "EQ_DebugLcdRasterFaultCount",
    lcd_sync_errors: "EQ_DebugLcdSyncLostCount",
    lcd_fifo_underflows: "EQ_DebugLcdFifoUnderflowCount",
    lcd_frame_address_mismatches:
        "EQ_DebugLcdFrameAddressMismatchCount",
    lcd_canary_failures:
        "EQ_DebugLcdFramebufferCanaryFailureCount",
    lcd_raster_stop_timeouts: "EQ_DebugLcdRasterStopTimeoutCount",
    lcd_swap_requests: "EQ_DebugLcdSwapRequestCount",
    lcd_swap_completes: "EQ_DebugLcdSwapCompleteCount",
    lcd_eof0: "EQ_DebugLcdEof0Count",
    lcd_eof1: "EQ_DebugLcdEof1Count",
    lcd_eof_ambiguous: "EQ_DebugLcdEofAmbiguousCount",
    lcd_writeback_count: "EQ_DebugLcdWritebackCount",
    lcd_writeback_bytes: "EQ_DebugLcdWritebackBytes",
    lcd_writeback_failures: "EQ_DebugLcdWritebackFailureCount",
    lcd_pending_dirty_regions: "EQ_DebugLcdPendingDirtyRegionCount",
    lcd_max_pending_dirty_regions:
        "EQ_DebugLcdMaxPendingDirtyRegionCount",
    lcd_page_sync_jobs: "EQ_DebugLcdJobTypeCount[23]",
    lcd_timing_compiled: "EQ_DebugLcdTimingCaptureCompiled"
};

var script = null;
var debugServer = null;
var debugSession = null;
var rawWriter = null;
var rawPath = "";
var summaryPath = "";
var currentStageData = null;

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

function logRecord(value) {
    rawWriter.write(jsonStringify(value) + "\n");
    rawWriter.flush();
}

function requireCondition(condition, message) {
    if (!condition) throw message;
}

function evaluate(expression) {
    return String(debugSession.expression.evaluate(expression));
}

function numberValue(expression) {
    var text = evaluate(expression);
    var hex = text.match(/-?0x[0-9a-fA-F]+/);
    var value = hex ? parseInt(hex[0], 16) : parseFloat(text);
    if (!isFinite(value)) {
        throw "unreadable diagnostic " + expression + "=" + text;
    }
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

function writeTarget(symbol, value) {
    evaluate(symbol + " = " + value);
}

function snapshot() {
    var value = {}, key, index, histogramSize;
    for (key in REQUIRED_SYMBOLS) {
        value[key] = numberValue(REQUIRED_SYMBOLS[key]);
    }
    histogramSize = numberValue("EQ_DebugTouchActionHistogramSize");
    requireCondition(histogramSize == 27,
        "EQ_DebugTouchActionHistogramSize is not 27");
    value.touch_action_histogram_size = histogramSize;
    value.touch_action_histogram = [];
    for (index = 0; index < histogramSize; index++) {
        value.touch_action_histogram.push(numberValue(
            "EQ_DebugTouchActionHistogram[" + index + "]"));
    }
    return value;
}

function counterDelta(after, before, key) {
    var value = after[key] - before[key];
    return value < 0 ? value + 4294967296 : value;
}

function histogramBinDelta(after, before, index) {
    var value = after.touch_action_histogram[index] -
        before.touch_action_histogram[index];
    return value < 0 ? value + 4294967296 : value;
}

function histogramDeltas(after, before) {
    var values = [], index;
    for (index = 0; index < 27; index++) {
        values.push(histogramBinDelta(after, before, index));
    }
    return values;
}

function histogramDeltaSum(after, before) {
    var values = histogramDeltas(after, before), total = 0, index;
    for (index = 1; index < values.length; index++) total += values[index];
    return total;
}

function runSlice(milliseconds) {
    debugSession.target.runAsynch();
    Thread.sleep(milliseconds);
    debugSession.target.halt();
}

function runContinuousWindow(milliseconds) {
    debugSession.target.runAsynch();
    Thread.sleep(milliseconds);
    debugSession.target.halt();
}

function connectTargetWithRecovery() {
    try {
        debugSession.target.connect();
    } catch (firstError) {
        System.out.println("TARGET_CONNECT_RECOVERY_RETRY=" + firstError);
        System.out.flush();
        Thread.sleep(1000);
        debugSession.target.connect();
    }
}

function waitForState(label, timeoutSeconds, predicate) {
    var elapsed = 0, state = null;
    while (elapsed < timeoutSeconds * 1000) {
        runSlice(50);
        elapsed += 50;
        state = snapshot();
        if (predicate(state)) return state;
    }
    throw label + " timed out after " + timeoutSeconds + " seconds";
}

function waitForPage(page, label) {
    var elapsed = 0, requested, displayed, building;
    while (elapsed < interactionTimeoutSeconds * 1000) {
        runSlice(1000);
        elapsed += 1000;
        requested = numberValue("EQ_DebugUiRequestedPage");
        displayed = numberValue("EQ_DebugUiDisplayedPage");
        building = numberValue("EQ_DebugUiPageBuilding");
        if (requested == page && displayed == page && building == 0) {
            return {
                requested_page: requested,
                displayed_page: displayed,
                page_building: building
            };
        }
    }
    throw label + " timed out after " + interactionTimeoutSeconds +
        " seconds";
}

function requestPage(page, label) {
    writeTarget("EQ_DebugUiRequestedPage", page);
    return waitForPage(page, label);
}

function requestPreset(preset, label) {
    var beforeToken = numberValue("EQ_DebugControlAppliedToken");
    var elapsed = 0, appliedToken, appliedMode;
    writeTarget("EQ_DebugDiagPath", 4);
    writeTarget("EQ_DebugMode", preset);
    while (elapsed < interactionTimeoutSeconds * 1000) {
        runSlice(250);
        elapsed += 250;
        appliedToken = numberValue("EQ_DebugControlAppliedToken");
        appliedMode = numberValue("EQ_DebugAppliedMode");
        if (appliedMode == preset && appliedToken != beforeToken) {
            return {applied_mode: appliedMode,
                    control_applied_token: appliedToken};
        }
    }
    throw label + " timed out after " + interactionTimeoutSeconds +
        " seconds";
}

function publishCustom(gains) {
    var current = numberValue("EQ_ControlMailbox.sequence");
    var odd, even, band;
    requireCondition((current % 2) == 0,
        "control mailbox sequence is odd while target is halted");
    odd = current >= 4294967294 ? 1 : current + 1;
    even = current >= 4294967294 ? 2 : current + 2;
    writeTarget("EQ_ControlMailbox.sequence", odd);
    writeTarget("EQ_ControlMailbox.command", 4);
    writeTarget("EQ_ControlMailbox.band", 0);
    writeTarget("EQ_ControlMailbox.preset", 5);
    writeTarget("EQ_ControlMailbox.value_db", 0.0);
    writeTarget("EQ_ControlMailbox.step_db", 0.0);
    for (band = 0; band < 10; band++) {
        writeTarget("EQ_ControlMailbox.shadow_gain_db[" + band + "]",
            gains[band]);
    }
    writeTarget("EQ_ControlMailbox.sequence", even);
    return even;
}

function requestCustom(gains, label) {
    var token = publishCustom(gains);
    var elapsed = 0, appliedToken, appliedMode;
    while (elapsed < interactionTimeoutSeconds * 1000) {
        runSlice(1500);
        elapsed += 1500;
        appliedToken = numberValue("EQ_DebugControlAppliedToken");
        appliedMode = numberValue("EQ_DebugAppliedMode");
        if (appliedToken == token && appliedMode == 5) {
            return {applied_mode: appliedMode,
                    control_applied_token: appliedToken};
        }
    }
    throw label + " timed out after " + interactionTimeoutSeconds +
        " seconds";
}

function actionName(action) {
    return ACTION_NAMES[action] || ("UNKNOWN_" + action);
}

function waitForPhysicalAction(item, promptPrefix, recordType) {
    var before = snapshot(), observed, held, released, after;
    var actionDelta, rejectedDelta, histogramDelta;
    var duplicate, outOfRange, accepted;
    var record;
    requireCondition(before.displayed_page == item.page &&
        before.page_building == 0,
        item.id + ": wrong starting page");
    System.out.println(promptPrefix + "=" + item.id +
        ";expected=" + actionName(item.action) +
        ";point=" + (item.pointHint || "center") +
        (item.longHold ? ";HOLD_FOR_1500MS" : ""));
    System.out.flush();
    observed = waitForState("physical action " + item.id,
        interactionTimeoutSeconds, function(state) {
            return state.touch_actions != before.touch_actions ||
                state.touch_rejected != before.touch_rejected;
        });
    held = observed;
    if (item.longHold) {
        System.out.println("HITBOX_HOLD_REQUIRED=" + item.id);
        System.out.flush();
        runContinuousWindow(1500);
        held = snapshot();
    }
    System.out.println("TOUCH_RELEASE_REQUIRED=" + item.id);
    System.out.flush();
    released = waitForState("physical release " + item.id,
        interactionTimeoutSeconds, function(state) {
            return state.touch_pressed == 0;
        });
    runSlice(250);
    after = snapshot();
    actionDelta = counterDelta(after, before, "touch_actions");
    rejectedDelta = counterDelta(after, before, "touch_rejected");
    histogramDelta = after.touch_action_histogram[item.action] -
        before.touch_action_histogram[item.action];
    if (histogramDelta < 0) histogramDelta += 4294967296;
    duplicate = actionDelta != 1 ||
        histogramDelta != 1 ||
        counterDelta(after, before, "touch_duplicate_actions") != 0;
    outOfRange = observed.touch_screen_x < 0 ||
        observed.touch_screen_x > 799 || observed.touch_screen_y < 0 ||
        observed.touch_screen_y > 479;
    accepted = actionDelta == 1 && rejectedDelta == 0 &&
        observed.touch_last_action == item.action;
    record = {
        record_type: recordType,
        trial_id: item.trialId || 0,
        hitbox_id: item.id,
        page: item.page == PAGE_STATUS ? "STATUS" : "EDITOR",
        raw_x: observed.touch_raw_x,
        raw_y: observed.touch_raw_y,
        screen_x: observed.touch_screen_x,
        screen_y: observed.touch_screen_y,
        expected_action: item.action,
        expected_action_name: actionName(item.action),
        actual_action: observed.touch_last_action,
        actual_action_name: actionName(observed.touch_last_action),
        accepted: accepted,
        rejected: rejectedDelta != 0,
        action_count_delta: actionDelta,
        action_histogram_bin: item.action,
        action_histogram_bin_delta: histogramDelta,
        duplicate_action: duplicate,
        out_of_range: outOfRange,
        long_hold_verified: item.longHold ?
            (held.touch_pressed == 1 &&
             counterDelta(held, before, "touch_actions") == 1) : false,
        release_rearm_verified: released.touch_pressed == 0 &&
            after.touch_pressed == 0 &&
            after.touch_actions == released.touch_actions,
        point_hint: item.pointHint || "center",
        before: before,
        observed: observed,
        after: after
    };
    logRecord(record);
    requireCondition(accepted, item.id + ": wrong or rejected action");
    requireCondition(!duplicate, item.id + ": duplicate action");
    requireCondition(!outOfRange, item.id + ": mapped coordinate out of range");
    requireCondition(record.release_rearm_verified,
        item.id + ": release did not re-arm cleanly");
    if (item.longHold) {
        requireCondition(record.long_hold_verified,
            item.id + ": long hold was not maintained without repeat");
    }
    if (item.action == 12) {
        waitForPage(item.page == PAGE_STATUS ? PAGE_EDITOR : PAGE_STATUS,
            "page change after " + item.id);
    }
    return record;
}

function verifyIdentityAndConfiguration() {
    var targetSha;
    requireCondition(/^[0-9a-f]{40}$/.test(expectedSha),
        "exact 40-character commit SHA is required");
    requireCondition(/^[0-9a-f]{64}$/.test(expectedProgramSha),
        "exact .out SHA256 is required");
    requireCondition(numberValue("EQ_DebugBuildMagic") == 0x33030003,
        "Project 3.3 build magic mismatch");
    requireCondition(numberValue("EQ_DebugBuildDirty") == 0,
        "target reports dirty build");
    targetSha = readCString("EQ_DebugBuildGitSha", 16).toLowerCase();
    requireCondition(targetSha == expectedSha.substring(0, 7),
        "target build SHA mismatch: " + targetSha);
    requireCondition(numberValue("EQ_DebugLcdJobTypeCountSize") == 25,
        "full editor job count is not 25");
    requireCondition(numberValue("EQ_DebugDynamicHitboxCountSize") == 12,
        "STATUS hitbox count is not 12");
    requireCondition(numberValue("EQ_DebugEditorHitboxCountSize") == 15,
        "EDITOR hitbox count is not 15");
    requireCondition(numberValue("EQ_DebugTouchActionHistogramSize") == 27,
        "Touch action histogram size is not 27");
}

function verifyBaseState(state) {
    requireCondition(state.build_dirty == 0, "target build dirty changed");
    requireCondition(state.init_stage == 11, "INIT milestone is not 11");
    requireCondition(state.lcd_enabled == 1 && state.lcd_mask == 31,
        "full LCD runtime mask is not active");
    requireCondition(state.analyzer_compiled == 1,
        "Analyzer is not compiled in the acceptance image");
}

function requireZeroDeltas(before, after, keys, label) {
    var index, key, value;
    for (index = 0; index < keys.length; index++) {
        key = keys[index];
        value = counterDelta(after, before, key);
        requireCondition(value == 0,
            label + ": " + key + " delta=" + value);
    }
}

function runNoTouch120s() {
    var before, after, started, elapsed;
    System.out.println("NO_TOUCH_START=Do not touch the LCD for 120 seconds");
    System.out.flush();
    before = snapshot();
    started = System.currentTimeMillis();
    logRecord({record_type: "snapshot", marker: "NO_TOUCH_START",
        snapshot: before, host_epoch_ms: Number(started)});
    runContinuousWindow(120000);
    elapsed = Number(System.currentTimeMillis() - started) / 1000.0;
    after = snapshot();
    logRecord({record_type: "snapshot", marker: "NO_TOUCH_END",
        snapshot: after, host_elapsed_seconds: elapsed});
    currentStageData = {
        host_elapsed_seconds: elapsed,
        before: before,
        after: after,
        touch_action_histogram_delta: histogramDeltas(after, before)
    };
    requireZeroDeltas(before, after, [
        "touch_actions", "touch_page_requests", "touch_preset_requests",
        "touch_dynamic_enable_requests",
        "touch_dynamic_strength_requests", "touch_editor_actions"
    ], "no-touch stop condition");
    requireCondition(histogramDeltaSum(after, before) == 0,
        "no-touch action histogram changed");
    return currentStageData;
}

function runHitbox27() {
    var start = snapshot(), index, item, end;
    requireCondition(start.displayed_page == PAGE_STATUS &&
        start.page_building == 0,
        "27-hitbox stage must start on STATUS");
    for (index = 0; index < HITBOXES.length; index++) {
        item = HITBOXES[index];
        item.trialId = index + 1;
        waitForPhysicalAction(item, "HITBOX_TOUCH_REQUIRED", "hitbox_trial");
    }
    end = snapshot();
    currentStageData = {
        expected_hitbox_count: 27,
        completed_hitbox_count: HITBOXES.length,
        before: start,
        after: end
    };
    return currentStageData;
}

function findHitbox(id) {
    var index;
    for (index = 0; index < HITBOXES.length; index++) {
        if (HITBOXES[index].id == id) return HITBOXES[index];
    }
    throw "unknown hitbox " + id;
}

function statusRoundAction(round) {
    var ids = [
        "status_preset_flat", "status_preset_bass",
        "status_preset_vocal", "status_preset_treble",
        "status_preset_v_shape", "status_smart_toggle",
        "status_smart_strength", "status_clarity_toggle",
        "status_clarity_strength", "status_guard_toggle",
        "status_guard_strength"
    ];
    return findHitbox(ids[(round - 1) % ids.length]);
}

function runPageSwitch30() {
    var before = snapshot(), round;
    requireCondition(before.displayed_page == PAGE_STATUS &&
        before.page_building == 0,
        "page-switch stage must start on STATUS");
    System.out.println("OPERATOR_VISUAL_OBSERVATION_REQUIRED=" +
        "watch circular offset, white flash, ghosting, and alignment");
    System.out.flush();
    for (round = 1; round <= PAGE_ROUND_TRIPS; round++) {
        requestPage(PAGE_EDITOR,
            "scripted STATUS-to-EDITOR round " + round);
        requestPage(PAGE_STATUS,
            "scripted EDITOR-to-STATUS round " + round);
        logRecord({
            record_type: "page_round",
            round: round,
            status_to_editor_scripted: true,
            editor_to_status_scripted: true,
            request_api: "EQ_DebugUiRequestedPage"
        });
    }
    currentStageData = {
        completed_round_trips: PAGE_ROUND_TRIPS,
        before: before,
        after: snapshot(),
        control_method: "SCRIPTED_PAGE_REQUEST_API",
        operator_visual_result: "PENDING_OPERATOR"
    };
    requireZeroDeltas(before, currentStageData.after, [
        "lcd_eof_ambiguous", "lcd_frame_address_mismatches",
        "lcd_unexpected_full_redraw", "lcd_writeback_failures",
        "lcd_raster_faults", "lcd_fifo_underflows", "lcd_sync_errors",
        "lcd_bounds_failures", "lcd_canary_failures",
        "lcd_raster_stop_timeouts"
    ], "page-switch stop condition");
    return currentStageData;
}

function timingCount(timingClass) {
    return numberValue("EQ_DebugLcdTimingSampleCount[" + timingClass + "]");
}

function waitForTimingClass(timingClass) {
    var elapsed = 0, count = timingCount(timingClass);
    var mode, page, toggle = 0;
    if (count >= timingSamplesPerClass) return;
    System.out.println("TIMING_AUTOMATIC_WAIT=" +
        LCD_TIMING_CLASSES[timingClass] +
        ";target_samples=" + timingSamplesPerClass);
    System.out.flush();
    while (elapsed < interactionTimeoutSeconds * 1000) {
        toggle++;
        if (timingClass == 0 || timingClass == 5 || timingClass == 6) {
            if ((timingClass == 5 || timingClass == 6) &&
                numberValue("EQ_DebugUiDisplayedPage") != PAGE_EDITOR) {
                requestPage(PAGE_EDITOR, "timing fallback editor");
            }
            mode = (numberValue("EQ_DebugAppliedMode") + 1) % 5;
            writeTarget("EQ_DebugMode", mode);
        } else if (timingClass == 2 || timingClass == 4) {
            writeTarget("EQ_DebugSmartBassEnabled", toggle % 2);
        } else if (timingClass == 3) {
            writeTarget("EQ_DebugSmartBassStrength", (toggle % 3) + 1);
        } else if (timingClass == 7 || timingClass == 8) {
            mode = (numberValue("EQ_DebugAppliedMode") + 1) % 5;
            requestPreset(mode, "timing fallback page dirtiness");
            page = (numberValue("EQ_DebugUiDisplayedPage") == PAGE_STATUS) ?
                PAGE_EDITOR : PAGE_STATUS;
            requestPage(page, "timing fallback page");
        }
        runSlice(750);
        elapsed += 750;
        count = timingCount(timingClass);
        if (count >= timingSamplesPerClass) return;
    }
    throw "timing class " + LCD_TIMING_CLASSES[timingClass] +
        " did not reach " + timingSamplesPerClass + " samples";
}

function driveLcdTimingRequests() {
    var index, enabled, strength, mode;

    requestPage(PAGE_STATUS, "timing start STATUS");
    for (index = 0; index < timingSamplesPerClass + 12; index++) {
        if (timingCount(0) >= timingSamplesPerClass &&
            timingCount(1) >= timingSamplesPerClass &&
            timingCount(2) >= timingSamplesPerClass &&
            timingCount(3) >= timingSamplesPerClass &&
            timingCount(4) >= timingSamplesPerClass) break;
        mode = (index + 1) % 5;
        requestPreset(mode, "timing preset " + index);
        enabled = (index % 2 == 0) ? 1 : 0;
        strength = (index % 3) + 1;
        writeTarget("EQ_DebugSmartBassEnabled", enabled);
        writeTarget("EQ_DebugDynamicClarityEnabled", enabled);
        writeTarget("EQ_DebugHarshnessGuardEnabled", enabled);
        writeTarget("EQ_DebugSmartBassStrength", strength);
        writeTarget("EQ_DebugDynamicClarityStrength", strength);
        writeTarget("EQ_DebugHarshnessGuardStrength", strength);
        runSlice(500);
    }
    for (index = 0; index <= 4; index++) {
        waitForTimingClass(index);
    }
    writeTarget("EQ_DebugAnalyzerEnabled", 0);
    runSlice(500);

    for (index = 0; index < timingSamplesPerClass + 12; index++) {
        if (timingCount(7) >= timingSamplesPerClass &&
            timingCount(8) >= timingSamplesPerClass) break;
        mode = (numberValue("EQ_DebugAppliedMode") + 1) % 5;
        requestPreset(mode, "timing page dirtiness " + index);
        requestPage((index % 2 == 0) ? PAGE_EDITOR : PAGE_STATUS,
            "timing scripted page request " + index);
    }
    requestPage(PAGE_EDITOR, "timing editor jobs");
    for (index = 0; index < timingSamplesPerClass + 12; index++) {
        if (timingCount(5) >= timingSamplesPerClass &&
            timingCount(6) >= timingSamplesPerClass) break;
        mode = (numberValue("EQ_DebugAppliedMode") + 1) % 5;
        requestPreset(mode, "timing editor update " + index);
        runSlice(1000);
    }
}

function exportTimingSamples() {
    var timingClass, count, total, dropped, deferred, arrived;
    var over2, over5, minimum, maximum, index, cycles;
    var classSummaries = [];
    for (timingClass = 0; timingClass < LCD_TIMING_CLASS_COUNT;
         timingClass++) {
        count = numberValue("EQ_DebugLcdTimingSampleCount[" +
            timingClass + "]");
        total = numberValue("EQ_DebugLcdTimingTotalCount[" +
            timingClass + "]");
        dropped = numberValue("EQ_DebugLcdTimingDroppedCount[" +
            timingClass + "]");
        minimum = numberValue("EQ_DebugLcdTimingMinCycles[" +
            timingClass + "]");
        maximum = numberValue("EQ_DebugLcdTimingMaxCycles[" +
            timingClass + "]");
        over2 = numberValue("EQ_DebugLcdTimingOver2msCount[" +
            timingClass + "]");
        over5 = numberValue("EQ_DebugLcdTimingOver5msCount[" +
            timingClass + "]");
        deferred = numberValue(
            "EQ_DebugLcdTimingDeferredByAudioCount[" + timingClass + "]");
        arrived = numberValue(
            "EQ_DebugLcdTimingAudioArrivedDuringDrawCount[" +
            timingClass + "]");
        for (index = 0; index < count; index++) {
            cycles = numberValue("EQ_DebugLcdTimingSamples[" + timingClass +
                "][" + index + "]");
            logRecord({
                record_type: "timing_sample",
                class_name: LCD_TIMING_CLASSES[timingClass],
                class_index: timingClass,
                sample_index: index + 1,
                cycles: cycles,
                job_count_delta: 1,
                deferred_by_audio_delta: 0,
                audio_arrived_during_draw_delta: 0,
                attribution: timingClass == 8 ?
                    "EOF descriptor update" : "board timing capture"
            });
        }
        classSummaries.push({
            record_type: "timing_class_summary",
            class_name: LCD_TIMING_CLASSES[timingClass],
            class_index: timingClass,
            sample_count: count,
            total_count: total,
            dropped_count: dropped,
            min_cycles: minimum,
            max_cycles: maximum,
            over_2ms_count: over2,
            over_5ms_count: over5,
            deferred_by_audio_count: deferred,
            audio_arrived_during_draw_count: arrived,
            measurement_region: timingClass == 8 ?
                "EOF descriptor update only" :
                "draw or descriptor operation only"
        });
        logRecord(classSummaries[classSummaries.length - 1]);
    }
    return classSummaries;
}

function runLcdTiming() {
    var before = snapshot(), summaries, after, index;
    requireCondition(before.lcd_timing_compiled == 1,
        "timing stage requires EQ_ENABLE_LCD_JOB_TIMING_CAPTURE=1");
    requireCondition(numberValue("EQ_DebugLcdTimingClassCount") == 9,
        "LCD timing class count is not 9");
    requireCondition(numberValue("EQ_DebugLcdTimingSampleCapacity") == 256,
        "LCD timing sample capacity is not 256");
    driveLcdTimingRequests();
    for (index = 0; index < LCD_TIMING_CLASS_COUNT; index++) {
        waitForTimingClass(index);
    }
    summaries = exportTimingSamples();
    after = snapshot();
    currentStageData = {
        before: before,
        after: after,
        timing_class_count: LCD_TIMING_CLASS_COUNT,
        timing_sample_capacity: LCD_TIMING_SAMPLE_CAPACITY,
        class_summaries: summaries,
        control_method: "SCRIPTED_REQUEST_AND_MAILBOX_APIS",
        page_swap_measurement_region: "EOF descriptor update only"
    };
    for (index = 0; index < summaries.length; index++) {
        requireCondition(summaries[index].over_5ms_count == 0,
            summaries[index].class_name + " contains a job over 5 ms");
    }
    requireZeroDeltas(before, after, [
        "lcd_unexpected_full_redraw", "lcd_writeback_failures",
        "lcd_raster_faults", "lcd_fifo_underflows", "lcd_sync_errors",
        "lcd_frame_address_mismatches", "lcd_bounds_failures",
        "lcd_canary_failures", "lcd_raster_stop_timeouts"
    ], "LCD timing stop condition");
    return currentStageData;
}

function allDynamicsEnabled(state) {
    return state.analyzer_enabled == 1 && state.smart_enabled == 1 &&
        state.clarity_enabled == 1 && state.guard_enabled == 1;
}

function runNonTouchControlSequence() {
    var preset, index;
    var custom = [1.0, -0.5, 0.5, 0.0, -1.0,
                  1.0, 0.5, -0.5, 0.5, -1.0];

    requestPage(PAGE_STATUS, "endurance scripted start STATUS");
    for (preset = 1; preset < 5; preset++) {
        requestPreset(preset, "endurance preset " + preset);
    }
    requestPreset(0, "endurance preset FLAT");

    writeTarget("EQ_DebugSmartBassEnabled", 1);
    writeTarget("EQ_DebugDynamicClarityEnabled", 1);
    writeTarget("EQ_DebugHarshnessGuardEnabled", 1);
    writeTarget("EQ_DebugSmartBassStrength", 2);
    writeTarget("EQ_DebugDynamicClarityStrength", 2);
    writeTarget("EQ_DebugHarshnessGuardStrength", 2);
    runSlice(1000);
    writeTarget("EQ_DebugSmartBassStrength", 3);
    writeTarget("EQ_DebugDynamicClarityStrength", 3);
    writeTarget("EQ_DebugHarshnessGuardStrength", 3);
    runSlice(1000);
    writeTarget("EQ_DebugSmartBassEnabled", 0);
    writeTarget("EQ_DebugDynamicClarityEnabled", 0);
    writeTarget("EQ_DebugHarshnessGuardEnabled", 0);
    runSlice(500);
    writeTarget("EQ_DebugSmartBassEnabled", 1);
    writeTarget("EQ_DebugDynamicClarityEnabled", 1);
    writeTarget("EQ_DebugHarshnessGuardEnabled", 1);
    runSlice(1000);

    requestCustom(custom, "endurance CUSTOM Apply");
    requestPreset(1, "endurance post-CUSTOM BASS");
    requestPreset(0, "endurance final FLAT");
    for (index = 0; index < 5; index++) {
        requestPage(PAGE_EDITOR,
            "endurance scripted STATUS-to-EDITOR " + index);
        requestPage(PAGE_STATUS,
            "endurance scripted EDITOR-to-STATUS " + index);
    }
    return {
        preset_each_minimum: 1,
        dynamic_enabled_each_changed: true,
        dynamic_strength_each_changed: true,
        custom_apply_count: 1,
        flat_count: 2,
        page_request_count: 10,
        control_method: "DEBUG_REQUEST_MAILBOX_AND_PAGE_APIS"
    };
}

function runEndurance300s() {
    var before, after, started, elapsed, scriptedControls;
    System.out.println("ENDURANCE_CONTROL=SCRIPTED_NON_TOUCH");
    System.out.println("ENDURANCE_START_DELAY_SECONDS=" +
        enduranceStartDelaySeconds);
    System.out.flush();
    Thread.sleep(enduranceStartDelaySeconds * 1000);
    scriptedControls = runNonTouchControlSequence();
    waitForState("all dynamic modules ready", interactionTimeoutSeconds,
        function(state) {
            return allDynamicsEnabled(state) &&
                state.analyzer_valid == 1 && state.analyzer_warm == 1;
        });
    before = snapshot();
    requireCondition(allDynamicsEnabled(before),
        "enable Analyzer and all three dynamic modules before endurance");
    logRecord({record_type: "snapshot", marker: "ENDURANCE_START",
        snapshot: before});
    writeTarget("EQ_DebugSmartBassStrength", 1);
    writeTarget("EQ_DebugDynamicClarityStrength", 1);
    writeTarget("EQ_DebugHarshnessGuardStrength", 1);
    requestPreset(1, "endurance overlap BASS");
    started = System.currentTimeMillis();
    runContinuousWindow(ENDURANCE_WINDOW_MILLISECONDS);
    elapsed = Number(System.currentTimeMillis() - started) / 1000.0;
    after = snapshot();
    logRecord({record_type: "snapshot", marker: "ENDURANCE_END",
        snapshot: after, host_elapsed_seconds: elapsed});
    currentStageData = {
        host_elapsed_seconds: elapsed,
        measurement_snapshot_count: 2,
        periodic_halt_count: 0,
        continuous_window_halt_count: 2,
        before: before,
        after: after,
        scripted_control_counts: scriptedControls,
        action_quota_source: "SCRIPTED_REQUEST_AND_MAILBOX_APIS",
        operator_quota_result: "NOT_REQUIRED"
    };
    requireZeroDeltas(before, after, [
        "deadline_misses", "latency_misses", "frame_overlaps",
        "dropped_frames", "clip_count", "invalid_count",
        "smart_saturation", "smart_nonfinite", "clarity_saturation",
        "clarity_nonfinite", "guard_saturation", "guard_nonfinite",
        "lcd_unexpected_full_redraw", "lcd_writeback_failures",
        "lcd_raster_faults", "lcd_fifo_underflows", "lcd_sync_errors",
        "lcd_frame_address_mismatches", "lcd_bounds_failures",
        "lcd_canary_failures", "lcd_raster_stop_timeouts"
    ], "endurance stop condition");
    requireCondition(after.frame_latency_max_cycles < 9338880,
        "maximum frame latency is not below 20.48 ms");
    requireCondition(counterDelta(after, before,
        "analyzer_publications") > 0,
        "Analyzer publication count did not advance");
    requireCondition(counterDelta(after, before, "smart_decisions") > 0,
        "Smart Bass decision count did not advance");
    requireCondition(counterDelta(after, before, "clarity_decisions") > 0,
        "Dynamic Clarity decision count did not advance");
    requireCondition(counterDelta(after, before, "guard_decisions") > 0,
        "HF Guard decision count did not advance");
    requireCondition(counterDelta(after, before,
        "static_dynamic_overlap_frames") >= 1,
        "no static/dynamic transition overlap frame was observed");
    currentStageData.operator_quota_result = "NOT_REQUIRED";
    return currentStageData;
}

function anyDynamicActive(state) {
    return state.smart_active == 1 || state.clarity_active == 1 ||
        state.guard_active == 1;
}

function runAnalyzerReset() {
    var before, invalid, recovered;
    System.out.println("ANALYZER_RESET_PRECONDITION=" +
        "enable all modules and use input that makes at least one active");
    System.out.flush();
    before = waitForState("Analyzer reset precondition",
        interactionTimeoutSeconds, function(state) {
            return allDynamicsEnabled(state) && state.analyzer_valid == 1 &&
                state.analyzer_warm == 1 && anyDynamicActive(state);
        });
    logRecord({record_type: "snapshot", marker: "ANALYZER_RESET_BEFORE",
        snapshot: before});
    writeTarget("EQ_DebugAnalyzerResetRequest", 1);
    invalid = waitForState("Analyzer invalid/warmup phase", 30,
        function(state) {
            return state.analyzer_epoch != before.analyzer_epoch &&
                (state.analyzer_valid == 0 || state.analyzer_warm == 0);
        });
    logRecord({record_type: "snapshot", marker: "ANALYZER_RESET_INVALID",
        snapshot: invalid});
    recovered = waitForState("Analyzer valid/warm recovery", 60,
        function(state) {
            return state.analyzer_epoch != before.analyzer_epoch &&
                state.analyzer_valid == 1 && state.analyzer_warm == 1 &&
                state.analyzer_publications > before.analyzer_publications &&
                state.smart_decisions > before.smart_decisions &&
                state.clarity_decisions > before.clarity_decisions &&
                state.guard_decisions > before.guard_decisions;
        });
    logRecord({record_type: "snapshot", marker: "ANALYZER_RESET_RECOVERED",
        snapshot: recovered});
    currentStageData = {
        before: before,
        invalid: invalid,
        recovered: recovered,
        reset_request_method: "EQ_DebugAnalyzerResetRequest",
        old_epoch_duplicate_consumption: "not_observed",
        module_invalid_policy:
            "existing smooth release-or-hold contract; no state override"
    };
    requireZeroDeltas(before, recovered, [
        "deadline_misses", "latency_misses", "frame_overlaps",
        "dropped_frames", "clip_count", "invalid_count",
        "smart_saturation", "smart_nonfinite", "clarity_saturation",
        "clarity_nonfinite", "guard_saturation", "guard_nonfinite"
    ], "Analyzer reset stop condition");
    return currentStageData;
}

function addStageData(destination, data) {
    var key;
    if (data === null) return destination;
    for (key in data) destination[key] = data[key];
    return destination;
}

function executeStage() {
    if (stage == "page_switch_30") return runPageSwitch30();
    if (stage == "lcd_job_timing") return runLcdTiming();
    if (stage == "endurance_300s") return runEndurance300s();
    if (stage == "analyzer_reset") return runAnalyzerReset();
    throw "unsupported final-acceptance stage: " + stage;
}

try {
    requireCondition(typeof STAGE_FILES[stage] != "undefined",
        "runner supplied no valid stage");
    requireCondition(new File(ccxml).exists(), "missing CCXML");
    requireCondition(new File(program).exists(), "missing program");
    requireCondition(interactionTimeoutSeconds >= 30,
        "interaction timeout is below 30 seconds");
    requireCondition(timingSamplesPerClass >= 1 &&
        timingSamplesPerClass <= LCD_TIMING_SAMPLE_CAPACITY,
        "timing sample target is outside 1..256");
    new File(resultDir).mkdirs();
    rawPath = resultDir + "/" + STAGE_FILES[stage] + ".jsonl";
    summaryPath = resultDir + "/" + STAGE_FILES[stage] +
        "_raw_summary.json";
    rawWriter = new FileWriter(rawPath, false);

    script = ScriptingEnvironment.instance();
    script.setScriptTimeout(stage == "endurance_300s" ?
        20 * 60 * 1000 : 4 * 60 * 60 * 1000);
    script.traceSetConsoleLevel(TraceLevel.INFO);
    debugServer = script.getServer("DebugServer.1");
    debugServer.setConfig(ccxml);
    debugSession = debugServer.openSession(".*C674X_0");
    connectTargetWithRecovery();
    debugSession.target.reset();
    Thread.sleep(500);
    debugSession.memory.loadProgram(program);
    Thread.sleep(500);
    runSlice(3000);
    verifyIdentityAndConfiguration();
    verifyBaseState(snapshot());

    currentStageData = executeStage();
    writeJson(summaryPath, addStageData({
        schema_version: 1,
        stage: stage,
        completed: true,
        result_label: "MEASURED_ON_CONNECTED_BOARD",
        measurement_label: stage == "page_switch_30" ||
            stage == "lcd_job_timing" ?
            "BOARD_UI_COUNTER" : "BOARD_REALTIME_COUNTER",
        exact_commit_sha: expectedSha,
        program_sha256: expectedProgramSha
    }, currentStageData));
    System.out.println("FINAL_ACCEPTANCE_STAGE_COMPLETE=" + stage);
} catch (error) {
    try {
        writeJson(summaryPath, addStageData({
            schema_version: 1,
            stage: stage,
            completed: false,
            result_label: "FAIL",
            exact_commit_sha: expectedSha,
            program_sha256: expectedProgramSha,
            error: String(error)
        }, currentStageData));
    } catch (writeError) {}
    System.err.println("DSS_ERROR=" + error);
    throw error;
} finally {
    try {
        if (debugSession != null) debugSession.target.halt();
    } catch (ignore1) {}
    try {
        if (debugSession != null) debugSession.target.disconnect();
    } catch (ignore2) {}
    try {
        if (debugSession != null) debugSession.terminate();
    } catch (ignore3) {}
    try {
        if (debugServer != null) debugServer.stop();
    } catch (ignore4) {}
    if (rawWriter != null) rawWriter.close();
}
