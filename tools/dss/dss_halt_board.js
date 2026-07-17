importPackage(Packages.com.ti.debug.engine.scripting);
importPackage(Packages.com.ti.ccstudio.scripting.environment);

var ccxml = String(System.getenv("DSP_TEST_CCXML"));
var script = null;
var debugServer = null;
var debugSession = null;

try {
    script = ScriptingEnvironment.instance();
    debugServer = script.getServer("DebugServer.1");
    debugServer.setConfig(ccxml);
    debugSession = debugServer.openSession(".*C674X_0");
    debugSession.target.connect();
    debugSession.target.halt();
    debugSession.target.disconnect();
    print("EMERGENCY_HALT_OK");
} finally {
    try { if (debugSession != null) debugSession.terminate(); } catch (ignore0) {}
    try { if (debugServer != null) debugServer.stop(); } catch (ignore1) {}
}
