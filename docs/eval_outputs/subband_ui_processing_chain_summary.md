# Subband UI Processing Chain Summary

## Implemented

- Removed visible `REQ/APPLIED` from the status panel.
- Removed visible `SEL/TGT/EST` from the bitrate panel.
- Added an independent processing-chain display region.
- Added `UI_DIRTY_CHAIN` so chain redraws do not repaint mode buttons,
  bitrate buttons, progress blocks, or the learning panel.
- Added chain diagnostics for CCS Watch.
- Kept internal debug variables and codec control semantics unchanged.

## Verification

- `python -m unittest tools.tests.test_subband_touch_ui_contract`
  passes with 16 tests.
- Board audio/display timing is not claimed here; it needs manual
  verification after flashing.

## Manual Board Checklist

1. Touch modes `0 -> 1 -> 2 -> 4 -> 6 -> 7 -> 8 -> 11`.
2. Confirm the chain follows the applied mode, not the last touched
   button.
3. In modes 8 and 11, touch 160, 240, and 320 kbps and confirm the
   chain updates the codec bitrate after the target takes effect.
4. Confirm no visible `REQ`, `APPLIED`, `SEL`, `TGT`, or `EST` remains.
5. Run mode 8 at 320 kbps for at least 60 seconds and watch for audio
   tearing, dropped frames, or chain text disappearance.
