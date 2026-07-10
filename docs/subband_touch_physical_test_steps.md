# Subband Touch Physical Test Steps

## Build Selection

For raw GT1151 validation, build with:

```c
#define SUBBAND_TOUCH_CHECK_ONLY 1
```

The Touch Check screen should show:

- Touch Check
- INT count
- Scan count
- Raw state
- Ready count
- No-data count
- Down count
- Release count
- I2C error count
- Touch_Sta
- Touch_X
- Touch_Y

## Raw Touch Check

1. Flash and run the Touch Check build.
2. Confirm the LCD page appears and no ADC/DAC audio flow starts.
3. Touch the upper-left, upper-right, lower-left, lower-right, and center
   positions.
4. For each point, record `INT count`, `Raw state`, `Touch_Sta`, `Touch_X`,
   `Touch_Y`, `Down count`, `Release count`, and `I2C error count`.
5. Confirm press sets `Touch_Sta=1`.
6. Confirm release sets `Touch_Sta=0` and increments `Release count`.
7. Confirm X/Y move consistently with finger position.
8. If X/Y are swapped or inverted, record the raw values first; do not change
   mapping by guesswork.

## Reliability Checks

1. Tap the screen 20 times at one visible point.
2. Long-press for 2 seconds, then release.
3. Confirm repeated taps increment both down and release counts.
4. Confirm the long press does not repeatedly trigger button-style actions.
5. Confirm `I2C error count` does not continuously increase.

## Full Subband UI Checks

After raw Touch Check is recorded, build with:

```c
#define SUBBAND_TOUCH_CHECK_ONLY 0
```

Then test:

1. Tap each of the 8 mode buttons once.
2. Confirm requested mode and applied mode update correctly.
3. In Mode 11, tap 160/240/320 kbps.
4. In Mode 8, tap 160/240/320 kbps.
5. Confirm changing bitrate outside an active codec mode is remembered but
   does not restart learning.
6. Confirm AD/DA/algo frame counters continue increasing during touch use.
7. Let the UI run for at least 5 minutes and check for touch loss or UI drift.

## Result Marker

Until a human operator performs the physical touch actions above, record:

`NOT_MEASURED_PHYSICAL_TOUCH_REQUIRES_OPERATOR`

DSS or watch-variable flag injection may be recorded separately as
`SIMULATED_FLAG_INJECTION`, but it must not be used as evidence that real
finger touch works.
