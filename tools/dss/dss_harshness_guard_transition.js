/* Objective Harshness Guard transition capture. */
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
var expectedSha = env("DSP_TEST_EXPECTED_SHA", "UNKNOWN");
var resultPath = resultDir + "/harshness_guard_transition_board.json";
var script = null;
var debugServer = null;
var debugSession = null;

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
        for (index = 0; index < value.length; index++) parts.push(json(value[index]));
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
    if (!isFinite(value)) throw "Unreadable diagnostic " + expression + "=" + text;
    return value;
}

function readCString(symbol, maximum) {
    var value = "", index, code;
    for (index = 0; index < maximum; index++) {
        code = numberValue("(int)" + symbol + "[" + index + "]");
        if (code == 0) break;
        value += String.fromCharCode(code & 0xff);
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
    requireCondition(playing.exists(), "Audio acknowledgement timeout for " + name);
    Thread.sleep(250);
}

function safetySnapshot() {
    return {
        deadline: numberValue("EQ_DebugDeadlineMissCount"),
        latency_miss: numberValue("EQ_DebugFrameLatencyDeadlineMissCount"),
        overlap: numberValue("EQ_DebugFrameServiceOverlapCount"),
        dropped: numberValue("EQ_DebugFrameServiceDroppedCount"),
        clip: numberValue("EQ_DebugClipCount"),
        smart_saturation: numberValue("EQ_DebugSmartBassSaturationCount"),
        smart_nonfinite: numberValue("EQ_DebugSmartBassNonFiniteCount"),
        clarity_saturation: numberValue("EQ_DebugDynamicClaritySaturationCount"),
        clarity_nonfinite: numberValue("EQ_DebugDynamicClarityNonFiniteCount"),
        guard_saturation: numberValue("EQ_DebugHarshnessGuardSaturationCount"),
        guard_nonfinite: numberValue("EQ_DebugHarshnessGuardNonFiniteCount")
    };
}

function requireSafetyZero(state, label) {
    var key;
    for (key in state) {
        requireCondition(state[key] == 0, label + ": non-zero " + key);
    }
}

function saveCapture(label, baseLevel, targetLevel) {
    var inputPath, outputPath, capture;
    write("EQ_DebugHarshnessGuardTransitionCaptureBaseLevel = " + baseLevel);
    write("EQ_DebugHarshnessGuardTransitionCaptureTargetLevel = " + targetLevel);
    write("EQ_DebugHarshnessGuardTransitionCaptureDone = 0");
    write("EQ_DebugHarshnessGuardTransitionCaptureRequest = 1");
    runFor(3000);
    requireCondition(
        numberValue("EQ_DebugHarshnessGuardTransitionCaptureDone") == 1,
        label + ": capture did not finish");
    requireCondition(numberValue("EQ_TriggerCaptureReady") == 1 &&
                     numberValue("EQ_TriggerCaptureSource") == 16,
                     label + ": trigger capture is invalid");
    requireCondition(
        numberValue("EQ_DebugHarshnessGuardTransitionCaptureResult") >= 0,
        label + ": diagnostic level request failed");

    inputPath = resultDir + "/" + label + "_input.raw";
    outputPath = resultDir + "/" + label + "_output.raw";
    debugSession.memory.saveRaw(
        Memory.Page.PROGRAM,
        debugSession.symbol.getAddress("EQ_TriggerCaptureInput"),
        inputPath, 6144, 16, false);
    debugSession.memory.saveRaw(
        Memory.Page.PROGRAM,
        debugSession.symbol.getAddress("EQ_TriggerCaptureOutput"),
        outputPath, 6144, 16, false);
    capture = {
        label: label,
        base_level: baseLevel,
        target_level: targetLevel,
        input: inputPath,
        output: outputPath,
        samples: 12288,
        pre_frames: 4,
        post_frames: 8,
        pre_write_index: numberValue("EQ_TriggerCapturePreWriteIndex"),
        request_frame: numberValue(
            "EQ_DebugHarshnessGuardTransitionCaptureRequestFrame"),
        trigger_frame: numberValue(
            "EQ_DebugHarshnessGuardTransitionCaptureTriggerFrame"),
        done_frame: numberValue(
            "EQ_DebugHarshnessGuardTransitionCaptureDoneFrame")
    };
    requireSafetyZero(safetySnapshot(), label);
    return capture;
}

function runObjective() {
    var result = {}, inputNames, shortNames, inputIndex, captures, band;
    requestAudio("transition_dual");
    runFor(1800);
    requireCondition(numberValue("EQ_DebugInitStage") == 11,
                     "Project 3.3 did not reach INIT 11");
    requireCondition(numberValue("EQ_DebugAnalyzerCompiled") == 1 &&
                     numberValue("EQ_DebugSmartBassCompiled") == 1 &&
                     numberValue("EQ_DebugDynamicClarityCompiled") == 1 &&
                     numberValue("EQ_DebugHarshnessGuardCompiled") == 1,
                     "Required feature was not compiled");
    requireCondition(readCString("EQ_DebugBuildGitSha", 16) == expectedSha &&
                     numberValue("EQ_DebugBuildDirty") == 0,
                     "Build identity is not the expected clean commit");
    write("EQ_DebugAnalyzerEnabled = 1");
    write("EQ_DebugMode = 0");
    write("EQ_DebugSmartBassEnabled = 0");
    write("EQ_DebugDynamicClarityEnabled = 0");
    write("EQ_DebugHarshnessGuardEnabled = 1");
    runFor(1800);
    requireCondition(numberValue("EQ_DebugAppliedMode") == 0 &&
                     numberValue("EQ_DebugTransitionTargetMode") == -1 &&
                     numberValue("EQ_DebugSmartBassAppliedLevel") == 0 &&
                     numberValue("EQ_DebugSmartBassTransitionActive") == 0 &&
                     numberValue("EQ_DebugDynamicClarityAppliedLevel") == 0 &&
                     numberValue("EQ_DebugDynamicClarityTransitionActive") == 0,
                     "Transition capture is not isolated to Harshness Guard");
    for (band = 0; band < 10; band++) {
        requireCondition(Math.abs(numberValue("EQ_DebugBandGainDb[" + band + "]")) <
                         0.001,
                         "FLAT gain is nonzero at band " + band);
    }

    inputNames = ["transition_dual", "transition_music", "transition_noise"];
    shortNames = ["dual", "music", "noise"];
    captures = [];
    for (inputIndex = 0; inputIndex < inputNames.length; inputIndex++) {
        requestAudio(inputNames[inputIndex]);
        captures.push(saveCapture(shortNames[inputIndex] + "_stable_0", 0, 0));
        requestAudio(inputNames[inputIndex]);
        captures.push(saveCapture(shortNames[inputIndex] + "_transition_0_to_1", 0, 1));
        requestAudio(inputNames[inputIndex]);
        captures.push(saveCapture(shortNames[inputIndex] + "_stable_1", 1, 1));
        requestAudio(inputNames[inputIndex]);
        captures.push(saveCapture(shortNames[inputIndex] + "_transition_1_to_2", 1, 2));
        requestAudio(inputNames[inputIndex]);
        captures.push(saveCapture(shortNames[inputIndex] + "_transition_1_to_0", 1, 0));
    }

    requireSafetyZero(safetySnapshot(), "final");
    result.evidence_label = "BOARD_INTERNAL_PCM_OBJECTIVE_TRANSIENT";
    result.pass = true;
    result.program = program;
    result.output_sha256 = outputSha256;
    result.build_git_sha = readCString("EQ_DebugBuildGitSha", 48);
    result.build_dirty = numberValue("EQ_DebugBuildDirty");
    result.init_stage = numberValue("EQ_DebugInitStage");
    result.captures = captures;
    result.function_max_cycles = numberValue(
        "EQ_DebugHarshnessGuardMaxCycles");
    result.frame_service_max_cycles = numberValue(
        "EQ_DebugFrameServiceMaxCycles");
    result.frame_latency_max_cycles = numberValue(
        "EQ_DebugFrameLatencyMaxCycles");
    result.algo_max_cycles = numberValue("EQ_DebugAlgoMaxCycles");
    result.safety = safetySnapshot();
    result.subjective_observation = "NOT_PERFORMED";
    return result;
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
    saveJson(resultPath, runObjective());
    debugSession.target.disconnect();
} catch (error) {
    try { if (debugSession != null) debugSession.target.halt(); } catch (ignore0) {}
    saveJson(resultPath, {
        evidence_label: "BOARD_INTERNAL_PCM_OBJECTIVE_TRANSIENT_FAILED",
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
