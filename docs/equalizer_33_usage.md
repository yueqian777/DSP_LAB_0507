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

## Host Test

Use MSYS2 on this machine:

```sh
cd /c/Users/zhangyueqian/lab8/DSP_LAB_0507
/mingw64/bin/gcc -std=c99 -Wall -Wextra -DEQ_ALGO_ONLY -DEQUALIZER_TEST_MAIN \
  -ICode/User/user_dsp \
  Code/User/user_dsp/user_equalizer.c \
  Code/User/user_dsp/user_equalizer_eval.c \
  -lm -o /tmp/equalizer_test && /tmp/equalizer_test
```

Expected CSV files:

- `equalizer_band_response.csv`
- `equalizer_system_response.csv`
- `equalizer_thd_report.csv`
- `equalizer_group_delay_report.csv`

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
