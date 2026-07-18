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

