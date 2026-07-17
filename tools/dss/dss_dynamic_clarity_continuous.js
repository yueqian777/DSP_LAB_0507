/* Project 3.3 Dynamic Clarity uninterrupted 300-second board evidence. */
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
var resultDir = env("DSP_TEST_RESULT_DIR",
                    String(System.getProperty("java.io.tmpdir")));
var audioSyncDir = env("DSP_TEST_AUDIO_SYNC_DIR", resultDir + "/audio_sync");
var outputSha256 = env("DSP_TEST_OUTPUT_SHA256", "UNKNOWN");
var resultPath = resultDir + "/dynamic_clarity_continuous_board.json";
var script = null;
var debugServer = null;
var debugSession = null;

var SAMPLE_RATE_HZ = 50000;
var FRAME_LEN = 1024;
var REQUIRED_DSP_SECONDS = 300.0;
var RUN_WINDOW_MILLISECONDS = 303000;
var PATH_COUNT = 5;

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

function saveJson(path, value) {
    var writer = new FileWriter(path, false);
    writer.write(json(value));
    writer.write("\n");
    writer.close();
}

function evaluate(expression) {
    return String(debugSession.expression.evaluate(expression));
}

function numberValue(expression) {
    var text = evaluate(expression);
    var hex = text.match(/-?0x[0-9a-fA-F]+/);
    var value = hex ? parseInt(hex[0], 16) : parseFloat(text);
    if (!isFinite(value)) {
        throw "Unreadable diagnostic " + expression + "=" + text;
    }
    return value;
}

function readArray(symbol, count) {
    var values = [], index;
    for (index = 0; index < count; index++) {
        values.push(numberValue(symbol + "[" + index + "]"));
    }
    return values;
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
    requireCondition(playing.exists(),
                     "Audio acknowledgement timeout for " + name);
    Thread.sleep(250);
}

function utcIso(epochMilliseconds) {
    var formatter = new Packages.java.text.SimpleDateFormat(
        "yyyy-MM-dd'T'HH:mm:ss.SSS'Z'");
    formatter.setTimeZone(Packages.java.util.TimeZone.getTimeZone("UTC"));
    return String(formatter.format(
        new Packages.java.util.Date(Number(epochMilliseconds))));
}

function counterDelta32(startValue, endValue) {
    var delta = endValue - startValue;
    return delta >= 0 ? delta : delta + 4294967296.0;
}

function safetySnapshot() {
    return {
        deadline_miss: numberValue("EQ_DebugDeadlineMissCount"),
        latency_miss: numberValue("EQ_DebugFrameLatencyDeadlineMissCount"),
        overlap: numberValue("EQ_DebugFrameServiceOverlapCount"),
        dropped: numberValue("EQ_DebugFrameServiceDroppedCount"),
        clip: numberValue("EQ_DebugClipCount"),
        smart_saturation: numberValue("EQ_DebugSmartBassSaturationCount"),
        smart_nonfinite: numberValue("EQ_DebugSmartBassNonFiniteCount"),
        clarity_saturation: numberValue(
            "EQ_DebugDynamicClaritySaturationCount"),
        clarity_nonfinite: numberValue(
            "EQ_DebugDynamicClarityNonFiniteCount")
    };
}

function safetyIsZero(state) {
    var key;
    for (key in state) {
        if (state[key] != 0) return false;
    }
    return true;
}

function featureSnapshot() {
    return {
        init_stage: numberValue("EQ_DebugInitStage"),
        ad_frames: numberValue("EQ_DebugAdFrames"),
        da_frames: numberValue("EQ_DebugDaFrames"),
        process_frames: numberValue("EQ_DebugProcessFrames"),
        analyzer: {
            compiled: numberValue("EQ_DebugAnalyzerCompiled"),
            enabled: numberValue("EQ_DebugAnalyzerEnabled"),
            valid: numberValue("EQ_DebugAnalyzerValid"),
            warmup: numberValue("EQ_DebugAnalyzerWarmup"),
            analysis_count: numberValue("EQ_DebugAnalyzerAnalysisCount")
        },
        smart_bass: {
            compiled: numberValue("EQ_DebugSmartBassCompiled"),
            enabled: numberValue("EQ_DebugSmartBassEnabled"),
            processing_active: numberValue(
                "EQ_DebugSmartBassProcessingActive"),
            requested_level: numberValue("EQ_DebugSmartBassRequestedLevel"),
            applied_level: numberValue("EQ_DebugSmartBassAppliedLevel"),
            pending_level: numberValue("EQ_DebugSmartBassPendingLevel"),
            transition_active: numberValue(
                "EQ_DebugSmartBassTransitionActive"),
            decision_count: numberValue("EQ_DebugSmartBassDecisionCount")
        },
        dynamic_clarity: {
            compiled: numberValue("EQ_DebugDynamicClarityCompiled"),
            enabled: numberValue("EQ_DebugDynamicClarityEnabled"),
            processing_active: numberValue(
                "EQ_DebugDynamicClarityProcessingActive"),
            requested_level: numberValue(
                "EQ_DebugDynamicClarityRequestedLevel"),
            applied_level: numberValue("EQ_DebugDynamicClarityAppliedLevel"),
            pending_level: numberValue("EQ_DebugDynamicClarityPendingLevel"),
            transition_active: numberValue(
                "EQ_DebugDynamicClarityTransitionActive"),
            decision_count: numberValue(
                "EQ_DebugDynamicClarityDecisionCount")
        }
    };
}

function requiredFeaturesActive(state) {
    return state.init_stage == 11 &&
        state.analyzer.compiled == 1 && state.analyzer.enabled == 1 &&
        state.analyzer.valid == 1 && state.analyzer.warmup == 1 &&
        state.smart_bass.compiled == 1 && state.smart_bass.enabled == 1 &&
        state.smart_bass.processing_active == 1 &&
        state.dynamic_clarity.compiled == 1 &&
        state.dynamic_clarity.enabled == 1 &&
        state.dynamic_clarity.processing_active == 1;
}

function timingSnapshot() {
    return {
        path_names: [
            "IDENTITY", "STABLE_FILTER", "TRANSITION_0_TO_FILTER",
            "TRANSITION_FILTER_TO_FILTER", "TRANSITION_FILTER_TO_0"
        ],
        frame_count: readArray(
            "EQ_DebugDynamicClarityTimingFrameCount", PATH_COUNT),
        last_cycles: readArray(
            "EQ_DebugDynamicClarityTimingLastCycles", PATH_COUNT),
        max_cycles: readArray(
            "EQ_DebugDynamicClarityTimingMaxCycles", PATH_COUNT),
        max_process_frame: readArray(
            "EQ_DebugDynamicClarityTimingMaxProcessFrame", PATH_COUNT),
        max_active_level: readArray(
            "EQ_DebugDynamicClarityTimingMaxActiveLevel", PATH_COUNT),
        max_pending_level: readArray(
            "EQ_DebugDynamicClarityTimingMaxPendingLevel", PATH_COUNT),
        max_target_level: readArray(
            "EQ_DebugDynamicClarityTimingMaxTargetLevel", PATH_COUNT),
        max_transition_remaining: readArray(
            "EQ_DebugDynamicClarityTimingMaxTransitionRemaining", PATH_COUNT),
        max_analyzer_count: readArray(
            "EQ_DebugDynamicClarityTimingMaxAnalyzerCount", PATH_COUNT),
        max_ad_frames: readArray(
            "EQ_DebugDynamicClarityTimingMaxAdFrames", PATH_COUNT),
        max_da_frames: readArray(
            "EQ_DebugDynamicClarityTimingMaxDaFrames", PATH_COUNT),
        max_flag_ad: readArray(
            "EQ_DebugDynamicClarityTimingMaxFlagAd", PATH_COUNT),
        max_flag_da: readArray(
            "EQ_DebugDynamicClarityTimingMaxFlagDa", PATH_COUNT),
        max_deadline_count: readArray(
            "EQ_DebugDynamicClarityTimingMaxDeadlineCount", PATH_COUNT),
        max_latency_miss_count: readArray(
            "EQ_DebugDynamicClarityTimingMaxLatencyMissCount", PATH_COUNT),
        max_overlap_count: readArray(
            "EQ_DebugDynamicClarityTimingMaxOverlapCount", PATH_COUNT),
        max_dropped_count: readArray(
            "EQ_DebugDynamicClarityTimingMaxDroppedCount", PATH_COUNT),
        max_update_count: readArray(
            "EQ_DebugDynamicClarityTimingMaxUpdateCount", PATH_COUNT),
        dynamic_clarity_last_cycles: numberValue(
            "EQ_DebugDynamicClarityLastCycles"),
        dynamic_clarity_max_cycles: numberValue(
            "EQ_DebugDynamicClarityMaxCycles"),
        frame_service_last_cycles: numberValue(
            "EQ_DebugFrameServiceLastCycles"),
        frame_service_max_cycles: numberValue(
            "EQ_DebugFrameServiceMaxCycles"),
        frame_latency_last_cycles: numberValue(
            "EQ_DebugFrameLatencyLastCycles"),
        frame_latency_max_cycles: numberValue(
            "EQ_DebugFrameLatencyMaxCycles"),
        algo_last_cycles: numberValue("EQ_DebugAlgoLastCycles"),
        algo_max_cycles: numberValue("EQ_DebugAlgoMaxCycles")
    };
}

function appendFailure(failures, condition, message) {
    if (!condition) failures.push(message);
}

try {
    var startState, finalState, startSafety, finalSafety, pathTiming;
    var startFrame, finalFrame, frameDelta, dspAudioSeconds;
    var startMonotonic, finalMonotonic, startUtcMs, finalUtcMs;
    var pcElapsedSeconds, failures, result;

    script = ScriptingEnvironment.instance();
    debugServer = script.getServer("DebugServer.1");
    debugServer.setConfig(ccxml);
    debugSession = debugServer.openSession(".*C674X_0");
    debugSession.target.connect();
    debugSession.target.reset();
    Thread.sleep(500);
    debugSession.memory.loadProgram(program);
    Thread.sleep(500);

    requestAudio("music_like");
    runFor(2500);
    requireCondition(numberValue("EQ_DebugInitStage") == 11,
                     "Project 3.3 did not reach INIT 11");
    requireCondition(numberValue("EQ_DebugAnalyzerCompiled") == 1 &&
                     numberValue("EQ_DebugSmartBassCompiled") == 1 &&
                     numberValue("EQ_DebugDynamicClarityCompiled") == 1,
                     "Required Dynamic/Smart/Clarity feature was not compiled");

    write("EQ_DebugMode = 0");
    write("EQ_DebugAnalyzerEnabled = 1");
    write("EQ_DebugSmartBassEnabled = 1");
    write("EQ_DebugDynamicClarityEnabled = 1");
    runFor(6000);

    startState = featureSnapshot();
    startSafety = safetySnapshot();
    requireCondition(requiredFeaturesActive(startState),
                     "Required feature was not enabled before the window");
    requireCondition(safetyIsZero(startSafety),
                     "Safety counter was non-zero before the window");

    startFrame = startState.process_frames;
    startMonotonic = System.nanoTime();
    startUtcMs = System.currentTimeMillis();

    /* Measurement window: intentionally no expression, Watch, or RAW access. */
    debugSession.target.runAsynch();
    Thread.sleep(RUN_WINDOW_MILLISECONDS);
    debugSession.target.halt();

    finalMonotonic = System.nanoTime();
    finalUtcMs = System.currentTimeMillis();
    finalState = featureSnapshot();
    finalFrame = finalState.process_frames;
    finalSafety = safetySnapshot();
    pathTiming = timingSnapshot();

    frameDelta = counterDelta32(startFrame, finalFrame);
    dspAudioSeconds = frameDelta * FRAME_LEN / SAMPLE_RATE_HZ;
    pcElapsedSeconds = Number(finalMonotonic - startMonotonic) /
        1000000000.0;

    failures = [];
    appendFailure(failures, pcElapsedSeconds >= 303.0,
                  "PC monotonic window was shorter than 303 seconds");
    appendFailure(failures, dspAudioSeconds >= REQUIRED_DSP_SECONDS,
                  "DSP audio frame time was shorter than 300 seconds");
    appendFailure(failures, safetyIsZero(finalSafety),
                  "Final safety counter was non-zero");
    appendFailure(failures, requiredFeaturesActive(finalState),
                  "Required feature was not active at the final boundary");
    appendFailure(failures,
                  finalState.analyzer.analysis_count >
                      startState.analyzer.analysis_count,
                  "Analyzer did not advance during the window");
    appendFailure(failures,
                  finalState.smart_bass.decision_count >
                      startState.smart_bass.decision_count,
                  "Smart Bass did not advance during the window");
    appendFailure(failures,
                  finalState.dynamic_clarity.decision_count >
                      startState.dynamic_clarity.decision_count,
                  "Dynamic Clarity did not advance during the window");

    result = {
        evidence_label: "BOARD_DYNAMIC_CLARITY_CONTINUOUS_300S",
        pass: failures.length == 0,
        program: program,
        output_sha256: outputSha256,
        target_seconds: REQUIRED_DSP_SECONDS,
        host_monotonic_start: String(startMonotonic),
        host_monotonic_end: String(finalMonotonic),
        host_elapsed_seconds: pcElapsedSeconds,
        dsp_start_frame: startFrame,
        dsp_end_frame: finalFrame,
        dsp_frame_delta: frameDelta,
        dsp_elapsed_seconds: dspAudioSeconds,
        start_uart_timestamp: utcIso(startUtcMs),
        end_uart_timestamp: utcIso(finalUtcMs),
        TARGET_WALL_TIME_SECONDS: REQUIRED_DSP_SECONDS,
        MEASURED_HOST_ELAPSED_SECONDS: pcElapsedSeconds,
        MEASURED_DSP_AUDIO_SECONDS: dspAudioSeconds,
        stimulus: {
            name: "music_like",
            playback: "LOOPING_PC_WAV"
        },
        contract: {
            requested_uninterrupted_run_seconds: 303.0,
            required_dsp_audio_seconds: REQUIRED_DSP_SECONDS,
            sample_rate_hz: SAMPLE_RATE_HZ,
            frame_len: FRAME_LEN,
            debugger_access_during_window: "NONE",
            watch_reads_during_window: 0,
            raw_reads_during_window: 0,
            intermediate_dss_access_during_window: 0,
            final_halt_count: 1
        },
        boundaries: {
            start: {
                pc_monotonic_ns: String(startMonotonic),
                pc_utc_epoch_ms: Number(startUtcMs),
                pc_utc: utcIso(startUtcMs),
                dsp_process_frames: startFrame
            },
            end: {
                pc_monotonic_ns: String(finalMonotonic),
                pc_utc_epoch_ms: Number(finalUtcMs),
                pc_utc: utcIso(finalUtcMs),
                dsp_process_frames: finalFrame
            }
        },
        elapsed: {
            pc_monotonic_seconds: pcElapsedSeconds,
            dsp_process_frame_delta: frameDelta,
            dsp_audio_seconds: dspAudioSeconds,
            ad_frame_delta: counterDelta32(
                startState.ad_frames, finalState.ad_frames),
            da_frame_delta: counterDelta32(
                startState.da_frames, finalState.da_frames)
        },
        start_state: startState,
        final_state: finalState,
        start_safety: startSafety,
        final_safety: finalSafety,
        path_timing: pathTiming,
        failures: failures,
        subjective_observation: "NOT_PERFORMED"
    };
    saveJson(resultPath, result);
    debugSession.target.runAsynch();
    Thread.sleep(250);
    debugSession.target.disconnect();
} catch (error) {
    try { if (debugSession != null) debugSession.target.halt(); } catch (ignore0) {}
    saveJson(resultPath, {
        evidence_label: "BOARD_DYNAMIC_CLARITY_CONTINUOUS_300S_FAILED",
        pass: false,
        error: String(error),
        subjective_observation: "NOT_PERFORMED"
    });
    try { if (debugSession != null) debugSession.target.disconnect(); } catch (ignore1) {}
    throw error;
} finally {
    try { if (debugSession != null) debugSession.terminate(); } catch (ignore2) {}
    try { if (debugServer != null) debugServer.stop(); } catch (ignore3) {}
}
