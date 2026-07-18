/* Isolated Smart Bass, Dynamic Clarity, and Harshness Guard benchmark. */
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
var expectedSha = env("DSP_TEST_EXPECTED_SHA", "UNKNOWN");
var kernelDiagnostics = env("DSP_TEST_KERNEL_DIAGNOSTICS", "0") == "1";
var cyclePath = resultDir + "/harshness_guard_benchmark_cycles.raw";
var metadataPath = resultDir + "/harshness_guard_benchmark_board.json";
var script = null;
var debugServer = null;
var debugSession = null;

var MODULES = ["dynamic_clarity", "smart_bass", "harshness_guard"];
var INPUTS = kernelDiagnostics ? [
    "dual_400hz_1953_125hz",
    "deterministic_pseudorandom"
] : [
    "tone_400hz", "dual_400hz_1953_125hz",
    "music_like_multitone", "deterministic_pseudorandom"
];
var CASES = kernelDiagnostics ? [
    "level_0_identity", "level_1_stable", "level_medium_max_stable",
    "transition_0_to_1", "transition_1_to_2", "transition_1_to_0"
] : [
    "level_0_identity", "level_1_stable", "level_medium_max_stable",
    "transition_0_to_1", "transition_1_to_2",
    "transition_medium_to_previous", "transition_1_to_0"
];
var MEDIUM_MAX_LEVEL = {
    dynamic_clarity: 4,
    smart_bass: 4,
    harshness_guard: 3
};
var JOB_COUNT = MODULES.length * INPUTS.length * CASES.length;
var WARMUP_COUNT = 64;
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

function requireCondition(condition, message) {
    if (!condition) throw message;
}

function boardState() {
    return {
        dynamic_benchmark_compiled: numberValue(
            "EQ_DebugDynamicClarityBenchmarkCompiled"),
        harshness_benchmark_compiled: numberValue(
            "EQ_DebugHarshnessGuardBenchmarkCompiled"),
        kernel_diagnostics_compiled: kernelDiagnostics ? numberValue(
            "EQ_DebugHarshnessGuardKernelDiagnosticsCompiled") : 0,
        audio_services_started: numberValue(
            "EQ_DebugDynamicClarityBenchmarkAudioServicesStarted"),
        started: numberValue("EQ_DebugDynamicClarityBenchmarkStarted"),
        running: numberValue("EQ_DebugDynamicClarityBenchmarkRunning"),
        ready: numberValue("EQ_DebugDynamicClarityBenchmarkReady"),
        status: numberValue("EQ_DebugDynamicClarityBenchmarkStatus"),
        completed_jobs: numberValue(
            "EQ_DebugDynamicClarityBenchmarkCompletedJobs"),
        init_stage: numberValue("EQ_DebugInitStage"),
        ad_en: numberValue("AD_En"),
        da_en: numberValue("DA_En"),
        flag_ad: numberValue("FLAG_AD"),
        flag_da: numberValue("FLAG_DA"),
        dynamic_saturation: numberValue(
            "EQ_DebugDynamicClarityBenchmarkDynamicSaturationCount"),
        dynamic_nonfinite: numberValue(
            "EQ_DebugDynamicClarityBenchmarkDynamicNonFiniteCount"),
        smart_saturation: numberValue(
            "EQ_DebugDynamicClarityBenchmarkSmartSaturationCount"),
        smart_nonfinite: numberValue(
            "EQ_DebugDynamicClarityBenchmarkSmartNonFiniteCount"),
        harshness_saturation: numberValue(
            "EQ_DebugDynamicClarityBenchmarkHarshnessSaturationCount"),
        harshness_nonfinite: numberValue(
            "EQ_DebugDynamicClarityBenchmarkHarshnessNonFiniteCount")
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
    evaluate("AD_En = 0");
    evaluate("DA_En = 0");
    Thread.sleep(100);
    evaluate("FLAG_AD = 0");
    evaluate("FLAG_DA = 0");
    requireCondition(numberValue("AD_En") == 0 &&
                     numberValue("DA_En") == 0 &&
                     numberValue("FLAG_AD") == 0 &&
                     numberValue("FLAG_DA") == 0,
                     "Unable to clear stale audio service flags");

    hostStartNs = System.nanoTime();
    debugSession.target.run();
    hostEndNs = System.nanoTime();

    state = boardState();
    requireCondition(readCString("EQ_DebugBuildGitSha", 16) == expectedSha &&
                     numberValue("EQ_DebugBuildDirty") == 0,
                     "Benchmark build identity is not the expected clean commit");
    requireCondition(state.dynamic_benchmark_compiled == 1 &&
                     state.harshness_benchmark_compiled == 1,
                     "Three-module benchmark was not compiled");
    requireCondition(!kernelDiagnostics ||
                     state.kernel_diagnostics_compiled == 1,
                     "Kernel diagnostics were not compiled");
    requireCondition(state.audio_services_started == 0,
                     "Benchmark audio service marker is not zero");
    requireCondition(state.started == 1 && state.running == 0 &&
                     state.ready == 1 && state.status == 1,
                     "Benchmark did not complete successfully");
    requireCondition(state.completed_jobs == JOB_COUNT,
                     "Benchmark job count mismatch");
    requireCondition(state.init_stage == 0 && state.ad_en == 0 &&
                     state.da_en == 0 && state.flag_ad == 0 &&
                     state.flag_da == 0,
                     "Audio initialization or service became active");
    requireCondition(state.dynamic_saturation == 0 &&
                     state.dynamic_nonfinite == 0 &&
                     state.smart_saturation == 0 &&
                     state.smart_nonfinite == 0 &&
                     state.harshness_saturation == 0 &&
                     state.harshness_nonfinite == 0,
                     "Benchmark safety counter is non-zero");

    debugSession.memory.saveRaw(
        Memory.Page.PROGRAM,
        debugSession.symbol.getAddress(
            "EQ_DebugDynamicClarityBenchmarkCycles"),
        cyclePath, CYCLE_COUNT, 32, false);

    result = {
        evidence_label: "BOARD_ISOLATED_THREE_MODULE_MICROBENCHMARK",
        pass: true,
        program: program,
        output_sha256: outputSha256,
        build_git_sha: readCString("EQ_DebugBuildGitSha", 16),
        build_dirty: numberValue("EQ_DebugBuildDirty"),
        kernel_diagnostics: kernelDiagnostics,
        host_elapsed_seconds: Number(hostEndNs - hostStartNs) / 1000000000.0,
        modules: MODULES,
        inputs: INPUTS,
        cases: CASES,
        medium_max_level: MEDIUM_MAX_LEVEL,
        job_count: JOB_COUNT,
        measured_count: MEASURED_COUNT,
        warmup_count: WARMUP_COUNT,
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
        evidence_label: "BOARD_ISOLATED_THREE_MODULE_MICROBENCHMARK_FAILED",
        pass: false,
        error: String(error),
        state: (typeof state == "undefined") ? null : state,
        subjective_observation: "NOT_PERFORMED"
    });
    try { if (debugSession != null) debugSession.target.disconnect(); } catch (ignore1) {}
    throw error;
} finally {
    try { if (debugSession != null) debugSession.terminate(); } catch (ignore2) {}
    try { if (debugServer != null) debugServer.stop(); } catch (ignore3) {}
}
