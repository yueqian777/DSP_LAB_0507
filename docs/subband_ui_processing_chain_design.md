# Subband UI Processing Chain Design

## Scope

This change updates only the LCD UI content and refresh state for the
subband touch page. It does not change WOLA, denoise, MCRA, codec,
ADC/DAC, mode ids, or the 160/240/320 kbps control path.

## Removed Visible Debug Fields

The LCD no longer draws these debug labels:

- `REQ`
- `APPLIED`
- `SEL`
- `TGT`
- `EST`

The backing debug variables remain available for CCS Watch and tests,
including `SUBBAND_DebugDemoMode`, `SUBBAND_DebugAppliedDemoMode`,
`SUBBAND_UI_DebugSelectedCodecKbps`,
`SUBBAND_CODEC_LOOP_DebugTargetKbps`, and
`SUBBAND_CODEC_LOOP_DebugEstimatedBitrateKbps`.

## Chain Source

The displayed processing chain is based on
`SUBBAND_DebugAppliedDemoMode`, not the requested mode or the last
touched button. Codec chain bitrate uses
`SUBBAND_CODEC_LOOP_DebugTargetKbps` when it is one of 160, 240, or
320 kbps. If the target is not valid immediately after entering a codec
mode, the display falls back to `SUBBAND_DebugPersistentCodecKbps`.

## Chain Mapping

| Mode | Displayed chain |
| --- | --- |
| 0 | `CHAIN: ADC > RAW > DAC` |
| 1 | `CHAIN: ADC > WOLA ANA` / `WOLA SYN > DAC` |
| 2 | `CHAIN: ADC > WOLA ANA > BASIC NR` / `WOLA SYN > DAC` |
| 4 | `CHAIN: ADC > WOLA ANA > MINSTAT NR` / `WOLA SYN > DAC` |
| 6 | `CHAIN: ADC > WOLA ANA > MCRA NR` / `WOLA SYN > DAC` |
| 7 | `CHAIN: ADC > WOLA ANA > STRONG MCRA NR` / `WOLA SYN > DAC` |
| 8 | `CHAIN: ADC > WOLA ANA > MCRA NR` / `CODEC XXX kbps > WOLA SYN > DAC` |
| 11 | `CHAIN: ADC > WOLA ANA` / `CODEC XXX kbps > WOLA SYN > DAC` |

ASCII is used for the chain to avoid adding large bitmap font tables
and to keep draw time bounded.

## Layout

The chain uses a fixed rectangle in the top status panel:

- `UI_CHAIN_X = 28`
- `UI_CHAIN_Y0 = 80`
- `UI_CHAIN_Y1 = 100`
- `UI_CHAIN_X_MAX = 760`

Only this rectangle is cleared when the chain changes. The chain redraw
does not redraw mode buttons, bitrate buttons, progress blocks, or the
learning panel.

## Dirty Flags

`UI_DIRTY_CHAIN` is independent from status, rate, progress, countdown,
and load. The chain is marked dirty only when:

- `SUBBAND_DebugAppliedDemoMode` changes.
- Applied mode is 8 or 11 and the effective target kbps changes.
- The page is initialized or a full redraw is requested.

Learning progress, algorithm load, estimated bitrate, AD/DA counters,
and timer ticks do not independently refresh the chain.

## Diagnostics

The UI exposes these CCS Watch variables:

- `SUBBAND_UI_DebugDisplayedChainMode`
- `SUBBAND_UI_DebugDisplayedChainKbps`
- `SUBBAND_UI_DebugChainRefreshCount`
- `SUBBAND_UI_DebugLastChainDrawCycles`
- `SUBBAND_UI_DebugMaxChainDrawCycles`

Board-side cycle and audio results require manual testing on the
TMS320C6748 board.
