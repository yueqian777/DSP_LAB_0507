# Subband UI Refresh Failure Analysis

## Scope

This change only modifies the subband touch UI, display scheduling, touch-page
event handling, and UI load statistics. It does not modify WOLA, MCRA, Minimum
Statistics, codec quantization or bit allocation, ADC/DAC sampling parameters,
or existing demo-mode algorithm parameters.

Baseline HEAD before this work:
`44e40c462a077ce331211e77d6fb667c3c7988ce`.

Current working tree is still based on that HEAD; no commit SHA was created in
this session.

## Root Causes

1. First 2 seconds denoise distortion risk
   `UI_DrawCountdown()` repeatedly cleared large rectangles and redrew a
   continuous 751-pixel progress bar. One runtime draw job could block long
   enough for the next audio frame to arrive.

2. Status panel redraw during learning
   `LearnHops` changes marked both countdown and status dirty. The status text
   normally does not change per hop, so this added unnecessary LCD work during
   the busiest learning interval.

3. Load showed historical maximum
   `UI_ComputeAlgoLoad()` used `SUBBAND_DebugMaxCycles`, which is a benchmark
   historical maximum. After running a heavy mode, lighter modes inherited that
   old peak in the UI.

4. Mode highlight and Chinese label loss
   The page used empty `RectangularButton` widgets and then overlaid Chinese
   text. Widget press/release repaint could redraw an empty button and erase the
   label. Widget pressed state also competed with Applied Mode as the visual
   highlight source.

## Fix Summary

- Added `Touch_ScanRaw()` for raw GT1151 scan results without Widget events.
  Existing `Touch_Scan()` still calls `Widget_Btn_App()` for old examples.
- Removed subband-page mode Widgets, `WidgetAdd()`, `WidgetPaint()`,
  `RectangularButton(...)`, and `UI_DrawModePhrasesOnly()`.
- Added fixed mode/rate button tables, table-based hit testing, and local
  button draw functions that fill background, draw border, and draw text in one
  function.
- Added a local press-release state machine. A click is accepted only when
  press and release land on the same fixed button. Long press does not repeat.
- Mode highlight is derived only from `SUBBAND_DebugAppliedDemoMode`.
- Replaced continuous learning progress with 10 discrete progress blocks.
  Runtime display service draws or clears at most one progress block per call.
- `LearnHops` changes now mark countdown/progress work only, not status.
- Added runtime draw budget telemetry:
  `SUBBAND_UI_DebugDrawOverBudgetCount`,
  `SUBBAND_UI_DebugLastDrawJob`,
  `SUBBAND_UI_DebugMaxProgressDrawCycles`,
  `SUBBAND_UI_DebugMaxButtonDrawCycles`,
  `SUBBAND_UI_DebugMaxTextDrawCycles`.
- Added rolling UI load statistics using integer EMA:
  first sample equals current cycles, then
  `(rolling * 7 + cycles + 4) / 8`.
- `SUBBAND_DebugMaxCycles` remains unchanged as historical benchmark data.

## Board Test Status

Automated contract tests and CCS Debug build passed. Physical board audio and
visual checks were not run by Codex in this session. Do not claim the first
2-second audio distortion is fixed until the board-side Mode 8 / 320 kbps /
60-second run is measured.
