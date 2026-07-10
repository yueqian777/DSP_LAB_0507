# Subband UI Refresh Fix Summary

## Verified In This Session

- Contract tests: `python -m unittest tools.tests.test_subband_touch_ui_contract`
  passed.
- CCS Debug build through MSYS2:
  `gmake -C Debug all` passed with 0 errors and 0 warnings.
- `git diff --check` passed.

## Requirement Answers

1. Four root causes are documented in
   `docs/subband_ui_refresh_failure_analysis.md`.
2. This page's Widget auto drawing was removed.
3. Other Widget examples keep compatibility through existing `Touch_Scan()`.
4. Learning progress is now incremental 10-block drawing.
5. `LearnHops` no longer marks the status area dirty.
6. Current UI load uses an integer EMA rolling cycle statistic.
7. Mode 0 can show its own current rolling load after enough samples; it no
   longer inherits `SUBBAND_DebugMaxCycles`.
8. Current mode highlight is based only on `SUBBAND_DebugAppliedDemoMode`.
9. Chinese labels are drawn inside each custom button draw function, not as a
   post-Widget overlay.
10. Mode 8 / 320 kbps / dynamic UI dropout status is not claimed until board
    measurement is run.
11. Runtime draw telemetry is available through the new
    `SUBBAND_UI_DebugMax*DrawCycles` and over-budget counters.
12. Before HEAD:
    `44e40c462a077ce331211e77d6fb667c3c7988ce`; after state is uncommitted
    working tree based on the same HEAD.

## Board Follow-Up

Run the required physical checks on the board:

- Mode 8, 320 kbps, learning countdown enabled, dynamic UI enabled, at least
  60 seconds.
- Record AD frames, DA frames, algo frames, codec frames, historical max cycles,
  rolling load, UI max draw cycles, draw over-budget count, touch max cycles,
  and invalid count.
- Repeat mode/rate touch stress and visually inspect that Chinese labels remain
  present and only the Applied Mode button is highlighted.
