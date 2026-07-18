param(
    [string]$RepoRoot = "",
    [string]$GmakePath =
        "D:\SoftwareDownload\CCS_20.5.0.00028_win\ccs\utils\bin\gmake.exe",
    [string]$NmPath =
        "D:\SoftwareDownload\CCS_20.5.0.00028_win\ccs\tools\compiler\ti-cgt-c6000_8.5.0.LTS\bin\nm6x.exe",
    [string]$OutputDirectory = ""
)

$ErrorActionPreference = "Stop"
if ([string]::IsNullOrWhiteSpace($RepoRoot)) {
    $RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
} else {
    $RepoRoot = (Resolve-Path $RepoRoot).Path
}
if ([string]::IsNullOrWhiteSpace($OutputDirectory)) {
    $stamp = Get-Date -Format "yyyyMMdd_HHmmss"
    $OutputDirectory = Join-Path $env:TEMP "equalizer_ui_build_matrix_$stamp"
}
if (Test-Path -LiteralPath $OutputDirectory) {
    throw "Output directory already exists: $OutputDirectory"
}
if (-not (Test-Path -LiteralPath $GmakePath)) {
    throw "gmake not found: $GmakePath"
}
if (-not (Test-Path -LiteralPath $NmPath)) {
    throw "nm6x not found: $NmPath"
}

$status = git -C $RepoRoot status --porcelain --untracked-files=normal
if ($LASTEXITCODE -ne 0 -or
    -not [string]::IsNullOrWhiteSpace(($status -join "`n"))) {
    throw "Build matrix requires a clean tracked worktree"
}

New-Item -ItemType Directory -Path $OutputDirectory | Out-Null
& (Join-Path $RepoRoot "tools\generate_equalizer_build_id.ps1") | Out-Host

$diagnosticsOff = @(
    "--define=EQ_ENABLE_DYNAMIC_CLARITY_TIMING_DIAGNOSTICS=0"
    "--define=EQ_ENABLE_DYNAMIC_CLARITY_TRANSITION_CAPTURE=0"
    "--define=EQ_ENABLE_HARSHNESS_GUARD_TRANSITION_CAPTURE=0"
    "--define=EQ_ENABLE_HARSHNESS_GUARD_KERNEL_DIAGNOSTICS=0"
    "--define=EQ_ENABLE_FOUR_WAY_TRANSITION_DIAGNOSTICS=0"
    "--define=EQ_ENABLE_DYNAMIC_CLARITY_BENCHMARK=0"
    "--define=EQ_ENABLE_HARSHNESS_GUARD_BENCHMARK=0"
    "--define=EQ_ENABLE_UART_DIAGNOSTICS=0"
) -join " "
$project33 = @(
    "--define=DSP_LAB_PROJECT_SELECT=33"
    "--define=EQ_USE_GENERATED_BUILD_ID=1"
    "--define=EQ_ENABLE_AUDIO_FEATURE_ANALYZER=1"
    "--define=EQ_ENABLE_SMART_BASS=1"
    "--define=EQ_ENABLE_DYNAMIC_CLARITY=1"
    "--define=EQ_ENABLE_HARSHNESS_GUARD=1"
) -join " "
$profiles = @(
    [pscustomobject]@{
        name = "A_project32"
        defines = @(
            "--define=DSP_LAB_PROJECT_SELECT=32"
            "--define=EQ_USE_GENERATED_BUILD_ID=1"
            "--define=EQ_ENABLE_AUDIO_FEATURE_ANALYZER=0"
            "--define=EQ_ENABLE_SMART_BASS=0"
            "--define=EQ_ENABLE_DYNAMIC_CLARITY=0"
            "--define=EQ_ENABLE_HARSHNESS_GUARD=0"
            "--define=EQ_ENABLE_LCD_DISPLAY=0"
            "--define=EQ_ENABLE_PROJECT33_TOUCH=0"
            $diagnosticsOff
        ) -join " "
    }
    [pscustomobject]@{
        name = "B_project33_lcd_off"
        defines = "$project33 --define=EQ_ENABLE_LCD_DISPLAY=0 " +
            "--define=EQ_ENABLE_PROJECT33_TOUCH=0 " +
            "--define=EQ_UI_RUNTIME_DEFAULT_MASK=0 $diagnosticsOff"
    }
    [pscustomobject]@{
        name = "C_project33_static"
        defines = "$project33 --define=EQ_ENABLE_LCD_DISPLAY=1 " +
            "--define=EQ_ENABLE_PROJECT33_TOUCH=0 " +
            "--define=EQ_UI_RUNTIME_DEFAULT_MASK=0 $diagnosticsOff"
    }
    [pscustomobject]@{
        name = "D_project33_dynamic"
        defines = "$project33 --define=EQ_ENABLE_LCD_DISPLAY=1 " +
            "--define=EQ_ENABLE_PROJECT33_TOUCH=0 " +
            "--define=EQ_UI_RUNTIME_DEFAULT_MASK=15 $diagnosticsOff"
    }
    [pscustomobject]@{
        name = "E_project33_touch"
        defines = "$project33 --define=EQ_ENABLE_LCD_DISPLAY=1 " +
            "--define=EQ_ENABLE_PROJECT33_TOUCH=1 " +
            "--define=EQ_UI_RUNTIME_DEFAULT_MASK=15 $diagnosticsOff"
    }
)

function Get-MapSectionSize {
    param([string]$MapPath, [string]$Section)

    $pattern = '^\s*[0-9a-fA-F]+\s+[0-9a-fA-F]+\s+' +
        '[0-9a-fA-F]+\s+[0-9a-fA-F]+\s+\S+\s+' +
        [regex]::Escape($Section) + '\s*$'
    $line = Get-Content -LiteralPath $MapPath |
        Where-Object { $_ -match $pattern } | Select-Object -First 1
    if (-not $line) {
        throw "Map section not found: $Section"
    }
    $parts = $line.Trim() -split '\s+'
    return [Convert]::ToInt64($parts[2], 16)
}

$results = @()
Push-Location $RepoRoot
try {
    foreach ($profile in $profiles) {
        Write-Host "BEGIN $($profile.name)"
        $cleanOutput = & $GmakePath -C Debug clean 2>&1
        if ($LASTEXITCODE -ne 0) {
            throw "Clean failed: $($profile.name)"
        }
        Remove-Item -ErrorAction SilentlyContinue -LiteralPath @(
            "Debug\Code\User\user_dsp\user_equalizer_ui_logic.obj"
            "Debug\Code\User\user_dsp\user_equalizer_ui_logic.d"
        )
        $buildOutput = & $GmakePath -B -C Debug all `
            "GEN_OPTS__FLAG=$($profile.defines)" 2>&1
        $buildExit = $LASTEXITCODE
        $logPath = Join-Path $OutputDirectory "$($profile.name).log"
        @($cleanOutput) + @($buildOutput) |
            Set-Content -LiteralPath $logPath -Encoding UTF8
        if ($buildExit -ne 0) {
            throw "Build failed: $($profile.name); see $logPath"
        }

        $mapPath = Join-Path $RepoRoot "Debug\DSP_LAB_0507.map"
        $outPath = Join-Path $RepoRoot "Debug\DSP_LAB_0507.out"
        $linkPath = Join-Path $RepoRoot "Debug\DSP_LAB_0507_linkInfo.xml"
        foreach ($artifact in @($mapPath, $outPath, $linkPath)) {
            if (-not (Test-Path -LiteralPath $artifact)) {
                throw "Missing artifact: $artifact"
            }
        }
        $linkText = Get-Content -Raw -LiteralPath $linkPath
        $linkErrors = [regex]::Match(
            $linkText, '<link_errors>([^<]+)</link_errors>').Groups[1].Value
        $symbols = (& $NmPath $outPath 2>&1) -join "`n"
        $warningCount = @($buildOutput |
            Select-String -Pattern 'warning #').Count
        $errorCount = @($buildOutput |
            Select-String -Pattern 'error #|undefined symbol').Count
        $mapCopy = Join-Path $OutputDirectory "$($profile.name).map"
        $linkCopy = Join-Path $OutputDirectory `
            "$($profile.name)_linkInfo.xml"
        Copy-Item -LiteralPath $mapPath -Destination $mapCopy
        Copy-Item -LiteralPath $linkPath -Destination $linkCopy

        $result = [ordered]@{
            profile = $profile.name
            exit_code = $buildExit
            warning_count = $warningCount
            error_count = $errorCount
            link_errors = $linkErrors
            text_bytes = Get-MapSectionSize $mapPath ".text"
            const_bytes = Get-MapSectionSize $mapPath ".const"
            bss_bytes = Get-MapSectionSize $mapPath ".bss"
            subband_l2_bytes = Get-MapSectionSize $mapPath ".subband_l2"
            lcd_symbol_hits = ([regex]::Matches(
                $symbols, '(?m)^.*\bEQ_DebugLcd\w*\s*$')).Count
            touch_symbol_hits = ([regex]::Matches(
                $symbols, '(?m)^.*\bEQ_DebugTouch\w*\s*$')).Count
            ui_logic_symbol_hits = ([regex]::Matches(
                $symbols, '(?m)^.*\bEqualizerUi\w*\s*$')).Count
            out_size = (Get-Item -LiteralPath $outPath).Length
            out_sha256 = (Get-FileHash -Algorithm SHA256 `
                -LiteralPath $outPath).Hash.ToLowerInvariant()
            defines = $profile.defines
            log_path = $logPath
            map_path = $mapCopy
        }
        $results += [pscustomobject]$result
        Write-Host ("END {0} warning={1} link={2}" -f `
            $profile.name, $warningCount, $linkErrors)
        if (($warningCount -ne 0) -or ($errorCount -ne 0) -or
            ($linkErrors -ne "0x0")) {
            throw "Diagnostics failed: $($profile.name)"
        }
    }
} finally {
    Pop-Location
}

$summaryPath = Join-Path $OutputDirectory "build_matrix_summary.json"
$results | ConvertTo-Json -Depth 5 |
    Set-Content -LiteralPath $summaryPath -Encoding UTF8
$results | Format-Table profile, warning_count, link_errors,
    text_bytes, const_bytes, bss_bytes, subband_l2_bytes,
    lcd_symbol_hits, touch_symbol_hits, ui_logic_symbol_hits -AutoSize
Write-Output $summaryPath
