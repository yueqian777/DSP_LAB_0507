[CmdletBinding()]
param(
    [ValidateSet(
        "DryRun", "Prepare", "Build", "NoTouch120s", "Hitbox27",
        "PageSwitch30", "LcdTiming", "Endurance300s", "AnalyzerReset",
        "Process")]
    [string]$Stage = "DryRun",
    [string]$ExpectedSha = "",
    [string]$ConfirmHardware = "",
    [string]$ConfirmLongManualStage = "",
    [string]$ResultDirectory = "",
    [string]$RepoRoot = "",
    [string]$CcsRoot = "D:\SoftwareDownload\CCS_20.5.0.00028_win\ccs",
    [string]$PythonPath = "C:\Python314\python.exe",
    [string]$AudioFile = "",
    [string]$OperatorVisualFile = "",
    [string]$EnduranceOperatorQuotaFile = "",
    [ValidateRange(30, 900)]
    [int]$InteractionTimeoutSeconds = 180,
    [ValidateRange(1, 256)]
    [int]$TimingSamplesPerClass = 8,
    [ValidateRange(5, 120)]
    [int]$EnduranceStartDelaySeconds = 15,
    [ValidateRange(5, 300)]
    [int]$DssTimeoutMinutes = 180
)

$ErrorActionPreference = "Stop"

# The default path is intentionally inert. It resolves no CCS/repository path,
# builds nothing, starts no audio, and creates no debug session.
if ($Stage -eq "DryRun") {
    Write-Output "NOT_RUN_NO_HARDWARE"
    return
}

$root = if ($RepoRoot) { (Resolve-Path -LiteralPath $RepoRoot).Path } else {
    (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")).Path
}
$processor = Join-Path $root "tools\process_equalizer_final_acceptance.py"
$buildExporter = Join-Path $root "tools\export_project33_h_build_evidence.py"
$dssScript = Join-Path $root "tools\dss\dss_equalizer_final_acceptance.js"
$buildMatrix = Join-Path $root "tools\run_equalizer_ui_build_matrix.ps1"
$ccxml = Join-Path $root "TargetConfig\C6748.ccxml"
$dss = Join-Path $CcsRoot "ccs_base\scripting\bin\dss.bat"
$gmakePath = Join-Path $CcsRoot "utils\bin\gmake.exe"
$nmPath = Join-Path $CcsRoot `
    "tools\compiler\ti-cgt-c6000_8.5.0.LTS\bin\nm6x.exe"

foreach ($path in @($processor, $buildExporter, $dssScript)) {
    if (-not (Test-Path -LiteralPath $path)) {
        throw "Missing final-acceptance tool: $path"
    }
}
if (-not (Test-Path -LiteralPath $PythonPath)) {
    throw "Python executable is missing: $PythonPath"
}

function Get-GitState {
    $branch = (& git -C $root branch --show-current).Trim()
    if ($LASTEXITCODE -ne 0) { throw "Cannot read Git branch" }
    $head = (& git -C $root rev-parse HEAD).Trim().ToLowerInvariant()
    if ($LASTEXITCODE -ne 0) { throw "Cannot read Git HEAD" }
    $dirty = @(& git -C $root status --porcelain --untracked-files=all)
    if ($LASTEXITCODE -ne 0) { throw "Cannot read Git worktree status" }
    [pscustomobject]@{ branch = $branch; head = $head; dirty = $dirty }
}

function Assert-ExactGitState([string]$Label) {
    if ($ExpectedSha -notmatch '^[0-9a-fA-F]{40}$') {
        throw "$Label requires -ExpectedSha with exactly 40 hexadecimal characters"
    }
    $state = Get-GitState
    $expected = $ExpectedSha.ToLowerInvariant()
    if ($state.branch -ne "main") {
        throw "$Label requires main; current branch is $($state.branch)"
    }
    if ($state.head -ne $expected) {
        throw "$Label HEAD $($state.head) does not match $expected"
    }
    if ($state.dirty.Count -ne 0) {
        throw "$Label requires a clean tree: $($state.dirty -join ';')"
    }
    $state
}

function New-DefaultResultDirectory {
    $state = Get-GitState
    $short = $state.head.Substring(0, 7)
    $stamp = Get-Date -Format "yyyyMMdd_HHmmss"
    Join-Path $env:TEMP `
        "DSP_LAB_0507\project33_final_acceptance\${short}_$stamp"
}

$resultDir = if ($ResultDirectory) {
    [IO.Path]::GetFullPath($ResultDirectory)
} else {
    New-DefaultResultDirectory
}
$rawDir = Join-Path $resultDir "raw"
$evidenceDir = Join-Path $resultDir "evidence"
$buildDir = Join-Path $rawDir "build"
New-Item -ItemType Directory -Path $rawDir, $evidenceDir -Force |
    Out-Null

function Invoke-Processor {
    & $PythonPath -B $processor --raw-dir $rawDir --output-dir $evidenceDir
    if ($LASTEXITCODE -ne 0) {
        throw "Final-acceptance processor failed with exit code $LASTEXITCODE"
    }
}

function Ensure-OperatorTemplates {
    $visualPath = Join-Path $rawDir "operator_visual_observation_raw.json"
    if (-not (Test-Path -LiteralPath $visualPath)) {
        [ordered]@{
            result_label = "PENDING_OPERATOR"
            operator = ""
            observed_at = ""
            observations = [ordered]@{
                circular_offset = "PENDING"
                full_screen_white_flash = "PENDING"
                old_page_ghosting = "PENDING"
                element_misalignment = "PENDING"
                touch_coordinate_drift = "PENDING"
            }
            notes = ""
        } | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $visualPath `
            -Encoding utf8
    }
    $quotaPath = Join-Path $rawDir "endurance_operator_quota_raw.json"
    if (-not (Test-Path -LiteralPath $quotaPath)) {
        [ordered]@{
            result_label = "PENDING_OPERATOR"
            operator = ""
            observed_at = ""
            preset_counts = [ordered]@{
                FLAT = 0; BASS = 0; VOCAL = 0; TREBLE = 0; V_SHAPE = 0
            }
            dynamic_toggle_counts = [ordered]@{
                SMART_BASS = 0; DYNAMIC_CLARITY = 0; HF_GUARD = 0
            }
            dynamic_strength_counts = [ordered]@{
                SMART_BASS = 0; DYNAMIC_CLARITY = 0; HF_GUARD = 0
            }
            page_round_trips = 0
            editor_distinct_bands = 0
            custom_apply_count = 0
            flat_count = 0
            overlap_transition_count = 0
            notes = ""
        } | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $quotaPath `
            -Encoding utf8
    }
}

function Import-OperatorFiles {
    if ($OperatorVisualFile) {
        $source = (Resolve-Path -LiteralPath $OperatorVisualFile).Path
        Copy-Item -LiteralPath $source -Destination `
            (Join-Path $rawDir "operator_visual_observation_raw.json") -Force
    }
    if ($EnduranceOperatorQuotaFile) {
        $source = (Resolve-Path -LiteralPath $EnduranceOperatorQuotaFile).Path
        Copy-Item -LiteralPath $source -Destination `
            (Join-Path $rawDir "endurance_operator_quota_raw.json") -Force
    }
}

Ensure-OperatorTemplates
Import-OperatorFiles

if ($Stage -eq "Prepare" -or $Stage -eq "Process") {
    Invoke-Processor
    Write-Output "RESULT_DIR=$resultDir"
    Write-Output "EVIDENCE_DIR=$evidenceDir"
    Write-Output "HARDWARE_RESULT=PENDING_HARDWARE"
    return
}

function Get-BuildResult([string]$SummaryPath) {
    if (-not (Test-Path -LiteralPath $SummaryPath)) {
        throw "Build matrix summary is missing: $SummaryPath"
    }
    $items = @(Get-Content -Raw -LiteralPath $SummaryPath |
        ConvertFrom-Json)
    if ($items.Count -ne 1 -or $items[0].profile -ne "H_project33_full") {
        throw "Expected exactly one H_project33_full result"
    }
    $value = $items[0]
    if (($value.warning_count -ne 0) -or ($value.error_count -ne 0) -or
        ($value.link_errors -ne "0x0")) {
        throw "H build is not warning_count=0/error_count=0/link_errors=0x0"
    }
    $value
}

function Save-NmOutput([string]$ProgramPath, [string]$Destination) {
    $lines = @(& $nmPath -l $ProgramPath 2>&1)
    if ($LASTEXITCODE -ne 0) {
        throw "nm6x failed for $ProgramPath"
    }
    $lines | Set-Content -LiteralPath $Destination -Encoding utf8
    $lines -join "`n"
}

function Copy-DebugArtifacts([string]$Prefix) {
    $sources = [ordered]@{
        ".out" = Join-Path $root "Debug\DSP_LAB_0507.out"
        ".map" = Join-Path $root "Debug\DSP_LAB_0507.map"
        "_linkInfo.xml" = Join-Path $root "Debug\DSP_LAB_0507_linkInfo.xml"
    }
    foreach ($suffix in $sources.Keys) {
        if (-not (Test-Path -LiteralPath $sources[$suffix])) {
            throw "Build artifact is missing: $($sources[$suffix])"
        }
        Copy-Item -LiteralPath $sources[$suffix] -Destination `
            (Join-Path $buildDir ($Prefix + $suffix)) -Force
    }
}

function Invoke-CleanTimingBuild([string]$BaseDefines) {
    $timingDefines = ($BaseDefines -replace `
        '--define=EQ_ENABLE_LCD_JOB_TIMING_CAPTURE=0\s*', '').Trim() +
        " --define=EQ_ENABLE_LCD_JOB_TIMING_CAPTURE=1"
    Push-Location $root
    try {
        $oldPreference = $ErrorActionPreference
        $ErrorActionPreference = "Continue"
        $cleanOutput = & $gmakePath -C Debug clean 2>&1
        if ($LASTEXITCODE -ne 0) {
            throw "Timing-image clean failed"
        }
        $buildOutput = & $gmakePath -B -C Debug all `
            "GEN_OPTS__FLAG=$timingDefines" 2>&1
        $buildExit = $LASTEXITCODE
        $ErrorActionPreference = $oldPreference
        @($cleanOutput) + @($buildOutput) | Set-Content -LiteralPath `
            (Join-Path $buildDir "H_project33_lcd_timing.log") -Encoding utf8
        if ($buildExit -ne 0) {
            throw "Timing-image build failed"
        }
    }
    finally {
        $ErrorActionPreference = "Stop"
        Pop-Location
    }
    $warningCount = @($buildOutput | Select-String -Pattern `
        '(?i)warning\s+#\d+|:\s*warning(?:\s|:)').Count
    $errorCount = @($buildOutput | Select-String -Pattern `
        '(?i)error\s+#\d+|:\s*error(?:\s|:)|undefined symbol').Count
    $linkPath = Join-Path $root "Debug\DSP_LAB_0507_linkInfo.xml"
    $linkText = Get-Content -Raw -LiteralPath $linkPath
    if (($warningCount -ne 0) -or ($errorCount -ne 0) -or
        ($linkText -notmatch '<link_errors>0x0</link_errors>')) {
        throw "Timing-image diagnostics are not warning/error/link clean"
    }
    Copy-DebugArtifacts "H_project33_lcd_timing"
    $timingProgram = Join-Path $buildDir "H_project33_lcd_timing.out"
    $nmText = Save-NmOutput $timingProgram `
        (Join-Path $buildDir "H_project33_lcd_timing_nm.txt")
    foreach ($symbol in @(
            "EQ_DebugLcdTimingSamples", "EQ_DebugLcdTimingClassCount",
            "EQ_DebugLcdTimingSampleCapacity",
            "EQ_DebugLcdTimingSampleCount")) {
        if ($nmText -notmatch ("\b{0}\b" -f [regex]::Escape($symbol))) {
            throw "Timing image is missing $symbol"
        }
    }
    [ordered]@{
        profile = "H_project33_lcd_timing"
        defines = $timingDefines
        warning_count = $warningCount
        error_count = $errorCount
        link_errors = "0x0"
        out_sha256 = (Get-FileHash -Algorithm SHA256 `
            -LiteralPath $timingProgram).Hash.ToLowerInvariant()
    }
}

if ($Stage -eq "Build") {
    Assert-ExactGitState "pre-build" | Out-Null
    foreach ($path in @($buildMatrix, $gmakePath, $nmPath)) {
        if (-not (Test-Path -LiteralPath $path)) {
            throw "Build dependency is missing: $path"
        }
    }
    $summaryPath = Join-Path $buildDir "build_matrix_summary.json"
    Write-Output "CLEAN_BUILD_PROFILE=H_project33_full"
    & $buildMatrix -RepoRoot $root -GmakePath $gmakePath -NmPath $nmPath `
        -OutputDirectory $buildDir -ProfileNames "H_project33_full" |
        Out-Host
    $buildResult = Get-BuildResult $summaryPath
    Copy-DebugArtifacts "H_project33_full"
    $productionProgram = Join-Path $buildDir "H_project33_full.out"
    $productionNm = Save-NmOutput $productionProgram `
        (Join-Path $buildDir "H_project33_full_nm.txt")
    foreach ($symbol in @(
            "EQ_DebugTouchActionHistogramSize",
            "EQ_DebugTouchActionHistogram",
            "EQ_DebugAnalyzerEpoch", "EQ_DebugAnalyzerPublicationCount",
            "EQ_DebugStaticDynamicTransitionOverlapFrameCount",
            "EQ_DebugLcdPendingDirtyRegionCount",
            "EQ_DebugLcdMaxPendingDirtyRegionCount")) {
        if ($productionNm -notmatch ("\b{0}\b" -f [regex]::Escape($symbol))) {
            throw "Production H image is missing acceptance symbol $symbol"
        }
    }
    $productionHash = (Get-FileHash -Algorithm SHA256 `
        -LiteralPath $productionProgram).Hash.ToLowerInvariant()
    if ($productionHash -ne
        ([string]$buildResult.out_sha256).ToLowerInvariant()) {
        throw "Saved production .out hash differs from build summary"
    }
    & $PythonPath -B $buildExporter `
        --build-matrix $summaryPath `
        --map (Join-Path $buildDir "H_project33_full.map") `
        --link-info (Join-Path $buildDir "H_project33_full_linkInfo.xml") `
        --out $productionProgram `
        --build-log (Join-Path $buildDir "H_project33_full.log") `
        --nm6x $nmPath `
        --output-dir $evidenceDir
    if ($LASTEXITCODE -ne 0) {
        throw "Strict H production-build evidence export failed"
    }

    Write-Output "MEASUREMENT_BUILD_PROFILE=H_project33_lcd_timing"
    $timingResult = Invoke-CleanTimingBuild $buildResult.defines

    $restoreDir = Join-Path $buildDir "production_restore"
    & $buildMatrix -RepoRoot $root -GmakePath $gmakePath -NmPath $nmPath `
        -OutputDirectory $restoreDir -ProfileNames "H_project33_full" |
        Out-Host
    $restoreResult = Get-BuildResult `
        (Join-Path $restoreDir "build_matrix_summary.json")
    $restoredProgram = Join-Path $root "Debug\DSP_LAB_0507.out"
    $restoredHash = (Get-FileHash -Algorithm SHA256 `
        -LiteralPath $restoredProgram).Hash.ToLowerInvariant()
    if ($restoredHash -ne $productionHash -or
        $restoredHash -ne
        ([string]$restoreResult.out_sha256).ToLowerInvariant()) {
        throw "Production H image was not restored exactly after timing build"
    }
    Assert-ExactGitState "post-build" | Out-Null
    [ordered]@{
        schema_version = 1
        source_commit = $ExpectedSha.ToLowerInvariant()
        production_profile = "H_project33_full"
        production_measurement_macros = [ordered]@{
            EQ_ENABLE_LCD_JOB_TIMING_CAPTURE = 0
            EQ_ENABLE_FINAL_METRICS_BOARD_TEST = 0
        }
        production_out_sha256 = $productionHash
        timing_profile = $timingResult.profile
        timing_out_sha256 = $timingResult.out_sha256
        timing_warning_count = $timingResult.warning_count
        timing_error_count = $timingResult.error_count
        timing_link_errors = $timingResult.link_errors
        production_restored_sha256 = $restoredHash
    } | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath `
        (Join-Path $buildDir "build_provenance.json") -Encoding utf8
    Invoke-Processor
    Write-Output "RESULT_DIR=$resultDir"
    Write-Output "BUILD_EVIDENCE=$evidenceDir"
    return
}

$stageMap = @{
    NoTouch120s = "no_touch_120s"
    Hitbox27 = "touch_hitbox_27"
    PageSwitch30 = "page_switch_30"
    LcdTiming = "lcd_job_timing"
    Endurance300s = "endurance_300s"
    AnalyzerReset = "analyzer_reset"
}
if (-not $stageMap.ContainsKey($Stage)) {
    throw "Unsupported stage: $Stage"
}
if ($ConfirmHardware -ne "C6748_CONNECTED_FINAL_ACCEPTANCE") {
    throw "Hardware stage requires -ConfirmHardware C6748_CONNECTED_FINAL_ACCEPTANCE"
}
$manualStages = @("Hitbox27", "PageSwitch30", "LcdTiming", "Endurance300s")
if (($manualStages -contains $Stage) -and
    ($ConfirmLongManualStage -ne "I_ACCEPT_MANUAL_LONG_STAGE")) {
    throw "Manual stage requires -ConfirmLongManualStage I_ACCEPT_MANUAL_LONG_STAGE"
}
Assert-ExactGitState "pre-hardware" | Out-Null
foreach ($path in @($dss, $ccxml, $nmPath)) {
    if (-not (Test-Path -LiteralPath $path)) {
        throw "Hardware dependency is missing: $path"
    }
}
$provenancePath = Join-Path $buildDir "build_provenance.json"
if (-not (Test-Path -LiteralPath $provenancePath)) {
    throw "Run the Build stage in this ResultDirectory before hardware stages"
}
$provenance = Get-Content -Raw -LiteralPath $provenancePath |
    ConvertFrom-Json
if ([string]$provenance.source_commit -ne $ExpectedSha.ToLowerInvariant()) {
    throw "Build provenance commit differs from ExpectedSha"
}
$programName = if ($Stage -eq "LcdTiming") {
    "H_project33_lcd_timing.out"
} else {
    "H_project33_full.out"
}
$expectedProgramHash = if ($Stage -eq "LcdTiming") {
    [string]$provenance.timing_out_sha256
} else {
    [string]$provenance.production_out_sha256
}
$program = Join-Path $buildDir $programName
if (-not (Test-Path -LiteralPath $program)) {
    throw "Stage program is missing: $program"
}
$programHash = (Get-FileHash -Algorithm SHA256 `
    -LiteralPath $program).Hash.ToLowerInvariant()
if ($programHash -ne $expectedProgramHash.ToLowerInvariant()) {
    throw "Stage .out hash differs from build provenance"
}

function New-MusicLikeWave([string]$Path) {
    $stream = [IO.File]::Open($Path, [IO.FileMode]::Create)
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
        $amplitude = [Math]::Pow(10.0, -15.0 / 20.0) * 32767.0
        for ($index = 0; $index -lt $samples; $index++) {
            $time = $index / [double]$sampleRate
            $value = $amplitude * (
                0.25 * [Math]::Sin(2 * [Math]::PI * 125.0 * $time) +
                0.22 * [Math]::Sin(2 * [Math]::PI * 500.0 * $time) +
                0.20 * [Math]::Sin(2 * [Math]::PI * 2000.0 * $time) +
                0.18 * [Math]::Sin(2 * [Math]::PI * 8000.0 * $time) +
                0.15 * [Math]::Sin(2 * [Math]::PI * 620.0 * $time))
            $writer.Write([int16][Math]::Round($value))
        }
    }
    finally {
        $writer.Dispose()
        $stream.Dispose()
    }
}

$wavePath = if ($AudioFile) {
    (Resolve-Path -LiteralPath $AudioFile).Path
} else {
    Join-Path $rawDir "music_like_50khz.wav"
}
if (-not $AudioFile -and -not (Test-Path -LiteralPath $wavePath)) {
    New-MusicLikeWave $wavePath
}

$rawStem = $stageMap[$Stage]
$existingRaw = Join-Path $rawDir ($rawStem + ".jsonl")
$existingSummary = Join-Path $rawDir ($rawStem + "_raw_summary.json")
if ((Test-Path -LiteralPath $existingRaw) -or
    (Test-Path -LiteralPath $existingSummary)) {
    $archiveDir = Join-Path $rawDir `
        ("archive\{0}_{1}" -f $rawStem, (Get-Date -Format "yyyyMMdd_HHmmss"))
    New-Item -ItemType Directory -Path $archiveDir -Force | Out-Null
    foreach ($path in @($existingRaw, $existingSummary)) {
        if (Test-Path -LiteralPath $path) {
            Copy-Item -LiteralPath $path -Destination $archiveDir
        }
    }
}

$player = [System.Media.SoundPlayer]::new($wavePath)
$dssProcess = $null
$runError = $null
try {
    $player.Load()
    $player.PlayLooping()
    $env:DSP_FINAL_ACCEPTANCE_HARDWARE_AUTH = `
        "C6748_CONNECTED_FINAL_ACCEPTANCE"
    $env:DSP_FINAL_ACCEPTANCE_STAGE = $stageMap[$Stage]
    $env:DSP_TEST_ROOT = $root -replace '\\', '/'
    $env:DSP_TEST_CCXML = $ccxml -replace '\\', '/'
    $env:DSP_TEST_PROGRAM = $program -replace '\\', '/'
    $env:DSP_TEST_RAW_DIR = $rawDir -replace '\\', '/'
    $env:DSP_TEST_EXPECTED_SHA = $ExpectedSha.ToLowerInvariant()
    $env:DSP_TEST_PROGRAM_SHA256 = $programHash
    $env:DSP_TEST_INTERACTION_TIMEOUT_SECONDS = `
        [string]$InteractionTimeoutSeconds
    $env:DSP_TEST_TIMING_SAMPLES_PER_CLASS = `
        [string]$TimingSamplesPerClass
    $env:DSP_TEST_ENDURANCE_START_DELAY_SECONDS = `
        [string]$EnduranceStartDelaySeconds

    Write-Output "RESULT_DIR=$resultDir"
    Write-Output "RAW_DIR=$rawDir"
    Write-Output "STAGE=$Stage"
    Write-Output "Wait for each *_REQUIRED prompt before touching the LCD."
    $arguments = @('/d', '/s', '/c', ('""{0}" "{1}""' -f $dss, $dssScript))
    $dssProcess = Start-Process -FilePath $env:ComSpec `
        -ArgumentList $arguments -PassThru -NoNewWindow
    if (-not $dssProcess.WaitForExit($DssTimeoutMinutes * 60 * 1000)) {
        & taskkill.exe /PID $dssProcess.Id /T /F | Out-Null
        throw "DSS exceeded the $DssTimeoutMinutes-minute timeout"
    }
    $dssProcess.WaitForExit()
    $dssProcess.Refresh()
    if ($dssProcess.ExitCode -ne 0) {
        throw "DSS exited with code $($dssProcess.ExitCode)"
    }
}
catch {
    $runError = $_
}
finally {
    $player.Stop()
    $player.Dispose()
    Remove-Item Env:DSP_FINAL_ACCEPTANCE_HARDWARE_AUTH,
        Env:DSP_FINAL_ACCEPTANCE_STAGE, Env:DSP_TEST_ROOT,
        Env:DSP_TEST_CCXML, Env:DSP_TEST_PROGRAM, Env:DSP_TEST_RAW_DIR,
        Env:DSP_TEST_EXPECTED_SHA, Env:DSP_TEST_PROGRAM_SHA256,
        Env:DSP_TEST_INTERACTION_TIMEOUT_SECONDS,
        Env:DSP_TEST_TIMING_SAMPLES_PER_CLASS,
        Env:DSP_TEST_ENDURANCE_START_DELAY_SECONDS `
        -ErrorAction SilentlyContinue
    Invoke-Processor
}

Assert-ExactGitState "post-hardware" | Out-Null
if ($null -ne $runError) {
    throw $runError
}
$summaryPath = Join-Path $rawDir ($rawStem + "_raw_summary.json")
if (-not (Test-Path -LiteralPath $summaryPath)) {
    throw "DSS produced no raw stage summary"
}
$summary = Get-Content -Raw -LiteralPath $summaryPath | ConvertFrom-Json
if ($summary.completed -ne $true) {
    throw "DSS stage failed; raw evidence retained: $($summary.error)"
}
Write-Output "RAW_STAGE_RESULT=MEASURED"
Write-Output "EVIDENCE_DIR=$evidenceDir"
