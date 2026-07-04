# WOLA-DFT Subband Design

## Goal

Replace the current realtime CH1 subband algorithm with a near-lossless streaming WOLA-DFT analysis/synthesis path. The public 1024-sample entry point remains `Subband_Process_1024(short *in, short *out)` so the existing ADC/DAC ping-pong framework does not change.

## Scope

This phase only implements analysis, eight user band gain hooks, synthesis, bypass, and offline verification. It does not implement noise learning, denoising, spectral subtraction, Wiener filtering, LMS/NLMS/FxLMS, ANC, compression, bit allocation, quantization, packing, or realtime `printf`.

## Architecture

`user_subband_flow.c` keeps the board-facing flow: ADC capture, CH1 processing, CH2-CH8 passthrough, and DAC buffer filling. The new `user_subband_wola.c` owns all WOLA state and processing. The new `user_subband_test.c` owns PC algo-only tests and is compiled only when `SUBBAND_ALGO_ONLY` is defined.

The streaming WOLA processor uses `NFFT=512`, `HOP=256`, periodic sqrt-Hann windows, in-place radix-2 float FFT/IFFT, and overlap-add. Each 1024-sample frame is processed as four hops, preserving continuity across frame boundaries through static analysis and OLA buffers.

## Interfaces

- `void SubbandWOLA_Init(void);`
- `void SubbandWOLA_ProcessFrame(short *in, short *out);`
- `void SubbandWOLA_SetBandGain(int band, float gain);`
- `void SubbandWOLA_SetBypass(int enable);`
- `void Subband_Process_1024(short *in, short *out);`
- `void SubbandWOLA_OfflineTest_All(void);`

## Defaults

- `SUBBAND_FRAME_LEN = 1024`
- `SUBBAND_SAMPLE_RATE = 50000.0f`
- `SUBBAND_NFFT = 512`
- `SUBBAND_HOP = 256`
- `SUBBAND_NUM_BANDS = 8`
- `SUBBAND_BYPASS = 0`
- `band_gain[0..7] = 1.0f`

## Data Flow

For each hop, the newest 256 input samples are shifted into a 512-sample analysis buffer. The buffer is multiplied by sqrt-Hann and transformed by FFT. The positive-frequency bins are mapped to eight bands and scaled by `band_gain`; negative-frequency bins are restored by conjugate symmetry. IFFT output is multiplied by the same sqrt-Hann, overlap-added, and the oldest 256 samples are saturated to `short` output.

For `NFFT=512` and `HOP=256`, the expected streaming WOLA delay is `NFFT - HOP = 256` samples. The final delay is not assumed from theory alone: the offline impulse test must report `impulse_peak_index`, `impulse_peak_value`, and measured `aligned_lag`.

IFFT normalization and overlap-add normalization are separate. IFFT applies the single `1/NFFT` scale. Periodic sqrt-Hann with 50 percent overlap should satisfy `window[n]^2 + window[n+HOP]^2 = 1`, so no extra OLA gain is applied in the realtime path. The offline test must report `max_cola_error` and target `< 1e-5`.

## Testing

PC algo-only tests compile `user_subband_wola.c` and `user_subband_test.c` with `SUBBAND_ALGO_ONLY` and `SUBBAND_TEST_MAIN`. They cover bypass exactness, COLA error, impulse delay/gain, WOLA passthrough on sine/noise inputs, single-band retention/leakage, and band-gain perturbation. SNR tests ignore startup and tail transient by computing only on the delay-aligned steady-state region, skipping at least `2*NFFT` samples at both ends. Board build verification uses the existing CCS `Debug` build graph and checks `Debug/DSP_LAB_0507.out`, `Debug/DSP_LAB_0507_linkInfo.xml`, and `Debug/DSP_LAB_0507.map`.

Single-band tests do not use SNR as the primary metric. For each retained band they report `main_band_energy_ratio`, `max_adjacent_band_leakage`, and `max_outband_leakage`.

## Hardware Notes

All realtime arrays are static, all realtime core math uses `float`, and output is saturated to `short`. Realtime code does not use `printf`, `malloc/free`, or `double`. The code keeps the current C6748 ADC/DAC initialization, ping-pong flags, and CH2-CH8 passthrough behavior. If `NFFT=512/HOP=256` is too slow on board, the same macros allow a later `NFFT=256/HOP=128` build without changing the external interface.
