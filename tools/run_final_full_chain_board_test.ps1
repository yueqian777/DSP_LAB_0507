param(
    [string]$InputGain = "NOT_MEASURED_OPERATOR_UNAVAILABLE",
    [string]$PlaybackVolume = "NOT_MEASURED_OPERATOR_UNAVAILABLE",
    [string]$RecordGain = "NOT_MEASURED_AUDIO_CAPTURE_UNAVAILABLE"
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$dss = "D:\SoftwareDownload\CCS_20.5.0.00028_win\ccs\ccs_base\scripting\bin\dss.bat"
$gmake = "D:\SoftwareDownload\CCS_20.5.0.00028_win\ccs\utils\bin\gmake.exe"
$python = "C:\Users\zhangyueqian\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe"
$audio = Join-Path $root "test_vectors\final_full_chain_2s_noise_speech_stationary_50k.wav"
$readyPrefix = Join-Path $root "tmp\final_full_chain_repeat"

Push-Location $root
try {
    if (git status --porcelain) {
        throw "Refusing formal board test: git status is not clean."
    }
    & $python "tools\create_final_full_chain_test_vector.py"
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
    if (git status --porcelain) {
        throw "Refusing formal board test: build changed tracked files."
    }
    1..3 | ForEach-Object {
        Remove-Item -LiteralPath ("{0}{1}.ready" -f $readyPrefix, $_) -Force -ErrorAction SilentlyContinue
        Remove-Item -LiteralPath ("{0}{1}.playing" -f $readyPrefix, $_) -Force -ErrorAction SilentlyContinue
    }
    $player = Start-Job -ScriptBlock {
        param($prefix, $wave)
        for ($repeat = 1; $repeat -le 3; $repeat++) {
            $ready = "{0}{1}.ready" -f $prefix, $repeat
            $playing = "{0}{1}.playing" -f $prefix, $repeat
            $deadline = (Get-Date).AddSeconds(90)
            while ((-not (Test-Path -LiteralPath $ready)) -and ((Get-Date) -lt $deadline)) {
                Start-Sleep -Milliseconds 50
            }
            if (-not (Test-Path -LiteralPath $ready)) { throw "Audio-ready timeout for repeat $repeat" }
            Set-Content -LiteralPath $playing -Value "started $(Get-Date -Format o)" -NoNewline
            $sound = New-Object System.Media.SoundPlayer $wave
            $sound.PlaySync()
        }
    } -ArgumentList $readyPrefix, $audio

    $env:DSP_TEST_COMMIT_SHA = $sha
    $env:DSP_TEST_AUDIO_FILE = $audio
    $env:DSP_TEST_INPUT_GAIN = $InputGain
    $env:DSP_TEST_PLAYBACK_VOLUME = $PlaybackVolume
    $env:DSP_TEST_RECORD_GAIN = $RecordGain
    & $dss "tools\dss\dss_board_missing_tests.js"
    if ($LASTEXITCODE -ne 0) { throw "DSS board test failed." }
    Receive-Job -Job $player -Wait -AutoRemoveJob
    & $python "tools\final_full_chain_240_report.py"
    if ($LASTEXITCODE -ne 0) { throw "Final report generation failed." }
}
finally {
    Pop-Location
}
