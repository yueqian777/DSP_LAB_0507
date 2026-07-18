import pathlib
import shutil
import subprocess
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[2]
RUNNER = ROOT / "tools" / "run_equalizer_ten_band_editor_hardware.ps1"
DSS = ROOT / "tools" / "dss" / "dss_equalizer_ten_band_editor.js"


def powershell_executable():
    executable = shutil.which("powershell.exe") or shutil.which("pwsh.exe")
    if executable is None:
        raise unittest.SkipTest("PowerShell is unavailable")
    return executable


def run_runner(*arguments):
    return subprocess.run(
        [
            powershell_executable(),
            "-NoProfile",
            "-ExecutionPolicy",
            "Bypass",
            "-File",
            str(RUNNER),
            *arguments,
        ],
        cwd=ROOT,
        text=True,
        capture_output=True,
        timeout=20,
        check=False,
    )


class EqualizerEditorHardwareToolingTest(unittest.TestCase):
    def test_tool_files_exist(self):
        self.assertTrue(RUNNER.is_file())
        self.assertTrue(DSS.is_file())

    def test_default_and_dry_run_are_hardware_inert(self):
        invocations = [(), ("-DryRun",), ("-Mode", "DryRun")]
        for arguments in invocations:
            with self.subTest(arguments=arguments):
                result = run_runner(*arguments)
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertEqual(result.stdout.strip(), "NOT_RUN_NO_HARDWARE")
                self.assertNotIn("PASS", result.stdout.upper())

    def test_powershell_syntax_and_guard_order(self):
        source = RUNNER.read_text(encoding="utf-8")
        quoted = str(RUNNER).replace("'", "''")
        command = (
            "$tokens=$null;$errors=$null;"
            f"[System.Management.Automation.Language.Parser]::ParseFile('{quoted}',"
            "[ref]$tokens,[ref]$errors)|Out-Null;"
            "if($errors.Count){$errors|ForEach-Object{$_.Message};exit 1}"
        )
        result = subprocess.run(
            [powershell_executable(), "-NoProfile", "-Command", command],
            cwd=ROOT,
            text=True,
            capture_output=True,
            timeout=20,
            check=False,
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        guard = source.index('if ($DryRun.IsPresent -or $Mode -ne "Hardware")')
        self.assertLess(guard, source.index("Resolve-Path"))
        self.assertLess(guard, source.index("Start-Process"))
        self.assertEqual(
            source.count('Write-Output "NOT_RUN_NO_HARDWARE"'), 1
        )

    def test_hardware_mode_requires_exact_provenance_and_profile(self):
        source = RUNNER.read_text(encoding="utf-8")
        required = [
            "^[0-9a-fA-F]{40}$",
            'branch -ne "main"',
            "rev-parse HEAD",
            "status --porcelain --untracked-files=all",
            "EQ_BUILD_DIRTY 0",
            "<link_errors>0x0</link_errors>",
            "C6748_CONNECTED_AND_AUDIO_LOOP_READY",
            "EQ_DebugUiEditorStateBytes",
            "EQ_DebugTouchActionCount",
            "EQ_DebugLcdFramebufferCanaryFailureCount",
            'ValidateRange(10, 30)',
        ]
        for contract in required:
            with self.subTest(contract=contract):
                self.assertIn(contract, source)

    def test_dss_contract_covers_deferred_board_acceptance(self):
        source = DSS.read_text(encoding="utf-8")
        auth_guard = source.index(
            'hardwareAuthorization != "C6748_CONNECTED_AND_AUDIO_LOOP_READY"'
        )
        self.assertLess(auth_guard, source.index("ScriptingEnvironment.instance()"))
        self.assertIn('System.out.println("NOT_RUN_NO_HARDWARE")', source)
        required = [
            "EQ_DebugBuildDirty",
            "EQ_DebugBuildGitSha",
            "EQ_DebugInitStage",
            "ACTION_EDITOR_BAND_0 + band",
            "ACTION_EDITOR_PLUS",
            "ACTION_EDITOR_MINUS",
            "ACTION_EDITOR_APPLY",
            "ACTION_EDITOR_RESET_FLAT",
            "PRESET_CUSTOM",
            "RESET FLAT applied",
            "stale_latest_wins",
            "PAGE_TO_EDITOR",
            "PAGE_TO_DYNAMIC",
            "dynamic_status",
            "EQ_DebugLcdMaxJobCycles",
            "EQ_DebugFrameLatencyMaxCycles",
            "EQ_DebugLcdRasterFaultCount",
            "EQ_DebugLcdFifoUnderflowCount",
            "EQ_DebugLcdFramebufferCanaryFailureCount",
            "EQ_DebugDeadlineMissCount",
            "EQ_DebugFrameServiceOverlapCount",
            "EQ_DebugFrameServiceDroppedCount",
            "EQ_DebugClipCount",
            "enduranceMinutes * 60 * 1000",
            "enduranceMinutes >= 10",
        ]
        for contract in required:
            with self.subTest(contract=contract):
                self.assertIn(contract, source)


if __name__ == "__main__":
    unittest.main()
