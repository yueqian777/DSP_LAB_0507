# Project 3.2 LCD touch UI design

## Hardware facts

- LCD framebuffer: 800 x 480, from `lcd_dma.h` and the LCD raster configuration.
- Pixel format: RGB565 through `GrOffScreen16BPPInit`.
- Touch controller: GT1151 configuration contains 800 x 480 (`0x0320`, `0x01e0`).
- Coordinates: the driver passes raw `Touch_X/Touch_Y` directly to grlib, with no
  swap, mirror, scale, or accumulated offset. The UI therefore uses X=0..799 and
  Y=0..479 in the same top-left coordinate system as the existing widgets.

## Fixed layout

| Region | Coordinates |
|---|---|
| Title | (0,0)-(799,39) |
| Current status | (16,44)-(783,115) |
| Mode row 1 | X=16/212/408/604, Y=124, W=180, H=52 |
| Mode row 2 | X=16/212/408/604, Y=188, W=180, H=52 |
| Codec rates | X=116/316/516, Y=258, W=168, H=46 |
| Learning status | (16,316)-(783,383) |
| Progress bar | (24,390)-(775,409) |
| Algorithm load | (16,420)-(783,463) |

Every draw creates a local rectangle from constants. Static background drawing is
restricted to initialization. Runtime refreshes use five dirty bits and process at
most two regions per service call.

## Mode mapping

| Button flag | Mode | Display |
|---|---:|---|
| `FLAG_BUTTON_1` | 0 | 原始直通 |
| `FLAG_BUTTON_2` | 1 | WOLA分析合成 |
| `FLAG_BUTTON_3` | 2 | 基础自适应降噪 |
| `FLAG_BUTTON_4` | 4 | 最小统计降噪 |
| `FLAG_BUTTON_5` | 6 | MCRA智能降噪 |
| `FLAG_BUTTON_6` | 7 | 强力MCRA降噪 |
| `FLAG_BUTTON_7` | 8 | MCRA降噪+感知压缩 |
| `FLAG_BUTTON_8` | 11 | 感知压缩 |

Modes 3, 5, 9, and 10 remain available through CCS Watch. The touch page does not
expose them. The backend remains WOLA; FIR and project-1 polyphase test backends are
unchanged.

## Touch state machine

`FLAG_TOUCH` is cleared before `Touch_Scan()`. Existing grlib pointer handling emits
one down and one release event. The eight release callbacks set the eight button
flags. The 160/240/320 kbps rectangles are hit-tested from `Touch_X/Touch_Y` and use
an extra press-release latch. A held press cannot be accepted again until release.
Clicking the already applied mode is ignored.

The UI only writes `SUBBAND_DebugDemoMode` for mode requests. Reset and learning
operations remain owned by `Subband_Service_Demo_Mode()`.

## Codec target persistence

`SUBBAND_DebugPersistentCodecKbps` defaults to 240 and accepts only 160, 240, or
320. Modes 8 and 11 apply it on entry. While either mode is running,
`Subband_Request_Codec_Target()` forwards a new value through
`SUBBAND_CODEC_LOOP_DebugRequestedTargetKbps`; it does not re-enter the mode, reset
WOLA, restart learning, or stop ADC/DAC. Other modes retain the selection but render
the rate buttons disabled.

## Learning countdown

Modes 2, 4, 6, 7, and 8 read the actual `Learning`, `LearnHops`, `TargetHops`, and
`Ready` variables. Remaining time is computed without blocking:

`ceil(max(TargetHops - LearnHops, 0) * HOP * 1000 / sample_rate)`

The state is checked every five 1024-sample audio frames. The codec/load area is
checked every 25 frames. No UI work runs in an ISR or any WOLA, denoise, codec, or
`Subband_Process_1024()` function.

## Chinese text and storage

The bundled grlib fonts are ASCII-only. `tools/generate_subband_ui_font.py` generates
only the 16 Chinese phrases used by this page from Microsoft YaHei. The packed
1-bit static bitmaps occupy 1,984 bytes and require no heap allocation.

## Real-time scheduling

The main-loop order is mode/backend requests, ADC frame, DAC frame, keys, touch,
then display. Display service runs only when no AD, DA, or pending processed frame is
present. `SUBBAND_UI_SHOW_ALGO_LOAD` can disable the optional integer algorithm-load
display without removing mode, rate, or countdown controls.

