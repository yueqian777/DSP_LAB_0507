# Project 3.2 Manual Board Test Plan

## Setup

1. Build and load the exact tested `.out` without changing source or macros.
2. Start a continuous 50 kHz audio input at a documented level.
3. Add the UI, AD/DA/algo, codec, invalid, and cycle counters to CCS Watch.
4. Reset counters, start audio, then run each case without stopping the source.

## Functional Checks

1. Select modes 0, 1, 2, 4, 6, 7, 8, and 11.
2. Confirm exactly one visible ASCII chain under the running status and no
   stale previous-mode text.
3. In modes 8 and 11, select 160/240/320 kbps by touch and CCS Watch. Confirm
   both `CODEC XXXk` and the selected rate-button color follow the applied rate.
4. For learning modes, confirm only `2 s`, `1 s`, and ready are displayed and
   the ten-block animation remains absent.
5. Confirm every touch still needs a valid press/release on the same button.

## Audio And Timing Checks

1. Compare uninterrupted mode 0 audio before and after repeated UI operations.
2. Run mode 8 at 320 kbps for 60 seconds while changing modes/rates normally.
3. Record AD frames, DA frames, algo frames, codec frames, invalid count,
   algorithm MaxCycles, UI MaxDrawCycles, Chain MaxDrawCycles,
   DrawOverBudgetCount, and MaxDrawJobsPerFrame.
4. Confirm all frame counters continue, invalid count stays zero, and
   `MaxDrawJobsPerFrame <= 1`.
5. Listen specifically for clicks, tearing, short silence, or distortion at
   each status, chain, button, learning, and load update.

Until executed, every item above is `NOT_MEASURED_BOARD_UNAVAILABLE`.
