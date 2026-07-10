param(
    [string]$InputGain = "NOT_MEASURED_OPERATOR_UNAVAILABLE",
    [string]$PlaybackVolume = "NOT_MEASURED_OPERATOR_UNAVAILABLE",
    [string]$RecordGain = "NOT_MEASURED_AUDIO_CAPTURE_UNAVAILABLE",
    [switch]$RequireCleanGit = $true
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$dss = "D:\SoftwareDownload\CCS_20.5.0.00028_win\ccs\ccs_base\scripting\bin\dss.bat"
$gmake = "D:\SoftwareDownload\CCS_20.5.0.00028_win\ccs\utils\bin\gmake.exe"
$python = "D:\SoftwareDownload\python.exe"
$audio = Join-Path $root "test_vectors\final_full_chain_2s_noise_speech_stationary_50k.wav"
$syncPrefix = Join-Path $root "tmp\board_missing_tests_audio_"
$doneFlag = "{0}done" -f $syncPrefix
$player = $null

Push-Location $root
try {
    if ($RequireCleanGit -and (git status --porcelain)) {
        throw "Refusing formal board test: git status is not clean."
    }

    & $python "tools\create_final_full_chain_test_vector.py"
    if ($LASTEXITCODE -ne 0) { throw "Test-vector generation failed." }
    if (-not (Test-Path -LiteralPath $audio)) {
        throw "Missing generated test audio: $audio"
    }

    $sha = (git rev-parse HEAD).Trim()
    & $gmake -C Debug all
    if ($LASTEXITCODE -ne 0) { throw "CCS Debug build failed." }
    $linkInfo = Get-Content -LiteralPath "Debug\DSP_LAB_0507_linkInfo.xml" -Raw
    if ($linkInfo -notmatch "<link_errors>0x0</link_errors>") {
        throw "CCS link_errors is nonzero."
    }
    if ($RequireCleanGit -and (git status --porcelain)) {
        throw "Refusing formal board test: build or vector generation changed tracked files."
    }

    Remove-Item -LiteralPath $doneFlag -Force -ErrorAction SilentlyContinue
    Get-ChildItem -Path (Join-Path $root "tmp") -Filter "board_missing_tests_audio_*" -ErrorAction SilentlyContinue |
        Remove-Item -Force -ErrorAction SilentlyContinue

    $player = Start-Job -ScriptBlock {
        param($prefix, $wave, $done)
        $request = 1
        while ($true) {
            $ready = "{0}{1}.ready" -f $prefix, $request
            $playing = "{0}{1}.playing" -f $prefix, $request
            if (Test-Path -LiteralPath $ready) {
                Set-Content -LiteralPath $playing -Value "started $(Get-Date -Format o)" -NoNewline
                $sound = New-Object System.Media.SoundPlayer $wave
                $sound.PlaySync()
                $request++
            } elseif (Test-Path -LiteralPath $done) {
                break
            } else {
                Start-Sleep -Milliseconds 50
            }
        }
    } -ArgumentList $syncPrefix, $audio, $doneFlag

    $env:DSP_TEST_COMMIT_SHA = $sha
    $env:DSP_TEST_AUDIO_FILE = $audio
    $env:DSP_TEST_AUDIO_SYNC_PREFIX = $syncPrefix
    $env:DSP_TEST_INPUT_GAIN = $InputGain
    $env:DSP_TEST_PLAYBACK_VOLUME = $PlaybackVolume
    $env:DSP_TEST_RECORD_GAIN = $RecordGain
    & $dss "tools\dss\dss_board_missing_tests.js"
    if ($LASTEXITCODE -ne 0) { throw "DSS board missing-tests rerun failed." }

    Set-Content -LiteralPath $doneFlag -Value "done $(Get-Date -Format o)" -NoNewline
    Receive-Job -Job $player -Wait -AutoRemoveJob
    $player = $null

    & $python "tools\process_board_missing_tests.py"
    if ($LASTEXITCODE -ne 0) { throw "Board missing-tests report generation failed." }
}
finally {
    Set-Content -LiteralPath $doneFlag -Value "done $(Get-Date -Format o)" -NoNewline -ErrorAction SilentlyContinue
    if ($null -ne $player) {
        Stop-Job -Job $player -ErrorAction SilentlyContinue
        Remove-Job -Job $player -Force -ErrorAction SilentlyContinue
    }
    Pop-Location
}
