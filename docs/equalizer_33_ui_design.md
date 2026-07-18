# Project 3.3 Status UI Design

## Scope and evidence state

The UI is a Project 3.3-only extension. It does not change the RBJ design,
presets, preamp, 120 ms crossfade, Analyzer math, dynamic-stage parameters,
ADC/DAC/EDMA/PRU, or Project 3.2. Build `5d1525a` is the preserved circular-
shift baseline. Renderer-state hardening is `67a22ef`; bounded Analyzer updates
are `b23a7ce`.

Host contracts and the clean CCS A-E matrix are verified. The static alignment
page passed a ten-minute board run and operator observation on `67a22ef`.
Dynamic Status on the hardened renderer, Chinese glyph appearance under that
load, and physical Touch accuracy remain `PENDING_HARDWARE`.

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
arithmetic to y=124..298, while drawable pixels remain inside y=125..297.
Each band separately tracks requested/displayed pixels and value text.

An Analyzer service performs exactly one operation: clear one old strip, fill
one new strip, or redraw one value field. A strip is at most 16 pixels high.
A cross-zero move first reaches the fixed zero line and then fills the other
side. A newer target replaces unfinished historical motion. Value text changes
when integer dB differs by at least 2 dB, valid/warm changes, or 50 audio frames
have elapsed.

## Audio-first service

LCD service is allowed only after the current AD/DA audio work and only when
all audio/pending flags are clear. Touch, builder, and Analyzer work performed
in the pass prevent an LCD job. The flags are checked again immediately before
drawing and immediately after drawing.

- Maximum work per service: one bounded field or Analyzer strip.
- Preset minimum gap: 2 processed frames, 40.96 ms.
- Dynamic-row minimum gap: 4 processed frames, 81.92 ms.
- Chain and Analyzer minimum gap: 8 processed frames, 163.84 ms.
- Global steady minimum gap: 7 processed frames; average at most 8 jobs/s.
- Control quiet period: 3 processed frames, 61.44 ms.
- Goal threshold: 456000 cycles, 1 ms at 456 MHz.
- Normal acceptance threshold: 912000 cycles, 2 ms at 456 MHz.
- Hard threshold: 2280000 cycles, 5 ms at 456 MHz.

A hard overrun auto-disables runtime LCD jobs instead of delaying audio. A
normal job above 2 ms fails the current stability contract even when it remains
below the 5 ms fail-closed threshold. The historical `5d1525a` run reached
1,800,578 cycles and 149 jobs above 2 ms; it is a failure baseline under the
current contract, not an accepted final result. Dynamic timing for `b23a7ce`
remains `PENDING_HARDWARE`.

## Raster and framebuffer guard

The renderer captures the initialized FB0 base/end and audits them after the
static layout, at low runtime cadence, after an overrun, or on operator request.
Fixed canaries guard the framebuffer boundary. Address mismatch, sync loss,
FIFO underflow, unknown raster fault, or canary failure latches the evidence,
sets the runtime mask to zero, and leaves the audio path active. Runtime code
does not call `Lcd_Init()`, restart raster, clear the full screen, or allocate a
second framebuffer.

## Memory and build gates

`EQ_ENABLE_LCD_DISPLAY=0` removes the runtime UI state and LCD diagnostics.
`EQ_ENABLE_PROJECT33_TOUCH=1` is rejected unless LCD is enabled. The source
defaults remain LCD=0, Touch=0, and runtime mask=0. The current UI state is 312
bytes; touch state plus transform is 36 bytes. The A-E matrix keeps Project 3.3
`.subband_l2` at 20,380 bytes for LCD OFF, static, dynamic, and Touch profiles.

## Offline ten-band editor extension

The values above describe the preserved editor-OFF Status UI. Feature source
`6c3daca0cfd645704446a60c5fe189ffeb0b8645` adds an optional second page behind
`EQ_ENABLE_TEN_BAND_EDITOR`, which still defaults to 0. Editor ON uses 27 jobs:
the original 15, ten applied-gain strip jobs, editor fields, and page tiles.
The editor state is 64 bytes; complete UI and editor state is 620 bytes in the
TI C6000 map, or 656 bytes with Touch state and transform.

Dynamic page state remains independent from editor applied/draft/submitted
state. Page construction is tiled and latest-wins; one service draws one tile
and never clears the full framebuffer. The editor columns render applied gains
only. Snapshot construction is event-driven and limited to one request per
processed frame. Host renderer trace proves fixed bounds and call ordering,
but is an `OFFLINE_RENDER_PREVIEW`, not a photograph or real-LCD pass.
