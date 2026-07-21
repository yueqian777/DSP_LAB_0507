/* Project 3.3 internal-digital response, THD, and reference-SNR capture. */
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
var expectedShortSha = env("DSP_TEST_EXPECTED_SHORT_SHA", "UNKNOWN");
var expectedFullSha = env("DSP_TEST_EXPECTED_FULL_SHA", "UNKNOWN");
var outputSha256 = env("DSP_TEST_OUTPUT_SHA256", "UNKNOWN");
var summaryPath = resultDir + "/dss_summary.json";
var script = null;
var debugServer = null;
var debugSession = null;

var CASE_COUNT = 6;
var THD_FREQUENCY_COUNT = 6;
var SNR_SIGNAL_COUNT = 3;
var CAPTURE_SAMPLES = 8192;
var PREROLL_SAMPLES = 4096;
var PACKED_WORDS = CAPTURE_SAMPLES / 2;
var REFERENCE_PACKED_WORDS = (CAPTURE_SAMPLES + PREROLL_SAMPLES) / 2;
var EXPECTED_CASES = CASE_COUNT *
    (1 + THD_FREQUENCY_COUNT + SNR_SIGNAL_COUNT);

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
    if (!isFinite(value)) throw "Unreadable " + expression + "=" + text;
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

function sum(values) {
    var total = 0, index;
    for (index = 0; index < values.length; index++) total += values[index];
    return total;
}

function requireCondition(condition, message) {
    if (!condition) throw message;
}

function runFor(milliseconds) {
    debugSession.target.runAsynch();
    Thread.sleep(milliseconds);
    debugSession.target.halt();
}

function saveRaw(symbol, path, words) {
    debugSession.memory.saveRaw(Memory.Page.PROGRAM,
        debugSession.symbol.getAddress(symbol), path, words, 32, false);
}

try {
    var status, waited, hostStartNs, hostEndNs;
    var responseClip, thdClip, snrClip, maxFrameCycles;
    var result;

    requireCondition(new File(ccxml).exists(), "Missing CCXML: " + ccxml);
    requireCondition(new File(program).exists(), "Missing program: " + program);
    requireCondition(new File(resultDir).isDirectory(),
                     "Missing result directory: " + resultDir);

    script = ScriptingEnvironment.instance();
    script.setScriptTimeout(10 * 60 * 1000);
    script.traceSetConsoleLevel(TraceLevel.INFO);
    debugServer = script.getServer("DebugServer.1");
    debugServer.setConfig(ccxml);
    debugSession = debugServer.openSession(".*C674X_0");
    debugSession.target.connect();
    debugSession.target.reset();
    Thread.sleep(500);
    debugSession.memory.loadProgram(program);
    Thread.sleep(500);

    hostStartNs = System.nanoTime();
    waited = 0;
    status = 0;
    while (waited < 120000) {
        runFor(1000);
        waited += 1000;
        status = numberValue("EQ_DebugFinalMetricsStatus");
        if (status == 2 || status == 3) break;
    }
    hostEndNs = System.nanoTime();

    requireCondition(numberValue("EQ_DebugFinalMetricsCompiled") == 1,
                     "Final metrics harness was not compiled");
    requireCondition(readCString("EQ_DebugFinalMetricsBuildGitSha", 16) ==
                     expectedShortSha, "Firmware short SHA mismatch");
    requireCondition(numberValue("EQ_DebugFinalMetricsBuildDirty") == 0,
                     "Firmware build identity is dirty");
    requireCondition(status == 2,
                     "Metrics harness did not reach READY; status=" + status);
    requireCondition(numberValue("EQ_DebugFinalMetricsCompletedCases") ==
                     EXPECTED_CASES, "Completed-case count mismatch");
    requireCondition(numberValue("EQ_DebugFinalMetricsErrorCount") == 0,
                     "Metrics harness error counter is nonzero");

    responseClip = readArray("EQ_FinalMetricsResponseClipCount", CASE_COUNT);
    thdClip = readArray("EQ_FinalMetricsThdClipCount",
                        CASE_COUNT * THD_FREQUENCY_COUNT);
    snrClip = readArray("EQ_FinalMetricsSnrClipCount",
                        CASE_COUNT * SNR_SIGNAL_COUNT);
    requireCondition(sum(responseClip) == 0 && sum(thdClip) == 0 &&
                     sum(snrClip) == 0,
                     "Normal metrics case reported clipping");

    saveRaw("EQ_FinalMetricsResponsePacked",
            resultDir + "/response_packed.raw",
            CASE_COUNT * PACKED_WORDS);
    saveRaw("EQ_FinalMetricsThdInputPacked",
            resultDir + "/thd_input_packed.raw",
            THD_FREQUENCY_COUNT * PACKED_WORDS);
    saveRaw("EQ_FinalMetricsThdOutputPacked",
            resultDir + "/thd_output_packed.raw",
            CASE_COUNT * THD_FREQUENCY_COUNT * PACKED_WORDS);
    saveRaw("EQ_FinalMetricsSnrInputPacked",
            resultDir + "/snr_input_packed.raw",
            SNR_SIGNAL_COUNT * REFERENCE_PACKED_WORDS);
    saveRaw("EQ_FinalMetricsSnrOutputPacked",
            resultDir + "/snr_output_packed.raw",
            CASE_COUNT * SNR_SIGNAL_COUNT * PACKED_WORDS);

    maxFrameCycles = numberValue("EQ_DebugFinalMetricsMaxFrameCycles");
    result = {
        pass: true,
        evidence_class: "BOARD_INTERNAL_DIGITAL",
        measurement_name: "PROJECT33_FINAL_STATIC_EQ_METRICS",
        commit_sha: expectedFullSha,
        build_git_sha: readCString("EQ_DebugFinalMetricsBuildGitSha", 16),
        build_dirty: numberValue("EQ_DebugFinalMetricsBuildDirty"),
        output_sha256: outputSha256,
        transport: "JTAG_DSS_MEMORY_SAVE",
        includes_adc: false,
        includes_dac_analog: false,
        sample_rate_hz: 50000,
        frame_len: 1024,
        capture_samples: CAPTURE_SAMPLES,
        preroll_samples: PREROLL_SAMPLES,
        completed_cases: EXPECTED_CASES,
        max_frame_cycles: maxFrameCycles,
        frame_budget_cycles_at_300mhz: 6144000,
        max_frame_budget_ratio: maxFrameCycles / 6144000.0,
        response_clip_count: responseClip,
        thd_clip_count: thdClip,
        snr_clip_count: snrClip,
        host_elapsed_seconds: Number(hostEndNs - hostStartNs) / 1000000000.0,
        raw_files: ["response_packed.raw", "thd_input_packed.raw",
                    "thd_output_packed.raw", "snr_input_packed.raw",
                    "snr_output_packed.raw"]
    };
    saveJson(summaryPath, result);
} catch (error) {
    try {
        saveJson(summaryPath, {pass: false, evidence_class: "BOARD_INTERNAL_DIGITAL",
                 commit_sha: expectedFullSha, output_sha256: outputSha256,
                 error: String(error)});
    } catch (summaryError) {}
    throw error;
} finally {
    try { if (debugSession != null) debugSession.target.halt(); } catch (ignore0) {}
    try { if (debugSession != null) debugSession.target.disconnect(); } catch (ignore1) {}
    try { if (debugSession != null) debugSession.terminate(); } catch (ignore2) {}
    try { if (debugServer != null) debugServer.stop(); } catch (ignore3) {}
}
