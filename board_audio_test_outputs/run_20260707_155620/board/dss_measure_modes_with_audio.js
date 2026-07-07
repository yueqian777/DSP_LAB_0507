importPackage(Packages.com.ti.debug.engine.scripting);
importPackage(Packages.com.ti.ccstudio.scripting.environment);
importPackage(Packages.java.lang);
importPackage(Packages.java.io);

var outDir = "C:/Users/zhangyueqian/lab8/DSP_LAB_0507/board_audio_test_outputs/run_20260707_155620/board";
var log = new FileWriter(outDir + "/dss_measure_modes_with_audio.log", false);
var csv = new FileWriter(outDir + "/board_mode_watch_metrics_with_audio.csv", false);
var audioPath = "D:\\NDM\\dsp_audio_testset_50k_10s_v1\\01_clean_music_melody\\noisy_input.wav";

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

function playAudioSync() {
    var command = "(New-Object System.Media.SoundPlayer '" + audioPath + "').PlaySync()";
    var proc = java.lang.Runtime.getRuntime().exec([
        "powershell.exe",
        "-NoProfile",
        "-ExecutionPolicy",
        "Bypass",
        "-Command",
        command
    ]);
    var exitCode = proc.waitFor();
    writeLog("audio playback exit=" + exitCode);
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
    "SUBBAND_DebugAppliedDemoMode",
    "SUBBAND_EVAL_DebugAdFrames",
    "SUBBAND_EVAL_DebugDaFrames",
    "SUBBAND_EVAL_DebugFrameBudgetMs",
    "SUBBAND_WOLA_DebugLastMs * 1000.0f",
    "SUBBAND_WOLA_DebugMaxMs * 1000.0f",
    "SUBBAND_EVAL_DebugCpuUsagePercent",
    "SUBBAND_DENOISE_DebugReady",
    "SUBBAND_DENOISE_DebugLearnProgress",
    "SUBBAND_DENOISE_DebugNoiseTrackMode",
    "SUBBAND_DENOISE_DebugMcraStrongMode",
    "SUBBAND_DENOISE_DebugInputPowerAvg",
    "SUBBAND_DENOISE_DebugOutputPowerAvg",
    "SUBBAND_DENOISE_DebugNoisePsdAvg"
];

var script = ScriptingEnvironment.instance();
var ccxml = "C:/Users/zhangyueqian/lab8/DSP_LAB_0507/TargetConfig/C6748.ccxml";
var program = "C:/Users/zhangyueqian/lab8/DSP_LAB_0507/Debug/DSP_LAB_0507.out";
var debugServer = null;
var debugSession = null;

csv.write("mode,mode_name,metric,value\n");
csv.flush();

try {
    writeLog("measure all modes with audio start audio=" + audioPath);
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
        debugSession.expression.evaluate("SUBBAND_DebugDemoMode = " + mode);
        debugSession.target.runAsynch();
        playAudioSync();
        debugSession.target.halt();
        writeLog("mode " + mode + " halted after audio playback");

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
