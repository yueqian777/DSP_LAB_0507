# Subband UI Static Verification

- Static checks: 31/31 PASS.
- Python contract suite: 24/24 PASS.
- Host C scheduler: executed and PASS.
- Host scheduler scenarios: 5/5 PASS.
- Pending digit then Mode 0 cancellation count: 1.
- Simultaneous full-state/digit cancellation count: 1.
- Other three scenario cancellation counts: 0.
- CCS Project 3.2 Debug clean/all: PASS.
- Link errors: `0x0`.
- Project 3.2 `.text`: 121216 B, +192 B from baseline HEAD `64c330d`.
- `.const`: 14632 B, delta 0 B.
- `.bss`: 43 B, delta 0 B.
- `.neardata`: 876 B, +4 B for the suppression debug counter.
- `graphify-out/`: fully removed from tracked files and ignored at the root.
- Board audio, touch, LCD appearance, frame continuity, and cycle timing:
  `NOT_MEASURED_BOARD_UNAVAILABLE`.

The structural result is a smaller digit-only LCD update and complete rejection
of zero-second digit jobs. It is not board evidence that audible distortion is
resolved.
