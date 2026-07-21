param(
    [string]$RepoRoot = "",
    [string]$CcsRoot = "D:\SoftwareDownload\CCS_20.5.0.00028_win\ccs",
    [string]$PythonPath = "C:\Python314\python.exe",
    [string]$OutputDirectory = "",
    [ValidateRange(1, 10)]
    [int]$DssTimeoutMinutes = 5,
    [switch]$SkipRestore
)

$ErrorActionPreference = "Stop"
$root = if ($RepoRoot) { (Resolve-Path $RepoRoot).Path } else {
    Split-Path -Parent $PSScriptRoot
}
$gmake = Join-Path $CcsRoot "utils\bin\gmake.exe"
$dss = Join-Path $CcsRoot "ccs_base\scripting\bin\dss.bat"
$nm = Join-Path $CcsRoot "tools\compiler\ti-cgt-c6000_8.5.0.LTS\bin\nm6x.exe"
$ccxml = Join-Path $root "TargetConfig\C6748.ccxml"
$program = Join-Path $root "Debug\DSP_LAB_0507.out"
$map = Join-Path $root "Debug\DSP_LAB_0507.map"
$linkInfo = Join-Path $root "Debug\DSP_LAB_0507_linkInfo.xml"
$dssScript = Join-Path $root "tools\dss\dss_equalizer_final_metrics.js"
$analyzer = Join-Path $root "tools\analyze_equalizer_final_metrics.py"
$restoreScript = Join-Path $root "tools\dss\dss_project33_leave_running.js"

foreach ($required in @($gmake, $dss, $nm, $ccxml, $dssScript, $analyzer,
        $restoreScript, $PythonPath)) {
    if (-not (Test-Path -LiteralPath $required)) {
        throw "Missing required path: $required"
    }
}
if (Get-Process ccstudio -ErrorAction SilentlyContinue) {
    throw "Close CCS before exclusive DSS access."
}
$branch = (& git -C $root branch --show-current).Trim()
$head = (& git -C $root rev-parse HEAD).Trim()
$shortSha = $head.Substring(0, 7)
$dirty = @(& git -C $root status --porcelain --untracked-files=normal)
if ($branch -ne "main" -or $dirty.Count -ne 0) {
    throw "Formal metrics run requires clean main; branch=$branch dirty=$($dirty -join ';')"
}
if (-not $OutputDirectory) {
    $stamp = Get-Date -Format "yyyyMMdd_HHmmss"
    $OutputDirectory = Join-Path $env:TEMP `
        "project33_final_metrics_${shortSha}_$stamp"
}
if (Test-Path -LiteralPath $OutputDirectory) {
    throw "Output directory already exists: $OutputDirectory"
}
$rawDir = Join-Path $OutputDirectory "raw"
$analysisDir = Join-Path $OutputDirectory "analysis"
New-Item -ItemType Directory -Path $rawDir, $analysisDir | Out-Null

& (Join-Path $root "tools\generate_equalizer_build_id.ps1") | Out-Host
$generated = Get-Content -Raw -LiteralPath `
    (Join-Path $root "Code\User\user_dsp\equalizer_build_id_generated.h")
if (($generated -notmatch ("EQ_BUILD_GIT_SHA `"{0}`"" -f
        [regex]::Escape($shortSha))) -or
    ($generated -notmatch "EQ_BUILD_DIRTY 0")) {
    throw "Generated build identity does not match $shortSha/clean."
}

$defines = @(
    "--define=DSP_LAB_PROJECT_SELECT=33"
    "--define=EQ_USE_GENERATED_BUILD_ID=1"
    "--define=EQ_ENABLE_FINAL_METRICS_BOARD_TEST=1"
    "--define=EQ_ENABLE_AUDIO_FEATURE_ANALYZER=0"
    "--define=EQ_ENABLE_SMART_BASS=0"
    "--define=EQ_ENABLE_DYNAMIC_CLARITY=0"
    "--define=EQ_ENABLE_HARSHNESS_GUARD=0"
    "--define=EQ_ENABLE_LCD_DISPLAY=0"
    "--define=EQ_ENABLE_PROJECT33_TOUCH=0"
    "--define=EQ_ENABLE_TEN_BAND_EDITOR=0"
    "--define=EQ_ENABLE_TEN_BAND_EDITOR_TOUCH=0"
    "--define=EQ_UI_RUNTIME_DEFAULT_MASK=0"
    "--define=EQ_ENABLE_DYNAMIC_CLARITY_TIMING_DIAGNOSTICS=0"
    "--define=EQ_ENABLE_DYNAMIC_CLARITY_TRANSITION_CAPTURE=0"
    "--define=EQ_ENABLE_HARSHNESS_GUARD_TRANSITION_CAPTURE=0"
    "--define=EQ_ENABLE_HARSHNESS_GUARD_KERNEL_DIAGNOSTICS=0"
    "--define=EQ_ENABLE_FOUR_WAY_TRANSITION_DIAGNOSTICS=0"
    "--define=EQ_ENABLE_DYNAMIC_CLARITY_BENCHMARK=0"
    "--define=EQ_ENABLE_HARSHNESS_GUARD_BENCHMARK=0"
    "--define=EQ_ENABLE_UART_DIAGNOSTICS=0"
) -join " "

$buildLog = Join-Path $OutputDirectory "metrics_build.log"
$nativeErrorActionPreference = $ErrorActionPreference
try {
    $ErrorActionPreference = "Continue"
    $cleanOutput = & $gmake -C (Join-Path $root "Debug") clean 2>&1
    $cleanExitCode = $LASTEXITCODE
} finally {
    $ErrorActionPreference = $nativeErrorActionPreference
}
if ($cleanExitCode -ne 0) {
    $cleanOutput | Set-Content -Encoding utf8 $buildLog
    throw "Metrics clean failed; see $buildLog"
}
$buildStart = [DateTime]::UtcNow
try {
    $ErrorActionPreference = "Continue"
    $buildOutput = & $gmake -B -C (Join-Path $root "Debug") all `
        "GEN_OPTS__FLAG=$defines" 2>&1
    $buildExitCode = $LASTEXITCODE
} finally {
    $ErrorActionPreference = $nativeErrorActionPreference
}
@($cleanOutput) + @($buildOutput) | Set-Content -Encoding utf8 $buildLog
if ($buildExitCode -ne 0) {
    throw "Metrics build failed; see $buildLog"
}
foreach ($artifact in @($program, $map, $linkInfo)) {
    if (-not (Test-Path -LiteralPath $artifact) -or
        (Get-Item $artifact).LastWriteTimeUtc -lt $buildStart.AddSeconds(-2)) {
        throw "Missing or stale build artifact: $artifact"
    }
}
$linkText = Get-Content -Raw -LiteralPath $linkInfo
if ($linkText -notmatch "<link_errors>0x0</link_errors>") {
    throw "Metrics link_errors is nonzero."
}
$buildText = $buildOutput -join "`n"
$warningCount = ([regex]::Matches(
    $buildText, "(?im)^.*\bwarning(?:\s+#\d+)?[: ].*$")).Count
if ($warningCount -ne 0) {
    throw "Metrics build produced $warningCount warning lines."
}
$symbols = (& $nm $program) -join "`n"
foreach ($symbol in @("EqualizerEval_BoardFinalMetrics",
        "EQ_FinalMetricsResponsePacked", "EQ_FinalMetricsThdOutputPacked",
        "EQ_FinalMetricsSnrOutputPacked", "EQ_DebugFinalMetricsCompiled")) {
    if ($symbols -notmatch [regex]::Escape($symbol)) {
        throw "Metrics program is missing symbol: $symbol"
    }
}

$metricsProgram = Join-Path $rawDir "project33_final_metrics.out"
$metricsMap = Join-Path $rawDir "project33_final_metrics.map"
$metricsLinkInfo = Join-Path $rawDir "project33_final_metrics_linkInfo.xml"
Copy-Item -LiteralPath $program -Destination $metricsProgram
Copy-Item -LiteralPath $map -Destination $metricsMap
Copy-Item -LiteralPath $linkInfo -Destination $metricsLinkInfo
$outputHash = (Get-FileHash -LiteralPath $metricsProgram -Algorithm SHA256).Hash.ToLowerInvariant()

$buildSummary = [ordered]@{
    commit_sha = $head
    build_git_sha = $shortSha
    dirty = 0
    branch = $branch
    evidence_class = "BUILD_EVIDENCE"
    configuration = "Project33 final internal-digital metrics"
    defines = $defines
    test_only_suppressed_diagnostics = @(
        "179-D: unused production scheduler roots in dedicated flow build"
    )
    warning_count = $warningCount
    link_errors = "0x0"
    output_bytes = (Get-Item $metricsProgram).Length
    output_sha256 = $outputHash
    target = "TMS320C6748"
    debug_probe = "Texas Instruments XDS100v3 USB Debug Probe"
    sample_rate_hz = 50000
    frame_len = 1024
    operator = $env:USERNAME
    test_date_utc = [DateTime]::UtcNow.ToString("o")
}
$buildSummary | ConvertTo-Json -Depth 5 |
    Set-Content -Encoding utf8 (Join-Path $OutputDirectory "build_summary.json")

$env:DSP_TEST_ROOT = ($root -replace "\\", "/")
$env:DSP_TEST_CCXML = ($ccxml -replace "\\", "/")
$env:DSP_TEST_PROGRAM = ($metricsProgram -replace "\\", "/")
$env:DSP_TEST_RESULT_DIR = ($rawDir -replace "\\", "/")
$env:DSP_TEST_EXPECTED_SHORT_SHA = $shortSha
$env:DSP_TEST_EXPECTED_FULL_SHA = $head
$env:DSP_TEST_OUTPUT_SHA256 = $outputHash
$dssStdout = Join-Path $rawDir "dss_stdout.log"
$dssStderr = Join-Path $rawDir "dss_stderr.log"

$restoreResult = "SKIPPED"
try {
    try {
        $process = Start-Process -FilePath $dss -ArgumentList @($dssScript) `
            -PassThru -WindowStyle Hidden -RedirectStandardOutput $dssStdout `
            -RedirectStandardError $dssStderr
        if (-not $process.WaitForExit($DssTimeoutMinutes * 60 * 1000)) {
            & taskkill.exe /PID $process.Id /T /F | Out-Null
            throw "Final metrics DSS timed out."
        }
        $process.WaitForExit()
        $process.Refresh()
        if (($null -ne $process.ExitCode) -and ($process.ExitCode -ne 0)) {
            throw "Final metrics DSS failed with exit code $($process.ExitCode)."
        }
        $dssSummaryPath = Join-Path $rawDir "dss_summary.json"
        if (-not (Test-Path -LiteralPath $dssSummaryPath)) {
            throw "DSS did not produce dss_summary.json."
        }
        $dssSummary = Get-Content -Raw -LiteralPath $dssSummaryPath |
            ConvertFrom-Json
        if (-not $dssSummary.pass) {
            throw "DSS metrics failed: $($dssSummary.error)"
        }

        & $PythonPath -B $analyzer --raw-dir $rawDir --output-dir $analysisDir
        if ($LASTEXITCODE -ne 0) {
            throw "Final metrics analysis failed."
        }
    }
    finally {
        if (-not $SkipRestore) {
            $restoreBuildDir = Join-Path $OutputDirectory "restore_h_build"
            & (Join-Path $root "tools\run_equalizer_ui_build_matrix.ps1") `
                -ProfileNames H_project33_full -OutputDirectory $restoreBuildDir
            if ($LASTEXITCODE -ne 0) {
                throw "H restore build failed."
            }
            $restoreHash = (Get-FileHash -LiteralPath $program -Algorithm SHA256).Hash.ToLowerInvariant()
            $env:DSP_TEST_PROGRAM = ($program -replace "\\", "/")
            $env:DSP_TEST_RESULT_PATH = ((Join-Path $OutputDirectory `
                "restore_h_running.json") -replace "\\", "/")
            $restoreStdout = Join-Path $OutputDirectory "restore_h_stdout.log"
            $restoreStderr = Join-Path $OutputDirectory "restore_h_stderr.log"
            $restoreProcess = Start-Process -FilePath $dss -ArgumentList @($restoreScript) `
                -PassThru -WindowStyle Hidden -RedirectStandardOutput $restoreStdout `
                -RedirectStandardError $restoreStderr
            if (-not $restoreProcess.WaitForExit($DssTimeoutMinutes * 60 * 1000)) {
                & taskkill.exe /PID $restoreProcess.Id /T /F | Out-Null
                throw "H restore DSS timed out."
            }
            $restoreProcess.WaitForExit()
            $restoreProcess.Refresh()
            if (($null -ne $restoreProcess.ExitCode) -and
                ($restoreProcess.ExitCode -ne 0)) {
                throw "H restore DSS failed with exit code $($restoreProcess.ExitCode)."
            }
            $restoreResult = "RUNNING_DISCONNECTED"
        }
    }

    [ordered]@{
        pass = $true
        commit_sha = $head
        output_sha256 = $outputHash
        warning_count = $warningCount
        link_errors = "0x0"
        result_directory = $OutputDirectory
        analysis_directory = $analysisDir
        final_board_state = $restoreResult
    } | ConvertTo-Json -Depth 4 |
        Set-Content -Encoding utf8 (Join-Path $OutputDirectory "run_summary.json")
    Write-Output "RESULT_DIR=$OutputDirectory"
}
finally {
    Remove-Item Env:DSP_TEST_ROOT, Env:DSP_TEST_CCXML, Env:DSP_TEST_PROGRAM,
        Env:DSP_TEST_RESULT_DIR, Env:DSP_TEST_EXPECTED_SHORT_SHA,
        Env:DSP_TEST_EXPECTED_FULL_SHA, Env:DSP_TEST_OUTPUT_SHA256,
        Env:DSP_TEST_RESULT_PATH -ErrorAction SilentlyContinue
}
