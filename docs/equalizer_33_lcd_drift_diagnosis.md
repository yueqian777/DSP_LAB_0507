# Project 3.3 LCD Circular-Shift Diagnosis

**Date:** 2026-07-18

**Current gate:** Static alignment page passed on the board. Dynamic Status,
Touch, and combined endurance remain `PENDING_HARDWARE`. Stage A is therefore
not complete, and the ten-band editor must not start.

## Failure baseline

The failure was preserved on clean source `5d1525ae984acb53765e00cf678874189d2aaf30`.
The loaded `.out` SHA-256 was
`1677223adb4ffc7e98e977eb689e49aef58ae144e78aa72744e8d9e890987c49`.
The operator photograph showed the bottom dynamic rows wrapped to the top and
the title moved into the middle. This is a vertical circular scan shift, not a
single fixed X/Y translation. No global coordinate offset was added.

The local baseline photograph is not committed. Its size is 2,938,988 bytes
and its SHA-256 is
`a7111d0ad3bd142ed442fb1b0c1de62081335778010f3019822a583c150146ea`.
The observation label is `OPERATOR_VISUAL_FAILURE_SCREEN_CIRCULAR_SHIFT`.

## Bounded changes

Commit `67a22ef399cda9e0a4c2bb74e3dcaa0392c342f0` hardened the Project 3.3
renderer without changing the audio algorithms:

- private drawing helpers use stack-local `tRectangle` values;
- every runtime rectangle is checked against the fixed 800x480 bounds;
- low-rate LCDC/DMA snapshots verify FB0 base/end and raster fault status;
- fixed framebuffer canaries are checked without scanning the framebuffer;
- a detected raster, address, or canary fault latches diagnostics and disables
  runtime UI work while audio remains active;
- the static alignment page is drawn once before audio and never refreshed.

Commit `b23a7ce7dbf89f6dd0920fd6a7db9c395a3d5e87` bounded runtime display
pressure:

- Analyzer bars use latest-wins differential strips, at most 16 pixels high;
- crossing 0 dB clears the old side before filling the new side;
- a service updates either one strip or one value field, never both;
- value text changes at 2 dB or after 50 audio frames;
- Preset, Dynamic, Chain, and Analyzer categories use 2/4/8/8-frame gaps;
- the global steady gap limits the average runtime rate to 8 jobs/s;
- all normal runtime jobs must be at most 912,000 cycles (2 ms).

No Project 3.2 source, equalizer coefficient math, preset, preamp, crossfade,
Analyzer FFT, dynamic-stage parameter, ADC/DAC/EDMA/PRU source, framebuffer
size, or `Code/main.c` selection was changed by these two commits.

## Static board gate

The alignment profile used clean build `67a22ef`, generated `dirty=0`, Touch
OFF, runtime mask 0, and a single static diagnostic page. The exact `.out`
SHA-256 was
`50c95e3e4b06a0a3d22ccc05744643c01aeec4a120ed8d7db9c36936f0942ba7`.

| Metric | Before | After 600,000 ms | Result |
|---|---:|---:|---|
| AD / DA / process frames | 195 | 29,443 | +29,248, aligned |
| Runtime LCD jobs | 0 | 0 | Static page remained static |
| FB0 base | `0xC0000004` | `0xC0000004` | Stable |
| FB0 end | `0xC00BB820` | `0xC00BB820` | Stable |
| Canary checks | 1 | 116 | Advanced |
| Frame service max cycles | 818,909 | 818,909 | No increase |
| Frame latency max cycles | 819,100 | 819,100 | No increase |

Raster fault, sync lost, FIFO underflow, frame-address mismatch, canary
failure, bounds failure, deadline, latency miss, overlap, dropped-frame, and
clip counters were all zero. Startup LCD status was captured as `0x145` and
became `0x141` after the one allowed status clear; raster remained enabled.

The automated result intentionally retained
`PENDING_OPERATOR_VISUAL_OBSERVATION`. After the full ten-minute window, the
operator answered that no circular shift was visible. The static visual gate
is therefore `OPERATOR_VISUAL_OBSERVATION_PASS`.

## Host and CCS evidence

At `46bc54e22d59372259c65cf230a427cbe70ab066`, 102 Project 3.3/equalizer
Python and Host tests passed. The Project 3.2 shared FFT regression passed all
8 frozen 512-point cases and 12 consecutive WOLA hops bit-exactly.

Full `tools/tests` discovery ran 187 tests: 186 passed. The one pre-existing
Project 3.2 tooling-contract failure expects
`final_full_chain_240_rerun.csv`, while the existing rerun DSS script names
`board_missing_tests_rerun.csv`. This is not an FFT/WOLA or Project 3.3
regression and was not changed in this LCD-only scope.

The clean A-E CCS matrix completed with zero warnings, zero errors, and
`link_errors=0x0` for every profile:

| Profile | `.text` | `.const` | `.bss` | `.subband_l2` | `.out` bytes |
|---|---:|---:|---:|---:|---:|
| A Project 3.2 | 122,176 | 14,636 | 43 | 257,124 | 884,188 |
| B Project 3.3 LCD OFF | 123,616 | 1,928 | 10 | 20,380 | 750,268 |
| C Alignment static | 156,864 | 5,256 | 30 | 20,380 | 954,184 |
| D Dynamic Status | 157,728 | 7,618 | 30 | 20,380 | 968,096 |
| E Dynamic + Touch | 177,440 | 14,506 | 43 | 20,380 | 1,129,000 |

The current `EQ_UI_STATE` is 312 bytes. Touch state plus transform is 36
bytes. No UI object was added to `.subband_l2`, and no second framebuffer was
created.

## Current conclusion

The ten-minute static run found no LCDC/DMA/address/canary fault and no visual
shift after renderer-state hardening. That is evidence against a persistent
hardware raster-start or framebuffer-address fault. The strongest current
software evidence is the old shared rectangle state plus oversized runtime
writes, but this is not yet a complete root-cause proof because the hardened
Dynamic Status page has not run for ten minutes on the board.

## Remaining Stage A hardware gates

- Dynamic Status, Analyzer and all three dynamic stages ON, Touch OFF,
  ten minutes: `PENDING_HARDWARE`.
- Physical Touch operation for five minutes: `PENDING_HARDWARE`.
- Combined audio, Analyzer, dynamics, preset switching, Touch and LCD for ten
  minutes: `PENDING_HARDWARE`.
- Operator visual confirmation for the dynamic and combined pages:
  `PENDING_HARDWARE`.

The prepared dynamic runner is
`tools/run_equalizer_lcd_dynamic_hardware.ps1`. It requires a clean worktree,
builds the exact no-Touch profile, uses PC default line-out only, records the
objective counters, disconnects DSS, and leaves the DSP running. It was not
executed after the hardware was removed.
