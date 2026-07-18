param(
    [string]$RepoRoot = "",
    [string]$CcsRoot = "D:\SoftwareDownload\CCS_20.5.0.00028_win\ccs",
    [ValidateRange(1, 60)]
    [int]$DurationMinutes = 10
)

$ErrorActionPreference = "Stop"
$root = if ([string]::IsNullOrWhiteSpace($RepoRoot)) {
    (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
} else {
    (Resolve-Path $RepoRoot).Path
}
$gmake = Join-Path $CcsRoot "utils\bin\gmake.exe"
$dss = Join-Path $CcsRoot "ccs_base\scripting\bin\dss.bat"
$nm = Join-Path $CcsRoot `
    "tools\compiler\ti-cgt-c6000_8.5.0.LTS\bin\nm6x.exe"
$generator = Join-Path $root "tools\generate_equalizer_build_id.ps1"
$script = Join-Path $root "tools\dss\dss_equalizer_lcd_alignment.js"
$program = Join-Path $root "Debug\DSP_LAB_0507.out"
$map = Join-Path $root "Debug\DSP_LAB_0507.map"
$linkInfo = Join-Path $root "Debug\DSP_LAB_0507_linkInfo.xml"
$ccxml = Join-Path $root "TargetConfig\C6748.ccxml"

foreach ($path in @($gmake, $dss, $nm, $generator, $script, $ccxml)) {
    if (-not (Test-Path -LiteralPath $path)) {
        throw "Missing path: $path"
    }
}
$dirty = @(& git -C $root status --porcelain --untracked-files=normal)
if ($LASTEXITCODE -ne 0 -or $dirty.Count -ne 0) {
    throw "Alignment hardware validation requires a clean worktree"
}

& $generator | Out-Host
$head = (& git -C $root rev-parse HEAD).Trim()
$shortSha = $head.Substring(0, 7)
$stamp = Get-Date -Format "yyyyMMdd_HHmmss"
$resultDir = Join-Path $env:TEMP `
    "DSP_LAB_0507\lcd_alignment\${shortSha}_$stamp"
New-Item -ItemType Directory -Path $resultDir -Force | Out-Null
$buildLog = Join-Path $resultDir "ccs_build.log"

$defines = @(
    "--define=DSP_LAB_PROJECT_SELECT=33"
    "--define=EQ_USE_GENERATED_BUILD_ID=1"
    "--define=EQ_ENABLE_AUDIO_FEATURE_ANALYZER=1"
    "--define=EQ_ENABLE_SMART_BASS=1"
    "--define=EQ_ENABLE_DYNAMIC_CLARITY=1"
    "--define=EQ_ENABLE_HARSHNESS_GUARD=1"
    "--define=EQ_ENABLE_LCD_DISPLAY=1"
    "--define=EQ_ENABLE_PROJECT33_TOUCH=0"
    "--define=EQ_UI_RUNTIME_DEFAULT_MASK=0"
    "--define=EQ_LCD_DIAGNOSTIC_ALIGNMENT_PATTERN=1"
    "--define=EQ_ENABLE_DYNAMIC_CLARITY_TIMING_DIAGNOSTICS=0"
    "--define=EQ_ENABLE_DYNAMIC_CLARITY_TRANSITION_CAPTURE=0"
    "--define=EQ_ENABLE_HARSHNESS_GUARD_TRANSITION_CAPTURE=0"
    "--define=EQ_ENABLE_HARSHNESS_GUARD_KERNEL_DIAGNOSTICS=0"
    "--define=EQ_ENABLE_FOUR_WAY_TRANSITION_DIAGNOSTICS=0"
    "--define=EQ_ENABLE_DYNAMIC_CLARITY_BENCHMARK=0"
    "--define=EQ_ENABLE_HARSHNESS_GUARD_BENCHMARK=0"
    "--define=EQ_ENABLE_UART_DIAGNOSTICS=0"
) -join " "

& $gmake -B -C (Join-Path $root "Debug") all `
    "GEN_OPTS__FLAG=$defines" 2>&1 | Tee-Object -FilePath $buildLog | Out-Host
if ($LASTEXITCODE -ne 0) {
    throw "CCS alignment build failed"
}
foreach ($path in @($program, $map, $linkInfo)) {
    if (-not (Test-Path -LiteralPath $path)) {
        throw "Missing build artifact: $path"
    }
}
$buildText = Get-Content -Raw -LiteralPath $buildLog
$warnings = @($buildText -split "`r?`n" | Where-Object {
    $_ -match '(?i)(warning\s*#|warning:|remark\s*#)' -and
    $_ -notmatch '0 Warning'
})
$errors = @($buildText -split "`r?`n" | Where-Object {
    $_ -match '(?i)(error\s*#|error:)' -and $_ -notmatch '0 Error'
})
if ($warnings.Count -ne 0 -or $errors.Count -ne 0) {
    throw "CCS diagnostics are nonzero: warnings=$($warnings.Count), errors=$($errors.Count)"
}
if ((Get-Content -Raw -LiteralPath $linkInfo) -notmatch
    '<link_errors>0x0</link_errors>') {
    throw "CCS link_errors is nonzero"
}
$symbols = (& $nm $program) -join "`n"
foreach ($required in @(
    "EQ_DebugLcdAlignmentPatternEnabled",
    "EQ_DebugLcdBufferAddress",
    "EQ_DebugLcdStartupRasterStatus",
    "EQ_DebugLcdFramebufferCanaryCheckCount"
)) {
    if ($symbols -notmatch [regex]::Escape($required)) {
        throw "Alignment symbol missing: $required"
    }
}
if ($symbols -match 'EQ_DebugTouchActionCount') {
    throw "Static no-touch profile unexpectedly contains Touch state"
}

$outHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $program).Hash
$buildSummary = [ordered]@{
    git_sha = $head
    git_short_sha = $shortSha
    build_dirty = 0
    profile = "project33_lcd_alignment_static_no_touch"
    warning_count = $warnings.Count
    error_count = $errors.Count
    link_errors = "0x0"
    out_sha256 = $outHash
    duration_minutes = $DurationMinutes
}
$buildSummary | ConvertTo-Json -Depth 4 |
    Set-Content -LiteralPath (Join-Path $resultDir "build_summary.json") `
        -Encoding UTF8

$resultPath = Join-Path $resultDir "lcd_alignment_result.json"
$stdout = Join-Path $resultDir "dss_stdout.log"
$stderr = Join-Path $resultDir "dss_stderr.log"
$process = $null
try {
    $env:DSP_TEST_ROOT = $root -replace '\\', '/'
    $env:DSP_TEST_CCXML = $ccxml -replace '\\', '/'
    $env:DSP_TEST_PROGRAM = $program -replace '\\', '/'
    $env:DSP_TEST_RESULT_PATH = $resultPath -replace '\\', '/'
    $env:DSP_TEST_EXPECTED_SHA = $shortSha
    $env:DSP_TEST_EXPECTED_DIRTY = "0"
    $env:DSP_TEST_DURATION_MS = [string]($DurationMinutes * 60 * 1000)
    $arguments = @('/d', '/s', '/c', ('""{0}" "{1}""' -f $dss, $script))
    $process = Start-Process -FilePath $env:ComSpec -ArgumentList $arguments `
        -PassThru -WindowStyle Hidden -RedirectStandardOutput $stdout `
        -RedirectStandardError $stderr
    $timeoutMs = ($DurationMinutes + 5) * 60 * 1000
    if (-not $process.WaitForExit($timeoutMs)) {
        & taskkill.exe /PID $process.Id /T /F | Out-Null
        throw "DSS alignment test timed out"
    }
    $process.WaitForExit()
    if (-not (Test-Path -LiteralPath $resultPath)) {
        throw "DSS produced no alignment result"
    }
    $result = Get-Content -Raw -LiteralPath $resultPath | ConvertFrom-Json
    if ($process.ExitCode -ne 0 -or -not $result.pass) {
        throw "DSS alignment test failed: $($result.error)"
    }
    Write-Output "RESULT_DIR=$resultDir"
    Write-Output "RESULT=$resultPath"
    Write-Output "OUT_SHA256=$outHash"
    Write-Output "OPERATOR_VISUAL_OBSERVATION=PENDING"
} finally {
    if ($null -ne $process -and -not $process.HasExited) {
        & taskkill.exe /PID $process.Id /T /F | Out-Null
    }
    Remove-Item Env:DSP_TEST_ROOT, Env:DSP_TEST_CCXML,
        Env:DSP_TEST_PROGRAM, Env:DSP_TEST_RESULT_PATH,
        Env:DSP_TEST_EXPECTED_SHA, Env:DSP_TEST_EXPECTED_DIRTY,
        Env:DSP_TEST_DURATION_MS -ErrorAction SilentlyContinue
}
