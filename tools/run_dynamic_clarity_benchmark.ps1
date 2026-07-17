param(
    [string]$ResultDir = "",
    [string]$Program = "",
    [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$ccsRoot = "D:\SoftwareDownload\CCS_20.5.0.00028_win\ccs"
$gmake = Join-Path $ccsRoot "utils\bin\gmake.exe"
$dss = Join-Path $ccsRoot "ccs_base\scripting\bin\dss.bat"
$js = Join-Path $PSScriptRoot "dss\dss_dynamic_clarity_benchmark.js"
$analyzer = Join-Path $PSScriptRoot "analyze_dynamic_clarity_benchmark.py"
$ccxml = Join-Path $root "TargetConfig\C6748.ccxml"

if ([string]::IsNullOrWhiteSpace($ResultDir)) {
    $stamp = Get-Date -Format "yyyyMMdd_HHmmss"
    $ResultDir = Join-Path $env:TEMP `
        "DSP_LAB_0507\dynamic_clarity_benchmark\$stamp"
}
$resultPath = [IO.Path]::GetFullPath($ResultDir)
$tempRoot = [IO.Path]::GetFullPath($env:TEMP).TrimEnd('\') + '\'
if (-not $resultPath.StartsWith(
    $tempRoot, [StringComparison]::OrdinalIgnoreCase)) {
    throw "ResultDir must stay under the current TEMP directory"
}
New-Item -ItemType Directory -Force -Path $resultPath | Out-Null

foreach ($required in @($gmake,$dss,$js,$analyzer,$ccxml)) {
    if (-not (Test-Path -LiteralPath $required)) {
        throw "Missing required path: $required"
    }
}

$defines = @(
    "--define=DSP_LAB_PROJECT_SELECT=33",
    "--define=EQ_ENABLE_LCD_DISPLAY=0",
    "--define=EQ_ENABLE_AUDIO_FEATURE_ANALYZER=1",
    "--define=EQ_ENABLE_SMART_BASS=1",
    "--define=EQ_ENABLE_DYNAMIC_CLARITY=1",
    "--define=EQ_ENABLE_DYNAMIC_CLARITY_BENCHMARK=1"
) -join ' '

if (-not $SkipBuild) {
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

$programCopy = Join-Path $resultPath "DSP_LAB_0507_benchmark.out"
Copy-Item -LiteralPath $Program -Destination $programCopy -Force
$outputHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $programCopy).Hash
$stdout = Join-Path $resultPath "dss_stdout.log"
$stderr = Join-Path $resultPath "dss_stderr.log"

$env:DSP_TEST_ROOT = ($root -replace '\\','/')
$env:DSP_TEST_CCXML = ($ccxml -replace '\\','/')
$env:DSP_TEST_PROGRAM = ($programCopy -replace '\\','/')
$env:DSP_TEST_RESULT_DIR = ($resultPath -replace '\\','/')
$env:DSP_TEST_OUTPUT_SHA256 = $outputHash
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
            "dynamic_clarity_benchmark_board.json"
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
        Env:DSP_TEST_OUTPUT_SHA256 -ErrorAction SilentlyContinue
}

& python $analyzer $resultPath
if ($LASTEXITCODE -ne 0) {
    throw "Benchmark analysis failed with exit code $LASTEXITCODE"
}

[pscustomobject]@{
    pass = $true
    evidence_label = "BOARD_ISOLATED_FUNCTION_MICROBENCHMARK"
    result_dir = $resultPath
    report = Join-Path $resultPath "dynamic_clarity_benchmark.json"
    output_sha256 = $outputHash
    subjective_observation = "NOT_PERFORMED"
} | ConvertTo-Json -Depth 4
