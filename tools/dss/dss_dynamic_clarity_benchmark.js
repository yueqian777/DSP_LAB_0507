/* Isolated Dynamic Clarity and Smart Bass board microbenchmark export. */
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
var outputSha256 = env("DSP_TEST_OUTPUT_SHA256", "UNKNOWN");
var cyclePath = resultDir + "/dynamic_clarity_benchmark_cycles.raw";
var metadataPath = resultDir + "/dynamic_clarity_benchmark_board.json";
var script = null;
var debugServer = null;
var debugSession = null;

var JOB_COUNT = 56;
var MEASURED_COUNT = 4096;
var CYCLE_COUNT = JOB_COUNT * MEASURED_COUNT;

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
    if (!isFinite(value)) throw "Unreadable diagnostic " + expression + "=" + text;
    return value;
}

function readArray(symbol, count) {
    var values = [], index;
    for (index = 0; index < count; index++) {
        values.push(numberValue(symbol + "[" + index + "]"));
    }
    return values;
}

function requireCondition(condition, message) {
    if (!condition) throw message;
}

function boardState() {
    return {
        compiled: numberValue("EQ_DebugDynamicClarityBenchmarkCompiled"),
        audio_services_started: numberValue(
            "EQ_DebugDynamicClarityBenchmarkAudioServicesStarted"),
        started: numberValue("EQ_DebugDynamicClarityBenchmarkStarted"),
        running: numberValue("EQ_DebugDynamicClarityBenchmarkRunning"),
        ready: numberValue("EQ_DebugDynamicClarityBenchmarkReady"),
        status: numberValue("EQ_DebugDynamicClarityBenchmarkStatus"),
        completed_jobs: numberValue(
            "EQ_DebugDynamicClarityBenchmarkCompletedJobs"),
        init_stage: numberValue("EQ_DebugInitStage"),
        flag_ad: numberValue("FLAG_AD"),
        flag_da: numberValue("FLAG_DA"),
        dynamic_saturation: numberValue(
            "EQ_DebugDynamicClarityBenchmarkDynamicSaturationCount"),
        dynamic_nonfinite: numberValue(
            "EQ_DebugDynamicClarityBenchmarkDynamicNonFiniteCount"),
        smart_saturation: numberValue(
            "EQ_DebugDynamicClarityBenchmarkSmartSaturationCount"),
        smart_nonfinite: numberValue(
            "EQ_DebugDynamicClarityBenchmarkSmartNonFiniteCount")
    };
}

try {
    var hostStartNs, hostEndNs, state, result;
    script = ScriptingEnvironment.instance();
    debugServer = script.getServer("DebugServer.1");
    debugServer.setConfig(ccxml);
    debugSession = debugServer.openSession(".*C674X_0");
    debugSession.target.connect();
    debugSession.target.reset();
    Thread.sleep(500);
    debugSession.memory.loadProgram(program);
    Thread.sleep(500);

    hostStartNs = System.nanoTime();
    debugSession.target.run();
    hostEndNs = System.nanoTime();

    state = boardState();
    requireCondition(state.compiled == 1, "Benchmark was not compiled");
    requireCondition(state.audio_services_started == 0,
                     "Benchmark audio service marker is not zero");
    requireCondition(state.started == 1 && state.running == 0 &&
                     state.ready == 1 && state.status == 1,
                     "Benchmark did not complete successfully");
    requireCondition(state.completed_jobs == JOB_COUNT,
                     "Benchmark job count mismatch");
    requireCondition(state.init_stage == 0 && state.flag_ad == 0 &&
                     state.flag_da == 0,
                     "Audio initialization or service became active");
    requireCondition(state.dynamic_saturation == 0 &&
                     state.dynamic_nonfinite == 0 &&
                     state.smart_saturation == 0 &&
                     state.smart_nonfinite == 0,
                     "Benchmark safety counter is non-zero");

    debugSession.memory.saveRaw(
        Memory.Page.PROGRAM,
        debugSession.symbol.getAddress(
            "EQ_DebugDynamicClarityBenchmarkCycles"),
        cyclePath, CYCLE_COUNT, 32, false);

    result = {
        evidence_label: "BOARD_ISOLATED_FUNCTION_MICROBENCHMARK",
        pass: true,
        program: program,
        output_sha256: outputSha256,
        host_elapsed_seconds: Number(hostEndNs - hostStartNs) / 1000000000.0,
        job_count: JOB_COUNT,
        measured_count: MEASURED_COUNT,
        warmup_count: 64,
        frame_len: 1024,
        sample_rate_hz: 50000,
        raw_cycles: cyclePath,
        state: state,
        first_call: readArray(
            "EQ_DebugDynamicClarityBenchmarkFirstCall", JOB_COUNT),
        warm_max: readArray(
            "EQ_DebugDynamicClarityBenchmarkWarmMax", JOB_COUNT),
        checksum: readArray(
            "EQ_DebugDynamicClarityBenchmarkChecksum", JOB_COUNT),
        subjective_observation: "NOT_PERFORMED"
    };
    saveJson(metadataPath, result);
    debugSession.target.disconnect();
} catch (error) {
    try { if (debugSession != null) debugSession.target.halt(); } catch (ignore0) {}
    saveJson(metadataPath, {
        evidence_label: "BOARD_ISOLATED_FUNCTION_MICROBENCHMARK_FAILED",
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
