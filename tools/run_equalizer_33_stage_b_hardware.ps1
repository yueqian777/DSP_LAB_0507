param(
    [string]$RepoRoot = "",
    [string]$CcsRoot = "D:\SoftwareDownload\CCS_20.5.0.00028_win\ccs",
    [ValidateRange(1, 20)]
    [int]$DssTimeoutMinutes = 20,
    [ValidateSet("SUBJECTIVE_SPEAKER_OBSERVATION", "FAIL", "NOT_OBSERVED")]
    [string]$OperatorStatus = "NOT_OBSERVED",
    [string]$OperatorNotes = ""
)

$ErrorActionPreference = "Stop"
$root = if ($RepoRoot) { (Resolve-Path $RepoRoot).Path } else {
    Split-Path -Parent $PSScriptRoot
}
$dss = Join-Path $CcsRoot "ccs_base\scripting\bin\dss.bat"
$program = Join-Path $root "Debug\DSP_LAB_0507.out"
$linkInfo = Join-Path $root "Debug\DSP_LAB_0507_linkInfo.xml"
$ccxml = Join-Path $root "TargetConfig\C6748.ccxml"
$script = Join-Path $root "tools\dss\dss_equalizer_33_stage_b_hardware.js"
$playerJob = $null
$dssProcess = $null
$timedOut = $false

function Write-MonoWave {
    param([string]$Path, [int]$Seconds, [scriptblock]$Sample)
    $sampleRate = 50000
    $count = $sampleRate * $Seconds
    $stream = [System.IO.File]::Open($Path, [System.IO.FileMode]::Create)
    $writer = [System.IO.BinaryWriter]::new($stream)
    try {
        $dataBytes = $count * 2
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
        for ($index = 0; $index -lt $count; $index++) {
            $value = [Math]::Max(-32768.0, [Math]::Min(32767.0, (& $Sample $index)))
            $writer.Write([int16][Math]::Round($value))
        }
    }
    finally {
        $writer.Dispose()
        $stream.Dispose()
    }
}

foreach ($required in @($dss, $program, $linkInfo, $ccxml, $script)) {
    if (-not (Test-Path -LiteralPath $required)) { throw "Missing required path: $required" }
}
$head = (& git -C $root rev-parse HEAD).Trim()
$shortSha = $head.Substring(0, 7)
$stamp = Get-Date -Format "yyyyMMdd_HHmmss"
$resultDir = Join-Path $env:TEMP "DSP_LAB_0507\equalizer_33_stage_b\${shortSha}_$stamp"
$syncDir = Join-Path $resultDir "audio_sync"
$tonePath = Join-Path $resultDir "tone_1khz_-18dbfs_50khz.wav"
$musicPath = Join-Path $resultDir "music_like_-18dbfs_50khz.wav"
$doneFlag = Join-Path $syncDir "done"
$allowedUntracked = @(
    "?? tools/dss/dss_equalizer_33_stage_b_hardware.js",
    "?? tools/run_equalizer_33_stage_b_hardware.ps1"
)
$unexpectedDirty = @(& git -C $root status --porcelain) |
    Where-Object { $_ -notin $allowedUntracked }
if ($unexpectedDirty.Count -ne 0) {
    throw "Formal hardware validation found unexpected worktree changes: $($unexpectedDirty -join ';')"
}
if ((Get-Content -Raw -LiteralPath $linkInfo) -notmatch "<link_errors>0x0</link_errors>") {
    throw "CCS link_errors is nonzero."
}
$generatedId = Get-Content -Raw -LiteralPath (Join-Path $root "Code\User\user_dsp\equalizer_build_id_generated.h")
if (($generatedId -notmatch ("EQ_BUILD_GIT_SHA `"{0}`"" -f [regex]::Escape($shortSha))) -or
    ($generatedId -notmatch 'EQ_BUILD_DIRTY 0')) {
    throw "Generated build identity does not match $shortSha/clean."
}

New-Item -ItemType Directory -Force -Path $syncDir | Out-Null
$amplitude = [Math]::Pow(10.0, -18.0 / 20.0) * 32767.0
Write-MonoWave -Path $tonePath -Seconds 5 -Sample {
    param($n)
    $amplitude * [Math]::Sin(2.0 * [Math]::PI * 1000.0 * $n / 50000.0)
}
Write-MonoWave -Path $musicPath -Seconds 5 -Sample {
    param($n)
    $t = $n / 50000.0
    $envelope = 0.70 + 0.20 * [Math]::Sin(2.0 * [Math]::PI * 1.3 * $t)
    $amplitude * $envelope * (
        0.52 * [Math]::Sin(2.0 * [Math]::PI * 180.0 * $t) +
        0.28 * [Math]::Sin(2.0 * [Math]::PI * 620.0 * $t) +
        0.20 * [Math]::Sin(2.0 * [Math]::PI * 2400.0 * $t))
}

[ordered]@{
    commit_sha = $head
    dirty = 0
    program = $program
    link_errors = 0
    audio_chain = "PC default output -> ADC CH1 -> Project 3.3 -> DAC CH1 -> speaker"
    tone = $tonePath
    music = $musicPath
    operator_status = $OperatorStatus
    operator_notes = $OperatorNotes
    timeout_minutes = $DssTimeoutMinutes
} | ConvertTo-Json | Set-Content -Encoding utf8 (Join-Path $resultDir "provenance.json")

try {
    $playerJob = Start-Job -ScriptBlock {
        param($sync, $tone, $music, $done)
        $player = $null
        $current = ""
        try {
            while (-not (Test-Path -LiteralPath $done)) {
                foreach ($kind in @("tone", "music")) {
                    $ready = Join-Path $sync "$kind.ready"
                    $playing = Join-Path $sync "$kind.playing"
                    if ((Test-Path -LiteralPath $ready) -and
                        (-not (Test-Path -LiteralPath $playing))) {
                        if ($null -ne $player) { $player.Stop(); $player.Dispose() }
                        $wave = if ($kind -eq "tone") { $tone } else { $music }
                        $player = [System.Media.SoundPlayer]::new($wave)
                        $player.Load()
                        $player.PlayLooping()
                        $current = $kind
                        Set-Content -LiteralPath $playing -Value "playing=$current" -NoNewline
                    }
                }
                Start-Sleep -Milliseconds 50
            }
        }
        finally {
            if ($null -ne $player) { $player.Stop(); $player.Dispose() }
        }
    } -ArgumentList $syncDir, $tonePath, $musicPath, $doneFlag

    $env:DSP_TEST_ROOT = ($root -replace '\\', '/')
    $env:DSP_TEST_CCXML = ($ccxml -replace '\\', '/')
    $env:DSP_TEST_PROGRAM = ($program -replace '\\', '/')
    $env:DSP_TEST_RESULT_DIR = ($resultDir -replace '\\', '/')
    $env:DSP_TEST_AUDIO_SYNC_DIR = ($syncDir -replace '\\', '/')
    $env:DSP_TEST_EXPECTED_SHA = $shortSha
    $env:DSP_TEST_OPERATOR_STATUS = $OperatorStatus
    $env:DSP_TEST_OPERATOR_NOTES = $OperatorNotes

    $stdout = Join-Path $resultDir "dss_stdout.log"
    $stderr = Join-Path $resultDir "dss_stderr.log"
    $dssArguments = @('/d', '/s', '/c', ('""{0}" "{1}""' -f $dss, $script))
    $dssProcess = Start-Process -FilePath $env:ComSpec -ArgumentList $dssArguments -PassThru `
        -WindowStyle Hidden -RedirectStandardOutput $stdout -RedirectStandardError $stderr
    if (-not $dssProcess.WaitForExit($DssTimeoutMinutes * 60 * 1000)) {
        $timedOut = $true
        & taskkill.exe /PID $dssProcess.Id /T /F | Out-Null
        throw "DSS exceeded the $DssTimeoutMinutes minute timeout."
    }
    $dssProcess.WaitForExit()
    $dssProcess.Refresh()
    $summaryPath = Join-Path $resultDir "summary.json"
    if (-not (Test-Path -LiteralPath $summaryPath)) {
        throw "DSS produced no summary. See $stderr"
    }
    $summary = Get-Content -Raw -LiteralPath $summaryPath | ConvertFrom-Json
    if ($summary.result_label -eq "FAIL") {
        throw "DSS validation failed: $($summary.error). See $stderr"
    }
    Write-Output "RESULT_DIR=$resultDir"
}
finally {
    Set-Content -LiteralPath $doneFlag -Value "done" -NoNewline -ErrorAction SilentlyContinue
    if (($null -ne $dssProcess) -and (-not $dssProcess.HasExited) -and (-not $timedOut)) {
        & taskkill.exe /PID $dssProcess.Id /T /F | Out-Null
    }
    if ($null -ne $playerJob) {
        Wait-Job $playerJob -Timeout 5 | Out-Null
        Stop-Job $playerJob -ErrorAction SilentlyContinue
        Remove-Job $playerJob -Force -ErrorAction SilentlyContinue
    }
    Remove-Item Env:DSP_TEST_ROOT, Env:DSP_TEST_CCXML, Env:DSP_TEST_PROGRAM,
        Env:DSP_TEST_RESULT_DIR, Env:DSP_TEST_AUDIO_SYNC_DIR,
        Env:DSP_TEST_EXPECTED_SHA, Env:DSP_TEST_OPERATOR_STATUS,
        Env:DSP_TEST_OPERATOR_NOTES -ErrorAction SilentlyContinue
}
