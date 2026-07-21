# Project 3.3 Ten-Band Editor

## Evidence boundary

The Editor is compile-gated by `EQ_ENABLE_TEN_BAND_EDITOR`, default 0. Editor
Touch additionally requires LCD, Project 3.3 Touch, and
`EQ_ENABLE_TEN_BAND_EDITOR_TOUCH=1`. Host and CCS results do not substitute for
physical operation; current board acceptance remains `PENDING_HARDWARE`.

## Fixed controls

The Editor page contains ten applied-gain bars labeled `31`, `62`, `125`,
`250`, `500`, `1k`, `2k`, `4k`, `8k`, and `16k`; a zero line; selected-band,
draft, applied, CUSTOM, and apply-state fields; minus, plus, APPLY, FLAT, and a
return-page switch.

The duplicated five-preset row is removed. Editor Touch therefore has exactly
15 non-overlapping hitboxes: ten bands, minus, plus, APPLY, FLAT, and page
switch. Dynamic Status retains its independent 12-hitbox table.

## Edit and control state

Gains use signed half-dB units from -36 to +24 (-18 dB to +12 dB).

- `applied_gain_half_db` comes only from the applied generation.
- `draft_gain_half_db` changes locally in 0.5 dB steps.
- `submitted_gain_half_db` identifies the accepted Apply target.

Apply submits one `EQ_CONTROL_SET_ALL` CUSTOM request. FLAT submits
`EQ_CONTROL_RESET_FLAT`. Neither action writes active or pending banks
directly. Existing mailbox, builder, safe-boundary installation, stale-token,
and 120 ms transition contracts remain authoritative.

## Permanent cache behavior

The complete Editor static page is drawn into `EQ_LcdEditorBuffer` before
Raster starts. Runtime updates only selected-band borders, one applied-gain
strip, and the fixed field cells. Static labels, bars, buttons, title, and
background are never rebuilt during a page switch.

While Dynamic Status is visible, Editor changes update only the latest
snapshot and Editor dirty mask. A switch uses zero or more `PAGE_SYNC` jobs to
bring those data regions to the latest state, followed on a later service call
by one `PAGE_SWAP` state. Displayed page changes only after EOF0 and EOF1 have
both converged. The old 18/24 fixed page-tile construction is removed.

The Editor's rendered preset/CUSTOM state is independent from Dynamic Status
preset highlighting, so rendering a preset on one page cannot falsely mark
the other page clean.

## Acceptance still required

The exact loaded build still needs short physical verification of all ten band
selectors, 125 Hz +2 dB Apply, multi-band CUSTOM, FLAT reset, 20 page switches,
Touch coordinates, complete-page swaps, audio continuity, LCD job budget,
cache mode, EOF ring, and all audio/raster fault counters. Endurance and any
burst16/burst8 A/B remain deferred until that short gate passes.
