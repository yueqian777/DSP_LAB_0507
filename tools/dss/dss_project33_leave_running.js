/* Load Project 3.3, verify boot/safety once, then leave the DSP running. */
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
var resultPath = env("DSP_TEST_RESULT_PATH", "project33_leave_running.json");
var script = null;
var debugServer = null;
var debugSession = null;

function quote(value) {
    return "\"" + String(value).replace(/\\/g, "\\\\")
        .replace(/\"/g, "\\\"").replace(/\r/g, "\\r")
        .replace(/\n/g, "\\n") + "\"";
}

function save(value) {
    var writer = new FileWriter(resultPath, false);
    var parts = [], key;
    for (key in value) {
        if (typeof value[key] == "number" || typeof value[key] == "boolean") {
            parts.push(quote(key) + ":" + String(value[key]));
        } else {
            parts.push(quote(key) + ":" + quote(value[key]));
        }
    }
    writer.write("{" + parts.join(",") + "}\n");
    writer.close();
}

function numberValue(expression) {
    var text = String(debugSession.expression.evaluate(expression));
    var hex = text.match(/-?0x[0-9a-fA-F]+/);
    var value = hex ? parseInt(hex[0], 16) : parseFloat(text);
    if (!isFinite(value)) throw "Unreadable " + expression + "=" + text;
    return value;
}

try {
    var initStage, deadline, latency, overlap, dropped, clip;
    script = ScriptingEnvironment.instance();
    debugServer = script.getServer("DebugServer.1");
    debugServer.setConfig(ccxml);
    debugSession = debugServer.openSession(".*C674X_0");
    debugSession.target.connect();
    debugSession.target.reset();
    Thread.sleep(500);
    debugSession.memory.loadProgram(program);
    Thread.sleep(500);
    debugSession.target.runAsynch();
    Thread.sleep(6000);
    debugSession.target.halt();
    initStage = numberValue("EQ_DebugInitStage");
    deadline = numberValue("EQ_DebugDeadlineMissCount");
    latency = numberValue("EQ_DebugFrameLatencyDeadlineMissCount");
    overlap = numberValue("EQ_DebugFrameServiceOverlapCount");
    dropped = numberValue("EQ_DebugFrameServiceDroppedCount");
    clip = numberValue("EQ_DebugClipCount");
    if (initStage != 11 || deadline != 0 || latency != 0 ||
        overlap != 0 || dropped != 0 || clip != 0) {
        throw "Project 3.3 boot/safety verification failed";
    }
    save({ pass: true, init_stage: initStage, deadline: deadline,
           latency_miss: latency, overlap: overlap, dropped: dropped,
           clip: clip, final_target_state: "RUNNING_DISCONNECTED",
           subjective_observation: "NOT_PERFORMED" });
    debugSession.target.runAsynch();
    Thread.sleep(250);
    debugSession.target.disconnect();
} catch (error) {
    try { if (debugSession != null) debugSession.target.halt(); } catch (ignore0) {}
    save({ pass: false, error: String(error),
           final_target_state: "HALTED_ON_FAILURE",
           subjective_observation: "NOT_PERFORMED" });
    try { if (debugSession != null) debugSession.target.disconnect(); } catch (ignore1) {}
    throw error;
} finally {
    try { if (debugSession != null) debugSession.terminate(); } catch (ignore2) {}
    try { if (debugServer != null) debugServer.stop(); } catch (ignore3) {}
}
