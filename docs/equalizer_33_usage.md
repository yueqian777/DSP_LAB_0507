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
headroom, coherent THD, expected-clip, long-stability, and transition CSVs.

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
clip counters.

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
& 'D:\SoftwareDownload\CCS_20.5.0.00028_win\ccs\utils\bin\gmake.exe' -B -C Debug all 'GEN_OPTS__FLAG=--define=DSP_LAB_PROJECT_SELECT=33'
```

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
- `EQ_DebugLastCycles`
- `EQ_DebugMaxCycles`
- `EQ_DebugLastMs`
- `EQ_DebugMaxMs`
- `EQ_DebugMode`
- `EQ_DebugClipCount`
- `EQ_DebugBandGainDb`

`EQ_DebugMode` selects presets:

- `0`: flat
- `1`: bass boost
- `2`: vocal
- `3`: treble boost
- `4`: V shape
