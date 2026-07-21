# Project 3.3 Status UI Design

## Scope and evidence

The LCD work is isolated to Project 3.3. It does not change the 50 kHz audio
path, frame size, RBJ filters, preset gains, headroom, transitions, Analyzer
math, dynamic-stage parameters, ADC/DAC/EDMA/PRU, or Project 3.2.

The current implementation is Host and CCS build testable. Physical LCD,
Touch, audio-continuity, and endurance acceptance remain `PENDING_HARDWARE`
until the exact build is loaded and observed without debugger halts.

## Permanent page caches

Editor builds own two fixed DDR2 framebuffers:

- Dynamic Status: `Lcd_Buffer`.
- EQ Editor: `EQ_LcdEditorBuffer`.

Before Raster is enabled, the renderer draws both complete pages, initializes
their rendered state, and points FB0 and FB1 at Dynamic Status. Runtime never
rebuilds a page title, background, static border, frequency label, or module
name. Each framebuffer remains the permanent cache for its page.

Hidden-page state is latest-wins. A new snapshot overwrites the previous
request and recomputes dirty regions against that page's rendered state. A
hidden page is normally not drawn. Immediately before a switch, `PAGE_SYNC`
updates at most one still-dirty target-page data region per service call. Once
the target is clean, the next service call runs `PAGE_SWAP`; it draws nothing
and only starts or observes the descriptor swap.

`page_requested_version[2]` and `page_rendered_version[2]` make this contract
observable. A rendered version advances only when that page has no dirty data
regions.

## Dynamic Status

The fixed 800x480 page contains:

- FLAT, BASS, VOCAL, TREBLE, and V-SHAPE preset controls.
- BASS, MUD, PRES, and HIGH Analyzer bars with a fixed zero line.
- SMART BASS, CLARITY, and HF GUARD rows.
- ON/OFF, LOW/MID/HIGH, and an ACTIVE/IDLE marker for each dynamic stage.
- one EDITOR page switch.

The old ADC/EQ/BASS/CLR/HF/DAC chain, geometric arrows, Analyzer dB text,
left-side +20/0/-20 text, and dynamic Level numbers are removed. Analyzer
runtime work is one differential strip of at most 16 pixels. Invalid or
non-warm snapshots clear the bar to the zero line without drawing a numeric
value.

Dynamic Status has 12 non-overlapping hitboxes: five presets, three toggles,
three strength controls, and the page switch.

## Jobs and scheduling

Editor OFF has 12 jobs:

- 1-5: preset highlight regions.
- 6-9: Analyzer bars.
- 10-12: dynamic rows.

Editor ON has 25 jobs:

- 1-12: Dynamic Status data jobs.
- 13-22: ten Editor applied-gain bars.
- 23: one Editor field region.
- 24: `EQ_UI_JOB_PAGE_SYNC`.
- 25: `EQ_UI_JOB_PAGE_SWAP`.

There are no chain jobs and no fixed page-tile counts. Runtime service remains
audio-first and performs at most one bounded LCD job after AD/DA work and only
when all audio flags are clear. The normal acceptance limit remains 912000
cycles (2 ms at 456 MHz); the threshold is not raised to hide a large region.

## EOF publication

LCDC remains in double-frame mode. FB0 and FB1 initially reference Dynamic
Status. EOF0 updates only FB0 and EOF1 updates only FB1. The software displayed
page changes only after both descriptors reference the same target. A request
that arrives after one descriptor has changed is retained as the latest
deferred page and is applied only after the current swap converges.

All Touch actions, including another page-switch action, are rejected while
`PAGE_SYNC` or `PAGE_SWAP` is active. The target page becomes touchable only
after descriptor convergence and logical publication.

## Cache visibility

Both framebuffer addresses are classified through their actual C6748 MAR
registers. `EQ_DebugLcdCacheMode` reports unknown, non-cacheable, cacheable, or
mixed. The raw MAR values are retained separately for both buffers.

For non-cacheable buffers, no cache API is called. For cacheable buffers, the
renderer calls the StarterWare `CacheWB` API for the real dirty bounding range
before a later `PAGE_SWAP` can set `swap_pending`. Startup writes back each
complete DMA frame, including palette bytes. Count, byte, and failure
diagnostics are exported. No MAR setting is changed by this feature.

## Fault evidence

A 64-entry fixed ring records cycle, process frame, EOF mask, FB0/FB1 base,
front page, target page, pending state, descriptor mask, completion state, and
raster status. The ISR performs only fixed-size assignments; it does not print
or allocate memory. The ring is intended for one debugger read after a run.

Existing exact-address, sync-loss, FIFO-underflow, canary, bounds, ambiguous
EOF, and raster-stop diagnostics remain. Burst16 and FIFO threshold8 are
unchanged. Burst8 A/B is permitted only after new physical FUF/SYNC evidence.
