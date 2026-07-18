param(
    [string]$Program = "",
    [Parameter(Mandatory = $true)]
    [string]$ResultDir,
    [string]$SerialPort = "COM7",
    [string]$ExpectedSha = "",
    [switch]$AnalyzeOnly,
    [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$ccsRoot = "D:\SoftwareDownload\CCS_20.5.0.00028_win\ccs"
$gmake = Join-Path $ccsRoot "utils\bin\gmake.exe"
$dss = Join-Path $ccsRoot "ccs_base\scripting\bin\dss.bat"
$js = Join-Path $PSScriptRoot "dss\dss_harshness_guard_board.js"
$haltJs = Join-Path $PSScriptRoot "dss\dss_halt_board.js"
$generator = Join-Path $PSScriptRoot "generate_harshness_guard_audio.py"
$analyzer = Join-Path $PSScriptRoot "analyze_harshness_guard_raw.py"
$buildIdGenerator = Join-Path $PSScriptRoot "generate_equalizer_build_id.ps1"
$ccxml = Join-Path $root "TargetConfig\C6748.ccxml"
$resultPath = [IO.Path]::GetFullPath($ResultDir)
$tempPath = [IO.Path]::GetFullPath($env:TEMP).TrimEnd('\') + '\'
if (-not $resultPath.StartsWith(
    $tempPath, [StringComparison]::OrdinalIgnoreCase)) {
    throw "ResultDir must stay under the current TEMP directory"
}

$audioDir = Join-Path $resultPath "audio"
$sync = Join-Path $resultPath "audio_sync"
$serialLog = Join-Path $resultPath "serial.log"
$serialReady = Join-Path $resultPath "serial.ready"
$stop = Join-Path $resultPath "stop"
$stdout = Join-Path $resultPath "dss_stdout.log"
$stderr = Join-Path $resultPath "dss_stderr.log"
$haltStdout = Join-Path $resultPath "halt_stdout.log"
$haltStderr = Join-Path $resultPath "halt_stderr.log"
$serialJob = $null
$audioJob = $null
$dssProcess = $null

if ([string]::IsNullOrWhiteSpace($ExpectedSha)) {
    $ExpectedSha = (git -C $root rev-parse --short HEAD).Trim()
    if ($LASTEXITCODE -ne 0) {
        throw "Unable to resolve the current Git SHA"
    }
}

if ($AnalyzeOnly) {
    $boardResultPath = Join-Path $resultPath "board_result.json"
    if (-not (Test-Path -LiteralPath $boardResultPath)) {
        throw "AnalyzeOnly requires board_result.json"
    }
    $boardResult = Get-Content -Raw -LiteralPath $boardResultPath |
        ConvertFrom-Json
    if (-not $boardResult.pass) {
        throw "Board result is not passing: $($boardResult.error)"
    }
    if (-not (Test-Path -LiteralPath $serialLog)) {
        throw "AnalyzeOnly requires serial.log"
    }
    $serialText = Get-Content -Raw -LiteralPath $serialLog
    if ($serialText -notmatch [Regex]::Escape("P33 BUILD $ExpectedSha")) {
        throw "UART log does not contain the expected P33 BUILD record"
    }
    if ($serialText -notmatch 'P33 INIT 11') {
        throw "UART log does not contain P33 INIT 11"
    }
    & python $analyzer $resultPath
    if ($LASTEXITCODE -ne 0) {
        throw "RAW analysis failed with exit code $LASTEXITCODE"
    }
    [pscustomobject]@{
        pass = $true
        mode = "ANALYZE_ONLY"
        expected_sha = $ExpectedSha
        result_dir = $resultPath
        board_result = $boardResultPath
        capture_metrics = Join-Path $resultPath "capture_metrics.json"
        serial_log = $serialLog
        subjective_observation = "NOT_PERFORMED"
    } | ConvertTo-Json -Depth 4
    return
}

function Start-DssProcess {
    param(
        [Parameter(Mandatory = $true)][string]$ScriptPath,
        [Parameter(Mandatory = $true)][string]$OutPath,
        [Parameter(Mandatory = $true)][string]$ErrorPath
    )

    $command = 'call "{0}" "{1}"' -f $dss,$ScriptPath
    Start-Process -FilePath $env:ComSpec -ArgumentList @('/d','/s','/c',$command) `
        -PassThru -WindowStyle Hidden -RedirectStandardOutput $OutPath `
        -RedirectStandardError $ErrorPath
}

function Invoke-EmergencyHalt {
    Remove-Item -LiteralPath $haltStdout,$haltStderr -Force `
        -ErrorAction SilentlyContinue
    $process = Start-DssProcess -ScriptPath $haltJs -OutPath $haltStdout `
        -ErrorPath $haltStderr
    if (-not $process.WaitForExit(30000)) {
        & taskkill.exe /PID $process.Id /T /F | Out-Null
        return $false
    }
    $process.Refresh()
    return ($process.ExitCode -eq 0)
}

if (-not $SkipBuild) {
    $dirtyPaths = git -C $root status --porcelain --untracked-files=normal
    if ($LASTEXITCODE -ne 0 -or
        -not [string]::IsNullOrWhiteSpace(($dirtyPaths -join "`n"))) {
        throw "Harshness Guard board validation requires a clean worktree"
    }
    & $buildIdGenerator | Out-Host
    $defines = @(
        "--define=DSP_LAB_PROJECT_SELECT=33",
        "--define=EQ_USE_GENERATED_BUILD_ID=1",
        "--define=EQ_ENABLE_LCD_DISPLAY=0",
        "--define=EQ_ENABLE_AUDIO_FEATURE_ANALYZER=1",
        "--define=EQ_ENABLE_SMART_BASS=1",
        "--define=EQ_ENABLE_DYNAMIC_CLARITY=1",
        "--define=EQ_ENABLE_HARSHNESS_GUARD=1",
        "--define=EQ_ENABLE_DYNAMIC_CLARITY_TIMING_DIAGNOSTICS=0",
        "--define=EQ_ENABLE_DYNAMIC_CLARITY_TRANSITION_CAPTURE=0",
        "--define=EQ_ENABLE_HARSHNESS_GUARD_TRANSITION_CAPTURE=0",
        "--define=EQ_ENABLE_FOUR_WAY_TRANSITION_DIAGNOSTICS=1",
        "--define=EQ_ENABLE_DYNAMIC_CLARITY_BENCHMARK=0",
        "--define=EQ_ENABLE_HARSHNESS_GUARD_BENCHMARK=0"
    ) -join ' '
    & $gmake -B -C (Join-Path $root "Debug") all `
        "GEN_OPTS__FLAG=$defines"
    if ($LASTEXITCODE -ne 0) {
        throw "CCS Harshness Guard board build failed with exit code $LASTEXITCODE"
    }
    $Program = Join-Path $root "Debug\DSP_LAB_0507.out"
}
if ([string]::IsNullOrWhiteSpace($Program)) {
    throw "Program is required when SkipBuild is selected"
}
foreach ($required in @(
    $Program,$gmake,$dss,$js,$haltJs,$generator,$analyzer,
    $buildIdGenerator,$ccxml
)) {
    if (-not (Test-Path -LiteralPath $required)) {
        throw "Missing required path: $required"
    }
}
if ([IO.Ports.SerialPort]::GetPortNames() -notcontains $SerialPort) {
    throw "Serial port is not present: $SerialPort"
}

New-Item -ItemType Directory -Force -Path $resultPath,$audioDir,$sync | Out-Null
$stale = @(
    $serialLog,$serialReady,$stop,$stdout,$stderr,$haltStdout,$haltStderr,
    (Join-Path $resultPath "board_result.json"),
    (Join-Path $resultPath "capture_metrics.json")
)
Remove-Item -LiteralPath $stale -Force -ErrorAction SilentlyContinue
Get-ChildItem -LiteralPath $resultPath -File -Filter "*.raw" `
    -ErrorAction SilentlyContinue | Remove-Item -Force
Get-ChildItem -LiteralPath $sync -File -ErrorAction SilentlyContinue |
    Remove-Item -Force

& python $generator $audioDir
if ($LASTEXITCODE -ne 0) {
    throw "Audio stimulus generation failed with exit code $LASTEXITCODE"
}
$manifest = Get-Content -Raw -LiteralPath `
    (Join-Path $audioDir "audio_manifest.json") | ConvertFrom-Json
$waves = @{}
foreach ($file in $manifest.files) {
    $name = [IO.Path]::GetFileNameWithoutExtension($file.file_name)
    $waves[$name] = Join-Path $audioDir $file.file_name
}

try {
    $serialJob = Start-Job -ScriptBlock {
        param($portName, $logPath, $readyPath, $stopPath)
        $port = [IO.Ports.SerialPort]::new(
            $portName, 115200, [IO.Ports.Parity]::None, 8,
            [IO.Ports.StopBits]::One)
        $port.ReadTimeout = 100
        try {
            $port.Open()
            [IO.File]::WriteAllText($logPath, "", [Text.Encoding]::ASCII)
            [IO.File]::WriteAllText($readyPath, "ready", [Text.Encoding]::ASCII)
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
                [IO.File]::AppendAllText($logPath, $text, [Text.Encoding]::ASCII)
            }
        }
        finally {
            if ($port.IsOpen) { $port.Close() }
            $port.Dispose()
        }
    } -ArgumentList $SerialPort,$serialLog,$serialReady,$stop

    $waited = 0
    while ((-not (Test-Path -LiteralPath $serialReady)) -and $waited -lt 5000) {
        Start-Sleep -Milliseconds 50
        $waited += 50
    }
    if (-not (Test-Path -LiteralPath $serialReady)) {
        $details = Receive-Job $serialJob -Keep -ErrorAction SilentlyContinue |
            Out-String
        throw "Serial monitor did not open $SerialPort. $details"
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
                        Set-Content -LiteralPath $playing `
                            -Value "playing=$name" -NoNewline
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
    } -ArgumentList $sync,$stop,$waves

    $env:DSP_TEST_ROOT = ($root -replace '\\','/')
    $env:DSP_TEST_CCXML = ($ccxml -replace '\\','/')
    $env:DSP_TEST_PROGRAM = ($Program -replace '\\','/')
    $env:DSP_TEST_RESULT_DIR = ($resultPath -replace '\\','/')
    $env:DSP_TEST_AUDIO_SYNC_DIR = ($sync -replace '\\','/')
    $env:DSP_TEST_EXPECTED_SHA = $ExpectedSha

    $dssProcess = Start-DssProcess -ScriptPath $js -OutPath $stdout `
        -ErrorPath $stderr
    $deadline = [DateTime]::UtcNow.AddMinutes(15)
    while (-not $dssProcess.HasExited) {
        if ([DateTime]::UtcNow -ge $deadline) {
            & taskkill.exe /PID $dssProcess.Id /T /F | Out-Null
            Start-Sleep -Milliseconds 500
            $haltRecovered = Invoke-EmergencyHalt
            throw "DSS objective run timed out; emergency_halt=$haltRecovered"
        }
        if (($audioJob.State -ne "Running") -or
            ($serialJob.State -ne "Running")) {
            & taskkill.exe /PID $dssProcess.Id /T /F | Out-Null
            Start-Sleep -Milliseconds 500
            $haltRecovered = Invoke-EmergencyHalt
            throw "Stimulus or UART job stopped; emergency_halt=$haltRecovered"
        }
        Start-Sleep -Milliseconds 250
    }
    $dssProcess.WaitForExit()
    $dssProcess.Refresh()
    $dssExitCode = $dssProcess.ExitCode
    if (($null -ne $dssExitCode) -and ($dssExitCode -ne 0)) {
        $details = if (Test-Path -LiteralPath $stderr) {
            Get-Content -Raw -LiteralPath $stderr
        } else { "" }
        throw "DSS objective run exited ${dssExitCode}: $details"
    }
    $boardResultPath = Join-Path $resultPath "board_result.json"
    if (-not (Test-Path -LiteralPath $boardResultPath)) {
        throw "DSS produced no board_result.json"
    }
    $boardResult = Get-Content -Raw -LiteralPath $boardResultPath |
        ConvertFrom-Json
    if (-not $boardResult.pass) {
        throw "DSS objective checks failed: $($boardResult.error)"
    }
}
finally {
    Set-Content -LiteralPath $stop -Value "stop" -NoNewline `
        -ErrorAction SilentlyContinue
    if (($null -ne $dssProcess) -and (-not $dssProcess.HasExited)) {
        & taskkill.exe /PID $dssProcess.Id /T /F | Out-Null
    }
    foreach ($job in @($audioJob,$serialJob)) {
        if ($null -ne $job) {
            Wait-Job $job -Timeout 5 | Out-Null
            Stop-Job $job -ErrorAction SilentlyContinue
            Remove-Job $job -Force -ErrorAction SilentlyContinue
        }
    }
    Remove-Item Env:DSP_TEST_ROOT,Env:DSP_TEST_CCXML,Env:DSP_TEST_PROGRAM,
        Env:DSP_TEST_RESULT_DIR,Env:DSP_TEST_AUDIO_SYNC_DIR,
        Env:DSP_TEST_EXPECTED_SHA -ErrorAction SilentlyContinue
}

if ((-not (Test-Path -LiteralPath $serialLog)) -or
    ((Get-Item -LiteralPath $serialLog).Length -eq 0)) {
    throw "UART capture is empty"
}
$serialText = [IO.File]::ReadAllText($serialLog, [Text.Encoding]::ASCII)
if ($serialText -notmatch [Regex]::Escape("P33 BUILD $ExpectedSha")) {
    throw "UART log does not contain the expected P33 BUILD record"
}
if ($serialText -notmatch 'P33 INIT 11') {
    throw "UART log does not contain P33 INIT 11"
}

& python $analyzer $resultPath
if ($LASTEXITCODE -ne 0) {
    throw "RAW analysis failed with exit code $LASTEXITCODE"
}

[pscustomobject]@{
    pass = $true
    expected_sha = $ExpectedSha
    result_dir = $resultPath
    board_result = Join-Path $resultPath "board_result.json"
    capture_metrics = Join-Path $resultPath "capture_metrics.json"
    serial_log = $serialLog
    subjective_observation = "NOT_PERFORMED"
} | ConvertTo-Json -Depth 4
