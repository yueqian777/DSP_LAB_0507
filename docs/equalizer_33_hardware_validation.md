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

**Current status:** Partial `MEASURED_ON_CURRENT_BOARD`. The Project 3.3 LCD
static alignment gate passed; hardened Dynamic Status, Touch, combined LCD
endurance, and unrelated remaining hardware work stay `PENDING_HARDWARE`.

**Scope note (2026-07-18):** The status above belongs to the original Stage B
run. Later feature-scoped results are recorded independently in Sections
9-13; Section 13 is the Harshness Guard V1 objective archive.

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

## 9. Analyzer gate baseline for Smart Bass development

**Date:** 2026-07-17

**Exact build commit:**
`a490e80a69297968ac23cd3ce8b2a4087cb0e016`

**Result:** `MEASURED_ON_CURRENT_BOARD_A490E80`

The operator confirmed a full power cycle before the clean build/load/run.
The configuration was Project 33, Analyzer ON, Smart Bass OFF, and LCD OFF:

```text
DSP_LAB_PROJECT_SELECT=33
EQ_ENABLE_AUDIO_FEATURE_ANALYZER=1
EQ_ENABLE_SMART_BASS=0
EQ_ENABLE_LCD_DISPLAY=0
EQ_USE_GENERATED_BUILD_ID=1
```

The TI C6000 full build completed with zero diagnostics and
`<link_errors>0x0</link_errors>`. The loaded output contained `a490e80` and no
`unavailable` fallback. UART reported `P33 BUILD a490e80`, followed by INIT
01 through 08 and INIT 11.

During the 60-second default-disabled window, AD/DA/process were
2891/2891/2891. `EQ_DebugAnalyzerCompiled=1`, enabled=0, run count=0,
analysis count=0, valid=0, and warmup=0. Deadline, latency miss, overlap, and
dropped counters were all zero.

After enabling the Analyzer, four integer-bin PC line-output stimuli produced
the expected dominant relative feature:

| Stimulus | Frequency | Dominant feature value | Other feature values |
| --- | ---: | ---: | --- |
| Bass | 97.65625 Hz | Bass 19 dB | -47, -53, -58 dB |
| Mud | 390.625 Hz | Mud 14 dB | -37, -48, -49 dB |
| Presence | 1953.125 Hz | Presence 7 dB | -35, -42, -36 dB |
| Brightness | 8007.8125 Hz | Brightness 1 dB | -26, -28, -21 dB |

Each tone snapshot had valid=1, warmup=1, and analysis count=24. At the final
snapshot, AD/DA/process were 3820/3820/3820. Maximum observed Analyzer,
algorithm, frame-service, and frame-latency times were 299587, 360424,
809876, and 810150 cycles. All four safety counters remained zero.

The UART serialization check synchronized immediately after a processed-frame
boundary. The first audit was observed with pending=1, complete=0, request=0,
and baseline frame 3673. Writing a second request while halted for the Watch
snapshot retained request=1, kept pending=1, and left the baseline at 3673.
After resuming, the retained request started the next audit at baseline 3674.
The final state was pending=0, complete=1, request=0, and all four UART safety
deltas were zero. Exactly two bounded `P33 FEAT` lines were captured.

The formal JSON, UART log, WAV stimuli, and build artifacts are retained under
`%TEMP%\DSP_LAB_0507\smart_bass_v1\a490e80`. This establishes only the
Analyzer/flow prerequisite for the exact a490e80 build. Smart Bass processing
was compiled out and was not measured in this baseline.

## 10. Smart Bass A-G validation

**Date:** 2026-07-17

**Exact feature commit:**
`eb2eb1f0e6274f253ac8d5e94704369f30955e6c`

**Result:** `MEASURED_ON_CURRENT_BOARD_SMART_BASS_EB2EB1F`

The loaded output used Project 33, Analyzer ON, Smart Bass ON, and LCD OFF.
It came from the exact-SHA build-matrix D artifact. Every formal DSS
stage captured `P33 BUILD eb2eb1f`, INIT 01 through 08, and INIT 11 over
UART2. The build diagnostics and linker XML were already zero-error before
the board run.

### 10.1 A-C identity and trigger

With runtime Smart Bass disabled for 60 seconds, AD, DA, and process counts
were all 2890. Applied level, processing-active, transition, Analyzer analysis
count, deadline, latency miss, overlap, dropped, clip, saturation, and
nonfinite counters were zero. The bypass service cost remained bounded: the
last and maximum Smart Bass counts were 1388 and 1880 cycles.

At 1953.125 Hz with FLAT base EQ, Presence was dominant at 7 dB and Smart
Bass stayed at requested/applied level 0. The 8192-sample input and output RAW
captures were byte-identical (SHA-256
`4451813DDDE30B77364B4B24726DC8A167D8B4D7A623DC5E14EB07188AD9865B`), so
the measured transfer difference was 0.000000 dB.

At 97.65625 Hz, Bass was dominant at 19 dB. MEDIUM advanced only through
adjacent levels 0, 1, 2, 3, and 4, then settled with a -2.0 dB requested
shelf and no transition active. Relative to the Smart Bass OFF capture, the
steady-state output transfer changed by -1.460741 dB at this frequency. This
is a measurable attenuation and is consistent with a -2 dB low shelf whose
corner is 125 Hz. No clip or safety counter increased.

### 10.2 D debounce and operator observation

Board calibration measured `mix12` at 4 dB and `mix14` at 7 dB Bass-relative
level. Twelve alternating two-second source windows produced 152 new
Analyzer decisions but only 12 adjacent Smart Bass level changes. The path
stayed within levels 0 and 1 and ended at stable level 0. All objective safety
counters remained zero.

The operator test used a separate 18-second window with no debugger halt or
Watch read while audio was running. The repeat processed 920 frames and kept
Analyzer and Smart Bass decisions active. The operator reported normal audio
was audible and no sustained pumping or obvious clicks were heard. This is
recorded only as `SUBJECTIVE_OPERATOR_OBSERVATION`; it is not an analog
distortion measurement.

### 10.3 E-F release and BASS preset composition

Switching from the Bass tone to the Presence tone released applied levels
4, 3, 2, 1, and 0 through adjacent transitions. The six-second observation
ended with requested/applied/pending level 0, transition 0, and
processing-active 0. Deadline, latency miss, overlap, dropped, clip,
saturation, and nonfinite counters remained zero.

The BASS base preset remained `[3, 3, 2, 1, 0, 0, 0, 0, 0, 0]` dB before and
after Smart Bass reached MEDIUM level 4. Builder slice count stayed at zero;
control request, accepted, and applied tokens all stayed at 2. Smart Bass did
not rewrite a ten-band gain, start the custom builder, or submit an EQ control
request.

### 10.4 G five-minute stability

The fixed 10.24-second `music_like.wav` stimulus loop ran for thirty 10-second
DSP windows. A short JTAG halt between windows captured each safety snapshot;
the cumulative running time was 300 seconds. Process, AD, and DA frame counts
all advanced from 283 to 14911, a delta of 14628. Analyzer analysis count and
Smart Bass decision count both advanced by 1829. Input RMS remained -28 dBFS
at the final checkpoint, and Smart Bass stayed at bounded MEDIUM level 4.

| Maximum or counter | Result |
| --- | ---: |
| Analyzer cycles | 299890 |
| Smart Bass cycles | 438088 |
| Algorithm cycles | 785688 |
| Frame-service cycles | 1220364 |
| Frame-latency cycles | 1220618 |
| Deadline miss | 0 |
| Latency miss | 0 |
| Service overlap | 0 |
| Dropped frame | 0 |
| Clip | 0 |
| Smart Bass saturation | 0 |
| Smart Bass nonfinite | 0 |

The operator also confirmed normal audio was audible during G. This confirms
the exact eb2eb1f digital flow and runtime diagnostics on the current board;
external analog THD, SNR, calibrated frequency response, and SPL remain
unmeasured.

Formal JSON, UART, RAW, stimulus, operator-observation, build, and gate
evidence is retained under
`%TEMP%\DSP_LAB_0507\smart_bass_v1\eb2eb1f`. The A-G gate file is
`hardware_gate_A_to_F.json`, and the G result directory is
`board_G_formal_20260717_172449`.

### 10.5 Combined transition stress

A short supplemental run used the same exact eb2eb1f
`Project33_SmartBass` output and the 97.65625 Hz integer-bin Bass stimulus.
It started from FLAT with Smart Bass enabled at MEDIUM. The test submitted
FLAT to BASS while the level 0 to 1 Smart Bass transition was active, then
submitted BASS to VOCAL while the level 1 to 2 transition was active.

Both overlap windows were directly observed: at process frame 187 the EQ
transition target was BASS while Smart Bass transition-active was one, and at
process frame 196 the EQ target was VOCAL while Smart Bass transition-active
was one. The run settled in VOCAL with no EQ transition target, Smart Bass
level 4, and no Smart Bass transition active.

Maximum Analyzer, Smart Bass, algorithm, frame-service, and frame-latency
counts were 299574, 437614, 1811815, 2248570, and 2248834 cycles. AD, DA, and
process counts ended at 265, 264, and 264. Deadline, latency miss, overlap,
dropped, clip, Smart Bass saturation, and Smart Bass nonfinite counters were
all zero. This debugger-polled objective test is not used as listening
evidence and is distinct from both `SEGMENTED_30X10S` and the deferred
`CONTINUOUS_300S` run.

## 11. Dynamic Clarity V1 objective A-H validation

**Date:** 2026-07-17

**Exact feature commit:**
`47337a0b9fe9b3f7025671806e3e3b2e8f269f56`

**Result:** `MEASURED_ON_CURRENT_BOARD_OBJECTIVE_ONLY`

No human-ear confirmation was performed. Subjective clarity, clicks, pumping,
and general sound quality are explicitly `NOT_PERFORMED`. The result covers
UART, Watch, internal PCM16 RAW capture, and runtime safety counters only.

The loaded output was the clean build-matrix F artifact with Project 33,
Analyzer ON, Smart Bass ON, Dynamic Clarity ON, and LCD OFF:

```text
DSP_LAB_PROJECT_SELECT=33
EQ_ENABLE_AUDIO_FEATURE_ANALYZER=1
EQ_ENABLE_SMART_BASS=1
EQ_ENABLE_DYNAMIC_CLARITY=1
EQ_ENABLE_LCD_DISPLAY=0
EQ_USE_GENERATED_BUILD_ID=1
```

The output SHA-256 was
`19026BDDC21BCA38F643A16859F37B6C7A8D3174E96664207828199ED01B38DC`.
The C6000 build had zero compiler diagnostics and
`<link_errors>0x0</link_errors>`. Map evidence placed Analyzer, Smart Bass,
Dynamic Clarity, and EQ state in `.subband_l2` using 16488, 252, 260, and
1064 bytes respectively; 242024 bytes remained free. UART captured
`P33 BUILD 47337a0`, INIT 01 through 08, and INIT 11.

### 11.1 A-D identity, trigger, mapping, and frequency bounds

Runtime OFF ran for 60 seconds with AD/DA/process all at 3062 frames. Smart
Bass and Dynamic Clarity remained at level zero and all safety counters were
zero. Its 8192-sample input/output capture was byte-identical with SHA-256
`B8722488486A8BB2690ACB7BC96F4F33FBFB4AEEE3A247D3EF2ABDF633AFC797`.

The 1953.125 Hz Presence stimulus measured relative features
`[-37, -44, 7, -36] dB` for Bass, Mud, Presence, and Brightness. Dynamic
Clarity stayed at requested/applied level zero. Its RAW pair was also
byte-identical with SHA-256
`DE81746DF75F2BA6B4A3671692A410F8A3F0E3CFE5EDF9AE92E2F2FBE4C5E5FB`.

A pure 390.625 Hz Mud tone measured Mud `14 dB`, Presence `-51 dB`, and
masking `66 dB`, but correctly remained at level zero with
`DYNAMIC_CLARITY_REASON_WEAK_PRESENCE`. This is required by the absolute
Presence gate; a pure Mud tone cannot be a valid trigger without weakening
the specified safety contract.

The actual trigger therefore used a Mud-dominant fixed combination with both
absolute gates satisfied. It measured Mud `13 dB`, Presence `-2 dB`, and
masking `16 dB`. MEDIUM advanced only through levels 0, 1, 2, 3, and 4, then
settled at level 4. Relative to runtime OFF, internal RAW transfer changed by
`-1.962837 dB` at 390.625 Hz, `-0.046256 dB` at 1953.125 Hz,
`-0.077097 dB` at 97.65625 Hz, and `+0.003331 dB` at 8007.8125 Hz.

The fixed masking sweep used Analyzer band-density compensation for the 12
Mud bins and 65 Presence bins:

| Target masking | Board masking | Expected level | Requested/applied |
| ---: | ---: | ---: | ---: |
| 2 dB | 2 dB | 0 | 0 / 0 |
| 5 dB | 5 dB | 0 | 0 / 0 |
| 8 dB | 8 dB | 2 | 2 / 2 |
| 12 dB | 12 dB | 3 | 3 / 3 |
| 16 dB | 16 dB | 4 | 4 / 4 |

The weak-Mud test measured Mud/Presence `-12/-20 dB` and stopped with reason
4. The weak-Presence test measured `14/-28 dB` and stopped with reason 5.
Both remained at level zero.

### 11.2 E-F release and independent composition

Switching the combined trigger to Presence released Dynamic Clarity through
levels 4, 3, 2, 1, and 0. Every adjacent decrement occurred after exactly two
new Dynamic Clarity decisions. Smart Bass independently followed the same
adjacent release sequence for this stimulus. The final processing-active and
transition flags were zero. The listening portion of E was not run.

The three-frequency combined input measured Bass `18 dB`, Mud `3 dB`,
Presence `-10 dB`, and masking `13 dB`. Smart Bass and Dynamic Clarity both
settled independently at MEDIUM level 4. Across A-F, all ten base EQ gains,
all mailbox/control tokens, response generations, and the complete builder
fingerprint were unchanged. Builder slices stayed zero.

### 11.3 G deterministic triple-transition windows

Two deterministic windows were captured in one load session. The first began
from FLAT with both dynamics at identity and overlapped FLAT to BASS with the
first Smart Bass and Dynamic Clarity transitions. After both dynamics released
to identity while BASS remained active, the second overlapped BASS to VOCAL
with both first dynamic transitions. Both triple-overlap flags were observed.

The two legal preset requests published mailbox stable sequences 2 and 4.
At the end, request, observed, accepted, target, prepared, installed, and
applied tokens were all 4. Builder generation, slices, cancel, and restart
counts were all zero. The final VOCAL gains were
`[-2,-1,0,1,2,3,2,1,0,-1]` dB.

The two 8192-sample transition captures had zero clipped samples and zero
repeated adjacent frames. Maximum adjacent output steps were `0.084046 FS`
and `0.058838 FS`. Maximum observed cycles were:

| Component | Maximum cycles |
| --- | ---: |
| Analyzer | 299524 |
| Smart Bass | 438590 |
| Dynamic Clarity | 2497790 |
| Complete algorithm chain | 4295830 |
| Frame service | 4789638 |
| Frame latency | 4789896 |

Deadline miss, latency miss, overlap, dropped, clip, both saturation counters,
and both nonfinite counters remained zero.

### 11.4 H approximately 300-second uninterrupted window

The fixed `music_like.wav` loop ran for one uninterrupted 300-second target
window. There was no DSS halt, Watch read, or periodic UART feature request
during the window. The single final snapshot was labeled `CONTINUOUS_300S`,
but that label described the target rather than the measured DSP duration.

Process frames advanced from 8560 to 23179, a delta of 14619. Analyzer,
Smart Bass, and Dynamic Clarity decisions each advanced by 1827. Both dynamics
ended at MEDIUM level 4. Control token 6 and builder slice count 0 were
unchanged across the window. Maximum cycles at the final snapshot were
299524 Analyzer, 438590 Smart Bass, 2497790 Dynamic Clarity, 4323000 complete
algorithm, 4789638 frame service, and 4789896 frame latency. Every safety,
clip, saturation, and nonfinite counter was zero.

At 50 kHz with 1024 samples per frame, 14619 frames equal `299.39712 s`.
Therefore this legacy result is an approximately 300-second uninterrupted run,
not proof that at least 300 seconds of audio were processed. Section 12.3
records the corrected measurement.

Formal JSON, UART, RAW, fixed stimuli, and capture metrics are retained under
`%TEMP%\DSP_LAB_0507\dynamic_clarity_v1\47337a0\board_objective_20260717_194013`.
`tools/run_dynamic_clarity_board.ps1 -AnalyzeOnly` reproduced all RAW and UART
acceptance checks after the board disconnected. External analog THD, SNR,
calibrated frequency response, SPL, and all subjective listening claims remain
unmeasured.

## 12. Dynamic Clarity diagnostic closure

**Date:** 2026-07-17

**Feature SHA:** `47337a0b9fe9b3f7025671806e3e3b2e8f269f56`

**Prior board-validation SHA:**
`9e5190594048d3c00a68a2e78b9d59b5a03aca40`

**Diagnostic commits:** `b8cb856`, `9afb130`

The board diagnostic output reported `P33 BUILD 9e51905`, `dirty=1`, reached
INIT 11, and had SHA-256
`6D973DF562EA18279CBED90ECA0A4B21773D414AC4762C26985521F11AA01BC4`.
That exact output hash is retained with every RAW and cycle file hash. The
subsequent clean A-G compile/link matrix used `9afb130`, `dirty=0`; it was not
loaded as part of this hardware run. This distinction prevents a clean build
claim from being substituted for the exact diagnostic binary that ran.

No algorithm parameter, threshold, gain table, center frequency, transition
duration, Analyzer behavior, Smart Bass behavior, EQ bank, control token,
I/O driver, LCD, Touch, Project 3.2 path, or `main.c` default was changed.

### 12.1 Isolated and real-time cycle evidence

The independent benchmark stopped audio services and measured 56 jobs: two
modules, four fixed inputs, and seven identity/stable/transition cases. Each
job used 64 warm-up calls and 4096 measured calls. INIT stayed zero, AD/DA
flags stayed zero, audio services never started, and all four module safety
counters stayed zero.

Across all jobs, Dynamic Clarity had P99 maximum `336177.2` and absolute
maximum `336505` cycles. Smart Bass had P99 maximum `336178.15` and absolute
maximum `336474` cycles. The largest Dynamic-to-Smart P99 ratio was
`1.007143`. First-call and warm maxima were also comparable.

The real-time path maxima were:

| Path | Max cycles | FLAG_AD | FLAG_DA |
|---|---:|---:|---:|
| Identity | 2199 | 0 | 1 |
| Stable filter | 253122 | 0 | 1 |
| 0 to filter | 2183186 | 0 | 1 |
| Filter to filter | 2190175 | 0 | 1 |
| Filter to 0 | 2183180 | 0 | 1 |

Deadline, latency miss, overlap, dropped, clip, saturation, and nonfinite
counters were zero. The approximately 2.19 million-cycle peaks also appeared
in the no-debug-access continuous window. The root-cause category is therefore
`C_INTERRUPT_OR_PREEMPTION_INCLUDED`: real-time TSCL intervals include DA
service or interrupt preemption. The evidence does not support true filter
compute cost, cold first-call behavior, or a debugger-only artifact.

### 12.2 Objective transition evidence

The `BOARD_INTERNAL_PCM_OBJECTIVE_TRANSIENT` test captured stable and actual
0-to-1, 1-to-2, and 1-to-0 cases for deterministic dual-tone, music-like, and
periodic pseudorandom inputs. All nine captures passed matched-input and
digital-integrity checks.

| Metric | Worst recorded value |
|---|---:|
| Minimum input correlation | 0.966769 |
| Residual peak | -33.356119 dBFS |
| Residual RMS | -46.877868 dBFS |
| First-difference peak | -30.615934 dBFS |
| Second-difference peak | -25.845146 dBFS |
| 4-20 kHz boundary/internal ratio | 0.857015 dB |
| Repeated adjacent frames | 0 |
| Clipped samples | 0 |

These numbers characterize board-internal PCM16 behavior only. No perceptual
threshold is applied, and no claim is made about audible clicks, subjective
clarity, or external analog output.

### 12.3 Corrected uninterrupted runtime

The corrected tool records host monotonic boundaries and DSP process-frame
boundaries independently. Analyzer, Smart Bass, and Dynamic Clarity were ON;
LCD was OFF; `music_like.wav` looped continuously. During the measurement
window there were zero halts, Watch reads, RAW reads, periodic UART requests,
or intermediate DSS accesses. Only the final status was read.

| Time definition | Result |
|---|---:|
| Target wall time | 300.0 s |
| Measured host elapsed | 303.0231563 s |
| DSP frame delta | 14764 |
| Measured DSP audio | 302.36672 s |

The DSP therefore processed at least 300 seconds of audio in this corrected
window. Analyzer, Smart Bass, and Dynamic Clarity counters each advanced from
36 to 1882. All deadline, latency, overlap, dropped, clip, saturation, and
nonfinite counters were zero. Subjective observation was `NOT_PERFORMED`.

### 12.4 Clean build matrix and archive

The clean `9afb130`, `dirty=0` A-G matrix covered Project 32; Project 33
Baseline; Analyzer only; Analyzer plus Smart Bass; Analyzer plus Dynamic
Clarity; both dynamics with LCD OFF; and both dynamics with LCD ON. All seven
full builds had zero warnings and `link_errors=0x0`. Production F and G maps
contained no benchmark or timing-diagnostic symbols. LCD ON was compile/link
only and was not asserted as a board UI result.

Compact, parseable evidence is under
`docs/evidence/dynamic_clarity_47337a0/`. Its SHA-256 manifest references 88
retained local files without committing WAV, RAW, `.out`, map, CSV, PNG, or
cache artifacts. External analog THD, SNR, calibrated response, and SPL remain
`UNMEASURED`; all subjective listening remains `NOT_PERFORMED`.

## 13. Harshness Guard V1 objective archive

**Date:** 2026-07-18

**Baseline SHA:** `a4fc259a5c925f20ca94c331eac8e90338ce03a9`

**Feature SHA:** `dade304fdeec3fb1acc4be407efac32d37a321e3`

**Validation/tooling SHA:** `6ef2c86abe90c68b9148702e10c3b7ff4c745d28`

Harshness Guard is a relative-energy high-frequency protection rule:
`Brightness relative dB - Presence relative dB`. It is not a perceptual
harshness detector. Runtime defaults are disabled/MEDIUM. Five fixed 6 kHz,
one-octave peaking banks provide 0, -0.5, -1.0, -1.5, and -2.0 dB levels with
an 80 ms adjacent-state crossfade.

### 13.1 Host and clean-build evidence

The Host control export contains 128 finite rows and reaches levels 0 through
4 without positive gain. The five 129-point responses measure 0.000000,
-0.498891, -0.997781, -1.496661, and -1.995530 dB at the nearest 6 kHz grid
point. At level 4, the nearest 1 kHz and 16 kHz points are only -0.030544 and
-0.081999 dB respectively. These are digital model results only.

The clean `dade304` A-H CCS matrix covered Project 32 and all required Project
3.3 feature combinations. All eight full builds exited zero, produced an
output, had zero warnings, and reported `link_errors=0x0`. At that feature
commit the Guard state was 220 bytes and the all-dynamics profiles used 20340
of 262144 `.subband_l2` bytes.

Performance closure aligns the Guard mutable-state offsets without changing
its coefficients or arithmetic. The state is now 260 bytes, matching Dynamic
Clarity, and production G uses 20380 of 262144 `.subband_l2` bytes. The clean
`4fd96b6`, `dirty=0` Project 32 and production-G builds both had warnings=0
and `link_errors=0x0`; production G had diagnostic symbol hits=0. Its six-
second boot reached INIT 11 with deadline, latency miss, overlap, dropped,
clip, all three saturation, and all three nonfinite counters zero, then left
the target `RUNNING_DISCONNECTED`.

### 13.2 Complete A-I objective run

The completed A-I gate used clean build `6f58613`, `dirty=0`, with PC line-out
to ADC CH1, DAC CH1 to the connected output path, 50 kHz, 1024-sample frames,
and LCD OFF. UART reported `P33 BUILD 6f58613`, INIT 01 through 08, and INIT
11. The runner's overall result was `pass=true` with evidence label
`MEASURED_ON_CURRENT_BOARD_OBJECTIVE_ONLY`.

| Gate | Objective result |
|---|---|
| A Runtime OFF | 60-second window; Guard identity; 8192 samples byte-exact |
| B Presence | level 0; 8192 samples byte-exact; no false trigger |
| C Brightness | MEDIUM reached level 3; center transfer -1.508627 dB |
| D Mapping | measured 2/7/10/14/18 dB -> levels 0/0/1/2/3 |
| E Release | silence completed adjacent release to level 0 |
| F Combined | Smart 4, Clarity 4, Guard 3 observed; builder/control unchanged |
| G Four-way | FLAT->BASS and BASS->VOCAL overlap observed; overlap count 4 |
| H Transient | objective digital transient gate passed |
| I Continuous | host 301.040023 s; DSP audio 300.38016 s; no debugger access |

The worst maxima in that full objective run were Analyzer 301202, Smart Bass
441040, Dynamic Clarity 438322, Harshness Guard 4027564, complete algorithm
6278052, frame service 6718432, and frame latency 6718864 cycles. Deadline,
latency miss, overlap, dropped, clip, all three saturation counters, and all
three nonfinite counters were zero. The ten-band gains, builder generation and
slice count, and control tokens did not change because of a dynamic decision.

### 13.3 Current-SHA diagnostic closure

Build `6ef2c86`, `dirty=0`, passed the original isolated 84-job benchmark but
exposed a maximum Guard P99 ratio of 3.254028. The wrapper was fair: state
restore, input copy, counter reads, statistics, and checksum were outside the
TSCL interval. All three source files used the same TI C6000 command:
`--cmd_file=ccsIncludes.opt -mv6740 -O3 -g --c89`, with no explicit or
file-specific speed/size, floating-point, function-subsection, or software-
pipeline option.

The archived benchmark functions were in external-DDR `.text` at
`c0173ca4` Dynamic Clarity, `c0175c90` Harshness Guard, and `c0177b20` Smart
Bass. Guard did not cross the 32 KiB boundary; Smart Bass did and remained
fast. All three loops were disqualified from software pipelining by control
code. They had the same visible call graph: `memcpy`, `__c6xabi_divf`, and the
module's transition helper. There was no float/integer conversion helper or
other extra per-sample Guard helper.

The decisive generated-code difference was stack traffic. Baseline Guard had
50 fixed-stack loads/stores versus 6 in each sibling: one state-base store and
42 reload sites from stack slot 3. Its stack frame was `0x48` versus `0x40` in
each sibling, with 166/96 total loads/stores versus 133/94. Matching
`active_state=140` and `pending_state=148` to the two fast layouts removed all
state-base reloads. Final Guard and Dynamic Clarity both have a 3904-byte OFD
executable range (`nm6x` symbol size 3992), `0x40` frame, 6 fixed-stack
loads/stores, and 133/94 total loads/stores. Of 1079 compared opcode lines,
only 5 external call displacements and 4 level-count constants differ. The
root-cause category is `REGISTER_SPILL_OR_ALIASING`.

The final `4fd96b6`, `dirty=0` diagnostic used 36 jobs: three modules, two
deterministic inputs, and identity, stable 1, stable MEDIUM, 0->1, 1->2, and
1->0 paths. Every job used 64 warm-up and 4096 measured calls. Summary maxima:

| Module | Min | Median | P95 | P99 | Max | First | Warm max |
|---|---:|---:|---:|---:|---:|---:|---:|
| Dynamic Clarity | 594 | 335459 | 335986 | 336205.3 | 336580 | 335519 | 336259 |
| Smart Bass | 594 | 335458 | 335980 | 336200.2 | 336591 | 336216 | 336076 |
| Harshness Guard | 594 | 335455 | 335981 | 336201.3 | 336525 | 335835 | 336305 |

The largest Guard-to-Dynamic and Guard-to-Smart P99 ratios were both
1.004261, below the 1.25 target. Twelve deterministic identity/stable/
transition paths preserved their frozen PCM16 hashes, endpoint levels,
transition counts, processing state, saturation count, and nonfinite count.
All board benchmark safety counters were zero.

The same build passed nine actual 0->1, 1->2, and 1->0 internal PCM16
transitions across dual-tone, music-like, and deterministic periodic noise.
Worst objective values were residual peak -36.749399 dBFS, residual RMS
-49.376686 dBFS, first difference -33.642987 dBFS, second difference
-28.692639 dBFS, 4-20 kHz boundary/internal ratio 3.968835 dB, absolute DC
step 0.000181662 full scale, and frame-boundary discontinuity 0.031341553 full
scale. Repeated adjacent frames and clipped samples were zero; all real-time
safety counters were zero. These values make no perceptual claim.

A new full A-I rerun under `6ef2c86` was stopped after the user requested a
shorter pressure-test plan. It is `ABORTED_BY_USER_TIME_LIMIT`, not a passed
current-SHA A-I result. The interrupted runner did not emit a final
`board_result.json`.

The complete A-I result on clean build `6f58613` remains accepted. The user
explicitly decided not to rerun it:
`FULL_A_TO_I_RERUN_NOT_REQUIRED_BY_USER_DECISION`. This is not incomplete or
failed validation and is not a blocking issue.

Manual Analyzer reset now sets the Guard analyzer fault, invalidates the
consumed epoch, and disables the Guard request while retaining both filter
states. Host state-machine evidence follows `3 -> 2 -> 1 -> 0` through three
adjacent 80 ms releases, with monotonic applied gain and zero saturation/
nonfinite. A new valid/warm epoch re-enables the still-requested Watch setting
and starts one adjacent attack; replay of the same epoch is rejected. The
optional short board reset test was not run:
`TARGETED_RESET_BOARD_TEST_NOT_PERFORMED`.

### 13.4 Remaining boundary

Subjective listening is `NOT_PERFORMED`. External analog THD, SNR, calibrated
frequency response, and SPL are `UNMEASURED`. The compact archive is
`docs/evidence/harshness_guard_dade304/`; large local artifacts remain under
`%TEMP%` and are referenced by SHA-256 rather than committed.

## 14. Historical Project 3.3 status UI baseline

### 14.1 Provenance and scope

The final UI board run used clean build
`5d1525ae984acb53765e00cf678874189d2aaf30`, E profile, generated dirty=0.
The exact loaded `.out` SHA-256 was
`1677223adb4ffc7e98e977eb689e49aef58ae144e78aa72744e8d9e890987c49`.
PC default line-out supplied a generated 50 kHz multi-tone to ADC CH1. DSS
changed existing preset and dynamic request variables and read Watch counters.
It made no visual, physical-touch, or listening assertion.

An intermediate `41d8a1b` run stopped correctly when Analyzer job 9 reached
2691228 cycles, above the 2280000-cycle hard limit. Audio safety counters were
zero. The inner mutable bar width was then reduced without changing Analyzer
math, frequency bands, smoothing, cadence, dB mapping, or outer layout.

### 14.2 Preserved 60-second E-profile window

**Result:** audio-safety objective counters passed under the earlier 5 ms hard
limit. This build is now the Stage A visual failure baseline. It fails the
current normal LCD acceptance because 149 jobs exceeded 2 ms, and the operator
observed a vertical circular screen shift.

The window processed 2935 frames and 367 Analyzer publications. Input/output
peaks were 792/790 PCM16 counts, proving nonzero CH1 traffic. Maximum algorithm,
frame-service, and frame-latency times were 4778400, 5217075, and 5217496
cycles, all below the 9338880-cycle frame budget.

| LCD category | Count at end | Window delta | Last cycles, post-run snapshot | Max cycles | Max ms |
|---|---:|---:|---:|---:|---:|
| Preset | 51 | 46 | 329393 | 340020 | 0.746 |
| Dynamic | 113 | 104 | 1174682 | 1403866 | 3.079 |
| Chain | 15 | 12 | 710814 | 924186 | 2.027 |
| Analyzer | 51 | 38 | 1287791 | 1800578 | 3.949 |

The Last column was read after the completed stress through an address-only
DSS snapshot. That connection allowed nine additional UI jobs, so Last is a
post-run diagnostic value; Count/Delta and Max come from the exact stress
summary. The additional jobs did not increase any recorded maximum.

The stress completed 200 runtime jobs. It recorded 149 new >2 ms warnings,
zero >5 ms jobs, 327 audio deferrals, five audio-arrived-during-draw events,
zero unexpected full redraws, and zero LCD auto-disables. UI state was 244
bytes; Touch state plus transform was 36 bytes.

Deadline, latency miss, overlap, dropped, clip, all three dynamic-stage
saturation counters, and all three nonfinite counters were zero. Therefore the
five audio-arrived-during-draw observations did not produce an audio safety
failure.

### 14.3 Touch and remaining boundary

No operator touch was requested. The controller nevertheless recorded 10
accepted and 6 rejected actions in the stress window. A post-run diagnostic
reported 95 DOWN, 51 RELEASE, zero I2C errors, and final raw/screen coordinate
`(678,37)` with last action V-SHAPE. These may be unconfirmed physical or
spurious controller events; they are not accepted as a physical Touch pass.

For that historical build, Chinese glyph appearance, complete static-page
alignment, lack of visible flicker, and physical hitbox accuracy remained
`PENDING_OPERATOR_TOUCH_VALIDATION`. No new full A-I run or 300-second run was
performed. External analog THD, SNR, calibrated response, and SPL remain
unmeasured. The compact archive is `docs/evidence/equalizer_ui_5d1525a/`.

## 15. LCD circular-shift suppression

### 15.1 Source and Host boundary

The preserved failure is `OPERATOR_VISUAL_FAILURE_SCREEN_CIRCULAR_SHIFT` on
clean build `5d1525a`. Commit `67a22ef` replaced Project 3.3 shared rectangle
state with bounded local rectangles and added low-cadence LCDC/DMA/address and
framebuffer-canary diagnostics. Commit `b23a7ce` changed Analyzer rendering to
latest-wins differential strips of at most 16 pixels and split value text into
a separate field. Neither commit changes Project 3.2 or audio algorithms.

At `46bc54e`, 102 Project 3.3/equalizer Host tests passed. The Project 3.2
shared FFT regression passed 8 frozen FFT cases and 12 consecutive WOLA hops
bit-exactly. The clean A-E matrix passed with zero warnings, zero errors, and
`link_errors=0x0` for every profile.

### 15.2 Ten-minute static alignment gate

**Result:** `MEASURED_ON_CURRENT_BOARD_OBJECTIVE_ONLY` plus
`OPERATOR_VISUAL_OBSERVATION_PASS`.

The clean `67a22ef`, dirty=0, no-Touch alignment build ran for 600,000 ms. AD,
DA, and process counters advanced together from 195 to 29,443. Runtime mask and
runtime job count remained zero. FB0 stayed at base `0xC0000004` and end
`0xC00BB820`; canary checks advanced from 1 to 116. Raster fault, sync lost,
FIFO underflow, frame-address mismatch, canary failure, bounds failure,
deadline, latency miss, overlap, dropped, and clip counters were zero.

Automation made no visual assertion. After the window, the operator explicitly
reported that no circular shift was visible. The exact `.out` SHA-256 was
`50c95e3e4b06a0a3d22ccc05744643c01aeec4a120ed8d7db9c36936f0942ba7`.

### 15.3 Hardware removed before remaining gates

The hardened Dynamic Status ten-minute run was prepared but not executed after
the board was removed. Dynamic Status timing/visual confirmation, five-minute
physical Touch, and ten-minute combined audio/Analyzer/dynamics/preset/Touch
remain `PENDING_HARDWARE`. Stage A is not complete and Stage B ten-band editor
work has not started. Compact evidence is under
`docs/evidence/equalizer_lcd_stability_46bc54e/`; details are in
`docs/equalizer_33_lcd_drift_diagnosis.md`.
