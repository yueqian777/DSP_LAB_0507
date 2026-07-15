# Project 3.3 Stage B Hardware Validation

**Date:** 2026-07-15

**Repository:** `main`

**Current remote HEAD/report commit:**
`59114278f09657a38a44879bfecc15325661b2f1`

**Stage runner implementation commit:**
`0440347dbbacb39ee947a4500f6b99664edcacc4`

**Runner base commit:**
`d8c48e504d5865518d5fea9410024bb466ccadcf`

**EDMA callback-table fix commit:**
`ca7bf26dd595fee1d9045c15921afa53e94ca478`

**Current status:** Partial `MEASURED_ON_CURRENT_BOARD`; final initialization
`FAIL`; remaining LCD and endurance work `PENDING_HARDWARE`.

## 1. Evidence boundary

The connected chain was PC default line output -> ADC CH1 -> Project 3.3
ten-band RBJ equalizer -> DAC CH1 -> speaker, using TMS320C6748 and XDS100v3.
The board configuration retained 50 kHz sampling, 1024-sample frames,
ping-pong buffers, the existing presets and preamp rule, and the 120 ms
crossfade. Project 3.2, Touch, DAC timing, ADC timing and PaRAM setup, PRU, and
the channel/TCC/interrupt mapping were not modified. The current fix is limited
to the EDMA0/EDMA1 callback tables, completion-dispatch guards, and the ADC
callback-registration bounds check.

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
| Project 3.3 Python contracts | `HOST_VERIFIED`, 66 tests, `OK` |
| EDMA source/map contract | `HOST_VERIFIED`, 4 tests, `OK` |
| Project 32 CCS build | `CCS_BUILD_VERIFIED`, warning=0, link error=0 |
| Project 33 LCD OFF CCS build | `CCS_BUILD_VERIFIED`, warning=0, link error=0 |
| Project 33 LCD ON CCS build | `CCS_BUILD_VERIFIED`, warning=0, link error=0 |

The EDMA-safe hardware rerun used the clean Project 33 LCD OFF output with build
identity `P33_FIX_V5`, SHA `ca7bf26`, dirty=0. The Project 32, Project 33 LCD
OFF, and Project 33 LCD ON link XML each reported
`<link_errors>0x0</link_errors>`.

| Callback table | Before fix | `ca7bf26` map |
| --- | ---: | ---: |
| `cb_Fxn` | 4 bytes at `0xC0019598` | 128 bytes at `0xC002F7E0` |
| `edma1_cb_Fxn` | incomplete tentative array; omitted from the old linked map | 128 bytes at `0xC002F860` |

For C6748, `EDMA3_NUM_TCC=32`. Slot 29 now resolves to `0xC002F854`,
inside `cb_Fxn[0xC002F7E0,0xC002F860)`. Neither callback table overlaps an
`EQ_Debug*` symbol. EDMA1 is explicitly retained so its complete table remains
auditable even though this program does not use EDMA1 at runtime.

## 3. Failures and bounded fixes

The baseline `e60757d` reproducibly failed the BASS audio window. One measured
window had AD/DA/process deltas 973/971/972, deadline +6, latency deadline +6,
overlap +1, dropped +1, clip +0, and maximum algorithm time 13,342,039 cycles
(29.259 ms at 456 MHz). The active and pending ten-section banks were both
processed during crossfade from DDR2.

Commit `6cd5fd8` keeps both bank calculations and the 120 ms transition. The
measured improvement is attributed to **fused dual-bank execution together
with internal-L2 EQ state placement**, not to the fused loop alone. A Host
sample-equivalence regression covers FLAT->BASS and BASS->VOCAL. The placement
is conditional on Project 33, so Project 32 placement is unchanged.

LCD STATUS later recorded 2,316,982 cycles (5.081 ms), exceeding the 2,280,000
cycle hard limit. Auto-disable and budget-exceeded each increased by one,
reason=1. The fixed mode-value clear rectangle was narrowed from 180 to 168
pixels; no coordinates, gain bars, font, or audio scheduling were changed.
This final LCD change compiled and linked but did not receive a completed board
window because of the initialization failure in Section 7.

### 3.1 Intermediate-build reproducibility audit

The two successful measurement commits remain available only through this
checkout's local reflog:

| Build | Full commit | Tree | Reachable from `origin/main` |
| --- | --- | --- | --- |
| Stage A success | `2a89458867c3b98245c405fc008337caa5696771` | `0be3aae2c60f778c9ab392ef6743ba98b6add3d0` | No |
| LCD STATUS failure | `06d473621033c7990e56233751b537e1c75d6215` | `16beb0f653402b64432150e67d88a3c7ebb1784e` | No |

Both are `NON_REMOTE_INTERMEDIATE_BUILD` and
`NOT_REPRODUCIBLE_FROM_CURRENT_REMOTE_HISTORY`. Their measurements remain
evidence for those exact loaded commits only. They are not described as a
current-remote pass.

`2a89458..70a5bd1` changes only the Stage B DSS runner (16 lines). The full
`2a89458..0440347` diff contains 43 files and 65,312 insertions because later
Project 3.2 THD and memory-audit work is also present. Restricted to Project
3.3 equalizer/runner paths, the current difference is five files with 369
insertions and 31 deletions; RBJ mathematics, the fused loop, L2 EQ state,
presets, preamp, crossfade, builder, and mailbox are unchanged.

## 4. Stage A: LCD OFF audio baseline

Build `2a89458867c3b98245c405fc008337caa5696771`, identity `P33_FIX_V5`,
dirty=0, completed all twelve 20-second windows: 1 kHz -18 dBFS and music-like input, each through RAW_COPY,
FLOAT_COPY, FLAT, BASS, VOCAL, and custom
`[-3,-1,1,3,4,2,0,-2,1,3] dB`.

**Build provenance:** `NON_REMOTE_INTERMEDIATE_BUILD`

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

The STATUS run on build
`06d473621033c7990e56233751b537e1c75d6215` completed its 60-second observation window
with AD/DA/process each +2,924 and zero deadline, latency-deadline, overlap,
dropped, and clip deltas. It also completed 43 builder slices. However, the
first STATUS job exceeded the LCD hard budget and auto-disabled LCD.

**Build provenance:** `NON_REMOTE_INTERMEDIATE_BUILD`

**STATUS result:** `FAIL`

**Final 168-pixel status rectangle:** `PENDING_HARDWARE`

**GAINS and BOTH:** `PENDING_HARDWARE` because the rule requires stopping after
STATUS fails and the final build did not initialize on the subsequent attempts.

## 7. Final and current-build initialization failures

The final runner commit `70a5bd1d7c07b3861700bb8ed458c74c904c259a`
compiled and linked with dirty=0. DSS read the expected build identity after
each load. On two bounded attempts, the initial one-second run left
AD/DA/process at 0/0/0. The runner labeled `target_initialization` as `FAIL`,
halted once, stopped PC audio, disconnected, and did not enter Stage A.

The remote baseline had advanced from the requested `a2205a1` to `d8c48e5`
before the reproduction audit began. A staged runner and one-time milestone
were committed on top. The first bounded diagnostic load reached milestone 11
and AD/DA/process 53/53/53. CPU IER was `0x8663`; EDMA event-enable and
interrupt-enable each contained bit 29. The harness stopped before CrossfadeA
because the initialization snapshot exposed a corrupted latency-miss value.

The value was `0xC002CC00`, exactly the address of `callback_adc` in that map.
The linked `cb_Fxn` object has size four bytes, while ADC initialization stores
`cb_Fxn[29]`; the destination was the latency-miss diagnostic address in that
intermediate layout. This is an existing EDMA callback-table out-of-bounds
write. At that stage the ADC/EDMA driver had not yet been changed. The final
milestone variables are placed in Project 3.3
`.subband_l2` so this new instrumentation does not participate in DDR global
layout; the final map still records the pre-existing callback-table overrun
risk separately: `cb_Fxn` is at `0xC0019598`, and slot 29 resolves to
`0xC001960C`, the address of `EQ_DebugFrameServiceDroppedCount`.

The second and final bounded diagnostic load used build identity `527c867`,
dirty=0. It stopped at milestone 8: flow entered, ADC and DAC init returned,
Equalizer initialization and fixed-preset cache completed, ADC and DAC start
returned, and the main loop was entered. After two seconds:

| Diagnostic | Value |
| --- | ---: |
| `FLAG_AD` | 0 |
| `FLAG_DA` | 1 |
| `flag_ad_done` mirror | 0 |
| AD/DA/process frames | 0 / 0 / 0 |
| CPU IER | `0x8663` |
| EDMA event-enable | `0x20000000` |
| EDMA interrupt-enable | `0x20000000` |

The concrete stop point is after both start calls and main-loop entry but
before the first ADC EDMA completion/`FLAG_AD`. DAC completion was observed via
`FLAG_DA=1`. No FLAT/BASS window ran, so all three current-build AlgoMax,
FrameServiceMax, FrameLatencyMax, safety deltas, and frame deltas are
`NOT_OBSERVED`. No further reset, reload, power cycle, LCD STATUS, or Full stage
was attempted for that build.

### 7.1 EDMA-safe bounded rerun

Commit `ca7bf26dd595fee1d9045c15921afa53e94ca478` replaces both
incomplete callback arrays with 32-entry initialized tables, adds C89
compile-time size/range checks, bounds and null guards in both completion ISRs,
and a bounded ADC registration failure state. This is a **confirmed
memory-safety defect and prerequisite fix**. It is not established as the sole
root cause of initialization milestone 8.

Two initialization-only attempts used the clean LCD OFF output. Each attempt
performed exactly one reset/load, ran for two seconds, halted once, read the
diagnostics, and disconnected. Both produced the same result:

| Diagnostic | Attempt 1 | Attempt 2 |
| --- | ---: | ---: |
| `EQ_DebugInitStage` | 8 | 8 |
| `FLAG_AD` / `FLAG_DA` | 0 / 1 | 0 / 1 |
| AD / DA / process frames | 0 / 0 / 0 | 0 / 0 / 0 |
| CPU IER | `0x8663` | `0x8663` |
| EDMA event-enable | `0x20000000` | `0x20000000` |
| EDMA interrupt-enable | `0x20000000` | `0x20000000` |
| deadline / latency miss / overlap / dropped | 0 / 0 / 0 / 0 | 0 / 0 / 0 / 0 |

No diagnostic counter contained a `callback_adc` address after the fix. The
current stop point is still before the first ADC EDMA completion and
`FLAG_AD`; the channel and TCC 29 enable bits are set. Locating why the first
ADC event/completion does not arrive remains separate driver/hardware work.
Because initialization did not pass twice, CrossfadeA, LCD, Full, and endurance
stages were not run.

The same GEL log line, `PSC0 Enable Verify Timeout on Domain 0, LPSC 6`, had
also appeared in earlier runs that did produce frames, so it is recorded but
is not by itself assigned as the root cause.

Multiple bounded reset/load attempts occurred during the campaign while
isolating the crossfade and LCD failures. No DSS timeout or persistent Debug
Server hang occurred. Early harness expectation failures and all load attempts
remain visible in the temporary evidence directories.

## 8. Remaining status

| Item | Status |
| --- | --- |
| EDMA-safe `ca7bf26` initialization reproduction | `FAIL` twice at initialization milestone 8 |
| Three current-build FLAT->BASS->FLAT cycles | `NOT_EXECUTED`; initialization prerequisite failed |
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
