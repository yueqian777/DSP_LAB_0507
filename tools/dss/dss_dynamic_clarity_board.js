/* Objective-only Project 3.3 Dynamic Clarity board validation. */
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
var resultDir = env(
    "DSP_TEST_RESULT_DIR", String(System.getProperty("java.io.tmpdir")));
var audioSyncDir = env("DSP_TEST_AUDIO_SYNC_DIR", resultDir + "/audio_sync");
var expectedSha = env("DSP_TEST_EXPECTED_SHA", "47337a0");
var resultPath = resultDir + "/board_result.json";
var script = null;
var debugServer = null;
var debugSession = null;
var failureDiagnostics = null;

function quote(value) {
    return "\"" + String(value).replace(/\\/g, "\\\\")
        .replace(/\"/g, "\\\"").replace(/\r/g, "\\r")
        .replace(/\n/g, "\\n").replace(/\t/g, "\\t") + "\"";
}

function json(value) {
    var kind, parts, key, index;
    if (value === null || typeof value == "undefined") return "null";
    kind = typeof value;
    if (kind == "string") return quote(value);
    if (kind == "number") return isFinite(value) ? String(value) : "null";
    if (kind == "boolean") return value ? "true" : "false";
    parts = [];
    if (value instanceof Array) {
        for (index = 0; index < value.length; index++) {
            parts.push(json(value[index]));
        }
        return "[" + parts.join(",") + "]";
    }
    for (key in value) parts.push(quote(key) + ":" + json(value[key]));
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
    Thread.sleep(milliseconds);
    debugSession.target.halt();
}

function requestAudio(name) {
    var ready = new File(audioSyncDir + "/" + name + ".ready");
    var playing = new File(audioSyncDir + "/" + name + ".playing");
    var writer, waited = 0;
    if (ready.exists()) ready["delete"]();
    if (playing.exists()) playing["delete"]();
    writer = new FileWriter(ready, false);
    writer.write("requested=" + name + "\n");
    writer.close();
    while (!playing.exists() && waited < 5000) {
        Thread.sleep(50);
        waited += 50;
    }
    requireCondition(playing.exists(), "Audio acknowledgement timeout for " + name);
    Thread.sleep(250);
}

function gainsSnapshot() {
    var values = [], band;
    for (band = 0; band < 10; band++) {
        values.push(numberValue("EQ_DebugBandGainDb[" + band + "]"));
    }
    return values;
}

function boardSnapshot() {
    return {
        init_stage: numberValue("EQ_DebugInitStage"),
        ad_frames: numberValue("EQ_DebugAdFrames"),
        da_frames: numberValue("EQ_DebugDaFrames"),
        process_frames: numberValue("EQ_DebugProcessFrames"),
        algo_max_cycles: numberValue("EQ_DebugAlgoMaxCycles"),
        frame_service_max_cycles: numberValue("EQ_DebugFrameServiceMaxCycles"),
        frame_latency_max_cycles: numberValue("EQ_DebugFrameLatencyMaxCycles"),
        deadline: numberValue("EQ_DebugDeadlineMissCount"),
        latency: numberValue("EQ_DebugFrameLatencyDeadlineMissCount"),
        overlap: numberValue("EQ_DebugFrameServiceOverlapCount"),
        dropped: numberValue("EQ_DebugFrameServiceDroppedCount"),
        clip: numberValue("EQ_DebugClipCount"),
        mode: numberValue("EQ_DebugAppliedMode"),
        eq_transition_target: numberValue("EQ_DebugTransitionTargetMode"),
        eq_mode_changes: numberValue("EQ_DebugModeChangeCount"),
        analyzer_compiled: numberValue("EQ_DebugAnalyzerCompiled"),
        analyzer_enabled: numberValue("EQ_DebugAnalyzerEnabled"),
        analyzer_valid: numberValue("EQ_DebugAnalyzerValid"),
        analyzer_warmup: numberValue("EQ_DebugAnalyzerWarmup"),
        analyzer_count: numberValue("EQ_DebugAnalyzerAnalysisCount"),
        analyzer_max_cycles: numberValue("EQ_DebugAnalyzerMaxCycles"),
        analyzer_rms_dbfs: numberValue("EQ_DebugAnalyzerRmsDbfs"),
        bass_db: numberValue("EQ_DebugAnalyzerBassDb"),
        mud_db: numberValue("EQ_DebugAnalyzerMudDb"),
        presence_db: numberValue("EQ_DebugAnalyzerPresenceDb"),
        brightness_db: numberValue("EQ_DebugAnalyzerBrightnessDb"),
        smart_compiled: numberValue("EQ_DebugSmartBassCompiled"),
        smart_enabled: numberValue("EQ_DebugSmartBassEnabled"),
        smart_processing: numberValue("EQ_DebugSmartBassProcessingActive"),
        smart_requested: numberValue("EQ_DebugSmartBassRequestedLevel"),
        smart_applied: numberValue("EQ_DebugSmartBassAppliedLevel"),
        smart_pending: numberValue("EQ_DebugSmartBassPendingLevel"),
        smart_requested_gain_db: numberValue("EQ_DebugSmartBassRequestedGainDb"),
        smart_applied_gain_db: numberValue("EQ_DebugSmartBassAppliedGainDb"),
        smart_transition: numberValue("EQ_DebugSmartBassTransitionActive"),
        smart_progress: numberValue("EQ_DebugSmartBassTransitionProgress"),
        smart_reason: numberValue("EQ_DebugSmartBassReason"),
        smart_decisions: numberValue("EQ_DebugSmartBassDecisionCount"),
        smart_level_changes: numberValue("EQ_DebugSmartBassLevelChangeCount"),
        smart_transitions: numberValue("EQ_DebugSmartBassTransitionCount"),
        smart_invalid_releases: numberValue("EQ_DebugSmartBassInvalidReleaseCount"),
        smart_last_cycles: numberValue("EQ_DebugSmartBassLastCycles"),
        smart_max_cycles: numberValue("EQ_DebugSmartBassMaxCycles"),
        smart_saturation: numberValue("EQ_DebugSmartBassSaturationCount"),
        smart_nonfinite: numberValue("EQ_DebugSmartBassNonFiniteCount"),
        clarity_compiled: numberValue("EQ_DebugDynamicClarityCompiled"),
        clarity_enabled: numberValue("EQ_DebugDynamicClarityEnabled"),
        clarity_processing: numberValue("EQ_DebugDynamicClarityProcessingActive"),
        clarity_mud_db: numberValue("EQ_DebugDynamicClarityMudDb"),
        clarity_presence_db: numberValue("EQ_DebugDynamicClarityPresenceDb"),
        clarity_masking_db: numberValue("EQ_DebugDynamicClarityMaskingDb"),
        clarity_rms_dbfs: numberValue("EQ_DebugDynamicClarityRmsDbfs"),
        clarity_requested: numberValue("EQ_DebugDynamicClarityRequestedLevel"),
        clarity_applied: numberValue("EQ_DebugDynamicClarityAppliedLevel"),
        clarity_pending: numberValue("EQ_DebugDynamicClarityPendingLevel"),
        clarity_requested_gain_db: numberValue("EQ_DebugDynamicClarityRequestedGainDb"),
        clarity_applied_gain_db: numberValue("EQ_DebugDynamicClarityAppliedGainDb"),
        clarity_transition: numberValue("EQ_DebugDynamicClarityTransitionActive"),
        clarity_progress: numberValue("EQ_DebugDynamicClarityTransitionProgress"),
        clarity_reason: numberValue("EQ_DebugDynamicClarityReason"),
        clarity_decisions: numberValue("EQ_DebugDynamicClarityDecisionCount"),
        clarity_level_changes: numberValue("EQ_DebugDynamicClarityLevelChangeCount"),
        clarity_transitions: numberValue("EQ_DebugDynamicClarityTransitionCount"),
        clarity_invalid_releases: numberValue("EQ_DebugDynamicClarityInvalidReleaseCount"),
        clarity_last_cycles: numberValue("EQ_DebugDynamicClarityLastCycles"),
        clarity_max_cycles: numberValue("EQ_DebugDynamicClarityMaxCycles"),
        clarity_saturation: numberValue("EQ_DebugDynamicClaritySaturationCount"),
        clarity_nonfinite: numberValue("EQ_DebugDynamicClarityNonFiniteCount"),
        builder_slices: numberValue("EQ_DebugBuilderSliceCount"),
        control_request: numberValue("EQ_DebugControlRequestToken"),
        control_observed: numberValue("EQ_DebugControlObservedToken"),
        control_accepted: numberValue("EQ_DebugControlAcceptedToken"),
        control_target: numberValue("EQ_DebugControlTargetToken"),
        control_prepared: numberValue("EQ_DebugControlPreparedToken"),
        control_ready_valid: numberValue("EQ_DebugControlReadyValid"),
        control_installed: numberValue("EQ_DebugControlInstalledToken"),
        control_applied: numberValue("EQ_DebugControlAppliedToken"),
        control_installed_pair_valid:
            numberValue("EQ_DebugControlInstalledPairValid"),
        control_rejected: numberValue("EQ_DebugControlRejectedCount"),
        control_coalesced: numberValue("EQ_DebugControlCoalescedCount"),
        control_last_error: numberValue("EQ_DebugControlLastError"),
        builder_state: numberValue("EQ_DebugBuilderState"),
        builder_section: numberValue("EQ_DebugBuilderSectionIndex"),
        builder_scan: numberValue("EQ_DebugBuilderScanIndex"),
        builder_generation: numberValue("EQ_DebugBuilderGeneration"),
        builder_request_token: numberValue("EQ_DebugBuilderRequestToken"),
        builder_cancel: numberValue("EQ_DebugBuilderCancelCount"),
        builder_restart: numberValue("EQ_DebugBuilderRestartCount"),
        builder_last_error: numberValue("EQ_DebugBuilderLastError"),
        response_active_generation:
            numberValue("EQ_DebugResponseActiveGeneration"),
        response_target_generation:
            numberValue("EQ_DebugResponseTargetGeneration")
    };
}

function verifyBuild() {
    var build = {
        sha: readCString("EQ_DebugBuildGitSha", 16),
        dirty: numberValue("EQ_DebugBuildDirty"),
        version: readCString("EQ_DebugBuildVersion", 32)
    };
    requireCondition(build.sha == expectedSha, "Build SHA mismatch " + build.sha);
    requireCondition(build.dirty == 0, "Build dirty mismatch " + build.dirty);
    return build;
}

function checkSafety(state, label) {
    requireCondition(state.init_stage == 11, label + ": INIT did not reach 11");
    requireCondition(state.ad_frames > 0 && state.da_frames > 0 &&
                     state.process_frames > 0, label + ": frame counters did not grow");
    requireCondition(Math.abs(state.ad_frames - state.da_frames) <= 1 &&
                     Math.abs(state.ad_frames - state.process_frames) <= 1,
                     label + ": AD/DA/process mismatch");
    requireCondition(state.deadline == 0 && state.latency == 0 &&
                     state.overlap == 0 && state.dropped == 0,
                     label + ": audio safety counter is nonzero");
    requireCondition(state.clip == 0 && state.smart_saturation == 0 &&
                     state.smart_nonfinite == 0 &&
                     state.clarity_saturation == 0 &&
                     state.clarity_nonfinite == 0,
                     label + ": clip, saturation, or nonfinite counter is nonzero");
    requireCondition(state.clarity_applied_gain_db <= 0.001,
                     label + ": Dynamic Clarity reported positive gain");
}

function checkAnalyzer(state, dominant, label) {
    var values = [state.bass_db, state.mud_db, state.presence_db,
                  state.brightness_db];
    var index;
    requireCondition(state.analyzer_enabled == 1 && state.analyzer_valid == 1 &&
                     state.analyzer_warmup == 1 && state.analyzer_count >= 3,
                     label + ": Analyzer is not valid and warm");
    for (index = 0; index < values.length; index++) {
        if (index != dominant) {
            requireCondition(values[dominant] > values[index],
                             label + ": expected feature is not dominant");
        }
    }
}

function capturePair(label) {
    armCapture();
    runFor(500);
    return saveArmedCapture(label);
}

function armCapture() {
    write("EQ_CaptureManualReady = 0");
    write("EQ_CaptureManualRequest = 0");
    write("EQ_CaptureManualFrameCount = 0");
    write("EQ_CaptureManualIndex = 0");
    write("EQ_CaptureManualRequest = 1");
}

function saveArmedCapture(label) {
    var inputPath = resultDir + "/" + label + "_input.raw";
    var outputPath = resultDir + "/" + label + "_output.raw";
    requireCondition(numberValue("EQ_CaptureManualReady") == 1 &&
                     numberValue("EQ_CaptureManualFrameCount") == 8,
                     label + ": manual capture did not complete");
    debugSession.memory.saveRaw(Memory.Page.PROGRAM,
        debugSession.symbol.getAddress("EQ_CaptureInput"),
        inputPath, 4096, 16, false);
    debugSession.memory.saveRaw(Memory.Page.PROGRAM,
        debugSession.symbol.getAddress("EQ_CaptureOutput"),
        outputPath, 4096, 16, false);
    write("EQ_CaptureManualReady = 0");
    write("EQ_CaptureManualRequest = 0");
    return { input: inputPath, output: outputPath, samples: 8192 };
}

function readSeries(count, milliseconds) {
    var values = [], index;
    for (index = 0; index < count; index++) {
        runFor(milliseconds);
        values.push(boardSnapshot());
    }
    return values;
}

function requireAdjacentClarity(values, label) {
    var index, before, after;
    for (index = 0; index < values.length; index++) {
        requireCondition(values[index].clarity_requested >= 0 &&
                         values[index].clarity_requested <= 4 &&
                         values[index].clarity_applied >= 0 &&
                         values[index].clarity_applied <= 4,
                         label + ": level exceeded MEDIUM ceiling");
        requireCondition(values[index].clarity_applied_gain_db <= 0.001,
                         label + ": positive gain observed");
        if (index > 0) {
            before = values[index - 1].clarity_applied;
            after = values[index].clarity_applied;
            requireCondition(Math.abs(after - before) <= 1,
                             label + ": applied level jumped by more than one");
        }
    }
}

function sameGains(before, after, label) {
    var index;
    for (index = 0; index < before.length; index++) {
        requireCondition(Math.abs(before[index] - after[index]) < 0.001,
                         label + ": base gain changed at band " + index);
    }
}

function sameControlFingerprint(before, after, label, includeResponse) {
    var fields = [
        "control_request", "control_observed", "control_accepted",
        "control_target", "control_prepared", "control_ready_valid",
        "control_installed", "control_applied",
        "control_installed_pair_valid", "control_rejected",
        "control_coalesced", "control_last_error", "builder_state",
        "builder_section", "builder_scan", "builder_generation",
        "builder_request_token", "builder_slices", "builder_cancel",
        "builder_restart", "builder_last_error"
    ];
    var index, field;
    if (includeResponse) {
        fields.push("response_active_generation");
        fields.push("response_target_generation");
    }
    for (index = 0; index < fields.length; index++) {
        field = fields[index];
        requireCondition(before[field] == after[field],
                         label + ": control fingerprint changed at " + field);
    }
}

function requireReleaseConfirmations(start, values, label) {
    var previousLevel = start.clarity_applied;
    var previousDecision = start.clarity_decisions;
    var index, state;
    for (index = 0; index < values.length; index++) {
        state = values[index];
        if (state.clarity_applied < previousLevel) {
            requireCondition(previousLevel - state.clarity_applied == 1,
                             label + ": release skipped an adjacent level");
            requireCondition(state.clarity_decisions - previousDecision >= 2,
                             label + ": release used fewer than two decisions");
            previousLevel = state.clarity_applied;
            previousDecision = state.clarity_decisions;
        }
    }
}

function requirePresetControlDelta(before, after, expectedRequests, label) {
    var expected = before.control_request + 2 * expectedRequests;
    var settledTokens = [
        after.control_request, after.control_observed, after.control_accepted,
        after.control_target, after.control_prepared, after.control_installed,
        after.control_applied
    ];
    var index;
    requireCondition(expected == after.control_request,
                     label + ": unexpected request-token delta");
    for (index = 0; index < settledTokens.length; index++) {
        requireCondition(settledTokens[index] == expected,
                         label + ": control tokens did not settle");
    }
    requireCondition(after.control_ready_valid == 0 &&
                     after.control_rejected == before.control_rejected &&
                     after.control_coalesced == before.control_coalesced &&
                     after.control_last_error == before.control_last_error,
                     label + ": control error state changed");
    requireCondition(after.builder_state == before.builder_state &&
                     after.builder_section == before.builder_section &&
                     after.builder_scan == before.builder_scan &&
                     after.builder_generation == before.builder_generation &&
                     after.builder_request_token == before.builder_request_token &&
                     after.builder_slices == before.builder_slices &&
                     after.builder_cancel == before.builder_cancel &&
                     after.builder_restart == before.builder_restart &&
                     after.builder_last_error == before.builder_last_error,
                     label + ": preset switch unexpectedly used builder");
    requireCondition(after.response_active_generation ==
                     before.response_active_generation + expectedRequests &&
                     after.response_target_generation ==
                     after.response_active_generation,
                     label + ": response generation delta mismatch");
}

function configureBase() {
    write("EQ_DebugDiagPath = 4");
    write("EQ_DebugMode = 0");
    write("EQ_DebugAnalyzerEnabled = 1");
    write("EQ_DebugSmartBassStrength = 2");
    write("EQ_DebugDynamicClarityStrength = 2");
    write("EQ_DebugSmartBassEnabled = 0");
    write("EQ_DebugDynamicClarityEnabled = 0");
}

function bootAndVerify() {
    var state, build;
    requestAudio("presence");
    runFor(1500);
    state = boardSnapshot();
    checkSafety(state, "boot");
    requireCondition(state.analyzer_compiled == 1 &&
                     state.smart_compiled == 1 && state.clarity_compiled == 1,
                     "compile-state Watch values are not all one");
    build = verifyBuild();
    return { build: build, state: state };
}

function runAllObjective() {
    var boot, result = {}, state, start, before, after, series;
    var maskingNames = ["mask02", "mask05", "mask08", "mask12", "mask16"];
    var maskingTargets = [2.0, 5.0, 8.0, 12.0, 16.0];
    var mapping = [], index, previousLevel = -1;
    var release, weakMud, weakPresence, pureMud, triggerState;
    var events = [], tripleBass = false, tripleVocal = false;
    var hStart, hFinal, controlBaseline, controlAfterF;
    var probe100Off, probe100On, probe8kOff, probe8kOn;
    var gCaptureBass = null, gCaptureVocal = null, expectedVocal, expectedLevel;
    var gBassRequested = false, gVocalRequested = false;
    var gBassSettled, gReleaseSeries;

    boot = bootAndVerify();
    result.evidence_label = "MEASURED_ON_CURRENT_BOARD_OBJECTIVE_ONLY";
    result.build = boot.build;
    result.boot = boot.state;
    result.subjective_observation = "NOT_PERFORMED";
    configureBase();

    requestAudio("presence");
    write("EQ_DebugAnalyzerResetRequest = 1");
    runFor(2000);
    controlBaseline = boardSnapshot();
    controlBaseline.gains = gainsSnapshot();
    runFor(60000);
    state = boardSnapshot();
    checkSafety(state, "A runtime OFF");
    requireCondition(state.analyzer_valid == 1 && state.analyzer_warmup == 1,
                     "A Analyzer did not remain valid");
    requireCondition(state.smart_applied == 0 && state.smart_processing == 0 &&
                     state.clarity_applied == 0 &&
                     state.clarity_processing == 0 &&
                     state.clarity_transition == 0,
                     "A runtime OFF path was not identity");
    result.runtime_off_60s = state;
    result.runtime_off_capture = capturePair("runtime_off_identity");

    write("EQ_DebugDynamicClarityEnabled = 1");
    requestAudio("presence");
    write("EQ_DebugAnalyzerResetRequest = 1");
    runFor(5000);
    state = boardSnapshot();
    checkSafety(state, "B Presence identity");
    checkAnalyzer(state, 2, "B Presence identity");
    requireCondition(state.clarity_requested == 0 &&
                     state.clarity_applied == 0 &&
                     state.clarity_transition == 0,
                     "B Presence input did not keep Clarity at identity");
    result.presence_identity = state;
    result.presence_capture = capturePair("presence_identity");

    requestAudio("mud");
    write("EQ_DebugAnalyzerResetRequest = 1");
    runFor(3500);
    pureMud = boardSnapshot();
    checkSafety(pureMud, "C pure Mud gate");
    checkAnalyzer(pureMud, 1, "C pure Mud gate");
    requireCondition(pureMud.clarity_presence_db < -12.0 &&
                     pureMud.clarity_requested == 0 &&
                     pureMud.clarity_applied == 0,
                     "C pure Mud did not exercise the weak-Presence gate");
    result.pure_mud_absolute_gate = pureMud;

    write("EQ_DebugDynamicClarityEnabled = 0");
    requestAudio("mask16");
    write("EQ_DebugAnalyzerResetRequest = 1");
    runFor(3500);
    state = boardSnapshot();
    checkSafety(state, "C trigger OFF baseline");
    result.mud_dominant_off = state;
    result.mud_dominant_off_capture = capturePair("mud_dominant_off");
    write("EQ_DebugDynamicClarityEnabled = 1");
    series = readSeries(45, 100);
    requireAdjacentClarity(series, "C Mud-dominant attack");
    runFor(1000);
    triggerState = boardSnapshot();
    checkSafety(triggerState, "C Mud-dominant trigger");
    requireCondition(triggerState.clarity_mud_db >= 0.0 &&
                     triggerState.clarity_presence_db >= -12.0 &&
                     triggerState.clarity_masking_db >= 14.0 &&
                     triggerState.clarity_requested == 4 &&
                     triggerState.clarity_applied == 4 &&
                     triggerState.clarity_transition == 0,
                     "C Mud-dominant stimulus did not settle at MEDIUM level 4");
    result.mud_dominant_attack = series;
    result.mud_dominant_on = triggerState;
    result.mud_dominant_on_capture = capturePair("mud_dominant_on");

    write("EQ_DebugDynamicClarityEnabled = 0");
    requestAudio("probe100");
    write("EQ_DebugAnalyzerResetRequest = 1");
    runFor(3500);
    probe100Off = boardSnapshot();
    checkSafety(probe100Off, "C 100 Hz probe OFF");
    result.probe100_off = probe100Off;
    result.probe100_off_capture = capturePair("probe100_off");
    write("EQ_DebugDynamicClarityEnabled = 1");
    runFor(5000);
    probe100On = boardSnapshot();
    checkSafety(probe100On, "C 100 Hz probe ON");
    requireCondition(probe100On.clarity_applied == 4,
                     "C 100 Hz probe did not keep Clarity active");
    result.probe100_on = probe100On;
    result.probe100_on_capture = capturePair("probe100_on");

    write("EQ_DebugDynamicClarityEnabled = 0");
    requestAudio("probe8k");
    write("EQ_DebugAnalyzerResetRequest = 1");
    runFor(3500);
    probe8kOff = boardSnapshot();
    checkSafety(probe8kOff, "C 8 kHz probe OFF");
    result.probe8k_off = probe8kOff;
    result.probe8k_off_capture = capturePair("probe8k_off");
    write("EQ_DebugDynamicClarityEnabled = 1");
    runFor(5000);
    probe8kOn = boardSnapshot();
    checkSafety(probe8kOn, "C 8 kHz probe ON");
    requireCondition(probe8kOn.clarity_applied == 4,
                     "C 8 kHz probe did not keep Clarity active");
    result.probe8k_on = probe8kOn;
    result.probe8k_on_capture = capturePair("probe8k_on");

    write("EQ_DebugDynamicClarityEnabled = 0");
    requestAudio("presence");
    runFor(3500);
    write("EQ_DebugDynamicClarityEnabled = 1");
    for (index = 0; index < maskingNames.length; index++) {
        requestAudio(maskingNames[index]);
        write("EQ_DebugAnalyzerResetRequest = 1");
        runFor(4500);
        state = boardSnapshot();
        checkSafety(state, "D masking " + maskingNames[index]);
        requireCondition(state.clarity_mud_db >= 0.0 &&
                         state.clarity_presence_db >= -12.0,
                         "D absolute gates failed for " + maskingNames[index]);
        requireCondition(Math.abs(state.clarity_masking_db -
                                  maskingTargets[index]) <= 2.0,
                         "D measured masking missed target for " +
                         maskingNames[index]);
        expectedLevel = 0;
        if (state.clarity_masking_db > 4.0) {
            expectedLevel = Math.floor(
                ((state.clarity_masking_db - 4.0) / 10.0) * 4.0 + 0.5);
            if (expectedLevel > 4) expectedLevel = 4;
        }
        requireCondition(state.clarity_requested == expectedLevel &&
                         state.clarity_applied == expectedLevel,
                         "D measured masking did not match control formula");
        requireCondition(state.clarity_applied >= previousLevel,
                         "D masking levels were not monotonic");
        previousLevel = state.clarity_applied;
        mapping.push({ stimulus: maskingNames[index],
                       target_masking_db: maskingTargets[index],
                       expected_level: expectedLevel, state: state });
    }
    requireCondition(mapping[0].state.clarity_applied == 0,
                     "D 2 dB stimulus was not identity");
    requireCondition(mapping[mapping.length - 1].state.clarity_applied == 4,
                     "D 16 dB stimulus did not reach MEDIUM ceiling");

    requestAudio("weak_mud");
    write("EQ_DebugAnalyzerResetRequest = 1");
    runFor(4000);
    weakMud = boardSnapshot();
    checkSafety(weakMud, "D weak Mud gate");
    requireCondition(weakMud.clarity_mud_db < 0.0 &&
                     weakMud.clarity_applied == 0,
                     "D weak-Mud absolute gate did not hold identity");
    requestAudio("weak_presence");
    write("EQ_DebugAnalyzerResetRequest = 1");
    runFor(4000);
    weakPresence = boardSnapshot();
    checkSafety(weakPresence, "D weak Presence gate");
    requireCondition(weakPresence.clarity_presence_db < -12.0 &&
                     weakPresence.clarity_applied == 0,
                     "D weak-Presence absolute gate did not hold identity");
    result.masking_mapping = mapping;
    result.weak_mud_gate = weakMud;
    result.weak_presence_gate = weakPresence;

    write("EQ_DebugMode = 0");
    write("EQ_DebugSmartBassEnabled = 0");
    write("EQ_DebugDynamicClarityEnabled = 0");
    requestAudio("presence");
    runFor(4000);
    before = boardSnapshot();
    before.gains = gainsSnapshot();
    write("EQ_DebugSmartBassEnabled = 1");
    write("EQ_DebugDynamicClarityEnabled = 1");
    requestAudio("combined");
    write("EQ_DebugAnalyzerResetRequest = 1");
    runFor(6000);
    after = boardSnapshot();
    after.gains = gainsSnapshot();
    checkSafety(after, "F combined dynamics");
    requireCondition(after.smart_applied > 0 && after.clarity_applied > 0 &&
                     after.smart_transition == 0 &&
                     after.clarity_transition == 0,
                     "F both dynamic modules did not settle active");
    requireCondition(after.builder_slices == before.builder_slices &&
                     after.control_request == before.control_request &&
                     after.control_accepted == before.control_accepted &&
                     after.control_applied == before.control_applied,
                     "F dynamic modules changed builder or EQ control tokens");
    sameGains(before.gains, after.gains, "F combined dynamics");
    result.combined_before = before;
    result.combined_after = after;

    controlAfterF = boardSnapshot();
    controlAfterF.gains = gainsSnapshot();
    sameControlFingerprint(
        controlBaseline, controlAfterF, "A-F dynamic-only control", true);
    sameGains(controlBaseline.gains, controlAfterF.gains,
              "A-F dynamic-only control");
    result.control_fingerprint_before_a = controlBaseline;
    result.control_fingerprint_after_f = controlAfterF;

    requestAudio("presence");
    release = readSeries(140, 50);
    requireAdjacentClarity(release, "E objective release");
    requireReleaseConfirmations(after, release, "E objective release");
    state = release[release.length - 1];
    checkSafety(state, "E objective release");
    requireCondition(state.clarity_applied == 0 &&
                     state.clarity_transition == 0 &&
                     state.clarity_processing == 0,
                     "E Dynamic Clarity did not release to identity");
    requireCondition(state.clarity_level_changes - after.clarity_level_changes <
                     state.clarity_decisions - after.clarity_decisions,
                     "E Dynamic Clarity changed level on every decision");
    result.objective_release = release;
    result.objective_release_final = state;
    result.objective_release_subjective = "NOT_PERFORMED";

    write("EQ_DebugMode = 0");
    requestAudio("presence");
    write("EQ_DebugAnalyzerResetRequest = 1");
    runFor(3000);
    start = boardSnapshot();
    start.gains = gainsSnapshot();
    checkSafety(start, "G identity precondition");
    requireCondition(start.mode == 0 && start.eq_transition_target == -1 &&
                     start.smart_applied == 0 && start.clarity_applied == 0,
                     "G did not start from FLAT dynamic identity");
    requestAudio("combined_transition");
    write("EQ_DebugAnalyzerResetRequest = 1");
    for (index = 0; index < 250; index++) {
        runFor(20);
        state = boardSnapshot();
        checkSafety(state, "G FLAT to BASS poll " + index);
        if (state.smart_transition != 0 || state.clarity_transition != 0 ||
            state.eq_transition_target != -1) {
            events.push({ window: "FLAT_TO_BASS", poll: index,
                mode: state.mode,
                eq_target: state.eq_transition_target,
                smart_level: state.smart_applied,
                smart_transition: state.smart_transition,
                smart_progress: state.smart_progress,
                clarity_level: state.clarity_applied,
                clarity_transition: state.clarity_transition,
                clarity_progress: state.clarity_progress,
                process_frames: state.process_frames });
        }
        failureDiagnostics = {
            window: "FLAT_TO_BASS",
            flat_to_bass_overlap: tripleBass,
            bass_to_vocal_overlap: tripleVocal,
            last_state: state,
            events: events
        };
        if ((!gBassRequested) && state.smart_transition != 0 &&
            state.clarity_transition != 0) {
            armCapture();
            write("EQ_DebugMode = 1");
            gBassRequested = true;
        }
        if (gBassRequested && state.eq_transition_target == 1 &&
            state.smart_transition != 0 && state.clarity_transition != 0) {
            tripleBass = true;
        }
        if (gBassRequested && gCaptureBass === null &&
            state.mode == 1 && state.eq_transition_target == -1 &&
            numberValue("EQ_CaptureManualReady") == 1) {
            gCaptureBass = saveArmedCapture("triple_flat_to_bass");
        }
        if (tripleBass && gCaptureBass !== null && state.mode == 1 &&
            state.smart_applied == 4 && state.clarity_applied == 4 &&
            state.smart_transition == 0 && state.clarity_transition == 0) {
            break;
        }
    }
    requireCondition(tripleBass, "G FLAT to BASS triple overlap not observed");
    requireCondition(gCaptureBass !== null,
                     "G FLAT to BASS transition RAW did not complete");
    requireCondition(state.mode == 1 && state.smart_applied == 4 &&
                     state.clarity_applied == 4,
                     "G FLAT to BASS window did not settle");
    gBassSettled = state;

    requestAudio("presence");
    gReleaseSeries = readSeries(160, 50);
    state = gReleaseSeries[gReleaseSeries.length - 1];
    checkSafety(state, "G BASS identity precondition");
    requireCondition(state.mode == 1 && state.eq_transition_target == -1 &&
                     state.smart_applied == 0 && state.clarity_applied == 0 &&
                     state.smart_transition == 0 && state.clarity_transition == 0,
                     "G dynamics did not release before BASS to VOCAL window");

    requestAudio("combined_transition");
    write("EQ_DebugAnalyzerResetRequest = 1");
    for (index = 0; index < 250; index++) {
        runFor(20);
        state = boardSnapshot();
        checkSafety(state, "G BASS to VOCAL poll " + index);
        if (state.smart_transition != 0 || state.clarity_transition != 0 ||
            state.eq_transition_target != -1) {
            events.push({ window: "BASS_TO_VOCAL", poll: index,
                mode: state.mode, eq_target: state.eq_transition_target,
                smart_level: state.smart_applied,
                smart_transition: state.smart_transition,
                smart_progress: state.smart_progress,
                clarity_level: state.clarity_applied,
                clarity_transition: state.clarity_transition,
                clarity_progress: state.clarity_progress,
                process_frames: state.process_frames });
        }
        failureDiagnostics = {
            window: "BASS_TO_VOCAL",
            flat_to_bass_overlap: tripleBass,
            bass_to_vocal_overlap: tripleVocal,
            last_state: state,
            events: events
        };
        if ((!gVocalRequested) && state.smart_transition != 0 &&
            state.clarity_transition != 0) {
            armCapture();
            write("EQ_DebugMode = 2");
            gVocalRequested = true;
        }
        if (gVocalRequested && state.eq_transition_target == 2 &&
            state.smart_transition != 0 && state.clarity_transition != 0) {
            tripleVocal = true;
        }
        if (gVocalRequested && gCaptureVocal === null &&
            state.mode == 2 && state.eq_transition_target == -1 &&
            numberValue("EQ_CaptureManualReady") == 1) {
            gCaptureVocal = saveArmedCapture("triple_bass_to_vocal");
        }
        if (tripleVocal && gCaptureVocal !== null && state.mode == 2 &&
            state.smart_applied == 4 && state.clarity_applied == 4 &&
            state.smart_transition == 0 && state.clarity_transition == 0) {
            break;
        }
    }
    requireCondition(tripleVocal,
                     "G BASS to VOCAL triple overlap not observed");
    requireCondition(gCaptureVocal !== null,
                     "G BASS to VOCAL transition RAW did not complete");
    requireCondition(state.mode == 2 && state.smart_applied == 4 &&
                     state.clarity_applied == 4,
                     "G BASS to VOCAL window did not settle");
    runFor(1000);
    state = boardSnapshot();
    state.gains = gainsSnapshot();
    checkSafety(state, "G final");
    requireCondition(gCaptureBass !== null && gCaptureVocal !== null,
                     "G transition RAW capture did not complete");
    expectedVocal = [-2, -1, 0, 1, 2, 3, 2, 1, 0, -1];
    sameGains(expectedVocal, state.gains, "G VOCAL final gains");
    requirePresetControlDelta(start, state, 2, "G preset control");
    result.triple_transition = {
        flat_to_bass_overlap: tripleBass,
        bass_to_vocal_overlap: tripleVocal,
        events: events,
        flat_to_bass_capture: gCaptureBass,
        bass_to_vocal_capture: gCaptureVocal,
        start_state: start,
        flat_to_bass_settled: gBassSettled,
        inter_window_release: gReleaseSeries,
        final_state: state
    };

    write("EQ_DebugMode = 0");
    requestAudio("music_like");
    write("EQ_DebugAnalyzerResetRequest = 1");
    runFor(6000);
    hStart = boardSnapshot();
    hStart.gains = gainsSnapshot();
    checkSafety(hStart, "H start");
    requireCondition(hStart.analyzer_valid == 1 &&
                     hStart.analyzer_warmup == 1 &&
                     hStart.smart_enabled == 1 &&
                     hStart.clarity_enabled == 1 &&
                     hStart.smart_decisions > 0 &&
                     hStart.clarity_decisions > 0 &&
                     hStart.smart_applied > 0 &&
                     hStart.clarity_applied > 0 &&
                     hStart.analyzer_rms_dbfs > -45.0,
                     "H feature processing was not active");
    debugSession.target.runAsynch();
    Thread.sleep(300000);
    debugSession.target.halt();
    hFinal = boardSnapshot();
    hFinal.gains = gainsSnapshot();
    checkSafety(hFinal, "H continuous 300s");
    requireCondition(hFinal.process_frames - hStart.process_frames > 14000 &&
                     hFinal.analyzer_count > hStart.analyzer_count &&
                     hFinal.smart_decisions > hStart.smart_decisions &&
                     hFinal.clarity_decisions > hStart.clarity_decisions,
                     "H did not complete 300 seconds of active processing");
    sameControlFingerprint(hStart, hFinal, "H dynamic-only control", true);
    sameGains(hStart.gains, hFinal.gains, "H dynamic-only control");
    result.continuous_300s = {
        label: "CONTINUOUS_300S",
        debugger_access_during_window: "NONE",
        start: hStart,
        final_state: hFinal
    };

    result.pass = true;
    return result;
}

function saveResult(result) {
    var writer = new FileWriter(resultPath, false);
    writer.write(json(result) + "\n");
    writer.close();
}

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
    saveResult(runAllObjective());
    debugSession.target.disconnect();
} catch (error) {
    try { if (debugSession != null) debugSession.target.halt(); } catch (ignore0) {}
    saveResult({ evidence_label: "BOARD_TEST_FAILED", pass: false,
                 error: String(error), diagnostics: failureDiagnostics,
                 subjective_observation: "NOT_PERFORMED" });
    try { if (debugSession != null) debugSession.target.disconnect(); } catch (ignore1) {}
    throw error;
} finally {
    try { if (debugSession != null) debugSession.terminate(); } catch (ignore2) {}
    try { if (debugServer != null) debugServer.stop(); } catch (ignore3) {}
}
