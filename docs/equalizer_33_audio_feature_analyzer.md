# Project 3.3 Read-Only Audio Feature Analyzer

## Scope

`user_audio_feature_analyzer` analyzes one 1024-sample PCM16 input frame at
50 kHz. It produces four spectral-density features and input level metrics.
The module has no ADC, DAC, UART, LCD, Touch, equalizer, control mailbox, or
builder dependency. It cannot modify the audio path.

The default cadence is one analysis per eight input frames. The caller owns
the analyzer state and decides when background execution is safe.

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
bit-exact 512-point twiddles and FFT output before the extraction is accepted.

The current Project 3.3 LCD-OFF map has 0x3FBD0 bytes free in `DSPL2RAM`, so a
future Project 3.3 integration may place one analyzer state in
`.subband_l2`. Project 3.2 has a different L2 footprint; its WOLA state is not
reused or resized by this module.

## API Results

- `AudioFeatureAnalyzer_ProcessFrame` returns `1` for a scheduled analysis,
  `0` for a cadence skip, and a negative value for invalid input.
- The setter APIs return `1` on success and `0` for invalid arguments.
- `AudioFeatureAnalyzer_GetSnapshot` copies only measurements and counters;
  it does not expose mutable FFT scratch storage.
