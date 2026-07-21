/* Deferred Project 3.3 ten-band editor validation for C6748 DSS. */
importPackage(Packages.com.ti.debug.engine.scripting);
importPackage(Packages.com.ti.ccstudio.scripting.environment);
importPackage(Packages.java.lang);
importPackage(Packages.java.io);

function env(name, fallback) {
    var value = String(System.getenv(name));
    return (value == "null" || value == "") ? fallback : value;
}

var hardwareAuthorization = env(
    "DSP_TEN_BAND_EDITOR_HARDWARE_AUTH", "NOT_AUTHORIZED");
if (hardwareAuthorization != "C6748_CONNECTED_AND_AUDIO_LOOP_READY") {
    System.out.println("NOT_RUN_NO_HARDWARE");
    System.exit(0);
}

var root = env("DSP_TEST_ROOT",
    String(new File(".").getCanonicalPath()).replace(/\\/g, "/"));
var ccxml = env("DSP_TEST_CCXML", root + "/TargetConfig/C6748.ccxml");
var program = env("DSP_TEST_PROGRAM", root + "/Debug/DSP_LAB_0507.out");
var resultDir = env("DSP_TEST_RESULT_DIR",
    String(System.getProperty("java.io.tmpdir")));
var expectedSha = env("DSP_TEST_EXPECTED_SHA", "").toLowerCase();
var expectedProgramSha = env("DSP_TEST_PROGRAM_SHA256", "").toLowerCase();
var buildProfile = env("DSP_TEST_BUILD_PROFILE", "");
var enduranceMinutes = parseInt(env("DSP_TEST_ENDURANCE_MINUTES", "10"), 10);
var interactionTimeoutSeconds = parseInt(
    env("DSP_TEST_INTERACTION_TIMEOUT_SECONDS", "180"), 10);
var summaryPath = resultDir + "/summary.json";
var resultPath = resultDir + "/hardware_steps.jsonl";
var FRAME_BUDGET_CYCLES = 9338880;
var FRAME_LATENCY_ACCEPTANCE_CYCLES = 8405000;
var LCD_NORMAL_JOB_CYCLES = 912000;
var LCD_HARD_CYCLES = 2280000;
var MAX_ANALYZER_STRIP_HEIGHT = 16;
var MAX_LCD_JOBS_PER_SECOND = 8;
var UINT32_MODULUS = 4294967296;
var PAGE_DYNAMIC = 0;
var PAGE_EDITOR = 1;
var PRESET_FLAT = 0;
var PRESET_BASS = 1;
var PRESET_VOCAL = 2;
var PRESET_TREBLE = 3;
var PRESET_V_SHAPE = 4;
var PRESET_CUSTOM = 5;
var ACTION_PRESET_FLAT = 1;
var ACTION_PRESET_BASS = 2;
var ACTION_PRESET_VOCAL = 3;
var ACTION_PRESET_TREBLE = 4;
var ACTION_PRESET_V_SHAPE = 5;
var ACTION_SMART_TOGGLE = 6;
var ACTION_SMART_STRENGTH = 7;
var ACTION_CLARITY_TOGGLE = 8;
var ACTION_CLARITY_STRENGTH = 9;
var ACTION_GUARD_TOGGLE = 10;
var ACTION_GUARD_STRENGTH = 11;
var ACTION_PAGE_SWITCH = 12;
var ACTION_EDITOR_BAND_0 = 13;
var ACTION_EDITOR_MINUS = 23;
var ACTION_EDITOR_PLUS = 24;
var ACTION_EDITOR_APPLY = 25;
var ACTION_EDITOR_RESET_FLAT = 26;
var FULL_UI_RUNTIME_MASK = 63;
var script = null;
var debugServer = null;
var debugSession = null;
var results = null;
var arraySizes = null;
var arraySizeSources = null;
var touchCalibration = [];

var symbols = {
    build_dirty: "EQ_DebugBuildDirty",
    init_stage: "EQ_DebugInitStage",
    ad_frames: "EQ_DebugAdFrames",
    da_frames: "EQ_DebugDaFrames",
    process_frames: "EQ_DebugProcessFrames",
    algo_last_cycles: "EQ_DebugAlgoLastCycles",
    algo_max_cycles: "EQ_DebugAlgoMaxCycles",
    frame_service_last_cycles: "EQ_DebugFrameServiceLastCycles",
    frame_service_max_cycles: "EQ_DebugFrameServiceMaxCycles",
    frame_latency_last_cycles: "EQ_DebugFrameLatencyLastCycles",
    frame_latency_max_cycles: "EQ_DebugFrameLatencyMaxCycles",
    deadline: "EQ_DebugDeadlineMissCount",
    latency_deadline: "EQ_DebugFrameLatencyDeadlineMissCount",
    overlap: "EQ_DebugFrameServiceOverlapCount",
    dropped: "EQ_DebugFrameServiceDroppedCount",
    clip: "EQ_DebugClipCount",
    input_peak: "EQ_DebugInputPeak",
    output_peak: "EQ_DebugOutputPeak",
    debug_mode: "EQ_DebugMode",
    requested_mode: "EQ_DebugRequestedMode",
    applied_mode: "EQ_DebugAppliedMode",
    mode_changes: "EQ_DebugModeChangeCount",
    request_token: "EQ_DebugControlRequestToken",
    accepted_token: "EQ_DebugControlAcceptedToken",
    target_token: "EQ_DebugControlTargetToken",
    prepared_token: "EQ_DebugControlPreparedToken",
    ready_valid: "EQ_DebugControlReadyValid",
    installed_token: "EQ_DebugControlInstalledToken",
    applied_token: "EQ_DebugControlAppliedToken",
    installed_pair_valid: "EQ_DebugControlInstalledPairValid",
    control_rejected: "EQ_DebugControlRejectedCount",
    control_coalesced: "EQ_DebugControlCoalescedCount",
    control_error: "EQ_DebugControlLastError",
    builder_state: "EQ_DebugBuilderState",
    builder_slices: "EQ_DebugBuilderSliceCount",
    builder_cancel: "EQ_DebugBuilderCancelCount",
    builder_restart: "EQ_DebugBuilderRestartCount",
    builder_error: "EQ_DebugBuilderLastError",
    transition_active: "EQ_DebugResponseTransitionActive",
    active_generation: "EQ_DebugResponseActiveGeneration",
    requested_page: "EQ_DebugUiRequestedPage",
    displayed_page: "EQ_DebugUiDisplayedPage",
    page_building: "EQ_DebugUiPageBuilding",
    snapshot_builds: "EQ_DebugUiSnapshotBuildCount",
    snapshot_skips: "EQ_DebugUiSnapshotSkippedCount",
    gain_refreshes: "EQ_DebugUiAppliedGainRefreshCount",
    draft_version: "EQ_DebugUiDraftVersion",
    editor_selected_band: "EQ_DebugUiEditorSelectedBand",
    editor_draft_dirty: "EQ_DebugUiEditorDraftDirty",
    editor_submitted_valid: "EQ_DebugUiEditorSubmittedValid",
    editor_apply_status: "EQ_DebugUiEditorApplyStatus",
    editor_submitted_sequence: "EQ_DebugUiEditorSubmittedSequence",
    editor_applied_sequence: "EQ_DebugUiEditorAppliedSequence",
    page_switches: "EQ_DebugUiPageSwitchCount",
    editor_state_bytes: "EQ_DebugUiEditorStateBytes",
    total_ui_state_bytes: "EQ_DebugUiTotalStateBytes",
    touch_actions: "EQ_DebugTouchActionCount",
    touch_rejected: "EQ_DebugTouchRejectedCount",
    touch_last_action: "EQ_DebugTouchLastAction",
    touch_raw_x: "EQ_DebugTouchRawX",
    touch_raw_y: "EQ_DebugTouchRawY",
    touch_screen_x: "EQ_DebugTouchScreenX",
    touch_screen_y: "EQ_DebugTouchScreenY",
    touch_pressed: "EQ_DebugTouchPressed",
    touch_last_cycles: "EQ_DebugTouchLastCycles",
    touch_max_cycles: "EQ_DebugTouchMaxCycles",
    analyzer_compiled: "EQ_DebugAnalyzerCompiled",
    analyzer_enabled: "EQ_DebugAnalyzerEnabled",
    analyzer_valid: "EQ_DebugAnalyzerValid",
    analyzer_warm: "EQ_DebugAnalyzerWarmup",
    analyzer_run_count: "EQ_DebugAnalyzerRunCount",
    analyzer_analysis_count: "EQ_DebugAnalyzerAnalysisCount",
    analyzer_deferred_count: "EQ_DebugAnalyzerDeferredCount",
    analyzer_last_cycles: "EQ_DebugAnalyzerLastCycles",
    analyzer_max_cycles: "EQ_DebugAnalyzerMaxCycles",
    smart_compiled: "EQ_DebugSmartBassCompiled",
    smart_enabled: "EQ_DebugSmartBassEnabled",
    smart_strength: "EQ_DebugSmartBassStrength",
    smart_active: "EQ_DebugSmartBassProcessingActive",
    smart_decisions: "EQ_DebugSmartBassDecisionCount",
    smart_level_changes: "EQ_DebugSmartBassLevelChangeCount",
    smart_last_cycles: "EQ_DebugSmartBassLastCycles",
    smart_max_cycles: "EQ_DebugSmartBassMaxCycles",
    smart_saturation: "EQ_DebugSmartBassSaturationCount",
    smart_nonfinite: "EQ_DebugSmartBassNonFiniteCount",
    clarity_compiled: "EQ_DebugDynamicClarityCompiled",
    clarity_enabled: "EQ_DebugDynamicClarityEnabled",
    clarity_strength: "EQ_DebugDynamicClarityStrength",
    clarity_active: "EQ_DebugDynamicClarityProcessingActive",
    clarity_decisions: "EQ_DebugDynamicClarityDecisionCount",
    clarity_level_changes: "EQ_DebugDynamicClarityLevelChangeCount",
    clarity_last_cycles: "EQ_DebugDynamicClarityLastCycles",
    clarity_max_cycles: "EQ_DebugDynamicClarityMaxCycles",
    clarity_saturation: "EQ_DebugDynamicClaritySaturationCount",
    clarity_nonfinite: "EQ_DebugDynamicClarityNonFiniteCount",
    guard_compiled: "EQ_DebugHarshnessGuardCompiled",
    guard_enabled: "EQ_DebugHarshnessGuardEnabled",
    guard_strength: "EQ_DebugHarshnessGuardStrength",
    guard_active: "EQ_DebugHarshnessGuardProcessingActive",
    guard_decisions: "EQ_DebugHarshnessGuardDecisionCount",
    guard_level_changes: "EQ_DebugHarshnessGuardLevelChangeCount",
    guard_last_cycles: "EQ_DebugHarshnessGuardLastCycles",
    guard_max_cycles: "EQ_DebugHarshnessGuardMaxCycles",
    guard_saturation: "EQ_DebugHarshnessGuardSaturationCount",
    guard_nonfinite: "EQ_DebugHarshnessGuardNonFiniteCount",
    lcd_enabled: "EQ_DebugLcdEnabled",
    lcd_mask: "EQ_DebugLcdRuntimeMask",
    lcd_refreshes: "EQ_DebugLcdRefreshCount",
    lcd_jobs: "EQ_DebugLcdJobCount",
    lcd_pending: "EQ_DebugLcdPendingMask",
    lcd_deferred_audio: "EQ_DebugLcdDeferredAudioCount",
    lcd_audio_arrived: "EQ_DebugLcdAudioArrivedDuringDrawCount",
    lcd_unexpected: "EQ_DebugLcdUnexpectedFullRedrawCount",
    lcd_static_draws: "EQ_DebugLcdStaticDrawCount",
    lcd_bounds: "EQ_DebugLcdBoundsFailureCount",
    lcd_hard_budget: "EQ_DebugLcdHardBudgetExceededCount",
    lcd_over_1ms: "EQ_DebugLcdOver1msCount",
    lcd_over_2ms: "EQ_DebugLcdOver2msCount",
    lcd_over_5ms: "EQ_DebugLcdOver5msCount",
    lcd_last_job: "EQ_DebugLcdLastJob",
    lcd_last_cycles: "EQ_DebugLcdLastJobCycles",
    lcd_max_cycles: "EQ_DebugLcdMaxJobCycles",
    lcd_auto_disabled: "EQ_DebugLcdAutoDisabledCount",
    lcd_analyzer_max_strip_height: "EQ_DebugLcdAnalyzerMaxStripHeight",
    lcd_analyzer_strip_count: "EQ_DebugLcdAnalyzerStripCount",
    lcd_raster_fault: "EQ_DebugLcdRasterFaultCount",
    lcd_sync_lost: "EQ_DebugLcdSyncLostCount",
    lcd_fifo_underflow: "EQ_DebugLcdFifoUnderflowCount",
    lcd_frame_mismatch: "EQ_DebugLcdFrameAddressMismatchCount",
    lcd_canary_checks: "EQ_DebugLcdFramebufferCanaryCheckCount",
    lcd_canary_failures: "EQ_DebugLcdFramebufferCanaryFailureCount",
    lcd_fault_latched: "EQ_DebugLcdFaultLatched",
    lcd_double_buffer_enabled: "EQ_DebugLcdDoubleBufferEnabled",
    lcd_front_page: "EQ_DebugLcdFrontPage",
    lcd_swap_target_page: "EQ_DebugLcdSwapTargetPage",
    lcd_swap_pending: "EQ_DebugLcdSwapPending",
    lcd_swap_descriptor_mask: "EQ_DebugLcdSwapDescriptorMask",
    lcd_eof_count: "EQ_DebugLcdEofCount",
    lcd_eof_ambiguous: "EQ_DebugLcdEofAmbiguousCount",
    lcd_swap_requests: "EQ_DebugLcdSwapRequestCount",
    lcd_swap_completes: "EQ_DebugLcdSwapCompleteCount",
    lcd_raster_stop_timeout: "EQ_DebugLcdRasterStopTimeoutCount",
    lcd_expected_base: "EQ_DebugLcdExpectedFrameBase",
    lcd_expected_end: "EQ_DebugLcdExpectedFrameEnd",
    lcd_current_base: "EQ_DebugLcdCurrentFrameBase",
    lcd_current_end: "EQ_DebugLcdCurrentFrameEnd",
    lcd_current1_base: "EQ_DebugLcdCurrentFrame1Base",
    lcd_current1_end: "EQ_DebugLcdCurrentFrame1End"
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

function logStep(name, before, after, detail) {
    results.write(jsonStringify({
        stage: name,
        result_label: "OBSERVED_ON_CONNECTED_BOARD",
        before: before,
        after: after,
        detail: detail || ""
    }) + "\n");
    results.flush();
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

function readArraySize(sizeSymbol, label) {
    var value = numberValue(sizeSymbol);
    requireCondition(value > 0 && value <= 128 && Math.floor(value) == value,
        label + " size is invalid: " + value);
    arraySizeSources[label] = sizeSymbol;
    return value;
}

function initializeArraySizes() {
    arraySizeSources = {};
    arraySizes = {
        lcd_category_count: readArraySize(
            "EQ_DebugLcdCategoryCountSize",
            "lcd_category_count"),
        lcd_job_type_count: readArraySize(
            "EQ_DebugLcdJobTypeCountSize",
            "lcd_job_type_count"),
        ui_job_count: readArraySize(
            "EQ_DebugUiJobCountSize",
            "ui_job_count"),
        dynamic_hitbox_count: readArraySize(
            "EQ_DebugDynamicHitboxCountSize",
            "dynamic_hitbox_count"),
        editor_hitbox_count: readArraySize(
            "EQ_DebugEditorHitboxCountSize",
            "editor_hitbox_count")
    };
    requireCondition(arraySizes.lcd_job_type_count == arraySizes.ui_job_count,
        "LCD job array size differs from UI job count");
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
    requireCondition(arraySizes !== null,
        "array sizes must be read before snapshot arrays");
    for (key in symbols) state[key] = numberValue(symbols[key]);
    state.array_sizes = arraySizes;
    state.array_size_sources = arraySizeSources;
    state.band_gain_db = [];
    state.editor_draft_gain_db = [];
    state.editor_submitted_gain_db = [];
    state.editor_applied_gain_db = [];
    for (index = 0; index < 10; index++) {
        state.band_gain_db.push(numberValue(
            "EQ_DebugBandGainDb[" + index + "]"));
        state.editor_draft_gain_db.push(0.5 * numberValue(
            "(int)EQ_DebugUiEditorDraftGainHalfDb[" + index + "]"));
        state.editor_submitted_gain_db.push(0.5 * numberValue(
            "(int)EQ_DebugUiEditorSubmittedGainHalfDb[" + index + "]"));
        state.editor_applied_gain_db.push(0.5 * numberValue(
            "(int)EQ_DebugUiEditorAppliedGainHalfDb[" + index + "]"));
    }
    state.lcd_category_count = [];
    state.lcd_category_last_cycles = [];
    state.lcd_category_max_cycles = [];
    for (index = 0; index < arraySizes.lcd_category_count; index++) {
        state.lcd_category_count.push(numberValue(
            "EQ_DebugLcdCategoryCount[" + index + "]"));
        state.lcd_category_last_cycles.push(numberValue(
            "EQ_DebugLcdCategoryLastCycles[" + index + "]"));
        state.lcd_category_max_cycles.push(numberValue(
            "EQ_DebugLcdCategoryMaxCycles[" + index + "]"));
    }
    state.lcd_job_type_count = [];
    state.lcd_job_type_last_cycles = [];
    state.lcd_job_type_max_cycles = [];
    for (index = 0; index < arraySizes.lcd_job_type_count; index++) {
        state.lcd_job_type_count.push(numberValue(
            "EQ_DebugLcdJobTypeCount[" + index + "]"));
        state.lcd_job_type_last_cycles.push(numberValue(
            "EQ_DebugLcdJobTypeLastCycles[" + index + "]"));
        state.lcd_job_type_max_cycles.push(numberValue(
            "EQ_DebugLcdJobTypeMaxCycles[" + index + "]"));
    }
    return state;
}

function delta(after, before, name) {
    return after[name] - before[name];
}

function sequenceDelta(after, before) {
    var value = after - before;
    return value < 0 ? value + UINT32_MODULUS : value;
}

function runFor(milliseconds) {
    debugSession.target.runAsynch();
    Thread.sleep(milliseconds);
    debugSession.target.halt();
}

function waitForState(name, timeoutSeconds, predicate) {
    var elapsed = 0, state = null;
    while (elapsed < timeoutSeconds * 1000) {
        runFor(100);
        elapsed += 100;
        state = snapshot();
        if (predicate(state)) return state;
    }
    throw name + " timed out after " + timeoutSeconds + " seconds";
}

function waitForAction(expectedAction, label) {
    var before = snapshot(), observed = null, after;
    System.out.println("ACTION_REQUIRED=" + label);
    System.out.flush();
    observed = waitForState("touch action " + label,
        interactionTimeoutSeconds, function(state) {
            return state.touch_actions != before.touch_actions;
        });
    requireCondition(delta(observed, before, "touch_actions") == 1,
        label + ": one press generated more than one action");
    requireCondition(observed.touch_last_action == expectedAction,
        label + ": expected action=" + expectedAction +
        " actual=" + observed.touch_last_action);
    runFor(500);
    after = snapshot();
    requireCondition(after.touch_actions == observed.touch_actions,
        label + ": held press repeated before re-arm");
    requireCondition(after.touch_pressed == 0,
        label + ": Touch did not return to released state");
    requireCondition(after.touch_rejected == before.touch_rejected,
        label + ": rejected Touch count changed");
    logStep("physical_touch_" + label, before, after,
        "one complete press/release;action=" + expectedAction);
    return {before: before, observed: observed, after: after};
}

function waitForStableCalibrationRelease(label, stableMilliseconds) {
    var elapsed = 0, stable = 0, state = null;
    while (elapsed < interactionTimeoutSeconds * 1000) {
        runFor(100);
        elapsed += 100;
        state = snapshot();
        if (state.touch_pressed == 0) {
            stable += 100;
            if (stable >= stableMilliseconds) return state;
        } else {
            stable = 0;
        }
    }
    throw "calibration stable release " + label + " timed out after " +
        interactionTimeoutSeconds + " seconds";
}

function captureCalibrationPoint(label, expectedX, expectedY) {
    var before = snapshot(), pressed, released, record;
    System.out.println("CALIBRATION_REQUIRED=" + label + "_PRESS_HOLD");
    System.out.flush();
    pressed = waitForState("calibration press " + label,
        interactionTimeoutSeconds, function(state) {
            return state.touch_pressed == 1;
        });
    System.out.println("CALIBRATION_RELEASE=" + label);
    System.out.flush();
    released = waitForState("calibration release " + label,
        interactionTimeoutSeconds, function(state) {
            return state.touch_pressed == 0;
        });
    released = waitForStableCalibrationRelease(label, 500);
    record = {
        label: label,
        expected_screen_x: expectedX,
        expected_screen_y: expectedY,
        raw_x: pressed.touch_raw_x,
        raw_y: pressed.touch_raw_y,
        screen_x: pressed.touch_screen_x,
        screen_y: pressed.touch_screen_y,
        pressed: pressed.touch_pressed,
        released: released.touch_pressed == 0,
        action_count_before: before.touch_actions,
        action_count_after: released.touch_actions,
        last_action: released.touch_last_action,
        rejected_count_before: before.touch_rejected,
        rejected_count_after: released.touch_rejected,
        max_cycles: released.touch_max_cycles
    };
    touchCalibration.push(record);
    logStep("touch_calibration_" + label, before, released,
        "raw=" + record.raw_x + "," + record.raw_y +
        ";screen=" + record.screen_x + "," + record.screen_y);
    return record;
}

function averagePair(first, second) {
    return (first + second) * 0.5;
}

function arrayMinimum(values) {
    var value = values[0], index;
    for (index = 1; index < values.length; index++) {
        if (values[index] < value) value = values[index];
    }
    return value;
}

function arrayMaximum(values) {
    var value = values[0], index;
    for (index = 1; index < values.length; index++) {
        if (values[index] > value) value = values[index];
    }
    return value;
}

function deriveTouchCalibration(points) {
    var rawX = [], rawY = [], index;
    var horizontalX, horizontalY, verticalX, verticalY, swap;
    var leftAxis, rightAxis, topAxis, bottomAxis;
    for (index = 0; index < points.length; index++) {
        rawX.push(points[index].raw_x);
        rawY.push(points[index].raw_y);
    }
    horizontalX = Math.abs(points[1].raw_x - points[0].raw_x) +
        Math.abs(points[3].raw_x - points[2].raw_x);
    horizontalY = Math.abs(points[1].raw_y - points[0].raw_y) +
        Math.abs(points[3].raw_y - points[2].raw_y);
    verticalX = Math.abs(points[2].raw_x - points[0].raw_x) +
        Math.abs(points[3].raw_x - points[1].raw_x);
    verticalY = Math.abs(points[2].raw_y - points[0].raw_y) +
        Math.abs(points[3].raw_y - points[1].raw_y);
    swap = horizontalY > horizontalX && verticalX > verticalY;
    if (swap) {
        leftAxis = averagePair(points[0].raw_y, points[2].raw_y);
        rightAxis = averagePair(points[1].raw_y, points[3].raw_y);
        topAxis = averagePair(points[0].raw_x, points[1].raw_x);
        bottomAxis = averagePair(points[2].raw_x, points[3].raw_x);
    } else {
        leftAxis = averagePair(points[0].raw_x, points[2].raw_x);
        rightAxis = averagePair(points[1].raw_x, points[3].raw_x);
        topAxis = averagePair(points[0].raw_y, points[1].raw_y);
        bottomAxis = averagePair(points[2].raw_y, points[3].raw_y);
    }
    return {
        raw_x_min_observed: arrayMinimum(rawX),
        raw_x_max_observed: arrayMaximum(rawX),
        raw_y_min_observed: arrayMinimum(rawY),
        raw_y_max_observed: arrayMaximum(rawY),
        swap_xy_derived: swap,
        flip_x_derived: rightAxis < leftAxis,
        flip_y_derived: bottomAxis < topAxis,
        horizontal_raw_x_span: horizontalX,
        horizontal_raw_y_span: horizontalY,
        vertical_raw_x_span: verticalX,
        vertical_raw_y_span: verticalY
    };
}

function runTouchCalibration() {
    var points = [], result, index;
    touchCalibration = [];
    points.push(captureCalibrationPoint("LEFT_TOP", 24, 24));
    points.push(captureCalibrationPoint("RIGHT_TOP", 775, 24));
    points.push(captureCalibrationPoint("LEFT_BOTTOM", 24, 455));
    points.push(captureCalibrationPoint("RIGHT_BOTTOM", 775, 455));
    points.push(captureCalibrationPoint("CENTER", 400, 240));
    result = {
        points: touchCalibration,
        derived_transform: deriveTouchCalibration(points)
    };
    for (index = 0; index < points.length; index++) {
        requireCondition(Math.abs(points[index].screen_x -
            points[index].expected_screen_x) <= 80 &&
            Math.abs(points[index].screen_y -
            points[index].expected_screen_y) <= 60,
            points[index].label + ": mapped screen coordinate is outside " +
            "the calibration tolerance");
    }
    requireCondition(result.derived_transform.raw_x_max_observed >
        result.derived_transform.raw_x_min_observed &&
        result.derived_transform.raw_y_max_observed >
        result.derived_transform.raw_y_min_observed,
        "Touch raw calibration spans are degenerate");
    return result;
}

function waitForPage(page, name) {
    return waitForState(name, 30, function(state) {
        return state.requested_page == page &&
            state.displayed_page == page && state.page_building == 0;
    });
}

function waitForApplied(token, preset, name) {
    return waitForState(name, 15, function(state) {
        return state.request_token == token &&
            state.applied_token == token && state.applied_mode == preset &&
            state.transition_active == 0;
    });
}

function waitForTransitionStart(token, name) {
    return waitForState(name, 15, function(state) {
        return state.request_token == token &&
            state.installed_token == token && state.transition_active == 1;
    });
}

function nextStrength(value) {
    return value >= 3 ? 1 : value + 1;
}

function runPhysicalDynamicSequence() {
    var presetCases = [
        { action: ACTION_PRESET_BASS, preset: PRESET_BASS,
          label: "DYNAMIC_PRESET_BASS" },
        { action: ACTION_PRESET_VOCAL, preset: PRESET_VOCAL,
          label: "DYNAMIC_PRESET_VOCAL" },
        { action: ACTION_PRESET_TREBLE, preset: PRESET_TREBLE,
          label: "DYNAMIC_PRESET_TREBLE" },
        { action: ACTION_PRESET_V_SHAPE, preset: PRESET_V_SHAPE,
          label: "DYNAMIC_PRESET_V_SHAPE" },
        { action: ACTION_PRESET_FLAT, preset: PRESET_FLAT,
          label: "DYNAMIC_PRESET_FLAT" }
    ];
    var moduleCases = [
        { action: ACTION_SMART_TOGGLE, label: "DYNAMIC_SMART_TOGGLE",
          enabled: "smart_enabled", strength: "smart_strength",
          isStrength: false },
        { action: ACTION_SMART_STRENGTH, label: "DYNAMIC_SMART_STRENGTH",
          enabled: "smart_enabled", strength: "smart_strength",
          isStrength: true },
        { action: ACTION_CLARITY_TOGGLE, label: "DYNAMIC_CLARITY_TOGGLE",
          enabled: "clarity_enabled", strength: "clarity_strength",
          isStrength: false },
        { action: ACTION_CLARITY_STRENGTH,
          label: "DYNAMIC_CLARITY_STRENGTH",
          enabled: "clarity_enabled", strength: "clarity_strength",
          isStrength: true },
        { action: ACTION_GUARD_TOGGLE, label: "DYNAMIC_GUARD_TOGGLE",
          enabled: "guard_enabled", strength: "guard_strength",
          isStrength: false },
        { action: ACTION_GUARD_STRENGTH, label: "DYNAMIC_GUARD_STRENGTH",
          enabled: "guard_enabled", strength: "guard_strength",
          isStrength: true }
    ];
    var index, touch, applied, item, pageTouch;
    var stageBefore = snapshot();
    requireCondition(stageBefore.displayed_page == PAGE_DYNAMIC &&
        stageBefore.page_building == 0,
        "physical Dynamic sequence did not start on a stable Dynamic page");

    for (index = 0; index < presetCases.length; index++) {
        item = presetCases[index];
        touch = waitForAction(item.action, item.label);
        requireCondition(touch.after.debug_mode == item.preset &&
            sequenceDelta(touch.after.request_token,
                touch.before.request_token) == 2,
            item.label + ": preset action/token mismatch");
        applied = waitForApplied(touch.after.request_token, item.preset,
            item.label + " applied");
        requireCondition(applied.applied_mode == item.preset,
            item.label + ": requested preset was not applied");
    }

    for (index = 0; index < moduleCases.length; index++) {
        item = moduleCases[index];
        touch = waitForAction(item.action, item.label);
        requireCondition(touch.after.request_token ==
            touch.before.request_token,
            item.label + ": dynamic control changed EQ mailbox token");
        if (item.isStrength) {
            requireCondition(touch.after[item.strength] ==
                nextStrength(touch.before[item.strength]),
                item.label + ": strength did not advance exactly once");
        } else {
            requireCondition(touch.after[item.enabled] ==
                (touch.before[item.enabled] == 0 ? 1 : 0),
                item.label + ": enabled state did not toggle exactly once");
        }
    }

    pageTouch = waitForAction(ACTION_PAGE_SWITCH,
        "DYNAMIC_PAGE_TO_EDITOR");
    requireCondition(pageTouch.after.request_token ==
        pageTouch.before.request_token,
        "Dynamic page switch changed EQ mailbox token");
    waitForPage(PAGE_EDITOR, "physical Dynamic-to-Editor page switch");
    logStep("physical_dynamic_12_actions", stageBefore, snapshot(),
        "five presets;six dynamic controls;one page switch");
}

function publishAll(gains) {
    var current = numberValue("EQ_ControlMailbox.sequence");
    var odd, even, band;
    requireCondition((current % 2) == 0,
        "mailbox sequence is odd while target is halted");
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

function gainsNear(actual, expected, tolerance) {
    var index;
    for (index = 0; index < 10; index++) {
        if (Math.abs(actual[index] - expected[index]) > tolerance) return false;
    }
    return true;
}

function verifyBuildIdentity() {
    var targetSha;
    requireCondition(/^[0-9a-f]{40}$/.test(expectedSha),
        "expected SHA is not an exact 40-character SHA");
    requireCondition(numberValue("EQ_DebugBuildMagic") == 0x33030003,
        "build magic mismatch");
    requireCondition(numberValue("EQ_DebugBuildDirty") == 0,
        "target build dirty is not zero");
    targetSha = readCString("EQ_DebugBuildGitSha", 16).toLowerCase();
    requireCondition(targetSha == expectedSha.substring(0, 7),
        "target build SHA mismatch: " + targetSha +
        " expected=" + expectedSha.substring(0, 7));
}

function verifyAudioSafety(before, after, name) {
    var ad = delta(after, before, "ad_frames");
    var da = delta(after, before, "da_frames");
    var process = delta(after, before, "process_frames");
    requireCondition(ad > 0 && da > 0 && process > 0,
        name + ": frame counters did not grow");
    requireCondition(Math.abs(ad - da) <= 1 && Math.abs(ad - process) <= 1,
        name + ": AD/DA/process mismatch");
    requireCondition(delta(after, before, "deadline") == 0 &&
        delta(after, before, "latency_deadline") == 0 &&
        delta(after, before, "overlap") == 0 &&
        delta(after, before, "dropped") == 0 &&
        after.deadline == 0 && after.latency_deadline == 0 &&
        after.overlap == 0 && after.dropped == 0,
        name + ": deadline/latency/overlap/dropped increased");
    requireCondition(delta(after, before, "clip") == 0 && after.clip == 0,
        name + ": clip count is nonzero");
    requireCondition(after.algo_max_cycles < FRAME_BUDGET_CYCLES &&
        after.frame_service_max_cycles < FRAME_BUDGET_CYCLES &&
        after.frame_latency_max_cycles < FRAME_BUDGET_CYCLES &&
        after.frame_latency_max_cycles <= FRAME_LATENCY_ACCEPTANCE_CYCLES,
        name + ": frame cycle budget exceeded");
    requireCondition(after.input_peak > 0 && after.output_peak > 0,
        name + ": no ADC/DAC audio peak observed");
}

function verifyLcdSafety(before, after, name, durationMilliseconds) {
    var index, executedJobs, jobsPerSecond;
    requireCondition(after.lcd_enabled == 1 &&
        after.lcd_mask == FULL_UI_RUNTIME_MASK,
        name + ": full dual-page LCD runtime mask is not active");
    executedJobs = delta(after, before, "lcd_jobs");
    requireCondition(executedJobs > 0,
        name + ": LCD jobs did not run");
    requireCondition(after.lcd_max_cycles <= LCD_NORMAL_JOB_CYCLES,
        name + ": normal LCD job exceeded 2 ms");
    requireCondition(delta(after, before, "lcd_over_2ms") == 0 &&
        delta(after, before, "lcd_over_5ms") == 0 &&
        after.lcd_over_2ms == 0 && after.lcd_over_5ms == 0,
        name + ": LCD over-2ms/over-5ms counter is nonzero");
    requireCondition(after.lcd_max_cycles <= LCD_HARD_CYCLES &&
        delta(after, before, "lcd_hard_budget") == 0 &&
        after.lcd_hard_budget == 0,
        name + ": LCD 5 ms fault cut-off was reached");
    requireCondition(after.lcd_analyzer_max_strip_height <=
        MAX_ANALYZER_STRIP_HEIGHT,
        name + ": Analyzer strip exceeded 16 pixels");
    requireCondition(after.lcd_unexpected == 0 && after.lcd_bounds == 0 &&
        after.lcd_auto_disabled == 0,
        name + ": LCD redraw/bounds/auto-disable fault");
    requireCondition(after.lcd_raster_fault == 0 &&
        after.lcd_sync_lost == 0 && after.lcd_fifo_underflow == 0 &&
        after.lcd_frame_mismatch == 0 && after.lcd_fault_latched == 0 &&
        after.lcd_raster_stop_timeout == 0,
        name + ": raster/fifo/frame fault is nonzero");
    requireCondition(after.lcd_double_buffer_enabled == 1 &&
        after.lcd_swap_pending == 0 && after.lcd_eof_ambiguous == 0 &&
        after.lcd_front_page == after.displayed_page,
        name + ": double-buffer page state is not stable");
    requireCondition(after.lcd_canary_failures == 0 &&
        delta(after, before, "lcd_canary_checks") > 0,
        name + ": framebuffer canary check failed or did not run");
    requireCondition(after.lcd_current_base == after.lcd_expected_base &&
        after.lcd_current_end == after.lcd_expected_end &&
        after.lcd_current1_base == after.lcd_expected_base &&
        after.lcd_current1_end == after.lcd_expected_end,
        name + ": framebuffer descriptors do not match expected range");
    requireCondition(after.lcd_job_type_count.length ==
        after.array_sizes.lcd_job_type_count,
        name + ": LCD job snapshot length mismatch");
    for (index = 0; index < after.lcd_job_type_count.length; index++) {
        if (after.lcd_job_type_count[index] > 0) {
            requireCondition(after.lcd_job_type_max_cycles[index] <=
                LCD_NORMAL_JOB_CYCLES,
                name + ": job type " + index + " exceeded 2 ms");
        }
    }
    for (index = 0; index < after.lcd_category_count.length; index++) {
        if (after.lcd_category_count[index] > 0) {
            requireCondition(after.lcd_category_max_cycles[index] <=
                LCD_NORMAL_JOB_CYCLES,
                name + ": LCD category " + index + " exceeded 2 ms");
        }
    }
    if (durationMilliseconds && durationMilliseconds > 0) {
        jobsPerSecond = executedJobs * 1000.0 / durationMilliseconds;
        requireCondition(jobsPerSecond <= MAX_LCD_JOBS_PER_SECOND,
            name + ": LCD cadence exceeded 8 jobs/s: " + jobsPerSecond);
    }
}

function verifyAnalyzerAndDynamics(before, after, name) {
    requireCondition(after.analyzer_compiled == 1 &&
        after.analyzer_enabled == 1 && after.analyzer_valid == 1 &&
        after.analyzer_warm == 1,
        name + ": Analyzer is not enabled/valid/warm");
    requireCondition(after.analyzer_run_count > before.analyzer_run_count &&
        after.analyzer_analysis_count > before.analyzer_analysis_count,
        name + ": Analyzer run/analysis count did not advance");
    requireCondition(after.smart_compiled == 1 && after.smart_enabled == 1 &&
        after.clarity_compiled == 1 && after.clarity_enabled == 1 &&
        after.guard_compiled == 1 && after.guard_enabled == 1,
        name + ": one or more dynamic modules are not compiled/enabled");
    requireCondition(after.smart_decisions > before.smart_decisions &&
        after.clarity_decisions > before.clarity_decisions &&
        after.guard_decisions > before.guard_decisions,
        name + ": one or more dynamic decision counts did not advance");
    requireCondition(after.smart_saturation == 0 &&
        after.smart_nonfinite == 0 && after.clarity_saturation == 0 &&
        after.clarity_nonfinite == 0 && after.guard_saturation == 0 &&
        after.guard_nonfinite == 0,
        name + ": dynamic saturation/nonfinite counter is nonzero");
}

function runPhysicalEditorSequence() {
    var bandTouch, select125, plusBefore, plus, plusIndex;
    var apply, custom, minus, reset, flat, pageDynamic, applyFrames, applyMs;
    var transitionStart, transitionFrames, transitionMs;
    var zero = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0];

    requireCondition(snapshot().displayed_page == PAGE_EDITOR &&
        snapshot().page_building == 0,
        "physical Editor sequence did not start on a stable Editor page");
    for (var band = 0; band < 10; band++) {
        bandTouch = waitForAction(ACTION_EDITOR_BAND_0 + band,
            "BAND_" + ["31", "62", "125", "250", "500",
                "1K", "2K", "4K", "8K", "16K"][band]);
        requireCondition(bandTouch.after.editor_selected_band == band,
            "band Touch did not select index " + band);
        requireCondition(bandTouch.after.request_token ==
            bandTouch.before.request_token &&
            bandTouch.after.builder_slices == bandTouch.before.builder_slices &&
            gainsNear(bandTouch.after.band_gain_db,
                bandTouch.before.band_gain_db, 0.001),
            "band selection changed mailbox/builder/applied gains");
    }

    select125 = waitForAction(ACTION_EDITOR_BAND_0 + 2,
        "BAND_125_FOR_CUSTOM");
    requireCondition(select125.after.editor_selected_band == 2,
        "125 Hz was not selected before CUSTOM edit");
    plusBefore = select125.after;
    for (plusIndex = 0; plusIndex < 4; plusIndex++) {
        plus = waitForAction(ACTION_EDITOR_PLUS,
            "PLUS_" + (plusIndex + 1));
        requireCondition(sequenceDelta(plus.after.request_token,
            plus.before.request_token) == 0,
            "PLUS changed the mailbox sequence");
        requireCondition(plus.after.builder_slices == plus.before.builder_slices,
            "PLUS started the builder");
        requireCondition(delta(plus.after, plus.before, "draft_version") == 1,
            "PLUS did not create exactly one draft version");
        requireCondition(gainsNear(plus.after.band_gain_db,
            plus.before.band_gain_db, 0.001),
            "PLUS changed applied gains before submission");
    }
    requireCondition(delta(plus.after, plusBefore, "draft_version") == 4 &&
        Math.abs(plus.after.editor_draft_gain_db[2] - 2.0) <= 0.01,
        "four PLUS actions did not create a +2.0 dB 125 Hz draft");

    apply = waitForAction(ACTION_EDITOR_APPLY, "APPLY_ONCE");
    requireCondition(sequenceDelta(apply.after.request_token,
        apply.before.request_token) == 2,
        "APPLY did not publish exactly one mailbox request");
    transitionStart = waitForTransitionStart(apply.after.request_token,
        "single-band CUSTOM transition start");
    custom = waitForApplied(apply.after.request_token, PRESET_CUSTOM,
        "CUSTOM applied");
    applyFrames = delta(custom, apply.observed, "process_frames");
    applyMs = applyFrames * 20.48;
    transitionFrames = delta(custom, transitionStart, "process_frames");
    transitionMs = transitionFrames * 20.48;
    requireCondition(applyMs <= 3000,
        "CUSTOM Apply-to-applied latency exceeded 3 seconds: " + applyMs);
    requireCondition(transitionMs >= 80 && transitionMs <= 180,
        "CUSTOM transition is not consistent with 120 ms: " + transitionMs);
    requireCondition(custom.installed_pair_valid == 1 &&
        custom.control_error == 0 && custom.builder_error == 0,
        "CUSTOM did not complete through builder/install");
    zero[2] = 2.0;
    requireCondition(gainsNear(custom.band_gain_db, zero, 0.01),
        "CUSTOM applied gains do not reflect the 125 Hz +2.0 dB edit");
    requireCondition(gainsNear(custom.editor_applied_gain_db, zero, 0.01),
        "Editor applied-gain snapshot differs from active CUSTOM gains");
    requireCondition(custom.request_token == apply.after.request_token,
        "a second request appeared after APPLY");
    logStep("custom_applied", apply.before, custom,
        "one SET_ALL;CUSTOM applied after transition;latency_ms=" + applyMs +
        ";transition_ms=" + transitionMs);

    minus = waitForAction(ACTION_EDITOR_MINUS, "MINUS_ONCE");
    requireCondition(sequenceDelta(minus.after.request_token,
        minus.before.request_token) == 0,
        "MINUS changed the mailbox sequence");
    requireCondition(delta(minus.after, minus.before, "draft_version") == 1,
        "MINUS did not create exactly one draft version");
    requireCondition(gainsNear(minus.after.band_gain_db, zero, 0.01),
        "MINUS changed applied gains before submission");
    requireCondition(Math.abs(minus.after.editor_draft_gain_db[2] - 1.5) <=
        0.01,
        "MINUS did not leave a +1.5 dB draft at 125 Hz");

    reset = waitForAction(ACTION_EDITOR_RESET_FLAT, "RESET_FLAT");
    requireCondition(sequenceDelta(reset.after.request_token,
        reset.before.request_token) == 2,
        "RESET FLAT did not publish exactly one mailbox request");
    flat = waitForApplied(reset.after.request_token, PRESET_FLAT,
        "RESET FLAT applied");
    requireCondition(gainsNear(flat.band_gain_db,
        [0, 0, 0, 0, 0, 0, 0, 0, 0, 0], 0.01),
        "RESET FLAT did not settle at zero gains");
    logStep("reset_flat_applied", reset.before, flat,
        "RESET_FLAT through mailbox/cache/transition");

    pageDynamic = waitForAction(ACTION_PAGE_SWITCH, "PAGE_TO_DYNAMIC");
    waitForPage(PAGE_DYNAMIC, "Dynamic Status page build");
    logStep("dynamic_status_restored", pageDynamic.before, snapshot(),
        "page tile build completed;Dynamic Status displayed");
}

function runPhysicalMultiBandCustom() {
    var target = [2.0, 1.5, 1.0, 0.0, -1.0,
                  -1.5, 0.0, 1.0, 1.5, 2.0];
    var pageTouch, before, selected, stepTouch, apply, transitionStart;
    var applied, band, step, stepCount, action, applyFrames, transitionFrames;
    var applyMs, transitionMs;

    pageTouch = waitForAction(ACTION_PAGE_SWITCH,
        "MULTI_CUSTOM_PAGE_TO_EDITOR");
    waitForPage(PAGE_EDITOR, "multi-band CUSTOM editor page");
    before = snapshot();
    requireCondition(gainsNear(before.editor_draft_gain_db,
        [0, 0, 0, 0, 0, 0, 0, 0, 0, 0], 0.01),
        "multi-band CUSTOM did not start from a FLAT draft");

    for (band = 0; band < 10; band++) {
        selected = waitForAction(ACTION_EDITOR_BAND_0 + band,
            "MULTI_SELECT_" +
            ["31", "62", "125", "250", "500",
             "1K", "2K", "4K", "8K", "16K"][band]);
        requireCondition(selected.after.editor_selected_band == band,
            "multi-band selection mismatch at index " + band);
        stepCount = Math.round(Math.abs(target[band]) * 2.0);
        action = target[band] < 0 ? ACTION_EDITOR_MINUS :
            ACTION_EDITOR_PLUS;
        for (step = 0; step < stepCount; step++) {
            stepTouch = waitForAction(action,
                "MULTI_BAND_" + band + "_STEP_" + (step + 1));
            requireCondition(stepTouch.after.request_token ==
                stepTouch.before.request_token &&
                stepTouch.after.builder_slices ==
                stepTouch.before.builder_slices,
                "multi-band draft edit reached mailbox/builder");
        }
    }
    requireCondition(gainsNear(snapshot().editor_draft_gain_db,
        target, 0.01),
        "multi-band physical draft does not match the required gains");
    requireCondition(snapshot().request_token == before.request_token &&
        snapshot().builder_slices == before.builder_slices,
        "multi-band draft changed control state before Apply");

    apply = waitForAction(ACTION_EDITOR_APPLY, "MULTI_CUSTOM_APPLY_ONCE");
    requireCondition(sequenceDelta(apply.after.request_token,
        apply.before.request_token) == 2,
        "multi-band Apply did not publish exactly one SET_ALL request");
    transitionStart = waitForTransitionStart(apply.after.request_token,
        "multi-band CUSTOM transition start");
    applied = waitForApplied(apply.after.request_token, PRESET_CUSTOM,
        "multi-band CUSTOM applied");
    applyFrames = delta(applied, apply.observed, "process_frames");
    transitionFrames = delta(applied, transitionStart, "process_frames");
    applyMs = applyFrames * 20.48;
    transitionMs = transitionFrames * 20.48;
    requireCondition(applyMs <= 3000,
        "multi-band Apply-to-applied latency exceeded 3 seconds");
    requireCondition(transitionMs >= 80 && transitionMs <= 180,
        "multi-band transition is not consistent with 120 ms: " +
        transitionMs);
    requireCondition(gainsNear(applied.band_gain_db, target, 0.01) &&
        gainsNear(applied.editor_applied_gain_db, target, 0.01),
        "multi-band applied gains do not match the required vector");
    requireCondition(applied.clip == 0 && applied.control_error == 0 &&
        applied.builder_error == 0,
        "multi-band CUSTOM produced clip/control/builder failure");
    logStep("physical_multi_band_custom", before, applied,
        "one Apply;latency_ms=" + applyMs +
        ";transition_ms=" + transitionMs);
}

function setRequestedPage(page) {
    write("EQ_DebugUiRequestedPage = " + page);
}

function runPageSwitchStress() {
    var before = snapshot(), after, mid, index, quickObserved = 0;
    requireCondition(before.displayed_page == PAGE_EDITOR &&
        before.page_building == 0,
        "page stress did not start from a stable Editor page");
    write("EQ_DebugLcdHardwareAuditRequest = 1");
    runFor(1000);
    before = snapshot();

    for (index = 0; index < 15; index++) {
        setRequestedPage(PAGE_DYNAMIC);
        waitForPage(PAGE_DYNAMIC, "normal page stress Dynamic " + index);
        setRequestedPage(PAGE_EDITOR);
        waitForPage(PAGE_EDITOR, "normal page stress Editor " + index);
    }
    for (index = 0; index < 5; index++) {
        setRequestedPage(PAGE_DYNAMIC);
        runFor(100);
        mid = snapshot();
        requireCondition(mid.requested_page == PAGE_DYNAMIC &&
            mid.page_building == 1,
            "quick page switch did not observe an incomplete tile build");
        quickObserved++;
        setRequestedPage(PAGE_EDITOR);
        waitForPage(PAGE_EDITOR, "quick latest-wins Editor " + index);
    }
    setRequestedPage(PAGE_DYNAMIC);
    waitForPage(PAGE_DYNAMIC, "page stress final Dynamic page");
    write("EQ_DebugLcdHardwareAuditRequest = 1");
    runFor(1000);
    after = snapshot();
    requireCondition(quickObserved == 5 &&
        after.displayed_page == PAGE_DYNAMIC && after.page_building == 0,
        "page stress final page/latest-wins mismatch");
    requireCondition(after.lcd_static_draws == before.lcd_static_draws &&
        after.lcd_unexpected == before.lcd_unexpected &&
        after.lcd_bounds == before.lcd_bounds,
        "page stress caused static/full-redraw/bounds damage");
    requireCondition(after.touch_actions == before.touch_actions,
        "controlled DSS page stress unexpectedly consumed Touch actions");
    verifyAudioSafety(before, after, "controlled DSS page stress");
    verifyLcdSafety(before, after, "controlled DSS page stress", 0);
    logStep("controlled_dss_page_latest_wins", before, after,
        "20 round trips;5 reversals while page tiles incomplete;final=Dynamic");
}

function runStaleRequestCheck() {
    var gainsA = [6, 5, 4, 3, 2, 1, 0, -1, -2, -3];
    var gainsB = [-3, -2, -1, 0, 1, 2, 3, 4, 5, 6];
    var before = snapshot(), tokenA, tokenB, mid, after;
    tokenA = publishAll(gainsA);
    runFor(100);
    mid = snapshot();
    requireCondition(mid.builder_state != 0 || mid.builder_slices > before.builder_slices,
        "stale test request A did not start builder work");
    tokenB = publishAll(gainsB);
    after = waitForApplied(tokenB, PRESET_CUSTOM, "latest SET_ALL applied");
    requireCondition(tokenA != tokenB && after.applied_token == tokenB &&
        after.installed_token == tokenB,
        "latest request B did not own installed/applied state");
    requireCondition(after.installed_token != tokenA &&
        (after.builder_cancel > before.builder_cancel ||
         after.builder_restart > before.builder_restart),
        "stale request A was not invalidated");
    requireCondition(gainsNear(after.band_gain_db, gainsB, 0.01),
        "latest request B gains are not active");
    logStep("stale_latest_wins", before, after,
        "A=" + tokenA + ";B=" + tokenB + ";A never installed");
}

function runDynamicStatusCheck() {
    var ready, before, after;
    write("EQ_DebugAnalyzerEnabled = 1");
    write("EQ_DebugSmartBassEnabled = 1");
    write("EQ_DebugDynamicClarityEnabled = 1");
    write("EQ_DebugHarshnessGuardEnabled = 1");
    ready = waitForState("Analyzer valid/warm", 15, function(state) {
        return state.displayed_page == PAGE_DYNAMIC &&
            state.page_building == 0 && state.analyzer_enabled == 1 &&
            state.analyzer_valid == 1 && state.analyzer_warm == 1;
    });
    requireCondition(ready.displayed_page == PAGE_DYNAMIC &&
        ready.page_building == 0,
        "Dynamic Status page is not stable");
    before = ready;
    write("EQ_DebugLcdHardwareAuditRequest = 1");
    runFor(3000);
    after = snapshot();
    requireCondition(delta(after, before, "lcd_jobs") > 0 &&
        after.lcd_analyzer_strip_count > before.lcd_analyzer_strip_count,
        "Dynamic Status analyzer jobs did not update");
    verifyAnalyzerAndDynamics(before, after, "Dynamic Status");
    verifyAudioSafety(before, after, "Dynamic Status");
    verifyLcdSafety(before, after, "Dynamic Status", 0);
    logStep("dynamic_status", before, after,
        "Analyzer valid/warm;analysis and all dynamic decisions advanced");
}

function runCombinedInteractive() {
    var schedule = [
        { action: ACTION_PRESET_BASS, label: "COMBINED_PRESET_BASS",
          preset: PRESET_BASS },
        { action: ACTION_PAGE_SWITCH, label: "COMBINED_PAGE_EDITOR",
          page: PAGE_EDITOR },
        { action: ACTION_EDITOR_BAND_0 + 2,
          label: "COMBINED_SELECT_125", band: 2 },
        { action: ACTION_EDITOR_PLUS, label: "COMBINED_DRAFT_PLUS" },
        { action: ACTION_PAGE_SWITCH, label: "COMBINED_PAGE_DYNAMIC",
          page: PAGE_DYNAMIC },
        { action: ACTION_SMART_STRENGTH,
          label: "COMBINED_SMART_STRENGTH", strength: "smart_strength" },
        { action: ACTION_CLARITY_STRENGTH,
          label: "COMBINED_CLARITY_STRENGTH",
          strength: "clarity_strength" },
        { action: ACTION_GUARD_STRENGTH,
          label: "COMBINED_GUARD_STRENGTH", strength: "guard_strength" },
        { action: ACTION_PRESET_FLAT, label: "COMBINED_PRESET_FLAT",
          preset: PRESET_FLAT },
        { action: ACTION_PRESET_VOCAL, label: "COMBINED_PRESET_VOCAL",
          preset: PRESET_VOCAL }
    ];
    var before, after, touch, applied, item, index, draftBefore;
    write("EQ_DebugLcdHardwareAuditRequest = 1");
    runFor(1000);
    before = snapshot();
    requireCondition(before.displayed_page == PAGE_DYNAMIC &&
        before.page_building == 0,
        "combined interactive phase did not start on Dynamic page");
    System.out.println("COMBINED_INTERACTIVE_START_MINUTES=10");
    System.out.flush();

    for (index = 0; index < schedule.length; index++) {
        item = schedule[index];
        System.out.println("COMBINED_CONTINUOUS_MINUTE=" + (index + 1));
        System.out.flush();
        runFor(60000);
        draftBefore = snapshot();
        touch = waitForAction(item.action, item.label);
        if (typeof item.preset != "undefined") {
            requireCondition(sequenceDelta(touch.after.request_token,
                touch.before.request_token) == 2,
                item.label + ": preset did not publish one request");
            applied = waitForApplied(touch.after.request_token, item.preset,
                item.label + " applied");
            requireCondition(applied.applied_mode == item.preset,
                item.label + ": preset did not become active");
        } else if (typeof item.page != "undefined") {
            waitForPage(item.page, item.label + " completed");
        } else if (typeof item.band != "undefined") {
            requireCondition(touch.after.editor_selected_band == item.band,
                item.label + ": selected band mismatch");
        } else if (typeof item.strength != "undefined") {
            requireCondition(touch.after[item.strength] ==
                nextStrength(touch.before[item.strength]),
                item.label + ": strength did not advance once");
        } else if (item.action == ACTION_EDITOR_PLUS) {
            requireCondition(touch.after.request_token ==
                touch.before.request_token &&
                touch.after.editor_draft_gain_db[2] ==
                draftBefore.editor_draft_gain_db[2] + 0.5,
                item.label + ": draft-only edit contract failed");
        }
    }

    write("EQ_DebugLcdHardwareAuditRequest = 1");
    runFor(1000);
    after = snapshot();
    requireCondition(delta(after, before, "touch_actions") ==
        schedule.length,
        "combined interactive phase did not observe ten Touch actions");
    requireCondition(after.displayed_page == PAGE_DYNAMIC &&
        after.page_building == 0 && after.applied_mode == PRESET_VOCAL,
        "combined interactive final page/preset mismatch");
    verifyAudioSafety(before, after, "combined interactive ten minutes");
    verifyLcdSafety(before, after, "combined interactive ten minutes",
        10 * 60 * 1000);
    verifyAnalyzerAndDynamics(before, after,
        "combined interactive ten minutes");
    logStep("combined_interactive_10min", before, after,
        "ten one-minute continuous windows;ten physical actions;" +
        "DSS halted only at minute boundaries and Touch polling");
}

function runEndurance() {
    var before, after;
    write("EQ_DebugLcdHardwareAuditRequest = 1");
    runFor(1000);
    before = snapshot();
    System.out.println("ENDURANCE_START_MINUTES=" + enduranceMinutes);
    System.out.flush();
    runFor(enduranceMinutes * 60 * 1000);
    write("EQ_DebugLcdHardwareAuditRequest = 1");
    runFor(1000);
    after = snapshot();
    verifyAudioSafety(before, after, "ten-minute endurance");
    verifyLcdSafety(before, after, "ten-minute endurance",
        enduranceMinutes * 60 * 1000);
    verifyAnalyzerAndDynamics(before, after, "ten-minute endurance");
    requireCondition(after.displayed_page == PAGE_DYNAMIC &&
        after.page_building == 0,
        "page changed or remained incomplete during endurance");
    requireCondition(after.touch_actions == before.touch_actions,
        "unexpected Touch action occurred during unattended endurance");
    requireCondition(after.editor_state_bytes > 0 &&
        after.editor_state_bytes <= 512 && after.total_ui_state_bytes > 0 &&
        after.total_ui_state_bytes <= 1536,
        "editor/UI state byte diagnostics exceed contract");
    logStep("uninterrupted_endurance_10min", before, after,
        "duration_minutes=" + enduranceMinutes +
        ";single continuous target run between start/end snapshots;" +
        "objective counters only;no visual or Touch-feel inference");
    return {before: before, after: after};
}

var beforeAll = null;
var finalState = null;
try {
    requireCondition(new File(ccxml).exists(), "missing CCXML");
    requireCondition(new File(program).exists(), "missing program");
    requireCondition(enduranceMinutes >= 10,
        "endurance must be at least 10 minutes");
    requireCondition(interactionTimeoutSeconds >= 30,
        "interaction timeout is too short");
    requireCondition(buildProfile == "H_project33_full",
        "runner did not identify the exact H build profile");
    requireCondition(/^[0-9a-f]{64}$/.test(expectedProgramSha),
        "runner did not provide the H .out SHA256");
    new File(resultDir).mkdirs();
    results = new FileWriter(resultPath, false);

    script = ScriptingEnvironment.instance();
    script.setScriptTimeout((enduranceMinutes + 40) * 60 * 1000);
    script.traceSetConsoleLevel(TraceLevel.INFO);
    debugServer = script.getServer("DebugServer.1");
    debugServer.setConfig(ccxml);
    debugSession = debugServer.openSession(".*C674X_0");
    debugSession.target.connect();
    debugSession.target.reset();
    Thread.sleep(500);
    debugSession.memory.loadProgram(program);
    Thread.sleep(500);
    runFor(3000);

    verifyBuildIdentity();
    initializeArraySizes();
    beforeAll = snapshot();
    requireCondition(beforeAll.build_dirty == 0,
        "target reports dirty build");
    requireCondition(beforeAll.init_stage == 11,
        "INIT did not reach milestone 11");
    requireCondition(beforeAll.editor_state_bytes > 0 &&
        beforeAll.total_ui_state_bytes > 0,
        "ten-band editor diagnostics are not linked");
    write("EQ_DebugLcdRuntimeMask = " + FULL_UI_RUNTIME_MASK);
    runFor(2000);
    requireCondition(snapshot().lcd_mask == FULL_UI_RUNTIME_MASK,
        "full editor LCD runtime mask did not latch");
    logStep("build_and_init", {}, snapshot(),
        "exact_sha=" + expectedSha + ";out_sha256=" + expectedProgramSha +
        ";profile=" + buildProfile + ";dirty=0;INIT=11");

    var calibrationResult = runTouchCalibration();
    runPhysicalDynamicSequence();
    runPhysicalEditorSequence();
    runPhysicalMultiBandCustom();
    runPageSwitchStress();
    runStaleRequestCheck();
    runDynamicStatusCheck();
    runCombinedInteractive();
    finalState = runEndurance().after;

    writeJson(summaryPath, {
        result_label: "AUTOMATED_BOARD_VALIDATION_COMPLETE",
        completed: true,
        exact_commit_sha: expectedSha,
        program_sha256: expectedProgramSha,
        build_profile: buildProfile,
        dirty: 0,
        init_stage: finalState.init_stage,
        endurance_minutes: enduranceMinutes,
        array_sizes: arraySizes,
        array_size_sources: arraySizeSources,
        touch_calibration: calibrationResult,
        thresholds: {
            frame_budget_cycles: FRAME_BUDGET_CYCLES,
            frame_latency_acceptance_cycles:
                FRAME_LATENCY_ACCEPTANCE_CYCLES,
            lcd_normal_job_cycles: LCD_NORMAL_JOB_CYCLES,
            lcd_hard_cutoff_cycles: LCD_HARD_CYCLES,
            analyzer_max_strip_height: MAX_ANALYZER_STRIP_HEIGHT,
            lcd_max_jobs_per_second: MAX_LCD_JOBS_PER_SECOND
        },
        physical_touch_sequence_observed: true,
        physical_dynamic_action_count: 12,
        physical_editor_band_count: 10,
        physical_multi_band_custom: true,
        controlled_page_round_trips: 20,
        combined_interactive_minutes: 10,
        uninterrupted_endurance_result:
            "MEASURED_ON_CURRENT_BOARD_UNINTERRUPTED_ENDURANCE",
        operator_visual_result: "PENDING_OPERATOR",
        operator_listening_result: "PENDING_OPERATOR",
        automated_counters_are_not_visual_evidence: true,
        final_state: finalState
    });
    System.out.println("HARDWARE_VALIDATION_COMPLETE");
} catch (error) {
    try {
        if (debugSession != null) finalState = snapshot();
    } catch (snapshotError) {}
    try {
        writeJson(summaryPath, {
            result_label: "FAIL",
            completed: false,
            exact_commit_sha: expectedSha,
            error: String(error),
            initial_state: beforeAll,
            final_state: finalState
        });
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
    if (results != null) results.close();
}
