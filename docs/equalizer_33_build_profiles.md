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
EQ_ENABLE_LCD_DISPLAY=0
```

Use this profile to verify the base Project 3.3 equalizer without Analyzer,
Smart Bass, Dynamic Clarity, or LCD state. All three feature-compiled Watch
values must be zero.

## Project33_Analyzer

```text
DSP_LAB_PROJECT_SELECT=33
EQ_ENABLE_AUDIO_FEATURE_ANALYZER=1
EQ_ENABLE_SMART_BASS=0
EQ_ENABLE_DYNAMIC_CLARITY=0
EQ_ENABLE_LCD_DISPLAY=0
```

Use this profile for Analyzer-only cadence, feature, and UART-audit evidence.
`EQ_DebugAnalyzerCompiled` must be one and
`EQ_DebugSmartBassCompiled` and `EQ_DebugDynamicClarityCompiled` must be zero.

## Project33_SmartBass

```text
DSP_LAB_PROJECT_SELECT=33
EQ_ENABLE_AUDIO_FEATURE_ANALYZER=1
EQ_ENABLE_SMART_BASS=1
EQ_ENABLE_DYNAMIC_CLARITY=0
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
EQ_ENABLE_LCD_DISPLAY=0
```

This profile isolates the content-aware low-mid de-masking stage. Runtime
enable remains zero until `EQ_DebugDynamicClarityEnabled` is set explicitly.

## Project33_SmartBass_DynamicClarity

```text
DSP_LAB_PROJECT_SELECT=33
EQ_ENABLE_AUDIO_FEATURE_ANALYZER=1
EQ_ENABLE_SMART_BASS=1
EQ_ENABLE_DYNAMIC_CLARITY=1
EQ_ENABLE_LCD_DISPLAY=0
```

This profile validates both independent dynamic stages and is the required
LCD-OFF profile for combined board timing tests. The compile/link-only LCD
profile uses the same macros with `EQ_ENABLE_LCD_DISPLAY=1`.

Dynamic Clarity requires the Analyzer but does not require Smart Bass. Its
compile-time and runtime defaults remain zero in all source headers.
