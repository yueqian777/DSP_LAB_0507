/* Project 3.3 audio-safe LCD and CH1 digital-capture board matrix. */
importPackage(Packages.com.ti.debug.engine.scripting);
importPackage(Packages.com.ti.ccstudio.scripting.environment);
importPackage(Packages.java.lang);
importPackage(Packages.java.io);

var defaultRoot = String(new File(".").getCanonicalPath()).replace(/\\/g, "/");
var root = env("DSP_TEST_ROOT", defaultRoot);
var ccxml = env("DSP_TEST_CCXML", root + "/TargetConfig/C6748.ccxml");
var program = env("DSP_TEST_PROGRAM", root + "/Debug/DSP_LAB_0507.out");
var resultDir = env("DSP_TEST_RESULT_DIR",
                    root + "/docs/eval_outputs/equalizer_lcd_audio_safe");
var csvPath = resultDir + "/matrix.csv";
var metadataPath = resultDir + "/capture_metadata.jsonl";
var summaryPath = resultDir + "/final_summary.json";
var commitSha = env("DSP_TEST_COMMIT_SHA", "UNKNOWN");
var buildConfig = env("DSP_TEST_BUILD_CONFIG", "C6748;Project=33;Fs=50k;Frame=1024;CH1");
var dirtyState = env("DSP_TEST_DIRTY_STATE", "UNKNOWN");
var operatorStatus = env("DSP_TEST_OPERATOR_STATUS", "NOT_OBSERVED");
var operatorNotes = env("DSP_TEST_OPERATOR_NOTES", "");
var python = env("DSP_TEST_PYTHON", "python");
var measurementMs = envInt("DSP_TEST_ROW_MS", 60000);
var stabilityMs = envInt("DSP_TEST_STABILITY_MS", 300000);
var debugCaptureArmMs = envInt("DSP_TEST_CAPTURE_ARM_MS", 500);
var debugCaptureRunMs = envInt("DSP_TEST_CAPTURE_RUN_MS", 1000);
var EQ_LCD_RUNTIME_STATUS = 1;
var EQ_LCD_RUNTIME_GAINS = 2;
var MASK_BOTH = EQ_LCD_RUNTIME_STATUS | EQ_LCD_RUNTIME_GAINS;
var debugSession = null;
var script = null;
var debugServer = null;
var csv = null;
var metadata = null;
var rowReadFailed = false;
var allSixtySecondRowsComplete = true;
var captureOverallPass = true;
var stabilityTechnicalPass = false;
var EQ_DIAG_RAW_COPY = 0;
var EQ_DIAG_FLAT = 2;
var EQ_DIAG_PRESET = 4;
var EQ_PRESET_COUNT = 6;

var fields = [
    "test_name", "requested_runtime_mask", "actual_runtime_mask", "mask_name",
    "duration_ms", "operator_mode", "operator_status", "operator_notes",
    "technical_pass", "pass", "pass_reasons",
    "process_frames", "ad_frames", "da_frames", "algo_last_cycles", "algo_max_cycles",
    "frame_service_last_cycles", "frame_service_max_cycles", "frame_latency_last_cycles",
    "frame_latency_max_cycles", "active_deadline_miss", "latency_deadline_miss",
    "service_overlap", "service_dropped",
    "lcd_pending_mask", "lcd_job_count", "lcd_last_job_cycles", "lcd_max_job_cycles",
    "lcd_audio_during_draw", "lcd_deferred_audio", "lcd_auto_disable_count",
    "lcd_auto_disable_reason", "lcd_unexpected_full_redraw", "capture_manual_ready",
    "capture_trigger_ready",
    "clip_count", "requested_mode", "transition_target_mode", "applied_mode",
    "baseline_process_frames", "delta_process_frames", "baseline_ad_frames", "delta_ad_frames",
    "baseline_da_frames", "delta_da_frames", "baseline_active_deadline_miss",
    "delta_active_deadline_miss", "baseline_latency_deadline_miss",
    "delta_latency_deadline_miss", "baseline_service_overlap", "delta_service_overlap",
    "baseline_service_dropped", "delta_service_dropped", "baseline_lcd_job_count",
    "delta_lcd_job_count", "baseline_lcd_auto_disable_count",
    "delta_lcd_auto_disable_count", "baseline_lcd_unexpected_full_redraw",
    "delta_lcd_unexpected_full_redraw", "measurement_status", "commit_sha",
    "build_config", "dirty_state", "notes"
];

var diagnostics = {
    process_frames: "EQ_DebugProcessFrames", ad_frames: "EQ_DebugAdFrames",
    da_frames: "EQ_DebugDaFrames", algo_last_cycles: "EQ_DebugAlgoLastCycles",
    algo_max_cycles: "EQ_DebugAlgoMaxCycles",
    frame_service_last_cycles: "EQ_DebugFrameServiceLastCycles",
    frame_service_max_cycles: "EQ_DebugFrameServiceMaxCycles",
    frame_latency_last_cycles: "EQ_DebugFrameLatencyLastCycles",
    frame_latency_max_cycles: "EQ_DebugFrameLatencyMaxCycles",
    active_deadline_miss: "EQ_DebugDeadlineMissCount",
    latency_deadline_miss: "EQ_DebugFrameLatencyDeadlineMissCount",
    service_overlap: "EQ_DebugFrameServiceOverlapCount",
    service_dropped: "EQ_DebugFrameServiceDroppedCount",
    lcd_pending_mask: "EQ_DebugLcdPendingMask", lcd_job_count: "EQ_DebugLcdJobCount",
    lcd_last_job_cycles: "EQ_DebugLcdLastJobCycles",
    lcd_max_job_cycles: "EQ_DebugLcdMaxJobCycles",
    lcd_audio_during_draw: "EQ_DebugLcdAudioArrivedDuringDrawCount",
    lcd_deferred_audio: "EQ_DebugLcdDeferredAudioCount",
    lcd_auto_disable_count: "EQ_DebugLcdAutoDisabledCount", /* EQ_DebugLcdAutoDisableCount */
    lcd_auto_disable_reason: "EQ_DebugLcdAutoDisableReason",
    lcd_unexpected_full_redraw: "EQ_DebugLcdUnexpectedFullRedrawCount",
    capture_manual_ready: "EQ_CaptureManualReady",
    capture_trigger_ready: "EQ_TriggerCaptureReady", clip_count: "EQ_DebugClipCount",
    requested_mode: "EQ_DebugRequestedMode",
    transition_target_mode: "EQ_DebugTransitionTargetMode",
    applied_mode: "EQ_DebugAppliedMode"
};

function env(name, fallback) {
    var value = String(System.getenv(name));
    return (value == "null" || value == "") ? fallback : value;
}

function envInt(name, fallback) {
    var value = parseInt(env(name, String(fallback)), 10);
    return isNaN(value) ? fallback : value;
}

function mkdirs(path) {
    var directory = new File(path);
    if (!directory.exists() && !directory.mkdirs()) throw "Cannot create " + path;
}

function evaluate(expression) { return String(debugSession.expression.evaluate(expression)); }

function read(expression, notes) {
    try { return evaluate(expression); }
    catch (error) {
        rowReadFailed = true;
        notes.push(expression + "=NOT_MEASURED");
        return "NOT_MEASURED";
    }
}

function write(expression, notes) {
    evaluate(expression);
    notes.push("write:" + expression);
}

function csvEscape(value) {
    var text = String(value);
    return /[,"\r\n]/.test(text) ? "\"" + text.replace(/"/g, "\"\"") + "\"" : text;
}

function writeRow(row) {
    var index;
    for (index = 0; index < fields.length; index++) {
        if (index) csv.write(",");
        csv.write(csvEscape(row[fields[index]]));
    }
    csv.write("\n"); csv.flush();
}

function runFor(milliseconds) {
    debugSession.target.runAsynch();
    Thread.sleep(milliseconds);
    debugSession.target.halt();
}

function maskName(mask) {
    if (mask == 0) return "OFF";
    if (mask == EQ_LCD_RUNTIME_STATUS) return "STATUS";
    if (mask == EQ_LCD_RUNTIME_GAINS) return "GAINS";
    if (mask == MASK_BOTH) return "BOTH";
    return "UNKNOWN";
}

function numeric(value) {
    var parsed = parseInt(String(value), 10);
    return isNaN(parsed) ? null : parsed;
}

function takeBaseline(notes) {
    var baseline = {}, field;
    rowReadFailed = false;
    for (field in diagnostics) baseline[field] = read(diagnostics[field], notes);
    return baseline;
}

function finalStatus(requestedMask, row, baseStatus) {
    var actualMask = numeric(row.actual_runtime_mask);
    var disabledDelta = numeric(row.delta_lcd_auto_disable_count);
    var activeMissDelta = numeric(row.delta_active_deadline_miss);
    var latencyMissDelta = numeric(row.delta_latency_deadline_miss);
    var overlapDelta = numeric(row.delta_service_overlap);
    var droppedDelta = numeric(row.delta_service_dropped);
    var redrawDelta = numeric(row.delta_lcd_unexpected_full_redraw);
    if (rowReadFailed) return "INCOMPLETE_READ_ERROR";
    if (actualMask != requestedMask) return "INCOMPLETE_MASK_MISMATCH";
    if ((disabledDelta != null) && (disabledDelta > 0)) return "INCOMPLETE_AUTO_DISABLED";
    if (((activeMissDelta != null) && (activeMissDelta > 0)) ||
        ((latencyMissDelta != null) && (latencyMissDelta > 0)) ||
        ((overlapDelta != null) && (overlapDelta > 0)) ||
        ((droppedDelta != null) && (droppedDelta > 0)) ||
        ((redrawDelta != null) && (redrawDelta > 0))) {
        return "INCOMPLETE_SAFETY_COUNTER_INCREASED";
    }
    if ((operatorStatus == "PASS" || operatorStatus == "FAIL") &&
        (row.operator_mode == true)) {
        return "MEASURED_DSS_WATCH_PLUS_OPERATOR_" + operatorStatus;
    }
    return baseStatus;
}

function snapshot(name, mask, duration, operatorMode, status, notes, baseline) {
    var row = {}, field, current, before;
    rowReadFailed = notes.join("|").indexOf("NOT_MEASURED") >= 0;
    row.test_name = name; row.requested_runtime_mask = mask;
    row.actual_runtime_mask = read("EQ_DebugLcdRuntimeMask", notes);
    row.mask_name = maskName(mask); row.duration_ms = duration;
    row.operator_mode = operatorMode; row.operator_status = operatorStatus;
    row.operator_notes = operatorNotes;
    for (field in diagnostics) row[field] = read(diagnostics[field], notes);
    for (field in baseline) {
        if ((field == "process_frames") || (field == "ad_frames") ||
            (field == "da_frames") || (field == "active_deadline_miss") ||
            (field == "latency_deadline_miss") || (field == "service_overlap") ||
            (field == "service_dropped") || (field == "lcd_job_count") ||
            (field == "lcd_auto_disable_count") ||
            (field == "lcd_unexpected_full_redraw")) {
            before = numeric(baseline[field]); current = numeric(row[field]);
            row["baseline_" + field] = baseline[field];
            row["delta_" + field] = (before == null || current == null) ?
                                      "NOT_MEASURED" : current - before;
        }
    }
    row.measurement_status = finalStatus(mask, row, status); row.commit_sha = commitSha;
    evaluateRowPass(row, mask);
    if ((duration == measurementMs) && !row.pass) {
        allSixtySecondRowsComplete = false;
    }
    if (duration == stabilityMs) stabilityTechnicalPass = row.technical_pass;
    row.build_config = buildConfig; row.dirty_state = dirtyState; row.notes = notes.join("|");
    writeRow(row);
}

function rowPassedSixtySecondCriteria(row) {
    return row.pass == true;
}

function evaluateRowPass(row, requestedMask) {
    var processDelta = numeric(row.delta_process_frames);
    var adDelta = numeric(row.delta_ad_frames);
    var daDelta = numeric(row.delta_da_frames);
    var activeMissDelta = numeric(row.delta_active_deadline_miss);
    var latencyMissDelta = numeric(row.delta_latency_deadline_miss);
    var overlapDelta = numeric(row.delta_service_overlap);
    var droppedDelta = numeric(row.delta_service_dropped);
    var redrawDelta = numeric(row.delta_lcd_unexpected_full_redraw);
    var disabledDelta = numeric(row.delta_lcd_auto_disable_count);
    var jobDelta = numeric(row.delta_lcd_job_count);
    var clipCount = numeric(row.clip_count);
    var maxJob = numeric(row.lcd_max_job_cycles);
    var reasons = [];
    if (row.measurement_status.indexOf("MEASURED_DSS_WATCH") != 0) reasons.push("status");
    if (processDelta == null || processDelta <= 0) reasons.push("process_frames");
    if (adDelta == null || adDelta <= 0) reasons.push("ad_frames");
    if (daDelta == null || daDelta <= 0) reasons.push("da_frames");
    if (!(processDelta != null && adDelta != null &&
          Math.abs(processDelta - adDelta) <= 1)) reasons.push("process_ad_pair");
    if (!(processDelta != null && daDelta != null &&
          Math.abs(processDelta - daDelta) <= 1)) reasons.push("process_da_pair");
    if (!(adDelta != null && daDelta != null && Math.abs(adDelta - daDelta) <= 1))
        reasons.push("ad_da_pair");
    if (clipCount != 0) reasons.push("clip_count");
    if (maxJob == null || maxJob > 2280000) reasons.push("lcd_job_over_5ms");
    if (requestedMask != 0 && (jobDelta == null || jobDelta <= 0))
        reasons.push("lcd_no_jobs");
    if (!(activeMissDelta == 0)) reasons.push("active_deadline_miss");
    if (!(latencyMissDelta == 0)) reasons.push("latency_deadline_miss");
    if (!(overlapDelta == 0)) reasons.push("service_overlap");
    if (!(droppedDelta == 0)) reasons.push("service_dropped");
    if (!(redrawDelta == 0)) reasons.push("unexpected_full_redraw");
    if (!(disabledDelta == 0)) reasons.push("auto_disable");
    row.technical_pass = reasons.length == 0;
    if ((row.duration_ms == stabilityMs) && !(operatorStatus == "PASS"))
        reasons.push("operator_not_pass");
    row.pass = reasons.length == 0;
    row.pass_reasons = row.pass ? "PASS" : reasons.join("|");
    return row.pass;
}

function writeFinalSummary() {
    var finalPass = allSixtySecondRowsComplete && stabilityTechnicalPass &&
                    captureOverallPass && (operatorStatus == "PASS");
    var finalStatus = finalPass ? "FINAL_PASS" : "FINAL_INCOMPLETE";
    if ((operatorStatus == "NOT_OBSERVED") && stabilityTechnicalPass)
        finalStatus = "MEASURED_DSS_ONLY";
    var writer = new FileWriter(summaryPath, false);
    writer.write(JSON.stringify({measurement_status: finalStatus, pass: finalPass,
        sixty_second_rows_pass: allSixtySecondRowsComplete,
        stability_pass: stabilityTechnicalPass, capture_pass: captureOverallPass,
        operator_status: operatorStatus, operator_notes: operatorNotes,
        commit_sha: commitSha, build_config: buildConfig, dirty_state: dirtyState}) + "\n");
    writer.close();
    return finalPass;
}

function measureRow(name, mask, duration, operatorMode, diagPath, mode) {
    var notes = [], baseline;
    write("EQ_DebugLcdRuntimeMask = " + mask, notes);
    write("EQ_DebugDiagPath = " + diagPath, notes);
    write("EQ_DebugMode = " + mode, notes);
    baseline = takeBaseline(notes);
    runFor(duration);
    snapshot(name, mask, duration, operatorMode, "MEASURED_DSS_WATCH", notes, baseline);
}

function recordManualSwitchingTest(name, duration, interval) {
    var row = {}, index, mode = 0, switches, sequence = [];
    switches = Math.floor(duration / interval);
    for (index = 0; index < switches; index++) {
        mode = (mode + 1) % EQ_PRESET_COUNT;
        sequence.push(mode);
    }
    for (index = 0; index < fields.length; index++) row[fields[index]] = "";
    row.test_name = name;
    row.requested_runtime_mask = MASK_BOTH;
    row.mask_name = "BOTH";
    row.duration_ms = duration;
    row.operator_mode = true;
    row.operator_status = "NOT_OBSERVED";
    row.technical_pass = false;
    row.pass = false;
    row.pass_reasons = "MANUAL_HARDWARE_TEST";
    row.measurement_status = "MANUAL_HARDWARE_TEST";
    row.commit_sha = commitSha;
    row.build_config = buildConfig;
    row.dirty_state = dirtyState;
    row.notes = "switch_interval_ms=" + interval +
                "|preset_sequence=" + sequence.join(":") +
                "|debugger halt/run forbidden";
    writeRow(row);
    allSixtySecondRowsComplete = false;
}

function addressOf(symbol) { return debugSession.symbol.getAddress(symbol); }

function saveRawI16LE(symbol, samples, fileName) {
    /* Target is halted by the caller. saveRaw writes signed little-endian 16-bit CH1 data. */
    debugSession.memory.saveRaw(Memory.Page.PROGRAM, addressOf(symbol), fileName,
                                samples, 16, false);
}

function metadataLine(kind, status, source, frameCount, prewrite, postCount, input, output) {
    metadata.write(JSON.stringify({capture_kind: kind, measurement_status: status,
        capture_scope: "DEBUG_CAPTURE",
        trigger_source: source, frame_count: frameCount, pre_write_index: prewrite,
        trigger_post_count: postCount, armed_ready: read("EQ_TriggerCaptureArmedReady", []),
        ready: kind == "manual" ? read("EQ_CaptureManualReady", []) :
                                  read("EQ_TriggerCaptureReady", []),
        input_raw: input, output_raw: output,
        commit_sha: commitSha, build_config: buildConfig, dirty_state: dirtyState}) + "\n");
    metadata.flush();
}

function runAnalyzer(kind, input, output, directory, prewrite, analyzerMode) {
    var command = [python, "-B", root + "/tools/equalizer_33_capture.py", kind,
                   "--input", input, "--output", output, "--out-dir", directory,
                   "--mode", analyzerMode, "--frame-len", "1024"];
    if (kind == "trigger") command.push("--prewrite-index", String(prewrite));
    var process = new ProcessBuilder(command).inheritIO().start();
    var exitCode = process.waitFor();
    return exitCode;
}

function recordAnalyzerResult(kind, exitCode, source, frameCount, prewrite,
                              postCount, input, output) {
    var status = exitCode == 0 ? "CAPTURE_ANALYZER_PASS" : "CAPTURE_ANALYZER_FAILED";
    if (exitCode != 0) captureOverallPass = false;
    metadata.write(JSON.stringify({capture_kind: kind, measurement_status: status,
        capture_scope: "DEBUG_CAPTURE",
        analyzer_exit_code: exitCode, trigger_source: source, frame_count: frameCount,
        pre_write_index: prewrite, trigger_post_count: postCount, input_raw: input,
        output_raw: output, commit_sha: commitSha}) + "\n");
    metadata.flush();
}

function captureManual() {
    var notes = [], directory = resultDir + "/capture_manual", exitCode;
    var input = directory + "/input.raw", output = directory + "/output.raw";
    mkdirs(directory);
    write("EQ_DebugDiagPath = " + EQ_DIAG_RAW_COPY, notes); /* RAW_COPY */
    write("EQ_DebugMode = 0", notes);
    write("EQ_CaptureManualRequest = 1", notes);
    runFor(debugCaptureRunMs);
    if (read("EQ_CaptureManualReady", notes) != "1") {
        captureOverallPass = false;
        metadataLine("manual", "DEBUG_CAPTURE_NOT_READY", 0, 0, 0, 0,
                     input, output);
        write("EqualizerCapture_AcknowledgeManual()", notes);
        write("EqualizerCapture_Reset()", notes);
        return;
    }
    debugSession.target.halt();
    saveRawI16LE("EQ_CaptureInput", 8 * 1024, input);
    saveRawI16LE("EQ_CaptureOutput", 8 * 1024, output);
    metadataLine("manual", "DEBUG_CAPTURE", 0, 8, 0, 0, input, output);
    exitCode = runAnalyzer("manual", input, output, directory, 0, "raw");
    recordAnalyzerResult("manual", exitCode, 0, 8, 0, 0, input, output);
    write("EqualizerCapture_AcknowledgeManual()", notes);
}

function captureTrigger(source, name) {
    var notes = [], directory = resultDir + "/capture_" + name;
    var input = directory + "/input.raw", output = directory + "/output.raw";
    var prewrite, actualSource, postCount, exitCode, armedReady, armedSource;
    mkdirs(directory);
    write("EQ_DebugDiagPath = " + EQ_DIAG_PRESET, notes); /* PRESET */
    write("EQ_DebugMode = 0", notes);
    if (source == 1) {
        write("EQ_DebugLcdRuntimeMask = " + MASK_BOTH, notes);
    }
    write("EQ_TriggerCaptureRequest = " + source, notes);
    runFor(debugCaptureArmMs);
    if (read("EQ_TriggerCaptureArmedReady", notes) != "1") {
        captureOverallPass = false;
        metadataLine("trigger", "DEBUG_CAPTURE_ARM_NOT_READY", source,
                     0, 0, 0, input, output);
        write("EqualizerCapture_AcknowledgeTrigger()", notes);
        write("EqualizerCapture_Reset()", notes);
        return;
    }
    if (source == 1) {
        if (parseInt(read("EQ_DebugLcdPendingMask", notes), 10) == 0) {
            captureOverallPass = false;
            metadataLine("trigger", "INCOMPLETE_LCD_NOT_PENDING", source,
                         0, 0, 0, input, output);
            write("EqualizerCapture_AcknowledgeTrigger()", notes);
            write("EqualizerCapture_Reset()", notes);
            return;
        }
    }
    if (source == 2) {
        /* Armed-ready is established before inducing a real accepted 0 -> 1 mode change. */
        write("EQ_DebugMode = 1", notes);
    }
    /* Fixed debug window, followed by one halt for metadata/export. */
    runFor(debugCaptureRunMs);
    if (read("EQ_TriggerCaptureReady", notes) != "1") {
        if (source != 4) captureOverallPass = false;
        metadataLine("trigger",
                     source == 4 ? "NOT_OBSERVED" : "DEBUG_CAPTURE_NOT_READY",
                     source, 0, 0, 0, input, output);
        write("EqualizerCapture_AcknowledgeTrigger()", notes);
        write("EqualizerCapture_Reset()", notes);
        return;
    }
    debugSession.target.halt();
    var readyValue = parseInt(read("EQ_TriggerCaptureReady", notes), 10);
    armedReady = parseInt(read("EQ_TriggerCaptureArmedReady", notes), 10);
    armedSource = parseInt(read("EQ_TriggerCaptureArmedSource", notes), 10);
    prewrite = parseInt(read("EQ_TriggerCapturePreWriteIndex", notes), 10);
    actualSource = parseInt(read("EQ_TriggerCaptureSource", notes), 10);
    postCount = parseInt(read("EQ_TriggerCapturePostCount", notes), 10);
    if ((actualSource != source) || (postCount != 8) || (readyValue != 1) ||
        (prewrite < 0) || (prewrite > 3) || (armedReady != 1) ||
        (armedSource != source)) {
        captureOverallPass = false;
        metadataLine("trigger", "INCOMPLETE_TRIGGER_METADATA", actualSource,
                     12, prewrite, postCount, input, output);
        write("EqualizerCapture_AcknowledgeTrigger()", notes);
        write("EqualizerCapture_Reset()", notes);
        return;
    }
    saveRawI16LE("EQ_TriggerCaptureInput", 12 * 1024, input);
    saveRawI16LE("EQ_TriggerCaptureOutput", 12 * 1024, output);
    metadataLine("trigger", "DEBUG_CAPTURE", actualSource,
                 12, prewrite, postCount, input, output);
    exitCode = runAnalyzer("trigger", input, output, directory, prewrite, "processed");
    recordAnalyzerResult("trigger", exitCode, actualSource, 12, prewrite,
                         postCount, input, output);
    write("EqualizerCapture_AcknowledgeTrigger()", notes);
}

try {
    if (!new File(ccxml).exists()) throw "Missing CCXML: " + ccxml;
    if (!new File(program).exists()) throw "Missing program: " + program;
    mkdirs(resultDir);
    csv = new FileWriter(csvPath, false); csv.write(fields.join(",") + "\n");
    metadata = new FileWriter(metadataPath, false);
    script = ScriptingEnvironment.instance(); script.traceSetConsoleLevel(TraceLevel.INFO);
    debugServer = script.getServer("DebugServer.1"); debugServer.setConfig(ccxml);
    debugSession = debugServer.openSession(".*C674X_0");
    debugSession.target.connect();
    debugSession.target.reset(); debugSession.memory.loadProgram(program);
    measureRow("raw_copy_lcd_off", 0, measurementMs, false, EQ_DIAG_RAW_COPY, 0); /* RAW_COPY */
    measureRow("flat_lcd_status", EQ_LCD_RUNTIME_STATUS, measurementMs, false, EQ_DIAG_FLAT, 0); /* FLAT */
    measureRow("preset_lcd_gains", EQ_LCD_RUNTIME_GAINS, measurementMs, false, EQ_DIAG_PRESET, 0); /* PRESET */
    measureRow("preset_lcd_both", MASK_BOTH, measurementMs, false, EQ_DIAG_PRESET, 0);
    recordManualSwitchingTest("mode_switch_5s", measurementMs, 5000);
    recordManualSwitchingTest("mode_switch_2s", measurementMs, 2000);
    if (allSixtySecondRowsComplete) {
        measureRow("stability_five_minutes", MASK_BOTH, stabilityMs, false,
                   EQ_DIAG_PRESET, 0);
    } else {
        System.out.println("Five-minute stability skipped: 60-second rows incomplete");
    }
    captureManual();
    captureTrigger(1, "lcd_job");
    captureTrigger(2, "mode_switch");
    captureTrigger(4, "audio_during_draw");
    if (!writeFinalSummary()) throw "FINAL_INCOMPLETE";
} catch (error) {
    System.err.println("DSS_ERROR=" + error); throw error;
} finally {
    try { if (debugSession != null) debugSession.target.halt(); } catch (ignored) {}
    try { if (debugSession != null) debugSession.target.disconnect(); } catch (ignored2) {}
    try { if (debugSession != null) debugSession.terminate(); } catch (ignored3) {}
    try { if (debugServer != null) debugServer.stop(); } catch (ignored4) {}
    if (csv != null) csv.close(); if (metadata != null) metadata.close();
}
