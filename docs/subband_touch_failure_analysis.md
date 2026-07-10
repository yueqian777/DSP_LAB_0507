# Subband Touch Failure Analysis

## Scope

This change targets the real GT1151 touch path used by the LCD subband UI.
It does not change WOLA, MCRA, Minimum Statistics, codec, ADC, or DAC
algorithm parameters.

## Confirmed Root Causes And Risks

1. The GT1151 status clear call used a null data pointer:
   `GT1151_WR_Reg(GT_GSTID_REG, 0, 1)`.
   `GT1151_WR_Reg` forwards a data pointer to `IIC_SeqWrite_Reg`, whose data
   argument is `unsigned char *send_dat`. Passing `0` means the I2C write path
   can dereference a null pointer instead of writing the one-byte clear value.

2. The scan logic treated exact states such as `0x81` and `0x80` as the only
   meaningful touch states. GT1151 reports bit fields: bit 7 is data-ready and
   bits 3:0 are the touch count. The fixed scanner now decodes
   `ready = State & 0x80` and `touch_count = State & 0x0F`.

3. Waiting only for the falling-edge interrupt can miss release handling for
   widgets using `PB_STYLE_RELEASE_NOTIFY`. A complete button callback needs a
   DOWN event followed by a UP event. The subband flow now adds 20 Hz TIM2
   polling so the main loop keeps scanning for release and delayed data-ready
   states without blocking.

4. The old init path printed a four-byte product ID as a null-terminated string.
   The fixed path uses `unsigned char id[5] = {0U}` and stores the ID in
   `Touch_DebugProductId[5]` for CCS Watch. Init printf is disabled by default
   through `TOUCH_ENABLE_INIT_PRINTF`.

## Fix Summary

- `touch_scan.c/.h`
  - Added `TouchScanResult`.
  - Reads coordinates before clearing `GT_GSTID_REG`.
  - Clears status with a real `clear_value` byte pointer.
  - Returns explicit `NO_DATA`, `DOWN`, `RELEASE`, and `ERROR` states.
  - Keeps `Touch_Sta` unchanged when data-ready is not set.

- `touch_pin.c/.h`
  - ISR only clears interrupt state, increments
    `Touch_DebugInterruptCount`, and sets `FLAG_TOUCH`.
  - No I2C, LCD drawing, printf, or complex logic runs inside the ISR.

- `touch_drv.c/.h`
  - Stores Product ID and config/control write status in watch variables.
  - Disables init printf unless `TOUCH_ENABLE_INIT_PRINTF` is enabled.
  - Uses `AckRolling` to count I2C errors.

- `user_subband_flow.c`
  - Adds `Tim2_Init(TIMER_20HZ)`.
  - Services touch on either `FLAG_TOUCH` or `FLAG_TIM2`.
  - Keeps touch handling in the main loop after audio flag handling.

- `user_subband_ui.c/.h`
  - `SubbandUI_ServiceTouch(unsigned char force_scan)` supports low-frequency
    polling without requiring a new interrupt.
  - Display service draws at most one dirty region per call.
  - Algorithm load display is disabled by default during touch recovery.

- `user_subband_touch_check.c/.h`
  - Adds a standalone touch diagnostic entry selected by
    `SUBBAND_TOUCH_CHECK_ONLY`.
  - Initializes only `Sys_Init`, `Lcd_Init`, `Touch_Init`, and
    `Tim2_Init(TIMER_20HZ)`.
  - Does not initialize ADC, DAC, WOLA, MCRA, codec, or the full subband UI.

## PINMUX And Interrupt Basis

The current repository defines the touch interrupt as GPIO8[10]:

- `TOUCH_INT_PIN = 139`
- `TOUCH_INT_MASK = SYSCFG_PINMUX18_PINMUX18_31_28`
- `TOUCH_INT_ENABLE = ... GPIO8_10 ...`
- PINMUX register: `SYSCFG0_PINMUX(18)`
- GPIO bank interrupt: bank 8
- DSP event: `SYS_INT_GPIO_B8INT`
- CPU mask interrupt: `C674X_MASK_INT13`

LCD headers also define `LCD_MCLK` on PINMUX18[31:28], but the active
`lcd_pin.c` path configures DATA8, DATA9, and PCLK on PINMUX18 and does not
write the MCLK field. The touch code therefore keeps GPIO8[10] on
PINMUX18[31:28] rather than copying another project blindly.

## Reference Code Status

The requested reference file `272b087e-00ba-416f-b480-a5ae531931e1.txt` was
not found in the local attachments or repository. The implementation therefore
uses only the requirements from the task text and existing repository APIs.
Borrowed ideas are limited to GT1151 status decoding, 20 Hz non-blocking
polling, and a raw coordinate self-check page. No equalizer, ADC/DAC flow, KEY
init, or unrelated UI code was copied.

## Coordinate Mapping

No coordinate swap or inversion was added. Raw `Touch_X` and `Touch_Y` are
shown directly in Touch Check and clipped to the expected LCD range
0..799 and 0..479 for the scaled map. Any axis swap or inversion must be based
on measured five-point raw coordinate results.

## Physical Test Status

Automated code and contract checks can verify the scan state machine and
integration shape, but they cannot prove real finger touch. Current physical
status is:

`NOT_MEASURED_PHYSICAL_TOUCH_REQUIRES_OPERATOR`

Run the Touch Check entry on the board and record the raw test CSV before
claiming real-touch pass/fail.
