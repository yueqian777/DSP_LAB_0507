# Project 3.3 Graphic Equalizer Notes

This project adds a separate 3.3 graphic equalizer path without replacing the
existing 3.2 subband/WOLA path.

## Main Selection

`Code/main.c` defaults to project 3.2:

```c
#ifndef DSP_LAB_PROJECT_SELECT
#define DSP_LAB_PROJECT_SELECT 32
#endif
```

Build with `DSP_LAB_PROJECT_SELECT=33` to run `Equalizer_Flow_Example()` on the
board. The default value keeps `Subband_Flow_Example()` as the active path.

## Offline Host Validation

Use MSYS2 on this machine:

```sh
cd /c/Users/zhangyueqian/lab8/DSP_LAB_0507
/mingw64/bin/gcc -std=c99 -Wall -Wextra -Werror -DEQ_ALGO_ONLY -DEQUALIZER_TEST_MAIN \
  -ICode/User/user_dsp \
  Code/User/user_dsp/user_equalizer.c \
  Code/User/user_dsp/user_equalizer_eval.c \
  -lm -o /tmp/equalizer_test && /tmp/equalizer_test
```

The strict evaluator writes transparency, coefficient, legacy/RBJ response,
headroom, coherent THD, expected-clip, long-stability, transition, bypass
restore, mode-state, and frame-service-contract CSVs.

Run the independent Python response check after the C test:

```sh
D:/SoftwareDownload/python.exe tools/equalizer_33_reference.py
```

Generate deterministic WAV A/B outputs, metrics, and PNG plots with:

```sh
D:/SoftwareDownload/python.exe tools/equalizer_33_process_wav.py
```

Outputs are written below `docs/eval_outputs/equalizer_33_offline/`.

## Core Selection

`EQ_CORE_RBJ_CASCADE` is the default 3.3 core.  The historical fixed-Q
parallel band-pass implementation remains available as `EQ_CORE_LEGACY` for
offline comparison only.  `EQ_CORE_RAW_COPY`, `EQ_CORE_FLOAT_COPY`, hard
bypass, and FLAT are transparent paths and do not update filter state or
clip counters. Bypass ON cancels an in-flight bank transition but preserves
the latest accepted preset/custom target. Bypass OFF restores that target with
a 120 ms dry-to-bank crossfade before the stable transparent FLAT path can be
used.

Preset/custom requests made during a transition do not replace the pending
bank. The current transition finishes first, then only the latest requested
target starts. The public state meanings are:

- requested: latest accepted preset/custom target
- transition target: bank currently being crossfaded to, or NONE
- applied: stable active preset, or NONE during bypass/dry restore/custom
- latest pending: a newer target waits behind the current transition

## Board Diagnostics

`EQ_DebugDiagPath` selects a 3.3-only CH1 diagnostic path:

- `0`: RAW_COPY
- `1`: FLOAT_COPY
- `2`: FLAT
- `3`: SINGLE_BAND (1 kHz, +3 dB)
- `4`: RBJ preset path

LCD drawing is disabled by default with `EQ_ENABLE_LCD_DISPLAY=0`.  The
Chinese bitmap glyph option remains enabled when LCD drawing is explicitly
enabled: `EQ_LCD_USE_CHINESE=1`.

## CCS Builds

The source default stays Project 3.2 (`DSP_LAB_PROJECT_SELECT=32`).  To build
Project 3.3 without editing `main.c`, inject this compiler symbol:

```text
DSP_LAB_PROJECT_SELECT=33
```

For the generated command-line build on this workstation:

```powershell
.\tools\generate_equalizer_build_id.ps1
& 'D:\SoftwareDownload\CCS_20.5.0.00028_win\ccs\utils\bin\gmake.exe' -B -C Debug all 'GEN_OPTS__FLAG=--define=DSP_LAB_PROJECT_SELECT=33 --define=EQ_USE_GENERATED_BUILD_ID=1'
```

The generator records the current short Git SHA and dirty state in an ignored
header. Direct builds use the stable `P33_FIX_V5` fallback and report Git as
unavailable instead of relying on a hand-maintained SHA.

## Legacy Files

The older summary CSV names are still produced for compatibility, but their
acceptance behavior is now strict: any normal test with clipping, non-finite
state, unstable pole, bypass mismatch, or FLAT mismatch fails.

## Board Path

`Equalizer_Flow_Example()` uses `ADC_50KHZ`, `DAC_50KHZ`, and 1024-sample
frames. CH1 is processed by the equalizer and copied to DA_CH1. CH2 through CH8
are passed through unchanged in the first version.

Watch variables:

- `EQ_DebugAdFrames`
- `EQ_DebugDaFrames`
- `EQ_DebugProcessFrames`
- `EQ_DebugAlgoLastCycles`, `EQ_DebugAlgoMaxCycles`
- `EQ_DebugAlgoLastMs`, `EQ_DebugAlgoMaxMs`
- `EQ_DebugModeServiceLastCycles`, `EQ_DebugModeServiceMaxCycles`
- `EQ_DebugFrameServiceLastCycles`, `EQ_DebugFrameServiceMaxCycles`
- `EQ_DebugFrameServiceLastMs`, `EQ_DebugFrameServiceMaxMs`
- `EQ_DebugFrameLatencyLastCycles`, `EQ_DebugFrameLatencyMaxCycles`
- `EQ_DebugFrameLatencyLastMs`, `EQ_DebugFrameLatencyMaxMs`
- `EQ_DebugDeadlineMissCount`
- `EQ_DebugFrameServiceOverlapCount`, `EQ_DebugFrameServiceDroppedCount`
- `EQ_DebugMode`
- `EQ_DebugRequestedMode`, `EQ_DebugTransitionTargetMode`
- `EQ_DebugAppliedMode`, `EQ_DebugModeChangeCount`
- `EQ_DebugClipCount`
- `EQ_DebugBandGainDb`

Algorithm time measures only CH1 equalizer processing. Mode-service time is
measured separately and runs only while AD/DA work is idle. Frame-service time
sums active ADC copy/algorithm/DAC copy segments and excludes waiting. Frame
latency runs from detection of `FLAG_AD` until the inactive DAC buffer is
written and remains an additional scheduling diagnostic. Deadline misses are
based on the active frame-service time against the 20.48 ms frame budget.

`EQ_DebugMode` selects presets:

- `0`: flat
- `1`: bass boost
- `2`: vocal
- `3`: treble boost
- `4`: V shape
