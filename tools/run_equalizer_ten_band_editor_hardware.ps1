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
    [ValidateRange(30, 120)]
    [int]$DssTimeoutMinutes = 60
)

$ErrorActionPreference = "Stop"

# Dry-run paths are intentionally inert and stop before resolving CCS paths,
# generating build artifacts, playing audio, or starting DSS.
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
$gmakePath = Join-Path $CcsRoot "utils\bin\gmake.exe"
$nmPath = Join-Path $CcsRoot `
    "tools\compiler\ti-cgt-c6000_8.5.0.LTS\bin\nm6x.exe"
$program = Join-Path $root "Debug\DSP_LAB_0507.out"
$linkInfo = Join-Path $root "Debug\DSP_LAB_0507_linkInfo.xml"
$ccxml = Join-Path $root "TargetConfig\C6748.ccxml"
$dssScript = Join-Path $root "tools\dss\dss_equalizer_ten_band_editor.js"
$buildMatrix = Join-Path $root "tools\run_equalizer_ui_build_matrix.ps1"
$buildIdPath = Join-Path $root `
    "Code\User\user_dsp\equalizer_build_id_generated.h"

foreach ($path in @($dss, $gmakePath, $nmPath, $ccxml, $dssScript,
        $buildMatrix, $buildIdPath)) {
    if (-not (Test-Path -LiteralPath $path)) {
        throw "Missing required path: $path"
    }
}

$expected = $ExpectedSha.ToLowerInvariant()
$shortSha = $expected.Substring(0, 7)

function Get-ExactGitState {
    $branch = (& git -C $root branch --show-current).Trim()
    if ($LASTEXITCODE -ne 0) { throw "Cannot read current Git branch" }
    $head = (& git -C $root rev-parse HEAD).Trim().ToLowerInvariant()
    if ($LASTEXITCODE -ne 0) { throw "Cannot read current Git HEAD" }
    $dirty = @(& git -C $root status --porcelain --untracked-files=all)
    if ($LASTEXITCODE -ne 0) { throw "Cannot read Git worktree status" }
    return [pscustomobject]@{
        branch = $branch
        head = $head
        dirty = @($dirty)
    }
}

function Assert-ExactGitState([string]$Stage) {
    $state = Get-ExactGitState
    if ($state.branch -ne "main") {
        throw "$Stage requires main; current branch is $($state.branch)"
    }
    if ($state.head -ne $expected) {
        throw "$Stage HEAD $($state.head) does not match $expected"
    }
    if ($state.dirty.Count -ne 0) {
        throw "$Stage requires dirty=0: $($state.dirty -join ';')"
    }
    return $state
}

function Assert-BuildIdentity([string]$Stage) {
    $buildId = Get-Content -Raw -LiteralPath $buildIdPath
    if (($buildId -notmatch
            ("EQ_BUILD_GIT_SHA `"{0}`"" -f [regex]::Escape($shortSha))) -or
        ($buildId -notmatch 'EQ_BUILD_DIRTY 0')) {
        throw "$Stage build identity does not match $shortSha/dirty=0"
    }
}

$initialGit = Assert-ExactGitState "pre-build"
$stamp = Get-Date -Format "yyyyMMdd_HHmmss"
$resultDir = Join-Path $env:TEMP `
    "DSP_LAB_0507\equalizer_ten_band_editor\${shortSha}_$stamp"
New-Item -ItemType Directory -Path $resultDir -Force | Out-Null
$matrixDir = Join-Path $resultDir "clean_h_build"
$matrixSummaryPath = Join-Path $matrixDir "build_matrix_summary.json"
$buildStartedUtc = [DateTime]::UtcNow

Write-Output "CLEAN_BUILD_PROFILE=H_project33_full"
& $buildMatrix -RepoRoot $root -GmakePath $gmakePath -NmPath $nmPath `
    -OutputDirectory $matrixDir -ProfileNames "H_project33_full" | Out-Host
if (-not (Test-Path -LiteralPath $matrixSummaryPath)) {
    throw "H clean build produced no build matrix summary"
}

$matrixResults = @(Get-Content -Raw -LiteralPath $matrixSummaryPath |
    ConvertFrom-Json)
if (($matrixResults.Count -ne 1) -or
    ($matrixResults[0].profile -ne "H_project33_full")) {
    throw "Hardware runner requires exactly one H_project33_full build result"
}
$buildResult = $matrixResults[0]
if (($buildResult.warning_count -ne 0) -or
    ($buildResult.error_count -ne 0) -or
    ($buildResult.link_errors -ne "0x0")) {
    throw "H build diagnostics are not warning=0/error=0/link_errors=0x0"
}
foreach ($define in @(
        "--define=EQ_ENABLE_LCD_DISPLAY=1",
        "--define=EQ_ENABLE_PROJECT33_TOUCH=1",
        "--define=EQ_ENABLE_TEN_BAND_EDITOR=1",
        "--define=EQ_ENABLE_TEN_BAND_EDITOR_TOUCH=1",
        "--define=EQ_UI_RUNTIME_DEFAULT_MASK=63")) {
    if ($buildResult.defines -notmatch [regex]::Escape($define)) {
        throw "H build macro missing: $define"
    }
}
if (($buildResult.job_count -ne 27) -or
    ($buildResult.dynamic_hitbox_count -ne 12) -or
    ($buildResult.editor_hitbox_count -ne 20) -or
    ($buildResult.framebuffer_symbol_count -ne 2) -or
    ($buildResult.second_framebuffer_symbol_hits -ne 1) -or
    ($buildResult.offscreen_buffer_object_count -ne 2) -or
    ($buildResult.offscreen_buffer_bytes -ne $buildResult.framebuffer_bytes) -or
    ($buildResult.editor_state_bytes -le 0)) {
    throw "H build UI/editor/link contract failed"
}
foreach ($artifact in @($program, $linkInfo)) {
    if (-not (Test-Path -LiteralPath $artifact)) {
        throw "H clean build missing artifact: $artifact"
    }
}
if ((Get-Item -LiteralPath $program).LastWriteTimeUtc -lt
    $buildStartedUtc.AddSeconds(-2)) {
    throw "Debug .out predates this H clean build"
}
if ((Get-Content -Raw -LiteralPath $linkInfo) -notmatch
    '<link_errors>0x0</link_errors>') {
    throw "H clean build linkInfo has nonzero link_errors"
}
Assert-BuildIdentity "post-build"
$postBuildGit = Assert-ExactGitState "post-build"

$programSha256 = (Get-FileHash -Algorithm SHA256 `
    -LiteralPath $program).Hash.ToLowerInvariant()
if ($programSha256 -ne
    ([string]$buildResult.out_sha256).ToLowerInvariant()) {
    throw "H .out SHA256 does not match the clean-build summary"
}

$symbolText = (& $nmPath -l $program 2>&1) -join "`n"
if ($LASTEXITCODE -ne 0) {
    throw "nm6x could not inspect the H Project 3.3 artifact"
}
$lcdDoubleBufferDebugSymbols = @(
    "EQ_DebugLcdDoubleBufferEnabled", "EQ_DebugLcdFrontPage",
    "EQ_DebugLcdSwapTargetPage", "EQ_DebugLcdSwapPending",
    "EQ_DebugLcdSwapDescriptorMask", "EQ_DebugLcdEofCount",
    "EQ_DebugLcdEofAmbiguousCount", "EQ_DebugLcdSwapRequestCount",
    "EQ_DebugLcdSwapCompleteCount", "EQ_DebugLcdCurrentFrame1Base",
    "EQ_DebugLcdCurrentFrame1End", "EQ_DebugLcdRasterStopTimeoutCount"
)
$requiredSymbols = @(
    "EQ_DebugBuildGitSha", "EQ_DebugBuildDirty", "EQ_DebugInitStage",
    "EQ_DebugTouchRawX", "EQ_DebugTouchRawY", "EQ_DebugTouchScreenX",
    "EQ_DebugTouchScreenY", "EQ_DebugTouchPressed",
    "EQ_DebugTouchActionCount", "EQ_DebugTouchLastAction",
    "EQ_DebugTouchRejectedCount", "EQ_DebugTouchMaxCycles",
    "EQ_DebugUiRequestedPage", "EQ_DebugUiDisplayedPage",
    "EQ_DebugUiPageBuilding", "EQ_DebugUiDraftVersion",
    "EQ_DebugUiEditorSelectedBand", "EQ_DebugUiEditorDraftDirty",
    "EQ_DebugUiEditorSubmittedValid", "EQ_DebugUiEditorApplyStatus",
    "EQ_DebugUiEditorSubmittedSequence", "EQ_DebugUiEditorAppliedSequence",
    "EQ_DebugUiEditorDraftGainHalfDb",
    "EQ_DebugUiEditorSubmittedGainHalfDb",
    "EQ_DebugUiEditorAppliedGainHalfDb",
    "EQ_DebugUiEditorStateBytes", "EQ_DebugUiTotalStateBytes",
    "EQ_DebugControlRequestToken", "EQ_DebugControlAppliedToken",
    "EQ_DebugBuilderCancelCount", "EQ_DebugLcdJobCount",
    "EQ_DebugLcdOver1msCount", "EQ_DebugLcdOver2msCount",
    "EQ_DebugLcdOver5msCount", "EQ_DebugLcdCategoryCount",
    "EQ_DebugLcdCategoryCountSize", "EQ_DebugLcdJobTypeCountSize",
    "EQ_DebugUiJobCountSize", "EQ_DebugDynamicHitboxCountSize",
    "EQ_DebugEditorHitboxCountSize",
    "EQ_DebugLcdCategoryLastCycles", "EQ_DebugLcdCategoryMaxCycles",
    "EQ_DebugLcdJobTypeCount", "EQ_DebugLcdJobTypeLastCycles",
    "EQ_DebugLcdJobTypeMaxCycles", "EQ_DebugLcdAnalyzerMaxStripHeight",
    "EQ_DebugLcdAnalyzerStripCount", "EQ_DebugLcdAnalyzerValueCount",
    "EQ_DebugLcdRasterFaultCount", "EQ_DebugLcdFifoUnderflowCount",
    "EQ_DebugLcdFramebufferCanaryFailureCount",
    "EQ_DebugAnalyzerEnabled", "EQ_DebugAnalyzerValid",
    "EQ_DebugAnalyzerWarmup", "EQ_DebugAnalyzerRunCount",
    "EQ_DebugAnalyzerAnalysisCount", "EQ_DebugAnalyzerDeferredCount",
    "EQ_DebugAnalyzerMaxCycles", "EQ_DebugSmartBassEnabled",
    "EQ_DebugSmartBassDecisionCount", "EQ_DebugSmartBassLevelChangeCount",
    "EQ_DebugSmartBassMaxCycles", "EQ_DebugSmartBassSaturationCount",
    "EQ_DebugSmartBassNonFiniteCount", "EQ_DebugDynamicClarityEnabled",
    "EQ_DebugDynamicClarityDecisionCount",
    "EQ_DebugDynamicClarityLevelChangeCount",
    "EQ_DebugDynamicClarityMaxCycles",
    "EQ_DebugDynamicClaritySaturationCount",
    "EQ_DebugDynamicClarityNonFiniteCount",
    "EQ_DebugHarshnessGuardEnabled",
    "EQ_DebugHarshnessGuardDecisionCount",
    "EQ_DebugHarshnessGuardLevelChangeCount",
    "EQ_DebugHarshnessGuardMaxCycles",
    "EQ_DebugHarshnessGuardSaturationCount",
    "EQ_DebugHarshnessGuardNonFiniteCount",
    "EQ_UI_DYNAMIC_HITBOXES", "EQ_UI_EDITOR_HITBOXES"
) + $lcdDoubleBufferDebugSymbols
$requiredSymbolPresence = [ordered]@{}
foreach ($symbol in $requiredSymbols) {
    if ($symbolText -notmatch ("\b{0}\b" -f [regex]::Escape($symbol))) {
        throw "H artifact diagnostic contract missing symbol: $symbol"
    }
    $requiredSymbolPresence[$symbol] = $true
}

$retainedSizeSymbols = @(
    "EQ_DebugLcdCategoryCountSize", "EQ_DebugLcdJobTypeCountSize",
    "EQ_DebugUiJobCountSize", "EQ_DebugDynamicHitboxCountSize",
    "EQ_DebugEditorHitboxCountSize"
)
$missingRetainedSizeSymbols = @($retainedSizeSymbols | Where-Object {
    $symbolText -notmatch ("\b{0}\b" -f [regex]::Escape($_))
})
if ($missingRetainedSizeSymbols.Count -ne 0) {
    throw "H artifact is missing retained size symbols: " +
        ($missingRetainedSizeSymbols -join ",")
}

$wavePath = Join-Path $resultDir "editor_endurance_music_50khz.wav"
$instructionPath = Join-Path $resultDir "operator_instructions.txt"
$visualChecklistPath = Join-Path $resultDir "operator_visual_checklist.txt"
$visualSummaryPath = Join-Path $resultDir "operator_visual_summary.json"

$visualItems = @(
    [ordered]@{ id = "no_vertical_circular_drift"; description = "No vertical circular drift" },
    [ordered]@{ id = "no_fixed_global_offset"; description = "No fixed whole-screen offset" },
    [ordered]@{ id = "chinese_text_complete"; description = "Chinese text is complete" },
    [ordered]@{ id = "ascii_arrow_complete"; description = "ASCII -> arrows are complete" },
    [ordered]@{ id = "title_static_text_survives"; description = "Title and static text remain intact" },
    [ordered]@{ id = "analyzer_labels_intact"; description = "Analyzer bars do not damage labels" },
    [ordered]@{ id = "ten_band_zero_lines_aligned"; description = "All ten 0 dB lines align" },
    [ordered]@{ id = "ten_band_bar_direction_correct"; description = "All ten bar directions are correct" },
    [ordered]@{ id = "selected_band_border_correct"; description = "Selected-band border is correct" },
    [ordered]@{ id = "draft_applied_distinction"; description = "Draft and applied states are distinguishable" },
    [ordered]@{ id = "custom_state_correct"; description = "CUSTOM state is displayed correctly" },
    [ordered]@{ id = "page_switch_no_long_freeze"; description = "Page switching has no long full-screen freeze" },
    [ordered]@{ id = "final_page_layout_complete"; description = "Layout is complete after final tile" },
    [ordered]@{ id = "no_visible_flicker"; description = "No obvious flicker" },
    [ordered]@{ id = "touch_hitbox_matches_rendered_control"; description = "Touch hitboxes align with rendered controls" }
)
$visualItems | ForEach-Object {
    "[PENDING] $($_.id): $($_.description)"
} | Set-Content -LiteralPath $visualChecklistPath -Encoding ascii
[ordered]@{
    result_label = "PENDING_OPERATOR"
    automated_counters_are_not_visual_evidence = $true
    items = @($visualItems | ForEach-Object {
        [ordered]@{
            id = $_.id
            description = $_.description
            status = "PENDING_OPERATOR"
            user_conclusion = ""
        }
    })
    photo_evidence = @([ordered]@{
        file_name = ""
        captured_at = ""
        size_bytes = 0
        sha256 = ""
        stage = ""
        user_conclusion = ""
    })
} | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath `
    $visualSummaryPath -Encoding utf8

@"
Wait for each CALIBRATION_REQUIRED or ACTION_REQUIRED line before touching.
Touch calibration: LEFT_TOP, RIGHT_TOP, LEFT_BOTTOM, RIGHT_BOTTOM, CENTER.
For calibration, hold until CALIBRATION_RELEASE, then release and keep the
finger off the screen until the next prompt. For actions, use one complete
press/release for every prompt.
Dynamic sequence: five presets, three toggles/strength controls, page switch.
Editor sequence: all ten bands, single-band +2 dB Apply/Reset, then the exact
multi-band CUSTOM vector. A ten-minute interactive phase requests one action
per minute. The script also performs controlled latest-wins/page stress and
then a separate uninterrupted objective endurance run.
Complete operator_visual_checklist.txt separately. DSS counters cannot prove
visual alignment, absence of drift/flicker, text integrity, or Touch feel.
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
    expected_commit_sha = $expected
    expected_dirty = 0
    expected_init_stage = 11
    build_profile = "H_project33_full"
    build_warning_count = $buildResult.warning_count
    build_error_count = $buildResult.error_count
    link_errors = $buildResult.link_errors
    build_defines = $buildResult.defines
    job_count = $buildResult.job_count
    dynamic_hitbox_count = $buildResult.dynamic_hitbox_count
    editor_hitbox_count = $buildResult.editor_hitbox_count
    framebuffer_count = $buildResult.framebuffer_symbol_count
    framebuffer_symbols = $buildResult.framebuffer_symbol_names
    second_framebuffer_count = $buildResult.second_framebuffer_symbol_hits
    framebuffer_bytes = $buildResult.framebuffer_bytes
    offscreen_buffer_object_count = $buildResult.offscreen_buffer_object_count
    offscreen_buffer_bytes = $buildResult.offscreen_buffer_bytes
    editor_state_bytes = $buildResult.editor_state_bytes
    program = $program
    program_sha256 = $programSha256
    build_matrix_summary = $matrixSummaryPath
    retained_size_symbols = $retainedSizeSymbols
    retained_size_symbols_missing = @()
    required_symbol_presence = $requiredSymbolPresence
    lcd_double_buffer_debug_symbols = $lcdDoubleBufferDebugSymbols
    endurance_minutes = $EnduranceMinutes
    interaction_timeout_seconds = $InteractionTimeoutSeconds
    operator_instructions = $instructionPath
    operator_visual_checklist = $visualChecklistPath
    operator_visual_summary = $visualSummaryPath
} | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath `
    (Join-Path $resultDir "provenance.json") -Encoding utf8

$player = [System.Media.SoundPlayer]::new($wavePath)
$dssProcess = $null
$timedOut = $false
try {
    # This is the final provenance boundary before DSS loads the program.
    Assert-ExactGitState "pre-load" | Out-Null
    Assert-BuildIdentity "pre-load"
    $preLoadProgramSha = (Get-FileHash -Algorithm SHA256 `
        -LiteralPath $program).Hash.ToLowerInvariant()
    if ($preLoadProgramSha -ne $programSha256) {
        throw "H .out changed after clean build and before load"
    }

    $player.Load()
    $player.PlayLooping()
    $env:DSP_TEN_BAND_EDITOR_HARDWARE_AUTH = `
        "C6748_CONNECTED_AND_AUDIO_LOOP_READY"
    $env:DSP_TEST_ROOT = $root -replace '\\', '/'
    $env:DSP_TEST_CCXML = $ccxml -replace '\\', '/'
    $env:DSP_TEST_PROGRAM = $program -replace '\\', '/'
    $env:DSP_TEST_RESULT_DIR = $resultDir -replace '\\', '/'
    $env:DSP_TEST_EXPECTED_SHA = $expected
    $env:DSP_TEST_PROGRAM_SHA256 = $programSha256
    $env:DSP_TEST_BUILD_PROFILE = "H_project33_full"
    $env:DSP_TEST_ENDURANCE_MINUTES = [string]$EnduranceMinutes
    $env:DSP_TEST_INTERACTION_TIMEOUT_SECONDS = `
        [string]$InteractionTimeoutSeconds

    Write-Output "RESULT_DIR=$resultDir"
    Write-Output "INSTRUCTIONS=$instructionPath"
    Write-Output "OPERATOR_VISUAL_CHECKLIST=$visualChecklistPath"
    Write-Output "Wait for each calibration/action prompt before touching."

    $arguments = @('/d', '/s', '/c', ('""{0}" "{1}""' -f $dss, $dssScript))
    $dssProcess = Start-Process -FilePath $env:ComSpec `
        -ArgumentList $arguments -PassThru -NoNewWindow
    if (-not $dssProcess.WaitForExit($DssTimeoutMinutes * 60 * 1000)) {
        $timedOut = $true
        & taskkill.exe /PID $dssProcess.Id /T /F | Out-Null
        throw "DSS exceeded the $DssTimeoutMinutes-minute timeout"
    }
    $dssProcess.WaitForExit()
    $dssProcess.Refresh()
    if ($dssProcess.ExitCode -ne 0) {
        throw "DSS exited with code $($dssProcess.ExitCode)"
    }

    $summaryPath = Join-Path $resultDir "summary.json"
    if (-not (Test-Path -LiteralPath $summaryPath)) {
        throw "DSS produced no summary"
    }
    $summary = Get-Content -Raw -LiteralPath $summaryPath | ConvertFrom-Json
    if (($summary.completed -ne $true) -or
        ($summary.result_label -ne "AUTOMATED_BOARD_VALIDATION_COMPLETE")) {
        throw "Automated hardware validation did not complete: $($summary.error)"
    }
    Write-Output "AUTOMATED_RESULT_LABEL=$($summary.result_label)"
    Write-Output "OPERATOR_VISUAL_RESULT=PENDING_OPERATOR"
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
        Env:DSP_TEST_PROGRAM_SHA256, Env:DSP_TEST_BUILD_PROFILE,
        Env:DSP_TEST_ENDURANCE_MINUTES,
        Env:DSP_TEST_INTERACTION_TIMEOUT_SECONDS -ErrorAction SilentlyContinue
}
