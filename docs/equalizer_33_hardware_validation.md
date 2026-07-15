# Project 3.3 Stage B Hardware Validation

**Date:** 2026-07-15

**Repository:** `main`

**Current implementation commit before this report:**
`70a5bd1d7c07b3861700bb8ed458c74c904c259a`

**Current status:** Partial `MEASURED_ON_CURRENT_BOARD`; final initialization
`FAIL`; remaining LCD and endurance work `PENDING_HARDWARE`.

## 1. Evidence boundary

The connected chain was PC default line output -> ADC CH1 -> Project 3.3
ten-band RBJ equalizer -> DAC CH1 -> speaker, using TMS320C6748 and XDS100v3.
The board configuration retained 50 kHz sampling, 1024-sample frames,
ping-pong buffers, the existing presets and preamp rule, and the 120 ms
crossfade. Project 3.2, Touch, ADC/DAC/EDMA/PRU, and interrupt drivers were not
modified.

The labels in this report distinguish Host/build evidence, counter-based board
evidence, debugger-assisted control writes, and subjective listening. No
external analog instrument was used, so this report makes no DAC THD, analog
SNR, speaker frequency-response, SPL, or total analog-distortion claim.

Generated WAV, JSON, and DSS logs remain under the temporary directory
`%TEMP%\DSP_LAB_0507\equalizer_33_stage_b`; they are not committed.

## 2. Host and CCS evidence

| Item | Result |
| --- | --- |
| `equalizer_control_test` | `HOST_VERIFIED`, `failures=0` |
| `equalizer_response_test` | `HOST_VERIFIED`, `failures=0` |
| Original equalizer evaluator | `HOST_VERIFIED`, `failures=0` |
| Project 3.3 Python contracts | `HOST_VERIFIED`, 56 tests, `OK` |
| Project 32 CCS build | `CCS_BUILD_VERIFIED`, warning=0, link error=0 |
| Project 33 LCD OFF CCS build | `CCS_BUILD_VERIFIED`, warning=0, link error=0 |
| Project 33 LCD ON CCS build | `CCS_BUILD_VERIFIED`, warning=0, link error=0 |

The current Project 33 LCD ON output had build identity
`P33_FIX_V5`, SHA `70a5bd1`, dirty=0, and the link XML reported
`<link_errors>0x0</link_errors>`.

## 3. Failures and bounded fixes

The baseline `e60757d` reproducibly failed the BASS audio window. One measured
window had AD/DA/process deltas 973/971/972, deadline +6, latency deadline +6,
overlap +1, dropped +1, clip +0, and maximum algorithm time 13,342,039 cycles
(29.259 ms at 456 MHz). The active and pending ten-section banks were both
processed during crossfade from DDR2.

Commit `6cd5fd8` keeps both bank calculations and the 120 ms transition, fuses
the pair processing without changing each bank's arithmetic order, and places
the Project 3.3 board state in the existing `.subband_l2` section. A Host
sample-equivalence regression covers FLAT->BASS and BASS->VOCAL. The section is
conditional on Project 33, so Project 32 placement is unchanged.

LCD STATUS later recorded 2,316,982 cycles (5.081 ms), exceeding the 2,280,000
cycle hard limit. Auto-disable and budget-exceeded each increased by one,
reason=1. The fixed mode-value clear rectangle was narrowed from 180 to 168
pixels; no coordinates, gain bars, font, or audio scheduling were changed.
This final LCD change compiled and linked but did not receive a completed board
window because of the initialization failure in Section 7.

## 4. Stage A: LCD OFF audio baseline

Build `2a89458`, identity `P33_FIX_V5`, dirty=0, completed all twelve 20-second
windows: 1 kHz -18 dBFS and music-like input, each through RAW_COPY,
FLOAT_COPY, FLAT, BASS, VOCAL, and custom
`[-3,-1,1,3,4,2,0,-2,1,3] dB`.

**Result:** `MEASURED_ON_CURRENT_BOARD`

Across those windows AD/DA/process each increased by 11,699 frames. Per-window
counter differences were at most one. Aggregate deadline, latency-deadline,
overlap, dropped, and clip deltas were all zero. Maximum observed values were:

| Metric | Cycles | Time at 456 MHz |
| --- | ---: | ---: |
| Algorithm | 1,415,849 | 3.105 ms |
| Frame service | 1,852,538 | 4.062 ms |
| Frame latency | 1,852,838 | 4.063 ms |

These are measurements for the exact loaded build and board, not a guarantee
for every C6748 platform.

## 5. Stages B-E: control and builder

| Stage | Result | Evidence |
| --- | --- | --- |
| B fixed preset cache | `DEBUG_CONTROL_FUNCTIONAL` | FLAT->BASS->VOCAL->TREBLE->V_SHAPE->FLAT; builder slices +0, restarts +0, headroom scans +0; token 28 reached observed/accepted/target/prepared/installed/applied |
| C custom builder | `MEASURED_ON_CURRENT_BOARD` | Exactly 43 slices and one headroom scan; token 30 reached observed/accepted/target/prepared/installed/applied; active generation=target generation=16; safety and clip deltas zero |
| C exact finalize/install boundary | `PENDING_HARDWARE` | Host test proves ready after slice 43 and install on the next safe boundary; exact board observation would require an intrusive breakpoint |
| D latest-wins | `DEBUG_CONTROL_FUNCTIONAL` | Requests A/B/C ended at token 36 and generation 19; cancel +2, restart +2, safety and clip deltas zero |
| E RAW identity return | `DEBUG_CONTROL_FUNCTIONAL` | Transition window path=4 (DRY_TO_BANK), installed token 38 while applied remained 36; settled without safety or clip increments |
| E FLOAT identity return | `DEBUG_CONTROL_FUNCTIONAL` | Transition window path=4, installed token 40 while applied remained 38; a later build settled to an RBJ path without safety or clip increments |
| Exact transient identity-hold sample | `NOT_OBSERVED` | The transient is shorter than a debugger-safe fixed observation window |

Maximum measured builder time through these runs was 2,254,921 cycles
(4.945 ms at 456 MHz). Debugger writes in B, D, and E halt the target briefly,
so they are control-function evidence, not continuous real-time evidence.

## 6. Stage F: LCD and shared budget

The STATUS run on build `06d4736` completed its 60-second observation window
with AD/DA/process each +2,924 and zero deadline, latency-deadline, overlap,
dropped, and clip deltas. It also completed 43 builder slices. However, the
first STATUS job exceeded the LCD hard budget and auto-disabled LCD.

**STATUS result:** `FAIL`

**Final 168-pixel status rectangle:** `PENDING_HARDWARE`

**GAINS and BOTH:** `PENDING_HARDWARE` because the rule requires stopping after
STATUS fails and the final build did not initialize on the subsequent attempts.

## 7. Final-build initialization failure

The final runner commit `70a5bd1` compiled and linked with dirty=0. DSS read the
expected build identity after each load. On two bounded attempts, the initial
one-second run left AD/DA/process at 0/0/0. The runner labeled
`target_initialization` as `FAIL`, halted once, stopped PC audio, disconnected
and terminated the debug session, and did not enter Stage A.

The same GEL log line, `PSC0 Enable Verify Timeout on Domain 0, LPSC 6`, had
also appeared in earlier runs that did produce frames, so it is recorded but
is not by itself assigned as the root cause. After the repeated identical
failure, no further reset or reload was attempted.

Multiple bounded reset/load attempts occurred during the campaign while
isolating the crossfade and LCD failures. No DSS timeout or persistent Debug
Server hang occurred. Early harness expectation failures and all load attempts
remain visible in the temporary evidence directories.

## 8. Remaining status

| Item | Status |
| --- | --- |
| LCD STATUS with final 168-pixel clear width | `PENDING_HARDWARE` |
| LCD GAINS and BOTH 60-second windows | `PENDING_HARDWARE` |
| Builder/LCD alternation on the final build | `PENDING_HARDWARE` |
| 5-second and 2-second switching continuity | `PENDING_HARDWARE` |
| Five-minute LCD BOTH stability run | `PENDING_HARDWARE` |
| Optional RAW/custom/trigger digital capture | `NOT_OBSERVED` |
| External analog THD/SNR/frequency/SPL | `PENDING_HARDWARE` |
| Speaker output was audible during completed earlier windows | `SUBJECTIVE_SPEAKER_OBSERVATION` |

The subjective observation records only that sound was heard. It is not a
no-distortion, no-pop, frequency-response, or analog-quality measurement.
