# Project 3.3 Read-Only Audio Feature Analyzer

## Scope

`user_audio_feature_analyzer` analyzes one 1024-sample PCM16 input frame at
50 kHz. It produces four spectral-density features and input level metrics.
The module has no ADC, DAC, UART, LCD, Touch, equalizer, control mailbox, or
builder dependency. It cannot modify the audio path.

The default cadence is one analysis per eight input frames. The caller owns
the analyzer state and decides when background execution is safe. Project 3.3
starts with `EQ_DebugAnalyzerEnabled=1` when the Analyzer is compiled, the LCD
is enabled, and `EQ_UI_RUNTIME_DEFAULT_MASK` contains
`EQ_UI_RUNTIME_ANALYZER`. Builds without that visible Analyzer surface retain
the disabled default. CCS Watch can still override the runtime value or request
an Analyzer reset.

## Signal Processing

Each scheduled analysis performs the following deterministic sequence:

1. Compute and remove the frame mean.
2. Measure peak and RMS in normalized PCM units.
3. Apply a precomputed 1024-point Hann window.
4. Run the shared stateless radix-2 forward FFT using precomputed twiddles.
5. Form a one-sided power spectrum.
6. Average power within each feature band and across the 20 Hz to 16 kHz
   global region.
7. Smooth each density with attack or release alpha.
8. Report each feature relative to the smoothed global density in dB.

Power density, not total band power, is used so a wide frequency band does
not receive a systematic advantage for white-noise input.

## Frequency Bins

The bin frequency is always calculated as:

```text
frequency_hz = bin * 50000 / 1024
```

The resolution is 48.828125 Hz. DC, bins above 16 kHz, and the Nyquist bin
are excluded. The resulting inclusive bin ranges are:

| Feature | Frequency contract | FFT bins | Actual bin-center range |
| --- | --- | ---: | ---: |
| Bass | 20 <= f < 200 Hz | 1-4 | 48.828125-195.312500 Hz |
| Mud | 200 <= f < 800 Hz | 5-16 | 244.140625-781.250000 Hz |
| Presence | 800 <= f < 4000 Hz | 17-81 | 830.078125-3955.078125 Hz |
| Brightness | 4000 <= f <= 16000 Hz | 82-327 | 4003.906250-15966.796875 Hz |

## State And Validity

The default smoothing coefficients are `attack_alpha=0.60` and
`release_alpha=0.90`, using `alpha * previous + (1-alpha) * current`.
Energy rises therefore react faster than energy falls. These values can be
changed only through `AudioFeatureAnalyzer_SetSmoothing`.

An analysis below -70 dBFS RMS is invalid. Its outputs remain finite and its
smoothed densities release toward zero. Three valid analyses are required
before `warmup_complete` is set.

`AudioFeatureAnalyzer_Reset` clears runtime measurements and counters while
preserving the precomputed tables, cadence, and smoothing settings.

## Memory

The state contains its own Hann table, complex twiddles, and split-complex FFT
scratch arrays. It uses no dynamic allocation, VLA, recursion, or large local
array. Its nominal table and FFT storage is 16 KiB.

Projects 3.2 and 3.3 call the same `user_spectral_fft` butterfly and twiddle
implementation. The common module owns no mutable state. Project 3.2 retains
its private 512-point WOLA history and scratch arrays; this analyzer retains
separate 1024-point arrays. A frozen legacy-kernel host regression requires
bit-exact 512-point twiddles, FFT output, and 12 consecutive 50%-overlap WOLA
output hops before the extraction is accepted.

The integrated Project 3.3 LCD-OFF map uses `0x4C98` bytes in `DSPL2RAM` and
leaves `0x3B368` bytes free. Its analyzer state and one 1024-sample pending
PCM snapshot are placed in `.subband_l2`. Project 3.2 has a different L2
footprint; its WOLA state is not reused or resized by this module.

## Board Scheduling

`EQ_CaptureAdcFrame` copies the current CH1 input to `EQ_AD_Buffer1`, observes
the cadence, and copies a due frame to the analyzer snapshot. It never runs an
FFT. If a due snapshot is already pending, the newest snapshot replaces it and
`EQ_DebugAnalyzerPendingOverwriteCount` increments.

Heavy analysis is a shared-budget background payload. The fixed priority is:

1. audio AD/DA frame service;
2. EQ control/install;
3. one custom-builder slice;
4. one analyzer FFT;
5. one explicitly requested UART feature line;
6. one LCD job.

The analyzer branch rechecks `FLAG_AD`, `FLAG_DA`, `flag_ad_done`, and
`EQ_FrameServicePending` immediately before execution. Builder eligibility
must also be zero. Audio arrival during the FFT is recorded, and the loop
returns directly to the audio checks after the analyzer payload.

The optional UART line is emitted only when
`EQ_DebugUartFeatureRequest=1`, a valid analyzer snapshot exists, and audio and
builder work are idle. It uses one bounded, integer-only line shorter than 72
bytes:

```text
P33 FEAT,<seq>,<rms_x10>,<bass_x10>,<mud_x10>,<presence_x10>,<brightness_x10>
```

The request clears after one line. There is no UART RX command, periodic
telemetry, `sprintf`, floating-point formatting, or analyzer-side UART
dependency.

## API Results

- `AudioFeatureAnalyzer_ProcessFrame` returns `1` for a scheduled analysis,
  `0` for a cadence skip, and a negative value for invalid input.
- `AudioFeatureAnalyzer_ObserveFrame` performs only cadence accounting and
  returns `1` when the caller should capture the current input frame.
- `AudioFeatureAnalyzer_AnalyzeObservedFrame` performs one heavy analysis of
  an already captured due frame without incrementing the ADC-frame counter.
- The setter APIs return `1` on success and `0` for invalid arguments.
- `AudioFeatureAnalyzer_GetSnapshot` copies only measurements and counters;
  it does not expose mutable FFT scratch storage.
