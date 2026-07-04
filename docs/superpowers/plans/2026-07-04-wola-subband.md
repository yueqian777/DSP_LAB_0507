# WOLA-DFT Subband Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a near-lossless streaming WOLA-DFT subband analysis/synthesis chain for CH1 realtime output while preserving the existing ADC/DAC ping-pong framework.

**Architecture:** Keep `user_subband_flow.c` as the board-facing wrapper and add `user_subband_wola.c/.h` for algorithm state and processing. Add `user_subband_test.c/.h` for PC algo-only metrics and smoke tests. `Subband_Process_1024` remains the external entry point and forwards to WOLA.

**Tech Stack:** C89-compatible C for TI C674x, static float buffers, radix-2 FFT, CCS gmake build, MSYS/GCC-style host test build.

---

## File Structure

- Create `Code/User/user_dsp/user_subband_wola.h`: public WOLA constants, debug variables, and realtime API.
- Create `Code/User/user_dsp/user_subband_wola.c`: state, window/twiddle init, FFT/IFFT, band gain, streaming overlap-add, saturation.
- Create `Code/User/user_dsp/user_subband_test.h`: offline test API.
- Create `Code/User/user_dsp/user_subband_test.c`: bypass, passthrough, band retention, gain perturbation tests.
- Modify `Code/User/user_dsp/user_subband_flow.h`: include the WOLA API and keep `Subband_Process_1024`.
- Modify `Code/User/user_dsp/user_subband_flow.c`: replace the old FIR processing body with a wrapper to `SubbandWOLA_ProcessFrame`; preserve `Subband_Flow_Example`.
- Modify `Code/User/user_include.h`: expose WOLA/test headers.
- Modify CCS generated build files under `Debug` if needed so CLI `gmake` builds the new files immediately.

## Tasks

### Task 1: Add host-test skeleton

- [ ] Create the WOLA/test headers and empty test main.
- [ ] Compile PC algo-only test and confirm it fails at link because WOLA functions are not implemented.

### Task 2: Implement WOLA bypass and state init

- [ ] Implement constants, static state, debug variables, `SubbandWOLA_Init`, `SetBypass`, `SetBandGain`, and bypass frame copy.
- [ ] Run host bypass test and confirm sine/noise exactness.

### Task 3: Implement FFT/IFFT and passthrough WOLA

- [ ] Implement radix-2 in-place FFT with precomputed twiddles.
- [ ] Implement periodic sqrt-Hann and 50 percent overlap-add with no extra OLA gain beyond the IFFT `1/NFFT`.
- [ ] Add COLA check reporting `max_cola_error`, target `< 1e-5`.
- [ ] Add impulse test reporting `impulse_peak_index`, `impulse_peak_value`, and measured `aligned_lag`; expected delay is `NFFT-HOP`.
- [ ] Run passthrough tests and tune only normalization/stream alignment if required.

### Task 4: Implement band mapping tests

- [ ] Implement positive-bin band mapping and conjugate symmetry.
- [ ] Add single-band metrics: `main_band_energy_ratio`, `max_adjacent_band_leakage`, and `max_outband_leakage`.
- [ ] Add band-gain perturbation metrics.
- [ ] Verify all tests run without realtime `printf` in board mode.

### Task 5: Integrate board entry point

- [ ] Keep the ping-pong framework in `user_subband_flow.c`.
- [ ] Make `Subband_Process_1024` call `SubbandWOLA_ProcessFrame`.
- [ ] Keep CH2-CH8 passthrough and expose CCS debug variables.

### Task 6: Build and inspect C6748 output

- [ ] Update the CCS build graph if needed.
- [ ] Run `gmake -C Debug all`.
- [ ] Check `.out`, link errors, and memory map.

### Task 7: Final requirement audit

- [ ] Re-read the request and check each requirement against code and test output.
- [ ] Report modified files, defaults, bypass switching, gain setting, offline test command, fixed delay, and future denoising insertion point.
