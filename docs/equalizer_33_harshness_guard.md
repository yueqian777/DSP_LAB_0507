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
| Performance closure | `26f9eea` |
| Analyzer-reset closure | `4fd96b6` |
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

At closure commit `4fd96b6`, the required Harshness Guard, Smart Bass,
Dynamic Clarity, Equalizer, hardware-tool contract, and shared FFT/WOLA suite
ran 113 tests with zero failures. The Project 3.2 regression again passed all
eight FFT cases and 12 WOLA hops.

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

The user accepted the complete A-I result on build `6f58613` and explicitly
waived another full rerun. This is
`FULL_A_TO_I_RERUN_NOT_REQUIRED_BY_USER_DECISION`; the interrupted current-SHA
attempt remains a historical time-limit stop, not a functional failure.

The relative-performance review is closed. All three files used the same
C6000 command (`-mv6740 -O3 -g --c89`) with no file-specific speed/size,
floating-point, or software-pipeline option. The original Guard function had
50 fixed-stack loads/stores versus 6 in each sibling, including one state-base
spill and 42 reload sites from stack slot 3. Matching `active_state` and
`pending_state` offsets to 140 and 148 removed every state-base reload without
changing arithmetic. The final Guard and Dynamic Clarity executable ranges
are both 3904 bytes (`nm6x` symbol size 3992), with identical `0x40` stack
frames and 133/94 total loads/stores. The root-cause class is
`REGISTER_SPILL_OR_ALIASING`, not benchmark unfairness or an extra runtime
helper.

The reduced 36-job board benchmark used two deterministic inputs, six paths,
64 warm-up calls, and 4096 measured calls per job. Guard P99 maximum fell from
1,007,913.55 to 336,201.3 cycles; Dynamic Clarity and Smart Bass measured
336,205.3 and 336,200.2 cycles. The largest per-case Guard P99 ratio was
1.004261. Twelve frozen PCM16 paths remained byte-exact, and all benchmark
saturation/nonfinite counters were zero.

Build `4fd96b6` was rebuilt as production profile G with LCD, benchmark,
transition capture, and four-way diagnostics all disabled. It had zero
warnings, `link_errors=0x0`, no diagnostic symbol hits, and SHA-256
`FD90B297BFE73E89FDA27835331F1CE3E051147B1AE08FAD8A4473C980F54033`.
A six-second boot check reached INIT 11 with deadline, latency miss, overlap,
dropped, clip, all three saturation, and all three nonfinite counters zero,
then left the DSP `RUNNING_DISCONNECTED`.

Manual Analyzer reset now invalidates the consumed epoch and calls
`HarshnessGuard_SetEnabled(..., 0)`. Existing filter history is retained while
the current level releases through adjacent 80 ms transitions to level 0.
A new valid/warm snapshot can re-enable the still-requested Watch setting and
is consumed exactly once. The Host trajectory passed `3 -> 2 -> 1 -> 0` with
zero saturation/nonfinite. The optional short board reset test was not run:
`TARGETED_RESET_BOARD_TEST_NOT_PERFORMED`.

Compact summaries are in `docs/evidence/harshness_guard_dade304/`. Large WAV,
RAW, `.out`, map, linker XML, and full logs remain under `%TEMP%` and are
referenced by path, size, and SHA-256; they are not committed.
