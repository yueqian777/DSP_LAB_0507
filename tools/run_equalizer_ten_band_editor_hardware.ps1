[CmdletBinding()]
param(
    [ValidateSet("NoHardware", "DryRun", "Hardware")]
    [string]$Mode = "NoHardware",
    [switch]$DryRun,
    [string]$ExpectedSha = "",
    [string]$ConfirmHardware = "",
    [string]$RepoRoot = "",
    [string]$CcsRoot = "D:\SoftwareDownload\CCS_20.5.0.00028_win\ccs",
    [ValidateRange(10, 30)]
    [int]$EnduranceMinutes = 10,
    [ValidateRange(30, 600)]
    [int]$InteractionTimeoutSeconds = 180,
    [ValidateRange(15, 45)]
    [int]$DssTimeoutMinutes = 25
)

$ErrorActionPreference = "Stop"

# The repository currently has no board. Both the default path and every
# dry-run path terminate before resolving CCS paths or starting a process.
if ($DryRun.IsPresent -or $Mode -ne "Hardware") {
    Write-Output "NOT_RUN_NO_HARDWARE"
    return
}

if ($ConfirmHardware -ne "C6748_CONNECTED_AND_AUDIO_LOOP_READY") {
    throw "Hardware mode requires -ConfirmHardware C6748_CONNECTED_AND_AUDIO_LOOP_READY"
}
if ($ExpectedSha -notmatch '^[0-9a-fA-F]{40}$') {
    throw "Hardware mode requires the exact 40-character commit SHA."
}

$root = if ($RepoRoot) { (Resolve-Path $RepoRoot).Path } else {
    (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
}
$dss = Join-Path $CcsRoot "ccs_base\scripting\bin\dss.bat"
$program = Join-Path $root "Debug\DSP_LAB_0507.out"
$linkInfo = Join-Path $root "Debug\DSP_LAB_0507_linkInfo.xml"
$ccxml = Join-Path $root "TargetConfig\C6748.ccxml"
$dssScript = Join-Path $root "tools\dss\dss_equalizer_ten_band_editor.js"
$buildIdPath = Join-Path $root `
    "Code\User\user_dsp\equalizer_build_id_generated.h"
$nmPath = Join-Path $CcsRoot `
    "tools\compiler\ti-cgt-c6000_8.5.0.LTS\bin\nm6x.exe"

foreach ($path in @($dss, $program, $linkInfo, $ccxml, $dssScript,
        $buildIdPath, $nmPath)) {
    if (-not (Test-Path -LiteralPath $path)) {
        throw "Missing required path: $path"
    }
}

$branch = (& git -C $root branch --show-current).Trim()
$head = (& git -C $root rev-parse HEAD).Trim().ToLowerInvariant()
$expected = $ExpectedSha.ToLowerInvariant()
$dirty = @(& git -C $root status --porcelain --untracked-files=all)
if ($branch -ne "main") {
    throw "Hardware validation must run from main; current branch is $branch"
}
if ($head -ne $expected) {
    throw "HEAD $head does not match exact expected SHA $expected"
}
if ($dirty.Count -ne 0) {
    throw "Hardware validation requires dirty=0: $($dirty -join ';')"
}
if ((Get-Content -Raw -LiteralPath $linkInfo) -notmatch
    '<link_errors>0x0</link_errors>') {
    throw "Current CCS artifact has nonzero link_errors."
}

$shortSha = $head.Substring(0, 7)
$buildId = Get-Content -Raw -LiteralPath $buildIdPath
if (($buildId -notmatch
        ("EQ_BUILD_GIT_SHA `"{0}`"" -f [regex]::Escape($shortSha))) -or
    ($buildId -notmatch 'EQ_BUILD_DIRTY 0')) {
    throw "Generated build identity does not match $shortSha/dirty=0."
}

$symbolText = (& $nmPath $program 2>&1) -join "`n"
if ($LASTEXITCODE -ne 0) {
    throw "nm6x could not inspect the Project 3.3 artifact."
}
$requiredSymbols = @(
    "EQ_DebugBuildGitSha",
    "EQ_DebugBuildDirty",
    "EQ_DebugInitStage",
    "EQ_DebugTouchActionCount",
    "EQ_DebugTouchLastAction",
    "EQ_DebugUiRequestedPage",
    "EQ_DebugUiDisplayedPage",
    "EQ_DebugUiPageBuilding",
    "EQ_DebugUiDraftVersion",
    "EQ_DebugUiEditorStateBytes",
    "EQ_DebugUiTotalStateBytes",
    "EQ_DebugControlRequestToken",
    "EQ_DebugControlAppliedToken",
    "EQ_DebugBuilderCancelCount",
    "EQ_DebugLcdJobCount",
    "EQ_DebugLcdRasterFaultCount",
    "EQ_DebugLcdFifoUnderflowCount",
    "EQ_DebugLcdFramebufferCanaryFailureCount"
)
foreach ($symbol in $requiredSymbols) {
    if ($symbolText -notmatch [regex]::Escape($symbol)) {
        throw "Artifact is not the ten-band editor + Touch profile: $symbol missing"
    }
}

$stamp = Get-Date -Format "yyyyMMdd_HHmmss"
$resultDir = Join-Path $env:TEMP `
    "DSP_LAB_0507\equalizer_ten_band_editor\${shortSha}_$stamp"
New-Item -ItemType Directory -Path $resultDir -Force | Out-Null
$wavePath = Join-Path $resultDir "editor_endurance_music_50khz.wav"
$instructionPath = Join-Path $resultDir "operator_instructions.txt"

@"
Physical action order (wait for ACTION_REQUIRED before each tap):
1. PAGE_TO_EDITOR
2. BAND_31, BAND_62, BAND_125, BAND_250, BAND_500
3. BAND_1K, BAND_2K, BAND_4K, BAND_8K, BAND_16K
4. PLUS_ONCE
5. APPLY_ONCE
6. MINUS_ONCE
7. RESET_FLAT
8. PAGE_TO_DYNAMIC

Each item is one complete press/release. Do not tap ahead of the prompt.
The script then performs a controlled stale-request check and a
$EnduranceMinutes-minute objective run. Visual quality and Touch feel remain
operator observations and are not inferred from DSS counters.
"@ | Set-Content -LiteralPath $instructionPath -Encoding ascii

$stream = [IO.File]::Open($wavePath, [IO.FileMode]::Create)
$writer = [IO.BinaryWriter]::new($stream)
try {
    $sampleRate = 50000
    $samples = $sampleRate * 5
    $dataBytes = $samples * 2
    $writer.Write([Text.Encoding]::ASCII.GetBytes("RIFF"))
    $writer.Write([int](36 + $dataBytes))
    $writer.Write([Text.Encoding]::ASCII.GetBytes("WAVEfmt "))
    $writer.Write([int]16)
    $writer.Write([int16]1)
    $writer.Write([int16]1)
    $writer.Write([int]$sampleRate)
    $writer.Write([int]($sampleRate * 2))
    $writer.Write([int16]2)
    $writer.Write([int16]16)
    $writer.Write([Text.Encoding]::ASCII.GetBytes("data"))
    $writer.Write([int]$dataBytes)
    $amplitude = [Math]::Pow(10.0, -18.0 / 20.0) * 32767.0
    for ($index = 0; $index -lt $samples; $index++) {
        $time = $index / [double]$sampleRate
        $value = $amplitude * (
            0.24 * [Math]::Sin(2 * [Math]::PI * 125.0 * $time) +
            0.22 * [Math]::Sin(2 * [Math]::PI * 500.0 * $time) +
            0.20 * [Math]::Sin(2 * [Math]::PI * 2000.0 * $time) +
            0.18 * [Math]::Sin(2 * [Math]::PI * 8000.0 * $time) +
            0.16 * [Math]::Sin(2 * [Math]::PI * 620.0 * $time))
        $writer.Write([int16][Math]::Round($value))
    }
}
finally {
    $writer.Dispose()
    $stream.Dispose()
}

[ordered]@{
    result_label = "PENDING_HARDWARE_RUN"
    expected_commit_sha = $head
    expected_dirty = 0
    expected_init_stage = 11
    endurance_minutes = $EnduranceMinutes
    interaction_timeout_seconds = $InteractionTimeoutSeconds
    program = $program
    program_sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $program).Hash
    link_errors = 0
    operator_instructions = $instructionPath
} | ConvertTo-Json | Set-Content -LiteralPath `
    (Join-Path $resultDir "provenance.json") -Encoding utf8

$player = [System.Media.SoundPlayer]::new($wavePath)
$dssProcess = $null
$timedOut = $false
try {
    $player.Load()
    $player.PlayLooping()

    $env:DSP_TEN_BAND_EDITOR_HARDWARE_AUTH = `
        "C6748_CONNECTED_AND_AUDIO_LOOP_READY"
    $env:DSP_TEST_ROOT = $root -replace '\\', '/'
    $env:DSP_TEST_CCXML = $ccxml -replace '\\', '/'
    $env:DSP_TEST_PROGRAM = $program -replace '\\', '/'
    $env:DSP_TEST_RESULT_DIR = $resultDir -replace '\\', '/'
    $env:DSP_TEST_EXPECTED_SHA = $head
    $env:DSP_TEST_ENDURANCE_MINUTES = [string]$EnduranceMinutes
    $env:DSP_TEST_INTERACTION_TIMEOUT_SECONDS = `
        [string]$InteractionTimeoutSeconds

    Write-Output "RESULT_DIR=$resultDir"
    Write-Output "INSTRUCTIONS=$instructionPath"
    Write-Output "Wait for each ACTION_REQUIRED line before touching the LCD."

    $arguments = @('/d', '/s', '/c', ('""{0}" "{1}""' -f $dss, $dssScript))
    $dssProcess = Start-Process -FilePath $env:ComSpec `
        -ArgumentList $arguments -PassThru -NoNewWindow
    if (-not $dssProcess.WaitForExit($DssTimeoutMinutes * 60 * 1000)) {
        $timedOut = $true
        & taskkill.exe /PID $dssProcess.Id /T /F | Out-Null
        throw "DSS exceeded the $DssTimeoutMinutes-minute timeout."
    }
    $dssProcess.WaitForExit()
    $dssProcess.Refresh()
    if ($dssProcess.ExitCode -ne 0) {
        throw "DSS exited with code $($dssProcess.ExitCode)."
    }

    $summaryPath = Join-Path $resultDir "summary.json"
    if (-not (Test-Path -LiteralPath $summaryPath)) {
        throw "DSS produced no summary."
    }
    $summary = Get-Content -Raw -LiteralPath $summaryPath | ConvertFrom-Json
    if (($summary.completed -ne $true) -or
        ($summary.result_label -ne "MEASURED_ON_CURRENT_BOARD")) {
        throw "Hardware validation did not complete: $($summary.error)"
    }
    Write-Output "RESULT_LABEL=$($summary.result_label)"
    Write-Output "SUMMARY=$summaryPath"
}
finally {
    $player.Stop()
    $player.Dispose()
    if (($null -ne $dssProcess) -and (-not $dssProcess.HasExited) -and
        (-not $timedOut)) {
        & taskkill.exe /PID $dssProcess.Id /T /F | Out-Null
    }
    Remove-Item Env:DSP_TEN_BAND_EDITOR_HARDWARE_AUTH,
        Env:DSP_TEST_ROOT, Env:DSP_TEST_CCXML, Env:DSP_TEST_PROGRAM,
        Env:DSP_TEST_RESULT_DIR, Env:DSP_TEST_EXPECTED_SHA,
        Env:DSP_TEST_ENDURANCE_MINUTES,
        Env:DSP_TEST_INTERACTION_TIMEOUT_SECONDS -ErrorAction SilentlyContinue
}
