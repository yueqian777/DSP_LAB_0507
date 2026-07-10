/* Focused C6748 board test for the Project 3.2 LCD/touch UI. */
importPackage(Packages.com.ti.debug.engine.scripting);
importPackage(Packages.com.ti.ccstudio.scripting.environment);
importPackage(Packages.java.lang);
importPackage(Packages.java.io);

var root = "C:/Users/zhangyueqian/lab8/DSP_LAB_0507";
var ccxml = root + "/TargetConfig/C6748.ccxml";
var program = root + "/Debug/DSP_LAB_0507.out";
var csvPath = root + "/docs/eval_outputs/subband_touch_ui_test.csv";
var logPath = root + "/docs/eval_outputs/dss_subband_touch_ui_test.log";
var commitSha = String(System.getenv("DSP_TEST_COMMIT_SHA"));
var cyclesPerMs = 456000.0;
var modes = [0, 1, 2, 4, 6, 7, 8, 11];
var learningModes = {"2": true, "4": true, "6": true,
                     "7": true, "8": true};
var fields = [
    "case_name", "phase", "requested_mode", "applied_mode",
    "displayed_mode", "selected_kbps", "target_kbps",
    "estimated_bitrate_kbps", "learning", "learn_hops",
    "target_hops", "ready", "countdown_ms", "mode_changes",
    "ad_frames", "da_frames", "algo_frames", "codec_frames",
    "ad_delta", "da_delta", "algo_delta", "refresh_delta",
    "invalid_count", "ui_touch_count", "ui_accepted_touch_count",
    "ui_refresh_count", "ui_full_redraw_count", "ui_dirty_flags",
    "ui_font_bytes",
    "algo_max_cycles", "algo_max_ms", "ui_max_draw_cycles",
    "ui_max_draw_ms", "ui_max_touch_cycles", "ui_max_touch_ms",
    "pass", "measurement_status", "commit_sha", "notes"
];

var script = null;
var debugServer = null;
var debugSession = null;
var csv = null;
var log = new FileWriter(logPath, false);

function say(message) {
    System.out.println(message);
    log.write((new java.util.Date()).toString() + " " + message + "\n");
    log.flush();
}

function evaluate(expression) {
    return String(debugSession.expression.evaluate(expression));
}

function integer(expression) {
    var value = parseInt(evaluate(expression), 10);
    if (isNaN(value)) { throw "non-integer expression: " + expression; }
    return value;
}

function setValue(name, value) {
    evaluate(name + " = " + value);
}

function runFor(milliseconds) {
    debugSession.target.runAsynch();
    Thread.sleep(milliseconds);
    debugSession.target.halt();
}

function csvEscape(value) {
    var valueText = String(value);
    if ((valueText.indexOf(",") >= 0) || (valueText.indexOf("\"") >= 0)) {
        return "\"" + valueText.replace(/\"/g, "\"\"") + "\"";
    }
    return valueText;
}

function writeRow(row) {
    var index;
    for (index = 0; index < fields.length; index++) {
        if (index > 0) { csv.write(","); }
        csv.write(csvEscape(row[fields[index]]));
    }
    csv.write("\n");
    csv.flush();
}

function snapshot(caseName, phase, requestedMode, pass, notes) {
    var row = {};
    var payloadBits;
    var maxCycles;
    var maxDrawCycles;
    var maxTouchCycles;

    row.case_name = caseName;
    row.phase = phase;
    row.requested_mode = requestedMode;
    row.applied_mode = integer("SUBBAND_DebugAppliedDemoMode");
    row.displayed_mode = integer("SUBBAND_UI_DebugDisplayedMode");
    row.selected_kbps = integer("SUBBAND_UI_DebugSelectedCodecKbps");
    row.target_kbps = integer("SUBBAND_CODEC_LOOP_DebugTargetKbps");
    payloadBits = integer("SUBBAND_CODEC_LOOP_DebugPayloadBitsPerHop");
    row.estimated_bitrate_kbps =
        (payloadBits * 50000.0 / 256.0 / 1000.0).toFixed(4);
    row.learning = integer("SUBBAND_DENOISE_DebugLearning");
    row.learn_hops = integer("SUBBAND_DENOISE_DebugLearnHops");
    row.target_hops = integer("SUBBAND_DENOISE_DebugTargetHops");
    row.ready = integer("SUBBAND_DENOISE_DebugReady");
    row.countdown_ms = integer("SUBBAND_UI_DebugCountdownMs");
    row.mode_changes = integer("SUBBAND_DebugDemoModeChanges");
    row.ad_frames = integer("SUBBAND_DebugAdFrames");
    row.da_frames = integer("SUBBAND_DebugDaFrames");
    row.algo_frames = integer("SUBBAND_DebugAlgoFrames");
    row.codec_frames = integer("SUBBAND_CODEC_LOOP_DebugFrames");
    row.ad_delta = "";
    row.da_delta = "";
    row.algo_delta = "";
    row.refresh_delta = "";
    row.invalid_count = integer("SUBBAND_CODEC_LOOP_DebugInvalidCount");
    row.ui_touch_count = integer("SUBBAND_UI_DebugTouchCount");
    row.ui_accepted_touch_count =
        integer("SUBBAND_UI_DebugAcceptedTouchCount");
    row.ui_refresh_count = integer("SUBBAND_UI_DebugRefreshCount");
    row.ui_full_redraw_count = integer("SUBBAND_UI_DebugFullRedrawCount");
    row.ui_dirty_flags = integer("SUBBAND_UI_DebugDirtyFlags");
    row.ui_font_bytes = integer("SUBBAND_UI_DebugFontBytes");
    maxCycles = integer("SUBBAND_DebugMaxCycles");
    maxDrawCycles = integer("SUBBAND_UI_DebugMaxDrawCycles");
    maxTouchCycles = integer("SUBBAND_UI_DebugMaxTouchCycles");
    row.algo_max_cycles = maxCycles;
    row.algo_max_ms = (maxCycles / cyclesPerMs).toFixed(6);
    row.ui_max_draw_cycles = maxDrawCycles;
    row.ui_max_draw_ms = (maxDrawCycles / cyclesPerMs).toFixed(6);
    row.ui_max_touch_cycles = maxTouchCycles;
    row.ui_max_touch_ms = (maxTouchCycles / cyclesPerMs).toFixed(6);
    row.pass = pass ? 1 : 0;
    row.measurement_status = "MEASURED_DSS_WATCH";
    row.commit_sha = commitSha;
    row.notes = notes;
    return row;
}

function requestModeWithButtonFlag(buttonIndex) {
    setValue("FLAG_BUTTON_" + (buttonIndex + 1), 1);
    setValue("FLAG_TOUCH", 1);
    runFor(300);
}

function testModes() {
    var index;
    var mode;
    var row;
    var pass;
    for (index = 0; index < modes.length; index++) {
        mode = modes[index];
        requestModeWithButtonFlag(index);
        row = snapshot("mode_" + mode, "entered", mode, true,
                       "mode button flag injected through UI touch service");
        pass = (row.applied_mode == mode) &&
               (row.displayed_mode == mode) &&
               (row.ad_frames > 0) && (row.da_frames > 0) &&
               (row.algo_frames > 0);
        if (learningModes[String(mode)]) {
            pass = pass && (row.learning == 1) &&
                   (row.countdown_ms > 0) && (row.countdown_ms <= 2002);
            row.pass = pass ? 1 : 0;
            writeRow(row);
            runFor(2200);
            row = snapshot("mode_" + mode, "learning_complete", mode,
                           true, "actual denoise hop counters");
            pass = (row.applied_mode == mode) &&
                   (row.displayed_mode == mode) &&
                   (row.learn_hops == row.target_hops) &&
                   (row.ready == 1) && (row.countdown_ms == 0);
            row.pass = pass ? 1 : 0;
            writeRow(row);
        } else {
            row.pass = pass ? 1 : 0;
            writeRow(row);
        }
    }
}

function selectRate(kbps) {
    setValue("SUBBAND_UI_DebugSelectedCodecKbps", kbps);
    setValue("SUBBAND_DebugPersistentCodecKbps", kbps);
    setValue("SUBBAND_CODEC_LOOP_DebugRequestedTargetKbps", kbps);
}

function testRates(mode, buttonIndex) {
    var rates = [160, 240, 320];
    var index;
    var rate;
    var changesBefore;
    var learnBefore;
    var row;
    var pass;

    requestModeWithButtonFlag(buttonIndex);
    for (index = 0; index < rates.length; index++) {
        rate = rates[index];
        changesBefore = integer("SUBBAND_DebugDemoModeChanges");
        learnBefore = integer("SUBBAND_DENOISE_DebugLearnHops");
        selectRate(rate);
        runFor(400);
        row = snapshot("mode_" + mode + "_rate_" + rate,
                       "dynamic_rate", mode, true,
                       "DSS injects the same persistent/request fields as UI");
        pass = (row.applied_mode == mode) &&
               (row.displayed_mode == mode) &&
               (row.target_kbps == rate) &&
               (row.mode_changes == changesBefore) &&
               (row.invalid_count == 0);
        if (mode == 8) {
            pass = pass && (row.learn_hops >= learnBefore);
        } else {
            pass = pass && (row.learn_hops == learnBefore);
        }
        row.pass = pass ? 1 : 0;
        writeRow(row);
    }
}

function testDuplicateCurrentModeGuard() {
    var acceptedBefore;
    var changesBefore;
    var repeat;
    var row;
    var pass;

    requestModeWithButtonFlag(6);
    acceptedBefore = integer("SUBBAND_UI_DebugAcceptedTouchCount");
    changesBefore = integer("SUBBAND_DebugDemoModeChanges");
    for (repeat = 0; repeat < 10; repeat++) {
        setValue("FLAG_BUTTON_7", 1);
        setValue("FLAG_TOUCH", 1);
        runFor(80);
    }
    row = snapshot("duplicate_current_mode", "ten_requests", 8, true,
                   "current-mode guard; physical long press not automated");
    pass = (row.applied_mode == 8) &&
           (row.mode_changes == changesBefore) &&
           (row.ui_accepted_touch_count == acceptedBefore);
    row.pass = pass ? 1 : 0;
    writeRow(row);
}

function testSwitchStress() {
    var index;
    var mode;
    var rate;
    var row;
    var pass;

    for (index = 0; index < 50; index++) {
        mode = modes[index % modes.length];
        setValue("SUBBAND_DebugDemoMode", mode);
        runFor(80);
    }
    runFor(300);
    row = snapshot("mode_switch_stress", "50_switches", mode, true,
                   "fixed coordinates; visual drift requires operator inspection");
    pass = (row.applied_mode == mode) &&
           (row.displayed_mode == mode) &&
           (row.ui_full_redraw_count == 1);
    row.pass = pass ? 1 : 0;
    writeRow(row);

    setValue("SUBBAND_DebugDemoMode", 11);
    runFor(300);
    for (index = 0; index < 50; index++) {
        rate = 160 + (index % 3) * 80;
        selectRate(rate);
        runFor(80);
    }
    runFor(300);
    row = snapshot("rate_switch_stress", "50_switches", 11, true,
                   "codec target updates without mode re-entry");
    pass = (row.applied_mode == 11) &&
           (row.target_kbps == rate) &&
           (row.invalid_count == 0) &&
           (row.ui_full_redraw_count == 1);
    row.pass = pass ? 1 : 0;
    writeRow(row);
}

function testFiveMinuteRealtime() {
    var adBefore;
    var daBefore;
    var algoBefore;
    var refreshBefore;
    var row;
    var pass;

    setValue("SUBBAND_DebugDemoMode", 8);
    selectRate(320);
    runFor(2500);
    setValue("SUBBAND_DebugBenchmarkResetRequest", 1);
    runFor(100);
    adBefore = integer("SUBBAND_DebugAdFrames");
    daBefore = integer("SUBBAND_DebugDaFrames");
    algoBefore = integer("SUBBAND_DebugAlgoFrames");
    refreshBefore = integer("SUBBAND_UI_DebugRefreshCount");
    say("five-minute mode-8/320 run started");
    runFor(300000);
    row = snapshot("mode_8_320_realtime", "300_seconds", 8, true,
                   "frame counters validate continuity; audio listening not automated");
    row.ad_delta = row.ad_frames - adBefore;
    row.da_delta = row.da_frames - daBefore;
    row.algo_delta = row.algo_frames - algoBefore;
    row.refresh_delta = row.ui_refresh_count - refreshBefore;
    pass = (row.applied_mode == 8) &&
           (row.displayed_mode == 8) &&
           (row.ad_delta > 14000) &&
           (row.da_delta > 14000) &&
           (row.algo_delta > 14000) &&
           (row.refresh_delta > 0) &&
           (row.ready == 1) &&
           (row.target_kbps == 320) &&
           (row.invalid_count == 0) &&
           (parseFloat(row.algo_max_ms) < 20.48) &&
           (row.ui_full_redraw_count == 1);
    row.pass = pass ? 1 : 0;
    writeRow(row);
    say("five-minute run finished pass=" + row.pass +
        " max_ms=" + row.algo_max_ms);
}

try {
    if ((commitSha == null) || (commitSha == "") ||
        (commitSha == "null")) {
        throw "DSP_TEST_COMMIT_SHA is required";
    }
    csv = new FileWriter(csvPath, false);
    csv.write(fields.join(",") + "\n");
    csv.flush();
    script = ScriptingEnvironment.instance();
    debugServer = script.getServer("DebugServer.1");
    debugServer.setConfig(ccxml);
    debugSession = debugServer.openSession(".*C674X_0");
    debugSession.target.connect();
    say("connected; commit=" + commitSha);
    debugSession.target.reset();
    Thread.sleep(500);
    debugSession.memory.loadProgram(program);
    say("loaded " + program);
    runFor(1500);
    testModes();
    testRates(11, 7);
    testRates(8, 6);
    testDuplicateCurrentModeGuard();
    testSwitchStress();
    testFiveMinuteRealtime();
    writeRow(snapshot("final_state", "complete", 8, true,
                      "physical visual and listening checks remain operator-observed"));
    debugSession.target.disconnect();
    say("disconnected");
} catch (error) {
    say("DSS_ERROR=" + error);
    throw error;
} finally {
    try { if (debugSession != null) { debugSession.terminate(); } }
    catch (ignore1) {}
    try { if (debugServer != null) { debugServer.stop(); } }
    catch (ignore2) {}
    try { if (csv != null) { csv.close(); } }
    catch (ignore3) {}
    log.close();
}
