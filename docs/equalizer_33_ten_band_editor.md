# Project 3.3 Ten-Band Editor

## Evidence boundary

The implementation baseline is `b67f755`; the final editor feature source is
`6c3daca0cfd645704446a60c5fe189ffeb0b8645`. The editor is complete for Host
contracts, deterministic PCM16 processing, renderer trace, and CCS clean-build
inspection. It has not been loaded or operated on a board in this phase.

`EQ_ENABLE_TEN_BAND_EDITOR` defaults to 0. Editor Touch additionally requires
LCD, Project 3.3 Touch, and `EQ_ENABLE_TEN_BAND_EDITOR_TOUCH=1`. The production
default therefore retains the pre-editor Dynamic Status page and behavior.

## Pages and fixed geometry

`EQ_UI_PAGE_DYNAMIC_STATUS=0` remains the startup page.
`EQ_UI_PAGE_EQ_EDITOR=1` contains the five fixed presets, ten applied-gain
columns, selected band, draft/applied values, minus, plus, Apply, Reset Flat,
apply state, and a page switch. The ten labels are `31`, `62`, `125`, `250`,
`500`, `1k`, `2k`, `4k`, `8k`, and `16k`.

Dynamic Status has 12 hitboxes: five presets; Smart, Clarity, and Guard toggle
and strength controls; and page switch `(670,80,104,28)`. The editor has 20:
the same five presets; ten fixed band rectangles starting at x=24 with width
68, height 218, and 76-pixel pitch; minus `(24,344,104,42)`; plus
`(140,344,104,42)`; Apply `(500,344,120,42)`; Reset Flat
`(632,344,144,42)`; and the page switch. Host geometry tests prove each page is
in 800x480 bounds and has no same-page overlap.

## State model

All editable gains use signed half-dB units. The valid integer range is -36 to
+24, representing -18 dB to +12 dB. The editor owns three ten-byte arrays:

- `applied_gain_half_db`: refreshed only from the active generation and
  applied sequence; the ten visible columns use only this array.
- `draft_gain_half_db`: changed locally by band selection and +/- 0.5 dB.
  Draft changes do not submit control work or change the active bank.
- `submitted_gain_half_db`: the last accepted Apply target and the reference
  for queued, building, ready, transition, applied, and error states.

CUSTOM remains distinct from FLAT. A dirty draft keeps the currently applied
preset highlighted. CUSTOM is shown as applied only after the submitted
sequence completes its 120 ms bank transition. Applying a fixed preset
resynchronizes draft and submitted values to its active gains.

## Control path

Apply converts the ten half-dB values to a temporary `float[10]` and submits
one `EQ_CONTROL_SET_ALL` request with `EQ_PRESET_CUSTOM`. Reset submits
`EQ_CONTROL_RESET_FLAT`. Neither path calls the builder, writes `EQ_STATE`, or
changes active/pending banks directly:

```text
Touch -> local draft -> Apply -> mailbox -> shadow target
      -> incremental builder -> ready candidate -> safe-boundary install
      -> 120 ms transition -> applied sequence -> applied columns
```

Rapid requests use the existing latest-wins token and stale-candidate rules.
The deterministic stress run observed stale work and cancellation while
recording zero stale installs.

## Rendering and scheduling

Editor builds contain 27 dirty jobs: the original 15, ten applied-band jobs,
one editor-fields job, and one page-tile job. A compile-time assertion keeps
the count within the 32-bit mask. Dynamic and editor page builds use 15 and 18
small tiles respectively; one service draws one tile, never a runtime full
clear. A newer requested page cancels unfinished tiles, and displayed page is
updated only after its final tile.

Snapshot construction is event driven. Analyzer count, active generation,
applied/submitted sequence, draft version, dynamics, selected band, apply
state, and page are versioned. `EQ_UiSnapshotLastRequestFrame` also limits
requests to one per processed frame; a Touch event after that request is
deferred to the next frame. Audio and frame service remain ahead of Touch,
builder, Analyzer, LCD, and UART work.

## Remaining hardware work

The historical static alignment page on `67a22ef` passed ten minutes and an
operator visual check. That result does not validate this editor. Dynamic
Status, physical Touch, CUSTOM Apply/Reset, real LCD glyphs and columns,
page switching, LCD timing, raster guards, and combined ten-minute endurance
all remain `PENDING_HARDWARE`.
