param(
    [string]$ResultDir = "",
    [string]$Program = "",
    [switch]$SkipBuild,
    [switch]$KernelDiagnostics
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$ccsRoot = "D:\SoftwareDownload\CCS_20.5.0.00028_win\ccs"
$gmake = Join-Path $ccsRoot "utils\bin\gmake.exe"
$dss = Join-Path $ccsRoot "ccs_base\scripting\bin\dss.bat"
$js = Join-Path $PSScriptRoot "dss\dss_harshness_guard_benchmark.js"
$analyzer = Join-Path $PSScriptRoot "analyze_harshness_guard_benchmark.py"
$buildIdGenerator = Join-Path $PSScriptRoot "generate_equalizer_build_id.ps1"
$ccxml = Join-Path $root "TargetConfig\C6748.ccxml"
$expectedSha = (git -C $root rev-parse --short HEAD).Trim()
if ($LASTEXITCODE -ne 0) {
    throw "Unable to resolve the current Git SHA"
}

if ([string]::IsNullOrWhiteSpace($ResultDir)) {
    $stamp = Get-Date -Format "yyyyMMdd_HHmmss"
    $ResultDir = Join-Path $env:TEMP `
        "DSP_LAB_0507\harshness_guard_benchmark\$stamp"
}
$resultPath = [IO.Path]::GetFullPath($ResultDir)
$tempRoot = [IO.Path]::GetFullPath($env:TEMP).TrimEnd('\') + '\'
if (-not $resultPath.StartsWith(
    $tempRoot, [StringComparison]::OrdinalIgnoreCase)) {
    throw "ResultDir must stay under the current TEMP directory"
}
New-Item -ItemType Directory -Force -Path $resultPath | Out-Null

foreach ($required in @($gmake,$dss,$js,$analyzer,$buildIdGenerator,$ccxml)) {
    if (-not (Test-Path -LiteralPath $required)) {
        throw "Missing required path: $required"
    }
}

$kernelDiagnosticsValue = if ($KernelDiagnostics) { "1" } else { "0" }
$defines = @(
    "--define=DSP_LAB_PROJECT_SELECT=33",
    "--define=EQ_ENABLE_LCD_DISPLAY=0",
    "--define=EQ_ENABLE_AUDIO_FEATURE_ANALYZER=1",
    "--define=EQ_ENABLE_SMART_BASS=1",
    "--define=EQ_ENABLE_DYNAMIC_CLARITY=1",
    "--define=EQ_ENABLE_HARSHNESS_GUARD=1",
    "--define=EQ_ENABLE_DYNAMIC_CLARITY_BENCHMARK=1",
    "--define=EQ_ENABLE_HARSHNESS_GUARD_BENCHMARK=1",
    "--define=EQ_ENABLE_HARSHNESS_GUARD_KERNEL_DIAGNOSTICS=$kernelDiagnosticsValue",
    "--define=EQ_USE_GENERATED_BUILD_ID=1"
) -join ' '

if (-not $SkipBuild) {
    $dirtyPaths = git -C $root status --porcelain --untracked-files=normal
    if ($LASTEXITCODE -ne 0 -or
        -not [string]::IsNullOrWhiteSpace(($dirtyPaths -join "`n"))) {
        throw "Harshness Guard benchmark requires a clean worktree"
    }
    & $buildIdGenerator | Out-Host
    & $gmake -B -C (Join-Path $root "Debug") all `
        "GEN_OPTS__FLAG=$defines"
    if ($LASTEXITCODE -ne 0) {
        throw "CCS benchmark build failed with exit code $LASTEXITCODE"
    }
    $Program = Join-Path $root "Debug\DSP_LAB_0507.out"
}
if ([string]::IsNullOrWhiteSpace($Program) -or
    (-not (Test-Path -LiteralPath $Program))) {
    throw "Benchmark program is missing: $Program"
}

$programCopy = Join-Path $resultPath "DSP_LAB_0507_harshness_benchmark.out"
Copy-Item -LiteralPath $Program -Destination $programCopy -Force
$outputHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $programCopy).Hash
$stdout = Join-Path $resultPath "dss_stdout.log"
$stderr = Join-Path $resultPath "dss_stderr.log"

$env:DSP_TEST_ROOT = ($root -replace '\\','/')
$env:DSP_TEST_CCXML = ($ccxml -replace '\\','/')
$env:DSP_TEST_PROGRAM = ($programCopy -replace '\\','/')
$env:DSP_TEST_RESULT_DIR = ($resultPath -replace '\\','/')
$env:DSP_TEST_OUTPUT_SHA256 = $outputHash
$env:DSP_TEST_EXPECTED_SHA = $expectedSha
$env:DSP_TEST_KERNEL_DIAGNOSTICS = $kernelDiagnosticsValue
try {
    $command = 'call "{0}" "{1}"' -f $dss,$js
    $process = Start-Process -FilePath $env:ComSpec `
        -ArgumentList @('/d','/s','/c',$command) -PassThru `
        -WindowStyle Hidden -RedirectStandardOutput $stdout `
        -RedirectStandardError $stderr
    if (-not $process.WaitForExit(900000)) {
        & taskkill.exe /PID $process.Id /T /F | Out-Null
        throw "DSS benchmark timed out"
    }
    $process.Refresh()
    if ($process.ExitCode -ne 0) {
        $boardPath = Join-Path $resultPath `
            "harshness_guard_benchmark_board.json"
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
            throw "DSS benchmark failed: $details"
        }
    }
}
finally {
    Remove-Item Env:DSP_TEST_ROOT,Env:DSP_TEST_CCXML,
        Env:DSP_TEST_PROGRAM,Env:DSP_TEST_RESULT_DIR,
        Env:DSP_TEST_OUTPUT_SHA256,Env:DSP_TEST_EXPECTED_SHA,
        Env:DSP_TEST_KERNEL_DIAGNOSTICS `
        -ErrorAction SilentlyContinue
}

& python $analyzer $resultPath
if ($LASTEXITCODE -ne 0) {
    throw "Benchmark analysis failed with exit code $LASTEXITCODE"
}

[pscustomobject]@{
    pass = $true
    evidence_label = "BOARD_ISOLATED_THREE_MODULE_MICROBENCHMARK"
    result_dir = $resultPath
    report = Join-Path $resultPath "harshness_guard_benchmark.json"
    output_sha256 = $outputHash
    expected_sha = $expectedSha
    kernel_diagnostics = [bool]$KernelDiagnostics
    subjective_observation = "NOT_PERFORMED"
} | ConvertTo-Json -Depth 4
