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
8. Exercise preset changes while monitoring the RBJ bank transition.
9. Record 10 seconds of frame, clip, and timing counters.
10. Run the selected preset for 60 seconds and inspect long-term stability.

Required Watch variables, all `NOT_RUN`:

- `EQ_DebugBuildMagic`, `EQ_DebugBuildId`, `EQ_DebugBuildDirty`
- `EQ_DebugDiagPath`, `EQ_DebugMode`, `EQ_DebugBandGainDb`
- `EQ_DebugAdFrames`, `EQ_DebugDaFrames`, `EQ_DebugProcessFrames`
- `EQ_DebugLastCycles`, `EQ_DebugMaxCycles`, `EQ_DebugLastMs`, `EQ_DebugMaxMs`
- `EQ_DebugClipCount`, `EQ_DebugPredictedPeakDb`, `EQ_DebugPreampDb`
- `EQ_DebugLcdEnabled`, `EQ_DebugLcdRefreshCount`, `EQ_DebugLcdSkipBusyCount`
- `EQ_DebugLcdGainRedrawCount`, `EQ_DebugLcdStatusRedrawCount`

Acceptance must be recorded from the real board.  The offline reports do not
establish AD/DA continuity, real-time CPU margin, LCD interference, audible
quality, download success, or actual Watch values.
