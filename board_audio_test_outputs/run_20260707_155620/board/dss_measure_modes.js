importPackage(Packages.com.ti.debug.engine.scripting);
importPackage(Packages.com.ti.ccstudio.scripting.environment);
importPackage(Packages.java.lang);
importPackage(Packages.java.io);

var outDir = "C:/Users/zhangyueqian/lab8/DSP_LAB_0507/board_audio_test_outputs/run_20260707_155620/board";
var log = new FileWriter(outDir + "/dss_measure_modes.log", false);
var csv = new FileWriter(outDir + "/board_mode_watch_metrics.csv", false);

function writeLog(msg) {
    log.write((new java.util.Date()).toString() + " " + msg + "\n");
    log.flush();
}

function csvEscape(value) {
    var s = String(value);
    return "\"" + s.replace(/\"/g, "\"\"") + "\"";
}

function evalExpr(expr) {
    try {
        return String(debugSession.expression.evaluate(expr));
    } catch (e) {
        return "ERR:" + e;
    }
}

function metricRow(mode, modeName, expr) {
    var value = evalExpr(expr);
    csv.write(mode + "," + csvEscape(modeName) + "," + csvEscape(expr) + "," + csvEscape(value) + "\n");
    csv.flush();
    writeLog("MODE " + mode + " " + modeName + " METRIC " + expr + "=" + value);
}

var modes = [
    [0, "raw_bypass"],
    [1, "wola_only"],
    [2, "fixed_denoise"],
    [3, "mild_fixed_denoise"],
    [4, "hybrid_ms"],
    [5, "mild_hybrid_ms"],
    [6, "mcra_normal"],
    [7, "mcra_strong"]
];

var metrics = [
    "SUBBAND_DebugDemoMode",
    "SUBBAND_DebugAppliedDemoMode",
    "SUBBAND_DebugDemoModeChanges",
    "SUBBAND_DebugAdFrames",
    "SUBBAND_DebugDaFrames",
    "SUBBAND_EVAL_DebugAdFrames",
    "SUBBAND_EVAL_DebugDaFrames",
    "SUBBAND_EVAL_DebugWolaFrames",
    "SUBBAND_EVAL_DebugDenoiseFrames",
    "SUBBAND_EVAL_DebugFrameBudgetMs",
    "SUBBAND_EVAL_DebugDenoiseLastMs",
    "SUBBAND_EVAL_DebugDenoiseMaxMs",
    "SUBBAND_EVAL_DebugCpuUsagePercent",
    "SUBBAND_WOLA_DebugFrames",
    "SUBBAND_WOLA_DebugInputEnergy",
    "SUBBAND_WOLA_DebugOutputEnergy",
    "SUBBAND_WOLA_DebugEnergyRatio",
    "SUBBAND_WOLA_DebugBypass",
    "SUBBAND_WOLA_DebugLastCycles",
    "SUBBAND_WOLA_DebugMaxCycles",
    "SUBBAND_WOLA_DebugLastMs",
    "SUBBAND_WOLA_DebugMaxMs",
    "SUBBAND_WOLA_DebugLastMs * 1000.0f",
    "SUBBAND_WOLA_DebugMaxMs * 1000.0f",
    "SUBBAND_DENOISE_DebugEnabled",
    "SUBBAND_DENOISE_DebugLearning",
    "SUBBAND_DENOISE_DebugReady",
    "SUBBAND_DENOISE_DebugLearnHops",
    "SUBBAND_DENOISE_DebugTargetHops",
    "SUBBAND_DENOISE_DebugLearnProgress",
    "SUBBAND_DENOISE_DebugInputPowerAvg",
    "SUBBAND_DENOISE_DebugOutputPowerAvg",
    "SUBBAND_DENOISE_DebugGainAvg",
    "SUBBAND_DENOISE_DebugMinGain",
    "SUBBAND_DENOISE_DebugMaxGain",
    "SUBBAND_DENOISE_DebugNoisePsdAvg",
    "SUBBAND_DENOISE_DebugFade",
    "SUBBAND_DENOISE_DebugNoiseTrackMode",
    "SUBBAND_DENOISE_DebugMsUpdateCount",
    "SUBBAND_DENOISE_DebugMsNoisePsdAvg",
    "SUBBAND_DENOISE_DebugMsMinPsdAvg",
    "SUBBAND_DENOISE_DebugMsBias",
    "SUBBAND_DENOISE_DebugMsWindowSeconds",
    "SUBBAND_DENOISE_DebugMcraSpeechProbAvg",
    "SUBBAND_DENOISE_DebugMcraOverdriveAvg",
    "SUBBAND_DENOISE_DebugMcraDeltaLow",
    "SUBBAND_DENOISE_DebugMcraDeltaHigh",
    "SUBBAND_DENOISE_DebugMcraAlphaNoise",
    "SUBBAND_DENOISE_DebugMcraAlphaSpeech",
    "SUBBAND_DENOISE_DebugMcraFloorAvg",
    "SUBBAND_DENOISE_DebugMcraStrongMode"
];

var script = ScriptingEnvironment.instance();
var ccxml = "C:/Users/zhangyueqian/lab8/DSP_LAB_0507/TargetConfig/C6748.ccxml";
var program = "C:/Users/zhangyueqian/lab8/DSP_LAB_0507/Debug/DSP_LAB_0507.out";
var debugServer = null;
var debugSession = null;

csv.write("mode,mode_name,metric,value\n");
csv.flush();

try {
    writeLog("measure all modes start");
    debugServer = script.getServer("DebugServer.1");
    debugServer.setConfig(ccxml);
    debugSession = debugServer.openSession(".*C674X_0");
    debugSession.target.connect();
    writeLog("connected target");

    for (var i = 0; i < modes.length; i++) {
        var mode = modes[i][0];
        var modeName = modes[i][1];
        writeLog("mode " + mode + " " + modeName + " start");
        debugSession.memory.loadProgram(program);
        writeLog("loaded program " + program);
        debugSession.expression.evaluate("SUBBAND_DebugDemoMode = " + mode);
        writeLog("set SUBBAND_DebugDemoMode=" + mode);
        debugSession.target.runAsynch();
        Thread.sleep(10000);
        debugSession.target.halt();
        writeLog("mode " + mode + " halted after 10000 ms");

        for (var j = 0; j < metrics.length; j++) {
            metricRow(mode, modeName, metrics[j]);
        }
    }

    debugSession.target.disconnect();
    writeLog("disconnected target");
} catch (e) {
    writeLog("ERROR=" + e);
    throw e;
} finally {
    try {
        if (debugSession != null) {
            debugSession.terminate();
            writeLog("terminated session");
        }
    } catch (sessionError) {
        writeLog("SESSION_TERM_WARN=" + sessionError);
    }
    try {
        if (debugServer != null) {
            debugServer.stop();
            writeLog("stopped server");
        }
    } catch (serverError) {
        writeLog("SERVER_STOP_WARN=" + serverError);
    }
    csv.close();
    log.close();
}
