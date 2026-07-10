/* C6748 Project 3.2 board rerun matrix. Run via dss.bat. */
importPackage(Packages.com.ti.debug.engine.scripting);
importPackage(Packages.com.ti.ccstudio.scripting.environment);
importPackage(Packages.java.lang);
importPackage(Packages.java.io);

var root = "C:/Users/zhangyueqian/lab8/DSP_LAB_0507";
var ccxml = root + "/TargetConfig/C6748.ccxml";
var program = root + "/Debug/DSP_LAB_0507.out";
var csvPath = root + "/docs/eval_outputs/board_missing_tests_rerun.csv";
var logPath = root + "/docs/eval_outputs/dss_board_missing_tests_rerun.log";
var cyclesPerMs = 456000.0;
var frameBudgetMs = 20.48;
var preflightDurationMs = 1000;
var steadyDurationMs = 10000;
var modeSwitchMs = 300;
var fullChainNoiseOnlyMs = 2000;
var preflightMinFrames = 20;
var commitSha = String(System.getenv("DSP_TEST_COMMIT_SHA"));
var audioFile = String(System.getenv("DSP_TEST_AUDIO_FILE"));
var audioSyncPrefix = String(System.getenv("DSP_TEST_AUDIO_SYNC_PREFIX"));
var inputGain = String(System.getenv("DSP_TEST_INPUT_GAIN"));
var playbackVolume = String(System.getenv("DSP_TEST_PLAYBACK_VOLUME"));
var recordGain = String(System.getenv("DSP_TEST_RECORD_GAIN"));
var buildConfig = "C6748;Fs=50k;Frame=1024;CH1;O3;Debug";
var audioRequestIndex = 0;

var fields = [
    "test_name", "backend", "backend_id", "mode", "target_kbps",
    "repeat_index", "valid_repeat", "applied_mode", "applied_backend",
    "actual_target_kbps",
    "ad_frames", "da_frames", "algo_frames", "codec_frames",
    "last_cycles", "max_cycles", "last_ms", "max_ms",
    "cpu_usage_percent", "realtime_pass",
    "payload_bits_per_hop", "scalar_bits_per_hop", "scalar_count_per_hop",
    "estimated_bitrate_kbps", "compression_ratio", "avg_bits_per_scalar",
    "band_bits_0", "band_bits_1", "band_bits_2", "band_bits_3",
    "band_bits_4", "band_bits_5", "band_bits_6", "band_bits_7",
    "quantizer_clamp_count", "total_scalar_count", "clamp_ratio",
    "invalid_count",
    "learning_hops", "target_learning_hops", "learning_progress",
    "denoise_ready", "gain_avg", "noise_psd_avg", "speech_probability_avg",
    "input_nonzero_frames", "output_nonzero_frames",
    "input_clip_frames", "output_clip_frames",
    "input_peak_max", "output_peak_max", "output_input_peak_ratio",
    "input_mean_square_avg", "output_mean_square_avg",
    "output_input_energy_ratio", "output_input_rms_ratio",
    "audio_source_file", "input_gain", "playback_volume", "record_gain",
    "measurement_status", "commit_sha", "build_config", "notes"
];

var tests = [
    { name: "method_compare", backend: "legacy_fir", backend_id: 1,
      mode: 1, target_kbps: 0, full_chain: false },
    { name: "method_compare", backend: "polyphase_project1", backend_id: 2,
      mode: 1, target_kbps: 0, full_chain: false },
    { name: "method_compare", backend: "wola", backend_id: 0,
      mode: 1, target_kbps: 0, full_chain: false },
    { name: "codec_only", backend: "wola", backend_id: 0,
      mode: 11, target_kbps: 160, full_chain: false },
    { name: "codec_only", backend: "wola", backend_id: 0,
      mode: 11, target_kbps: 240, full_chain: false },
    { name: "codec_only", backend: "wola", backend_id: 0,
      mode: 11, target_kbps: 320, full_chain: false },
    { name: "full_chain_240k", backend: "wola", backend_id: 0,
      mode: 8, target_kbps: 240, full_chain: true }
];

var log = null;
var csv = null;
var script = null;
var debugServer = null;
var debugSession = null;

function say(message) {
    System.out.println(message);
    if (log != null) {
        log.write((new java.util.Date()).toString() + " " + message + "\n");
        log.flush();
    }
}

function requireEnv(value, name) {
    if ((value == null) || (value == "") || (value == "null")) {
        throw "Missing required environment variable " + name;
    }
}

function optionalEnv(value, fallback) {
    if ((value == null) || (value == "") || (value == "null")) {
        return fallback;
    }
    return value;
}

function csvEscape(value) {
    var text = String(value);
    if ((text.indexOf(",") >= 0) || (text.indexOf("\"") >= 0) ||
        (text.indexOf("\n") >= 0) || (text.indexOf("\r") >= 0)) {
        return "\"" + text.replace(/"/g, "\"\"") + "\"";
    }
    return text;
}

function writeRow(row) {
    var i;
    for (i = 0; i < fields.length; i++) {
        if (i > 0) {
            csv.write(",");
        }
        csv.write(csvEscape(row[fields[i]]));
    }
    csv.write("\n");
    csv.flush();
}

function evaluate(expr) {
    return String(debugSession.expression.evaluate(expr));
}

function readExpr(expr, notes) {
    try {
        return evaluate(expr);
    } catch (e) {
        notes.push(expr + "=ERR");
        return "NOT_MEASURED";
    }
}

function asNumber(value, fallback) {
    var parsed = parseFloat(String(value));
    return isNaN(parsed) ? fallback : parsed;
}

function asInt(value, fallback) {
    var parsed = parseInt(String(value), 10);
    return isNaN(parsed) ? fallback : parsed;
}

function fixed(value, digits) {
    if (!isFinite(value)) {
        return "NOT_MEASURED";
    }
    return value.toFixed(digits);
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
    notes.push("loaded_program");
    Thread.sleep(500);
}

function deleteFlag(path) {
    var file = new File(path);
    if (file.exists()) {
        file["delete"]();
    }
}

function signalAudioStart(notes) {
    var ready;
    var playing;
    var writer;
    var waited;

    if ((audioSyncPrefix == null) || (audioSyncPrefix == "") ||
        (audioSyncPrefix == "null")) {
        notes.push("audio_sync_disabled_external_ch1_required");
        return;
    }

    audioRequestIndex++;
    ready = audioSyncPrefix + audioRequestIndex + ".ready";
    playing = audioSyncPrefix + audioRequestIndex + ".playing";
    deleteFlag(ready);
    deleteFlag(playing);
    writer = new FileWriter(ready, false);
    writer.write("audio_request=" + audioRequestIndex + "\n");
    writer.close();
    notes.push("audio_ready_flag=" + ready);

    waited = 0;
    while ((!new File(playing).exists()) && (waited < 5000)) {
        Thread.sleep(50);
        waited += 50;
    }
    if (!(new File(playing).exists())) {
        throw "Audio playback acknowledgement was not received for request " +
              audioRequestIndex;
    }
    notes.push("audio_playing_ack_ms=" + waited);
}

function switchToMode0(notes) {
    setExpr("SUBBAND_DebugDemoMode = 0", notes);
    runFor(modeSwitchMs, notes);
}

function configureTest(test, notes) {
    switchToMode0(notes);
    setExpr("SUBBAND_DebugRequestedBackend = " + test.backend_id, notes);
    if (test.target_kbps > 0) {
        setExpr("SUBBAND_DebugPersistentCodecKbps = " + test.target_kbps,
                notes);
        setExpr("SUBBAND_CODEC_LOOP_DebugRequestedTargetKbps = " +
                test.target_kbps, notes);
    }
    setExpr("SUBBAND_DebugDemoMode = " + test.mode, notes);
    runFor(modeSwitchMs, notes);
    if (test.target_kbps > 0) {
        setExpr("SUBBAND_CODEC_LOOP_DebugRequestedTargetKbps = " +
                test.target_kbps, notes);
    }
}

function resetBenchmark(notes) {
    setExpr("SUBBAND_DebugBenchmarkResetRequest = 1", notes);
    runFor(100, notes);
}

function readCoreCounters(notes) {
    return {
        ad: asInt(readExpr("SUBBAND_DebugAdFrames", notes), 0),
        da: asInt(readExpr("SUBBAND_DebugDaFrames", notes), 0),
        algo: asInt(readExpr("SUBBAND_DebugAlgoFrames", notes), 0),
        applied_mode: asInt(readExpr("SUBBAND_DebugAppliedDemoMode", notes), -1),
        applied_backend: asInt(readExpr("SUBBAND_DebugAppliedBackend", notes), -1)
    };
}

function assertApplied(test, counters) {
    if (counters.applied_mode != test.mode) {
        throw "AppliedDemoMode mismatch requested=" + test.mode +
              " actual=" + counters.applied_mode;
    }
    if (counters.applied_backend != test.backend_id) {
        throw "AppliedBackend mismatch requested=" + test.backend_id +
              " actual=" + counters.applied_backend;
    }
}

function preflight(test, notes) {
    var counters;

    signalAudioStart(notes);
    if (test.full_chain) {
        runFor(fullChainNoiseOnlyMs, notes);
        notes.push("full_chain_noise_only_ms=" + fullChainNoiseOnlyMs);
    }
    resetBenchmark(notes);
    runFor(preflightDurationMs, notes);
    counters = readCoreCounters(notes);
    assertApplied(test, counters);
    if ((counters.ad >= preflightMinFrames) &&
        (counters.algo >= preflightMinFrames)) {
        notes.push("preflight_pass_ad=" + counters.ad +
                   "_algo=" + counters.algo);
        return true;
    }
    notes.push("preflight_fail_ad=" + counters.ad +
               "_algo=" + counters.algo);
    return false;
}

function computeStatus(row) {
    if ((asInt(row.ad_frames, 0) < preflightMinFrames) ||
        (asInt(row.algo_frames, 0) < preflightMinFrames)) {
        return "NOT_MEASURED_NO_FRAMES";
    }
    if ((asInt(row.input_nonzero_frames, 0) <= 0) ||
        (asInt(row.input_peak_max, 0) <= 0)) {
        return "NOT_MEASURED_INPUT_SILENT";
    }
    if ((asInt(row.output_nonzero_frames, 0) <= 0) ||
        (asInt(row.output_peak_max, 0) <= 64)) {
        return "VALID_TIMING_OUTPUT_BELOW_THRESHOLD";
    }
    return "VALID";
}

function computeValidRepeat(row, test, status) {
    var baseValid;
    var estimatedBitrate;
    var invalidCount;
    var codecFrames;
    var actualTarget;
    var learningHops;
    var targetHops;
    var progress;
    var ready;

    baseValid = ((status == "VALID") ||
                 (status == "VALID_TIMING_OUTPUT_BELOW_THRESHOLD"));
    if (!baseValid) {
        return 0;
    }

    if (test.target_kbps > 0) {
        actualTarget = asInt(row.actual_target_kbps, -1);
        if (actualTarget != test.target_kbps) {
            row.notes = row.notes + "|target_kbps_mismatch_requested=" +
                        test.target_kbps + "_actual=" + actualTarget;
            return 0;
        }
    }

    if (test.name == "codec_only") {
        estimatedBitrate = asNumber(row.estimated_bitrate_kbps, 0.0);
        invalidCount = asInt(row.invalid_count, -1);
        codecFrames = asInt(row.codec_frames, 0);
        return ((codecFrames > 0) &&
                (estimatedBitrate > 0.0) &&
                (invalidCount == 0)) ? 1 : 0;
    }

    if (test.full_chain) {
        estimatedBitrate = asNumber(row.estimated_bitrate_kbps, 0.0);
        invalidCount = asInt(row.invalid_count, -1);
        codecFrames = asInt(row.codec_frames, 0);
        learningHops = asInt(row.learning_hops, 0);
        targetHops = asInt(row.target_learning_hops, -1);
        progress = asNumber(row.learning_progress, 0.0);
        ready = asInt(row.denoise_ready, 0);
        return ((learningHops == targetHops) &&
                (targetHops > 0) &&
                (progress >= 0.9990) &&
                (ready == 1) &&
                (codecFrames > 0) &&
                (estimatedBitrate > 0.0) &&
                (invalidCount == 0)) ? 1 : 0;
    }

    return 1;
}

function readScaled(expr, divisor, notes) {
    return asNumber(readExpr(expr, notes), 0.0) / divisor;
}

function readRow(test, repeatIndex, notes) {
    var row = {};
    var lastCycles;
    var maxCycles;
    var maxMs;
    var payloadBits;
    var scalarBits;
    var scalarCount;
    var totalScalars;
    var clampCount;
    var inputPeak;
    var outputPeak;
    var inputMs;
    var outputMs;
    var estimatedBitrate;
    var status;
    var band;

    row.test_name = test.name;
    row.backend = test.backend;
    row.backend_id = test.backend_id;
    row.mode = test.mode;
    row.target_kbps = test.target_kbps;
    row.repeat_index = repeatIndex;
    row.applied_mode = readExpr("SUBBAND_DebugAppliedDemoMode", notes);
    row.applied_backend = readExpr("SUBBAND_DebugAppliedBackend", notes);
    row.actual_target_kbps = readExpr("SUBBAND_CODEC_LOOP_DebugTargetKbps",
                                      notes);
    row.ad_frames = readExpr("SUBBAND_DebugAdFrames", notes);
    row.da_frames = readExpr("SUBBAND_DebugDaFrames", notes);
    row.algo_frames = readExpr("SUBBAND_DebugAlgoFrames", notes);
    row.codec_frames = readExpr("SUBBAND_CODEC_LOOP_DebugFrames", notes);

    row.last_cycles = readExpr("SUBBAND_DebugLastCycles", notes);
    row.max_cycles = readExpr("SUBBAND_DebugMaxCycles", notes);
    lastCycles = asNumber(row.last_cycles, -1.0);
    maxCycles = asNumber(row.max_cycles, -1.0);
    row.last_ms = fixed(lastCycles / cyclesPerMs, 6);
    row.max_ms = fixed(maxCycles / cyclesPerMs, 6);
    maxMs = maxCycles / cyclesPerMs;
    row.cpu_usage_percent = fixed((maxMs / frameBudgetMs) * 100.0, 6);
    row.realtime_pass = ((maxMs >= 0.0) && (maxMs < frameBudgetMs)) ? 1 : 0;

    payloadBits = asInt(readExpr("SUBBAND_CODEC_LOOP_DebugPayloadBitsPerHop",
                                 notes), 0);
    scalarBits = asInt(readExpr("SUBBAND_CODEC_LOOP_DebugScalarBitsPerHop",
                                notes), 0);
    scalarCount = asInt(readExpr("SUBBAND_CODEC_LOOP_DebugScalarCountPerHop",
                                 notes), 0);
    clampCount = asInt(readExpr("SUBBAND_CODEC_LOOP_DebugQuantizerClampCount",
                                notes), 0);
    totalScalars = asInt(readExpr("SUBBAND_CODEC_LOOP_DebugTotalScalarCount",
                                  notes), 0);
    estimatedBitrate = (payloadBits * 50000.0 / 256.0) / 1000.0;
    row.payload_bits_per_hop = payloadBits;
    row.scalar_bits_per_hop = scalarBits;
    row.scalar_count_per_hop = scalarCount;
    row.estimated_bitrate_kbps = fixed(estimatedBitrate, 6);
    row.compression_ratio =
        (estimatedBitrate > 0.0) ? fixed(800.0 / estimatedBitrate, 6) : "0.000000";
    row.avg_bits_per_scalar =
        (scalarCount > 0) ? fixed(scalarBits / scalarCount, 6) : "0.000000";
    for (band = 0; band < 8; band++) {
        row["band_bits_" + band] =
            readExpr("SUBBAND_CODEC_LOOP_DebugBandBits" + band, notes);
    }
    row.quantizer_clamp_count = clampCount;
    row.total_scalar_count = totalScalars;
    row.clamp_ratio =
        (totalScalars > 0) ? fixed(clampCount / totalScalars, 8) : "0.00000000";
    row.invalid_count = readExpr("SUBBAND_CODEC_LOOP_DebugInvalidCount", notes);

    row.learning_hops = readExpr("SUBBAND_DENOISE_DebugLearnHops", notes);
    row.target_learning_hops =
        readExpr("SUBBAND_DENOISE_DebugTargetHops", notes);
    row.learning_progress =
        fixed(readScaled("SUBBAND_DENOISE_DebugLearnProgressX1000000",
                         1000000.0, notes), 6);
    row.denoise_ready = readExpr("SUBBAND_DENOISE_DebugReady", notes);
    row.gain_avg = fixed(readScaled("SUBBAND_DENOISE_DebugGainAvgX1000000",
                                    1000000.0, notes), 6);
    row.noise_psd_avg =
        readExpr("SUBBAND_DENOISE_DebugNoisePsdAvgDiv16", notes);
    row.speech_probability_avg =
        fixed(readScaled("SUBBAND_DENOISE_DebugMcraSpeechProbAvgX1000000",
                         1000000.0, notes), 6);

    row.input_nonzero_frames =
        readExpr("SUBBAND_DebugInputNonzeroFrames", notes);
    row.output_nonzero_frames =
        readExpr("SUBBAND_DebugOutputNonzeroFrames", notes);
    row.input_clip_frames = readExpr("SUBBAND_DebugInputClipFrames", notes);
    row.output_clip_frames = readExpr("SUBBAND_DebugOutputClipFrames", notes);
    row.input_peak_max = readExpr("SUBBAND_DebugInputPeakMax", notes);
    row.output_peak_max = readExpr("SUBBAND_DebugOutputPeakMax", notes);
    inputPeak = asNumber(row.input_peak_max, 0.0);
    outputPeak = asNumber(row.output_peak_max, 0.0);
    row.output_input_peak_ratio =
        (inputPeak > 0.0) ? fixed(outputPeak / inputPeak, 6) : "0.000000";
    row.input_mean_square_avg =
        readExpr("SUBBAND_DebugInputMeanSquareAvg", notes);
    row.output_mean_square_avg =
        readExpr("SUBBAND_DebugOutputMeanSquareAvg", notes);
    inputMs = asNumber(row.input_mean_square_avg, 0.0);
    outputMs = asNumber(row.output_mean_square_avg, 0.0);
    row.output_input_energy_ratio =
        (inputMs > 0.0) ? fixed(outputMs / inputMs, 6) : "0.000000";
    row.output_input_rms_ratio =
        (inputMs > 0.0) ? fixed(Math.sqrt(outputMs / inputMs), 6) : "0.000000";

    status = computeStatus(row);
    row.measurement_status = status;
    row.audio_source_file = audioFile;
    row.input_gain = inputGain;
    row.playback_volume = playbackVolume;
    row.record_gain = recordGain;
    row.commit_sha = commitSha;
    row.build_config = buildConfig;
    row.notes = notes.join("|");
    row.valid_repeat = computeValidRepeat(row, test, status);
    return row;
}

function runMeasurement(test, repeatIndex, allowRetry) {
    var notes = [];
    var row;

    say("BEGIN " + test.name + " backend=" + test.backend +
        " target=" + test.target_kbps + " repeat=" + repeatIndex);
    configureTest(test, notes);
    if (!preflight(test, notes)) {
        if (allowRetry) {
            say("PRECHECK FAILED; reset/load retry once");
            resetAndLoad(notes);
            return runMeasurement(test, repeatIndex, false);
        }
        throw "Preflight failed twice for " + test.name +
              " backend=" + test.backend + " target=" + test.target_kbps +
              "; stop remaining tests";
    }
    resetBenchmark(notes);
    runFor(steadyDurationMs, notes);
    row = readRow(test, repeatIndex, notes);
    writeRow(row);
    say("END " + test.name + " backend=" + test.backend +
        " target=" + test.target_kbps + " repeat=" + repeatIndex +
        " status=" + row.measurement_status +
        " max_ms=" + row.max_ms +
        " cpu=" + row.cpu_usage_percent);
    return row;
}

try {
    requireEnv(commitSha, "DSP_TEST_COMMIT_SHA");
    audioFile = optionalEnv(audioFile, "EXTERNAL_CONTINUOUS_CH1_INPUT");
    inputGain = optionalEnv(inputGain, "NOT_MEASURED_OPERATOR_UNAVAILABLE");
    playbackVolume = optionalEnv(playbackVolume,
                                 "NOT_MEASURED_OPERATOR_UNAVAILABLE");
    recordGain = optionalEnv(recordGain,
                             "NOT_MEASURED_AUDIO_CAPTURE_UNAVAILABLE");

    log = new FileWriter(logPath, false);
    csv = new FileWriter(csvPath, false);
    csv.write(fields.join(",") + "\n");
    csv.flush();

    script = ScriptingEnvironment.instance();
    debugServer = script.getServer("DebugServer.1");
    debugServer.setConfig(ccxml);
    debugSession = debugServer.openSession(".*C674X_0");
    debugSession.target.connect();
    say("connected C674X_0 commit=" + commitSha);
    resetAndLoad([]);

    for (var repeat = 1; repeat <= 3; repeat++) {
        for (var index = 0; index < tests.length; index++) {
            runMeasurement(tests[index], repeat, true);
        }
    }

    debugSession.target.disconnect();
    say("disconnected C674X_0");
} catch (e) {
    say("DSS_ERROR=" + e);
    throw e;
} finally {
    try {
        if (debugSession != null) {
            debugSession.terminate();
        }
    } catch (ignore1) {}
    try {
        if (debugServer != null) {
            debugServer.stop();
        }
    } catch (ignore2) {}
    try {
        if (csv != null) {
            csv.close();
        }
    } catch (ignore3) {}
    try {
        if (log != null) {
            log.close();
        }
    } catch (ignore4) {}
}
