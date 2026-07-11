# Project 3.2 Manual Board Test Plan

## Setup

1. Build and load the exact verified Project 3.2 `Debug/DSP_LAB_0507.out`.
2. Start a continuous 50 kHz audio input at a documented level.
3. Add AD/DA/algo, codec, invalid, touch, and UI cycle counters to CCS Watch.
4. Also watch `SUBBAND_UI_DebugLearningStateDrawCount`,
   `SUBBAND_UI_DebugRemainingDigitDrawCount`,
   `SUBBAND_UI_DebugCancelledDigitJobs`, and
   `SUBBAND_UI_DebugSuppressedZeroSecondJobs`.
5. Reset counters and run each case without stopping the audio source.

## Learning Display Checks

1. Enter Mode 6 or Mode 8 and confirm the sequence is `2 s`, `1 s`, then ready.
2. Confirm `0 s` is never visible.
3. For a normal two-second learning run, expect one full state job on entry,
   one independent digit job for `2 s -> 1 s`, and one full state job on ready.
4. Switch to Mode 0 while `1 s` is pending or visible. Confirm the final panel
   shows running and no old digit appears afterward.
5. Repeat a fast mode switch near completion and verify
   `SUBBAND_UI_DebugCancelledDigitJobs` can increase while no stale digit is
   drawn.
6. Confirm the ten-block progress animation remains absent under the coarse
   progress policy.

## General UI Checks

1. Select modes 0, 1, 2, 4, 6, 7, 8, and 11.
2. Confirm one visible ASCII processing chain follows the applied mode and no
   previous-mode text remains.
3. In modes 8 and 11, select 160, 240, and 320 kbps. Confirm the chain and rate
   button follow the applied target.
4. Confirm each valid press/release activates once without requiring repeated
   touches.

## Audio And Timing Checks

1. Compare uninterrupted Mode 0 audio before and after repeated UI operations.
2. Run Mode 8 at 320 kbps for 60 seconds while changing modes and rates.
3. Listen specifically during learning entry, `2 s -> 1 s`, ready, chain, rate,
   and load updates.
4. Record AD frames, DA frames, algo frames, codec frames, invalid count,
   algorithm MaxCycles, UI MaxDrawCycles, full-state MaxCycles, independent
   digit MaxCycles, DrawOverBudgetCount, and MaxDrawJobsPerFrame.
5. Confirm frame counters continue, invalid count remains zero, and
   `MaxDrawJobsPerFrame <= 1`.
6. Record whether clicks, tearing, short silence, or distortion occur.

Until these checks are completed on the board, every audio, touch, LCD, frame,
and cycle conclusion is `NOT_MEASURED_BOARD_UNAVAILABLE`.
