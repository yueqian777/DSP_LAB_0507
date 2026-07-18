# Project 3.3 Harshness Guard V1

## Status and scope

Harshness Guard V1 is a bounded, content-aware high-frequency excess
attenuation stage for Project 3.3. It is a relative-energy protection rule,
not a perceptual harshness detector, de-esser, voice detector, or source
classifier. The implementation does not justify perceptual listening claims.

| Item | Status |
|---|---|
| Baseline | `a4fc259a5c925f20ca94c331eac8e90338ce03a9` |
| Feature commit | `dade304fdeec3fb1acc4be407efac32d37a321e3` |
| Validation/tooling commit | `6ef2c86abe90c68b9148702e10c3b7ff4c745d28` |
| Host implementation and contracts | `HOST_VERIFIED` |
| Clean A-H CCS matrix at feature commit | `CCS_BUILD_VERIFIED` |
| Objective board evidence | Versioned results described below |
| Subjective listening | `NOT_PERFORMED` |
| External analog THD/SNR/SPL | `UNMEASURED` |

Project 3.2, the shared FFT, Analyzer mathematics, Smart Bass mathematics,
Dynamic Clarity mathematics, ADC/DAC/EDMA/PRU, UART2 driver, LCD, Touch, and
the default project selection were not changed by this feature.

## Control rule

The detector consumes a read-only Analyzer snapshot of raw ADC CH1 and only
makes a decision when `analysis_count` changes:

```text
harshness_db = brightness_relative_db - presence_relative_db
```

Attenuation is eligible only when all of the following are true:

- runtime Guard enable is nonzero;
- the Analyzer snapshot is valid and warm;
- all consumed values are finite;
- `rms_dbfs >= -45 dBFS`;
- `brightness_relative_db >= 0 dB`;
- `harshness_db > 6 dB`.

The target is linearly mapped from 6 to 18 dB and quantized to an adjacent
0.5 dB level. Attack advances at most one level per new analysis. Release,
including invalid, not-warm, low-RMS, or weak-Brightness input, requires two
new analyses per downward level. Runtime defaults are disabled and MEDIUM.

| Level | Peaking gain |
|---:|---:|
| 0 | 0.0 dB |
| 1 | -0.5 dB |
| 2 | -1.0 dB |
| 3 | -1.5 dB |
| 4 | -2.0 dB |

LOW, MEDIUM, and HIGH cap the target at levels 2, 3, and 4 respectively.
No state can request positive gain.

## Filter and integration

The five fixed RBJ peaking banks are designed once during initialization at
50 kHz, with center frequency 6 kHz and one-octave bandwidth. The localized
peaking cut was selected instead of a high shelf to limit broad high-frequency
darkening. Adjacent banks use an 80 ms output crossfade with independent DF2T
state. Stable level 0 is a PCM16 identity path.

The Project 3.3 audio order is:

```text
Static ten-band EQ -> Smart Bass -> Dynamic Clarity -> Harshness Guard
```

All three dynamic stages consume the same immutable Analyzer publication but
own independent state. Harshness Guard does not write ten-band gains, invoke
the custom builder, or submit control tokens. It requires the Analyzer at
compile time but does not require Smart Bass or Dynamic Clarity.

## Host evidence

The feature-commit export contains 128 deterministic control rows and five
129-point response curves. The nearest exported point to 6 kHz is
6101.055664 Hz:

| Level | 973.935 Hz | 6101.056 Hz | 16116.844 Hz |
|---:|---:|---:|---:|
| 0 | 0.000000 dB | 0.000000 dB | 0.000000 dB |
| 1 | -0.007576 dB | -0.498891 dB | -0.020351 dB |
| 2 | -0.015178 dB | -0.997781 dB | -0.040762 dB |
| 3 | -0.022825 dB | -1.496661 dB | -0.061291 dB |
| 4 | -0.030544 dB | -1.995530 dB | -0.081999 dB |

These are digital Host-model results. They do not measure the codec, analog
path, loudspeaker, or human perception.

At validation commit `6ef2c86`, all 116 Project 3.3 Python tests passed. The
equalizer control, response, evaluator, and Harshness Guard C regressions each
reported `failures=0`. The shared FFT regression also matched the frozen
Project 3.2 kernel in eight bit-exact cases and 12 WOLA hops.

## Board evidence boundary

The complete objective A-I runner passed on clean build `6f58613`, reporting
`P33 INIT 11`, runtime-OFF and Presence byte-exact identity captures, the
measured 2/7/10/14/18 dB mapping to levels 0/0/1/2/3, a -1.508627 dB Guard
center transfer for the Brightness capture, an observed four-way overlap
count of 4, zero safety counters, and 300.38016 seconds of DSP audio.

Current build `6ef2c86` separately passed the isolated 84-job benchmark and
the nine-case `BOARD_INTERNAL_PCM_OBJECTIVE_TRANSIENT` capture. Its attempted
full A-I rerun was stopped after the user shortened the pressure-test plan and
is archived as `ABORTED_BY_USER_TIME_LIMIT`; it is not reported as a current
A-I pass. The interrupted runner did not emit a final `board_result.json`.

The benchmark's safety/completeness checks passed, but Guard warm P99 was up
to 3.25403 times the comparison modules in the same isolated harness. This is
recorded as an open relative-performance review rather than hidden by the
overall artifact `pass` field. The measured Guard warm maximum was 1,008,406
cycles, with audio services stopped and all module saturation/nonfinite
counters zero.

Static inspection found similarly sized optimized C6000 functions
(`DynamicClarity_ProcessFrame` 3992 bytes and
`HarshnessGuard_ProcessFrame` 3892 bytes), with benchmark state in
`.subband_l2`; this did not explain the P99 gap. No performance acceptance
claim is made until instruction-cache or memory-layout behavior is isolated.

The same clean commit was finally rebuilt as production profile G with LCD,
benchmark, transition capture, and four-way diagnostics all disabled. It had
zero warnings, `link_errors=0x0`, no diagnostic symbol hits, and SHA-256
`45092DE91CC083CA22C0E1BE8A623DA78B0040D4FF1D449B87D8A64F4D102EDD`.
A six-second boot check reached INIT 11 with all real-time safety counters
zero, then left the DSP `RUNNING_DISCONNECTED`.

Compact summaries are in `docs/evidence/harshness_guard_dade304/`. Large WAV,
RAW, `.out`, map, linker XML, and full logs remain under `%TEMP%` and are
referenced by path, size, and SHA-256; they are not committed.
