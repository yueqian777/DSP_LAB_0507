# Project 3.3 Touch Contract

## Build and ownership

Touch is compile-gated by `EQ_ENABLE_PROJECT33_TOUCH`, default 0. Touch ON
requires `EQ_ENABLE_LCD_DISPLAY=1`. The Touch ISR only sets `FLAG_TOUCH`; the
audio-first main loop performs at most one scan/action in a processed frame.

Preset actions submit through the existing Project 3.3 request/mailbox path.
Dynamic actions change only the existing runtime enable/strength requests.
They do not write active/pending banks, levels, filter state, transitions,
tokens, builder state, or applied mode.

## Coordinate transform

All actions use `EqualizerUi_MapTouchRawToScreen()`. The default transform is:

```text
raw X: 0..799
raw Y: 0..479
swap XY: 0
flip X: 0
flip Y: 0
screen clamp: 800x480
```

The six values are compile-time overridable. No individual hitbox applies its
own offset, swap, or inversion.

## Hitboxes

| Action | Rectangle x,y,w,h |
|---|---|
| Preset FLAT | 26,34,140,40 |
| Preset BASS | 178,34,140,40 |
| Preset VOCAL | 330,34,140,40 |
| Preset TREBLE | 482,34,140,40 |
| Preset V-SHAPE | 634,34,140,40 |
| Smart toggle | 166,342,96,40 |
| Smart strength | 286,342,96,40 |
| Clarity toggle | 166,386,96,40 |
| Clarity strength | 286,386,96,40 |
| Guard toggle | 166,430,96,40 |
| Guard strength | 286,430,96,40 |

Host validation checks all 11 rectangles are in bounds and non-overlapping.
Strength cycles LOW -> MEDIUM -> HIGH -> LOW and is independent of enable.

## Press lifecycle

The first DOWN latches one action. Repeated DOWN samples while latched produce
no action. A RELEASE clears the latch and is required before another action.
An out-of-hitbox point increments the rejected count. The flow also rejects a
second accepted action in the same processed frame.

The board stress did not request operator touch. Nevertheless, the controller
reported 10 accepted and 6 rejected actions during the final 60-second window.
A post-run snapshot reported raw/screen `(678,37)`, last action V-SHAPE, 95
DOWN samples, 51 RELEASE samples, and zero I2C errors. Because no operator
confirmed those presses, their source is unresolved and they are not evidence
of physical hitbox accuracy. Visual and physical-touch validation remains
`PENDING_OPERATOR_TOUCH_VALIDATION`.

## Optional editor page

Feature source `6c3daca0cfd645704446a60c5fe189ffeb0b8645` makes hit testing page-aware.
The preserved Dynamic Status page now has 12 hitboxes, adding the fixed page
switch. Editor builds add a separate 20-hitbox table: five presets, ten band
selectors, minus, plus, Apply, Reset Flat, and page switch. Cross-page
coordinate reuse is permitted; each individual table is bounded and
non-overlapping.

Band selectors change only the selected index. Minus and plus change the local
draft by one half-dB step and retain the existing press/release latch, so a
held press cannot repeat. Apply submits one SET_ALL request and Reset submits
RESET_FLAT. While hidden-page regions are incomplete, old-page content actions
are rejected and counted. Page switch is latest-wins until descriptor
publication starts; after one DMA descriptor has changed, the latest request is
retained and applied only after both descriptors converge at EOF boundaries.

`EQ_ENABLE_TEN_BAND_EDITOR_TOUCH` defaults to 0 and is rejected unless both
the editor and Project 3.3 Touch are enabled. These Host contracts do not prove
the physical transform, hitbox feel, controller noise behavior, or actual LCD
alignment; those remain `PENDING_HARDWARE`.
