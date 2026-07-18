# Project 3.3 Build Profiles

These names define reproducible Project 3.3 build configurations. They are
documentation profiles, not new source defaults. Selecting a profile means
passing all listed macros explicitly to a clean CCS build.

## Project33_Baseline

```text
DSP_LAB_PROJECT_SELECT=33
EQ_ENABLE_AUDIO_FEATURE_ANALYZER=0
EQ_ENABLE_SMART_BASS=0
EQ_ENABLE_DYNAMIC_CLARITY=0
EQ_ENABLE_HARSHNESS_GUARD=0
EQ_ENABLE_LCD_DISPLAY=0
```

Use this profile to verify the base Project 3.3 equalizer without Analyzer,
Smart Bass, Dynamic Clarity, Harshness Guard, or LCD state. All four
feature-compiled Watch values must be zero.

## Project33_Analyzer

```text
DSP_LAB_PROJECT_SELECT=33
EQ_ENABLE_AUDIO_FEATURE_ANALYZER=1
EQ_ENABLE_SMART_BASS=0
EQ_ENABLE_DYNAMIC_CLARITY=0
EQ_ENABLE_HARSHNESS_GUARD=0
EQ_ENABLE_LCD_DISPLAY=0
```

Use this profile for Analyzer-only cadence, feature, and UART-audit evidence.
`EQ_DebugAnalyzerCompiled` must be one and
`EQ_DebugSmartBassCompiled`, `EQ_DebugDynamicClarityCompiled`, and
`EQ_DebugHarshnessGuardCompiled` must be zero.

## Project33_SmartBass

```text
DSP_LAB_PROJECT_SELECT=33
EQ_ENABLE_AUDIO_FEATURE_ANALYZER=1
EQ_ENABLE_SMART_BASS=1
EQ_ENABLE_DYNAMIC_CLARITY=0
EQ_ENABLE_HARSHNESS_GUARD=0
EQ_ENABLE_LCD_DISPLAY=0
```

Use this profile for Smart Bass board validation. Analyzer and Smart Bass
compiled-state Watch values must be one, while Dynamic Clarity must be zero.
Smart Bass runtime enable still defaults to zero and must be enabled explicitly
through `EQ_DebugSmartBassEnabled`.

Smart Bass requires the Analyzer; a build with Smart Bass one and Analyzer
zero is intentionally rejected. LCD-enabled compile/link checks are separate
from these LCD-OFF runtime profiles. None of these profiles changes the macro
defaults in `Code/main.c` or `Code/User/user_include.h`.

The exact `Project33_SmartBass` artifact measured on 2026-07-17 was built from
`eb2eb1f0e6274f253ac8d5e94704369f30955e6c`. It had zero compiler diagnostics,
`link_errors=0x0`, and SHA-256
`0eccd11421d24176d58e3ebbf841db1ab284f8436ff5936d503980986644f58b`.

## Project33_DynamicClarity

```text
DSP_LAB_PROJECT_SELECT=33
EQ_ENABLE_AUDIO_FEATURE_ANALYZER=1
EQ_ENABLE_SMART_BASS=0
EQ_ENABLE_DYNAMIC_CLARITY=1
EQ_ENABLE_HARSHNESS_GUARD=0
EQ_ENABLE_LCD_DISPLAY=0
```

This profile isolates the content-aware low-mid de-masking stage. Runtime
enable remains zero until `EQ_DebugDynamicClarityEnabled` is set explicitly.

## Project33_HarshnessGuard

```text
DSP_LAB_PROJECT_SELECT=33
EQ_ENABLE_AUDIO_FEATURE_ANALYZER=1
EQ_ENABLE_SMART_BASS=0
EQ_ENABLE_DYNAMIC_CLARITY=0
EQ_ENABLE_HARSHNESS_GUARD=1
EQ_ENABLE_LCD_DISPLAY=0
```

This profile isolates the bounded high-frequency excess attenuation stage.
Runtime enable remains zero until `EQ_DebugHarshnessGuardEnabled` is set.
Harshness Guard requires the Analyzer but does not require either of the other
dynamic stages.

## Project33_SmartBass_DynamicClarity

```text
DSP_LAB_PROJECT_SELECT=33
EQ_ENABLE_AUDIO_FEATURE_ANALYZER=1
EQ_ENABLE_SMART_BASS=1
EQ_ENABLE_DYNAMIC_CLARITY=1
EQ_ENABLE_HARSHNESS_GUARD=1
EQ_ENABLE_LCD_DISPLAY=0
```

This profile validates all three independent dynamic stages and is the
required LCD-OFF profile for combined board timing tests. The compile/link-only LCD
profile uses the same macros with `EQ_ENABLE_LCD_DISPLAY=1`.

Dynamic Clarity and Harshness Guard require the Analyzer but do not require
Smart Bass or each other. Their compile-time and runtime defaults remain zero
in all source headers.

## Harshness Guard A-H clean matrix

The clean feature-commit matrix at `dade304` used explicit macros for Project
32 and seven Project 3.3 profiles: Baseline, Analyzer only, Analyzer plus
Smart Bass, Analyzer plus Dynamic Clarity, Analyzer plus Harshness Guard, all
dynamics with LCD OFF, and all dynamics with LCD ON. All eight builds exited
zero with warning count zero and `link_errors=0x0`.

| Profile | `.subband_l2` used | Free | Feature state sizes |
|---|---:|---:|---|
| A Project 32 | 257124 | 5020 | none |
| B P33 baseline | 1072 | 261072 | none |
| C Analyzer | 19608 | 242536 | Analyzer 16488 |
| D Analyzer + Smart | 19860 | 242284 | Analyzer 16488; Smart 252 |
| E Analyzer + Clarity | 19868 | 242276 | Analyzer 16488; Clarity 260 |
| F Analyzer + Guard | 19828 | 242316 | Analyzer 16488; Guard 220 |
| G all, LCD OFF | 20340 | 241804 | 16488 + 252 + 260 + 220 |
| H all, LCD ON | 20340 | 241804 | 16488 + 252 + 260 + 220 |

Production profiles contained no benchmark or transition-capture symbols.

The final clean profile G rebuild at validation commit `6ef2c86` also exited
zero with zero warnings and `link_errors=0x0`. Benchmark, transition-capture,
and four-way diagnostic symbol searches all returned zero hits. The output
SHA-256 was
`45092DE91CC083CA22C0E1BE8A623DA78B0040D4FF1D449B87D8A64F4D102EDD`.

## Status UI A-E clean matrix

The final status-UI matrix used build `5d1525a`, generated identity
`P33_FIX_V5`, dirty=0. Every profile was cleaned and rebuilt with `-B`.

| Profile | Project/LCD/Touch/mask | warning | link | `.text` | `.const` | `.bss` | `.subband_l2` | LCD/Touch/UI symbols |
|---|---|---:|---:|---:|---:|---:|---:|---:|
| A | 32/0/0/n/a | 0 | 0x0 | 122176 | 14636 | 43 | 257124 | 0/0/0 |
| B | 33/0/0/0 | 0 | 0x0 | 123616 | 1928 | 10 | 20380 | 0/0/0 |
| C | 33/1/0/0 | 0 | 0x0 | 153632 | 7618 | 30 | 20380 | 28/0/19 |
| D | 33/1/0/15 | 0 | 0x0 | 153632 | 7618 | 30 | 20380 | 28/0/19 |
| E | 33/1/1/15 | 0 | 0x0 | 173120 | 14506 | 43 | 20380 | 28/11/19 |

All three dynamic stages and Analyzer were enabled in B-E. Timing, benchmark,
transition-capture, four-way, and UART diagnostics were disabled. The final E
output is 1107296 bytes with SHA-256
`1677223adb4ffc7e98e977eb689e49aef58ae144e78aa72744e8d9e890987c49`.
`EQ_DebugUiStateBytes=244` and `EQ_DebugTouchStateBytes=36` were retained in E.

## Ten-band editor A-H clean matrix

The final offline-editor matrix used exact feature source
`6c3daca0cfd645704446a60c5fe189ffeb0b8645`, generated build identity
`P33_FIX_V5`, and dirty=0. Every profile used clean plus `-B`; all had zero
warnings, zero errors, and `link_errors=0x0`.

| Profile | LCD/Touch/Editor/mask | `.text` | `.const` | `.bss` | `.subband_l2` | UI/Editor/Touch B | jobs | hitboxes D/E | FB |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|
| A Project 32 | 0/0/0/0 | 122176 | 14636 | 43 | 257124 | 0/0/0 | 0 | 0/0 | 1 |
| B P33 LCD OFF | 0/0/0/0 | 123616 | 1928 | 10 | 20380 | 0/0/0 | 0 | 0/0 | 0 |
| C P33 static | 1/0/0/0 | 157568 | 5272 | 30 | 20380 | 312/0/0 | 15 | 12/0 | 1 |
| D P33 dynamic | 1/0/0/15 | 158400 | 7634 | 30 | 20380 | 312/0/0 | 15 | 12/0 | 1 |
| E P33 Touch | 1/1/0/15 | 178176 | 14522 | 43 | 20380 | 312/0/36 | 15 | 12/0 | 1 |
| F Editor read-only | 1/0/1/49 | 173824 | 11714 | 30 | 20380 | 556/64/0 | 27 | 12/20 | 1 |
| G Editor Touch | 1/1/1/49 | 194336 | 18602 | 43 | 20380 | 556/64/36 | 27 | 12/20 | 1 |
| H full dual page | 1/1/1/63 | 194336 | 18602 | 43 | 20380 | 556/64/36 | 27 | 12/20 | 1 |

Matched Editor overhead is 15424 bytes `.text`, 4080 `.const`, zero `.bss`,
and zero `.subband_l2` for F-D; G-E is 16160, 4080, zero, and zero. Both are
inside the soft budgets. UI plus editor state is 620 bytes; with Touch it is
656 bytes. Editor-OFF profiles contain zero editor runtime symbols. No UI
symbol or object enters `.subband_l2`. B intentionally links no framebuffer;
A and C-H link the existing single 768036-byte guarded `Lcd_Buffer`, and no
profile contains a second framebuffer candidate.

These results are `TARGET_CLEAN_BUILD_PASS` and
`TARGET_LINK_MAP_AUDIT_PASS`. They do not show that H ran on a target.
