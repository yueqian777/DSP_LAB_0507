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
$js = Join-Path $PSScriptRoot "dss\dss_dynamic_clarity_transition.js"
$generator = Join-Path $PSScriptRoot "generate_dynamic_clarity_audio.py"
$analyzer = Join-Path $PSScriptRoot "analyze_dynamic_clarity_transition.py"
$buildIdGenerator = Join-Path $PSScriptRoot "generate_equalizer_build_id.ps1"
$ccxml = Join-Path $root "TargetConfig\C6748.ccxml"

if ([string]::IsNullOrWhiteSpace($ResultDir)) {
    $stamp = Get-Date -Format "yyyyMMdd_HHmmss"
    $ResultDir = Join-Path $env:TEMP `
        "DSP_LAB_0507\dynamic_clarity_transition\$stamp"
}
$resultPath = [IO.Path]::GetFullPath($ResultDir)
$tempRoot = [IO.Path]::GetFullPath($env:TEMP).TrimEnd('\') + '\'
if (-not $resultPath.StartsWith(
    $tempRoot, [StringComparison]::OrdinalIgnoreCase)) {
    throw "ResultDir must stay under the current TEMP directory"
}
$audioDir = Join-Path $resultPath "audio"
$syncDir = Join-Path $resultPath "audio_sync"
New-Item -ItemType Directory -Force -Path $resultPath,$audioDir,$syncDir |
    Out-Null

foreach ($required in @(
    $gmake,$dss,$js,$generator,$analyzer,$buildIdGenerator,$ccxml
)) {
    if (-not (Test-Path -LiteralPath $required)) {
        throw "Missing required path: $required"
    }
}
if ([IO.Ports.SerialPort]::GetPortNames() -notcontains $SerialPort) {
    throw "Serial port is not present: $SerialPort"
}

& python $generator $audioDir
if ($LASTEXITCODE -ne 0) {
    throw "Audio stimulus generation failed"
}
$manifest = Get-Content -Raw -LiteralPath `
    (Join-Path $audioDir "audio_manifest.json") | ConvertFrom-Json
$waves = @{}
foreach ($name in @("transition_dual","transition_music","transition_noise")) {
    $waves[$name] = Join-Path $audioDir "$name.wav"
}

$defines = @(
    "--define=DSP_LAB_PROJECT_SELECT=33",
    "--define=EQ_ENABLE_LCD_DISPLAY=0",
    "--define=EQ_ENABLE_AUDIO_FEATURE_ANALYZER=1",
    "--define=EQ_ENABLE_SMART_BASS=1",
    "--define=EQ_ENABLE_DYNAMIC_CLARITY=1",
    "--define=EQ_ENABLE_DYNAMIC_CLARITY_TIMING_DIAGNOSTICS=1",
    "--define=EQ_ENABLE_DYNAMIC_CLARITY_TRANSITION_CAPTURE=1",
    "--define=EQ_ENABLE_DYNAMIC_CLARITY_BENCHMARK=0",
    "--define=EQ_USE_GENERATED_BUILD_ID=1"
) -join ' '

if (-not $SkipBuild) {
    & $buildIdGenerator | Out-Host
    $objects = @(
        (Join-Path $root "Debug\Code\User\user_dsp\user_dynamic_clarity.obj"),
        (Join-Path $root "Debug\Code\User\user_dsp\user_dynamic_clarity_benchmark.obj"),
        (Join-Path $root "Debug\Code\User\user_dsp\user_equalizer_flow.obj")
    )
    Remove-Item -LiteralPath $objects -Force -ErrorAction SilentlyContinue
    & $gmake -C (Join-Path $root "Debug") all `
        "GEN_OPTS__FLAG=$defines"
    if ($LASTEXITCODE -ne 0) {
        throw "CCS transition build failed with exit code $LASTEXITCODE"
    }
    $Program = Join-Path $root "Debug\DSP_LAB_0507.out"
}
if ([string]::IsNullOrWhiteSpace($Program) -or
    (-not (Test-Path -LiteralPath $Program))) {
    throw "Transition program is missing: $Program"
}

$programCopy = Join-Path $resultPath "DSP_LAB_0507_transition.out"
Copy-Item -LiteralPath $Program -Destination $programCopy -Force
$outputHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $programCopy).Hash
$serialLog = Join-Path $resultPath "serial.log"
$serialReady = Join-Path $resultPath "serial.ready"
$stopFile = Join-Path $resultPath "stop"
$stdout = Join-Path $resultPath "dss_stdout.log"
$stderr = Join-Path $resultPath "dss_stderr.log"
$serialJob = $null
$audioJob = $null

try {
    $serialJob = Start-Job -ScriptBlock {
        param($portName, $logPath, $readyPath, $stopPath)
        $port = [IO.Ports.SerialPort]::new(
            $portName, 115200, [IO.Ports.Parity]::None, 8,
            [IO.Ports.StopBits]::One)
        $port.ReadTimeout = 100
        try {
            $port.Open()
            [IO.File]::WriteAllText($readyPath, "ready", [Text.Encoding]::ASCII)
            while (-not (Test-Path -LiteralPath $stopPath)) {
                $text = $port.ReadExisting()
                if ($text.Length -gt 0) {
                    [IO.File]::AppendAllText(
                        $logPath, $text, [Text.Encoding]::ASCII)
                }
                Start-Sleep -Milliseconds 10
            }
        }
        finally {
            if ($port.IsOpen) { $port.Close() }
            $port.Dispose()
        }
    } -ArgumentList $SerialPort,$serialLog,$serialReady,$stopFile

    $waited = 0
    while ((-not (Test-Path -LiteralPath $serialReady)) -and $waited -lt 5000) {
        Start-Sleep -Milliseconds 50
        $waited += 50
    }
    if (-not (Test-Path -LiteralPath $serialReady)) {
        throw "Serial monitor did not open $SerialPort"
    }

    $audioJob = Start-Job -ScriptBlock {
        param($syncPath, $stopPath, $waveTable)
        $player = $null
        try {
            while (-not (Test-Path -LiteralPath $stopPath)) {
                foreach ($name in $waveTable.Keys) {
                    $ready = Join-Path $syncPath "$name.ready"
                    $playing = Join-Path $syncPath "$name.playing"
                    if ((Test-Path -LiteralPath $ready) -and
                        (-not (Test-Path -LiteralPath $playing))) {
                        if ($null -ne $player) {
                            $player.Stop()
                            $player.Dispose()
                        }
                        $player = [Media.SoundPlayer]::new($waveTable[$name])
                        $player.Load()
                        $player.PlayLooping()
                        Set-Content -LiteralPath $playing -Value "playing" -NoNewline
                    }
                }
                Start-Sleep -Milliseconds 20
            }
        }
        finally {
            if ($null -ne $player) {
                $player.Stop()
                $player.Dispose()
            }
        }
    } -ArgumentList $syncDir,$stopFile,$waves

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
    if (-not $process.WaitForExit(600000)) {
        & taskkill.exe /PID $process.Id /T /F | Out-Null
        throw "DSS transition capture timed out"
    }
    $process.Refresh()
    if ($process.ExitCode -ne 0) {
        $boardPath = Join-Path $resultPath `
            "dynamic_clarity_transition_board.json"
        $completed = $false
        if (Test-Path -LiteralPath $boardPath) {
            try {
                $completed = [bool]((Get-Content -Raw -LiteralPath $boardPath |
                    ConvertFrom-Json).pass)
            }
            catch {
                $completed = $false
            }
        }
        if (-not $completed) {
            $details = if (Test-Path -LiteralPath $stderr) {
                Get-Content -Raw -LiteralPath $stderr
            } else { "" }
            throw "DSS transition capture failed: $details"
        }
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

$boardPath = Join-Path $resultPath "dynamic_clarity_transition_board.json"
$board = Get-Content -Raw -LiteralPath $boardPath | ConvertFrom-Json
if (-not $board.pass) {
    throw "Board transition result failed: $($board.error)"
}
$serialText = Get-Content -Raw -LiteralPath $serialLog
if ($serialText -notmatch 'P33 INIT 11') {
    throw "UART log does not contain P33 INIT 11"
}

& python $analyzer $resultPath
if ($LASTEXITCODE -ne 0) {
    throw "Transition analysis failed with exit code $LASTEXITCODE"
}

[pscustomobject]@{
    pass = $true
    evidence_label = "BOARD_INTERNAL_PCM_OBJECTIVE_TRANSIENT"
    result_dir = $resultPath
    report = Join-Path $resultPath "dynamic_clarity_transition_artifact.json"
    timing = $boardPath
    output_sha256 = $outputHash
    subjective_observation = "NOT_PERFORMED"
} | ConvertTo-Json -Depth 4
