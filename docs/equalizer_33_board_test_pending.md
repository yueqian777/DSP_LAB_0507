# Project 3.3 Board Test Checklist

Status for every item in this document: `PENDING_HARDWARE` / `NOT_RUN`.
No hardware, audio quality, timing, LCD, or Watch result is implied by the
offline validation work.

1. Set `EQ_DebugDiagPath=EQ_DIAG_RAW_COPY` and keep `EQ_ENABLE_LCD_DISPLAY=0`.
   Verify CH1 input and DA_CH1 output are sample-identical and CH2-CH8 remain
   passthrough.
2. Set `EQ_DebugDiagPath=EQ_DIAG_FLOAT_COPY` with LCD off.
3. Set `EQ_DebugDiagPath=EQ_DIAG_FLAT` with LCD off.
4. Set `EQ_DebugDiagPath=EQ_DIAG_SINGLE_BAND` with LCD off.
5. Set `EQ_DebugDiagPath=EQ_DIAG_PRESET` with LCD off.  Exercise FLAT, BASS,
   VOCAL, TREBLE, and V_SHAPE.
6. Repeat RAW_COPY with LCD on (`EQ_ENABLE_LCD_DISPLAY=1`).
7. Repeat FLAT with LCD on.
8. Exercise preset changes and bypass ON/OFF while monitoring requested,
   pending, applied, and latest-target state.
9. Record 10 seconds of frame, clip, and timing counters.
10. Run the selected preset for 60 seconds and inspect long-term stability.

Required Watch variables, all `NOT_RUN`:

- `EQ_DebugBuildMagic`, `EQ_DebugBuildVersion`, `EQ_DebugBuildGitSha`
- `EQ_DebugBuildTimestamp`, `EQ_DebugBuildDirty`
- `EQ_DebugDiagPath`, `EQ_DebugMode`, `EQ_DebugBandGainDb`
- `EQ_DebugRequestedMode`, `EQ_DebugTransitionTargetMode`
- `EQ_DebugAppliedMode`, `EQ_DebugModeChangeCount`
- `EQ_DebugAdFrames`, `EQ_DebugDaFrames`, `EQ_DebugProcessFrames`
- `EQ_DebugAlgoLastCycles`, `EQ_DebugAlgoMaxCycles`
- `EQ_DebugAlgoLastMs`, `EQ_DebugAlgoMaxMs`
- `EQ_DebugModeServiceLastCycles`, `EQ_DebugModeServiceMaxCycles`
- `EQ_DebugFrameServiceLastCycles`, `EQ_DebugFrameServiceMaxCycles`
- `EQ_DebugFrameServiceLastMs`, `EQ_DebugFrameServiceMaxMs`
- `EQ_DebugFrameLatencyLastCycles`, `EQ_DebugFrameLatencyMaxCycles`
- `EQ_DebugFrameLatencyLastMs`, `EQ_DebugFrameLatencyMaxMs`
- `EQ_DebugDeadlineMissCount`, `EQ_DebugFrameServiceOverlapCount`
- `EQ_DebugFrameServiceDroppedCount`
- `EQ_DebugClipCount`, `EQ_DebugPredictedPeakDb`, `EQ_DebugPreampDb`
- `EQ_DebugLcdEnabled`, `EQ_DebugLcdRefreshCount`, `EQ_DebugLcdSkipBusyCount`
- `EQ_DebugLcdGainRedrawCount`, `EQ_DebugLcdStatusRedrawCount`

Acceptance must be recorded from the real board.  The offline reports do not
establish AD/DA continuity, real-time CPU margin, LCD interference, audible
quality, download success, or actual Watch values.

Active frame-service time includes ADC copies, CH1 processing, passthrough
preparation, and DAC copies while excluding hardware wait time; it drives the
20.48 ms deadline counter. End-to-end AD-to-DAC latency is reported separately
as a scheduling diagnostic. Both remain `PENDING_HARDWARE`.
