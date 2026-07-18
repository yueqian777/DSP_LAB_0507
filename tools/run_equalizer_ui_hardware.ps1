param(
    [string]$RepoRoot = "",
    [string]$CcsRoot = "D:\SoftwareDownload\CCS_20.5.0.00028_win\ccs",
    [ValidateRange(2, 10)]
    [int]$TimeoutMinutes = 5
)

$ErrorActionPreference = "Stop"
$root = if ($RepoRoot) { (Resolve-Path $RepoRoot).Path } else {
    (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
}
$dss = Join-Path $CcsRoot "ccs_base\scripting\bin\dss.bat"
$program = Join-Path $root "Debug\DSP_LAB_0507.out"
$linkInfo = Join-Path $root "Debug\DSP_LAB_0507_linkInfo.xml"
$ccxml = Join-Path $root "TargetConfig\C6748.ccxml"
$dssScript = Join-Path $root "tools\dss\dss_equalizer_ui_hardware.js"
$allowed = @(
    "?? tools/dss/dss_equalizer_ui_hardware.js"
    "?? tools/run_equalizer_ui_hardware.ps1"
)
$dirty = @(& git -C $root status --porcelain) |
    Where-Object { $_ -notin $allowed }
if ($dirty.Count -ne 0) {
    throw "UI hardware test found unexpected worktree changes: $($dirty -join ';')"
}
foreach ($path in @($dss, $program, $linkInfo, $ccxml, $dssScript)) {
    if (-not (Test-Path -LiteralPath $path)) { throw "Missing path: $path" }
}
if ((Get-Content -Raw -LiteralPath $linkInfo) -notmatch
    '<link_errors>0x0</link_errors>') {
    throw "Current CCS artifact has link errors"
}
$nmPath = Join-Path $CcsRoot `
    "tools\compiler\ti-cgt-c6000_8.5.0.LTS\bin\nm6x.exe"
$symbols = & $nmPath $program
foreach ($required in @("EQ_DebugLcdRuntimeMask", "EQ_DebugTouchActionCount",
    "EQ_DebugUiStateBytes", "EQ_DebugTouchStateBytes")) {
    if (($symbols -join "`n") -notmatch [regex]::Escape($required)) {
        throw "Current artifact is not the LCD+Touch profile: $required missing"
    }
}

$head = (& git -C $root rev-parse HEAD).Trim()
$shortSha = $head.Substring(0, 7)
$buildIdPath = Join-Path $root `
    "Code\User\user_dsp\equalizer_build_id_generated.h"
$buildId = Get-Content -Raw -LiteralPath $buildIdPath
if (($buildId -notmatch ("EQ_BUILD_GIT_SHA `"{0}`"" -f $shortSha)) -or
    ($buildId -notmatch 'EQ_BUILD_DIRTY 0')) {
    throw "Generated build ID does not match $shortSha/clean"
}

$stamp = Get-Date -Format "yyyyMMdd_HHmmss"
$resultDir = Join-Path $env:TEMP `
    "DSP_LAB_0507\equalizer_ui\${shortSha}_$stamp"
New-Item -ItemType Directory -Path $resultDir -Force | Out-Null
$wavePath = Join-Path $resultDir "ui_stress_music_50khz.wav"
$stream = [IO.File]::Open($wavePath, [IO.FileMode]::Create)
$writer = [IO.BinaryWriter]::new($stream)
try {
    $sampleRate = 50000
    $samples = $sampleRate * 5
    $dataBytes = $samples * 2
    $writer.Write([Text.Encoding]::ASCII.GetBytes("RIFF"))
    $writer.Write([int](36 + $dataBytes))
    $writer.Write([Text.Encoding]::ASCII.GetBytes("WAVEfmt "))
    $writer.Write([int]16); $writer.Write([int16]1); $writer.Write([int16]1)
    $writer.Write([int]$sampleRate); $writer.Write([int]($sampleRate * 2))
    $writer.Write([int16]2); $writer.Write([int16]16)
    $writer.Write([Text.Encoding]::ASCII.GetBytes("data"))
    $writer.Write([int]$dataBytes)
    $amplitude = [Math]::Pow(10.0, -18.0 / 20.0) * 32767.0
    for ($index = 0; $index -lt $samples; $index++) {
        $time = $index / [double]$sampleRate
        $value = $amplitude * (
            0.22 * [Math]::Sin(2 * [Math]::PI * 97.65625 * $time) +
            0.22 * [Math]::Sin(2 * [Math]::PI * 390.625 * $time) +
            0.22 * [Math]::Sin(2 * [Math]::PI * 1953.125 * $time) +
            0.18 * [Math]::Sin(2 * [Math]::PI * 8007.8125 * $time) +
            0.16 * [Math]::Sin(2 * [Math]::PI * 620.0 * $time))
        $writer.Write([int16][Math]::Round($value))
    }
} finally {
    $writer.Dispose(); $stream.Dispose()
}

$player = [System.Media.SoundPlayer]::new($wavePath)
$dssProcess = $null
try {
    $player.Load(); $player.PlayLooping()
    $env:DSP_TEST_ROOT = $root -replace '\\', '/'
    $env:DSP_TEST_CCXML = $ccxml -replace '\\', '/'
    $env:DSP_TEST_PROGRAM = $program -replace '\\', '/'
    $env:DSP_TEST_RESULT_DIR = $resultDir -replace '\\', '/'
    $env:DSP_TEST_EXPECTED_SHA = $shortSha
    $stdout = Join-Path $resultDir "dss_stdout.log"
    $stderr = Join-Path $resultDir "dss_stderr.log"
    $arguments = @('/d', '/s', '/c', ('""{0}" "{1}""' -f $dss, $dssScript))
    $dssProcess = Start-Process -FilePath $env:ComSpec -ArgumentList $arguments `
        -PassThru -WindowStyle Hidden -RedirectStandardOutput $stdout `
        -RedirectStandardError $stderr
    if (-not $dssProcess.WaitForExit($TimeoutMinutes * 60 * 1000)) {
        & taskkill.exe /PID $dssProcess.Id /T /F | Out-Null
        throw "DSS timeout after $TimeoutMinutes minutes"
    }
    $dssProcess.WaitForExit(); $dssProcess.Refresh()
    $summaryPath = Join-Path $resultDir "summary.json"
    if (-not (Test-Path -LiteralPath $summaryPath)) {
        throw "DSS produced no summary: $stderr"
    }
    $summary = Get-Content -Raw -LiteralPath $summaryPath | ConvertFrom-Json
    if (-not $summary.pass) {
        throw "UI hardware validation failed: $($summary.error)"
    }
    Write-Output "RESULT_DIR=$resultDir"
    Write-Output "SUMMARY=$summaryPath"
} finally {
    $player.Stop(); $player.Dispose()
    if (($null -ne $dssProcess) -and (-not $dssProcess.HasExited)) {
        & taskkill.exe /PID $dssProcess.Id /T /F | Out-Null
    }
    Remove-Item Env:DSP_TEST_ROOT, Env:DSP_TEST_CCXML, Env:DSP_TEST_PROGRAM,
        Env:DSP_TEST_RESULT_DIR, Env:DSP_TEST_EXPECTED_SHA `
        -ErrorAction SilentlyContinue
}
