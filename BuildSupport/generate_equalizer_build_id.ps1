param(
    [string]$OutputPath = "Code/User/user_dsp/equalizer_build_id_generated.h"
)

$ErrorActionPreference = "Stop"
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$output = Join-Path $repoRoot $OutputPath
$sha = "unavailable"
$dirty = -1

try {
    $shaResult = git -C $repoRoot rev-parse --short HEAD 2>$null
    if ($LASTEXITCODE -eq 0) {
        $sha = ($shaResult | Select-Object -First 1).Trim()
        $status = git -C $repoRoot status --porcelain --untracked-files=normal
        if ($LASTEXITCODE -eq 0) {
            $dirty = if ([string]::IsNullOrWhiteSpace(($status -join "`n"))) {
                0
            } else {
                1
            }
        }
    }
} catch {
    $sha = "unavailable"
    $dirty = -1
}

$content = @"
#ifndef _EQUALIZER_BUILD_ID_GENERATED_H_
#define _EQUALIZER_BUILD_ID_GENERATED_H_

#define EQ_BUILD_VERSION "P33_FIX_V5"
#define EQ_BUILD_GIT_SHA "$sha"
#define EQ_BUILD_DIRTY $dirty

#endif /* _EQUALIZER_BUILD_ID_GENERATED_H_ */
"@
$content = $content.TrimEnd() + "`r`n"

$parent = Split-Path -Parent $output
New-Item -ItemType Directory -Force -Path $parent | Out-Null
if ((-not (Test-Path $output)) -or
    ((Get-Content $output -Raw) -ne $content)) {
    $temporary = "$output.tmp"
    [System.IO.File]::WriteAllText(
        $temporary, $content, [System.Text.UTF8Encoding]::new($false))
    Move-Item -Force -LiteralPath $temporary -Destination $output
}
