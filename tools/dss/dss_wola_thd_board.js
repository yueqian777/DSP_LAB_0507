/* C6748 WOLA THD capture using JTAG memory transfer only; no PC soundcard. */
importPackage(Packages.com.ti.debug.engine.scripting);
importPackage(Packages.com.ti.ccstudio.scripting.environment);
importPackage(Packages.java.lang);
importPackage(Packages.java.io);

var root = env("DSP_TEST_ROOT", String(new File(".").getCanonicalPath()).replace(/\\/g, "/"));
var ccxml = env("DSP_TEST_CCXML", root + "/TargetConfig/C6748.ccxml");
var program = env("DSP_TEST_PROGRAM", root + "/Debug/DSP_LAB_0507.out");
var inputCaptureRaw = env("DSP_TEST_CAPTURE_INPUT_RAW", "");
var outputRaw = env("DSP_TEST_OUTPUT_RAW", "");
var summaryPath = env("DSP_TEST_SUMMARY", "");
var expectedSha = env("DSP_TEST_EXPECTED_SHA", "UNKNOWN");
var frequencyHz = env("DSP_TEST_FREQUENCY_HZ", "UNKNOWN");
var processedSamples = 500736;
var expectedFrames = 489;
var script = null;
var debugServer = null;
var debugSession = null;
var completed = false;

function env(name, fallback) {
    var value = String(System.getenv(name));
    return value == "null" || value == "" ? fallback : value;
}

function quote(value) {
    return "\"" + String(value).replace(/\\/g, "\\\\")
        .replace(/\"/g, "\\\"").replace(/\r/g, "\\r")
        .replace(/\n/g, "\\n") + "\"";
}

function writeSummary(status, detail) {
    var writer = new FileWriter(summaryPath, false);
    writer.write("{" +
        "\"measurement_status\":" + quote(status) + "," +
        "\"frequency_hz\":" + quote(frequencyHz) + "," +
        "\"commit_sha\":" + quote(expectedSha) + "," +
        "\"uses_pc_soundcard\":false," +
        "\"transport\":\"JTAG_DSS_MEMORY_SAVE\"," +
        "\"frames\":" + numberValue("SUBBAND_THD_DebugFrames") + "," +
        "\"cycle_count\":" + numberValue("SUBBAND_THD_DebugCycleCount") + "," +
        "\"max_frame_cycles\":" + numberValue("SUBBAND_THD_DebugMaxFrameCycles") + "," +
        "\"detail\":" + quote(detail) + "}\n");
    writer.close();
}

function evaluate(expression) {
    return String(debugSession.expression.evaluate(expression));
}

function numberValue(expression) {
    var text = evaluate(expression);
    var hex = text.match(/-?0x[0-9a-fA-F]+/);
    var value = hex ? parseInt(hex[0], 16) : parseFloat(text);
    if (!isFinite(value)) throw "Unreadable value " + expression + "=" + text;
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

function requireCondition(condition, message) {
    if (!condition) throw message;
}

function runFor(milliseconds) {
    debugSession.target.runAsynch();
    Thread.sleep(milliseconds);
    debugSession.target.halt();
}

function addressOf(symbol) {
    return debugSession.symbol.getAddress(symbol);
}

try {
    requireCondition(new File(ccxml).exists(), "missing CCXML");
    requireCondition(new File(program).exists(), "missing program");
    requireCondition(inputCaptureRaw != "", "missing input capture path");
    requireCondition(summaryPath != "", "missing summary path");
    script = ScriptingEnvironment.instance();
    script.setScriptTimeout(10 * 60 * 1000);
    script.traceSetConsoleLevel(TraceLevel.INFO);
    debugServer = script.getServer("DebugServer.1");
    debugServer.setConfig(ccxml);
    debugSession = debugServer.openSession(".*C674X_0");
    debugSession.target.connect();
    debugSession.target.reset();
    debugSession.memory.loadProgram(program);

    runFor(2000);
    var initialStatus = numberValue("SUBBAND_THD_DebugStatus");
    var initialDirty = numberValue("SUBBAND_THD_DebugBuildDirty");
    var initialSha = readCString("SUBBAND_THD_DebugBuildGitSha", 16);
    requireCondition(initialStatus == 1,
                     "firmware did not enter JTAG trigger wait state; status=" +
                     initialStatus + ";dirty=" + initialDirty + ";sha=" +
                     initialSha);
    requireCondition(initialDirty == 0,
                     "firmware build identity is dirty");
    requireCondition(initialSha == expectedSha,
                     "firmware SHA does not match " + expectedSha);

    evaluate("SUBBAND_THD_DebugFrequencyHz = " + parseInt(frequencyHz, 10));
    evaluate("SUBBAND_THD_DebugRequest = 1");

    var waited = 0;
    while (waited < 30000 && numberValue("SUBBAND_THD_DebugStatus") != 3) {
        runFor(5000);
        waited += 5000;
    }
    requireCondition(numberValue("SUBBAND_THD_DebugStatus") == 3,
                     "board WOLA processing did not complete");
    requireCondition(numberValue("SUBBAND_THD_DebugFrames") == expectedFrames,
                     "board WOLA frame count mismatch");

    debugSession.memory.saveRaw(Memory.Page.PROGRAM,
                                addressOf("SUBBAND_THD_InputPacked"), inputCaptureRaw,
                                processedSamples / 2, 32, false);
    debugSession.memory.saveRaw(Memory.Page.PROGRAM,
                                addressOf("SUBBAND_THD_OutputPacked"), outputRaw,
                                processedSamples / 2, 32, false);
    writeSummary("MEASURED_BOARD_DIGITAL_NO_ADC_DAC_ANALOG",
                 "C6748-generated PCM -> C6748 WOLA -> DDR PCM; JTAG capture; no PC soundcard");
    completed = true;
} catch (error) {
    try {
        if (debugSession != null && summaryPath != "") {
            writeSummary("FAIL", String(error));
        }
    } catch (summaryError) {}
    throw error;
} finally {
    try {
        if (debugSession != null) debugSession.target.disconnect();
    } catch (ignore1) {}
    try {
        if (debugSession != null) debugSession.terminate();
    } catch (ignore2) {}
    try {
        if (debugServer != null) debugServer.stop();
    } catch (ignore3) {}
}
