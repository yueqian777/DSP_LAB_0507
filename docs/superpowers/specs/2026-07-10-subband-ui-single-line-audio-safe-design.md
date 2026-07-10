# Project 3.2 Single-Line LCD And Audio-Safe Scheduler Design

## Scope

Only the Project 3.2 display/control layer changes. WOLA, all denoisers, codec,
mode numbers, ADC/DAC, and the 160/240/320 kbps control path remain unchanged.
The current round has no board, so runtime audio, touch, frame continuity, and
LCD cycle conclusions are `NOT_MEASURED_BOARD_UNAVAILABLE`.

## Processing Chain

The chain is one ASCII line inside `(28,80)-(770,106)`. It uses ` -> ` because
the bundled grlib fonts are ASCII-only. A bounded pure helper builds the exact
mode string from applied mode and applied codec target. Modes 8 and 11 insert
`160k`, `240k`, or `320k`; all strings end in `DAC`, and every WOLA path retains
`WOLA SYN`.

`UI_DrawProcessingChain()` snapshots applied mode and target, builds the text,
selects the first fitting existing font with `GrStringWidthGet()`, clears only
the single-line rectangle, and draws once. Displayed-chain state and the chain
dirty bit change only after the draw completes. A bounded fallback is
`ADC -> PROCESSING -> DAC`.

## Dirty Lifecycle And Priority

Mode changes mark old/new mode buttons, status, chain, relevant rate buttons,
countdown state, and load. The runtime priority is:

1. old/new mode button (one per job)
2. status
3. processing chain
4. codec rate button (one per job)
5. learning/countdown
6. optional ten-block progress
7. algorithm load

The status job never clears chain dirty. The chain job clears it only after the
actual draw. Chain set/draw/wait counters expose starvation. The duplicate
standalone rate text and its draw job are removed; the buttons and chain carry
all necessary rate information.

## Audio-Safe Runtime Scheduler

The main loop calls display service only when AD, DA, pending AD-to-DA work,
audio service, and touch service are all idle. `FLAG_AD_DONE == 0U` is a hard
condition.

Runtime service executes at most one effective job for an algorithm frame.
Successful jobs update `UI_LastDrawAlgoFrame`; repeated calls in that frame are
skipped. Consecutive jobs are separated by two algorithm frames. Applying a new
mode starts a three-frame draw holdoff after all algorithm configuration is
complete. Initialization may still draw the full static page before ADC/DAC
start.

## Learning Display

The default policy is coarse. Modes with learning produce at most three visible
states: `2 s`, `1 s`, and ready. Raw LearnHops changes do not mark LCD work.
Only a change in the rounded-up remaining seconds or a learning/ready state
transition marks the combined countdown job. Ten-block progress code remains
behind an explicit policy but is not selected by default.

## Load Accounting

`SubbandUI_RecordAlgoCycles()` only updates integer EMA cycles and sample count.
No 64-bit multiply, frame-budget division, percentage conversion, or dirty
decision runs per audio frame. The low-frequency load update performs the
percentage conversion every 50 frames and remains lowest priority.

## Verification

Host tests cover exact eight chain strings, bounded fallback, dynamic codec
rates, no Unicode arrow, single-line geometry, chain dirty lifecycle, priority,
one-job-per-frame, draw gap, holdoff, coarse learning transitions, load-path
restrictions, fixed coordinates, and the `FLAG_AD_DONE` guard. CCS Debug/O3 is
built without board execution. A manual plan records all board-only checks as
not measured.
