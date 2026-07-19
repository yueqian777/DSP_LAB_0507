param(
    [string]$RepoRoot = "",
    [string]$GmakePath =
        "D:\SoftwareDownload\CCS_20.5.0.00028_win\ccs\utils\bin\gmake.exe",
    [string]$NmPath =
        "D:\SoftwareDownload\CCS_20.5.0.00028_win\ccs\tools\compiler\ti-cgt-c6000_8.5.0.LTS\bin\nm6x.exe",
    [string]$OutputDirectory = "",
    [string[]]$ProfileNames = @()
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
$statusAfterBuildId = git -C $RepoRoot status --porcelain `
    --untracked-files=normal
if ($LASTEXITCODE -ne 0 -or
    -not [string]::IsNullOrWhiteSpace(($statusAfterBuildId -join "`n"))) {
    throw "Generated build identity changed the clean tracked worktree"
}

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
        lcd_enabled = 0
        touch_enabled = 0
        editor_enabled = 0
        editor_touch_enabled = 0
        runtime_mask = 0
        expected_job_count = 0
        expected_dynamic_hitbox_count = 0
        expected_editor_hitbox_count = 0
        expected_framebuffer_count = 1
        defines = @(
            "--define=DSP_LAB_PROJECT_SELECT=32"
            "--define=EQ_USE_GENERATED_BUILD_ID=1"
            "--define=EQ_ENABLE_AUDIO_FEATURE_ANALYZER=0"
            "--define=EQ_ENABLE_SMART_BASS=0"
            "--define=EQ_ENABLE_DYNAMIC_CLARITY=0"
            "--define=EQ_ENABLE_HARSHNESS_GUARD=0"
            "--define=EQ_ENABLE_LCD_DISPLAY=0"
            "--define=EQ_ENABLE_PROJECT33_TOUCH=0"
            "--define=EQ_ENABLE_TEN_BAND_EDITOR=0"
            "--define=EQ_ENABLE_TEN_BAND_EDITOR_TOUCH=0"
            "--define=EQ_UI_RUNTIME_DEFAULT_MASK=0"
            $diagnosticsOff
        ) -join " "
    }
    [pscustomobject]@{
        name = "B_project33_lcd_off"
        lcd_enabled = 0
        touch_enabled = 0
        editor_enabled = 0
        editor_touch_enabled = 0
        runtime_mask = 0
        expected_job_count = 0
        expected_dynamic_hitbox_count = 0
        expected_editor_hitbox_count = 0
        expected_framebuffer_count = 0
        defines = "$project33 --define=EQ_ENABLE_LCD_DISPLAY=0 " +
            "--define=EQ_ENABLE_PROJECT33_TOUCH=0 " +
            "--define=EQ_ENABLE_TEN_BAND_EDITOR=0 " +
            "--define=EQ_ENABLE_TEN_BAND_EDITOR_TOUCH=0 " +
            "--define=EQ_UI_RUNTIME_DEFAULT_MASK=0 " +
            "--define=EQ_LCD_DIAGNOSTIC_ALIGNMENT_PATTERN=0 $diagnosticsOff"
    }
    [pscustomobject]@{
        name = "C_project33_static"
        lcd_enabled = 1
        touch_enabled = 0
        editor_enabled = 0
        editor_touch_enabled = 0
        runtime_mask = 0
        expected_job_count = 15
        expected_dynamic_hitbox_count = 12
        expected_editor_hitbox_count = 0
        expected_framebuffer_count = 1
        defines = "$project33 --define=EQ_ENABLE_LCD_DISPLAY=1 " +
            "--define=EQ_ENABLE_PROJECT33_TOUCH=0 " +
            "--define=EQ_ENABLE_TEN_BAND_EDITOR=0 " +
            "--define=EQ_ENABLE_TEN_BAND_EDITOR_TOUCH=0 " +
            "--define=EQ_UI_RUNTIME_DEFAULT_MASK=0 " +
            "--define=EQ_LCD_DIAGNOSTIC_ALIGNMENT_PATTERN=1 $diagnosticsOff"
    }
    [pscustomobject]@{
        name = "D_project33_dynamic"
        lcd_enabled = 1
        touch_enabled = 0
        editor_enabled = 0
        editor_touch_enabled = 0
        runtime_mask = 15
        expected_job_count = 15
        expected_dynamic_hitbox_count = 12
        expected_editor_hitbox_count = 0
        expected_framebuffer_count = 1
        defines = "$project33 --define=EQ_ENABLE_LCD_DISPLAY=1 " +
            "--define=EQ_ENABLE_PROJECT33_TOUCH=0 " +
            "--define=EQ_ENABLE_TEN_BAND_EDITOR=0 " +
            "--define=EQ_ENABLE_TEN_BAND_EDITOR_TOUCH=0 " +
            "--define=EQ_UI_RUNTIME_DEFAULT_MASK=15 " +
            "--define=EQ_LCD_DIAGNOSTIC_ALIGNMENT_PATTERN=0 $diagnosticsOff"
    }
    [pscustomobject]@{
        name = "E_project33_touch"
        lcd_enabled = 1
        touch_enabled = 1
        editor_enabled = 0
        editor_touch_enabled = 0
        runtime_mask = 15
        expected_job_count = 15
        expected_dynamic_hitbox_count = 12
        expected_editor_hitbox_count = 0
        expected_framebuffer_count = 1
        defines = "$project33 --define=EQ_ENABLE_LCD_DISPLAY=1 " +
            "--define=EQ_ENABLE_PROJECT33_TOUCH=1 " +
            "--define=EQ_ENABLE_TEN_BAND_EDITOR=0 " +
            "--define=EQ_ENABLE_TEN_BAND_EDITOR_TOUCH=0 " +
            "--define=EQ_UI_RUNTIME_DEFAULT_MASK=15 " +
            "--define=EQ_LCD_DIAGNOSTIC_ALIGNMENT_PATTERN=0 $diagnosticsOff"
    }
    [pscustomobject]@{
        name = "F_project33_editor_readonly"
        lcd_enabled = 1
        touch_enabled = 0
        editor_enabled = 1
        editor_touch_enabled = 0
        runtime_mask = 49
        expected_job_count = 27
        expected_dynamic_hitbox_count = 12
        expected_editor_hitbox_count = 20
        expected_framebuffer_count = 1
        defines = "$project33 --define=EQ_ENABLE_LCD_DISPLAY=1 " +
            "--define=EQ_ENABLE_PROJECT33_TOUCH=0 " +
            "--define=EQ_ENABLE_TEN_BAND_EDITOR=1 " +
            "--define=EQ_ENABLE_TEN_BAND_EDITOR_TOUCH=0 " +
            "--define=EQ_UI_RUNTIME_DEFAULT_MASK=49 " +
            "--define=EQ_LCD_DIAGNOSTIC_ALIGNMENT_PATTERN=0 $diagnosticsOff"
    }
    [pscustomobject]@{
        name = "G_project33_editor_touch"
        lcd_enabled = 1
        touch_enabled = 1
        editor_enabled = 1
        editor_touch_enabled = 1
        runtime_mask = 49
        expected_job_count = 27
        expected_dynamic_hitbox_count = 12
        expected_editor_hitbox_count = 20
        expected_framebuffer_count = 1
        defines = "$project33 --define=EQ_ENABLE_LCD_DISPLAY=1 " +
            "--define=EQ_ENABLE_PROJECT33_TOUCH=1 " +
            "--define=EQ_ENABLE_TEN_BAND_EDITOR=1 " +
            "--define=EQ_ENABLE_TEN_BAND_EDITOR_TOUCH=1 " +
            "--define=EQ_UI_RUNTIME_DEFAULT_MASK=49 " +
            "--define=EQ_LCD_DIAGNOSTIC_ALIGNMENT_PATTERN=0 $diagnosticsOff"
    }
    [pscustomobject]@{
        name = "H_project33_full"
        lcd_enabled = 1
        touch_enabled = 1
        editor_enabled = 1
        editor_touch_enabled = 1
        runtime_mask = 63
        expected_job_count = 27
        expected_dynamic_hitbox_count = 12
        expected_editor_hitbox_count = 20
        expected_framebuffer_count = 1
        defines = "$project33 --define=EQ_ENABLE_LCD_DISPLAY=1 " +
            "--define=EQ_ENABLE_PROJECT33_TOUCH=1 " +
            "--define=EQ_ENABLE_TEN_BAND_EDITOR=1 " +
            "--define=EQ_ENABLE_TEN_BAND_EDITOR_TOUCH=1 " +
            "--define=EQ_UI_RUNTIME_DEFAULT_MASK=63 " +
            "--define=EQ_LCD_DIAGNOSTIC_ALIGNMENT_PATTERN=0 $diagnosticsOff"
    }
)

$expectedProfileNames = @(
    "A_project32", "B_project33_lcd_off", "C_project33_static",
    "D_project33_dynamic", "E_project33_touch",
    "F_project33_editor_readonly", "G_project33_editor_touch",
    "H_project33_full"
)
if (($profiles.Count -ne 8) -or
    (($profiles.name -join "|") -ne ($expectedProfileNames -join "|"))) {
    throw "Build matrix profile contract is not A-H"
}
foreach ($profile in $profiles) {
    if (($profile.editor_touch_enabled -ne 0) -and
        (($profile.editor_enabled -eq 0) -or
         ($profile.touch_enabled -eq 0))) {
        throw "Editor Touch requires Editor and Touch: $($profile.name)"
    }
    if (($profile.runtime_mask -band 0x30) -ne 0 -and
        $profile.editor_enabled -eq 0) {
        throw "Editor/page runtime bits require Editor: $($profile.name)"
    }
}

$profilesToBuild = @($profiles)
if ($ProfileNames.Count -gt 0) {
    $requestedNames = @($ProfileNames | ForEach-Object { $_.Trim() } |
        Where-Object { -not [string]::IsNullOrWhiteSpace($_) })
    if ($requestedNames.Count -ne (@($requestedNames | Select-Object -Unique)).Count) {
        throw "ProfileNames contains duplicate profile names"
    }
    $profilesToBuild = @()
    foreach ($requestedName in $requestedNames) {
        if ($expectedProfileNames -notcontains $requestedName) {
            throw "Unknown build profile: $requestedName"
        }
        $profilesToBuild += @($profiles | Where-Object {
            $_.name -eq $requestedName
        })[0]
    }
}

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

function Get-MapSectionInfo {
    param([string]$MapPath, [string]$Section)

    $pattern = '^\s*([0-9a-fA-F]+)\s+([0-9a-fA-F]+)\s+' +
        '([0-9a-fA-F]+)\s+[0-9a-fA-F]+\s+\S+\s+' +
        [regex]::Escape($Section) + '\s*$'
    $line = Get-Content -LiteralPath $MapPath |
        Where-Object { $_ -match $pattern } | Select-Object -First 1
    if (-not $line) {
        throw "Map section not found: $Section"
    }
    $match = [regex]::Match($line, $pattern)
    return [pscustomobject]@{
        start = [Convert]::ToInt64($match.Groups[1].Value, 16)
        size = [Convert]::ToInt64($match.Groups[3].Value, 16)
    }
}

function Get-NmSymbols {
    param([string[]]$Lines)

    $parsed = @()
    $pattern = '^\[\d+\]\s+\|0x([0-9a-fA-F]+)\|' +
        '(\d+)\|([^|]+)\|([^|]+)\|[^|]*\|[^|]*\|(.+)$'
    foreach ($line in $Lines) {
        $match = [regex]::Match([string]$line, $pattern)
        if ($match.Success) {
            $parsed += [pscustomobject]@{
                address = [Convert]::ToInt64($match.Groups[1].Value, 16)
                size = [Convert]::ToInt64($match.Groups[2].Value, 10)
                bind = $match.Groups[3].Value.Trim()
                type = $match.Groups[4].Value.Trim()
                name = $match.Groups[5].Value.Trim()
            }
        }
    }
    return $parsed
}

function Get-SymbolSize {
    param([object[]]$Symbols, [string]$Name)

    $matches = @($Symbols | Where-Object {
        ($_.name -eq $Name) -and
        (($_.type -eq "OBJT") -or ($_.type -eq "COMN"))
    })
    if ($matches.Count -eq 0) {
        return 0L
    }
    if ($matches.Count -ne 1) {
        throw "Expected one object symbol named ${Name}, found $($matches.Count)"
    }
    return [long]$matches[0].size
}

function Test-AddressInSection {
    param([long]$Address, [object]$SectionInfo)

    return (($Address -ge $SectionInfo.start) -and
            ($Address -lt ($SectionInfo.start + $SectionInfo.size)))
}

$results = @()
Push-Location $RepoRoot
try {
    foreach ($profile in $profilesToBuild) {
        Write-Host "BEGIN $($profile.name)"
        $ErrorActionPreference = "Continue"
        $cleanOutput = & $GmakePath -C Debug clean 2>&1
        $cleanExit = $LASTEXITCODE
        $ErrorActionPreference = "Stop"
        if ($cleanExit -ne 0) {
            throw "Clean failed: $($profile.name)"
        }
        Remove-Item -ErrorAction SilentlyContinue -LiteralPath @(
            "Debug\Code\User\user_dsp\user_equalizer_ui_logic.obj"
            "Debug\Code\User\user_dsp\user_equalizer_ui_logic.d"
        )
        $ErrorActionPreference = "Continue"
        $buildOutput = & $GmakePath -B -C Debug all `
            "GEN_OPTS__FLAG=$($profile.defines)" 2>&1
        $buildExit = $LASTEXITCODE
        $ErrorActionPreference = "Stop"
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
        $linkMatch = [regex]::Match(
            $linkText, '<link_errors>([^<]+)</link_errors>')
        if (-not $linkMatch.Success) {
            throw "link_errors missing: $($profile.name)"
        }
        $linkErrors = $linkMatch.Groups[1].Value
        $nmOutput = @(& $NmPath -l $outPath 2>&1)
        if ($LASTEXITCODE -ne 0) {
            throw "nm6x failed: $($profile.name)"
        }
        $symbols = $nmOutput -join "`n"
        $symbolTable = @(Get-NmSymbols $nmOutput)
        if ($symbolTable.Count -eq 0) {
            throw "nm6x returned no parsed symbols: $($profile.name)"
        }
        $warningCount = @($buildOutput |
            Select-String -Pattern '(?i)warning\s+#\d+|:\s*warning(?:\s|:)').Count
        $errorCount = @($buildOutput |
            Select-String -Pattern `
                '(?i)error\s+#\d+|:\s*error(?:\s|:)|undefined symbol').Count
        $mapCopy = Join-Path $OutputDirectory "$($profile.name).map"
        $linkCopy = Join-Path $OutputDirectory `
            "$($profile.name)_linkInfo.xml"
        Copy-Item -LiteralPath $mapPath -Destination $mapCopy
        Copy-Item -LiteralPath $linkPath -Destination $linkCopy

        $mapText = Get-Content -Raw -LiteralPath $mapPath
        $subbandInfo = Get-MapSectionInfo $mapPath ".subband_l2"
        $uiObjectPattern = '^(?:s_(?:ui|lcd|editor)_\w+|' +
            's_layout_drawn|s_runtime_started|EQ_Ui\w+|EQ_Touch\w+|' +
            'EQ_UI_(?:PRESET|CHAIN|ANALYZER|DYNAMIC|PAGE|EDITOR)_\w+|' +
            'EQ_Debug(?:Lcd|Touch|Ui)\w*)$'
        $uiSubbandSymbols = @($symbolTable | Where-Object {
            (($_.type -eq "OBJT") -or ($_.type -eq "COMN")) -and
            ($_.name -match $uiObjectPattern) -and
            (Test-AddressInSection $_.address $subbandInfo)
        })
        $uiSubbandObjectHits = [regex]::Matches(
            $mapText,
            '(?im)^\s*[0-9a-f]+\s+[0-9a-f]+\s+' +
            'user_equalizer_(?:display|ui_logic)\.obj ' +
            '\(\.subband_l2(?::[^)]*)?\)\s*$').Count

        $editorSymbolPattern = '^(?:EqualizerUiEditor\w*|' +
            'EqualizerUiLogic_(?:\w*Editor\w*|\w*Page\w*)|' +
            'EQ_Ui(?:Editor|Event)\w*|' +
            'EQ_(?:UpdateUiEditorFeedback|UiSnapshotEventChanged|' +
            'RecordUiSnapshotEvent|RequestUiSnapshotIfChanged|' +
            'SubmitUiEditorRequest)\w*|' +
            'EQ_UI_EDITOR_\w+|s_editor_\w+|EQ_Draw(?:Editor|Page)\w*|' +
            'EQ_DebugUi(?:RequestedPage|DisplayedPage|PageBuilding|' +
            'Snapshot\w*|AppliedGain\w*|DraftVersion|PageSwitch\w*|' +
            'EditorStateBytes|TotalStateBytes))$'
        $editorRuntimeSymbolHits = @($symbolTable | Where-Object {
            (($_.type -eq "FUNC") -or ($_.type -eq "OBJT") -or
             ($_.type -eq "COMN")) -and
            ($_.name -match $editorSymbolPattern)
        }).Count

        $uiStateBytes = Get-SymbolSize $symbolTable "s_ui_state"
        $editorStateBytes = Get-SymbolSize $symbolTable "EQ_UiEditorState"
        $touchStateBytes =
            (Get-SymbolSize $symbolTable "EQ_TouchState") +
            (Get-SymbolSize $symbolTable "EQ_TouchTransform")
        $uiTotalStateBytes = $uiStateBytes + $editorStateBytes
        $combinedUiTouchStateBytes = $uiTotalStateBytes + $touchStateBytes

        $jobArrayBytes = Get-SymbolSize `
            $symbolTable "EQ_DebugLcdJobTypeCount"
        $jobCounterBytes = Get-SymbolSize `
            $symbolTable "EQ_DebugLcdJobCount"
        if (($jobArrayBytes -ne 0) -and
            (($jobCounterBytes -eq 0) -or
             (($jobArrayBytes % $jobCounterBytes) -ne 0))) {
            throw "Cannot derive UI job count: $($profile.name)"
        }
        if ($jobCounterBytes -eq 0) {
            $jobCount = 0
        } else {
            $jobCount = [int]($jobArrayBytes / $jobCounterBytes)
        }

        $hitboxEntryBytes = 20L
        $dynamicHitboxBytes = Get-SymbolSize `
            $symbolTable "EQ_UI_DYNAMIC_HITBOXES"
        $editorHitboxBytes = Get-SymbolSize `
            $symbolTable "EQ_UI_EDITOR_HITBOXES"
        if ((($dynamicHitboxBytes % $hitboxEntryBytes) -ne 0) -or
            (($editorHitboxBytes % $hitboxEntryBytes) -ne 0)) {
            throw "Unexpected C6000 hitbox ABI size: $($profile.name)"
        }
        $dynamicHitboxCount = [int]($dynamicHitboxBytes / $hitboxEntryBytes)
        $editorHitboxCount = [int]($editorHitboxBytes / $hitboxEntryBytes)

        $framebufferSymbols = @($symbolTable | Where-Object {
            (($_.type -eq "OBJT") -or ($_.type -eq "COMN")) -and
            ($_.name -eq "Lcd_Buffer")
        })
        $largeFramebufferSymbols = @($symbolTable | Where-Object {
            (($_.type -eq "OBJT") -or ($_.type -eq "COMN")) -and
            ($_.size -ge 700000)
        })
        $secondFramebufferSymbolHits = @($largeFramebufferSymbols |
            Where-Object { $_.name -ne "Lcd_Buffer" }).Count
        $framebufferBytes = Get-SymbolSize $symbolTable "Lcd_Buffer"
        $offscreenMatches = [regex]::Matches(
            $mapText,
            '(?im)^\s*[0-9a-f]+\s+([0-9a-f]+)\s+' +
            '\S+\.obj\s+\(offscreen_buffer(?::[^)]*)?\)\s*$')
        $offscreenBufferBytes = 0L
        foreach ($offscreenMatch in $offscreenMatches) {
            $offscreenBufferBytes += [Convert]::ToInt64(
                $offscreenMatch.Groups[1].Value, 16)
        }

        if (($profile.editor_enabled -eq 0) -and
            ($editorRuntimeSymbolHits -ne 0)) {
            throw "Editor runtime symbols retained while Editor is OFF: " +
                "$($profile.name) ($editorRuntimeSymbolHits)"
        }
        if (($profile.editor_enabled -ne 0) -and
            (($editorRuntimeSymbolHits -eq 0) -or
             ($editorStateBytes -eq 0))) {
            throw "Editor runtime evidence missing: $($profile.name)"
        }
        if (($uiSubbandSymbols.Count -ne 0) -or
            ($uiSubbandObjectHits -ne 0)) {
            throw "UI symbols or objects placed in .subband_l2: " +
                "$($profile.name)"
        }
        if ($jobCount -ne $profile.expected_job_count) {
            throw "Unexpected UI job count: $($profile.name) " +
                "$jobCount != $($profile.expected_job_count)"
        }
        if ($dynamicHitboxCount -ne
            $profile.expected_dynamic_hitbox_count) {
            throw "Unexpected dynamic hitbox count: $($profile.name) " +
                "$dynamicHitboxCount != " +
                "$($profile.expected_dynamic_hitbox_count)"
        }
        if ($editorHitboxCount -ne $profile.expected_editor_hitbox_count) {
            throw "Unexpected editor hitbox count: $($profile.name) " +
                "$editorHitboxCount != $($profile.expected_editor_hitbox_count)"
        }
        if (($profile.lcd_enabled -ne 0) -ne ($uiStateBytes -ne 0)) {
            throw "LCD UI state allocation mismatch: $($profile.name)"
        }
        if (($profile.touch_enabled -ne 0) -ne ($touchStateBytes -ne 0)) {
            throw "Touch state allocation mismatch: $($profile.name)"
        }
        if ($framebufferSymbols.Count -ne
            $profile.expected_framebuffer_count) {
            throw "Unexpected linked framebuffer count: $($profile.name) " +
                "$($framebufferSymbols.Count) != " +
                "$($profile.expected_framebuffer_count)"
        }
        if ($secondFramebufferSymbolHits -ne 0) {
            throw "Second framebuffer candidate detected: $($profile.name)"
        }
        if (($offscreenMatches.Count -ne
             $profile.expected_framebuffer_count) -or
            ($offscreenBufferBytes -ne $framebufferBytes)) {
            throw "Framebuffer section/object mismatch: $($profile.name)"
        }

        $result = [ordered]@{
            profile = $profile.name
            lcd_enabled = $profile.lcd_enabled
            touch_enabled = $profile.touch_enabled
            editor_enabled = $profile.editor_enabled
            editor_touch_enabled = $profile.editor_touch_enabled
            runtime_mask = $profile.runtime_mask
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
            editor_runtime_symbol_hits = $editorRuntimeSymbolHits
            ui_state_bytes = $uiStateBytes
            editor_state_bytes = $editorStateBytes
            touch_state_bytes = $touchStateBytes
            ui_total_state_bytes = $uiTotalStateBytes
            combined_ui_touch_state_bytes = $combinedUiTouchStateBytes
            job_count = $jobCount
            dynamic_hitbox_count = $dynamicHitboxCount
            editor_hitbox_count = $editorHitboxCount
            total_hitbox_count = $dynamicHitboxCount + $editorHitboxCount
            ui_subband_symbol_hits = $uiSubbandSymbols.Count
            ui_subband_object_hits = $uiSubbandObjectHits
            framebuffer_symbol_count = $framebufferSymbols.Count
            framebuffer_bytes = $framebufferBytes
            offscreen_buffer_bytes = $offscreenBufferBytes
            offscreen_buffer_object_count = $offscreenMatches.Count
            second_framebuffer_symbol_hits = $secondFramebufferSymbolHits
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
    ui_state_bytes, editor_state_bytes, touch_state_bytes,
    job_count, dynamic_hitbox_count, editor_hitbox_count,
    ui_subband_symbol_hits, framebuffer_symbol_count -AutoSize
Write-Output $summaryPath
