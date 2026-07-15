param(
    [string]$RepoRoot = "",
    [string]$CcsRoot = "D:\SoftwareDownload\CCS_20.5.0.00028_win\ccs",
    [string]$PythonPath = "C:\Python314\python.exe",
    [ValidateRange(1, 20)]
    [int]$DssTimeoutMinutes = 5
)

$ErrorActionPreference = "Stop"
$root = if ($RepoRoot) { (Resolve-Path $RepoRoot).Path } else {
    Split-Path -Parent $PSScriptRoot
}
$gmake = Join-Path $CcsRoot "utils\bin\gmake.exe"
$dss = Join-Path $CcsRoot "ccs_base\scripting\bin\dss.bat"
$ccxml = Join-Path $root "TargetConfig\C6748.ccxml"
$program = Join-Path $root "Debug\DSP_LAB_0507.out"
$linkInfo = Join-Path $root "Debug\DSP_LAB_0507_linkInfo.xml"
$script = Join-Path $root "tools\dss\dss_wola_thd_board.js"
$post = Join-Path $root "tools\thd\board_thd_results.py"
$frequencies = @(500, 1000, 3000, 4000, 8000)

foreach ($required in @($gmake, $dss, $ccxml, $script, $post, $PythonPath)) {
    if (-not (Test-Path -LiteralPath $required)) { throw "Missing required path: $required" }
}

$dirty = @(& git -C $root status --porcelain)
if ($dirty.Count -ne 0) {
    throw "Formal JTAG THD run requires a clean worktree: $($dirty -join ';')"
}
$head = (& git -C $root rev-parse HEAD).Trim()
$shortSha = $head.Substring(0, 7)
$generatedId = Get-Content -Raw -LiteralPath `
    (Join-Path $root "Code\User\user_dsp\equalizer_build_id_generated.h")
if (($generatedId -notmatch ("EQ_BUILD_GIT_SHA `"{0}`"" -f [regex]::Escape($shortSha))) -or
    ($generatedId -notmatch 'EQ_BUILD_DIRTY 0')) {
    throw "Generated build identity does not match $shortSha/clean."
}

$resultDir = Join-Path $root "results\thd\board_digital\$shortSha"
New-Item -ItemType Directory -Force -Path $resultDir | Out-Null
& $PythonPath -B $post prepare --output-dir $resultDir
if ($LASTEXITCODE -ne 0) { throw "Board THD input preparation failed." }

$buildStart = [DateTime]::UtcNow
$buildLog = Join-Path $resultDir "project32_jtag_thd_build.log"
& $gmake -C (Join-Path $root "Debug") -B all `
    "GEN_OPTS__FLAG=--define=DSP_LAB_PROJECT_SELECT=32 --define=SUBBAND_THD_BOARD_TEST=1 --define=EQ_USE_GENERATED_BUILD_ID=1" `
    2>&1 | Tee-Object -FilePath $buildLog
if ($LASTEXITCODE -ne 0) { throw "Project 3.2 JTAG THD build failed." }
foreach ($artifact in @($program, $linkInfo)) {
    if (-not (Test-Path -LiteralPath $artifact)) { throw "Missing build artifact: $artifact" }
    if ((Get-Item $artifact).LastWriteTimeUtc -lt $buildStart.AddSeconds(-2)) {
        throw "Stale build artifact: $artifact"
    }
}
if ((Get-Content -Raw -LiteralPath $linkInfo) -notmatch '<link_errors>0x0</link_errors>') {
    throw "CCS link_errors is nonzero."
}
$expanded = (& $gmake -C (Join-Path $root "Debug") -n -B all `
    "GEN_OPTS__FLAG=--define=DSP_LAB_PROJECT_SELECT=32 --define=SUBBAND_THD_BOARD_TEST=1 --define=EQ_USE_GENERATED_BUILD_ID=1" `
    2>&1 | Out-String)
$expanded | Set-Content -Encoding utf8 (Join-Path $resultDir "expanded_build_commands.log")
if (($expanded -notmatch 'DSP_LAB_PROJECT_SELECT=32') -or
    ($expanded -notmatch 'SUBBAND_THD_BOARD_TEST=1') -or
    ($expanded -notmatch 'EQ_USE_GENERATED_BUILD_ID=1')) {
    throw "Expanded build commands do not prove the JTAG THD configuration."
}

[ordered]@{
    commit_sha = $head
    dirty = 0
    build_config = "C6748;Project=32;SUBBAND_THD_BOARD_TEST=1;EQ_USE_GENERATED_BUILD_ID=1;Fs=50k;Frame=1024"
    transport = "JTAG_DSS_MEMORY_LOAD_SAVE"
    uses_pc_soundcard = $false
    input_source = "deterministic PCM16 loaded into DSP DDR"
    capture_point = "C6748 WOLA output in DSP DDR"
    includes_adc = $false
    includes_dac_analog = $false
    link_errors = 0
} | ConvertTo-Json | Set-Content -Encoding utf8 (Join-Path $resultDir "provenance.json")

try {
    foreach ($frequency in $frequencies) {
        $frequencyDir = Join-Path $resultDir "${frequency}Hz"
        $inputRaw = Join-Path $frequencyDir "input_full.pcm16le"
        $outputRaw = Join-Path $frequencyDir "output_full.pcm16le"
        $summary = Join-Path $frequencyDir "dss_summary.json"
        $stdout = Join-Path $frequencyDir "dss_stdout.log"
        $stderr = Join-Path $frequencyDir "dss_stderr.log"

        $env:DSP_TEST_ROOT = ($root -replace '\\', '/')
        $env:DSP_TEST_CCXML = ($ccxml -replace '\\', '/')
        $env:DSP_TEST_PROGRAM = ($program -replace '\\', '/')
        $env:DSP_TEST_INPUT_RAW = ($inputRaw -replace '\\', '/')
        $env:DSP_TEST_OUTPUT_RAW = ($outputRaw -replace '\\', '/')
        $env:DSP_TEST_SUMMARY = ($summary -replace '\\', '/')
        $env:DSP_TEST_EXPECTED_SHA = $shortSha
        $env:DSP_TEST_FREQUENCY_HZ = [string]$frequency

        $process = Start-Process -FilePath $dss -ArgumentList @($script) -PassThru `
            -WindowStyle Hidden -RedirectStandardOutput $stdout -RedirectStandardError $stderr
        if (-not $process.WaitForExit($DssTimeoutMinutes * 60 * 1000)) {
            & taskkill.exe /PID $process.Id /T /F | Out-Null
            throw "DSS timed out at $frequency Hz."
        }
        $process.WaitForExit()
        $process.Refresh()
        if (($null -ne $process.ExitCode) -and ($process.ExitCode -ne 0)) {
            throw "DSS failed at $frequency Hz with exit code $($process.ExitCode). See $stderr"
        }
        if (-not (Test-Path -LiteralPath $summary)) {
            throw "DSS produced no summary at $frequency Hz."
        }
        $record = Get-Content -Raw -LiteralPath $summary | ConvertFrom-Json
        if ($record.measurement_status -ne "MEASURED_BOARD_DIGITAL_NO_ADC_DAC_ANALOG") {
            throw "DSS measurement failed at $frequency Hz: $($record.detail)"
        }
        Write-Output "BOARD_CAPTURE frequency=$frequency Hz frames=$($record.frames) max_cycles=$($record.max_frame_cycles)"
    }
}
finally {
    Remove-Item Env:DSP_TEST_ROOT, Env:DSP_TEST_CCXML, Env:DSP_TEST_PROGRAM,
        Env:DSP_TEST_INPUT_RAW, Env:DSP_TEST_OUTPUT_RAW, Env:DSP_TEST_SUMMARY,
        Env:DSP_TEST_EXPECTED_SHA, Env:DSP_TEST_FREQUENCY_HZ -ErrorAction SilentlyContinue
}

& $PythonPath -B $post analyze --output-dir $resultDir
if ($LASTEXITCODE -ne 0) { throw "Board JTAG THD analysis failed." }
Write-Output "RESULT_DIR=$resultDir"
