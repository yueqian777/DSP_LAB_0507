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
var enduranceMinutes = parseInt(env("DSP_TEST_ENDURANCE_MINUTES", "10"), 10);
var interactionTimeoutSeconds = parseInt(
    env("DSP_TEST_INTERACTION_TIMEOUT_SECONDS", "180"), 10);
var summaryPath = resultDir + "/summary.json";
var resultPath = resultDir + "/hardware_steps.jsonl";
var FRAME_BUDGET_CYCLES = 9338880;
var LCD_HARD_CYCLES = 2280000;
var UINT32_MODULUS = 4294967296;
var PAGE_DYNAMIC = 0;
var PAGE_EDITOR = 1;
var PRESET_FLAT = 0;
var PRESET_CUSTOM = 5;
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

var symbols = {
    build_dirty: "EQ_DebugBuildDirty",
    init_stage: "EQ_DebugInitStage",
    ad_frames: "EQ_DebugAdFrames",
    da_frames: "EQ_DebugDaFrames",
    process_frames: "EQ_DebugProcessFrames",
    algo_max_cycles: "EQ_DebugAlgoMaxCycles",
    frame_service_max_cycles: "EQ_DebugFrameServiceMaxCycles",
    frame_latency_max_cycles: "EQ_DebugFrameLatencyMaxCycles",
    deadline: "EQ_DebugDeadlineMissCount",
    latency_deadline: "EQ_DebugFrameLatencyDeadlineMissCount",
    overlap: "EQ_DebugFrameServiceOverlapCount",
    dropped: "EQ_DebugFrameServiceDroppedCount",
    clip: "EQ_DebugClipCount",
    input_peak: "EQ_DebugInputPeak",
    output_peak: "EQ_DebugOutputPeak",
    applied_mode: "EQ_DebugAppliedMode",
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
    page_switches: "EQ_DebugUiPageSwitchCount",
    editor_state_bytes: "EQ_DebugUiEditorStateBytes",
    total_ui_state_bytes: "EQ_DebugUiTotalStateBytes",
    touch_actions: "EQ_DebugTouchActionCount",
    touch_rejected: "EQ_DebugTouchRejectedCount",
    touch_last_action: "EQ_DebugTouchLastAction",
    touch_max_cycles: "EQ_DebugTouchMaxCycles",
    analyzer_compiled: "EQ_DebugAnalyzerCompiled",
    analyzer_valid: "EQ_DebugAnalyzerValid",
    smart_compiled: "EQ_DebugSmartBassCompiled",
    clarity_compiled: "EQ_DebugDynamicClarityCompiled",
    guard_compiled: "EQ_DebugHarshnessGuardCompiled",
    lcd_enabled: "EQ_DebugLcdEnabled",
    lcd_mask: "EQ_DebugLcdRuntimeMask",
    lcd_jobs: "EQ_DebugLcdJobCount",
    lcd_pending: "EQ_DebugLcdPendingMask",
    lcd_unexpected: "EQ_DebugLcdUnexpectedFullRedrawCount",
    lcd_bounds: "EQ_DebugLcdBoundsFailureCount",
    lcd_hard_budget: "EQ_DebugLcdHardBudgetExceededCount",
    lcd_max_cycles: "EQ_DebugLcdMaxJobCycles",
    lcd_auto_disabled: "EQ_DebugLcdAutoDisabledCount",
    lcd_raster_fault: "EQ_DebugLcdRasterFaultCount",
    lcd_sync_lost: "EQ_DebugLcdSyncLostCount",
    lcd_fifo_underflow: "EQ_DebugLcdFifoUnderflowCount",
    lcd_frame_mismatch: "EQ_DebugLcdFrameAddressMismatchCount",
    lcd_canary_checks: "EQ_DebugLcdFramebufferCanaryCheckCount",
    lcd_canary_failures: "EQ_DebugLcdFramebufferCanaryFailureCount",
    lcd_fault_latched: "EQ_DebugLcdFaultLatched",
    lcd_expected_base: "EQ_DebugLcdExpectedFrameBase",
    lcd_expected_end: "EQ_DebugLcdExpectedFrameEnd",
    lcd_current_base: "EQ_DebugLcdCurrentFrameBase",
    lcd_current_end: "EQ_DebugLcdCurrentFrameEnd"
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
    state.band_gain_db = [];
    for (index = 0; index < 10; index++) {
        state.band_gain_db.push(numberValue(
            "EQ_DebugBandGainDb[" + index + "]"));
    }
    state.lcd_category_count = [];
    state.lcd_category_max_cycles = [];
    for (index = 0; index < 6; index++) {
        state.lcd_category_count.push(numberValue(
            "EQ_DebugLcdCategoryCount[" + index + "]"));
        state.lcd_category_max_cycles.push(numberValue(
            "EQ_DebugLcdCategoryMaxCycles[" + index + "]"));
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
    logStep("physical_touch_" + label, before, after,
        "one complete press/release;action=" + expectedAction);
    return {before: before, observed: observed, after: after};
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
        delta(after, before, "dropped") == 0,
        name + ": deadline/latency/overlap/dropped increased");
    requireCondition(delta(after, before, "clip") == 0 && after.clip == 0,
        name + ": clip count is nonzero");
    requireCondition(after.algo_max_cycles < FRAME_BUDGET_CYCLES &&
        after.frame_service_max_cycles < FRAME_BUDGET_CYCLES &&
        after.frame_latency_max_cycles < FRAME_BUDGET_CYCLES,
        name + ": frame cycle budget exceeded");
    requireCondition(after.input_peak > 0 && after.output_peak > 0,
        name + ": no ADC/DAC audio peak observed");
}

function verifyLcdSafety(before, after, name) {
    requireCondition(after.lcd_enabled == 1 &&
        after.lcd_mask == FULL_UI_RUNTIME_MASK,
        name + ": full dual-page LCD runtime mask is not active");
    requireCondition(delta(after, before, "lcd_jobs") > 0,
        name + ": LCD jobs did not run");
    requireCondition(after.lcd_max_cycles <= LCD_HARD_CYCLES &&
        delta(after, before, "lcd_hard_budget") == 0,
        name + ": LCD job exceeded the 5 ms hard limit");
    requireCondition(after.lcd_unexpected == 0 && after.lcd_bounds == 0 &&
        after.lcd_auto_disabled == 0,
        name + ": LCD redraw/bounds/auto-disable fault");
    requireCondition(after.lcd_raster_fault == 0 &&
        after.lcd_sync_lost == 0 && after.lcd_fifo_underflow == 0 &&
        after.lcd_frame_mismatch == 0 && after.lcd_fault_latched == 0,
        name + ": raster/fifo/frame fault is nonzero");
    requireCondition(after.lcd_canary_failures == 0 &&
        delta(after, before, "lcd_canary_checks") > 0,
        name + ": framebuffer canary check failed or did not run");
    requireCondition(after.lcd_current_base == after.lcd_expected_base &&
        after.lcd_current_end == after.lcd_expected_end,
        name + ": active framebuffer address does not match expected range");
}

function runPhysicalEditorSequence() {
    var action, plus, apply, custom, minus, reset, flat, pageDynamic;
    var zero = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0];

    action = waitForAction(ACTION_PAGE_SWITCH, "PAGE_TO_EDITOR");
    waitForPage(PAGE_EDITOR, "editor page build");
    for (var band = 0; band < 10; band++) {
        waitForAction(ACTION_EDITOR_BAND_0 + band,
            "BAND_" + ["31", "62", "125", "250", "500",
                "1K", "2K", "4K", "8K", "16K"][band]);
    }

    plus = waitForAction(ACTION_EDITOR_PLUS, "PLUS_ONCE");
    requireCondition(sequenceDelta(plus.after.request_token,
        plus.before.request_token) == 0,
        "PLUS changed the mailbox sequence");
    requireCondition(plus.after.builder_slices == plus.before.builder_slices,
        "PLUS started the builder");
    requireCondition(delta(plus.after, plus.before, "draft_version") == 1,
        "PLUS did not create exactly one draft version");

    apply = waitForAction(ACTION_EDITOR_APPLY, "APPLY_ONCE");
    requireCondition(sequenceDelta(apply.after.request_token,
        apply.before.request_token) == 2,
        "APPLY did not publish exactly one mailbox request");
    custom = waitForApplied(apply.after.request_token, PRESET_CUSTOM,
        "CUSTOM applied");
    requireCondition(custom.installed_pair_valid == 1 &&
        custom.control_error == 0 && custom.builder_error == 0,
        "CUSTOM did not complete through builder/install");
    zero[9] = 0.5;
    requireCondition(gainsNear(custom.band_gain_db, zero, 0.01),
        "CUSTOM applied gains do not reflect the single +0.5 dB edit");
    requireCondition(custom.request_token == apply.after.request_token,
        "a second request appeared after APPLY");
    logStep("custom_applied", apply.before, custom,
        "one SET_ALL;CUSTOM applied after transition");

    minus = waitForAction(ACTION_EDITOR_MINUS, "MINUS_ONCE");
    requireCondition(sequenceDelta(minus.after.request_token,
        minus.before.request_token) == 0,
        "MINUS changed the mailbox sequence");
    requireCondition(delta(minus.after, minus.before, "draft_version") == 1,
        "MINUS did not create exactly one draft version");
    requireCondition(gainsNear(minus.after.band_gain_db, zero, 0.01),
        "MINUS changed applied gains before submission");

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
    var before = snapshot(), after;
    requireCondition(before.displayed_page == PAGE_DYNAMIC &&
        before.page_building == 0,
        "Dynamic Status page is not stable");
    runFor(3000);
    after = snapshot();
    requireCondition(after.analyzer_compiled == 1 &&
        after.smart_compiled == 1 && after.clarity_compiled == 1 &&
        after.guard_compiled == 1,
        "Dynamic Status build features are missing");
    requireCondition(delta(after, before, "lcd_jobs") > 0 &&
        after.lcd_category_count[3] > before.lcd_category_count[3],
        "Dynamic Status analyzer jobs did not update");
    verifyAudioSafety(before, after, "Dynamic Status");
    logStep("dynamic_status", before, after,
        "Analyzer and dynamic modules compiled;runtime jobs observed");
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
    verifyLcdSafety(before, after, "ten-minute endurance");
    requireCondition(after.displayed_page == PAGE_DYNAMIC &&
        after.page_building == 0,
        "page changed or remained incomplete during endurance");
    requireCondition(after.touch_actions == before.touch_actions,
        "unexpected Touch action occurred during unattended endurance");
    requireCondition(after.editor_state_bytes > 0 &&
        after.editor_state_bytes <= 512 && after.total_ui_state_bytes > 0 &&
        after.total_ui_state_bytes <= 1536,
        "editor/UI state byte diagnostics exceed contract");
    logStep("combined_endurance_10min", before, after,
        "duration_minutes=" + enduranceMinutes +
        ";objective counters only;no visual or Touch-feel inference");
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
    new File(resultDir).mkdirs();
    results = new FileWriter(resultPath, false);

    script = ScriptingEnvironment.instance();
    script.setScriptTimeout((enduranceMinutes + 15) * 60 * 1000);
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
        "exact_sha=" + expectedSha + ";dirty=0;INIT=11");

    runPhysicalEditorSequence();
    runStaleRequestCheck();
    runDynamicStatusCheck();
    finalState = runEndurance().after;

    writeJson(summaryPath, {
        result_label: "MEASURED_ON_CURRENT_BOARD",
        completed: true,
        exact_commit_sha: expectedSha,
        dirty: 0,
        init_stage: finalState.init_stage,
        endurance_minutes: enduranceMinutes,
        physical_touch_sequence_observed: true,
        lcd_visual_quality_observed: false,
        touch_feel_observed: false,
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
