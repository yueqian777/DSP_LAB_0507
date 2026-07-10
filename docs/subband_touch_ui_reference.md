# Project 3.2 LCD touch UI references

## Local primary sources

- `Code/Driver/11_lcd/lcd_dma.h`: authoritative 800 x 480 dimensions.
- `Code/Driver/11_lcd/lcd_grlib.c`: RGB565 off-screen framebuffer initialization.
- `Code/Driver/12_touch/touch_drv.c`: GT1151 800 x 480 controller configuration.
- `Code/Driver/12_touch/touch_scan.c`: raw coordinate acquisition and direct grlib
  dispatch.
- `Code/Driver/12_touch/touch_btn.c`: existing press/release latch and widget queue.
- `Code/User/user_driver/11_lcd/user_lcd.c` and
  `Code/User/user_driver/12_touch/user_touch.c`: board examples from the current BSP.
- Experiment guide sections 1.7.10 and 1.7.11: required LCD and touch API sequence.

These local sources define the actual board wiring and API behavior and take
precedence over generic examples.

## TI official references

- [TMS320C6748 DSP data sheet](https://www.ti.com/lit/ds/symlink/tms320c6748.pdf):
  device capabilities, memory system, clocking, LCDC, and electrical context.
- [TMS320C6748 DSP Technical Reference Manual, SPRUH79A](https://www.ti.com/lit/ug/spruh79a/spruh79a.pdf):
  LCD controller architecture and DMA/raster behavior.
- [Processor SDK RTOS foundational components](https://software-dl.ti.com/processor-sdk-rtos/esd/docs/06_03_00_106/rtos/index_Foundational_Components.html):
  TI software stack and StarterWare relationship.
- [TI E2E Processors forum](https://e2e.ti.com/support/processors-group/processors/f/processors-forum):
  issue cross-checking for C6748 LCDC/StarterWare behavior. No forum code was copied.

The implementation deliberately reuses the repository's StarterWare grlib and board
BSP. It does not import controller code, coordinate transforms, debounce algorithms,
or widget layouts from unrelated blog projects.

## Verification scope

The official documents were used to confirm platform constraints. Resolution,
orientation, touch range, and color format were derived from the exact checked-in
BSP because those are board-specific. Internet access to TI PDF endpoints was slow
during implementation; the stable TI publication links above are retained for
reproducibility.

