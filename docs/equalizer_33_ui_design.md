# Project 3.3 Status UI Design

## Scope and evidence state

The UI is a Project 3.3-only extension. It does not change the RBJ design,
presets, preamp, 120 ms crossfade, Analyzer math, dynamic-stage parameters,
ADC/DAC/EDMA/PRU, or Project 3.2. The implementation build tested on the board
is `5d1525ae984acb53765e00cf678874189d2aaf30`, dirty=0.

Host contracts, the CCS A-E matrix, and a 60-second objective board stress are
verified. Visual alignment, Chinese glyph appearance, and physical touch
accuracy remain operator checks.

## Fixed 800x480 layout

The page has five fixed bands:

1. Static title: ten-band dynamic equalizer system.
2. Presets: FLAT, BASS, VOCAL, TREBLE, and V-SHAPE.
3. One-line chain: `ADC -> EQ -> BASS -> CLR -> HF -> DAC`.
4. Four Analyzer slots: Bass, Mud, Presence, and Brightness.
5. Three dynamic rows: Smart Bass, Dynamic Clarity, and Harshness Guard.

The preset rectangles are `(26,34,140,40)`, `(178,34,140,40)`,
`(330,34,140,40)`, `(482,34,140,40)`, and `(634,34,140,40)`. Analyzer
slots are `(60,116,125,190)`, `(245,116,125,190)`, `(430,116,125,190)`,
and `(615,116,125,190)`. Dynamic rows start at y=342, 386, and 430.

Chinese text uses the local 16x16 bitmap renderer and is enabled by default
with `EQ_LCD_USE_CHINESE=1`. Source control strings remain ASCII-only. The
English renderer remains build-tested with `EQ_LCD_USE_CHINESE=0`. Chain
arrows are always ASCII `->`.

## Static and runtime boundary

`EqualizerDisplay_DrawStaticLayout()` completes before `Adc_Start()` and
`Dac_Start()`. It draws the background, title, fixed labels, button frames,
chain arrows, Analyzer frames/zero marks, and dynamic-row names once.

After `EqualizerDisplay_BeginRuntime()`, a static-layout request cannot draw;
it increments `EQ_DebugLcdUnexpectedFullRedrawCount`. Runtime rendering only
clears and redraws one bounded region. Request functions compare snapshots and
set dirty bits; they never call an LCD primitive.

## Dirty jobs

There are 15 logical jobs:

- Jobs 1-5: one job for each applied-preset button.
- Jobs 6-8: BASS, CLR, and HF chain segments.
- Jobs 9-12: one job for each Analyzer bar.
- Jobs 13-15: Smart Bass, Clarity, and Guard rows.

Each dynamic-row job carries independent ENABLED, STRENGTH, and LEVEL field
bits. The renderer completes at most one field per service call, so a row with
three changed values requires three bounded services. Displayed state advances
only after the corresponding region is drawn. A new snapshot replaces an
undrawn old request rather than accumulating history.

Applied preset controls highlighting; requested preset never highlights
early. Analyzer values are clamped to -20..+20 dB and mapped with integer
arithmetic to y=124..298. A one-pixel change is suppressed; a two-pixel change
or the 50-frame maximum age may request that bar only.

## Audio-first service

LCD service is allowed only after the current AD/DA audio work and only when
all audio/pending flags are clear. Touch, builder, and Analyzer work performed
in the pass prevent an LCD job. The flags are checked again immediately before
drawing and immediately after drawing.

- Maximum work per service: one fixed-region job.
- Minimum gap: 2 processed frames, 40.96 ms at 50 kHz/1024 samples.
- Control quiet period: 3 processed frames, 61.44 ms.
- Warning threshold: 912000 cycles, 2 ms at 456 MHz.
- Hard threshold: 2280000 cycles, 5 ms at 456 MHz.

A hard overrun auto-disables runtime LCD jobs instead of delaying audio. The
final board run measured a worst job of 1800578 cycles (3.949 ms), no hard
overrun, no auto-disable, and no unexpected full redraw.

## Memory and build gates

`EQ_ENABLE_LCD_DISPLAY=0` removes the runtime UI state and LCD diagnostics.
`EQ_ENABLE_PROJECT33_TOUCH=1` is rejected unless LCD is enabled. The source
defaults remain LCD=0, Touch=0, and runtime mask=0. The measured E profile uses
244 bytes for UI state and 36 bytes for touch state/transform.

