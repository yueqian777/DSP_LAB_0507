param(
    [string]$RepoRoot = "",
    [string]$CcsRoot = $env:CCS_ROOT,
    [string]$PythonPath = $env:DSP_TEST_PYTHON,
    [string]$DssPath = "",
    [string]$GmakePath = "",
    [ValidateSet("Tone", "MusicLike")]
    [string]$AudioKind = "MusicLike",
    [int]$RowSeconds = 60,
    [int]$StabilitySeconds = 300,
    [ValidateSet("PASS", "FAIL", "NOT_OBSERVED")]
    [string]$OperatorStatus = "NOT_OBSERVED",
    [string]$OperatorNotes = "",
    [ValidateRange(1, 120)]
    [int]$DssTimeoutMinutes = 20
)

$ErrorActionPreference = "Stop"
$root = if ($RepoRoot) { (Resolve-Path $RepoRoot).Path } else { Split-Path -Parent $PSScriptRoot }
if (-not $CcsRoot -and (-not $DssPath -or -not $GmakePath)) {
    throw "Set -CcsRoot or CCS_ROOT, or pass both -DssPath and -GmakePath."
}
$dss = if ($DssPath) { $DssPath } else {
    Join-Path $CcsRoot "ccs_base\scripting\bin\dss.bat"
}
$gmake = if ($GmakePath) { $GmakePath } else {
    Join-Path $CcsRoot "utils\bin\gmake.exe"
}
if (-not $PythonPath) {
    $pythonCommand = Get-Command python.exe -ErrorAction SilentlyContinue
    if ($null -eq $pythonCommand) {
        throw "Pass -PythonPath, set DSP_TEST_PYTHON, or add python.exe to PATH."
    }
    $PythonPath = $pythonCommand.Source
}
$script = Join-Path $PSScriptRoot "dss\dss_equalizer_lcd_audio_safe.js"
$outputDirectory = Join-Path $root "docs\eval_outputs\equalizer_lcd_audio_safe"
$tonePath = Join-Path $outputDirectory "1khz_-18dbfs_50khz.wav"
$musicPath = Join-Path $outputDirectory "music-like_50khz.wav"
$sampleRate = 50000
$durationSeconds = 10 # Looping keeps playback active through every 60 s / 300 s window.
$player = $null
$dssProcess = $null
$timedOut = $false

function Write-MonoWave {
    param([string]$Path, [int]$Seconds, [scriptblock]$Sample)
    $count = $sampleRate * $Seconds
    $stream = [System.IO.File]::Open($Path, [System.IO.FileMode]::Create)
    $writer = [System.IO.BinaryWriter]::new($stream)
    try {
        $dataBytes = $count * 2
        $writer.Write([Text.Encoding]::ASCII.GetBytes("RIFF"))
        $writer.Write([int](36 + $dataBytes))
        $writer.Write([Text.Encoding]::ASCII.GetBytes("WAVEfmt "))
        $writer.Write([int]16); $writer.Write([int16]1); $writer.Write([int16]1)
        $writer.Write([int]$sampleRate); $writer.Write([int]($sampleRate * 2))
        $writer.Write([int16]2); $writer.Write([int16]16)
        $writer.Write([Text.Encoding]::ASCII.GetBytes("data")); $writer.Write([int]$dataBytes)
        for ($index = 0; $index -lt $count; $index++) {
            $writer.Write([int16](& $Sample $index))
        }
    }
    finally { $writer.Dispose(); $stream.Dispose() }
}

New-Item -ItemType Directory -Force -Path $outputDirectory | Out-Null
$amplitude = [Math]::Pow(10.0, -18.0 / 20.0) * 32767.0
Write-MonoWave -Path $tonePath -Seconds $durationSeconds -Sample {
    param($n) [Math]::Round($amplitude * [Math]::Sin(2.0 * [Math]::PI * 1000.0 * $n / $sampleRate))
}
Write-MonoWave -Path $musicPath -Seconds $durationSeconds -Sample {
    param($n)
    $t = $n / [double]$sampleRate
    $envelope = 0.65 + 0.25 * [Math]::Sin(2.0 * [Math]::PI * 1.7 * $t)
    $value = $envelope * (0.50 * [Math]::Sin(2.0 * [Math]::PI * 220.0 * $t) +
        0.30 * [Math]::Sin(2.0 * [Math]::PI * 440.0 * $t) +
        0.20 * [Math]::Sin(2.0 * [Math]::PI * (880.0 + 30.0 * [Math]::Sin(2.0 * [Math]::PI * 0.3 * $t)) * $t))
    [Math]::Round($amplitude * $value)
}

$audioPath = if ($AudioKind -eq "Tone") { $tonePath } else { $musicPath }
$sha = (& git -C $root rev-parse HEAD).Trim()
$dirtyLines = @(& git -C $root status --porcelain)
$dirtyState = if ($dirtyLines.Count -eq 0) { "clean" } else { $dirtyLines -join ";" }
$actualProjectMacro = "33 (build parameter)"

$buildStart = [DateTime]::UtcNow
if (-not (Test-Path $gmake)) { throw "CCS gmake not found: $gmake" }
# Force a complete Project 33 build with the LCD implementation enabled.
$buildLogPath = Join-Path $outputDirectory "project33_lcd_on_build.log"
& $gmake -C (Join-Path $root "Debug") -B all `
    "GEN_OPTS__FLAG=--define=DSP_LAB_PROJECT_SELECT=33 --define=EQ_ENABLE_LCD_DISPLAY=1" `
    2>&1 | Tee-Object -FilePath $buildLogPath
if ($LASTEXITCODE -ne 0) { throw "Project 33 LCD build failed." }
$linkInfoPath = Join-Path $root "Debug\DSP_LAB_0507_linkInfo.xml"
$outPath = Join-Path $root "Debug\DSP_LAB_0507.out"
$mapPath = Join-Path $root "Debug\DSP_LAB_0507.map"
foreach ($artifact in @($linkInfoPath, $outPath, $mapPath)) {
    if (-not (Test-Path $artifact)) { throw "Missing build artifact: $artifact" }
    if ((Get-Item $artifact).LastWriteTimeUtc -lt $buildStart.AddSeconds(-2)) {
        throw "Stale build artifact: $artifact"
    }
}
$linkInfo = Get-Content -Raw $linkInfoPath
if ($linkInfo -notmatch "<link_errors>0x0</link_errors>") {
    throw "CCS link_errors is nonzero."
}
$expandedBuildPath = Join-Path $outputDirectory "project33_lcd_on_commands.log"
$expandedBuild = (& $gmake -C (Join-Path $root "Debug") -n -B all `
    "GEN_OPTS__FLAG=--define=DSP_LAB_PROJECT_SELECT=33 --define=EQ_ENABLE_LCD_DISPLAY=1" `
    2>&1 | Out-String)
$expandedBuild | Set-Content -Encoding utf8 $expandedBuildPath
if (($expandedBuild -notmatch "DSP_LAB_PROJECT_SELECT=33") -or
    ($expandedBuild -notmatch "EQ_ENABLE_LCD_DISPLAY=1")) {
    throw "Expanded compiler commands do not prove Project 33 with LCD enabled."
}

$provenance = [ordered]@{
    commit_sha = $sha
    dirty_state = $dirtyState
    allowed_known_dirty = "Code/main.c"
    actual_project_macro = $actualProjectMacro
    audio_file = $audioPath
    playback_path = "default PC line-output"
    row_seconds = $RowSeconds
    stability_seconds = $StabilitySeconds
    loudspeaker_observation = "subjective operator observation only"
    operator_status = $OperatorStatus
    operator_notes = $OperatorNotes
    dss_timeout_minutes = $DssTimeoutMinutes
}
$provenance | ConvertTo-Json | Set-Content -Encoding utf8 (Join-Path $outputDirectory "run_provenance.json")

try {
    # SoundPlayer opens only the Windows default PC line-output and loops for all 60/300 s rows.
    $player = [System.Media.SoundPlayer]::new($audioPath)
    $player.Load()
    $player.PlayLooping()
    Start-Sleep -Milliseconds 500
    if (-not (Test-Path $dss)) { throw "DSS runner not found: $dss" }
    $env:DSP_TEST_COMMIT_SHA = $sha
    $env:DSP_TEST_DIRTY_STATE = $dirtyState
    $env:DSP_TEST_BUILD_CONFIG = "C6748;Project=$actualProjectMacro;Fs=50k;Frame=1024;CH1"
    $env:DSP_TEST_ROW_MS = [string]($RowSeconds * 1000)
    $env:DSP_TEST_STABILITY_MS = [string]($StabilitySeconds * 1000)
    $env:DSP_TEST_ROOT = ($root -replace '\\', '/')
    $env:DSP_TEST_CCXML = ((Join-Path $root "TargetConfig\C6748.ccxml") -replace '\\', '/')
    $env:DSP_TEST_PROGRAM = ($outPath -replace '\\', '/')
    $env:DSP_TEST_RESULT_DIR = ($outputDirectory -replace '\\', '/')
    $env:DSP_TEST_PYTHON = ($PythonPath -replace '\\', '/')
    $env:DSP_TEST_OPERATOR_STATUS = $OperatorStatus
    $env:DSP_TEST_OPERATOR_NOTES = $OperatorNotes
    $dssProcess = Start-Process -FilePath $dss -ArgumentList @($script) `
        -PassThru -WindowStyle Hidden
    if (-not $dssProcess.WaitForExit($DssTimeoutMinutes * 60 * 1000)) {
        $timedOut = $true
        & taskkill.exe /PID $dssProcess.Id /T /F | Out-Null
        [ordered]@{
            measurement_status = "TIMEOUT"
            timeout_minutes = $DssTimeoutMinutes
            commit_sha = $sha
        } | ConvertTo-Json | Set-Content -Encoding utf8 `
            (Join-Path $outputDirectory "run_timeout.json")
        throw "Project 3.3 DSS exceeded the external timeout."
    }
    if ($dssProcess.ExitCode -ne 0) {
        throw "Project 3.3 DSS test failed with exit code $($dssProcess.ExitCode)"
    }
}
finally {
    if (($null -ne $dssProcess) -and (-not $dssProcess.HasExited) -and (-not $timedOut)) {
        & taskkill.exe /PID $dssProcess.Id /T /F | Out-Null
    }
    if ($null -ne $player) { $player.Stop(); $player.Dispose() }
    Remove-Item Env:DSP_TEST_COMMIT_SHA, Env:DSP_TEST_DIRTY_STATE,
        Env:DSP_TEST_BUILD_CONFIG, Env:DSP_TEST_ROOT, Env:DSP_TEST_CCXML,
        Env:DSP_TEST_PROGRAM, Env:DSP_TEST_RESULT_DIR, Env:DSP_TEST_ROW_MS,
        Env:DSP_TEST_STABILITY_MS, Env:DSP_TEST_PYTHON,
        Env:DSP_TEST_OPERATOR_STATUS, Env:DSP_TEST_OPERATOR_NOTES -ErrorAction SilentlyContinue
}
