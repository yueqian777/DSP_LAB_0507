# Project 3.2 memory audit

## 1. Production firmware result

- Commit: `c07c53e6ca726dfc0877d0d7ac62324b62ecff7e`; build identity `P33_FIX_V5 / c07c53e / dirty=0`.
- Configuration: CCS `Debug` configuration, TI C6000 v8.5.0.LTS, `-O3`.
- Production macros: `DSP_LAB_PROJECT_SELECT=32`, `SUBBAND_THD_BOARD_TEST=0`, `EQ_USE_GENERATED_BUILD_ID=1`.
- Production clean/full link: `link_errors=0x0`; linked at Wed Jul 15 13:51:36 2026.
- The total below is linker-mapped run-time memory. The `.out` file size is deliberately not used.

### PPT-ready overall table

| Category | Usage | Note |
|---|---|---|
| Program code and read-only constants | 136.496 KiB | .text/.const/.switch/.cinit etc. |
| Static read/write data | 1535.298 KiB | .subband_l2/.far/.bss/frame buffers etc. |
| Stack reservation | 2.000 KiB | Linker reservation, not measured peak |
| Heap reservation | 0.000 KiB mapped | 2.000 KiB linker option; .sysmem not instantiated |
| Production total run-time RAM | 1673.794 KiB | Excludes THD test-only capture |
| Highest general-region utilization | 98.085% | DSPL2RAM, 4.902 KiB free |

Normal Project 3.2 production uses **136.496 KiB** for code/read-only content and **1535.298 KiB** for static run-time data. The highest general-purpose region utilization is **DSPL2RAM 98.085%**, leaving **4.902 KiB**. Fixed AD/DA linker windows are intentionally 100% occupied and are shown separately in the complete region table.

## 2. Code/read-only and static RAM boundaries

- Code/read-only: 139772 bytes (136.496 KiB).
- Static read/write RAM: 1572145 bytes (1535.298 KiB), including the normal LCD off-screen buffer and fixed board ADC/DAC buffers.
- Stack mapped reservation: 2048 bytes (2.000 KiB). This is not a measured peak.
- Heap: linker option `--heap_size=0x800` configures 2048 bytes, while the production map has `.sysmem=0`; therefore mapped heap reservation is 0 bytes and no 2 KiB is added to run-time RAM.
- Total mapped run-time RAM: 1713965 bytes (1673.794 KiB). Load/run addresses are not double-counted.

## 3. Memory regions

| Region | Origin | Capacity KiB | Used KiB | Free KiB | Utilization |
|---|---|---|---|---|---|
| DSPL2ROM | 0x00700000 | 1024.000 | 0.000 | 1024.000 | 0.000% |
| DSPL2RAM | 0x00800000 | 256.000 | 251.098 | 4.902 | 98.085% |
| DSPL1PRAM | 0x00e00000 | 32.000 | 0.000 | 32.000 | 0.000% |
| DSPL1DRAM | 0x00f00000 | 32.000 | 0.000 | 32.000 | 0.000% |
| SHDSPL2ROM | 0x11700000 | 1024.000 | 0.000 | 1024.000 | 0.000% |
| SHDSPL2RAM | 0x11800000 | 256.000 | 0.000 | 256.000 | 0.000% |
| SHDSPL1PRAM | 0x11e00000 | 32.000 | 0.000 | 32.000 | 0.000% |
| SHDSPL1DRAM | 0x11f00000 | 32.000 | 0.000 | 32.000 | 0.000% |
| EMIFACS0 | 0x40000000 | 524288.000 | 0.000 | 524288.000 | 0.000% |
| EMIFACS2 | 0x60000000 | 32768.000 | 0.000 | 32768.000 | 0.000% |
| EMIFACS3 | 0x62000000 | 32768.000 | 0.000 | 32768.000 | 0.000% |
| EMIFACS4 | 0x64000000 | 32768.000 | 0.000 | 32768.000 | 0.000% |
| EMIFACS5 | 0x66000000 | 32768.000 | 0.000 | 32768.000 | 0.000% |
| User_Buf1 | 0x80000000 | 32.000 | 0.000 | 32.000 | 0.000% |
| User_Buf2 | 0x80008000 | 32.000 | 0.000 | 32.000 | 0.000% |
| SHRAM | 0x80010000 | 64.000 | 0.000 | 64.000 | 0.000% |
| DDR2 | 0xc0000000 | 114687.984 | 910.688 | 113777.297 | 0.794% |
| User_ADLen | 0xc6fffff0 | 0.002 | 0.000 | 0.002 | 0.000% |
| User_ADEn | 0xc6fffff2 | 0.001 | 0.001 | 0.000 | 100.000% |
| User_ADFlag | 0xc6fffff3 | 0.001 | 0.001 | 0.000 | 100.000% |
| User_ADPingPong | 0xc6fffff4 | 0.001 | 0.001 | 0.000 | 100.000% |
| User_DALen | 0xc6fffff8 | 0.002 | 0.002 | 0.000 | 100.000% |
| User_DAEn | 0xc6fffffa | 0.001 | 0.001 | 0.000 | 100.000% |
| User_DAFlag | 0xc6fffffb | 0.001 | 0.001 | 0.000 | 100.000% |
| User_DAPingPong | 0xc6fffffc | 0.001 | 0.001 | 0.000 | 100.000% |
| User_DASel | 0xc6fffffd | 0.001 | 0.001 | 0.000 | 100.000% |
| AD_Ch1Buf0 | 0xc7000000 | 16.000 | 16.000 | 0.000 | 100.000% |
| AD_Ch2Buf0 | 0xc7004000 | 16.000 | 16.000 | 0.000 | 100.000% |
| AD_Ch3Buf0 | 0xc7008000 | 16.000 | 16.000 | 0.000 | 100.000% |
| AD_Ch4Buf0 | 0xc700c000 | 16.000 | 16.000 | 0.000 | 100.000% |
| AD_Ch5Buf0 | 0xc7010000 | 16.000 | 16.000 | 0.000 | 100.000% |
| AD_Ch6Buf0 | 0xc7014000 | 16.000 | 16.000 | 0.000 | 100.000% |
| AD_Ch7Buf0 | 0xc7018000 | 16.000 | 16.000 | 0.000 | 100.000% |
| AD_Ch8Buf0 | 0xc701c000 | 16.000 | 16.000 | 0.000 | 100.000% |
| AD_Ch1Buf1 | 0xc7020000 | 16.000 | 16.000 | 0.000 | 100.000% |
| AD_Ch2Buf1 | 0xc7024000 | 16.000 | 16.000 | 0.000 | 100.000% |
| AD_Ch3Buf1 | 0xc7028000 | 16.000 | 16.000 | 0.000 | 100.000% |
| AD_Ch4Buf1 | 0xc702c000 | 16.000 | 16.000 | 0.000 | 100.000% |
| AD_Ch5Buf1 | 0xc7030000 | 16.000 | 16.000 | 0.000 | 100.000% |
| AD_Ch6Buf1 | 0xc7034000 | 16.000 | 16.000 | 0.000 | 100.000% |
| AD_Ch7Buf1 | 0xc7038000 | 16.000 | 16.000 | 0.000 | 100.000% |
| AD_Ch8Buf1 | 0xc703c000 | 16.000 | 16.000 | 0.000 | 100.000% |
| DA_Ch1Buf0 | 0xc7040000 | 16.000 | 16.000 | 0.000 | 100.000% |
| DA_Ch2Buf0 | 0xc7044000 | 16.000 | 16.000 | 0.000 | 100.000% |
| DA_Ch3Buf0 | 0xc7048000 | 16.000 | 16.000 | 0.000 | 100.000% |
| DA_Ch4Buf0 | 0xc704c000 | 16.000 | 16.000 | 0.000 | 100.000% |
| DA_Ch5Buf0 | 0xc7050000 | 16.000 | 16.000 | 0.000 | 100.000% |
| DA_Ch6Buf0 | 0xc7054000 | 16.000 | 16.000 | 0.000 | 100.000% |
| DA_Ch7Buf0 | 0xc7058000 | 16.000 | 16.000 | 0.000 | 100.000% |
| DA_Ch8Buf0 | 0xc705c000 | 16.000 | 16.000 | 0.000 | 100.000% |
| DA_Ch1Buf1 | 0xc7060000 | 16.000 | 16.000 | 0.000 | 100.000% |
| DA_Ch2Buf1 | 0xc7064000 | 16.000 | 16.000 | 0.000 | 100.000% |
| DA_Ch3Buf1 | 0xc7068000 | 16.000 | 16.000 | 0.000 | 100.000% |
| DA_Ch4Buf1 | 0xc706c000 | 16.000 | 16.000 | 0.000 | 100.000% |
| DA_Ch5Buf1 | 0xc7070000 | 16.000 | 16.000 | 0.000 | 100.000% |
| DA_Ch6Buf1 | 0xc7074000 | 16.000 | 16.000 | 0.000 | 100.000% |
| DA_Ch7Buf1 | 0xc7078000 | 16.000 | 16.000 | 0.000 | 100.000% |
| DA_Ch8Buf1 | 0xc707c000 | 16.000 | 16.000 | 0.000 | 100.000% |

## 4. Module usage

| Module | Code/const KiB | Static RAM KiB |
|---|---|---|
| WOLA / FFT | 11.219 | 18.680 |
| Denoise | 12.344 | 19.332 |
| Compression | 5.000 | 0.324 |
| Main chain and audio buffers | 11.227 | 743.321 |
| UI and display | 48.827 | 751.950 |
| Platform and shared runtime | 47.880 | 1.690 |

Module values use TI linker object-file contributions. Shared libraries and linker alignment/holes are retained in `Platform and shared runtime`; no shared contribution is counted twice.

## 5. Largest linked contributions

### Code (top 8 of 15 in CSV)

| Rank | Object contribution | Bytes | Region | Purpose |
|---|---|---|---|---|
| 1 | user_subband_denoise.obj (.text) | 12640 | DDR2 | Subband denoise state and processing code |
| 2 | user_subband_ui.obj (.text) | 9024 | DDR2 | Project 3.2 UI, display state, text or glyph support |
| 3 | user_subband_flow.obj (.text) | 7936 | DDR2 | Project 3.2 audio main chain, frame and ping-pong state |
| 4 | system_config.lib:interrupt.obj (.text:retain) | 6816 | DDR2 | Platform, driver or shared runtime contribution |
| 5 | drivers.lib:edma.obj (.text) | 6048 | DDR2 | Platform, driver or shared runtime contribution |
| 6 | user_subband_polyphase.obj (.text) | 5856 | DDR2 | Polyphase/FFT filter-bank state and code |
| 7 | user_subband_wola.obj (.text) | 5632 | DDR2 | WOLA analysis/synthesis, overlap and spectrum state |
| 8 | user_subband_codec_loopback.obj (.text) | 5088 | DDR2 | Subband codec/quantization state and code |

### Static data (top 8 of 15 in CSV)

| Rank | Object contribution | Bytes | Region | Purpose |
|---|---|---|---|---|
| 1 | lcd_dma.obj (offscreen_buffer) | 768036 | DDR2 | LCD frame/off-screen buffer |
| 2 | user_subband_flow.obj (.subband_l2) | 218184 | DSPL2RAM | Project 3.2 audio main chain, frame and ping-pong state |
| 3 | user_subband_denoise.obj (.subband_l2) | 19640 | DSPL2RAM | Subband denoise state and processing code |
| 4 | user_subband_wola.obj (.subband_l2) | 16428 | DSPL2RAM | WOLA analysis/synthesis, overlap and spectrum state |
| 5 | adc_api.obj (AD_Ch1Buf0_File) | 16384 | AD_Ch1Buf0 | ADC/DAC driver or fixed board audio buffer |
| 6 | adc_api.obj (AD_Ch2Buf0_File) | 16384 | AD_Ch2Buf0 | ADC/DAC driver or fixed board audio buffer |
| 7 | adc_api.obj (AD_Ch3Buf0_File) | 16384 | AD_Ch3Buf0 | ADC/DAC driver or fixed board audio buffer |
| 8 | adc_api.obj (AD_Ch4Buf0_File) | 16384 | AD_Ch4Buf0 | ADC/DAC driver or fixed board audio buffer |

### Constants (top 8 of 15 in CSV)

| Rank | Object contribution | Bytes | Region | Purpose |
|---|---|---|---|---|
| 1 | fontcmss38b.obj (.const:.string:g_pucCmss38bData) | 4086 | DDR2 | LCD, touch, font or graphics support |
| 2 | fontcmss22b.obj (.const:.string:g_pucCmss22bData) | 2334 | DDR2 | LCD, touch, font or graphics support |
| 3 | fontcmss18b.obj (.const:.string:g_pucCmss18bData) | 1922 | DDR2 | LCD, touch, font or graphics support |
| 4 | fontcmtt12.obj (.const:.string:g_pucCmtt12Data) | 1313 | DDR2 | LCD, touch, font or graphics support |
| 5 | dac_pru.obj (.const:PRU0_Code) | 1212 | DDR2 | ADC/DAC driver or fixed board audio buffer |
| 6 | user_subband_ui_logic.obj (.const:.string) | 356 | DDR2 | Project 3.2 UI, display state, text or glyph support |
| 7 | user_subband_ui_font.obj (.const:.string:Glyph_0) | 320 | DDR2 | Project 3.2 UI, display state, text or glyph support |
| 8 | fontcmss18b.obj (.const:g_sFontCmss18b) | 200 | DDR2 | LCD, touch, font or graphics support |

## 6. THD test-only increment

The THD build adds **2008648 bytes (1961.570 KiB)** of mapped run-time memory. Of this, **2007240 bytes (1960.195 KiB)** is the `TEST_ONLY` `user_subband_flow.obj (.far)` contribution in DDR containing the 10 s JTAG capture buffers and adjacent THD frame scratch state. It is absent from the production map and excluded from all production totals.

- `SUBBAND_THD_InputPacked`: address `0xc0000000`; 1001472 bytes until the next linker symbol.
- `SUBBAND_THD_OutputPacked`: address `0xc00f4800`; 1005768 bytes remain to the end of the same THD-only object contribution.
- The linker XML proves the whole contribution and both symbol boundaries, but does not provide an individual variable-size record for `OutputPacked` versus adjacent private THD frame scratch arrays. This audit does not replace that missing precision with a source estimate.

| Category | Production bytes | THD bytes | TEST_ONLY increment |
|---|---|---|---|
| program_code_and_readonly | 139772 | 141156 | 1384 |
| static_readwrite_ram | 1572145 | 3579409 | 2007264 |
| stack_mapped_reserved | 2048 | 2048 | 0 |
| heap_mapped_reserved | 0 | 0 | 0 |
| total_runtime_ram | 1713965 | 3722613 | 2008648 |

## 7. Dynamic allocation and stack/heap

Production links no `malloc/calloc/realloc/free` symbols and maps `.sysmem` to 0 bytes. The source-level calls found in `user_subband_audio_compare.c` are inside `SUBBAND_ALGO_ONLY`, a host/offline-only branch absent from this board build. Therefore: **运行期未发现动态堆分配，主要内存为静态分配。** The 2 KiB heap option is configuration, not measured use; the 2 KiB stack is only a linker reservation, not a measured high-water mark.

## 8. Evidence and limitations

- All byte counts come from the fresh production/THD TI linker map and XML link-info files.
- Production and THD both report `link_errors=0x0`; their maps were produced by separate clean/full builds.
- Program load and run sizes are never added together, and `.out` artifact size is not treated as RAM.
- Module and largest-object reporting is object-file contribution granularity. The map cannot safely assign every common symbol or shared library to a single algorithm.
- No board-side stack watermark or run-time heap peak was measured; neither is claimed.
- Debug information sections exist in the ELF artifact but have run address 0 and are excluded from target RAM/program-image totals.
