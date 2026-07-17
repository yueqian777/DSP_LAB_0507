param(
    [string]$ResultDir = "",
    [string]$Program = "",
    [string]$SerialPort = "COM7",
    [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$ccsRoot = "D:\SoftwareDownload\CCS_20.5.0.00028_win\ccs"
$gmake = Join-Path $ccsRoot "utils\bin\gmake.exe"
$dss = Join-Path $ccsRoot "ccs_base\scripting\bin\dss.bat"
$js = Join-Path $PSScriptRoot "dss\dss_dynamic_clarity_continuous.js"
$generator = Join-Path $PSScriptRoot "generate_dynamic_clarity_audio.py"
$buildIdGenerator = Join-Path $PSScriptRoot "generate_equalizer_build_id.ps1"
$ccxml = Join-Path $root "TargetConfig\C6748.ccxml"

if ([string]::IsNullOrWhiteSpace($ResultDir)) {
    $stamp = Get-Date -Format "yyyyMMdd_HHmmss"
    $ResultDir = Join-Path $env:TEMP `
        "DSP_LAB_0507\dynamic_clarity_continuous\$stamp"
}
$resultPath = [IO.Path]::GetFullPath($ResultDir)
$tempRoot = [IO.Path]::GetFullPath($env:TEMP).TrimEnd('\') + '\'
if (-not $resultPath.StartsWith(
    $tempRoot, [StringComparison]::OrdinalIgnoreCase)) {
    throw "ResultDir must stay under the current TEMP directory"
}

$audioDir = Join-Path $resultPath "audio"
$syncDir = Join-Path $resultPath "audio_sync"
$serialLog = Join-Path $resultPath "serial.log"
$serialReady = Join-Path $resultPath "serial.ready"
$stopFile = Join-Path $resultPath "stop"
$stdout = Join-Path $resultPath "dss_stdout.log"
$stderr = Join-Path $resultPath "dss_stderr.log"
$boardPath = Join-Path $resultPath `
    "dynamic_clarity_continuous_board.json"
$serialJob = $null
$audioJob = $null
$dssExitCode = $null

New-Item -ItemType Directory -Force -Path $resultPath,$audioDir,$syncDir |
    Out-Null
Remove-Item -LiteralPath $serialLog,$serialReady,$stopFile,$stdout,$stderr,
    $boardPath -Force -ErrorAction SilentlyContinue
Get-ChildItem -LiteralPath $syncDir -File -ErrorAction SilentlyContinue |
    Remove-Item -Force

$required = @($dss,$js,$generator,$ccxml)
if (-not $SkipBuild) {
    $required += @($gmake,$buildIdGenerator)
}
foreach ($path in $required) {
    if (-not (Test-Path -LiteralPath $path)) {
        throw "Missing required path: $path"
    }
}
if ([IO.Ports.SerialPort]::GetPortNames() -notcontains $SerialPort) {
    throw "Serial port is not present: $SerialPort"
}

& python $generator $audioDir
if ($LASTEXITCODE -ne 0) {
    throw "Audio stimulus generation failed with exit code $LASTEXITCODE"
}
$musicWave = Join-Path $audioDir "music_like.wav"
if (-not (Test-Path -LiteralPath $musicWave)) {
    throw "Music-like stimulus was not generated: $musicWave"
}

$defines = @(
    "--define=DSP_LAB_PROJECT_SELECT=33",
    "--define=EQ_ENABLE_LCD_DISPLAY=0",
    "--define=EQ_ENABLE_AUDIO_FEATURE_ANALYZER=1",
    "--define=EQ_ENABLE_SMART_BASS=1",
    "--define=EQ_ENABLE_DYNAMIC_CLARITY=1",
    "--define=EQ_ENABLE_DYNAMIC_CLARITY_TIMING_DIAGNOSTICS=1",
    "--define=EQ_ENABLE_DYNAMIC_CLARITY_TRANSITION_CAPTURE=0",
    "--define=EQ_ENABLE_DYNAMIC_CLARITY_BENCHMARK=0",
    "--define=EQ_USE_GENERATED_BUILD_ID=1"
) -join ' '

if (-not $SkipBuild) {
    & $buildIdGenerator | Out-Host
    & $gmake -B -C (Join-Path $root "Debug") all `
        "GEN_OPTS__FLAG=$defines"
    if ($LASTEXITCODE -ne 0) {
        throw "CCS continuous-test build failed with exit code $LASTEXITCODE"
    }
    $Program = Join-Path $root "Debug\DSP_LAB_0507.out"
}
if ([string]::IsNullOrWhiteSpace($Program) -or
    (-not (Test-Path -LiteralPath $Program))) {
    throw "Continuous-test program is missing: $Program"
}

$programCopy = Join-Path $resultPath "DSP_LAB_0507_continuous.out"
Copy-Item -LiteralPath $Program -Destination $programCopy -Force
$outputHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $programCopy).Hash

try {
    $serialJob = Start-Job -ScriptBlock {
        param($portName, $logPath, $readyPath, $stopPath)
        $port = [IO.Ports.SerialPort]::new(
            $portName, 115200, [IO.Ports.Parity]::None, 8,
            [IO.Ports.StopBits]::One)
        $port.ReadTimeout = 100
        try {
            $port.Open()
            [IO.File]::WriteAllText(
                $readyPath, "ready", [Text.Encoding]::ASCII)
            while (-not (Test-Path -LiteralPath $stopPath)) {
                $text = $port.ReadExisting()
                if ($text.Length -gt 0) {
                    [IO.File]::AppendAllText(
                        $logPath, $text, [Text.Encoding]::ASCII)
                }
                Start-Sleep -Milliseconds 10
            }
            $text = $port.ReadExisting()
            if ($text.Length -gt 0) {
                [IO.File]::AppendAllText(
                    $logPath, $text, [Text.Encoding]::ASCII)
            }
        }
        finally {
            if ($port.IsOpen) { $port.Close() }
            $port.Dispose()
        }
    } -ArgumentList $SerialPort,$serialLog,$serialReady,$stopFile

    $waited = 0
    while ((-not (Test-Path -LiteralPath $serialReady)) -and
        $waited -lt 5000) {
        Start-Sleep -Milliseconds 50
        $waited += 50
    }
    if (-not (Test-Path -LiteralPath $serialReady)) {
        throw "Serial monitor did not open $SerialPort"
    }

    $audioJob = Start-Job -ScriptBlock {
        param($syncPath, $stopPath, $wavePath)
        $player = $null
        $ready = Join-Path $syncPath "music_like.ready"
        $playing = Join-Path $syncPath "music_like.playing"
        try {
            while ((-not (Test-Path -LiteralPath $ready)) -and
                (-not (Test-Path -LiteralPath $stopPath))) {
                Start-Sleep -Milliseconds 20
            }
            if (-not (Test-Path -LiteralPath $stopPath)) {
                $player = [Media.SoundPlayer]::new($wavePath)
                $player.Load()
                $player.PlayLooping()
                Set-Content -LiteralPath $playing -Value "playing=music_like" `
                    -NoNewline
                while (-not (Test-Path -LiteralPath $stopPath)) {
                    Start-Sleep -Milliseconds 100
                }
            }
        }
        finally {
            if ($null -ne $player) {
                $player.Stop()
                $player.Dispose()
            }
        }
    } -ArgumentList $syncDir,$stopFile,$musicWave

    $env:DSP_TEST_ROOT = ($root -replace '\\','/')
    $env:DSP_TEST_CCXML = ($ccxml -replace '\\','/')
    $env:DSP_TEST_PROGRAM = ($programCopy -replace '\\','/')
    $env:DSP_TEST_RESULT_DIR = ($resultPath -replace '\\','/')
    $env:DSP_TEST_AUDIO_SYNC_DIR = ($syncDir -replace '\\','/')
    $env:DSP_TEST_OUTPUT_SHA256 = $outputHash

    $command = 'call "{0}" "{1}"' -f $dss,$js
    $process = Start-Process -FilePath $env:ComSpec `
        -ArgumentList @('/d','/s','/c',$command) -PassThru `
        -WindowStyle Hidden -RedirectStandardOutput $stdout `
        -RedirectStandardError $stderr
    if (-not $process.WaitForExit(480000)) {
        & taskkill.exe /PID $process.Id /T /F | Out-Null
        throw "DSS continuous test timed out"
    }
    $process.Refresh()
    $dssExitCode = $process.ExitCode

    $board = $null
    if (Test-Path -LiteralPath $boardPath) {
        try {
            $board = Get-Content -Raw -LiteralPath $boardPath |
                ConvertFrom-Json
        }
        catch {
            $board = $null
        }
    }
    $validPassJson = ($null -ne $board) -and [bool]$board.pass
    if (($dssExitCode -ne 0) -and (-not $validPassJson)) {
        $details = if (Test-Path -LiteralPath $stderr) {
            Get-Content -Raw -LiteralPath $stderr
        } else { "" }
        throw "DSS continuous test failed: $details"
    }
    if (-not $validPassJson) {
        $errorText = if ($null -ne $board) { $board.error } else { "" }
        throw "Board continuous result failed: $errorText"
    }
    if (($audioJob.State -ne "Running") -or
        ($serialJob.State -ne "Running")) {
        throw "Audio or UART background job stopped before test completion"
    }
}
finally {
    Set-Content -LiteralPath $stopFile -Value "stop" -NoNewline `
        -ErrorAction SilentlyContinue
    foreach ($job in @($audioJob,$serialJob)) {
        if ($null -ne $job) {
            Wait-Job $job -Timeout 5 | Out-Null
            Stop-Job $job -ErrorAction SilentlyContinue
            Remove-Job $job -Force -ErrorAction SilentlyContinue
        }
    }
    Remove-Item Env:DSP_TEST_ROOT,Env:DSP_TEST_CCXML,
        Env:DSP_TEST_PROGRAM,Env:DSP_TEST_RESULT_DIR,
        Env:DSP_TEST_AUDIO_SYNC_DIR,Env:DSP_TEST_OUTPUT_SHA256 `
        -ErrorAction SilentlyContinue
}

$serialText = if (Test-Path -LiteralPath $serialLog) {
    Get-Content -Raw -LiteralPath $serialLog
} else { "" }
if ($serialText -notmatch 'P33 INIT 11') {
    throw "UART log does not contain P33 INIT 11"
}

$board = Get-Content -Raw -LiteralPath $boardPath | ConvertFrom-Json
[pscustomobject]@{
    pass = [bool]$board.pass
    evidence_label = "BOARD_DYNAMIC_CLARITY_CONTINUOUS_300S"
    result_dir = $resultPath
    board_result = $boardPath
    serial_log = $serialLog
    program = $programCopy
    output_sha256 = $outputHash
    dss_exit_code = $dssExitCode
    pc_monotonic_seconds = $board.elapsed.pc_monotonic_seconds
    dsp_audio_seconds = $board.elapsed.dsp_audio_seconds
    subjective_observation = "NOT_PERFORMED"
} | ConvertTo-Json -Depth 4
