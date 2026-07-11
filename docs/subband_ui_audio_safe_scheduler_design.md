# Project 3.2 Audio-Safe LCD Scheduler

Audio remains above LCD work in the main loop. `SubbandUI_ServiceDisplay()` is
called only when `FLAG_AD`, `FLAG_DA`, and `FLAG_AD_DONE` are all zero and no
audio or touch service ran in the current loop pass. `FLAG_AD_DONE` is retained
because it represents processed ADC data whose corresponding DAC update is
still pending.

Runtime drawing follows these limits:

- One effective LCD job per `SUBBAND_DebugAlgoFrames` value.
- A two-frame minimum gap between effective jobs, about 40.96 ms.
- A three-frame holdoff after an applied mode change, about 61.44 ms.
- Priority: old/new mode buttons, status, chain, rate buttons, learning state,
  optional ten-block progress, then load.
- Initialization may draw the full static page before streaming starts.

The status job does not clear chain dirty. `UI_MarkChainDirty()` records a new
dirty transition; only `UI_DrawProcessingChain()` updates displayed mode/rate,
increments draw counters, and clears `UI_DIRTY_CHAIN`. Dirty-without-draw frame
counting exposes starvation.

The default learning policy is `SUBBAND_UI_PROGRESS_COARSE`. Raw LearnHops only
changes internal state. LCD work is marked when the visible state changes to
`2 s`, `1 s`, or ready, giving at most three learning-related updates. The old
ten-block implementation remains available only through the explicit
`SUBBAND_UI_PROGRESS_TEN_BLOCK` build policy.

`SubbandUI_RecordAlgoCycles()` performs only an integer EMA and sample count.
The frame-budget division and load dirty decision run once every 50 algorithm
frames in `UI_UpdateLoadDirtyState()`.
