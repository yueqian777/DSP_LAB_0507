# Project 3.2 Single-Line Processing Chain

The bundled grlib fonts are ASCII fonts. A Unicode arrow is a multi-byte UTF-8
sequence and `GrStringDraw()` does not map it to a glyph, so every chain uses
the plain ASCII separator ` -> `. No arrow bitmap, vector animation, or extra
font resource is added.

The chain now occupies one bounded rectangle, `(28,80)-(770,106)`, instead of
two rows. `SubbandUI_BuildProcessingChain()` builds into a fixed 96-byte buffer
using bounded copy/append helpers. Invalid state falls back to
`ADC -> PROCESSING -> DAC`. `UI_SelectChainFont()` checks the actual pixel width
with `GrStringWidthGet()`, reuses the already linked 18b UI font when it fits,
and falls back to the existing 12 pixel Cmtt font. The longest mode 8 chain is
719 pixels in Cmtt12, below the 742-pixel limit. It never wraps or removes
`WOLA SYN` or `DAC`.

| Mode | Display |
|---:|---|
| 0 | `ADC -> RAW -> DAC` |
| 1 | `ADC -> WOLA ANA -> WOLA SYN -> DAC` |
| 2 | `ADC -> WOLA ANA -> BASIC NR -> WOLA SYN -> DAC` |
| 4 | `ADC -> WOLA ANA -> MINSTAT NR -> WOLA SYN -> DAC` |
| 6 | `ADC -> WOLA ANA -> MCRA NR -> WOLA SYN -> DAC` |
| 7 | `ADC -> WOLA ANA -> STRONG MCRA -> WOLA SYN -> DAC` |
| 8 | `ADC -> WOLA ANA -> MCRA NR -> CODEC 240k -> WOLA SYN -> DAC` |
| 11 | `ADC -> WOLA ANA -> CODEC 240k -> WOLA SYN -> DAC` |

Modes 8 and 11 replace `240k` dynamically with the applied `160k`, `240k`, or
`320k` target. A target change also updates the selected rate-button color from
the applied codec target, including a target applied through CCS Watch.

The old second line could be hidden at the bottom of the top panel and required
two text draws plus a taller clear rectangle. The one-line job clears only its
own rectangle and calls `GrStringDraw()` once.
