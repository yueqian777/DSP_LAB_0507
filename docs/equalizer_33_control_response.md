# Project 3.3 Stage B Control and Response Specification

**Status:** `DESIGN_APPROVED`; specification/TDD review in progress;
`IMPLEMENTATION_NOT_STARTED`.

**Baseline:** `3d9bb2f3c65123761130571c624e82b7d5306c32`

**Planned implementation branch:** `codex/project33-control-response`. Before
Task 1 it must be fast-forwarded from the local `main` commit containing this
approved specification.

**Integration target:** local `main` after all automated checks pass. No remote
push is authorized by this specification.

**Hardware status:** `PENDING_HARDWARE`

## 1. Objective

Stage B completes the control and evidence layer around the existing Project 3.3
ten-band real-time equalizer. It does not introduce a new filter algorithm.

The deliverable provides:

- stable arbitrary ten-band gain commands;
- bounded, nonblocking custom-bank preparation;
- deterministic request, accepted, prepared, pending, and applied state;
- immutable response snapshots with explicit signal-path semantics;
- independent desired-versus-actual response analysis;
- a stable backend for a later touch preset and slider stage.

The existing CH1 input -> EQ -> CH1 DAC path, fixed presets, RBJ coefficient
formulae, automatic preamp/headroom rule, and 120 ms bank crossfade remain the
authoritative signal-processing implementation.

## 2. Scope and non-goals

### 2.1 In scope

- Host C unit and contract tests.
- Independent Python reference calculations.
- Offline CSV and PNG generation.
- CCS Project 32 and Project 33 compile/link checks.
- Watch-visible control, builder, response, and token diagnostics.
- Board-side mailbox acceptance, one-slice builder service, prepared-bank
  installation, and frame-boundary token observation.

### 2.2 Explicitly out of scope

- DSS execution or program download.
- Claims about board timing, audio quality, LCD behavior, or touch behavior.
- LCD curve drawing or calculation of a 129-point curve in the board flow.
- Touch buttons, sliders, coordinates, or gesture handling.
- Microphone input, ADC2, environmental-noise capture, road-condition
  recognition, ANC, adaptive denoising, or automatic scene selection.
- Changes to Project 3.2, Project 3.4, low-level ADC/DAC/EDMA/PRU/interrupt
  drivers, the 50 kHz sample rate, the 1024-sample frame, or ping-pong buffers.
- Changes to RBJ equations, center frequencies, preset gains, preamp policy, or
  the 120 ms crossfade.

Future city, highway, rain, and tunnel modes remain manually selected fixed EQ
presets. They are not part of Stage B.

## 3. Existing implementation audit

The baseline already contains:

- `Equalizer_GetCoefficientInfo`;
- `Equalizer_GetSystemResponse`;
- `Equalizer_GetSystemMagnitudeDb`;
- `Equalizer_GetSystemPhaseDeg`;
- `Equalizer_GetSystemGroupDelaySamples`.

The current synchronous custom path is:

```text
Equalizer_SetBandGainDb / Equalizer_SetAllGainsDb
  -> EQ_QueueOrStartCurrentTarget
  -> EQ_StartCurrentTarget
  -> EQ_InstallRbjTargetKind
  -> EQ_BuildRbjBank
  -> ten section designs + complete 512-point headroom scan
```

If a newer custom target is queued while a bank transition is active, the
transition-completion sample can call `EQ_StartCurrentTarget` from
`EQ_ProcessRbjSample`. That can execute the complete custom build and scan in
the audio sample path. Stage B must remove that path.

`EQ_EnsureRbjBankCache` is lazy, but `Equalizer_Init` currently calls it before
the board flow calls `Adc_Start` and `Dac_Start`. Stage B makes this ordering an
explicit contract and verifies that post-start preset changes do not increase
the full-scan counter or start the custom builder.

The legacy `Equalizer_GetBandMagnitudeDb` reports the historical fixed-Q
bandpass prototype. It is not the response of one RBJ cascade section.

The legacy system-response APIs select the pending bank during a transition.
They therefore describe a static target-like bank, not the time-varying audible
crossfade. They remain for compatibility, with corrected documentation; all new
code uses explicit snapshots.

The pre-Stage-B automated baseline is retained as regression evidence, not as
new board validation:

- Host evaluator: `EqualizerEval_OfflineTest_All failures=0`;
- existing Python contracts: 38 of 38 passed;
- independent Python/C magnitude comparison: maximum error
  `0.002120463721 dB`;
- existing `Debug/DSP_LAB_0507.out` is present and the existing link XML reports
  `<link_errors>0x0</link_errors>`.

These results describe the baseline checkout only. Every Stage B board property
remains `PENDING_HARDWARE` until later physical-board testing.

## 4. Architecture

### 4.1 Control layer

`user_equalizer_control.c/.h` owns:

- the seqlock mailbox;
- validated local request snapshots;
- the command shadow gains;
- accepted, prepared, installed, and applied sequence bookkeeping;
- the incremental custom-bank builder;
- stale/cancel/restart/reject/coalesce diagnostics.

External Watch code and later touch code write only the mailbox API surface.
They never write `EQ_STATE`, `active_bank`, `pending_bank`, or biquad
coefficients.

The stable control service surface is:

```c
void EqualizerControl_Init(EQ_CONTROL_STATE *control,
                           const EQ_STATE *equalizer);
int EqualizerControl_ServiceMailbox(EQ_CONTROL_STATE *control,
                                    const EQ_CONTROL_MAILBOX *mailbox,
                                    EQ_STATE *equalizer);
int EqualizerControl_BuilderEligible(const EQ_CONTROL_STATE *control,
                                     const EQ_STATE *equalizer);
int EqualizerControl_ServiceOneBuilderSlice(EQ_CONTROL_STATE *control);
int EqualizerControl_TryInstallReady(EQ_CONTROL_STATE *control,
                                     EQ_STATE *equalizer);
void EqualizerControl_ObserveFrameBoundary(EQ_CONTROL_STATE *control,
                                           const EQ_STATE *equalizer);
void EqualizerControl_RebaseAfterDirectPathChange(
    EQ_CONTROL_STATE *control,
    const EQ_STATE *equalizer);
```

Mailbox service performs one seqlock copy attempt. Builder service performs at
most one of the 43 payload slices. Ready installation refuses a transition or a
stale candidate. Frame-boundary observation is the only control function that
may advance `applied_sequence`.

### 4.2 Equalizer core

`user_equalizer.c/.h` remains the single source of truth for:

- RBJ coefficient design;
- cache contents;
- headroom and preamp policy;
- bank installation;
- sample processing and crossfade.

Stage B exposes small reusable design and prepared-install primitives without
changing their mathematics. The core no longer starts an unprepared custom
target from the sample path.

The public core boundary added by Stage B is exact and bounded:

```c
int Equalizer_PublishLogicalTarget(
    EQ_STATE *st,
    const float gains_db[EQ_NUM_BANDS],
    int preset,
    unsigned long *generation_out,
    int *bank_id_out);

int Equalizer_CopyPresetGainsDb(int preset,
                                float gains_out[EQ_NUM_BANDS]);

int Equalizer_CopyCachedPreparedBank(
    int bank_id,
    int preset,
    unsigned long generation,
    EQ_CONTROL_SEQUENCE request_sequence,
    EQ_PREPARED_BANK *out);

int Equalizer_DesignRbjSection(EQ_BIQUAD *section_out,
                               int section,
                               float gain_db);
int Equalizer_EvaluateHeadroomPointDb(const EQ_FILTER_BANK *bank,
                                      int point_index,
                                      float *magnitude_db);
void Equalizer_FinalizeRbjBank(EQ_FILTER_BANK *bank,
                               int gains_are_flat,
                               float peak_db);
```

`Equalizer_PublishLogicalTarget` only validates/copies target metadata,
classifies the target through the existing private cache lookup, and allocates
the next nonzero core generation. It returns the authoritative bank ID. It never
designs coefficients, scans headroom, installs a bank, or starts a transition.

`Equalizer_CopyPresetGainsDb` is the only new reader for the private unchanged
RBJ preset table. The control module never duplicates preset gain constants.

`Equalizer_CopyCachedPreparedBank` succeeds only after cache prewarm and copies
one already-built fixed cache entry into a complete prepared candidate. It does
not call the synchronous bank builder and does not increment the full-scan
counter. The cache remains private to `user_equalizer.c`.

The one-section, indexed-headroom-point, and finalize primitives are also used
by the existing synchronous `EQ_BuildRbjBank` wrapper. The exact RBJ equations,
20 Hz to 20 kHz 512-point grid, peak rule, `-0.5 dB` safety margin, and dB-to-gain
conversion therefore have one implementation.

`user_equalizer.h` exposes `EQ_HEADROOM_POINT_COUNT` with the unchanged value
512. Core and control code use that one public constant; the incremental builder
does not depend on a private `.c` macro.

The existing direct target setters remain compatibility APIs only for Host or
pre-audio settled-state reference use, when no bank transition is active. They
are not a second post-start board-control authority, and their transition-active
case is a deterministic rejected/no-state-change operation. All post-start
board RBJ target changes, including fixed preset, FLAT, single-1-kHz, and custom
gain changes, are routed through the mailbox. Existing RAW_COPY, FLOAT_COPY,
and hard-bypass diagnostic path switches remain outside the target-token model;
after such a direct diagnostic change the flow calls
`EqualizerControl_RebaseAfterDirectPathChange`, which cancels uninstalled
builder/ready work, invalidates target and installed-pair latches, copies the
current active gains to the edit shadow, preserves historical observed,
accepted, prepared, and applied diagnostics, and never advances
`applied_sequence`. Returning to RBJ submits a fresh mailbox target.

The new mailbox path never calls a synchronous custom setter. Removing the
transition-completion call that starts an unprepared custom target is mandatory;
no new control/builder/response call is reachable from
`Equalizer_ProcessFrame` or `EQ_ProcessRbjSample`.

### 4.3 Response layer

`user_equalizer_response.c/.h` owns:

- immutable command and coefficient snapshots;
- explicit current-path classification;
- the canonical section and cascade complex-response calculation;
- desired visual interpolation;
- shape and total response metrics;
- section response and interaction-matrix calculations.

It never copies or changes `s1`, `s2`, `clip_count`, transition counters, or
audio samples.

### 4.4 Board-flow integration

`user_equalizer_flow.c/.h` owns one shared low-priority budget. In one processed
audio frame the board flow can execute at most one heavy background payload:

- one builder slice; or
- one LCD job.

Control acceptance, ready-bank installation, and token observation are bounded
metadata operations and do not count as one of the 43 builder payload slices.
They still run only in an audio-safe window.

## 5. Seqlock mailbox protocol

### 5.1 Mailbox representation

The completed even `sequence` is also the request token exposed through Watch.
No independent token can disagree with the seqlock commit identifier.

```c
typedef unsigned int EQ_CONTROL_SEQUENCE;
typedef char EQ_CONTROL_SEQUENCE_MUST_BE_32_BITS[
    (sizeof(EQ_CONTROL_SEQUENCE) == 4U) ? 1 : -1];
```

`EQ_CONTROL_SEQUENCE` is declared once in `user_equalizer.h` before
`EQ_PREPARED_BANK`; the control and response headers reuse it. This avoids a
core/control circular include while keeping one public token type.

The target TI ELF/EABI and the MSYS2 Host ABI both use a naturally aligned
32-bit `unsigned int`. `sequence` is the first mailbox member, so each sequence
access is one aligned 32-bit volatile load or store. The C89 typedef assertion
and Host/TI size checks are mandatory. Core generations remain the existing
`unsigned long` type and are not used as the seqlock word.

```c
typedef struct
{
    volatile EQ_CONTROL_SEQUENCE sequence;
    volatile int command;
    volatile int band;
    volatile int preset;
    volatile float value_db;
    volatile float step_db;
    volatile float shadow_gain_db[EQ_NUM_BANDS];
} EQ_CONTROL_MAILBOX;

typedef struct
{
    EQ_CONTROL_SEQUENCE sequence;
    int command;
    int band;
    int preset;
    float value_db;
    float step_db;
    float shadow_gain_db[EQ_NUM_BANDS];
} EQ_CONTROL_REQUEST;
```

All mailbox payload members are volatile. The reader copies them with separate
ordered volatile reads between the two sequence reads. This is compatible with
the TI C6000 C89 compiler and with asynchronous CCS Watch writes.

The mailbox has exactly one serialized writer at a time. CCS Watch and a future
touch producer must not write concurrently; a future touch implementation uses
one control-layer submit function that owns the complete odd/payload/even
sequence. The seqlock protects a reader against an in-progress writer, not two
writers against each other.

```c
EQ_CONTROL_SEQUENCE EqualizerControl_SubmitRequest(
    EQ_CONTROL_MAILBOX *mailbox,
    const EQ_CONTROL_REQUEST *request);
```

The submit helper ignores `request->sequence`, rejects a null request or an
already-odd mailbox by returning zero, performs one complete write transaction,
and returns the new nonzero even sequence. CCS Watch may perform the same three
steps manually; future board producers call only this helper.

### 5.2 Writer protocol

For the current completed even mailbox sequence `2N`, a writer performs:

1. write `sequence = 2N + 1`;
2. write command, band, preset, value, step, and all ten gains;
3. write `sequence = 2N + 2` last.

Sequence zero means no submitted command. A wrap skips zero and restarts at two.
The writer derives the next token only from the current completed mailbox
sequence, never from `accepted_sequence`, because a stable rejected request can
make the accepted token lag the mailbox. The writer must not complete an entire
32-bit sequence cycle between reader service opportunities; this bounded-rate
assumption prevents modulo aliasing after wrap.

The writer performs the odd sequence write before any payload write and the
even sequence write after every payload write. Each volatile access is a
separate full expression, and the submit helper preserves that source order in
TI C89 and Host builds. No C11 atomics, lock, or dynamic memory is introduced.

### 5.3 Reader protocol

The reader performs exactly one bounded copy attempt per service call:

```c
seq_before = mailbox->sequence;
if ((seq_before == 0UL) || ((seq_before & 1UL) != 0UL))
{
    return EQ_CONTROL_DEFERRED;
}

request.command = mailbox->command;
request.band = mailbox->band;
request.preset = mailbox->preset;
request.value_db = mailbox->value_db;
request.step_db = mailbox->step_db;
for (band = 0; band < EQ_NUM_BANDS; band++)
{
    request.shadow_gain_db[band] = mailbox->shadow_gain_db[band];
}

seq_after = mailbox->sequence;
if ((seq_before != seq_after) || ((seq_after & 1UL) != 0UL))
{
    return EQ_CONTROL_DEFERRED;
}
if (seq_after == control->observed_sequence)
{
    return EQ_CONTROL_DUPLICATE;
}
request.sequence = seq_after;
```

Validation and state changes occur only after the final equality/even checks.
An odd sequence or a sequence change during the payload copy never updates the
target, builder, accepted sequence, or active bank.

`observed_sequence` is the last stable even request that completed exactly one
validation attempt, whether accepted or rejected. After a stable copy:

- validation failure records `observed_sequence`, increments `rejected_count`
  once, leaves `accepted_sequence` unchanged, and never retries the unchanged
  invalid payload on later service calls;
- validation success records both `observed_sequence` and
  `accepted_sequence`, then applies the bounded command metadata changes;
- an odd or changing sequence records neither value.

Skipped-generation/coalescing diagnostics are calculated from successive
stable observed sequences, not from `accepted_sequence`, so a rejected request
does not cause later requests to be counted or processed repeatedly.

## 6. Commands and validation

The control layer supports:

```c
EQ_CONTROL_NONE
EQ_CONTROL_APPLY_PRESET
EQ_CONTROL_SET_BAND_ABS
EQ_CONTROL_STEP_BAND
EQ_CONTROL_SET_ALL
EQ_CONTROL_RESET_FLAT
EQ_CONTROL_COPY_ACTIVE_TO_SHADOW
EQ_CONTROL_CANCEL_PENDING
```

Validation rules:

- invalid band, preset, or command is rejected;
- a nonfinite command-relevant `value_db` or selected `shadow_gain_db` element
  is rejected; unused payload fields do not affect another command;
- finite gain values are clamped to `[-18, +12] dB`;
- a missing or nonfinite `step_db` uses the finite default `0.5 dB` only for
  `EQ_CONTROL_STEP_BAND`; nonfinite gain values are never clamped;
- duplicate completed sequences are not reapplied;
- a newer stable even sequence supersedes any stable request not yet accepted;
- `coalesced_count` records skipped stable generations without claiming an
  exact writer-event count across sequence wrap.

Command-to-shadow behavior is exact:

- `APPLY_PRESET` replaces all ten shadow gains with the unchanged fixed preset;
- `SET_BAND_ABS` clamps `value_db` and replaces one selected shadow gain;
- `STEP_BAND` adds the validated/default step to one shadow gain, then clamps;
- `SET_ALL` validates all ten submitted gains before atomically replacing the
  local shadow copy;
- `RESET_FLAT` sets all ten shadow gains to zero;
- `COPY_ACTIVE_TO_SHADOW` copies installed active gain metadata;
- `CANCEL_PENDING` changes no gain value.

Only after the complete local shadow result is valid does a target-bearing
command publish one logical target generation. No command applies a partially
updated ten-band array.

`COPY_ACTIVE_TO_SHADOW` copies the active gain metadata, not coefficients or
filter states. It changes only the edit shadow and does not change the immutable
target sequence/generation/gain tuple or cancel its builder.

`CANCEL_PENDING` cancels only the latest target-bearing request if that target
has not yet been installed. It invalidates `target_valid`, the matching builder,
and the matching ready candidate, while leaving the shadow gains and historical
sequence diagnostics unchanged. The dormant core logical metadata is never
serviced again unless a later target-bearing request publishes a new generation.
If the latest target is already represented by the installed-pair latch, the
command is a deterministic no-op: it never overwrites, reverses, or cancels a
bank already participating in a crossfade.

Commands are divided into two deterministic classes:

- target-bearing: `APPLY_PRESET`, `SET_BAND_ABS`, `STEP_BAND`, `SET_ALL`, and
  `RESET_FLAT`; after acceptance they publish one logical target generation and
  atomically replace `target_sequence`, `target_generation`,
  `target_gain_db[10]`, target preset, and target bank ID, then participate in
  prepared/installed/applied sequence tracking;
- administrative: `COPY_ACTIVE_TO_SHADOW` and `CANCEL_PENDING`; they update
  `observed_sequence` and `accepted_sequence` but allocate no generation and do
  not replace the immutable target tuple or change prepared/installed/applied
  sequences except that `CANCEL_PENDING` invalidates matching uninstalled work
  as specified above.

`EQ_CONTROL_NONE` with sequence zero means no request. A nonzero committed
`EQ_CONTROL_NONE` is consumed once as a validated no-op and allocates no
generation.

## 7. Token and generation model

The following identities are maintained:

- `request_sequence`: current stable even mailbox sequence;
- `observed_sequence`: latest stable even request validated or rejected once;
- `accepted_sequence`: latest fully copied and validated request;
- `target_sequence`: latest accepted target-bearing request paired with the
  current logical target generation and immutable gain snapshot;
- `prepared_sequence`: historical token of the most recent complete candidate
  publication, paired with `ready_candidate.valid` to determine whether that
  candidate is still current after cancellation or staleness;
- `installed_sequence`: retained latch for the most recently successfully
  installed request, kept through pending-to-active promotion until a later
  successful install or an explicit diagnostic rebase;
- `applied_sequence`: request whose generation has become the active bank after
  transition completion.

The core generation and control sequence are distinct values. Every prepared
candidate stores both:

```c
typedef struct
{
    EQ_FILTER_BANK bank;
    float gains_db[EQ_NUM_BANDS];
    int bank_id;
    int preset;
    unsigned long generation;
    EQ_CONTROL_SEQUENCE request_sequence;
    int valid;
} EQ_PREPARED_BANK;
```

`EQ_CONTROL_SEQUENCE` and `EQ_PREPARED_BANK` are declared in
`user_equalizer.h`, before the prepared-bank install prototype.
`EQ_BANK_BUILDER` is declared in
`user_equalizer_control.h`, before `EQ_CONTROL_STATE`, avoiding circular header
dependencies.

A prepared result can be installed only if its generation, request sequence,
gain snapshot, preset, and bank identity still match the immutable
`target_sequence`/`target_generation`/`target_gain_db` tuple. An administrative
accepted sequence is never substituted for the target sequence. Stale
candidates are discarded and never become pending or active.

For a target-bearing request, the control layer first calls
`Equalizer_PublishLogicalTarget`; the returned core generation and target
sequence are captured together in the builder or cached prepared candidate.
The control layer never writes `EQ_STATE.band_gain_db`, target/pending/active
generation fields, or either bank directly.

The control layer does not receive a callback from `EQ_ProcessRbjSample`.
After frame processing, at an audio-safe frame boundary, it compares the core's
`active_generation` with the retained installed-pair latch. It updates
`applied_sequence` only when:

- no transition is active;
- the active generation equals the installed generation; and
- the installed sequence/generation pair is valid.

It does not update `applied_sequence` when a bank is prepared, installed into
pending, or starts crossfading.

Validation responsibility is split deliberately. `EqualizerControl_TryInstallReady`
owns the request-token check against `target_sequence` and the full control
target tuple. `Equalizer_InstallPreparedBank` owns only core checks: no active
transition, matching core generation, gains, preset, and bank identity. The core
installer cannot and does not inspect control state. After a successful install,
the control layer writes `installed_sequence`, `installed_generation`, and
`installed_pair_valid = 1` as one bounded latch update. Promotion from pending
to active does not clear that latch; frame-boundary observation consumes it
logically, and the next successful install may replace it only after the
mandatory observation-first order in Section 10.1.

## 8. Builder state machine

```c
typedef enum
{
    EQ_BUILDER_IDLE = 0,
    EQ_BUILDER_DESIGN_SECTION,
    EQ_BUILDER_SCAN_HEADROOM,
    EQ_BUILDER_FINALIZE,
    EQ_BUILDER_READY,
    EQ_BUILDER_CANCELLED,
    EQ_BUILDER_ERROR
} EQ_BUILDER_STATE;
```

The builder captures the accepted target atomically into local nonvolatile
storage before calculation starts.

```c
typedef struct
{
    int state;
    int section_index;
    int scan_index;
    unsigned long generation;
    EQ_CONTROL_SEQUENCE request_sequence;
    float gains_db[EQ_NUM_BANDS];
    EQ_FILTER_BANK candidate;
    float peak_db;
    unsigned long payload_slice_count;
    unsigned long cancel_count;
    unsigned long restart_count;
    int last_error;
} EQ_BANK_BUILDER;
```

The final control state is declared only after `EQ_PREPARED_BANK` and
`EQ_BANK_BUILDER` are complete types:

```c
typedef struct
{
    float shadow_gain_db[EQ_NUM_BANDS];
    float target_gain_db[EQ_NUM_BANDS];
    EQ_CONTROL_SEQUENCE observed_sequence;
    EQ_CONTROL_SEQUENCE accepted_sequence;
    EQ_CONTROL_SEQUENCE target_sequence;
    EQ_CONTROL_SEQUENCE prepared_sequence;
    EQ_CONTROL_SEQUENCE installed_sequence;
    EQ_CONTROL_SEQUENCE applied_sequence;
    unsigned long target_generation;
    unsigned long installed_generation;
    int target_preset;
    int target_bank_id;
    int target_valid;
    int installed_pair_valid;
    EQ_BANK_BUILDER builder;
    EQ_PREPARED_BANK ready_candidate;
    unsigned long rejected_count;
    unsigned long coalesced_count;
    int last_error;
} EQ_CONTROL_STATE;
```

`shadow_gain_db` is the editable command base. `target_gain_db` is immutable for
the lifetime of `target_sequence` and is the only gain array used to validate a
builder, prepared candidate, or logical-target response snapshot. Administrative
commands may change the shadow or cancel uninstalled work but cannot silently
relabel the target tuple.

The builder candidate is private and incomplete until finalize succeeds. Only
finalize may publish a complete copy into `ready_candidate`, set its generation
and request sequence, and advance `prepared_sequence`. Fixed presets populate
the same `ready_candidate` through `Equalizer_CopyCachedPreparedBank` without
entering a builder state or incrementing any builder payload counter.

### 8.1 Exact payload-slice count

One complete non-flat custom build consists of exactly:

- 10 section-design payload slices, one section per call;
- 32 headroom payload slices, at most 16 of the 512 points per call;
- 1 finalize payload slice;
- 43 calculation payload slices total.

Initialization, request acceptance, cancellation, ready installation, and token
synchronization are bounded operations but do not count toward the 43 payload
slices.

`ServiceOneSlice` contains no loop that advances through multiple states. The
headroom state may contain one bounded loop whose upper bound is 16 points. A
call returns immediately after one section, at most 16 scan points, or one
finalize action.

Section slices call `Equalizer_DesignRbjSection`. Scan slices call
`Equalizer_EvaluateHeadroomPointDb` for the current indexed points. The finalize
slice calls `Equalizer_FinalizeRbjBank` and publishes `ready_candidate`; it does
not repeat any scan point.

The existing full-headroom-scan debug count advances exactly once when an
incremental 512-point scan completes. It does not advance per slice. Fixed
preset cache switches after prewarm do not advance it.

### 8.2 Conservative board policy

Before hardware cycle evidence exists:

- a transition-active state makes the builder ineligible;
- the builder resumes or restarts the latest generation after the transition;
- a ready candidate waits until no transition is active;
- only a later hardware-validated stage may allow builder work during a
  crossfade.

This policy is `PENDING_HARDWARE`; Host tests verify the control decision, not
C6748 timing sufficiency.

## 9. Deterministic transition and latest-wins semantics

If request A has already been installed as pending and is crossfading when B is
accepted:

1. A's pending bank is not overwritten.
2. Any uninstalled builder generation older than B becomes stale/cancelled.
3. B is retained as the latest logical target.
4. Builder payload work is deferred while A is transitioning.
5. A completes deterministically and becomes active.
6. At the following safe frame boundary, A's applied sequence is observed.
7. The builder starts or resumes only the latest valid target B.
8. B can enter pending only after it is complete and A's transition has ended.

Thus latest-wins applies to requests that have not entered pending. An already
started crossfade completes before the latest target is installed.

## 10. Shared background-service budget

The board flow maintains:

```c
typedef struct
{
    unsigned long consumed_frame;
    int consumed_frame_valid;
    int consumed_kind;
    int next_preference;
} EQ_BACKGROUND_SERVICE_STATE;
```

```c
#define EQ_BACKGROUND_NONE    0
#define EQ_BACKGROUND_BUILDER 1
#define EQ_BACKGROUND_LCD     2

int EqualizerBackgroundService_Decide(
    EQ_BACKGROUND_SERVICE_STATE *state,
    unsigned long processed_frame,
    int builder_eligible,
    int lcd_eligible);
void EqualizerBackgroundService_Record(
    EQ_BACKGROUND_SERVICE_STATE *state,
    unsigned long processed_frame,
    int completed_kind);
```

`Decide` is pure bounded metadata selection and does not call either workload.
`Record` accepts only a completed builder slice or completed LCD job and updates
the frame latch and alternating preference.

Heavy service eligibility requires all of:

```text
FLAG_AD == 0
FLAG_DA == 0
flag_ad_done == 0
EQ_FrameServicePending == 0
EQ_DebugProcessFrames != consumed_frame
```

Scheduling rules:

- `consumed_frame_valid` starts false, so frame zero is not accidentally
  treated as already consumed;
- if only the LCD has work, service one LCD job;
- if only the builder is eligible, service one builder slice;
- if both are eligible, alternate preference to prevent starvation;
- if a transition is active, the builder is ineligible and the LCD may use the
  frame's budget;
- a completed LCD job or builder payload slice records `consumed_frame`;
- no loop iteration can consume the budget twice for the same processed frame.

One decision function returns exactly one of `EQ_BACKGROUND_NONE`,
`EQ_BACKGROUND_BUILDER`, or `EQ_BACKGROUND_LCD`. The outer loop calls only the
selected service and never falls through to the other service in that frame,
even if the selected service reports no payload. No builder or LCD heavy-service
call site bypasses this decision function.

The existing LCD request generation and `EqualizerDisplay_ServiceOneJob`
implementation are unchanged; only its existing outer-loop call site is routed
through the shared decision.

### 10.1 Mandatory safe-boundary order

At each eligible post-frame safe boundary, metadata and payload service occur in
this exact order:

1. `EqualizerControl_ObserveFrameBoundary` records completion of the previously
   installed pair before any latch can be replaced;
2. `EqualizerControl_ServiceMailbox` performs one seqlock attempt and accepts
   the latest stable request;
3. the control layer cancels or discards builder/ready work that no longer
   matches the immutable target tuple;
4. `EqualizerControl_TryInstallReady` validates and, if eligible, installs only
   the current ready tuple;
5. the shared scheduler decides at most one heavy payload, then executes either
   one builder slice or one LCD job and records the frame only if work completed.

A builder finalize at step 5 is not installed by looping back in the same
boundary; it waits for step 4 of the next boundary. A direct RAW_COPY,
FLOAT_COPY, or hard-bypass diagnostic change is a mutually exclusive branch:
after step 1 the flow applies that direct path change, calls
`EqualizerControl_RebaseAfterDirectPathChange`, and skips steps 2 through 5 for
that boundary.

This order prevents both identified races: installing ready B cannot erase the
installed-pair latch before A is observed applied, and a same-boundary C request
invalidates stale ready B before B can enter pending.

## 11. Preset-cache prewarm contract

All five fixed presets and the single-1-kHz diagnostic bank are built before
`Adc_Start` and `Dac_Start`. The contract is:

```text
Equalizer_Init
  -> EQ_EnsureRbjBankCache
  -> all fixed cache entries ready
  -> board mode initialization
  -> Adc_Start / Dac_Start
```

The core exposes a read-only cache-ready query for Host and static contract
tests. After the startup scan count is captured, switching through every fixed
preset must:

- leave `EQ_DebugHeadroomScanCount` unchanged;
- leave builder payload/restart counts unchanged;
- use the existing 120 ms crossfade.

## 12. Response snapshots and path semantics

### 12.1 Path classification

Every current-path snapshot identifies at least one of:

```c
EQ_RESPONSE_PATH_IDENTITY_RAW_COPY
EQ_RESPONSE_PATH_IDENTITY_FLOAT_COPY
EQ_RESPONSE_PATH_IDENTITY_HARD_BYPASS
EQ_RESPONSE_PATH_IDENTITY_RBJ_FLAT_COPY
EQ_RESPONSE_PATH_DRY_TO_BANK_TRANSITION
EQ_RESPONSE_PATH_ACTIVE_BANK
EQ_RESPONSE_PATH_BANK_TO_BANK_TRANSITION
EQ_RESPONSE_PATH_UNSUPPORTED_LEGACY
```

The snapshot role is one of:

```c
EQ_RESPONSE_ROLE_ACTIVE
EQ_RESPONSE_ROLE_PENDING
EQ_RESPONSE_ROLE_PREPARED_TARGET
EQ_RESPONSE_ROLE_LOGICAL_TARGET
```

Classification precedence is deterministic: hard bypass first, then explicit
RAW_COPY/FLOAT_COPY core modes, then RBJ transition/stable state. A stable flat
RBJ configuration is classified as `IDENTITY_RBJ_FLAT_COPY` because the current
core executes its transparent path instead of processing the identity bank.
The retained legacy parallel core is identified explicitly but is outside the
new RBJ snapshot response engine; its RBJ coefficient snapshot has `valid = 0`
and existing legacy APIs remain authoritative.

### 12.2 Immutable coefficient snapshot

```c
typedef struct
{
    double real;
    double imag;
    int valid;
} EQ_RESPONSE_COMPLEX;

typedef struct
{
    EQ_BIQUAD section[EQ_NUM_BANDS];
    float gain_db[EQ_NUM_BANDS];
    float predicted_peak_db;
    float preamp_db;
    float preamp_gain;
    int path_type;
    int role;
    int bank_id;
    int preset;
    unsigned long generation;
    int valid;
    int identity;
    int transition_active;
    float transition_progress;
} EQ_RESPONSE_SNAPSHOT;
```

It never contains `s1` or `s2`.

`EQ_RESPONSE_COMPLEX` is the one public complex type used by
`EqualizerResponse_GetSectionComplex` and the canonical cascade helpers. The
old private `EQ_COMPLEX` implementation is removed or made an exact alias; core
and response code do not maintain two incompatible complex representations.

### 12.3 Correct identity behavior

- RAW_COPY: active response is 0 dB identity.
- FLOAT_COPY: active response is 0 dB identity.
- hard bypass: active response is 0 dB identity.
- stable flat RBJ transparent copy: active response is 0 dB identity.
- dry-to-bank: the active/dry side is identity and the pending side is the bank.
- bank-to-bank: active and pending banks are reported separately.
- stable RBJ: active response is the active bank.

No API labels the pending bank as the current audible response. During either
transition the report contains the two endpoint responses and transition
progress, not a fabricated single LTI response.

### 12.4 Command target versus actual target

The latest accepted target-bearing ten-band command is copied from the immutable
target tuple to a separate command snapshot. Its 129-point desired visual curve
is always available on Host.

```c
typedef struct
{
    float gain_db[EQ_NUM_BANDS];
    EQ_CONTROL_SEQUENCE request_sequence;
    unsigned long generation;
    int preset;
    int bank_id;
    int valid;
    int target_response_valid;
} EQ_COMMAND_SNAPSHOT;
```

This snapshot is copied from `target_gain_db`, `target_sequence`, and the paired
target metadata, not from mutable `shadow_gain_db` or the possibly newer
administrative `accepted_sequence`.

If the corresponding coefficients are not prepared, the coefficient target
snapshot is invalid and reports `target_response_valid = 0`. A partial builder
candidate is never published as an actual response.

The target convenience API resolves the immutable target-bearing tuple without
guessing from one mutable bank or a newer administrative accepted sequence:

```c
int EqualizerResponse_CopyTarget(const EQ_STATE *st,
                                 const EQ_CONTROL_STATE *control,
                                 EQ_RESPONSE_SNAPSHOT *out);
```

It returns the matching pending snapshot if that generation is installed, the
matching complete prepared snapshot if it is ready but uninstalled, or an
invalid coefficient snapshot carrying target metadata if the builder is still
incomplete. The separate command snapshot always remains available for desired
curve generation.

## 13. Response calculations

The Host response grid contains 129 logarithmically spaced points from 20 Hz to
20 kHz.

The desired visual curve is piecewise linear in `log2(frequency)` through the
ten control gains. It uses the first gain below the first center and the last
gain above the last center. It is a visualization of the command, not a
guaranteed physical transfer function.

For an immutable coefficient snapshot:

```text
actual_shape_db = 20 log10(|product of the ten section responses|)
actual_total_db = actual_shape_db + preamp_db
```

Shelf reference points are 20 Hz and 20 kHz. The eight peaking references use
their center frequencies.

Metrics include separate shape and total-response errors, center/plateau error,
peak/minimum response, preamp, maximum valid-region group delay, transition
state, and active/target generations.

The response module exposes pointwise section/cascade evaluation and immutable
data types only. The Host report tool owns the 129-point loop. No board API in
this stage performs a 129-point batch calculation, and no 129-point buffer or
10 x 10 interaction matrix is added to `EQ_STATE` or board globals.

The response module is not called by `Equalizer_ProcessFrame`. The current
board flow does not calculate a 129-point curve and does not connect curve data
to the LCD. Board incremental curve calculation is reserved for the stage after
LCD hardware stability passes and is marked
`NEXT_STAGE_AFTER_LCD_HARDWARE_PASS`.

## 14. Section response and interaction matrix

The canonical RBJ section-response API evaluates the coefficients contained in
an immutable snapshot. It does not call `Equalizer_GetBandMagnitudeDb`.

The 10 x 10 interaction matrix starts from FLAT. For row `i`, only band `i` is
set to +1 dB. Column `j` is the shape-response change at band `j`'s reference
frequency. The implementation reports diagonal mean, maximum absolute
off-diagonal response, off-diagonal RMS, and interaction ratio. It never
modifies the source `EQ_STATE` and does not compensate the gains.

## 15. Independent Python reference

The Python implementation independently calculates:

- RBJ peaking, low-shelf, and high-shelf coefficients;
- section and cascade complex response;
- preamp application;
- phase unwrap and group delay;
- desired log-frequency interpolation;
- the interaction matrix.

It does not call C response functions or reuse C-computed response values.
Comparison uses 512 logarithmic points from 20 Hz to 20 kHz. Magnitude maximum
error must be below 0.01 dB. Group delay is compared only from 30 Hz to 18 kHz
where magnitude is above -60 dB. Singular/invalid points are marked invalid and
excluded from aggregate pass/fail instead of being replaced by zero.

The Host report generator produces exactly these ignored local plots under
`docs/eval_outputs/equalizer_control_response/`:

```text
target_vs_actual_flat.png
target_vs_actual_bass.png
target_vs_actual_vocal.png
target_vs_actual_treble.png
target_vs_actual_v_shape.png
target_vs_actual_custom.png
rbj_individual_sections.png
interaction_matrix_heatmap.png
group_delay_comparison.png
```

Every plot records the input configuration, logarithmic Hz axis, dB or sample
axis, command/desired curve where applicable, actual shape, actual total,
preamp, active/target state, and commit SHA. Generated PNG/CSV data is never
committed.

## 16. Complexity, diagnostics, and real-time constraints

### 16.1 Complexity and storage evidence

The final implementation records complexity/storage evidence in this Markdown
file because the path whitelist does not permit a second complexity report.
The evidence distinguishes formulas from measured board data:

- stable RBJ hot path: ten DF2T sections, five multiplies and four
  additions/subtractions per section per sample, plus one preamp multiply and
  existing short saturation/conversion;
- bank crossfade: active and pending banks are both processed plus the existing
  mix; builder payload is prohibited during this interval;
- builder: ten section designs, 512 headroom points in 32 slices of at most 16,
  and one finalize slice, for 43 payload slices;
- response: the 129-point curve and 10 x 10 interaction matrix are Host-owned
  arrays and consume no permanent board-state storage in this stage;
- storage table: Host `sizeof` values are informational, while TI C6000
  `sizeof`/map values for `EQ_STATE`, mailbox, control state, builder candidate,
  prepared bank, and response snapshot are the compile-time board evidence;
- no Host wall-clock value is converted into a C6748 cycle claim.

Actual builder cycles, crossfade headroom, and low-priority timing safety remain
`PENDING_HARDWARE`.

### 16.2 Watch diagnostics

The board build exports these stable Watch variables; each `Token` value is the
corresponding stable even seqlock sequence:

```text
EQ_DebugControlCommand
EQ_DebugControlBand
EQ_DebugControlPreset
EQ_DebugControlValueDb
EQ_DebugControlStepDb
EQ_DebugControlShadowGainDb[10]
EQ_DebugControlRequestToken
EQ_DebugControlObservedToken
EQ_DebugControlAcceptedToken
EQ_DebugControlTargetToken
EQ_DebugControlPreparedToken
EQ_DebugControlReadyValid
EQ_DebugControlInstalledToken
EQ_DebugControlAppliedToken
EQ_DebugControlInstalledPairValid
EQ_DebugControlRejectedCount
EQ_DebugControlCoalescedCount
EQ_DebugControlLastError
EQ_DebugBuilderState
EQ_DebugBuilderSectionIndex
EQ_DebugBuilderScanIndex
EQ_DebugBuilderGeneration
EQ_DebugBuilderSliceCount
EQ_DebugBuilderCancelCount
EQ_DebugBuilderRestartCount
EQ_DebugResponseActiveGeneration
EQ_DebugResponseTargetGeneration
EQ_DebugResponseTransitionActive
```

No diagnostic adds LCD text, curve drawing, touch input, file I/O, or hot-path
formatting.

### 16.3 C89 and real-time constraints

Board-visible code must compile with the TI C6000 C89 mode:

- declarations precede statements in each block;
- no variable-length arrays, compound literals, designated initializers,
  `for (int ...)`, `//` line comments, or C99-only library use;
- no dynamic allocation or locks;
- the seqlock word is the asserted 32-bit `EQ_CONTROL_SEQUENCE`, first and
  naturally aligned in the mailbox;
- finite checks use `<float.h>` `FLT_MAX`, not an artificial decimal threshold;
- no `sin`, `cos`, `log`, `exp`, `pow`, `printf`, or `fopen` in sample/frame
  processing;
- no builder or response computation in `Equalizer_ProcessFrame`;
- one builder slice is bounded as specified above.

Host-only Python and C test code may use their normal language features, but the
new C modules must also pass GCC C99 with `-Wall -Wextra -Werror`.

## 17. File whitelist

### Create

- `Code/User/user_dsp/user_equalizer_control.c`
- `Code/User/user_dsp/user_equalizer_control.h`
- `Code/User/user_dsp/user_equalizer_response.c`
- `Code/User/user_dsp/user_equalizer_response.h`
- `tools/equalizer_33_response_report.py`
- `tools/tests/equalizer_control_test.c`
- `tools/tests/equalizer_response_test.c`
- `tools/tests/test_equalizer_control.py`
- `tools/tests/test_equalizer_response.py`
- `docs/equalizer_33_control_response.md`

### Minimal modification

- `Code/User/user_dsp/user_equalizer.c`
- `Code/User/user_dsp/user_equalizer.h`
- `Code/User/user_dsp/user_equalizer_flow.c`
- `Code/User/user_dsp/user_equalizer_flow.h`
- `Code/User/user_dsp/user_equalizer_eval.c`
- `Code/User/user_dsp/user_equalizer_eval.h`
- `Code/User/user_include.h`
- `.gitignore`

Generated `Debug/` build files may be refreshed locally for CCS verification
but are not committed. Project metadata is changed only if CCS proves that the
new source files cannot otherwise enter the managed build; such a need is a
stop-and-report condition before committing any project metadata.

### Never modify or commit

- `outputs/`, PPTX, or inspect NDJSON files;
- `user_equalizer_display.c/.h`;
- any `user_subband_*` file;
- Touch code;
- `Code/main.c` or its default Project 32 selection;
- ADC/DAC/EDMA/PRU/interrupt drivers;
- DSS outputs, WAV, RAW, PNG, large CSV, `__pycache__`, or `.pyc` files.

The unrelated untracked `projects/` directory is outside Stage B and remains
untouched and unstaged.

## 18. Acceptance test matrix

### 18.1 Mailbox and control

- Odd sequence defers without state change.
- Stable even sequence is accepted exactly once.
- Sequence changing during payload copy defers and cannot mix payloads.
- Duplicate sequence is ignored.
- One stable invalid sequence is rejected exactly once and then treated as
  already observed without changing `accepted_sequence`.
- Duplicate detection compares only `observed_sequence`; an invalid token
  followed by the next valid token is accepted normally.
- The single-writer contract is documented and all board submissions use the
  odd/payload/even submit helper.
- Submit-helper wrap skips zero, derives from the completed mailbox token rather
  than `accepted_sequence`, and leaves an already-odd mailbox unchanged.
- Host and TI checks prove `EQ_CONTROL_SEQUENCE` is 32 bits and the sequence is
  the aligned first mailbox member.
- Invalid band/preset/command and NaN/Inf cannot damage active state.
- A/B/C submissions accept the latest stable request deterministically.
- Shadow gains without an even commit cannot change the target.
- `COPY_ACTIVE_TO_SHADOW` during a build changes the edit shadow but not the
  target sequence/generation/gains or builder candidate.
- `CANCEL_PENDING` invalidates only matching uninstalled work and cannot cancel
  an already-installed crossfade bank.

### 18.2 Builder

- Exactly ten section payload slices, thirty-two scan payload slices, and one
  finalize payload slice for a non-flat custom build.
- One call processes at most one section or sixteen scan points.
- No call completes the full builder through an internal state loop.
- Publishing a logical custom target performs zero section designs and zero
  headroom points.
- Fixed cached preparation copies a ready bank and performs zero builder
  payload slices.
- A newer request invalidates an uninstalled older generation.
- Builder work is deferred while a transition is active.
- Stale candidates cannot enter pending or active.
- Incremental and synchronous reference coefficients differ by less than
  `1e-6`; peak/preamp differ by less than `1e-4 dB`; response differs by less
  than `0.01 dB`.

### 18.3 Preset cache and background budget

- Cache is ready before audio start.
- All fixed preset switches after the startup marker add zero full scans.
- Fixed presets start zero custom builder slices.
- One processed frame completes at most one builder slice or one LCD job.
- When both are pending, neither service starves.
- The safe-boundary call order is observe, mailbox, stale invalidation, install,
  then at most one scheduled heavy payload.

### 18.4 Tokens and transitions

- `prepared_sequence` updates only for a complete candidate/cache entry.
- Cancellation/staleness clears `ready_candidate.valid`; the historical
  `prepared_sequence` alone never authorizes installation.
- Administrative acceptance cannot relabel the immutable target tuple or a
  prepared candidate.
- The core installer validates core metadata only; the control installer
  validates `request_sequence == target_sequence` before calling it.
- `applied_sequence` remains unchanged at install and crossfade start.
- `applied_sequence` changes only at a safe frame boundary after the matching
  generation is active and transition is inactive.
- A pending A transition completes before a later B candidate is installed.
- If A completes while cached B is ready and C is committed at the same safe
  boundary, A is first recorded applied, C is accepted, stale B is discarded,
  and B never enters pending.
- After A completes while B is still unprepared, the audio path continues to
  use installed active A; it never switches to identity or B merely because the
  logical command gains changed.

### 18.5 Response

- RAW_COPY, FLOAT_COPY, hard bypass, and dry side report identity.
- Stable flat RBJ transparent copy reports identity rather than an old bank.
- Active and pending banks remain distinct during bank-to-bank transition.
- Snapshots do not copy or mutate `s1/s2`, clipping, or transition state.
- Desired endpoints and log-frequency interpolation are correct.
- FLAT shape and total responses are finite and near 0 dB.
- Section responses and the 10 x 10 interaction matrix are finite and do not
  mutate source state.
- No board-flow call calculates a 129-point curve or sends it to LCD.
- The nine required Host plot names are generated under the ignored output
  directory and none is staged.

### 18.6 Regression and build

- Existing Host evaluator returns zero failures.
- Existing transition-interrupt evaluator cases use the mailbox/control helper;
  post-start board source contains no direct RBJ target setter call.
- Existing Python flow/LCD contracts remain green.
- Independent C/Python response comparison passes.
- Project 32, Project 33 LCD OFF, and Project 33 LCD ON compile and link.
- `Debug/DSP_LAB_0507.out` exists and link XML reports zero errors.
- TI C6000 C89 compilation succeeds for all new board modules.
- Host and TI compile-time size/map evidence is recorded without claiming
  measured C6748 cycles.
- All board timing, audio, LCD, and touch conclusions remain
  `PENDING_HARDWARE`.

---

# Project 3.3 Stage B Control and Response Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a seqlock-controlled ten-band command backend, a bounded custom-bank builder, deterministic sequence/generation state, and immutable Host response evidence without changing the existing RBJ mathematics, display renderer, or LCD request/job implementation.

**Architecture:** The equalizer core remains authoritative for coefficient design, cache, installation, and audio processing. A new control module owns mailbox and builder state; a new response module owns immutable snapshots and frequency-domain calculations. The board flow supplies one shared heavy-background budget per processed frame.

**Tech Stack:** TI C6000 C89, Host GCC C99, Python `unittest`, NumPy/Matplotlib when available, CCS 20.5 managed build.

**Execution status:** `NOT_STARTED`. This document is the implementation gate.

Before Task 1, start from the committed design gate:

```bash
git switch codex/project33-control-response
git merge --ff-only main
```

If that fast-forward is unavailable, stop and inspect branch ancestry instead
of rebasing or rewriting either branch.

## File responsibility map

- `user_equalizer_control.h/.c`: seqlock, commands, builder, sequence mapping.
- `user_equalizer_response.h/.c`: path-aware snapshots and response math.
- `user_equalizer.h/.c`: shared design primitives, cache query, prepared install,
  generation getters, removal of synchronous custom build from the sample path.
- `user_equalizer_flow.h/.c`: shared builder/LCD budget and frame-boundary token
  observation.
- `user_equalizer_eval.h/.c`: existing regression integration and CSV exports.
- `equalizer_control_test.c`: executable control/builder behavioral tests.
- `equalizer_response_test.c`: executable snapshot/response behavioral tests.
- `test_equalizer_control.py`: source contracts and MSYS2 harness orchestration.
- `test_equalizer_response.py`: Python independent reference and artifact checks.
- `equalizer_33_response_report.py`: offline CSV/PNG generation.

### Task 1: Establish failing Stage B control contracts

**Files:**
- Create: `tools/tests/equalizer_control_test.c`
- Create: `tools/tests/test_equalizer_control.py`
- Test: `Code/User/user_dsp/user_equalizer.c`
- Test: `Code/User/user_dsp/user_equalizer_flow.c`

- [ ] **Step 1: Add the C test's required API surface**

Create a harness that includes `user_equalizer_control.h` and asserts these
initial behaviors:

```c
static void test_odd_sequence_defers(void)
{
    EQ_CONTROL_MAILBOX mailbox;
    EQ_CONTROL_STATE control;
    EQ_STATE equalizer;

    memset(&mailbox, 0, sizeof(mailbox));
    Equalizer_Init(&equalizer);
    EqualizerControl_Init(&control, &equalizer);
    mailbox.sequence = 1U;
    mailbox.command = EQ_CONTROL_RESET_FLAT;
    assert(EqualizerControl_ServiceMailbox(&control, &mailbox, &equalizer) ==
           EQ_CONTROL_DEFERRED);
    assert(control.accepted_sequence == 0U);
}
```

Add test functions for stable-even acceptance, duplicate sequence, sequence
mutation during copy, invalid finite checks, and one-time handling of a stable
invalid sequence. Test the submit helper's odd/payload/even result, odd-mailbox
rejection, invalid-then-valid next-token behavior, duplicate comparison against
`observed_sequence` only, wrap-to-two behavior, and the 32-bit/aligned sequence
ABI contract. Builder and cache tests are added only in their later RED tasks.
The harness prints exactly:

```text
equalizer control tests: PASS
```

and returns zero only when every assertion passes.

- [ ] **Step 2: Add the Python source-contract test**

The Python test runs the C harness through MSYS2 and statically checks:

```python
def test_audio_path_has_no_builder_or_response_service(self):
    body = extract_function(CORE_SOURCE, "Equalizer_ProcessFrame")
    self.assertNotIn("Builder", body)
    self.assertNotIn("Response", body)
    self.assertNotIn("Control", body)

def test_hot_path_has_no_forbidden_calls(self):
    forbidden = ("sin", "cos", "log", "exp", "pow", "malloc",
                 "printf", "fopen")
    for function_name in HOT_PATH_FUNCTIONS:
        body = extract_function(CORE_SOURCE, function_name)
        for call in forbidden:
            self.assertNotRegex(body, rf"\b{call}f?\s*\(")

def test_board_sources_use_c89_declaration_style(self):
    for source in NEW_BOARD_SOURCES:
        text = source.read_text(encoding="utf-8")
        self.assertNotRegex(text, r"for\s*\(\s*int\s+")
        self.assertNotRegex(text, r"(^|\s)//")
```

`HOT_PATH_FUNCTIONS` contains `Equalizer_ProcessFrame`,
`Equalizer_ProcessFrameFloat`, `EQ_ProcessRbjSample`, and
`EQ_ProcessRbjBank`. Task 1 initializes `NEW_BOARD_SOURCES` with the control
module; Task 6 extends it with the response module when that file is introduced.
The TI `--c89` build remains the decisive syntax check.

- [ ] **Step 3: Run RED verification**

Run:

```powershell
& 'D:\SoftwareDownload\python.exe' -B -m unittest tools.tests.test_equalizer_control
```

Expected: FAIL because `user_equalizer_control.h/.c` and their APIs do not yet
exist. Confirm the failure is missing Stage B behavior, not a Python syntax or
path error.

- [ ] **Step 4: Commit the failing contracts**

```bash
git add tools/tests/equalizer_control_test.c tools/tests/test_equalizer_control.py
git commit -m "test: define project 3.3 control contracts"
```

### Task 2: Implement the seqlock mailbox and validated commands

**Files:**
- Create: `Code/User/user_dsp/user_equalizer_control.h`
- Create: `Code/User/user_dsp/user_equalizer_control.c`
- Modify: `Code/User/user_dsp/user_equalizer.h`
- Modify: `Code/User/user_dsp/user_equalizer.c`
- Modify: `Code/User/user_include.h`
- Test: `tools/tests/equalizer_control_test.c`

- [ ] **Step 1: Define C89-compatible control types and results**

Add `EQ_CONTROL_SEQUENCE` and its 32-bit typedef assertion to
`user_equalizer.h` before `EQ_PREPARED_BANK`, then add the exact mailbox/request
types from Section 5 plus:

```c
#define EQ_CONTROL_ACCEPTED 1
#define EQ_CONTROL_NO_REQUEST 0
#define EQ_CONTROL_DEFERRED (-1)
#define EQ_CONTROL_DUPLICATE (-2)
#define EQ_CONTROL_REJECTED (-3)

typedef struct
{
    float shadow_gain_db[EQ_NUM_BANDS];
    float target_gain_db[EQ_NUM_BANDS];
    EQ_CONTROL_SEQUENCE observed_sequence;
    EQ_CONTROL_SEQUENCE accepted_sequence;
    EQ_CONTROL_SEQUENCE target_sequence;
    EQ_CONTROL_SEQUENCE prepared_sequence;
    EQ_CONTROL_SEQUENCE installed_sequence;
    EQ_CONTROL_SEQUENCE applied_sequence;
    unsigned long target_generation;
    int target_preset;
    int target_bank_id;
    int target_valid;
    int installed_pair_valid;
    unsigned long rejected_count;
    unsigned long coalesced_count;
    int last_error;
} EQ_CONTROL_STATE;
```

Task 3 extends this structure only after `EQ_PREPARED_BANK` and
`EQ_BANK_BUILDER` have been declared in dependency order.

Declare initialization, mailbox service, builder eligibility/service, ready
installation, frame-boundary observation, and
`EqualizerControl_SubmitRequest` functions.

- [ ] **Step 2: Implement one-attempt seqlock copying**

Implement the exact reader algorithm in Section 5.3. Use one bounded ten-item
copy loop and no retry loop. Add an `EQ_CONTROL_TEST_HOOK` callback only under
`EQ_ALGO_ONLY` so the C harness can change the sequence between payload copy and
the second sequence read.

- [ ] **Step 3: Implement validation and command-to-shadow behavior**

Include C89 `<float.h>` and use explicit helpers:

```c
static int EQ_ControlIsFinite(float value)
{
    return ((value == value) &&
            (value <= FLT_MAX) && (value >= -FLT_MAX));
}

static float EQ_ControlClampDb(float value)
{
    if (value < EQ_GAIN_MIN_DB) return EQ_GAIN_MIN_DB;
    if (value > EQ_GAIN_MAX_DB) return EQ_GAIN_MAX_DB;
    return value;
}
```

Only after validation succeeds, publish `accepted_sequence` and update the
logical command shadow. After any stable validation attempt, publish
`observed_sequence`. Invalid requests increment `rejected_count` exactly once,
leave `accepted_sequence` unchanged, and do not change active, pending,
prepared, builder, or applied state.

Target-bearing commands copy the complete local result into both the edit shadow
and immutable target tuple, then pair the returned generation with
`target_sequence`. Administrative commands update `accepted_sequence` without
relabeling that tuple. Implement the exact `COPY_ACTIVE_TO_SHADOW` and
`CANCEL_PENDING` semantics from Section 6.

- [ ] **Step 4: Add the bounded core logical-target publisher**

Declare `EQ_PREPARED_BANK` before the future prepared-install prototypes and
implement `Equalizer_CopyPresetGainsDb` and
`Equalizer_PublishLogicalTarget` exactly as Section 4.2 specifies. A valid
target-bearing command calls the publisher once after mailbox validation.
Assert that accepting a custom target changes the logical generation but
produces zero headroom-scan delta, no bank installation, and no transition
start.

In the same bounded core change, remove the transition-completion call that
starts an unprepared target, and make transparent-path selection use installed
active metadata rather than the latest logical command. This is required before
the mailbox can safely publish B while A is crossfading. The audio path may
promote an already-installed pending bank, but it may not design or install the
next logical target.

Make direct RBJ target setters reject without state change while a transition is
active. Retain them only as no-transition Host/pre-start settled references;
later tasks migrate all board target-bearing control and transition-interrupt
evaluator cases to the mailbox helper.

- [ ] **Step 5: Run GREEN verification**

Run the Python test from Task 1. Expected: all mailbox acceptance, odd/even,
mutation, duplicate, validation, and one-time rejection tests pass with zero
warnings. No builder test exists yet.

- [ ] **Step 6: Commit the mailbox implementation**

```bash
git add Code/User/user_dsp/user_equalizer_control.h Code/User/user_dsp/user_equalizer_control.c Code/User/user_dsp/user_equalizer.h Code/User/user_dsp/user_equalizer.c Code/User/user_include.h tools/tests/equalizer_control_test.c
git commit -m "feat: add seqlock equalizer control mailbox"
```

### Task 3: Extract reusable core design primitives and implement the builder

**Files:**
- Modify: `Code/User/user_dsp/user_equalizer.h`
- Modify: `Code/User/user_dsp/user_equalizer.c`
- Modify: `Code/User/user_dsp/user_equalizer_control.h`
- Modify: `Code/User/user_dsp/user_equalizer_control.c`
- Test: `tools/tests/equalizer_control_test.c`

- [ ] **Step 1: Extend RED tests for exact slice accounting**

For a non-flat custom target, assert after every call:

```c
assert(section_delta <= 1);
assert(scan_delta <= 16);
assert(builder.payload_slice_count == expected_payload_slices);
```

After completion assert ten design slices, thirty-two scan slices, one finalize
slice, forty-three total, and exactly one completed full-headroom-scan count
increment from the incremental builder.

- [ ] **Step 2: Run RED verification**

Expected: FAIL because the builder states and per-slice primitives are missing.

- [ ] **Step 3: Expose the bounded logical-target, cache, and design primitives**

Promote the existing bank IDs needed by the prepared API without changing their
numeric values. Expose `#define EQ_HEADROOM_POINT_COUNT 512` in
`user_equalizer.h`, replace the private core count with that symbol, and add the
remaining exact APIs from Section 4.2:

```c
int Equalizer_CopyCachedPreparedBank(
    int bank_id,
    int preset,
    unsigned long generation,
    EQ_CONTROL_SEQUENCE request_sequence,
    EQ_PREPARED_BANK *out);
int Equalizer_DesignRbjSection(EQ_BIQUAD *section_out,
                               int section,
                               float gain_db);
int Equalizer_EvaluateHeadroomPointDb(const EQ_FILTER_BANK *bank,
                                      int point_index,
                                      float *magnitude_db);
void Equalizer_FinalizeRbjBank(EQ_FILTER_BANK *bank,
                               int gains_are_flat,
                               float peak_db);
int Equalizer_IsPresetCacheReady(void);
```

`Equalizer_CopyCachedPreparedBank` must fail before cache ready and must never
call `EQ_BuildRbjBank`.

Declare `EQ_BANK_BUILDER` before `EQ_CONTROL_STATE` in
`user_equalizer_control.h`, use `EQ_FILTER_BANK candidate` inside the builder,
then extend the control state with:

```c
EQ_BANK_BUILDER builder;
EQ_PREPARED_BANK ready_candidate;
unsigned long installed_generation;
```

Refactor `EQ_BuildRbjBank` to call these primitives synchronously so Host
reference and incremental builder share one formula implementation.

- [ ] **Step 4: Implement one builder payload per call**

`EqualizerControl_ServiceOneBuilderSlice` uses a `switch` and returns from each
case. The headroom case calculates:

```c
points = EQ_HEADROOM_POINT_COUNT - builder->scan_index;
if (points > 16) points = 16;
for (index = 0; index < points; index++)
{
    /* call Equalizer_EvaluateHeadroomPointDb for this indexed point */
}
builder->scan_index += points;
builder->payload_slice_count++;
return EQ_BUILDER_WORKED;
```

It does not use a `while` loop and does not fall through to another state.
Completing point 512 advances `EQ_DebugHeadroomScanCount` once; earlier scan
slices do not change that counter.

The finalize slice calls `Equalizer_FinalizeRbjBank`, copies the complete result
to `ready_candidate`, and only then updates `prepared_sequence`. A partial
builder candidate is never visible through response APIs.

- [ ] **Step 5: Verify synchronous/incremental equivalence**

The C harness compares all 50 coefficients, predicted peak, preamp, pole radii,
and 512-point response. Expected limits are those in Section 18.2.

- [ ] **Step 6: Run GREEN verification and commit**

```bash
git add Code/User/user_dsp/user_equalizer.h Code/User/user_dsp/user_equalizer.c Code/User/user_dsp/user_equalizer_control.h Code/User/user_dsp/user_equalizer_control.c tools/tests/equalizer_control_test.c
git commit -m "feat: add bounded custom equalizer bank builder"
```

### Task 4: Add stale-safe prepared installation and applied observation

**Files:**
- Modify: `Code/User/user_dsp/user_equalizer.h`
- Modify: `Code/User/user_dsp/user_equalizer.c`
- Modify: `Code/User/user_dsp/user_equalizer_control.c`
- Test: `tools/tests/equalizer_control_test.c`

- [ ] **Step 1: Add failing A-pending/B-latest and token tests**

The harness must prove:

```text
prepare A -> install A pending -> accept B -> A remains pending
complete A transition -> observe applied A at frame boundary
build latest B -> install B pending -> applied remains A
complete B transition -> observe applied B
```

It also attempts to install an old generation and expects `STALE` with no state
change.

Add a same-boundary regression with A just completed, cached B already ready,
and C committed in the mailbox. Drive the exact Section 10.1 order and assert:
A advances `applied_sequence`; C becomes the target tuple; B is discarded before
installation; neither pending nor active ever reports B after that boundary.

Add a regression where B is FLAT but remains unprepared after A completes. The
transparent-path decision must use installed active metadata and continue
processing A until B is actually installed and applied.

- [ ] **Step 2: Add the prepared install API**

```c
#define EQ_INSTALL_INSTALLED 1
#define EQ_INSTALL_BUSY 0
#define EQ_INSTALL_STALE (-1)
#define EQ_INSTALL_INVALID (-2)

int Equalizer_InstallPreparedBank(EQ_STATE *st,
                                  const EQ_PREPARED_BANK *prepared,
                                  int transition_kind);
unsigned long Equalizer_GetActiveGeneration(const EQ_STATE *st);
```

The core installer validates only core-owned metadata (generation, gains,
preset, bank identity, core mode, and no active transition), copies the
candidate into pending, clears destination `s1/s2`, and never reads or updates
control tokens. `EqualizerControl_TryInstallReady` first validates the request
sequence and complete immutable control target tuple, then calls the core
installer. Only after core success does it publish the retained
`installed_sequence`/`installed_generation`/`installed_pair_valid` latch.

- [ ] **Step 3: Re-verify sample-path isolation and installed-path selection**

Retain the Task 2 rule: transition completion only promotes pending to active,
and a newer logical target remains uninstalled for the control service. Add the
A-active/B-unprepared processing regression and the source-contract assertion
that `EQ_ProcessRbjSample` contains no target start, builder, or install call.

- [ ] **Step 4: Implement frame-boundary applied observation**

`EqualizerControl_ObserveFrameBoundary` compares transition state and active
generation with the valid installed-pair latch; it is the only function that
advances `applied_sequence`. Promotion does not clear the latch. A later install
can replace it only after observation has run first at that boundary.

- [ ] **Step 5: Run tests and commit**

```bash
git add Code/User/user_dsp/user_equalizer.h Code/User/user_dsp/user_equalizer.c Code/User/user_dsp/user_equalizer_control.c tools/tests/equalizer_control_test.c
git commit -m "feat: install prepared equalizer banks by generation"
```

### Task 5: Share the board background budget and prove cache prewarm

**Files:**
- Modify: `Code/User/user_dsp/user_equalizer_flow.h`
- Modify: `Code/User/user_dsp/user_equalizer_flow.c`
- Modify: `tools/tests/test_equalizer_control.py`
- Test: `tools/tests/equalizer_control_test.c`

- [ ] **Step 1: Add failing scheduler source contracts**

Add these assertions to the new, whitelisted
`tools/tests/test_equalizer_control.py`: the runtime loop owns one
`consumed_frame`, builder and LCD services both pass through the same decision
function, and no branch can call both services after the same
`EQ_DebugProcessFrames` value.

Statically assert the safe block calls, in order, frame-boundary observation,
one mailbox service, stale invalidation, ready installation, and one shared
background decision. Add a source contract that post-start board code calls no
direct RBJ target setter (`Equalizer_ApplyPreset`,
`Equalizer_ApplySingleBand1kPlus3Db`, `Equalizer_SetBandGainDb`, or
`Equalizer_SetAllGainsDb`).

The existing `tools/tests/test_equalizer_flow_contract.py` is regression input
only. Stage B does not edit or stage it.

Add a contract confirming `Equalizer_Init` completes before `Adc_Start` and
`Dac_Start`.

- [ ] **Step 2: Run RED verification**

```powershell
& 'D:\SoftwareDownload\python.exe' -B -m unittest tools.tests.test_equalizer_flow_contract tools.tests.test_equalizer_control
```

Expected: scheduler-sharing checks fail.

- [ ] **Step 3: Implement the shared scheduler**

Add `EQ_BACKGROUND_SERVICE_STATE`,
`EqualizerBackgroundService_Decide`, and
`EqualizerBackgroundService_Record`. When both workloads are eligible,
alternate `next_preference`. Record the frame only after a real LCD job or
builder payload slice completes.

Initialize `consumed_frame_valid = 0`; route the existing LCD job call and the
new builder call through the one decision result; return to the loop without
fall-through after the selected service attempt.

The builder eligibility check includes
`Equalizer_HasPendingTransition(&EQ_BoardState) == 0`. Control acceptance,
installation, and applied observation occur in the same safe outer block but
outside the heavy-payload budget.

Implement the exact Section 10.1 order. A finalize slice waits until the next
safe boundary for installation; there is no local loop back to install it.

Route fixed preset, FLAT, single-1-kHz, and future custom post-start board target
changes through `EqualizerControl_SubmitRequest`. Keep RAW_COPY, FLOAT_COPY, and
hard-bypass diagnostic path changes mutually exclusive with target service. In
that branch, apply the direct diagnostic path change, call
`EqualizerControl_RebaseAfterDirectPathChange`, and skip mailbox/install/heavy
service for the boundary. Returning to RBJ submits a fresh mailbox target.

- [ ] **Step 4: Add preset-cache Host proof**

After `Equalizer_Init`, record a simulated audio-start marker, capture
`EQ_DebugHeadroomScanCount`, submit and settle all five presets through the
mailbox/control path, and assert no increase and no builder payload/restart
count. The harness also proves every fixed cache entry was ready before the
marker.

- [ ] **Step 5: Run GREEN verification and commit**

```bash
git add Code/User/user_dsp/user_equalizer_flow.h Code/User/user_dsp/user_equalizer_flow.c tools/tests/test_equalizer_control.py tools/tests/equalizer_control_test.c
git commit -m "feat: share equalizer background service budget"
```

### Task 6: Add path-aware immutable response snapshots

**Files:**
- Create: `Code/User/user_dsp/user_equalizer_response.h`
- Create: `Code/User/user_dsp/user_equalizer_response.c`
- Create: `tools/tests/equalizer_response_test.c`
- Modify: `Code/User/user_include.h`
- Modify: `Code/User/user_dsp/user_equalizer.c`

- [ ] **Step 1: Write failing path and immutability tests**

For RAW_COPY, FLOAT_COPY, hard bypass, flat-RBJ transparent copy, dry-to-bank,
stable bank, and bank-to-bank states, copy snapshots and assert the expected
path type, identity flag, valid bank roles, and transition progress.
Byte-compare all `s1/s2`, clip counters, and transition fields before and after
response calculations.

- [ ] **Step 2: Run RED verification**

Run:

```powershell
& 'C:\msys64\usr\bin\bash.exe' -lc 'export PATH=/mingw64/bin:/usr/bin:$PATH; cd /c/Users/zhangyueqian/lab8/DSP_LAB_0507; gcc -std=c99 -Wall -Wextra -Werror -DEQ_ALGO_ONLY -ICode/User/user_dsp Code/User/user_dsp/user_equalizer.c Code/User/user_dsp/user_equalizer_control.c Code/User/user_dsp/user_equalizer_response.c tools/tests/equalizer_response_test.c -lm -o /tmp/equalizer_response_test.exe'
```

Expected: FAIL because the response header/APIs do not exist, not because MSYS2
or include paths are unavailable.

- [ ] **Step 3: Implement snapshot APIs**

```c
int EqualizerResponse_CopyActive(const EQ_STATE *st,
                                 EQ_RESPONSE_SNAPSHOT *out);
int EqualizerResponse_CopyPending(const EQ_STATE *st,
                                  EQ_RESPONSE_SNAPSHOT *out);
int EqualizerResponse_CopyPrepared(const EQ_PREPARED_BANK *prepared,
                                   EQ_RESPONSE_SNAPSHOT *out);
int EqualizerResponse_CopyCommand(const EQ_CONTROL_STATE *control,
                                  EQ_COMMAND_SNAPSHOT *out);
int EqualizerResponse_CopyTarget(const EQ_STATE *st,
                                 const EQ_CONTROL_STATE *control,
                                 EQ_RESPONSE_SNAPSHOT *out);
```

Identity paths contain identity sections, zero preamp, and no copied runtime
state. Dry-to-bank publishes identity as active and the bank as pending.

- [ ] **Step 4: Implement canonical response math**

Move the existing section complex-response calculation behind
`EqualizerResponse_GetSectionComplex`. Core headroom evaluation and new Host
response calculations call this canonical function so there is no duplicate
denominator/sign convention. Define `EQ_RESPONSE_COMPLEX` once in the public
response header and remove or exactly alias the private core complex type.

- [ ] **Step 5: Run GREEN verification and commit**

```bash
git add Code/User/user_dsp/user_equalizer_response.h Code/User/user_dsp/user_equalizer_response.c Code/User/user_include.h tools/tests/equalizer_response_test.c Code/User/user_dsp/user_equalizer.c
git commit -m "feat: add path-aware equalizer response snapshots"
```

### Task 7: Add desired/actual metrics and independent Python reports

**Files:**
- Create: `tools/tests/test_equalizer_response.py`
- Create: `tools/equalizer_33_response_report.py`
- Modify: `Code/User/user_dsp/user_equalizer_eval.c`
- Modify: `Code/User/user_dsp/user_equalizer_eval.h`
- Modify: `docs/equalizer_33_control_response.md`
- Modify: `.gitignore`

- [ ] **Step 1: Add failing Python reference tests**

Test independent RBJ coefficients, desired interpolation endpoints, shape/total
separation, phase unwrap, masked group delay, singular marking, and the 10 x 10
interaction matrix. The test must not import C bindings or parse C-computed
response values as its expected result.

- [ ] **Step 2: Run RED verification**

Expected: FAIL because the report module and new C CSVs do not exist.

- [ ] **Step 3: Extend Host C exports**

Generate these local artifacts:

```text
equalizer_desired_actual_response.csv
equalizer_response_metrics.csv
equalizer_rbj_section_response.csv
equalizer_interaction_matrix.csv
equalizer_control_contract.csv
equalizer_builder_equivalence.csv
```

Rows explicitly identify path type, active/target/pending generations,
transition state, desired visual curve, actual shape, actual total, and preamp.

Migrate existing evaluator scenarios that submit a second target during an
active transition (latest-preset and bypass-restore cases) to a Host control
helper that drives the new mailbox/builder/frame-boundary services. Preserve
their report names and pass thresholds; do not weaken or delete assertions.
The synchronous direct target setters remain only the no-transition
settled-target Host/pre-start reference, not the Stage B asynchronous board
control path. Add a regression proving a direct target setter invoked during a
transition is rejected without changing logical, pending, or active metadata.

- [ ] **Step 4: Implement the independent report generator and exact plots**

Generate exactly:

```text
target_vs_actual_flat.png
target_vs_actual_bass.png
target_vs_actual_vocal.png
target_vs_actual_treble.png
target_vs_actual_v_shape.png
target_vs_actual_custom.png
rbj_individual_sections.png
interaction_matrix_heatmap.png
group_delay_comparison.png
```

under `docs/eval_outputs/equalizer_control_response/`. The directory and
generated CSV/PNG artifacts are ignored. The repository retains only code and
this Markdown specification/summary.

- [ ] **Step 5: Record complexity and storage evidence**

Add Host `sizeof` output to `equalizer_control_contract.csv`. During the CCS
verification task, capture TI `sizeof`/map evidence for the final structures and
record the compact table in Section 16.1 of this document. Record formula-based
hot-path and builder counts separately from measured values, and keep builder
cycles/timing marked `PENDING_HARDWARE`.

- [ ] **Step 6: Run GREEN verification and commit**

```bash
git add tools/tests/test_equalizer_response.py tools/equalizer_33_response_report.py Code/User/user_dsp/user_equalizer_eval.c Code/User/user_dsp/user_equalizer_eval.h docs/equalizer_33_control_response.md .gitignore
git commit -m "test: add equalizer control response evidence"
```

### Task 8: Add Watch diagnostics and complete Host regression

**Files:**
- Modify: `Code/User/user_dsp/user_equalizer_flow.h`
- Modify: `Code/User/user_dsp/user_equalizer_flow.c`
- Modify: `Code/User/user_dsp/user_equalizer_control.h`
- Modify: `Code/User/user_dsp/user_equalizer_control.c`
- Test: all Stage B and existing equalizer tests

- [ ] **Step 1: Export the approved Watch variables**

Export the exact variables in Section 16.2, including observed and installed
tokens. Do not add LCD text or curve drawing.

- [ ] **Step 2: Run the complete MSYS2 Host C suite**

```powershell
& 'C:\msys64\usr\bin\bash.exe' -lc 'export PATH=/mingw64/bin:/usr/bin:$PATH; cd /c/Users/zhangyueqian/lab8/DSP_LAB_0507; gcc -std=c99 -Wall -Wextra -Werror -DEQ_ALGO_ONLY -DEQUALIZER_TEST_MAIN -ICode/User/user_dsp Code/User/user_dsp/user_equalizer.c Code/User/user_dsp/user_equalizer_control.c Code/User/user_dsp/user_equalizer_response.c Code/User/user_dsp/user_equalizer_eval.c -lm -o /tmp/equalizer_all_test.exe && /tmp/equalizer_all_test.exe'
& 'C:\msys64\usr\bin\bash.exe' -lc 'export PATH=/mingw64/bin:/usr/bin:$PATH; cd /c/Users/zhangyueqian/lab8/DSP_LAB_0507; gcc -std=c99 -Wall -Wextra -Werror -DEQ_ALGO_ONLY -ICode/User/user_dsp Code/User/user_dsp/user_equalizer.c Code/User/user_dsp/user_equalizer_control.c Code/User/user_dsp/user_equalizer_response.c tools/tests/equalizer_control_test.c -lm -o /tmp/equalizer_control_test.exe && /tmp/equalizer_control_test.exe'
& 'C:\msys64\usr\bin\bash.exe' -lc 'export PATH=/mingw64/bin:/usr/bin:$PATH; cd /c/Users/zhangyueqian/lab8/DSP_LAB_0507; gcc -std=c99 -Wall -Wextra -Werror -DEQ_ALGO_ONLY -ICode/User/user_dsp Code/User/user_dsp/user_equalizer.c Code/User/user_dsp/user_equalizer_control.c Code/User/user_dsp/user_equalizer_response.c tools/tests/equalizer_response_test.c -lm -o /tmp/equalizer_response_test.exe && /tmp/equalizer_response_test.exe'
```

Expected: all three commands exit zero with zero warnings and zero failures.

- [ ] **Step 3: Run all Python contracts and reports**

```powershell
& 'D:\SoftwareDownload\python.exe' -B -m unittest tools.tests.test_equalizer_control tools.tests.test_equalizer_response tools.tests.test_equalizer_flow_contract tools.tests.test_equalizer_lcd_tooling
& 'D:\SoftwareDownload\python.exe' -B tools\equalizer_33_reference.py
& 'D:\SoftwareDownload\python.exe' -B tools\equalizer_33_response_report.py
```

Expected: all tests green; independent magnitude difference below 0.01 dB; no
`__pycache__` or `.pyc` files.

- [ ] **Step 4: Commit the integration**

```bash
git add Code/User/user_dsp/user_equalizer_flow.h Code/User/user_dsp/user_equalizer_flow.c Code/User/user_dsp/user_equalizer_control.h Code/User/user_dsp/user_equalizer_control.c
git commit -m "feat: expose equalizer control builder diagnostics"
```

### Task 9: CCS verification, whitelist audit, and final feature commit

**Files:**
- Verify all allowed Stage B paths
- Do not stage `Debug/`, generated evidence, `projects/`, or any forbidden path

- [ ] **Step 1: Refresh the local generated CCS build graph if required**

Regenerate the ignored CCS managed-build graph and verify both new modules are
present in at least these generated locations:

- `Debug/Code/User/user_dsp/subdir_vars.mk`: `C_SRCS`, `C_DEPS`, `OBJS`, and all
  quoted lists;
- `Debug/makefile`: `ORDERED_OBJS` and clean dependency/object lists;
- `Debug/ccsObjs.opt`: linker object inputs.

Do not stage any `Debug/` file. If CCS cannot regenerate these entries without a
tracked project-metadata change, stop and report the exact missing dependency
before modifying project metadata.

- [ ] **Step 2: Build the three CCS configurations**

Use the installed CCS gmake:

```powershell
& 'D:\SoftwareDownload\CCS_20.5.0.00028_win\ccs\utils\bin\gmake.exe' -B -C Debug all 'GEN_OPTS__FLAG=--define=DSP_LAB_PROJECT_SELECT=32'
& 'D:\SoftwareDownload\CCS_20.5.0.00028_win\ccs\utils\bin\gmake.exe' -B -C Debug all 'GEN_OPTS__FLAG=--define=DSP_LAB_PROJECT_SELECT=33 --define=EQ_ENABLE_LCD_DISPLAY=0'
& 'D:\SoftwareDownload\CCS_20.5.0.00028_win\ccs\utils\bin\gmake.exe' -B -C Debug all 'GEN_OPTS__FLAG=--define=DSP_LAB_PROJECT_SELECT=33 --define=EQ_ENABLE_LCD_DISPLAY=1'
```

Each command has a ten-minute maximum. Do not run DSS. Confirm
`Debug/DSP_LAB_0507.out` exists and the link XML contains
`<link_errors>0x0</link_errors>`.

- [ ] **Step 3: Audit the exact diff and forbidden paths**

```powershell
git status --short
git diff --check
git diff --stat
git diff --cached --name-only
git diff --name-only -- Code/User/user_dsp/user_subband_* Code/User/user_touch* Code/main.c outputs
```

The final command must produce no Stage B changes. The cached-name list must
contain only Section 17 whitelist paths. Confirm no PNG, WAV, RAW, large CSV,
PPTX, NDJSON, `__pycache__`, `.pyc`, `projects/`, or generated `Debug/` file is
staged.

- [ ] **Step 4: Run a final fresh verification pass**

Re-run the full Host C suite, Python suite, three CCS builds, link XML check,
`git diff --check`, and whitelist audit after all edits are complete.

- [ ] **Step 5: Create the final feature commit on the development branch**

```bash
git add .gitignore Code/User/user_include.h Code/User/user_dsp/user_equalizer.c Code/User/user_dsp/user_equalizer.h Code/User/user_dsp/user_equalizer_control.c Code/User/user_dsp/user_equalizer_control.h Code/User/user_dsp/user_equalizer_response.c Code/User/user_dsp/user_equalizer_response.h Code/User/user_dsp/user_equalizer_flow.c Code/User/user_dsp/user_equalizer_flow.h Code/User/user_dsp/user_equalizer_eval.c Code/User/user_dsp/user_equalizer_eval.h tools/equalizer_33_response_report.py tools/tests/equalizer_control_test.c tools/tests/equalizer_response_test.c tools/tests/test_equalizer_control.py tools/tests/test_equalizer_response.py docs/equalizer_33_control_response.md
git commit -m "feat: add nonblocking project 3.3 control and response backend"
```

- [ ] **Step 6: Integrate the verified branch into local main**

Require a clean Stage B index, preserve the unrelated untracked `projects/`
directory, and run:

```bash
git switch main
git merge --ff-only codex/project33-control-response
git status --short --branch
```

If `main` cannot fast-forward, stop and report instead of forcing, rebasing, or
discarding work. Do not push the remote without a separate user instruction.
Report all unverified board properties as `PENDING_HARDWARE` and stop.
