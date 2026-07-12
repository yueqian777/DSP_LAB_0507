# Project 3.2 Mode 6 MCRA Musical-Noise Adjustment Design

## Goal

Apply the first-round, narrowly scoped Mode 6 parameter adjustment from
`7.12 musical noise调整.txt`. The change is an A/B candidate intended to reduce
musical-noise risk; it must not be described as a confirmed fix until the DSP
board has been auditioned.

## Scope

Only the following production files change:

- `Code/User/user_dsp/user_subband_denoise.h`
- `Code/User/user_dsp/user_subband_denoise.c`
- `Code/User/user_dsp/user_subband_flow.c`

Tests and a parameter/A-B guide may be added under `tools/tests/` and `docs/`.
WOLA, FFT, hop size, windows, two-second learning, codec processing, the main
loop, noise-PSD updates, Wiener gain, and decision-directed prior SNR remain
unchanged. Mode 7 and Mode 8 configuration remain unchanged.

## Production Changes

### Stateful tonal guard parameters

Add these fields to `SubbandDenoiseState`:

```c
float mcra_tonal_snr_min;
float mcra_tonal_neighbor_ratio;
float mcra_tonal_floor;
```

Initialization and Reset assign the legacy defaults `3.0f`, `1.35f`, and
`0.45f`. Add this public setter:

```c
void SubbandDenoise_SetMcraTonalGuardParams(
    float snr_min,
    float neighbor_ratio,
    float tonal_floor);
```

Replace only the existing hard-coded tonal-guard values with these state
fields. The existing `mcra_strong_mode == 0` condition continues to prevent the
tonal floor from changing strong MCRA behavior. No separate tonal-guard
algorithm is added.

### Mode 6 smoothing parameters

Immediately after `SubbandDenoise_Reset()` in the Mode 6 branch, call:

```c
SubbandDenoise_SetParams(0.96f, 0.15f, 0.85f, 0.80f);
SubbandDenoise_SetMcraTonalGuardParams(5.0f, 1.80f, 0.28f);
```

Keep the Mode 6 MCRA parameters at `1.5f, 4.0f, 0.85f, 0.998f, 1.10f,
1.70f, 1.40f, 0`. Do not add either setter call to Mode 8. Mode 7 and Mode 9
also retain their current configuration. Because each mode calls Reset first,
Mode 8 restores `3.0f`, `1.35f`, `0.45f`, and `gain_smooth_down=0.60f` after a
Mode 6 run.

### Debug counters

Expose two `volatile unsigned long` watch variables without feeding them into
audio calculations:

- `SUBBAND_DENOISE_DebugMcraTonalGuardHits`: cumulative bin-trigger count.
- `SUBBAND_DENOISE_DebugMcraTonalGuardBinsLastFrame`: bin-trigger count for the
  current/most recently processed spectrum frame.

At the start of every non-null `SubbandDenoise_ProcessSpectrum()` call, clear
the last-frame count. Each bin for which the tonal guard becomes active
increments both values once. `SubbandDenoise_Reset()` clears both counters;
normal frame processing never clears the cumulative count.

## Compatibility

Existing function signatures and mode numbers remain unchanged; one setter is
added. Default denoise behavior remains unchanged because Init and Reset load
the legacy tonal-guard values. Modes other than Mode 6 retain their current
parameter behavior.

## Verification

Host verification will cover:

- exact state defaults and replacement of the three hard-coded tonal values;
- Mode 6 call order and both setter parameter tuples;
- absence of both new Mode 6 setter calls in Mode 8;
- Mode 6 to Mode 8 and Mode 8 to Mode 6 transitions, proving Reset prevents
  tonal-guard and gain-smoothing parameter leakage;
- unchanged Mode 6 MCRA parameter tuple;
- runtime counter increment, last-frame reset, and reset behavior where the
  existing host harness can exercise the denoiser deterministically;
- the existing host test suite.

Firmware verification will run CCS Debug and O3 builds from MSYS2 and inspect
the generated link information for zero link errors. These checks establish
build and contract correctness, not audible quality.

## Board A/B Procedure

Build A from the parent commit and build B from this change, both with Project
3.2 and Mode 6 selected. Use the same input, level, microphone/speaker path,
two-second learning conditions, listening position, and playback segments.
Compare residual tonal chirps/warbling, speech consonant clarity, low-level
speech continuity, and background-noise naturalness. Also watch the cumulative
and last-frame tonal-guard counters to confirm the guard is active during
relevant material.

Potential costs in Mode 6 are weaker protection of moderately tonal bins
because the SNR and neighbor-ratio thresholds rise, and increased residual
noise because the tonal floor changes from `0.45f` to `0.28f`. The slower
gain-down smoothing (`0.60f` to `0.80f`) may reduce rapid gain fluctuations but
can make suppression respond more slowly. Record these tradeoffs during A/B
listening; do not synchronize the settings to Mode 8 until Mode 6 is accepted
on board.
