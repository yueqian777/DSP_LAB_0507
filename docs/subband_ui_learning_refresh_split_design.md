# Project 3.2 Learning Refresh Split Design

## Scope

This change finalizes the existing Project 3.2 learning-state refresh split. It
does not change Project 3.3, WOLA, MCRA, Minimum Statistics, codec algorithms,
ADC/DAC, touch scanning, mode IDs, or the audio/display service guard.

## Original Cost

Before the split, the normal `2 s -> 1 s` learning transition repainted the
complete Chinese learning-state area from `(28, 326)` through `(222, 376)`.
That is a 195 x 51 pixel region and includes phrase drawing that is unnecessary
when only one ASCII digit changes.

## Current Design

Learning display work is divided into two jobs:

- Full state job: `UI_DIRTY_COUNTDOWN`, drawn by `UI_DrawLearningState()`.
- Remaining digit job: `UI_DIRTY_REMAINING_DIGIT`, drawn by
  `UI_DrawRemainingDigit()`.

The independent digit rectangle is `(82, 352)` through `(130, 376)`, a 49 x 25
pixel region. `UI_DrawOneJob()` gives the full state job priority over the digit
job. A full state draw cancels any pending digit job, so an older `1 s` update
cannot run after a mode or learning-state transition.

The normal two-second sequence therefore needs only:

1. Enter learning: one full state job, including the initial `2 s`.
2. Change `2 s -> 1 s`: one independent ASCII digit job.
3. Finish learning: one full state job showing ready.

The existing one-job-per-frame limit, two-frame draw gap, three-frame mode
holdoff, `FLAG_AD_DONE` idle guard, and coarse progress policy are unchanged.

## Zero-Second Suppression

`0 s` is blocked at three boundaries:

1. `SubbandUI_SelectLearningDisplayJob()` only creates a digit job when
   `remaining_seconds > 0`.
2. `UI_DrawNextRemainingDigitJob()` cancels a pending job when
   `remaining_seconds <= 0` and increments
   `SUBBAND_UI_DebugSuppressedZeroSecondJobs`.
3. A full state draw calls `UI_DrawRemainingDigit()` only when the remaining
   value is positive.

`UI_DrawRemainingDigit(int remaining_seconds)` now receives the value already
computed by its caller. This both prevents a zero or negative value from
reaching the renderer and removes a duplicate remaining-time calculation from
each digit draw.

If the algorithm briefly remains `Learning=1`, `Ready=0`, `Remaining=0` without
a state transition, no new dirty bit is set. The current display remains in
place until a later full state update.

## Cancellation Rules

A pending digit job is cancelled when:

- the applied mode changes;
- the learning or ready state changes;
- a full state job executes while a digit job is pending;
- the mode no longer uses learning;
- learning has stopped;
- the displayed state no longer permits a digit-only update;
- the current value is zero, negative, or already displayed.

`SUBBAND_UI_DebugCancelledDigitJobs` counts these stale-job cancellations. The
host state-machine test expects one cancellation in the pending-digit mode
switch scenario and one in the simultaneous full-state/digit scenario.

## Debug Counter Semantics

`SUBBAND_UI_DebugRemainingDigitDrawCount`,
`SUBBAND_UI_DebugLastRemainingDigitDrawCycles`, and
`SUBBAND_UI_DebugMaxRemainingDigitDrawCycles` cover independent
`UI_DRAW_JOB_REMAINING_DIGIT` jobs only. They do not count the initial digit
drawn inside a full state job. Full state cycle counters already include that
initial digit cost.

## Host State-Machine Coverage

The no-LCD test model is compiled only when `SUBBAND_UI_HOST_TEST` is defined.
It reuses the production job-selection function while modeling dirty flags,
displayed state, job priority, and cancellation. It covers:

1. Normal `2 s -> 1 s -> ready`.
2. Pending `1 s` digit followed by Mode 0.
3. Direct `2 s -> ready`.
4. `Learning=1`, `Remaining=0`.
5. Full state and digit dirty at the same time.

The host-only model is excluded from the TI build, so it adds no C6748 runtime
branches or data.

## Resource Result

Compared with baseline HEAD `64c330d`, the Project 3.2 Debug image changed by:

- `.text`: 121024 B -> 121216 B, +192 B.
- `.const`: 14632 B -> 14632 B, no change.
- `.bss`: 43 B -> 43 B, no change.
- `.neardata`: 872 B -> 876 B, +4 B for the suppression debug counter.

The structural change reduces the LCD work for a second-only transition, but
board audio, LCD timing, frame continuity, and touch behavior remain
`NOT_MEASURED_BOARD_UNAVAILABLE` until the generated image is tested on the
actual board.
