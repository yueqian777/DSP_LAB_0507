/* C6748 final mode-8 / 240-kbps diagnostic rerun. Run via dss.bat. */
importPackage(Packages.com.ti.debug.engine.scripting);
importPackage(Packages.com.ti.ccstudio.scripting.environment);
importPackage(Packages.java.lang);
importPackage(Packages.java.io);

var root = "C:/Users/zhangyueqian/lab8/DSP_LAB_0507";
var ccxml = root + "/TargetConfig/C6748.ccxml";
var program = root + "/Debug/DSP_LAB_0507.out";
var csvPath = root + "/docs/eval_outputs/final_full_chain_240_rerun.csv";
var logPath = root + "/docs/eval_outputs/dss_final_full_chain.log";
var readyPrefix = root + "/tmp/final_full_chain_repeat";
var cyclesPerMs = 456000.0;
var frameBudgetMs = 20.48;
var noiseDurationMs = 2000;
var speechPreflightMs = 1000;
var steadyDurationMs = 10000;
var preflightMinFrames = 20;
var commitSha = String(System.getenv("DSP_TEST_COMMIT_SHA"));
var audioFile = String(System.getenv("DSP_TEST_AUDIO_FILE"));
var inputGain = String(System.getenv("DSP_TEST_INPUT_GAIN"));
var playbackVolume = String(System.getenv("DSP_TEST_PLAYBACK_VOLUME"));
var recordGain = String(System.getenv("DSP_TEST_RECORD_GAIN"));

var fields = [
    "test_name", "mode", "target_kbps", "repeat_index", "valid_repeat",
    "ad_frames", "da_frames", "algo_frames", "codec_frames",
    "last_cycles", "max_cycles", "last_ms", "max_ms", "cpu_usage_percent", "realtime_pass",
    "payload_bits_per_hop", "scalar_bits_per_hop", "scalar_count_per_hop",
    "estimated_bitrate_kbps", "compression_ratio", "avg_bits_per_scalar",
    "quantizer_clamp_count", "total_scalar_count", "quantizer_clamp_ratio", "invalid_count",
    "band_bits_0", "band_bits_1", "band_bits_2", "band_bits_3", "band_bits_4", "band_bits_5", "band_bits_6", "band_bits_7",
    "learning_hops", "target_learning_hops", "learning_progress", "denoise_ready",
    "input_power_avg", "output_power_avg", "gain_avg", "min_gain", "max_gain", "noise_psd_avg",
    "mcra_speech_probability_avg", "mcra_overdrive_avg", "mcra_floor_avg",
    "noise_phase_speech_probability_avg", "noise_phase_gain_avg", "speech_phase_speech_probability_avg", "speech_phase_gain_avg",
    "input_peak_max", "output_peak_max", "output_input_peak_ratio",
    "input_mean_square_avg", "output_mean_square_avg", "output_input_energy_ratio", "output_input_rms_ratio",
    "input_nonzero_frames", "output_nonzero_frames", "input_clip_frames", "output_clip_frames",
    "audio_source_file", "input_gain", "playback_volume", "record_gain", "audio_capture_status",
    "measurement_status", "commit_sha", "build_config", "notes"
];

var log = new FileWriter(logPath, false);
var csv = null;
var script = null;
var debugServer = null;
var debugSession = null;

function say(message) {
    System.out.println(message);
    log.write((new java.util.Date()).toString() + " " + message + "\n");
    log.flush();
}

function requireEnv(value, name) {
    if ((value == null) || (value == "") || (value == "null")) {
        throw "Missing required environment variable " + name;
    }
}

function csvEscape(value) {
    var text = String(value);
    if ((text.indexOf(",") >= 0) || (text.indexOf("\"") >= 0)) {
        return "\"" + text.replace(/"/g, "\"\"") + "\"";
    }
    return text;
}

function writeRow(row) {
    var i;
    for (i = 0; i < fields.length; i++) {
        if (i > 0) { csv.write(","); }
        csv.write(csvEscape(row[fields[i]]));
    }
    csv.write("\n");
    csv.flush();
}

function evaluate(expr) { return String(debugSession.expression.evaluate(expr)); }

function readExpr(expr, notes) {
    try { return evaluate(expr); }
    catch (e) { notes.push(expr + "=ERR"); return "NOT_MEASURED"; }
}

function number(value, fallback) {
    var parsed = parseFloat(String(value));
    return isNaN(parsed) ? fallback : parsed;
}

function integer(value, fallback) {
    var parsed = parseInt(String(value), 10);
    return isNaN(parsed) ? fallback : parsed;
}

function setExpr(expr, notes) {
    evaluate(expr);
    notes.push("set_" + expr.replace(/\s+/g, ""));
}

function runFor(milliseconds, notes) {
    debugSession.target.runAsynch();
    Thread.sleep(milliseconds);
    debugSession.target.halt();
    notes.push("halted_after_" + milliseconds + "ms");
}

function resetAndLoad(notes) {
    debugSession.target.reset();
    notes.push("target_reset");
    Thread.sleep(500);
    debugSession.memory.loadProgram(program);
    notes.push("loaded_current_program");
}

function flagPath(repeatIndex, suffix) {
    return readyPrefix + repeatIndex + "." + suffix;
}

function deleteFlag(path) {
    var file = new File(path);
    if (file.exists()) { file.delete(); }
}

function signalAudioStart(repeatIndex, notes) {
    var ready = flagPath(repeatIndex, "ready");
    var playing = flagPath(repeatIndex, "playing");
    var writer;
    var waited = 0;

    deleteFlag(ready);
    deleteFlag(playing);
    writer = new FileWriter(ready, false);
    writer.write("mode8_ready\n");
    writer.close();
    notes.push("audio_ready_flag=" + ready);
    while ((!new File(playing).exists()) && (waited < 4000)) {
        Thread.sleep(50);
        waited += 50;
    }
    if (!(new File(playing).exists())) {
        throw "Audio playback acknowledgement was not received for repeat " + repeatIndex;
    }
    notes.push("audio_playing_ack_ms=" + waited);
}

function configureMode8(notes) {
    setExpr("SUBBAND_DebugDemoMode = 0", notes);
    runFor(250, notes);
    setExpr("SUBBAND_DebugRequestedBackend = 0", notes);
    setExpr("SUBBAND_CODEC_LOOP_DebugRequestedTargetKbps = 240", notes);
    setExpr("SUBBAND_DebugDemoMode = 8", notes);
}

function scaled(expr, divisor, notes) {
    return (number(readExpr(expr, notes), 0.0) / divisor).toFixed(6);
}

function phaseSnapshot(notes) {
    return {
        speech: scaled("SUBBAND_DENOISE_DebugMcraSpeechProbAvgX1000000", 1000000.0, notes),
        gain: scaled("SUBBAND_DENOISE_DebugGainAvgX1000000", 1000000.0, notes)
    };
}

function preflight(notes) {
    var ad = integer(readExpr("SUBBAND_DebugAdFrames", notes), 0);
    var algo = integer(readExpr("SUBBAND_DebugAlgoFrames", notes), 0);
    if ((ad >= preflightMinFrames) && (algo >= preflightMinFrames)) {
        notes.push("preflight_pass_ad=" + ad + "_algo=" + algo);
        return true;
    }
    notes.push("preflight_fail_ad=" + ad + "_algo=" + algo);
    return false;
}

function getRow(repeatIndex, noisePhase, speechPhase, notes) {
    var row = {};
    var payload;
    var scalarBits;
    var scalarCount;
    var clampCount;
    var totalScalars;
    var inputPower;
    var outputPower;
    var inputMs;
    var outputMs;
    var maxCycles;
    var maxMs;

    row.test_name = "final_full_chain_240";
    row.mode = 8;
    row.target_kbps = 240;
    row.repeat_index = repeatIndex;
    row.ad_frames = readExpr("SUBBAND_DebugAdFrames", notes);
    row.da_frames = readExpr("SUBBAND_DebugDaFrames", notes);
    row.algo_frames = readExpr("SUBBAND_DebugAlgoFrames", notes);
    row.codec_frames = readExpr("SUBBAND_CODEC_LOOP_DebugFrames", notes);
    row.last_cycles = readExpr("SUBBAND_DebugLastCycles", notes);
    row.max_cycles = readExpr("SUBBAND_DebugMaxCycles", notes);
    maxCycles = number(row.max_cycles, -1.0);
    row.last_ms = (number(row.last_cycles, -1.0) / cyclesPerMs).toFixed(6);
    row.max_ms = (maxCycles / cyclesPerMs).toFixed(6);
    maxMs = maxCycles / cyclesPerMs;
    row.cpu_usage_percent = ((maxMs / frameBudgetMs) * 100.0).toFixed(6);
    row.realtime_pass = (maxMs < frameBudgetMs) ? 1 : 0;

    payload = integer(readExpr("SUBBAND_CODEC_LOOP_DebugPayloadBitsPerHop", notes), 0);
    scalarBits = integer(readExpr("SUBBAND_CODEC_LOOP_DebugScalarBitsPerHop", notes), 0);
    scalarCount = integer(readExpr("SUBBAND_CODEC_LOOP_DebugScalarCountPerHop", notes), 0);
    clampCount = integer(readExpr("SUBBAND_CODEC_LOOP_DebugQuantizerClampCount", notes), 0);
    totalScalars = integer(readExpr("SUBBAND_CODEC_LOOP_DebugTotalScalarCount", notes), 0);
    row.payload_bits_per_hop = payload;
    row.scalar_bits_per_hop = scalarBits;
    row.scalar_count_per_hop = scalarCount;
    row.estimated_bitrate_kbps = ((payload * 50000.0 / 256.0) / 1000.0).toFixed(6);
    row.compression_ratio = (800.0 / number(row.estimated_bitrate_kbps, 0.0)).toFixed(6);
    row.avg_bits_per_scalar = (scalarBits / scalarCount).toFixed(6);
    row.quantizer_clamp_count = clampCount;
    row.total_scalar_count = totalScalars;
    row.quantizer_clamp_ratio = (clampCount / totalScalars).toFixed(6);
    row.invalid_count = readExpr("SUBBAND_CODEC_LOOP_DebugInvalidCount", notes);
    for (var band = 0; band < 8; band++) {
        row["band_bits_" + band] = readExpr("SUBBAND_CODEC_LOOP_DebugBandBits" + band, notes);
    }

    row.learning_hops = readExpr("SUBBAND_DENOISE_DebugLearnHops", notes);
    row.target_learning_hops = readExpr("SUBBAND_DENOISE_DebugTargetHops", notes);
    row.learning_progress = scaled("SUBBAND_DENOISE_DebugLearnProgressX1000000", 1000000.0, notes);
    row.denoise_ready = readExpr("SUBBAND_DENOISE_DebugReady", notes);
    row.input_power_avg = scaled("SUBBAND_DENOISE_DebugInputPowerAvgDiv16", 1.0 / 16.0, notes);
    row.output_power_avg = scaled("SUBBAND_DENOISE_DebugOutputPowerAvgDiv16", 1.0 / 16.0, notes);
    row.gain_avg = scaled("SUBBAND_DENOISE_DebugGainAvgX1000000", 1000000.0, notes);
    row.min_gain = scaled("SUBBAND_DENOISE_DebugMinGainX1000000", 1000000.0, notes);
    row.max_gain = scaled("SUBBAND_DENOISE_DebugMaxGainX1000000", 1000000.0, notes);
    row.noise_psd_avg = scaled("SUBBAND_DENOISE_DebugNoisePsdAvgDiv16", 1.0 / 16.0, notes);
    row.mcra_speech_probability_avg = scaled("SUBBAND_DENOISE_DebugMcraSpeechProbAvgX1000000", 1000000.0, notes);
    row.mcra_overdrive_avg = scaled("SUBBAND_DENOISE_DebugMcraOverdriveAvgX1000000", 1000000.0, notes);
    row.mcra_floor_avg = scaled("SUBBAND_DENOISE_DebugMcraFloorAvgX1000000", 1000000.0, notes);
    row.noise_phase_speech_probability_avg = noisePhase.speech;
    row.noise_phase_gain_avg = noisePhase.gain;
    row.speech_phase_speech_probability_avg = speechPhase.speech;
    row.speech_phase_gain_avg = speechPhase.gain;
    row.input_peak_max = readExpr("SUBBAND_DebugInputPeakMax", notes);
    row.output_peak_max = readExpr("SUBBAND_DebugOutputPeakMax", notes);
    row.output_input_peak_ratio = (number(row.output_peak_max, 0.0) / number(row.input_peak_max, 0.0)).toFixed(6);
    row.input_mean_square_avg = readExpr("SUBBAND_DebugInputMeanSquareAvg", notes);
    row.output_mean_square_avg = readExpr("SUBBAND_DebugOutputMeanSquareAvg", notes);
    inputMs = number(row.input_mean_square_avg, 0.0);
    outputMs = number(row.output_mean_square_avg, 0.0);
    row.output_input_energy_ratio = (outputMs / inputMs).toFixed(6);
    row.output_input_rms_ratio = Math.sqrt(outputMs / inputMs).toFixed(6);
    row.input_nonzero_frames = readExpr("SUBBAND_DebugInputNonzeroFrames", notes);
    row.output_nonzero_frames = readExpr("SUBBAND_DebugOutputNonzeroFrames", notes);
    row.input_clip_frames = readExpr("SUBBAND_DebugInputClipFrames", notes);
    row.output_clip_frames = readExpr("SUBBAND_DebugOutputClipFrames", notes);
    row.audio_source_file = audioFile;
    row.input_gain = inputGain;
    row.playback_volume = playbackVolume;
    row.record_gain = recordGain;
    row.audio_capture_status = "NOT_MEASURED_AUDIO_CAPTURE_UNAVAILABLE";
    row.valid_repeat = ((integer(row.ad_frames, 0) >= 470) &&
                        (integer(row.da_frames, 0) >= 470) &&
                        (integer(row.algo_frames, 0) >= 470) &&
                        (integer(row.input_nonzero_frames, 0) > 0) &&
                        (integer(row.learning_hops, 0) == integer(row.target_learning_hops, -1)) &&
                        (number(row.learning_progress, 0.0) >= 0.9999) &&
                        (integer(row.denoise_ready, 0) == 1) &&
                        (integer(row.codec_frames, 0) > 0) &&
                        (number(row.estimated_bitrate_kbps, 0.0) > 200.0) &&
                        (number(row.estimated_bitrate_kbps, 0.0) < 280.0) &&
                        (integer(row.invalid_count, -1) == 0) &&
                        (maxMs < frameBudgetMs)) ? 1 : 0;
    row.measurement_status = (row.valid_repeat == 1) ? "VALID" : "INVALID_REQUIREMENTS_NOT_MET";
    row.commit_sha = commitSha;
    row.build_config = "C6748;Fs=50k;Frame=1024;CH1;O3;Debug";
    row.notes = notes.join("|");
    return row;
}

function runRepeat(repeatIndex, allowRetry) {
    var notes = [];
    var noisePhase;
    var speechPhase;
    var row;

    say("BEGIN final_full_chain_240 repeat=" + repeatIndex);
    configureMode8(notes);
    signalAudioStart(repeatIndex, notes);
    runFor(noiseDurationMs, notes);
    noisePhase = phaseSnapshot(notes);
    runFor(speechPreflightMs, notes);
    speechPhase = phaseSnapshot(notes);
    if (!preflight(notes)) {
        if (allowRetry) {
            say("PRECHECK FAILED; reset/load retry once");
            resetAndLoad(notes);
            return runRepeat(repeatIndex, false);
        }
        throw "ADC/algo preflight failed twice; stop remaining tests";
    }
    setExpr("SUBBAND_DebugBenchmarkResetRequest = 1", notes);
    runFor(steadyDurationMs, notes);
    row = getRow(repeatIndex, noisePhase, speechPhase, notes);
    writeRow(row);
    say("END repeat=" + repeatIndex + " valid=" + row.valid_repeat +
        " max_ms=" + row.max_ms + " bitrate=" + row.estimated_bitrate_kbps);
    if (row.valid_repeat != 1) {
        throw "repeat " + repeatIndex + " did not meet validity criteria";
    }
}

try {
    requireEnv(commitSha, "DSP_TEST_COMMIT_SHA");
    requireEnv(audioFile, "DSP_TEST_AUDIO_FILE");
    csv = new FileWriter(csvPath, false);
    csv.write(fields.join(",") + "\n");
    csv.flush();
    script = ScriptingEnvironment.instance();
    debugServer = script.getServer("DebugServer.1");
    debugServer.setConfig(ccxml);
    debugSession = debugServer.openSession(".*C674X_0");
    debugSession.target.connect();
    say("connected C674X_0 commit=" + commitSha);
    var startupNotes = [];
    resetAndLoad(startupNotes);
    runFor(1000, startupNotes);
    for (var repeat = 1; repeat <= 3; repeat++) {
        runRepeat(repeat, true);
    }
    debugSession.target.disconnect();
    say("disconnected C674X_0");
} catch (e) {
    say("DSS_ERROR=" + e);
    throw e;
} finally {
    try { if (debugSession != null) { debugSession.terminate(); } } catch (ignore1) {}
    try { if (debugServer != null) { debugServer.stop(); } } catch (ignore2) {}
    try { if (csv != null) { csv.close(); } } catch (ignore3) {}
    log.close();
}
