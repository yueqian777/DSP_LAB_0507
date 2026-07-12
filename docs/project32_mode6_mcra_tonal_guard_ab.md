# Project 3.2 Mode 6 MCRA Tonal Guard A/B Guide

## Change Summary

This change is a first-round Mode 6 tuning candidate for reducing musical-noise
risk. It does not change WOLA, FFT, hop size, windows, two-second learning,
codec processing, the main loop, noise-PSD updates, the Wiener gain formula, or
decision-directed prior SNR.

| Parameter | Reset / Mode 8 | Mode 6 |
| --- | ---: | ---: |
| Denoise alpha | 0.96 | 0.96 |
| Gain floor | 0.15 | 0.15 |
| Gain smoothing up | 0.85 | 0.85 |
| Gain smoothing down | 0.60 | 0.80 |
| Tonal SNR minimum | 3.0 | 5.0 |
| Tonal neighbor ratio | 1.35 | 1.80 |
| Tonal floor | 0.45 | 0.28 |

Mode 6 keeps the existing MCRA tuple `1.5, 4.0, 0.85, 0.998, 1.10, 1.70,
1.40, 0`. Mode 7 is unchanged. Mode 8 does not call either Mode 6 tuning
setter; its Reset restores the `Reset / Mode 8` defaults before applying its
existing MCRA tuple.

## Diagnostics

Watch these variables in CCS:

- `SUBBAND_DENOISE_DebugMcraTonalGuardHits`: cumulative guarded-bin count since
  the most recent denoiser Reset.
- `SUBBAND_DENOISE_DebugMcraTonalGuardBinsLastFrame`: guarded-bin count in the
  most recently processed spectrum frame.

Both counters are observation-only and do not feed the audio calculation.

## Potential Costs

The higher SNR and neighbor-ratio thresholds make the guard more selective, so
moderately tonal residuals may receive less protection. Reducing the guarded
floor from 0.45 to 0.28 can improve suppression but can also expose tonal
artifacts if the guard decision is unstable. Raising gain-down smoothing from
0.60 to 0.80 reduces rapid attenuation changes but may slow suppression after a
noise change. These tradeoffs require listening evaluation.

## Board A/B Procedure

1. Build A from the implementation commit's parent and build B from the
   implementation commit. Select Project 3.2 and Mode 6 for both.
2. Keep input audio, playback level, microphone and speaker path, physical
   placement, and the two-second learning noise identical.
3. Use repeated segments containing stationary noise, speech pauses, vowels,
   consonants, and low-level speech. Alternate A and B without changing gain.
4. Compare residual whistles/chirps, warbling during speech pauses, consonant
   clarity, low-level speech continuity, suppression response, and background
   noise naturalness.
5. Record both tonal-guard counters during the same segments. A zero cumulative
   count means the selected material did not exercise the guard and is not a
   useful guard A/B case.
6. Switch Mode 6 to Mode 8 and back to Mode 6. Confirm Mode 8 starts from its
   legacy behavior and Mode 6 reapplies its overrides.

Host tests and CCS builds prove parameter routing, reset isolation, diagnostic
behavior, compilation, and linkage. They do not prove an audible improvement.
Do not claim musical noise is solved or copy these values to Mode 8 until the
board A/B result is accepted.
