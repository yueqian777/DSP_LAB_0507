/**
 * user_equalizer_display.c
 *
 * Fixed-region LCD renderer for the Project 3.3 status UI.
 */

#include "user_equalizer_display.h"

#if defined(EQ_ALGO_ONLY) || (EQ_ENABLE_LCD_DISPLAY != 0)

#include "string.h"

#if defined(EQ_ALGO_ONLY)
#include "stdio.h"
#endif

#if (!defined(EQ_ALGO_ONLY))
#include "lcd_api.h"
#include "lcd_dma.h"
#include "hw_lcdc.h"
#include "hw_dspcache.h"
#include "hw_types.h"
#include "interrupt.h"
#include "raster.h"
#include "soc_C6748.h"
#include "dspcache.h"
#endif

#if defined(__TI_COMPILER_VERSION__) || defined(__TMS320C6X__)
#include "c6x.h"
#endif

#define EQ_UI_TITLE_X 20
#define EQ_UI_TITLE_Y 5
#define EQ_UI_TITLE_W 760
#define EQ_UI_TITLE_H 24

#define EQ_UI_ANALYZER_INNER_X_OFFSET 30
#define EQ_UI_ANALYZER_INNER_W 16
#define EQ_UI_ANALYZER_ZERO_Y EQ_UI_ANALYZER_ZERO_PIXEL
#define EQ_UI_DYNAMIC_VALUE_CLEAR_W 44
#define EQ_UI_DYNAMIC_VALUE_CLEAR_H 20

#if EQ_ENABLE_TEN_BAND_EDITOR != 0
#define EQ_UI_EDITOR_INNER_X_OFFSET 26
#define EQ_UI_EDITOR_INNER_W 16
#define EQ_UI_EDITOR_VALUE_CLEAR_W 96
#define EQ_UI_EDITOR_VALUE_CLEAR_H 22
static const EQ_UI_RECT s_editor_field_rects[5] =
{
    { 24, 398, 112, 70 },
    { 148, 398, 112, 70 },
    { 272, 398, 112, 70 },
    { 396, 398, 112, 70 },
    { 520, 398, 136, 70 }
};
#endif

#define EQ_FONT_SMALL 0
#define EQ_FONT_MEDIUM 1
#define EQ_FONT_LARGE 2

#define EQ_COLOR_BG 0
#define EQ_COLOR_TEXT 1
#define EQ_COLOR_MUTED 2
#define EQ_COLOR_BORDER 3
#define EQ_COLOR_HIGHLIGHT 4
#define EQ_COLOR_ACTIVE 5
#define EQ_COLOR_BAR_POS 6
#define EQ_COLOR_BAR_NEG 7
#define EQ_COLOR_ZERO 8

#define EQ_CN_GLYPH_SIZE 16
#define EQ_CN_GLYPH_ADVANCE 17

#define EQ_LCD_PALETTE_OFFSET 4UL
#define EQ_LCD_PALETTE_SIZE 32UL
#define EQ_LCD_BYTES_PER_PIXEL 2UL
#define EQ_LCD_FRAME_DMA_BYTES \
    (EQ_LCD_PALETTE_SIZE + \
     ((unsigned long)EQ_UI_SCREEN_WIDTH * \
      (unsigned long)EQ_UI_SCREEN_HEIGHT * EQ_LCD_BYTES_PER_PIXEL))
#define EQ_LCD_BUFFER_TOTAL_BYTES \
    (EQ_LCD_PALETTE_OFFSET + EQ_LCD_FRAME_DMA_BYTES)
#define EQ_LCD_DMA_ADDRESS_MASK 0xFFFFFFFCUL
#define EQ_LCD_HOST_BUFFER_ADDRESS 0xC0000000UL
#define EQ_LCD_HOST_EDITOR_BUFFER_ADDRESS \
    (EQ_LCD_HOST_BUFFER_ADDRESS + EQ_LCD_BUFFER_TOTAL_BYTES)
#if defined(EQ_ALGO_ONLY)
#define EQ_LCD_STATUS_SYNC_MASK 0x00000004UL
#define EQ_LCD_STATUS_FIFO_UNDERFLOW_MASK 0x00000020UL
#define EQ_LCD_STATUS_DONE_MASK 0x00000001UL
#define EQ_LCD_STATUS_EOF0_MASK 0x00000100UL
#define EQ_LCD_STATUS_EOF1_MASK 0x00000200UL
#define EQ_LCD_RASTER_ENABLE_MASK 0x00000001UL
#else
#define EQ_LCD_STATUS_SYNC_MASK ((unsigned long)LCDC_LCD_STAT_SYNC)
#define EQ_LCD_STATUS_FIFO_UNDERFLOW_MASK \
    ((unsigned long)LCDC_LCD_STAT_FUF)
#define EQ_LCD_STATUS_DONE_MASK ((unsigned long)LCDC_LCD_STAT_DONE)
#define EQ_LCD_STATUS_EOF0_MASK ((unsigned long)LCDC_LCD_STAT_EOF0)
#define EQ_LCD_STATUS_EOF1_MASK ((unsigned long)LCDC_LCD_STAT_EOF1)
#define EQ_LCD_RASTER_ENABLE_MASK \
    ((unsigned long)LCDC_RASTER_CTRL_RASTER_EN)
#endif
#define EQ_LCD_STATUS_EOF_MASK \
    (EQ_LCD_STATUS_EOF0_MASK | EQ_LCD_STATUS_EOF1_MASK)
#define EQ_LCD_DESCRIPTOR_FB0 0x01U
#define EQ_LCD_DESCRIPTOR_FB1 0x02U
#define EQ_LCD_DESCRIPTOR_BOTH \
    (EQ_LCD_DESCRIPTOR_FB0 | EQ_LCD_DESCRIPTOR_FB1)
#define EQ_LCD_INIT_STOP_TIMEOUT_SPINS 16000000UL
#define EQ_LCD_FAULT_STOP_POLL_SPINS 4096UL
#define EQ_LCD_CLEARABLE_FAULT_MASK \
    (EQ_LCD_STATUS_SYNC_MASK | EQ_LCD_STATUS_FIFO_UNDERFLOW_MASK)
#if EQ_ENABLE_TEN_BAND_EDITOR != 0
#define EQ_LCD_PAGE_COUNT 2
#else
#define EQ_LCD_PAGE_COUNT 1
#endif

#if !defined(EQ_ALGO_ONLY)
#if (PALETTE_OFFSET != 4) || (PALETTE_SIZE != 32)
#error Project 3.3 LCD audit assumes the current 4-byte header and 32-byte palette.
#endif

#if !defined(EQ_ALGO_ONLY) && (EQ_ENABLE_TEN_BAND_EDITOR != 0)
#pragma DATA_SECTION(EQ_LcdEditorBuffer, "offscreen_buffer")
#pragma DATA_ALIGN(EQ_LcdEditorBuffer, 4);
unsigned char EQ_LcdEditorBuffer[
    GrOffScreen16BPPSize(LCD_WIDTH, LCD_HEIGHT)];
static tDisplay s_editor_display;
static tContext s_editor_context;
#endif
#endif

#if EQ_LCD_USE_CHINESE
typedef enum
{
    CN_SHI_TEN = 0,
    CN_DUAN,
    CN_DONG,
    CN_TAI,
    CN_JUN,
    CN_HENG,
    CN_XI,
    CN_TONG,
    CN_PING,
    CN_ZHI,
    CN_DI,
    CN_YIN,
    CN_REN,
    CN_SHENG,
    CN_GAO,
    CN_XING,
    CN_PIN,
    CN_HUN,
    CN_ZHUO,
    CN_QING,
    CN_XI_CLEAR,
    CN_ZHI_WISE,
    CN_NENG,
    CN_DU,
    CN_BAO,
    CN_HU,
    CN_KAI,
    CN_GUAN,
    CN_ZHONG,
    CN_COUNT
} EQ_CN_GLYPH;

static const unsigned short s_cn_bits[CN_COUNT][EQ_CN_GLYPH_SIZE] =
{
    { 0x0180U, 0x0180U, 0x0180U, 0x0180U, 0x0180U, 0x0180U,
      0x7FFEU, 0x0180U, 0x0180U, 0x0180U, 0x0180U, 0x0180U,
      0x0180U, 0x0180U, 0x0000U, 0x0000U }, /* U+5341 */
    { 0x0400U, 0x3CF8U, 0x2088U, 0x2088U, 0x3C88U, 0x218EU,
      0x2000U, 0x3DFCU, 0x2088U, 0x2048U, 0x2E50U, 0x7030U,
      0x2070U, 0x238EU, 0x2002U, 0x0000U }, /* U+6BB5 */
    { 0x0030U, 0x0030U, 0x3E30U, 0x0030U, 0x00FEU, 0x7F32U,
      0x7F22U, 0x1022U, 0x1622U, 0x2222U, 0x2362U, 0x7D46U,
      0x00C6U, 0x019CU, 0x0000U, 0x0000U }, /* U+52A8 */
    { 0x0100U, 0x0100U, 0x7FFEU, 0x0240U, 0x0660U, 0x0420U,
      0x1B18U, 0x310EU, 0x6000U, 0x0080U, 0x248CU, 0x2406U,
      0x6412U, 0x47F0U, 0x0000U, 0x0000U }, /* U+6001 */
    { 0x1040U, 0x10C0U, 0x1080U, 0x10FEU, 0x7D86U, 0x1146U,
      0x1366U, 0x1026U, 0x1016U, 0x1036U, 0x1CC4U, 0x7184U,
      0x0004U, 0x001CU, 0x0000U, 0x0000U }, /* U+5747 */
    { 0x0100U, 0x1200U, 0x35CEU, 0x6080U, 0x17E0U, 0x14BEU,
      0x37E4U, 0x74A4U, 0x77E4U, 0x3104U, 0x3FF4U, 0x3184U,
      0x3664U, 0x3C2CU, 0x3008U, 0x0000U }, /* U+8861 */
    { 0x001CU, 0x3FE0U, 0x0180U, 0x0210U, 0x0C60U, 0x1FC0U,
      0x0310U, 0x0C0CU, 0x3FFEU, 0x0080U, 0x0890U, 0x108CU,
      0x2086U, 0x4380U, 0x0000U, 0x0000U }, /* U+7CFB */
    { 0x1060U, 0x1020U, 0x33FEU, 0x2440U, 0x4C40U, 0x7888U,
      0x118CU, 0x11FEU, 0x2450U, 0x78D0U, 0x00D0U, 0x0490U,
      0x7992U, 0x431EU, 0x0200U, 0x0000U }, /* U+7EDF */
    { 0x3FFCU, 0x0180U, 0x1188U, 0x1998U, 0x0D90U, 0x05A0U,
      0x0180U, 0x7FFEU, 0x0180U, 0x0180U, 0x0180U, 0x0180U,
      0x0180U, 0x0180U, 0x0000U, 0x0000U }, /* U+5E73 */
    { 0x0000U, 0x0180U, 0x0180U, 0x7FFCU, 0x0100U, 0x1FF8U,
      0x1818U, 0x1FF8U, 0x1818U, 0x1FF8U, 0x1818U, 0x1818U,
      0x1FF8U, 0x1818U, 0x7FFEU, 0x0000U }, /* U+76F4 */
    { 0x0800U, 0x18FCU, 0x13A0U, 0x1220U, 0x3220U, 0x3220U,
      0x53FEU, 0x5220U, 0x1220U, 0x1230U, 0x1290U, 0x1392U,
      0x138AU, 0x124EU, 0x1000U, 0x0000U }, /* U+4F4E */
    { 0x0100U, 0x0180U, 0x3FFCU, 0x0C30U, 0x0420U, 0x0460U,
      0x7FFEU, 0x0000U, 0x1FF8U, 0x1818U, 0x1FF8U, 0x1818U,
      0x1818U, 0x1FF8U, 0x1818U, 0x0000U }, /* U+97F3 */
    { 0x0180U, 0x0180U, 0x0180U, 0x0180U, 0x0180U, 0x0180U,
      0x0180U, 0x03C0U, 0x0240U, 0x0660U, 0x0430U, 0x0818U,
      0x300CU, 0x6006U, 0x0000U, 0x0000U }, /* U+4EBA */
    { 0x0080U, 0x3FFEU, 0x0080U, 0x0080U, 0x1FFCU, 0x0000U,
      0x1FFCU, 0x108CU, 0x108CU, 0x1FFCU, 0x100CU, 0x3000U,
      0x2000U, 0x6000U, 0x0000U, 0x0000U }, /* U+58F0 */
    { 0x0100U, 0x0180U, 0x7FFEU, 0x1FF0U, 0x0810U, 0x0810U,
      0x1FF0U, 0x7FFEU, 0x2006U, 0x27E6U, 0x2426U, 0x2426U,
      0x27E6U, 0x200EU, 0x0008U, 0x0000U }, /* U+9AD8 */
    { 0x0004U, 0x3FA4U, 0x0924U, 0x0924U, 0x7FE4U, 0x7FE4U,
      0x1124U, 0x3104U, 0x610CU, 0x0180U, 0x1FFCU, 0x0180U,
      0x0180U, 0x7FFEU, 0x0000U, 0x0000U }, /* U+578B */
    { 0x0800U, 0x28FEU, 0x2F10U, 0x2820U, 0x287EU, 0x7F46U,
      0x0856U, 0x2856U, 0x2B56U, 0x6A56U, 0x0E56U, 0x0C20U,
      0x186CU, 0x61C6U, 0x0000U, 0x0000U }, /* U+9891 */
    { 0x0000U, 0x37FEU, 0x1E02U, 0x06C0U, 0x43FCU, 0x6080U,
      0x1120U, 0x0320U, 0x03FCU, 0x1820U, 0x1020U, 0x37FEU,
      0x2020U, 0x6020U, 0x0020U, 0x0000U }, /* U+6D51 */
    { 0x0040U, 0x7040U, 0x1840U, 0x03FCU, 0x0244U, 0x6244U,
      0x3244U, 0x0244U, 0x03FCU, 0x1244U, 0x3040U, 0x2044U,
      0x2046U, 0x67FAU, 0x0000U, 0x0000U }, /* U+6D4A */
    { 0x0040U, 0x27FCU, 0x1840U, 0x03FCU, 0x4040U, 0x67FEU,
      0x1000U, 0x03FCU, 0x020CU, 0x1BFCU, 0x120CU, 0x23FCU,
      0x220CU, 0x421CU, 0x0200U, 0x0000U }, /* U+6E05 */
    { 0x0100U, 0x793EU, 0x4930U, 0x4930U, 0x4FD0U, 0x491EU,
      0x7B14U, 0x4B94U, 0x4B74U, 0x4D24U, 0x7D24U, 0x4924U,
      0x4164U, 0x0144U, 0x0000U, 0x0000U }, /* U+6670 */
    { 0x1800U, 0x1000U, 0x3FBCU, 0x2424U, 0x7FA4U, 0x0C24U,
      0x1B3CU, 0x3100U, 0x2FF8U, 0x0818U, 0x0FF8U, 0x0818U,
      0x0818U, 0x0FF8U, 0x0818U, 0x0000U }, /* U+667A */
    { 0x0840U, 0x1844U, 0x3658U, 0x2360U, 0x7FC2U, 0x007EU,
      0x3F00U, 0x2340U, 0x3F44U, 0x2378U, 0x2360U, 0x3F40U,
      0x2342U, 0x277EU, 0x2000U, 0x0000U }, /* U+80FD */
    { 0x0080U, 0x0080U, 0x3FFCU, 0x2210U, 0x2210U, 0x2FFCU,
      0x2210U, 0x23F0U, 0x2000U, 0x2FF8U, 0x2310U, 0x21A0U,
      0x60C0U, 0x473EU, 0x0802U, 0x0000U }, /* U+5EA6 */
    { 0x0800U, 0x19FCU, 0x110CU, 0x110CU, 0x31FCU, 0x3060U,
      0x5060U, 0x57FEU, 0x10F0U, 0x11F0U, 0x1178U, 0x126CU,
      0x1666U, 0x1060U, 0x0000U, 0x0000U }, /* U+4FDD */
    { 0x1820U, 0x1810U, 0x1800U, 0x18FEU, 0x7E82U, 0x1882U,
      0x1882U, 0x1EFEU, 0x7886U, 0x1880U, 0x1980U, 0x1900U,
      0x1900U, 0x3200U, 0x0200U, 0x0000U }, /* U+62A4 */
    { 0x3FFCU, 0x0620U, 0x0620U, 0x0620U, 0x0620U, 0x7FFEU,
      0x7FFEU, 0x0620U, 0x0420U, 0x0420U, 0x0C20U, 0x1820U,
      0x7020U, 0x0000U, 0x0000U, 0x0000U }, /* U+5F00 */
    { 0x0420U, 0x0420U, 0x0660U, 0x3FFCU, 0x3FFCU, 0x0180U,
      0x0180U, 0x0180U, 0x7FFEU, 0x0180U, 0x0340U, 0x0670U,
      0x0C1CU, 0x700EU, 0x0000U, 0x0000U }, /* U+5173 */
    { 0x0180U, 0x0180U, 0x0180U, 0x3FFCU, 0x2184U, 0x2184U,
      0x2184U, 0x2184U, 0x3FFCU, 0x2184U, 0x0180U, 0x0180U,
      0x0180U, 0x0180U, 0x0180U, 0x0000U }  /* U+4E2D */
};

#if !EQ_LCD_DIAGNOSTIC_ALIGNMENT_PATTERN
static const unsigned char s_title[] =
{
    CN_SHI_TEN, CN_DUAN, CN_DONG, CN_TAI,
    CN_JUN, CN_HENG, CN_XI, CN_TONG
};
#if EQ_ENABLE_TEN_BAND_EDITOR != 0
static const unsigned char s_editor_title[] =
{
    CN_SHI_TEN, CN_DUAN, CN_JUN, CN_HENG, CN_XI, CN_TONG
};
#endif
static const unsigned char s_preset_flat[] = { CN_PING, CN_ZHI };
static const unsigned char s_preset_bass[] = { CN_DI, CN_YIN };
static const unsigned char s_preset_vocal[] = { CN_REN, CN_SHENG };
static const unsigned char s_preset_treble[] = { CN_GAO, CN_YIN };
#endif
#endif

volatile unsigned long EQ_DebugLcdRefreshCount = 0UL;
volatile unsigned long EQ_DebugLcdSkipBusyCount = 0UL;
volatile int EQ_DebugLcdEnabled = 0;
volatile unsigned int EQ_DebugLcdRuntimeMask = 0U;
volatile unsigned long EQ_DebugLcdPendingMask = 0UL;
volatile unsigned long EQ_DebugLcdJobCount = 0UL;
volatile unsigned long EQ_DebugLcdDeferredAudioCount = 0UL;
volatile unsigned long EQ_DebugLcdAudioArrivedDuringDrawCount = 0UL;
volatile unsigned long EQ_DebugLcdUnexpectedFullRedrawCount = 0UL;
volatile unsigned long EQ_DebugLcdStaticDrawCount = 0UL;
volatile unsigned long EQ_DebugLcdBoundsFailureCount = 0UL;
volatile unsigned long EQ_DebugLcdBudgetExceededCount = 0UL;
volatile unsigned long EQ_DebugLcdHardBudgetExceededCount = 0UL;
volatile unsigned long EQ_DebugLcdOver1msCount = 0UL;
volatile unsigned long EQ_DebugLcdOver2msCount = 0UL;
volatile unsigned long EQ_DebugLcdOver5msCount = 0UL;
volatile unsigned long EQ_DebugLcdLastJobStartCycles = 0UL;
volatile unsigned long EQ_DebugLcdLastJobEndCycles = 0UL;
volatile unsigned long EQ_DebugLcdLastJobCycles = 0UL;
volatile unsigned long EQ_DebugLcdMaxJobCycles = 0UL;
volatile unsigned long EQ_DebugLcdPageSyncMaxCycles = 0UL;
volatile unsigned int EQ_DebugLcdPageSyncMaxJob =
    EQ_LCD_PAGE_SYNC_JOB_NONE;
volatile unsigned int EQ_DebugLcdPageSyncLastOver2msJob =
    EQ_LCD_PAGE_SYNC_JOB_NONE;
volatile unsigned long EQ_DebugLcdLastJobTenthsMs = 0UL;
volatile unsigned long EQ_DebugLcdMaxJobTenthsMs = 0UL;
volatile int EQ_DebugLcdLastJob = EQ_LCD_JOB_NONE;
volatile unsigned long EQ_DebugLcdAutoDisabledCount = 0UL;
volatile unsigned long EQ_DebugLcdAutoDisableReason = 0UL;
volatile unsigned int EQ_DebugLcdPageRasterPaused = 0U;
volatile unsigned long EQ_DebugLcdPageRasterPauseCount = 0UL;
volatile unsigned long EQ_DebugLcdPageRasterResumeCount = 0UL;
#if defined(__TI_COMPILER_VERSION__) || defined(__TMS320C6X__)
#pragma RETAIN(EQ_DebugLcdDoubleBufferEnabled)
#endif
volatile const unsigned int EQ_DebugLcdDoubleBufferEnabled =
    (EQ_ENABLE_TEN_BAND_EDITOR != 0) ? 1U : 0U;
volatile int EQ_DebugLcdFrontPage = EQ_UI_PAGE_DYNAMIC_STATUS;
volatile int EQ_DebugLcdSwapTargetPage = EQ_UI_PAGE_DYNAMIC_STATUS;
volatile unsigned int EQ_DebugLcdSwapPending = 0U;
volatile unsigned int EQ_DebugLcdSwapDescriptorMask = 0U;
volatile unsigned long EQ_DebugLcdEofCount = 0UL;
volatile unsigned long EQ_DebugLcdEofAmbiguousCount = 0UL;
volatile unsigned long EQ_DebugLcdSwapRequestCount = 0UL;
volatile unsigned long EQ_DebugLcdSwapCompleteCount = 0UL;
volatile unsigned long EQ_DebugLcdEof0Count = 0UL;
volatile unsigned long EQ_DebugLcdEof1Count = 0UL;
volatile unsigned long EQ_DebugLcdRasterStopTimeoutCount = 0UL;
volatile unsigned int EQ_DebugLcdCacheMode = EQ_LCD_CACHE_UNKNOWN;
volatile unsigned long EQ_DebugLcdBufferMar = 0UL;
volatile unsigned long EQ_DebugLcdEditorBufferMar = 0UL;
volatile unsigned long EQ_DebugLcdWritebackCount = 0UL;
volatile unsigned long EQ_DebugLcdWritebackBytes = 0UL;
volatile unsigned long EQ_DebugLcdWritebackFailureCount = 0UL;
volatile unsigned int EQ_DebugLcdPendingDirtyRegionCount = 0U;
volatile unsigned int EQ_DebugLcdMaxPendingDirtyRegionCount = 0U;
volatile unsigned int EQ_DebugLcdPagePhase = 0U;
volatile unsigned long EQ_DebugLcdDynamicDirtyMask = 0UL;
volatile unsigned long EQ_DebugLcdEditorDirtyMask = 0UL;
volatile unsigned long EQ_DebugLcdPageRequestedVersion[2];
volatile unsigned long EQ_DebugLcdPageRenderedVersion[2];
volatile EQ_LCD_SWAP_TRACE_ENTRY
    EQ_DebugLcdSwapTrace[EQ_LCD_SWAP_TRACE_DEPTH];
volatile unsigned int EQ_DebugLcdSwapTraceWriteIndex = 0U;
volatile unsigned int EQ_DebugLcdSwapTraceCount = 0U;
volatile unsigned long EQ_DebugLcdSwapTraceWrapCount = 0UL;
#if defined(__TI_COMPILER_VERSION__) || defined(__TMS320C6X__)
#pragma RETAIN(EQ_DebugLcdCategoryCountSize)
#pragma RETAIN(EQ_DebugLcdJobTypeCountSize)
#pragma RETAIN(EQ_DebugUiJobCountSize)
#pragma RETAIN(EQ_DebugDynamicHitboxCountSize)
#pragma RETAIN(EQ_DebugEditorHitboxCountSize)
#endif
volatile const unsigned int EQ_DebugLcdCategoryCountSize =
    (unsigned int)EQ_LCD_CATEGORY_COUNT;
volatile const unsigned int EQ_DebugLcdJobTypeCountSize =
    (unsigned int)EQ_LCD_JOB_COUNT;
volatile const unsigned int EQ_DebugUiJobCountSize =
    (unsigned int)EQ_UI_JOB_COUNT;
volatile const unsigned int EQ_DebugDynamicHitboxCountSize =
    (unsigned int)EQ_UI_DYNAMIC_HITBOX_COUNT;
#if EQ_ENABLE_TEN_BAND_EDITOR != 0
volatile const unsigned int EQ_DebugEditorHitboxCountSize =
    (unsigned int)EQ_UI_EDITOR_HITBOX_COUNT;
#else
volatile const unsigned int EQ_DebugEditorHitboxCountSize = 0U;
#endif
volatile unsigned long EQ_DebugLcdCategoryCount[EQ_LCD_CATEGORY_COUNT];
volatile unsigned long EQ_DebugLcdCategoryLastCycles[EQ_LCD_CATEGORY_COUNT];
volatile unsigned long EQ_DebugLcdCategoryMaxCycles[EQ_LCD_CATEGORY_COUNT];
volatile unsigned long EQ_DebugLcdJobTypeCount[EQ_LCD_JOB_COUNT];
volatile unsigned long EQ_DebugLcdJobTypeLastCycles[EQ_LCD_JOB_COUNT];
volatile unsigned long EQ_DebugLcdJobTypeMaxCycles[EQ_LCD_JOB_COUNT];
volatile int EQ_DebugLcdAnalyzerLastBand = -1;
volatile unsigned int EQ_DebugLcdAnalyzerLastField = 0U;
volatile int EQ_DebugLcdAnalyzerLastStripY = 0;
volatile unsigned int EQ_DebugLcdAnalyzerLastStripHeight = 0U;
volatile unsigned int EQ_DebugLcdAnalyzerLastStripOperation =
    EQ_LCD_ANALYZER_STRIP_NONE;
volatile unsigned int EQ_DebugLcdAnalyzerMaxStripHeight = 0U;
volatile unsigned long EQ_DebugLcdAnalyzerStripCount = 0UL;
volatile unsigned long EQ_DebugLcdExpectedFrameBase = 0UL;
volatile unsigned long EQ_DebugLcdExpectedFrameEnd = 0UL;
volatile unsigned long EQ_DebugLcdBufferAddress = 0UL;
volatile unsigned long EQ_DebugLcdBufferEndAddress = 0UL;
volatile unsigned long EQ_DebugLcdEditorBufferAddress = 0UL;
volatile unsigned long EQ_DebugLcdEditorBufferEndAddress = 0UL;
volatile unsigned long EQ_DebugLcdCurrentFrameBase = 0UL;
volatile unsigned long EQ_DebugLcdCurrentFrameEnd = 0UL;
volatile unsigned long EQ_DebugLcdCurrentFrame1Base = 0UL;
volatile unsigned long EQ_DebugLcdCurrentFrame1End = 0UL;
volatile unsigned long EQ_DebugLcdRasterFaultCount = 0UL;
volatile unsigned long EQ_DebugLcdSyncLostCount = 0UL;
volatile unsigned long EQ_DebugLcdFifoUnderflowCount = 0UL;
volatile unsigned long EQ_DebugLcdFrameAddressMismatchCount = 0UL;
volatile unsigned long EQ_DebugLcdLastRasterStatus = 0UL;
volatile unsigned long EQ_DebugLcdLastIrqStatus = 0UL;
volatile unsigned long EQ_DebugLcdStartupRasterStatus = 0UL;
volatile unsigned long EQ_DebugLcdStartupStatusAfterClear = 0UL;
volatile unsigned long EQ_DebugLcdStartupFaultClearCount = 0UL;
volatile unsigned long EQ_DebugLcdFirstFaultFrame = 0UL;
volatile unsigned long EQ_DebugLcdFirstFaultJob = 0UL;
volatile unsigned long EQ_DebugLcdFramebufferCanaryCheckCount = 0UL;
volatile unsigned long EQ_DebugLcdFramebufferCanaryFailureCount = 0UL;
volatile unsigned long EQ_DebugLcdFramebufferFirstFailureFrame = 0UL;
volatile unsigned long EQ_DebugLcdFramebufferFirstFailureJob = 0UL;
volatile unsigned int EQ_DebugLcdFaultLatched = 0U;
volatile unsigned int EQ_DebugLcdHardwareAuditRequest = 0U;
volatile EQ_LCD_HW_SNAPSHOT EQ_DebugLcdHwSnapshot;
#if defined(__TI_COMPILER_VERSION__) || defined(__TMS320C6X__)
#pragma RETAIN(EQ_DebugLcdAlignmentPatternEnabled)
#endif
volatile const unsigned int EQ_DebugLcdAlignmentPatternEnabled =
    EQ_LCD_DIAGNOSTIC_ALIGNMENT_PATTERN;
#if defined(__TI_COMPILER_VERSION__) || defined(__TMS320C6X__)
#pragma RETAIN(EQ_DebugLcdTimingCaptureCompiled)
#endif
volatile const unsigned int EQ_DebugLcdTimingCaptureCompiled =
    EQ_ENABLE_LCD_JOB_TIMING_CAPTURE;
#if EQ_ENABLE_LCD_JOB_TIMING_CAPTURE != 0
#if defined(__TI_COMPILER_VERSION__) || defined(__TMS320C6X__)
#pragma RETAIN(EQ_DebugLcdTimingClassCount)
#pragma RETAIN(EQ_DebugLcdTimingSampleCapacity)
#pragma DATA_SECTION(EQ_DebugLcdTimingSamples, ".far")
#endif
volatile const unsigned int EQ_DebugLcdTimingClassCount =
    EQ_LCD_TIMING_CLASS_COUNT;
volatile const unsigned int EQ_DebugLcdTimingSampleCapacity =
    EQ_LCD_TIMING_SAMPLE_CAPACITY;
volatile unsigned long
    EQ_DebugLcdTimingSamples[EQ_LCD_TIMING_CLASS_COUNT]
                            [EQ_LCD_TIMING_SAMPLE_CAPACITY];
volatile unsigned long
    EQ_DebugLcdTimingTotalCount[EQ_LCD_TIMING_CLASS_COUNT];
volatile unsigned int
    EQ_DebugLcdTimingSampleCount[EQ_LCD_TIMING_CLASS_COUNT];
volatile unsigned long
    EQ_DebugLcdTimingDroppedCount[EQ_LCD_TIMING_CLASS_COUNT];
volatile unsigned long
    EQ_DebugLcdTimingMinCycles[EQ_LCD_TIMING_CLASS_COUNT];
volatile unsigned long
    EQ_DebugLcdTimingMaxCycles[EQ_LCD_TIMING_CLASS_COUNT];
volatile unsigned long
    EQ_DebugLcdTimingOver2msCount[EQ_LCD_TIMING_CLASS_COUNT];
volatile unsigned long
    EQ_DebugLcdTimingOver5msCount[EQ_LCD_TIMING_CLASS_COUNT];
volatile unsigned long
    EQ_DebugLcdTimingDeferredByAudioCount[EQ_LCD_TIMING_CLASS_COUNT];
volatile unsigned long
    EQ_DebugLcdTimingAudioArrivedDuringDrawCount
        [EQ_LCD_TIMING_CLASS_COUNT];
volatile unsigned int EQ_DebugLcdTimingLastClass =
    EQ_LCD_TIMING_CLASS_NONE;
#endif
#if defined(__TI_COMPILER_VERSION__) || defined(__TMS320C6X__)
#pragma RETAIN(EQ_DebugUiStateBytes)
#endif
volatile const unsigned long EQ_DebugUiStateBytes =
    (unsigned long)sizeof(EQ_UI_STATE);

static EQ_UI_STATE s_ui_state;
#if EQ_ENABLE_LCD_JOB_TIMING_CAPTURE != 0
static EQ_UI_STATE s_timing_peek_state;
static unsigned int s_timing_last_service_class =
    EQ_LCD_TIMING_CLASS_NONE;
#endif
static volatile int s_lcd_busy = 0;
static int s_layout_drawn = 0;
static int s_runtime_started = 0;
static unsigned int s_dirty_valid[EQ_LCD_PAGE_COUNT];
static int s_dirty_x0[EQ_LCD_PAGE_COUNT];
static int s_dirty_y0[EQ_LCD_PAGE_COUNT];
static int s_dirty_x1[EQ_LCD_PAGE_COUNT];
static int s_dirty_y1[EQ_LCD_PAGE_COUNT];
static volatile unsigned long s_swap_trace_process_frame = 0UL;
static unsigned long s_lcd_last_audit_job = 0UL;
#if EQ_LCD_DIAGNOSTIC_ALIGNMENT_PATTERN
static unsigned long s_lcd_last_audit_frame = 0UL;
#endif
static unsigned int s_lcd_canary_front = 0U;
#if EQ_ENABLE_TEN_BAND_EDITOR != 0
static unsigned int s_lcd_canary_editor = 0U;
#endif
static int s_lcd_canary_ready = 0;
static int s_lcd_seen_sync_lost = 0;
static int s_lcd_seen_fifo_underflow = 0;
static int s_lcd_seen_frame_mismatch = 0;
#if EQ_ENABLE_TEN_BAND_EDITOR != 0
static int s_draw_page = EQ_UI_PAGE_DYNAMIC_STATUS;
static volatile int s_front_page = EQ_UI_PAGE_DYNAMIC_STATUS;
static volatile int s_swap_target_page = EQ_UI_PAGE_DYNAMIC_STATUS;
static volatile unsigned int s_swap_pending = 0U;
static volatile unsigned int s_swap_complete = 0U;
static volatile unsigned int s_swap_descriptor_mask = 0U;
static volatile unsigned int s_eof_fault_pending = 0U;
static unsigned int s_deferred_page_valid = 0U;
static int s_deferred_page = EQ_UI_PAGE_DYNAMIC_STATUS;
static unsigned int s_cancel_after_swap = 0U;
#if !defined(EQ_ALGO_ONLY)
static unsigned int s_raster_reconfigure_ready = 1U;
#endif
#endif

#if defined(EQ_ALGO_ONLY)
static unsigned long s_mock_primitive_count = 0UL;
static unsigned long s_mock_cycle_clock = 0UL;
static unsigned long s_mock_forced_job_cycles = 0UL;
static EQ_LCD_HW_SNAPSHOT s_mock_hw_snapshot;
static int s_mock_canary_failed = 0;
static FILE *s_mock_trace_file = 0;
static unsigned long s_mock_trace_record_count = 0UL;
static int s_mock_trace_job = EQ_LCD_JOB_NONE;
static unsigned long s_mock_trace_frame = 0UL;
#if EQ_ENABLE_TEN_BAND_EDITOR != 0
static unsigned int s_mock_next_eof_descriptor = EQ_LCD_DESCRIPTOR_FB0;
#endif
#endif

#if EQ_ENABLE_LCD_JOB_TIMING_CAPTURE != 0
static unsigned long EQ_ReadCycles(void);

static unsigned int EQ_LcdTimingClassForJob(
    int job, unsigned int completed_fields)
{
    if ((job >= EQ_UI_JOB_PRESET_0) && (job <= EQ_UI_JOB_PRESET_4))
    {
        return EQ_LCD_TIMING_CLASS_PRESET;
    }
    if ((job >= EQ_UI_JOB_ANALYZER_0) &&
        (job <= EQ_UI_JOB_ANALYZER_3) &&
        ((completed_fields & EQ_UI_ANALYZER_FIELD_BAR) != 0U))
    {
        return EQ_LCD_TIMING_CLASS_ANALYZER_STRIP;
    }
    if ((job >= EQ_UI_JOB_DYNAMIC_0) && (job <= EQ_UI_JOB_DYNAMIC_2))
    {
        if ((completed_fields & EQ_UI_DYNAMIC_FIELD_ENABLED) != 0U)
        {
            return EQ_LCD_TIMING_CLASS_DYNAMIC_ENABLED;
        }
        if ((completed_fields & EQ_UI_DYNAMIC_FIELD_STRENGTH) != 0U)
        {
            return EQ_LCD_TIMING_CLASS_DYNAMIC_STRENGTH;
        }
        if ((completed_fields & EQ_UI_DYNAMIC_FIELD_ACTIVE) != 0U)
        {
            return EQ_LCD_TIMING_CLASS_DYNAMIC_ACTIVE;
        }
    }
#if EQ_ENABLE_TEN_BAND_EDITOR != 0
    if ((job >= EQ_UI_JOB_EDITOR_BAND_0) &&
        (job <= EQ_UI_JOB_EDITOR_BAND_9))
    {
        return EQ_LCD_TIMING_CLASS_EDITOR_BAND;
    }
    if ((job == EQ_UI_JOB_EDITOR_FIELDS) && (completed_fields != 0U))
    {
        return EQ_LCD_TIMING_CLASS_EDITOR_FIELD;
    }
    if (job == EQ_UI_JOB_PAGE_SYNC)
    {
        return EQ_LCD_TIMING_CLASS_PAGE_SYNC;
    }
    if (job == EQ_UI_JOB_PAGE_SWAP)
    {
        return EQ_LCD_TIMING_CLASS_PAGE_SWAP;
    }
#endif
    return EQ_LCD_TIMING_CLASS_NONE;
}

static unsigned int EQ_LcdTimingNextFieldForJob(
    const EQ_UI_STATE *state, int job)
{
    unsigned int fields;

    if ((job >= EQ_UI_JOB_DYNAMIC_0) && (job <= EQ_UI_JOB_DYNAMIC_2))
    {
        fields = EqualizerUiLogic_DynamicFieldMask(
            state, job - EQ_UI_JOB_DYNAMIC_0);
        if ((fields & EQ_UI_DYNAMIC_FIELD_ENABLED) != 0U)
        {
            return EQ_UI_DYNAMIC_FIELD_ENABLED;
        }
        if ((fields & EQ_UI_DYNAMIC_FIELD_STRENGTH) != 0U)
        {
            return EQ_UI_DYNAMIC_FIELD_STRENGTH;
        }
        return fields & EQ_UI_DYNAMIC_FIELD_ACTIVE;
    }
    if ((job >= EQ_UI_JOB_ANALYZER_0) &&
        (job <= EQ_UI_JOB_ANALYZER_3))
    {
        return EqualizerUiLogic_AnalyzerNextField(
            state, job - EQ_UI_JOB_ANALYZER_0);
    }
#if EQ_ENABLE_TEN_BAND_EDITOR != 0
    if (job == EQ_UI_JOB_EDITOR_FIELDS)
    {
        return EqualizerUiLogic_EditorNextField(state);
    }
#endif
    return 0U;
}

static void EQ_RecordLcdTimingSample(
    unsigned int timing_class, unsigned long cycles)
{
    unsigned int sample_index;
    unsigned long count;

    if (timing_class >= EQ_LCD_TIMING_CLASS_COUNT)
    {
        return;
    }
    count = EQ_DebugLcdTimingTotalCount[timing_class];
    if ((count == 0UL) ||
        (cycles < EQ_DebugLcdTimingMinCycles[timing_class]))
    {
        EQ_DebugLcdTimingMinCycles[timing_class] = cycles;
    }
    if ((count == 0UL) ||
        (cycles > EQ_DebugLcdTimingMaxCycles[timing_class]))
    {
        EQ_DebugLcdTimingMaxCycles[timing_class] = cycles;
    }
    EQ_DebugLcdTimingTotalCount[timing_class] = count + 1UL;
    sample_index = EQ_DebugLcdTimingSampleCount[timing_class];
    if (sample_index < EQ_LCD_TIMING_SAMPLE_CAPACITY)
    {
        EQ_DebugLcdTimingSamples[timing_class][sample_index] = cycles;
        EQ_DebugLcdTimingSampleCount[timing_class] = sample_index + 1U;
    }
    else
    {
        EQ_DebugLcdTimingDroppedCount[timing_class]++;
    }
    if (cycles > EQ_LCD_JOB_ACCEPTANCE_CYCLES)
    {
        EQ_DebugLcdTimingOver2msCount[timing_class]++;
    }
    if (cycles > EQ_LCD_JOB_HARD_CYCLES)
    {
        EQ_DebugLcdTimingOver5msCount[timing_class]++;
    }
    EQ_DebugLcdTimingLastClass = timing_class;
}
#endif

#if EQ_ENABLE_TEN_BAND_EDITOR != 0
static void EQ_SwapStateLock(void)
{
#if !defined(EQ_ALGO_ONLY)
    IntDisable(C674X_MASK_INT14);
#endif
}

static void EQ_SwapStateUnlock(void)
{
#if !defined(EQ_ALGO_ONLY)
    IntEnable(C674X_MASK_INT14);
#endif
}
#endif

static unsigned long EQ_PageBufferAddress(int page)
{
#if defined(EQ_ALGO_ONLY)
#if EQ_ENABLE_TEN_BAND_EDITOR != 0
    if (page == EQ_UI_PAGE_EQ_EDITOR)
    {
        return EQ_LCD_HOST_EDITOR_BUFFER_ADDRESS;
    }
#else
    (void)page;
#endif
    return EQ_LCD_HOST_BUFFER_ADDRESS;
#else
#if EQ_ENABLE_TEN_BAND_EDITOR != 0
    if (page == EQ_UI_PAGE_EQ_EDITOR)
    {
        return (unsigned long)EQ_LcdEditorBuffer;
    }
#else
    (void)page;
#endif
    return (unsigned long)Lcd_Buffer;
#endif
}

static unsigned long EQ_ExpectedFrameBaseForPage(int page)
{
    return (EQ_PageBufferAddress(page) + EQ_LCD_PALETTE_OFFSET) &
           EQ_LCD_DMA_ADDRESS_MASK;
}

static unsigned long EQ_ExpectedFrameEndForPage(int page)
{
    return (EQ_ExpectedFrameBaseForPage(page) +
            EQ_LCD_FRAME_DMA_BYTES - 2UL) &
           EQ_LCD_DMA_ADDRESS_MASK;
}

static void EQ_SetExpectedFrontPage(int page)
{
    EQ_DebugLcdExpectedFrameBase =
        EQ_ExpectedFrameBaseForPage(page);
    EQ_DebugLcdExpectedFrameEnd =
        EQ_ExpectedFrameEndForPage(page);
    EQ_DebugLcdFrontPage = page;
}

#if EQ_ENABLE_TEN_BAND_EDITOR != 0
static void EQ_SetDrawPage(int page)
{
    s_draw_page = (page == EQ_UI_PAGE_EQ_EDITOR) ?
        EQ_UI_PAGE_EQ_EDITOR : EQ_UI_PAGE_DYNAMIC_STATUS;
}
#endif

static void EQ_SyncPageStateDebug(void)
{
#if EQ_ENABLE_TEN_BAND_EDITOR != 0
    EQ_DebugLcdPagePhase = s_ui_state.page_phase;
    EQ_DebugLcdDynamicDirtyMask =
        EqualizerUiLogic_GetPageDirtyMask(
            &s_ui_state, EQ_UI_PAGE_DYNAMIC_STATUS);
    EQ_DebugLcdEditorDirtyMask =
        EqualizerUiLogic_GetPageDirtyMask(
            &s_ui_state, EQ_UI_PAGE_EQ_EDITOR);
    EQ_DebugLcdPageRequestedVersion[EQ_UI_PAGE_DYNAMIC_STATUS] =
        s_ui_state.page_requested_version[EQ_UI_PAGE_DYNAMIC_STATUS];
    EQ_DebugLcdPageRequestedVersion[EQ_UI_PAGE_EQ_EDITOR] =
        s_ui_state.page_requested_version[EQ_UI_PAGE_EQ_EDITOR];
    EQ_DebugLcdPageRenderedVersion[EQ_UI_PAGE_DYNAMIC_STATUS] =
        s_ui_state.page_rendered_version[EQ_UI_PAGE_DYNAMIC_STATUS];
    EQ_DebugLcdPageRenderedVersion[EQ_UI_PAGE_EQ_EDITOR] =
        s_ui_state.page_rendered_version[EQ_UI_PAGE_EQ_EDITOR];
#else
    EQ_DebugLcdPagePhase = 0U;
    EQ_DebugLcdDynamicDirtyMask = s_ui_state.dirty_mask;
    EQ_DebugLcdEditorDirtyMask = 0UL;
    EQ_DebugLcdPageRequestedVersion[0] = 0UL;
    EQ_DebugLcdPageRequestedVersion[1] = 0UL;
    EQ_DebugLcdPageRenderedVersion[0] = 0UL;
    EQ_DebugLcdPageRenderedVersion[1] = 0UL;
#endif
}

#if !defined(EQ_ALGO_ONLY)
static tContext *EQ_DrawContext(void)
{
#if EQ_ENABLE_TEN_BAND_EDITOR != 0
    if (s_draw_page == EQ_UI_PAGE_EQ_EDITOR)
    {
        return &s_editor_context;
    }
#endif
    return &Lcd_Context;
}

static tDisplay *EQ_DrawDisplay(void)
{
#if EQ_ENABLE_TEN_BAND_EDITOR != 0
    if (s_draw_page == EQ_UI_PAGE_EQ_EDITOR)
    {
        return &s_editor_display;
    }
#endif
    return &Lcd_Display;
}

static unsigned char *EQ_DrawBuffer(void)
{
#if EQ_ENABLE_TEN_BAND_EDITOR != 0
    if (s_draw_page == EQ_UI_PAGE_EQ_EDITOR)
    {
        return EQ_LcdEditorBuffer;
    }
#endif
    return Lcd_Buffer;
}
#endif

static void EQ_CopyHardwareSnapshotToDebug(
    const EQ_LCD_HW_SNAPSHOT *snapshot)
{
    EQ_DebugLcdHwSnapshot.frame_base = snapshot->frame_base;
    EQ_DebugLcdHwSnapshot.frame_end = snapshot->frame_end;
    EQ_DebugLcdHwSnapshot.frame1_base = snapshot->frame1_base;
    EQ_DebugLcdHwSnapshot.frame1_end = snapshot->frame1_end;
    EQ_DebugLcdHwSnapshot.raster_control = snapshot->raster_control;
    EQ_DebugLcdHwSnapshot.raster_status = snapshot->raster_status;
    EQ_DebugLcdHwSnapshot.dma_control = snapshot->dma_control;
    EQ_DebugLcdHwSnapshot.irq_status = snapshot->irq_status;
    EQ_DebugLcdCurrentFrameBase = snapshot->frame_base;
    EQ_DebugLcdCurrentFrameEnd = snapshot->frame_end;
    EQ_DebugLcdCurrentFrame1Base = snapshot->frame1_base;
    EQ_DebugLcdCurrentFrame1End = snapshot->frame1_end;
    EQ_DebugLcdLastRasterStatus = snapshot->raster_status;
    EQ_DebugLcdLastIrqStatus = snapshot->irq_status;
}

static void EQ_ReadHardwareSnapshot(EQ_LCD_HW_SNAPSHOT *snapshot)
{
#if defined(EQ_ALGO_ONLY)
    *snapshot = s_mock_hw_snapshot;
#else
    snapshot->frame_base =
        (unsigned long)HWREG(SOC_LCDC_0_REGS + LCDC_LCDDMA_FB0_BASE);
    snapshot->frame_end =
        (unsigned long)HWREG(SOC_LCDC_0_REGS + LCDC_LCDDMA_FB0_CEILING);
    snapshot->frame1_base =
        (unsigned long)HWREG(SOC_LCDC_0_REGS + LCDC_LCDDMA_FB1_BASE);
    snapshot->frame1_end =
        (unsigned long)HWREG(SOC_LCDC_0_REGS + LCDC_LCDDMA_FB1_CEILING);
    snapshot->raster_control =
        (unsigned long)HWREG(SOC_LCDC_0_REGS + LCDC_RASTER_CTRL);
    snapshot->raster_status =
        (unsigned long)HWREG(SOC_LCDC_0_REGS + LCDC_LCD_STAT);
    snapshot->dma_control =
        (unsigned long)HWREG(SOC_LCDC_0_REGS + LCDC_LCDDMA_CTRL);
    /* C6748 exposes raster interrupt state through LCD_STAT, not IRQSTATUS. */
    snapshot->irq_status = 0UL;
#endif
}

#if EQ_ENABLE_TEN_BAND_EDITOR != 0
static void EQ_RecordSwapTrace(unsigned long eof_mask)
{
    EQ_LCD_HW_SNAPSHOT snapshot;
    volatile EQ_LCD_SWAP_TRACE_ENTRY *entry;
    unsigned int index;

    EQ_ReadHardwareSnapshot(&snapshot);
    index = EQ_DebugLcdSwapTraceWriteIndex;
    entry = &EQ_DebugLcdSwapTrace[index];
#if defined(__TI_COMPILER_VERSION__) || defined(__TMS320C6X__)
    entry->cycle = (unsigned long)TSCL;
#elif defined(EQ_ALGO_ONLY)
    entry->cycle = s_mock_cycle_clock;
#else
    entry->cycle = 0UL;
#endif
    entry->process_frame = s_swap_trace_process_frame;
    entry->eof_mask = eof_mask & EQ_LCD_STATUS_EOF_MASK;
    entry->frame_base = snapshot.frame_base;
    entry->frame1_base = snapshot.frame1_base;
    entry->front_page = s_front_page;
    entry->target_page = s_swap_target_page;
    entry->swap_pending = s_swap_pending;
    entry->descriptor_mask = s_swap_descriptor_mask;
    entry->swap_complete = s_swap_complete;
    entry->raster_status = snapshot.raster_status;
    index++;
    if (index >= EQ_LCD_SWAP_TRACE_DEPTH)
    {
        index = 0U;
    }
    EQ_DebugLcdSwapTraceWriteIndex = index;
    if (EQ_DebugLcdSwapTraceCount < EQ_LCD_SWAP_TRACE_DEPTH)
    {
        EQ_DebugLcdSwapTraceCount++;
    }
    else
    {
        EQ_DebugLcdSwapTraceWrapCount++;
    }
}
#endif

static void EQ_ClearStartupFaultStatus(void)
{
    EQ_LCD_HW_SNAPSHOT snapshot;
    unsigned long clear_mask;

    EQ_ReadHardwareSnapshot(&snapshot);
    EQ_DebugLcdStartupRasterStatus = snapshot.raster_status;
    clear_mask = snapshot.raster_status & EQ_LCD_CLEARABLE_FAULT_MASK;
    if (clear_mask != 0UL)
    {
#if defined(EQ_ALGO_ONLY)
        s_mock_hw_snapshot.raster_status &= ~clear_mask;
#else
        (void)RasterClearGetIntStatus(SOC_LCDC_0_REGS,
                                     (unsigned int)clear_mask);
#endif
        EQ_DebugLcdStartupFaultClearCount++;
    }
    EQ_ReadHardwareSnapshot(&snapshot);
    EQ_DebugLcdStartupStatusAfterClear = snapshot.raster_status;
}

static void EQ_CaptureFramebufferCanary(void)
{
#if defined(EQ_ALGO_ONLY)
    s_lcd_canary_front = 0U;
#if EQ_ENABLE_TEN_BAND_EDITOR != 0
    s_lcd_canary_editor = 0U;
#endif
#else
    volatile unsigned int *front;

    front = (volatile unsigned int *)(Lcd_Buffer);
    s_lcd_canary_front = *front;
#if EQ_ENABLE_TEN_BAND_EDITOR != 0
    front = (volatile unsigned int *)(EQ_LcdEditorBuffer);
    s_lcd_canary_editor = *front;
#endif
#endif
    s_lcd_canary_ready = 1;
}

static void EQ_LatchHardwareFault(unsigned long process_frame,
                                  unsigned long reason)
{
    if (EQ_DebugLcdFaultLatched == 0U)
    {
        EQ_DebugLcdFaultLatched = 1U;
        EQ_DebugLcdFirstFaultFrame = process_frame;
        EQ_DebugLcdFirstFaultJob =
            (unsigned long)EQ_DebugLcdLastJob;
    }
    EqualizerDisplay_AutoDisable(reason);
}

static int EQ_CheckFramebufferCanary(unsigned long process_frame)
{
    int failed;

    EQ_DebugLcdFramebufferCanaryCheckCount++;
    failed = 0;
    if (s_lcd_canary_ready == 0)
    {
        failed = 1;
    }
#if defined(EQ_ALGO_ONLY)
    if (s_mock_canary_failed != 0)
    {
        failed = 1;
    }
#else
    else
    {
        volatile unsigned int *front;

        front = (volatile unsigned int *)(Lcd_Buffer);
        if ((*front != s_lcd_canary_front) ||
            (EQ_DebugLcdBufferAddress !=
             EQ_PageBufferAddress(EQ_UI_PAGE_DYNAMIC_STATUS)) ||
            (EQ_DebugLcdBufferEndAddress !=
             (EQ_PageBufferAddress(EQ_UI_PAGE_DYNAMIC_STATUS) +
              EQ_LCD_BUFFER_TOTAL_BYTES - 1UL)) ||
            (EQ_DebugLcdExpectedFrameBase !=
             EQ_ExpectedFrameBaseForPage(EQ_DebugLcdFrontPage)) ||
            (EQ_DebugLcdExpectedFrameEnd !=
             EQ_ExpectedFrameEndForPage(EQ_DebugLcdFrontPage)))
        {
            failed = 1;
        }
#if EQ_ENABLE_TEN_BAND_EDITOR != 0
        front = (volatile unsigned int *)(EQ_LcdEditorBuffer);
        if ((*front != s_lcd_canary_editor) ||
            (EQ_DebugLcdEditorBufferAddress !=
             EQ_PageBufferAddress(EQ_UI_PAGE_EQ_EDITOR)) ||
            (EQ_DebugLcdEditorBufferEndAddress !=
             (EQ_PageBufferAddress(EQ_UI_PAGE_EQ_EDITOR) +
              EQ_LCD_BUFFER_TOTAL_BYTES - 1UL)))
        {
            failed = 1;
        }
#endif
    }
#endif
    if (failed != 0)
    {
        if (EQ_DebugLcdFramebufferCanaryFailureCount == 0UL)
        {
            EQ_DebugLcdFramebufferFirstFailureFrame = process_frame;
            EQ_DebugLcdFramebufferFirstFailureJob =
                (unsigned long)EQ_DebugLcdLastJob;
        }
        EQ_DebugLcdFramebufferCanaryFailureCount++;
    }
    return failed;
}

void EqualizerDisplay_AuditHardware(unsigned long process_frame, int force)
{
    EQ_LCD_HW_SNAPSHOT snapshot;
    unsigned long status;
#if EQ_ENABLE_TEN_BAND_EDITOR != 0
    unsigned long target_base;
    unsigned long target_end;
    unsigned int eof_fault_pending;
#endif
    int due;
    int canary_failed;
    int frame_mismatch;
    int hardware_fault;

#if EQ_LCD_DIAGNOSTIC_ALIGNMENT_PATTERN
    EQ_DebugLcdRuntimeMask = 0U;
    EQ_DebugLcdPendingMask = 0UL;
#endif
    due = force != 0;
    if (EQ_DebugLcdHardwareAuditRequest != 0U)
    {
        due = 1;
    }
    if ((EQ_DebugLcdJobCount - s_lcd_last_audit_job) >=
        EQ_LCD_HW_AUDIT_JOB_INTERVAL)
    {
        due = 1;
    }
#if EQ_LCD_DIAGNOSTIC_ALIGNMENT_PATTERN
    if ((process_frame - s_lcd_last_audit_frame) >=
        EQ_LCD_HW_AUDIT_FRAME_INTERVAL)
    {
        due = 1;
    }
#endif
    if (due == 0)
    {
        return;
    }
    EQ_DebugLcdHardwareAuditRequest = 0U;
    s_lcd_last_audit_job = EQ_DebugLcdJobCount;
#if EQ_LCD_DIAGNOSTIC_ALIGNMENT_PATTERN
    s_lcd_last_audit_frame = process_frame;
#endif

#if EQ_ENABLE_TEN_BAND_EDITOR != 0
    EQ_SwapStateLock();
#endif
    canary_failed = EQ_CheckFramebufferCanary(process_frame);
    EQ_ReadHardwareSnapshot(&snapshot);
    EQ_CopyHardwareSnapshotToDebug(&snapshot);
    status = snapshot.raster_status;
#if EQ_ENABLE_TEN_BAND_EDITOR != 0
    target_base = EQ_ExpectedFrameBaseForPage(s_swap_target_page);
    target_end = EQ_ExpectedFrameEndForPage(s_swap_target_page);
    if (s_swap_pending != 0U)
    {
        frame_mismatch =
            (((snapshot.frame_base != EQ_DebugLcdExpectedFrameBase) ||
              (snapshot.frame_end != EQ_DebugLcdExpectedFrameEnd)) &&
             ((snapshot.frame_base != target_base) ||
              (snapshot.frame_end != target_end))) ||
            (((snapshot.frame1_base != EQ_DebugLcdExpectedFrameBase) ||
              (snapshot.frame1_end != EQ_DebugLcdExpectedFrameEnd)) &&
             ((snapshot.frame1_base != target_base) ||
              (snapshot.frame1_end != target_end)));
    }
    else
    {
        frame_mismatch =
            (snapshot.frame_base != EQ_DebugLcdExpectedFrameBase) ||
            (snapshot.frame_end != EQ_DebugLcdExpectedFrameEnd) ||
            (snapshot.frame1_base != EQ_DebugLcdExpectedFrameBase) ||
            (snapshot.frame1_end != EQ_DebugLcdExpectedFrameEnd);
    }
    eof_fault_pending = s_eof_fault_pending;
    EQ_SwapStateUnlock();
#else
    frame_mismatch =
        (snapshot.frame_base != EQ_DebugLcdExpectedFrameBase) ||
        (snapshot.frame_end != EQ_DebugLcdExpectedFrameEnd);
#endif
    hardware_fault = 0;

    if ((status & EQ_LCD_STATUS_SYNC_MASK) != 0UL)
    {
        if (s_lcd_seen_sync_lost == 0)
        {
            EQ_DebugLcdSyncLostCount++;
            s_lcd_seen_sync_lost = 1;
        }
        hardware_fault = 1;
    }
    if ((status & EQ_LCD_STATUS_FIFO_UNDERFLOW_MASK) != 0UL)
    {
        if (s_lcd_seen_fifo_underflow == 0)
        {
            EQ_DebugLcdFifoUnderflowCount++;
            s_lcd_seen_fifo_underflow = 1;
        }
        hardware_fault = 1;
    }
    if (frame_mismatch != 0)
    {
        if (s_lcd_seen_frame_mismatch == 0)
        {
            EQ_DebugLcdFrameAddressMismatchCount++;
            s_lcd_seen_frame_mismatch = 1;
        }
        hardware_fault = 1;
    }
    if ((snapshot.raster_control & EQ_LCD_RASTER_ENABLE_MASK) == 0UL)
    {
        hardware_fault = 1;
    }
#if EQ_ENABLE_TEN_BAND_EDITOR != 0
    if (eof_fault_pending != 0U)
    {
        hardware_fault = 1;
    }
#endif
    if (hardware_fault != 0)
    {
        if (EQ_DebugLcdRasterFaultCount == 0UL)
        {
            EQ_DebugLcdRasterFaultCount++;
        }
    }
    if ((hardware_fault != 0) || (canary_failed != 0))
    {
        EQ_LatchHardwareFault(
            process_frame,
            ((hardware_fault != 0) ? EQ_LCD_AUTO_DISABLE_HW_FAULT : 0UL) |
            ((canary_failed != 0) ? EQ_LCD_AUTO_DISABLE_CANARY : 0UL));
    }
}

static int EQ_ClampInt(int value, int minimum, int maximum)
{
    if (value < minimum)
    {
        return minimum;
    }
    if (value > maximum)
    {
        return maximum;
    }
    return value;
}

#if EQ_ENABLE_TEN_BAND_EDITOR != 0
static void EQ_ConfigFrameDescriptorRaw(unsigned int descriptor, int page)
{
    unsigned long frame_base;
    unsigned long frame_end;

    frame_base = EQ_ExpectedFrameBaseForPage(page);
    frame_end = EQ_ExpectedFrameEndForPage(page);
#if defined(EQ_ALGO_ONLY)
    if (descriptor == EQ_LCD_DESCRIPTOR_FB0)
    {
        s_mock_hw_snapshot.frame_base = frame_base;
        s_mock_hw_snapshot.frame_end = frame_end;
    }
    else
    {
        s_mock_hw_snapshot.frame1_base = frame_base;
        s_mock_hw_snapshot.frame1_end = frame_end;
    }
#else
    RasterDMAFBConfig(SOC_LCDC_0_REGS,
                      (unsigned int)frame_base,
                      (unsigned int)frame_end,
                      (descriptor == EQ_LCD_DESCRIPTOR_FB0) ?
                          LCD_FRAME_0 : LCD_FRAME_1);
#endif
}

static void EQ_ConfigSwapDescriptor(unsigned int descriptor)
{
    if ((s_swap_descriptor_mask & descriptor) != 0U)
    {
        return;
    }
    EQ_ConfigFrameDescriptorRaw(descriptor, s_swap_target_page);
    s_swap_descriptor_mask |= descriptor;
    EQ_DebugLcdSwapDescriptorMask = s_swap_descriptor_mask;
}

static void EQ_CompleteFrontPage(void)
{
    if (s_swap_descriptor_mask != EQ_LCD_DESCRIPTOR_BOTH)
    {
        return;
    }
    s_front_page = s_swap_target_page;
    s_swap_pending = 0U;
    s_swap_complete = 1U;
    EQ_DebugLcdSwapPending = 0U;
    EQ_DebugLcdSwapCompleteCount++;
    EQ_SetExpectedFrontPage(s_front_page);
}

static void EQ_HandleEofStatus(unsigned long status)
{
    unsigned long eof_status;
    unsigned int descriptor;
#if EQ_ENABLE_LCD_JOB_TIMING_CAPTURE != 0
    unsigned long descriptor_start_cycles;
    unsigned long descriptor_end_cycles;
#endif

    eof_status = status & EQ_LCD_STATUS_EOF_MASK;
    if (eof_status == 0UL)
    {
        return;
    }
    if ((eof_status & EQ_LCD_STATUS_EOF0_MASK) != 0UL)
    {
        EQ_DebugLcdEof0Count++;
    }
    if ((eof_status & EQ_LCD_STATUS_EOF1_MASK) != 0UL)
    {
        EQ_DebugLcdEof1Count++;
    }
    if (eof_status == EQ_LCD_STATUS_EOF_MASK)
    {
        if (s_swap_pending == 0U)
        {
            /* A debugger halt can leave both sticky EOF bits set while idle. */
            EQ_DebugLcdEofCount += 2UL;
            EQ_RecordSwapTrace(eof_status);
            return;
        }
        EQ_DebugLcdEofAmbiguousCount++;
        s_eof_fault_pending = 1U;
        EQ_DebugLcdHardwareAuditRequest = 1U;
        EQ_RecordSwapTrace(eof_status);
        return;
    }
    EQ_DebugLcdEofCount++;
    if (s_swap_pending == 0U)
    {
        EQ_RecordSwapTrace(eof_status);
        return;
    }
    descriptor = ((eof_status & EQ_LCD_STATUS_EOF0_MASK) != 0UL) ?
        EQ_LCD_DESCRIPTOR_FB0 : EQ_LCD_DESCRIPTOR_FB1;
#if EQ_ENABLE_LCD_JOB_TIMING_CAPTURE != 0
    descriptor_start_cycles = EQ_ReadCycles();
#endif
    EQ_ConfigSwapDescriptor(descriptor);
    if (s_swap_descriptor_mask == EQ_LCD_DESCRIPTOR_BOTH)
    {
        EQ_CompleteFrontPage();
    }
#if EQ_ENABLE_LCD_JOB_TIMING_CAPTURE != 0
    descriptor_end_cycles = EQ_ReadCycles();
    EQ_RecordLcdTimingSample(
        EQ_LCD_TIMING_CLASS_PAGE_SWAP,
        descriptor_end_cycles - descriptor_start_cycles);
#endif
    EQ_RecordSwapTrace(eof_status);
}

#if !defined(EQ_ALGO_ONLY)
static void EQ_LcdEndOfFrameIsr(void)
{
    unsigned long status;

    status = (unsigned long)RasterClearGetIntStatus(
        SOC_LCDC_0_REGS, (unsigned int)EQ_LCD_STATUS_EOF_MASK);
    EQ_HandleEofStatus(status);
    IntEventClear(SYS_INT_LCDC_INT);
}
#endif

static int EQ_StopRasterForReconfigure(unsigned long maximum_spins)
{
#if defined(EQ_ALGO_ONLY)
    (void)maximum_spins;
    s_mock_hw_snapshot.raster_control &= ~EQ_LCD_RASTER_ENABLE_MASK;
    return 1;
#else
    unsigned long spins;

    (void)RasterClearGetIntStatus(
        SOC_LCDC_0_REGS, (unsigned int)EQ_LCD_STATUS_DONE_MASK);
    RasterDisable(SOC_LCDC_0_REGS);
    spins = 0UL;
    while (((unsigned long)HWREG(SOC_LCDC_0_REGS + LCDC_LCD_STAT) &
            EQ_LCD_STATUS_DONE_MASK) == 0UL)
    {
        if (spins >= maximum_spins)
        {
            EQ_DebugLcdRasterStopTimeoutCount++;
            return 0;
        }
        spins++;
    }
    (void)RasterClearGetIntStatus(
        SOC_LCDC_0_REGS, (unsigned int)EQ_LCD_STATUS_DONE_MASK);
    return 1;
#endif
}

static void EQ_PageSwapInit(void)
{
    s_draw_page = EQ_UI_PAGE_DYNAMIC_STATUS;
    s_front_page = EQ_UI_PAGE_DYNAMIC_STATUS;
    s_swap_target_page = EQ_UI_PAGE_DYNAMIC_STATUS;
    s_swap_pending = 0U;
    s_swap_complete = 0U;
    s_swap_descriptor_mask = 0U;
#if defined(EQ_ALGO_ONLY)
    s_eof_fault_pending = 0U;
#else
    s_eof_fault_pending =
        (s_raster_reconfigure_ready != 0U) ? 0U : 1U;
#endif
    s_deferred_page_valid = 0U;
    s_deferred_page = EQ_UI_PAGE_DYNAMIC_STATUS;
    s_cancel_after_swap = 0U;
    EQ_DebugLcdSwapDescriptorMask = 0U;
#if defined(EQ_ALGO_ONLY)
    s_mock_next_eof_descriptor = EQ_LCD_DESCRIPTOR_FB0;
    EQ_ConfigFrameDescriptorRaw(EQ_LCD_DESCRIPTOR_FB0,
                                EQ_UI_PAGE_DYNAMIC_STATUS);
    EQ_ConfigFrameDescriptorRaw(EQ_LCD_DESCRIPTOR_FB1,
                                EQ_UI_PAGE_DYNAMIC_STATUS);
#else
    if (s_raster_reconfigure_ready == 0U)
    {
        return;
    }
    IntDisable(C674X_MASK_INT14);
    RasterEndOfFrameIntDisable(SOC_LCDC_0_REGS);
    RasterDMAConfig(SOC_LCDC_0_REGS, RASTER_DOUBLE_FRAME_BUFFER,
                    RASTER_BURST_SIZE_16, RASTER_FIFO_THRESHOLD_8,
                    RASTER_BIG_ENDIAN_DISABLE);
    EQ_ConfigFrameDescriptorRaw(EQ_LCD_DESCRIPTOR_FB0,
                                EQ_UI_PAGE_DYNAMIC_STATUS);
    EQ_ConfigFrameDescriptorRaw(EQ_LCD_DESCRIPTOR_FB1,
                                EQ_UI_PAGE_DYNAMIC_STATUS);
    (void)RasterClearGetIntStatus(
        SOC_LCDC_0_REGS, (unsigned int)EQ_LCD_STATUS_EOF_MASK);
    IntRegister(C674X_MASK_INT14, EQ_LcdEndOfFrameIsr);
    IntEventMap(C674X_MASK_INT14, SYS_INT_LCDC_INT);
    IntEventClear(SYS_INT_LCDC_INT);
    RasterEndOfFrameIntEnable(SOC_LCDC_0_REGS);
    IntEnable(C674X_MASK_INT14);
#endif
}

static void EQ_CancelPageSwap(void)
{
    if ((s_swap_pending != 0U) &&
        (s_swap_descriptor_mask == 0U) &&
        (s_front_page == s_ui_state.displayed_page))
    {
        s_swap_pending = 0U;
        s_swap_complete = 0U;
        s_swap_target_page = s_front_page;
        s_swap_descriptor_mask = 0U;
        EQ_DebugLcdSwapPending = 0U;
        EQ_DebugLcdSwapTargetPage = s_front_page;
        EQ_DebugLcdSwapDescriptorMask = 0U;
    }
}

static void EQ_ForceStopPageSwap(void)
{
    int raster_stopped;

#if !defined(EQ_ALGO_ONLY)
    RasterEndOfFrameIntDisable(SOC_LCDC_0_REGS);
#endif
    raster_stopped = EQ_StopRasterForReconfigure(
        EQ_LCD_FAULT_STOP_POLL_SPINS);
    if (raster_stopped != 0)
    {
        EQ_ConfigFrameDescriptorRaw(EQ_LCD_DESCRIPTOR_FB0, s_front_page);
        EQ_ConfigFrameDescriptorRaw(EQ_LCD_DESCRIPTOR_FB1, s_front_page);
        s_swap_descriptor_mask = 0U;
        EQ_DebugLcdSwapDescriptorMask = 0U;
    }
#if !defined(EQ_ALGO_ONLY)
    (void)RasterClearGetIntStatus(
        SOC_LCDC_0_REGS, (unsigned int)EQ_LCD_STATUS_EOF_MASK);
    IntEventClear(SYS_INT_LCDC_INT);
#endif
    s_swap_target_page = s_front_page;
    s_swap_pending = 0U;
    s_swap_complete = 0U;
    s_deferred_page_valid = 0U;
    s_cancel_after_swap = 0U;
    EQ_DebugLcdSwapTargetPage = s_front_page;
    EQ_DebugLcdSwapPending = 0U;
    EQ_SetExpectedFrontPage(s_front_page);
}

static int EQ_ServicePageSwap(int page)
{
    int completed;

    completed = 0;
    EQ_SwapStateLock();
    if ((s_swap_complete != 0U) && (s_front_page == page))
    {
        s_swap_complete = 0U;
        completed = 1;
    }
    else if (s_swap_pending == 0U)
    {
        if (s_front_page == page)
        {
            completed = 1;
        }
        else
        {
            s_swap_target_page = page;
            s_swap_complete = 0U;
            s_swap_descriptor_mask = 0U;
            EQ_DebugLcdSwapTargetPage = page;
            EQ_DebugLcdSwapDescriptorMask = 0U;
            EQ_DebugLcdSwapPending = 1U;
            EQ_DebugLcdSwapRequestCount++;
            s_swap_pending = 1U;
            EQ_RecordSwapTrace(0UL);
        }
    }
    else
    {
#if defined(EQ_ALGO_ONLY)
        if (s_mock_next_eof_descriptor == EQ_LCD_DESCRIPTOR_FB0)
        {
            EQ_HandleEofStatus(EQ_LCD_STATUS_EOF0_MASK);
            s_mock_next_eof_descriptor = EQ_LCD_DESCRIPTOR_FB1;
        }
        else
        {
            EQ_HandleEofStatus(EQ_LCD_STATUS_EOF1_MASK);
            s_mock_next_eof_descriptor = EQ_LCD_DESCRIPTOR_FB0;
        }
        if ((s_swap_complete != 0U) && (s_front_page == page))
        {
            s_swap_complete = 0U;
            completed = 1;
        }
#endif
    }
    EQ_SwapStateUnlock();
    return completed;
}

static void EQ_ApplyDeferredPageRequest(unsigned long process_frame)
{
    EQ_UI_SNAPSHOT deferred;
    int page;

    if (s_cancel_after_swap != 0U)
    {
        s_cancel_after_swap = 0U;
        s_deferred_page_valid = 0U;
        EqualizerUiLogic_Cancel(&s_ui_state);
        return;
    }
    if (s_deferred_page_valid == 0U)
    {
        return;
    }
    page = s_deferred_page;
    s_deferred_page_valid = 0U;
    if (page == s_ui_state.displayed_page)
    {
        return;
    }
    deferred = s_ui_state.requested;
    deferred.page = page;
    EqualizerUiLogic_Request(&s_ui_state, &deferred,
                             EQ_DebugLcdRuntimeMask, process_frame);
}

#endif

static void EQ_DetectFramebufferCacheMode(void)
{
#if defined(EQ_ALGO_ONLY)
    EQ_DebugLcdBufferMar = 0UL;
    EQ_DebugLcdEditorBufferMar = 0UL;
    EQ_DebugLcdCacheMode = EQ_LCD_CACHE_NONCACHEABLE;
#else
    unsigned int front_mar_index;
    unsigned int editor_mar_index;
    unsigned long front_mar;
    unsigned long editor_mar;

    front_mar_index =
        (unsigned int)((EQ_PageBufferAddress(
            EQ_UI_PAGE_DYNAMIC_STATUS) >> 24) & 0xFFUL);
    front_mar = (unsigned long)HWREG(
        SOC_CACHE_0_REGS + DSPCACHE_MAR(front_mar_index));
#if EQ_ENABLE_TEN_BAND_EDITOR != 0
    editor_mar_index =
        (unsigned int)((EQ_PageBufferAddress(
            EQ_UI_PAGE_EQ_EDITOR) >> 24) & 0xFFUL);
    editor_mar = (unsigned long)HWREG(
        SOC_CACHE_0_REGS + DSPCACHE_MAR(editor_mar_index));
#else
    editor_mar_index = front_mar_index;
    editor_mar = front_mar;
#endif
    (void)editor_mar_index;
    EQ_DebugLcdBufferMar = front_mar;
    EQ_DebugLcdEditorBufferMar = editor_mar;
    if (((front_mar & DSPCACHE_MAR_PC) == 0UL) &&
        ((editor_mar & DSPCACHE_MAR_PC) == 0UL))
    {
        EQ_DebugLcdCacheMode = EQ_LCD_CACHE_NONCACHEABLE;
    }
    else if (((front_mar & DSPCACHE_MAR_PC) != 0UL) &&
             ((editor_mar & DSPCACHE_MAR_PC) != 0UL))
    {
        EQ_DebugLcdCacheMode = EQ_LCD_CACHE_CACHEABLE;
    }
    else
    {
        EQ_DebugLcdCacheMode = EQ_LCD_CACHE_MIXED;
    }
#endif
}

static void EQ_ResetDirtyBounds(void)
{
    int page;

    for (page = 0; page < EQ_LCD_PAGE_COUNT; page++)
    {
        s_dirty_valid[page] = 0U;
        s_dirty_x0[page] = EQ_UI_SCREEN_WIDTH;
        s_dirty_y0[page] = EQ_UI_SCREEN_HEIGHT;
        s_dirty_x1[page] = 0;
        s_dirty_y1[page] = 0;
    }
    EQ_DebugLcdPendingDirtyRegionCount = 0U;
}

static void EQ_UpdatePendingDirtyRegionCount(void)
{
    int page;
    unsigned int count;

    count = 0U;
    for (page = 0; page < EQ_LCD_PAGE_COUNT; page++)
    {
        if (s_dirty_valid[page] != 0U)
        {
            count++;
        }
    }
    EQ_DebugLcdPendingDirtyRegionCount = count;
    if (count > EQ_DebugLcdMaxPendingDirtyRegionCount)
    {
        EQ_DebugLcdMaxPendingDirtyRegionCount = count;
    }
}

static void EQ_RecordDirtyRect(int x, int y, int w, int h)
{
    int page;
    int x1;
    int y1;

#if EQ_ENABLE_TEN_BAND_EDITOR != 0
    page = s_draw_page;
#else
    page = EQ_UI_PAGE_DYNAMIC_STATUS;
#endif
    if ((page < 0) || (page >= EQ_LCD_PAGE_COUNT) ||
        (w <= 0) || (h <= 0))
    {
        return;
    }
    x1 = x + w;
    y1 = y + h;
    if (s_dirty_valid[page] == 0U)
    {
        s_dirty_x0[page] = x;
        s_dirty_y0[page] = y;
        s_dirty_x1[page] = x1;
        s_dirty_y1[page] = y1;
        s_dirty_valid[page] = 1U;
        EQ_UpdatePendingDirtyRegionCount();
        return;
    }
    if (x < s_dirty_x0[page])
        s_dirty_x0[page] = x;
    if (y < s_dirty_y0[page])
        s_dirty_y0[page] = y;
    if (x1 > s_dirty_x1[page])
        s_dirty_x1[page] = x1;
    if (y1 > s_dirty_y1[page])
        s_dirty_y1[page] = y1;
}

static void EQ_WritebackRange(unsigned long address, unsigned long bytes)
{
    if ((bytes == 0UL) ||
        (EQ_DebugLcdCacheMode != EQ_LCD_CACHE_CACHEABLE))
    {
        return;
    }
#if !defined(EQ_ALGO_ONLY)
    CacheWB((unsigned int)address, (unsigned int)bytes);
#else
    (void)address;
#endif
    EQ_DebugLcdWritebackCount++;
    EQ_DebugLcdWritebackBytes += bytes;
}

static void EQ_FlushDirtyRegions(void)
{
    int page;
    unsigned long address;
    unsigned long end;
    unsigned long bytes;

    if (EQ_DebugLcdCacheMode != EQ_LCD_CACHE_CACHEABLE)
    {
        EQ_ResetDirtyBounds();
        return;
    }
    for (page = 0; page < EQ_LCD_PAGE_COUNT; page++)
    {
        if (s_dirty_valid[page] == 0U)
        {
            continue;
        }
        if (s_runtime_started == 0)
        {
            address = EQ_ExpectedFrameBaseForPage(page);
            bytes = EQ_LCD_FRAME_DMA_BYTES;
        }
        else if ((s_dirty_x0[page] < 0) ||
                 (s_dirty_y0[page] < 0) ||
                 (s_dirty_x1[page] > EQ_UI_SCREEN_WIDTH) ||
                 (s_dirty_y1[page] > EQ_UI_SCREEN_HEIGHT) ||
                 (s_dirty_x0[page] >= s_dirty_x1[page]) ||
                 (s_dirty_y0[page] >= s_dirty_y1[page]))
        {
            EQ_DebugLcdWritebackFailureCount++;
            continue;
        }
        else
        {
            address = EQ_PageBufferAddress(page) +
                EQ_LCD_PALETTE_OFFSET + EQ_LCD_PALETTE_SIZE +
                (((unsigned long)s_dirty_y0[page] *
                  (unsigned long)EQ_UI_SCREEN_WIDTH +
                  (unsigned long)s_dirty_x0[page]) *
                 EQ_LCD_BYTES_PER_PIXEL);
            end = EQ_PageBufferAddress(page) +
                EQ_LCD_PALETTE_OFFSET + EQ_LCD_PALETTE_SIZE +
                ((((unsigned long)(s_dirty_y1[page] - 1) *
                   (unsigned long)EQ_UI_SCREEN_WIDTH) +
                  (unsigned long)s_dirty_x1[page]) *
                 EQ_LCD_BYTES_PER_PIXEL);
            bytes = end - address;
        }
        EQ_WritebackRange(address, bytes);
    }
    EQ_ResetDirtyBounds();
}

static int EQ_BeginDraw(void)
{
    if (s_lcd_busy != 0)
    {
        EQ_DebugLcdSkipBusyCount++;
        return 0;
    }
    s_lcd_busy = 1;
    EQ_ResetDirtyBounds();
    return 1;
}

static void EQ_EndDraw(void)
{
    EQ_FlushDirtyRegions();
    s_lcd_busy = 0;
}

static int EQ_CheckRect(int x, int y, int w, int h)
{
    if ((x < 0) || (y < 0) || (w <= 0) || (h <= 0) ||
        ((x + w) > EQ_UI_SCREEN_WIDTH) ||
        ((y + h) > EQ_UI_SCREEN_HEIGHT))
    {
        EQ_DebugLcdBoundsFailureCount++;
        return 0;
    }
    return 1;
}

#if defined(EQ_ALGO_ONLY)
static int EQ_TracePage(void)
{
#if EQ_ENABLE_TEN_BAND_EDITOR != 0
    return s_draw_page;
#else
    return EQ_UI_PAGE_DYNAMIC_STATUS;
#endif
}

static void EQ_TraceJsonString(const char *text, int length)
{
    int index;

    fputc('"', s_mock_trace_file);
    for (index = 0; index < length; index++)
    {
        unsigned char value;

        value = (unsigned char)text[index];
        if ((value == '"') || (value == '\\'))
        {
            fputc('\\', s_mock_trace_file);
            fputc((int)value, s_mock_trace_file);
        }
        else if (value == '\n')
        {
            fputs("\\n", s_mock_trace_file);
        }
        else if ((value >= 0x20U) && (value < 0x7FU))
        {
            fputc((int)value, s_mock_trace_file);
        }
        else
        {
            fprintf(s_mock_trace_file, "\\u%04X", (unsigned int)value);
        }
    }
    fputc('"', s_mock_trace_file);
}

static void EQ_TracePrimitive(const char *operation,
                              int x, int y, int w, int h, int color,
                              const char *text, int text_length,
                              int glyph_id, int font, int centered)
{
    if (s_mock_trace_file == 0)
    {
        return;
    }
    fprintf(s_mock_trace_file,
            "{\"draw_sequence\":%lu,\"operation\":\"%s\","
            "\"x\":%d,\"y\":%d,\"w\":%d,\"h\":%d,"
            "\"color\":%d,\"job\":%d,\"page\":%d,"
            "\"frame\":%lu,\"font\":%d,\"centered\":%d,"
            "\"text_or_glyph_id\":",
            s_mock_trace_record_count, operation,
            x, y, w, h, color, s_mock_trace_job, EQ_TracePage(),
            s_mock_trace_frame, font, centered);
    if (text != 0)
    {
        EQ_TraceJsonString(text, text_length);
    }
    else if (glyph_id >= 0)
    {
        fprintf(s_mock_trace_file, "%d", glyph_id);
    }
    else
    {
        fputs("null", s_mock_trace_file);
    }
    fputs("}\n", s_mock_trace_file);
    s_mock_trace_record_count++;
}
#endif

#if !defined(EQ_ALGO_ONLY)
static unsigned long EQ_MapColor(int color)
{
    switch (color)
    {
        case EQ_COLOR_TEXT:
            return ClrWhite;
        case EQ_COLOR_MUTED:
            return ClrLightSteelBlue;
        case EQ_COLOR_BORDER:
            return ClrGray;
        case EQ_COLOR_HIGHLIGHT:
            return ClrDodgerBlue;
        case EQ_COLOR_ACTIVE:
            return ClrLimeGreen;
        case EQ_COLOR_BAR_POS:
            return ClrDeepSkyBlue;
        case EQ_COLOR_BAR_NEG:
            return ClrOrangeRed;
        case EQ_COLOR_ZERO:
            return ClrGold;
        case EQ_COLOR_BG:
        default:
            return ClrBlack;
    }
}

static void EQ_SetFont(int font)
{
    if (font == EQ_FONT_LARGE)
    {
        GrContextFontSet(EQ_DrawContext(), &g_sFontCm24);
    }
    else if (font == EQ_FONT_MEDIUM)
    {
        GrContextFontSet(EQ_DrawContext(), &g_sFontCm18);
    }
    else
    {
        GrContextFontSet(EQ_DrawContext(), &g_sFontCm12);
    }
}
#else
static void EQ_SetFont(int font)
{
    (void)font;
}
#endif

#if !defined(EQ_ALGO_ONLY)
static unsigned short EQ_LcdColor16(int color)
{
    return (unsigned short)DpyColorTranslate(
        EQ_DrawDisplay(), (unsigned int)EQ_MapColor(color));
}

static void EQ_LcdFillRect16(int x, int y, int w, int h,
                             unsigned short pixel_value)
{
    unsigned int packed_value;
    int row;

    packed_value = (unsigned int)pixel_value |
                   ((unsigned int)pixel_value << 16);
    for (row = 0; row < h; row++)
    {
        unsigned short *pixel;
        int remaining;

        pixel = (unsigned short *)(EQ_DrawBuffer() +
                                   EQ_LCD_PALETTE_OFFSET +
                                   EQ_LCD_PALETTE_SIZE) +
                (y + row) * EQ_UI_SCREEN_WIDTH + x;
        remaining = w;
        if ((((unsigned long)pixel & 2UL) != 0UL) && (remaining > 0))
        {
            *pixel++ = pixel_value;
            remaining--;
        }
        while (remaining >= 2)
        {
            *((unsigned int *)pixel) = packed_value;
            pixel += 2;
            remaining -= 2;
        }
        if (remaining != 0)
        {
            *pixel = pixel_value;
        }
    }
}
#endif

static void EQ_LcdFillRect(int x, int y, int w, int h, int color)
{
    if (EQ_CheckRect(x, y, w, h) == 0)
    {
        return;
    }
    EQ_RecordDirtyRect(x, y, w, h);
#if !defined(EQ_ALGO_ONLY)
    /* The bundled Grlib library is a debug build.  Packed framebuffer writes
       keep runtime tiles bounded while preserving its RGB565 representation. */
    EQ_LcdFillRect16(x, y, w, h, EQ_LcdColor16(color));
#else
    EQ_TracePrimitive("fill_rect", x, y, w, h, color,
                      0, 0, -1, -1, 0);
    s_mock_primitive_count++;
#endif
}

static void EQ_LcdFillRectStartup(int x, int y, int w, int h, int color)
{
#if !defined(EQ_ALGO_ONLY)
    tRectangle rect;

    if (EQ_CheckRect(x, y, w, h) == 0)
    {
        return;
    }
    EQ_RecordDirtyRect(x, y, w, h);
    rect.sXMin = x;
    rect.sYMin = y;
    rect.sXMax = x + w - 1;
    rect.sYMax = y + h - 1;
    GrContextForegroundSet(EQ_DrawContext(), EQ_MapColor(color));
    GrRectFill(EQ_DrawContext(), &rect);
#else
    EQ_LcdFillRect(x, y, w, h, color);
#endif
}

static void EQ_LcdDrawRect(int x, int y, int w, int h, int color)
{
#if !defined(EQ_ALGO_ONLY)
    tRectangle rect;
#endif

    if (EQ_CheckRect(x, y, w, h) == 0)
    {
        return;
    }
    EQ_RecordDirtyRect(x, y, w, h);
#if !defined(EQ_ALGO_ONLY)
    rect.sXMin = x;
    rect.sYMin = y;
    rect.sXMax = x + w - 1;
    rect.sYMax = y + h - 1;
    GrContextForegroundSet(EQ_DrawContext(), EQ_MapColor(color));
    GrRectDraw(EQ_DrawContext(), &rect);
#else
    EQ_TracePrimitive("draw_rect", x, y, w, h, color,
                      0, 0, -1, -1, 0);
    s_mock_primitive_count++;
#endif
}

static void EQ_LcdDrawLine(int x1, int y1, int x2, int y2, int color)
{
    x1 = EQ_ClampInt(x1, 0, EQ_UI_SCREEN_WIDTH - 1);
    x2 = EQ_ClampInt(x2, 0, EQ_UI_SCREEN_WIDTH - 1);
    y1 = EQ_ClampInt(y1, 0, EQ_UI_SCREEN_HEIGHT - 1);
    y2 = EQ_ClampInt(y2, 0, EQ_UI_SCREEN_HEIGHT - 1);
    EQ_RecordDirtyRect((x1 < x2) ? x1 : x2,
                       (y1 < y2) ? y1 : y2,
                       ((x1 < x2) ? x2 - x1 : x1 - x2) + 1,
                       ((y1 < y2) ? y2 - y1 : y1 - y2) + 1);
#if !defined(EQ_ALGO_ONLY)
    GrContextForegroundSet(EQ_DrawContext(), EQ_MapColor(color));
    GrLineDraw(EQ_DrawContext(), x1, y1, x2, y2);
#else
    {
        int left;
        int top;
        int width;
        int height;

        left = (x1 < x2) ? x1 : x2;
        top = (y1 < y2) ? y1 : y2;
        width = ((x1 < x2) ? x2 - x1 : x1 - x2) + 1;
        height = ((y1 < y2) ? y2 - y1 : y1 - y2) + 1;
        EQ_TracePrimitive("line", left, top, width, height, color,
                          0, 0, -1, -1, 0);
    }
    s_mock_primitive_count++;
#endif
}

static void EQ_LcdDrawHLine(int x1, int x2, int y, int color)
{
    x1 = EQ_ClampInt(x1, 0, EQ_UI_SCREEN_WIDTH - 1);
    x2 = EQ_ClampInt(x2, 0, EQ_UI_SCREEN_WIDTH - 1);
    y = EQ_ClampInt(y, 0, EQ_UI_SCREEN_HEIGHT - 1);
    EQ_RecordDirtyRect(x1, y, x2 - x1 + 1, 1);
#if !defined(EQ_ALGO_ONLY)
    EQ_LcdFillRect16(x1, y, x2 - x1 + 1, 1, EQ_LcdColor16(color));
#else
    EQ_TracePrimitive("line", x1, y, x2 - x1 + 1, 1, color,
                      0, 0, -1, -1, 0);
    s_mock_primitive_count++;
#endif
}

static void EQ_LcdDrawText(const EQ_UI_RECT *rect, const char *text,
                           int length, int font, int color, int centered)
{
    int x;
    int y;

    if (EQ_CheckRect(rect->x, rect->y, rect->w, rect->h) == 0)
    {
        return;
    }
    EQ_RecordDirtyRect(rect->x, rect->y, rect->w, rect->h);
    EQ_SetFont(font);
    x = rect->x + 2;
    y = rect->y + ((rect->h - 12) / 2);
#if !defined(EQ_ALGO_ONLY)
    GrContextForegroundSet(EQ_DrawContext(), EQ_MapColor(color));
    if (centered != 0)
    {
        GrStringDrawCentered(EQ_DrawContext(), text, length,
                             rect->x + rect->w / 2, y + 5, 0);
    }
    else
    {
        GrStringDraw(EQ_DrawContext(), text, length, x, y, 0);
    }
#else
    EQ_TracePrimitive("ascii_text", rect->x, rect->y,
                      rect->w, rect->h, color,
                      text, length, -1, font, centered);
    (void)x;
    (void)y;
    s_mock_primitive_count++;
#endif
}

#if EQ_LCD_DIAGNOSTIC_ALIGNMENT_PATTERN
static void EQ_FormatHex8(char *destination, unsigned long value)
{
    static const char digits[] = "0123456789ABCDEF";
    int index;

    for (index = 7; index >= 0; index--)
    {
        destination[index] = digits[value & 0x0FUL];
        value >>= 4;
    }
}

static void EQ_DrawAlignmentPattern(void)
{
    static const char * const labels[12] =
    {
        "000", "040", "080", "120", "160", "200",
        "240", "280", "320", "360", "400", "440"
    };
    EQ_UI_RECT text_rect;
    char frame_text[40] = "ALIGN 800x480 FB0=00000000 END=00000000";
    int index;
    int y;

    /* Keep the one-time full-frame clear on the board driver's proven path.
       Runtime tiles still use packed writes after the raster has stabilized. */
    EQ_LcdFillRectStartup(0, 0, EQ_UI_SCREEN_WIDTH, EQ_UI_SCREEN_HEIGHT,
                          EQ_COLOR_BG);
    EQ_LcdDrawRect(0, 0, EQ_UI_SCREEN_WIDTH, EQ_UI_SCREEN_HEIGHT,
                   EQ_COLOR_HIGHLIGHT);
    for (index = 0; index < 12; index++)
    {
        y = index * 40;
        if (y != 0)
        {
            EQ_LcdDrawLine(0, y, EQ_UI_SCREEN_WIDTH - 1, y,
                           (y == 240) ? EQ_COLOR_ZERO : EQ_COLOR_BORDER);
        }
        text_rect.x = 4;
        text_rect.y = (y == 0) ? 2 : y - 9;
        text_rect.w = 38;
        text_rect.h = 18;
        EQ_LcdDrawText(&text_rect, labels[index], 3, EQ_FONT_SMALL,
                       (y == 240) ? EQ_COLOR_ZERO : EQ_COLOR_TEXT, 0);
    }
    text_rect.x = 4;
    text_rect.y = 460;
    text_rect.w = 38;
    text_rect.h = 18;
    EQ_LcdDrawText(&text_rect, "479", 3, EQ_FONT_SMALL,
                   EQ_COLOR_TEXT, 0);

    EQ_LcdDrawLine(400, 216, 400, 264, EQ_COLOR_ACTIVE);
    EQ_LcdDrawLine(376, 240, 424, 240, EQ_COLOR_ACTIVE);
    EQ_LcdDrawLine(0, 0, 30, 0, EQ_COLOR_ACTIVE);
    EQ_LcdDrawLine(0, 0, 0, 30, EQ_COLOR_ACTIVE);
    EQ_LcdDrawLine(769, 0, 799, 0, EQ_COLOR_ACTIVE);
    EQ_LcdDrawLine(799, 0, 799, 30, EQ_COLOR_ACTIVE);
    EQ_LcdDrawLine(0, 479, 30, 479, EQ_COLOR_ACTIVE);
    EQ_LcdDrawLine(0, 449, 0, 479, EQ_COLOR_ACTIVE);
    EQ_LcdDrawLine(769, 479, 799, 479, EQ_COLOR_ACTIVE);
    EQ_LcdDrawLine(799, 449, 799, 479, EQ_COLOR_ACTIVE);

    text_rect.x = 190;
    text_rect.y = 5;
    text_rect.w = 420;
    text_rect.h = 22;
    EQ_FormatHex8(&frame_text[18], EQ_DebugLcdExpectedFrameBase);
    EQ_FormatHex8(&frame_text[31], EQ_DebugLcdExpectedFrameEnd);
    EQ_LcdDrawText(&text_rect, frame_text, 39,
                   EQ_FONT_SMALL, EQ_COLOR_TEXT, 1);
}
#endif

#if EQ_LCD_USE_CHINESE
static int EQ_DrawCnGlyph(int x, int y, unsigned char glyph, int color)
{
    int row;

    if (glyph >= (unsigned char)CN_COUNT)
    {
        return x;
    }
#if defined(EQ_ALGO_ONLY)
    EQ_TracePrimitive("chinese_glyph", x, y,
                      EQ_CN_GLYPH_SIZE, EQ_CN_GLYPH_SIZE, color,
                      0, 0, (int)glyph, -1, 0);
#endif
    for (row = 0; row < EQ_CN_GLYPH_SIZE; row++)
    {
        unsigned short bits;
        int col;

        bits = s_cn_bits[glyph][row];
        col = 0;
        while (col < EQ_CN_GLYPH_SIZE)
        {
            int start;

            if ((bits & (unsigned short)(0x8000U >> col)) == 0U)
            {
                col++;
                continue;
            }
            start = col;
            do
            {
                col++;
            }
            while ((col < EQ_CN_GLYPH_SIZE) &&
                   ((bits & (unsigned short)(0x8000U >> col)) != 0U));
            EQ_LcdDrawHLine(x + start, x + col - 1, y + row, color);
        }
    }
    return x + EQ_CN_GLYPH_ADVANCE;
}

#if !EQ_LCD_DIAGNOSTIC_ALIGNMENT_PATTERN
static void EQ_DrawCnCentered(const EQ_UI_RECT *rect,
                              const unsigned char *glyphs,
                              int count, int color)
{
    int index;
    int width;
    int x;
    int y;

    width = count * EQ_CN_GLYPH_ADVANCE - 1;
    x = rect->x + (rect->w - width) / 2;
    y = rect->y + (rect->h - EQ_CN_GLYPH_SIZE) / 2;
    for (index = 0; index < count; index++)
    {
        x = EQ_DrawCnGlyph(x, y, glyphs[index], color);
    }
}
#endif

static void EQ_DrawCnValue(const EQ_UI_RECT *rect,
                           unsigned char glyph, int color)
{
    int x;
    int y;

    x = rect->x + (rect->w - EQ_CN_GLYPH_SIZE) / 2;
    y = rect->y + (rect->h - EQ_CN_GLYPH_SIZE) / 2;
    (void)EQ_DrawCnGlyph(x, y, glyph, color);
}
#endif

#if EQ_ENABLE_TEN_BAND_EDITOR != 0
static int EQ_FormatHalfDb(char *buffer, int half_db)
{
    int magnitude;
    int whole;
    int length;

    half_db = EQ_ClampInt(half_db,
                          EQ_UI_GAIN_HALF_DB_MIN,
                          EQ_UI_GAIN_HALF_DB_MAX);
    length = 0;
    buffer[length++] = (half_db >= 0) ? '+' : '-';
    magnitude = (half_db >= 0) ? half_db : -half_db;
    whole = magnitude / 2;
    if (whole >= 10)
    {
        buffer[length++] = (char)('0' + (whole / 10));
    }
    buffer[length++] = (char)('0' + (whole % 10));
    buffer[length++] = '.';
    buffer[length++] = ((magnitude & 1) != 0) ? '5' : '0';
    buffer[length] = '\0';
    return length;
}
#endif

static void EQ_ClearDynamicValue(const EQ_UI_RECT *rect)
{
    EQ_LcdFillRect(
        rect->x + (rect->w - EQ_UI_DYNAMIC_VALUE_CLEAR_W) / 2,
        rect->y + (rect->h - EQ_UI_DYNAMIC_VALUE_CLEAR_H) / 2,
        EQ_UI_DYNAMIC_VALUE_CLEAR_W,
        EQ_UI_DYNAMIC_VALUE_CLEAR_H,
        EQ_COLOR_BG);
}

#if EQ_ENABLE_TEN_BAND_EDITOR != 0
static void EQ_DrawPageTitle(int page)
{
    EQ_UI_RECT rect;

    rect.x = EQ_UI_TITLE_X;
    rect.y = EQ_UI_TITLE_Y;
    rect.w = EQ_UI_TITLE_W;
    rect.h = EQ_UI_TITLE_H;
    EQ_LcdFillRect(rect.x, rect.y, rect.w, rect.h, EQ_COLOR_BG);
#if EQ_LCD_USE_CHINESE
    if (page == EQ_UI_PAGE_EQ_EDITOR)
    {
        EQ_DrawCnCentered(&rect, s_editor_title, 6, EQ_COLOR_TEXT);
    }
    else
    {
        EQ_DrawCnCentered(&rect, s_title, 8, EQ_COLOR_TEXT);
    }
#else
    if (page == EQ_UI_PAGE_EQ_EDITOR)
    {
        EQ_LcdDrawText(&rect, "P3.3 10-Band EQ", 16,
                       EQ_FONT_LARGE, EQ_COLOR_TEXT, 1);
    }
    else
    {
        EQ_LcdDrawText(&rect, "P3.3 Dynamic EQ", 14,
                       EQ_FONT_LARGE, EQ_COLOR_TEXT, 1);
    }
#endif
}

static void EQ_DrawPageSwitch(int page)
{
    const char *label;
    int length;

    label = (page == EQ_UI_PAGE_EQ_EDITOR) ? "STATUS" : "EDITOR";
    length = 6;
    EQ_LcdFillRect(EQ_UI_PAGE_SWITCH_RECT.x,
                   EQ_UI_PAGE_SWITCH_RECT.y,
                   EQ_UI_PAGE_SWITCH_RECT.w,
                   EQ_UI_PAGE_SWITCH_RECT.h, EQ_COLOR_BG);
    EQ_LcdDrawRect(EQ_UI_PAGE_SWITCH_RECT.x,
                   EQ_UI_PAGE_SWITCH_RECT.y,
                   EQ_UI_PAGE_SWITCH_RECT.w,
                   EQ_UI_PAGE_SWITCH_RECT.h, EQ_COLOR_BORDER);
    EQ_LcdDrawText(&EQ_UI_PAGE_SWITCH_RECT, label, length,
                   EQ_FONT_SMALL, EQ_COLOR_TEXT, 1);
}

static int EQ_EditorGainToPixel(int gain_half_db)
{
    long numerator;
    int span;

    gain_half_db = EQ_ClampInt(gain_half_db,
                               EQ_UI_GAIN_HALF_DB_MIN,
                               EQ_UI_GAIN_HALF_DB_MAX);
    span = EQ_UI_EDITOR_DRAW_BOTTOM - EQ_UI_EDITOR_DRAW_TOP;
    numerator = (long)(EQ_UI_GAIN_HALF_DB_MAX - gain_half_db) *
                (long)span;
    return EQ_UI_EDITOR_DRAW_TOP + (int)((numerator + 30L) / 60L);
}

static void EQ_DrawEditorBandFull(int band)
{
    static const char * const labels[EQ_NUM_BANDS] =
    {
        "31", "62", "125", "250", "500",
        "1k", "2k", "4k", "8k", "16k"
    };
    static const int lengths[EQ_NUM_BANDS] =
    {
        2, 2, 3, 3, 3, 2, 2, 2, 2, 3
    };
    const EQ_UI_RECT *rect;
    EQ_UI_RECT label_rect;
    int inner_x;
    int pixel;
    int selected;

    /* Dedicated page buffers retain static pixels between page switches. */
    rect = &EQ_UI_EDITOR_BAND_RECTS[band];
    selected = (s_ui_state.requested.editor_selected_band == band) ? 1 : 0;
    EQ_LcdDrawRect(rect->x, rect->y, rect->w, rect->h,
                   selected ? EQ_COLOR_HIGHLIGHT : EQ_COLOR_BORDER);
    inner_x = rect->x + EQ_UI_EDITOR_INNER_X_OFFSET;
    EQ_LcdFillRect(inner_x + 1, EQ_UI_EDITOR_BAR_TOP + 1,
                   EQ_UI_EDITOR_INNER_W - 2,
                   EQ_UI_EDITOR_BAR_BOTTOM - EQ_UI_EDITOR_BAR_TOP - 1,
                   EQ_COLOR_BG);
    EQ_LcdDrawRect(inner_x, EQ_UI_EDITOR_BAR_TOP,
                   EQ_UI_EDITOR_INNER_W,
                   EQ_UI_EDITOR_BAR_BOTTOM - EQ_UI_EDITOR_BAR_TOP + 1,
                   EQ_COLOR_BORDER);
    EQ_LcdDrawLine(inner_x - 5, EQ_UI_EDITOR_ZERO_PIXEL,
                   inner_x - 1, EQ_UI_EDITOR_ZERO_PIXEL, EQ_COLOR_ZERO);
    EQ_LcdDrawLine(inner_x + EQ_UI_EDITOR_INNER_W,
                   EQ_UI_EDITOR_ZERO_PIXEL,
                   inner_x + EQ_UI_EDITOR_INNER_W + 4,
                   EQ_UI_EDITOR_ZERO_PIXEL, EQ_COLOR_ZERO);
    pixel = EQ_EditorGainToPixel(
        (int)s_ui_state.requested.applied_gain_half_db[band]);
    if (pixel < EQ_UI_EDITOR_ZERO_PIXEL)
    {
        EQ_LcdFillRect(inner_x + 1, pixel,
                       EQ_UI_EDITOR_INNER_W - 2,
                       EQ_UI_EDITOR_ZERO_PIXEL - pixel,
                       EQ_COLOR_BAR_POS);
    }
    else if (pixel > EQ_UI_EDITOR_ZERO_PIXEL)
    {
        EQ_LcdFillRect(inner_x + 1, EQ_UI_EDITOR_ZERO_PIXEL + 1,
                       EQ_UI_EDITOR_INNER_W - 2,
                       pixel - EQ_UI_EDITOR_ZERO_PIXEL,
                       EQ_COLOR_BAR_NEG);
    }
    label_rect.x = rect->x;
    label_rect.y = 298;
    label_rect.w = rect->w;
    label_rect.h = 24;
    EQ_LcdDrawText(&label_rect, labels[band], lengths[band],
                   EQ_FONT_SMALL, EQ_COLOR_TEXT, 1);
}
#endif

#if !EQ_LCD_DIAGNOSTIC_ALIGNMENT_PATTERN
static void EQ_DrawPresetStatic(int index)
{
    const EQ_UI_RECT *rect;

    rect = &EQ_UI_PRESET_RECTS[index];
    EQ_LcdFillRect(rect->x, rect->y, rect->w, rect->h, EQ_COLOR_BG);
    EQ_LcdDrawRect(rect->x, rect->y, rect->w, rect->h, EQ_COLOR_BORDER);
#if EQ_LCD_USE_CHINESE
    if (index == 0)
        EQ_DrawCnCentered(rect, s_preset_flat, 2, EQ_COLOR_TEXT);
    else if (index == 1)
        EQ_DrawCnCentered(rect, s_preset_bass, 2, EQ_COLOR_TEXT);
    else if (index == 2)
        EQ_DrawCnCentered(rect, s_preset_vocal, 2, EQ_COLOR_TEXT);
    else if (index == 3)
        EQ_DrawCnCentered(rect, s_preset_treble, 2, EQ_COLOR_TEXT);
    else
    {
        EQ_UI_RECT v_rect;
        EQ_UI_RECT cn_rect;

        v_rect.x = rect->x + rect->w / 2 - 18;
        v_rect.y = rect->y;
        v_rect.w = 18;
        v_rect.h = rect->h;
        cn_rect.x = rect->x + rect->w / 2;
        cn_rect.y = rect->y;
        cn_rect.w = 18;
        cn_rect.h = rect->h;
        EQ_LcdDrawText(&v_rect, "V", 1, EQ_FONT_MEDIUM,
                       EQ_COLOR_TEXT, 1);
        EQ_DrawCnValue(&cn_rect, CN_XING, EQ_COLOR_TEXT);
    }
#else
    {
        static const char * const labels[EQ_UI_PRESET_COUNT] =
        {
            "FLAT", "BASS", "VOCAL", "TREBLE", "V-SHAPE"
        };
        static const int lengths[EQ_UI_PRESET_COUNT] = { 4, 4, 5, 6, 7 };
        EQ_LcdDrawText(rect, labels[index], lengths[index],
                       EQ_FONT_MEDIUM, EQ_COLOR_TEXT, 1);
    }
#endif
}

static void EQ_DrawAnalyzerStatic(void)
{
    static const char * const en_labels[EQ_UI_ANALYZER_COUNT] =
    {
        "BASS", "MUD", "PRES", "HIGH"
    };
    static const int en_lengths[EQ_UI_ANALYZER_COUNT] = { 4, 3, 4, 4 };
    int band;
    int inner_x;
    EQ_UI_RECT label_rect;

    for (band = 0; band < EQ_UI_ANALYZER_COUNT; band++)
    {
        EQ_LcdDrawRect(EQ_UI_ANALYZER_RECTS[band].x,
                       EQ_UI_ANALYZER_RECTS[band].y,
                       EQ_UI_ANALYZER_RECTS[band].w,
                       EQ_UI_ANALYZER_RECTS[band].h,
                       EQ_COLOR_BORDER);
        inner_x = EQ_UI_ANALYZER_RECTS[band].x +
                  EQ_UI_ANALYZER_INNER_X_OFFSET;
        EQ_LcdDrawRect(inner_x, EQ_UI_ANALYZER_BAR_TOP,
                       EQ_UI_ANALYZER_INNER_W,
                       EQ_UI_ANALYZER_BAR_BOTTOM -
                       EQ_UI_ANALYZER_BAR_TOP + 1,
                       EQ_COLOR_BORDER);
        EQ_LcdDrawLine(inner_x - 6, EQ_UI_ANALYZER_ZERO_Y,
                       inner_x - 2, EQ_UI_ANALYZER_ZERO_Y,
                       EQ_COLOR_ZERO);
        EQ_LcdDrawLine(inner_x + EQ_UI_ANALYZER_INNER_W + 1,
                       EQ_UI_ANALYZER_ZERO_Y,
                       inner_x + EQ_UI_ANALYZER_INNER_W + 5,
                       EQ_UI_ANALYZER_ZERO_Y,
                       EQ_COLOR_ZERO);
        label_rect.x = EQ_UI_ANALYZER_RECTS[band].x;
        label_rect.y = 310;
        label_rect.w = EQ_UI_ANALYZER_RECTS[band].w;
        label_rect.h = 24;
        EQ_LcdDrawText(&label_rect, en_labels[band], en_lengths[band],
                       EQ_FONT_SMALL, EQ_COLOR_TEXT, 1);
    }
}

static void EQ_DrawDynamicStatic(void)
{
    static const char * const en_labels[EQ_UI_DYNAMIC_COUNT] =
    {
        "SMART BASS", "CLARITY", "HF GUARD"
    };
    static const int en_lengths[EQ_UI_DYNAMIC_COUNT] = { 10, 7, 8 };
    EQ_UI_RECT label_rect;
    int index;

    for (index = 0; index < EQ_UI_DYNAMIC_COUNT; index++)
    {
        label_rect.x = 24;
        label_rect.y = EQ_UI_DYNAMIC_RECTS[index].y;
        label_rect.w = 124;
        label_rect.h = EQ_UI_DYNAMIC_RECTS[index].h;
        EQ_LcdDrawText(&label_rect, en_labels[index], en_lengths[index],
                       EQ_FONT_SMALL, EQ_COLOR_TEXT, 0);
        EQ_LcdDrawRect(EQ_UI_DYNAMIC_TOGGLE_RECTS[index].x,
                       EQ_UI_DYNAMIC_TOGGLE_RECTS[index].y,
                       EQ_UI_DYNAMIC_TOGGLE_RECTS[index].w,
                       EQ_UI_DYNAMIC_TOGGLE_RECTS[index].h,
                       EQ_COLOR_BORDER);
        EQ_LcdDrawRect(EQ_UI_DYNAMIC_STRENGTH_RECTS[index].x,
                       EQ_UI_DYNAMIC_STRENGTH_RECTS[index].y,
                       EQ_UI_DYNAMIC_STRENGTH_RECTS[index].w,
                       EQ_UI_DYNAMIC_STRENGTH_RECTS[index].h,
                       EQ_COLOR_BORDER);
        EQ_LcdDrawRect(EQ_UI_DYNAMIC_STATUS_RECTS[index].x,
                       EQ_UI_DYNAMIC_STATUS_RECTS[index].y,
                       EQ_UI_DYNAMIC_STATUS_RECTS[index].w,
                       EQ_UI_DYNAMIC_STATUS_RECTS[index].h,
                       EQ_COLOR_BORDER);
#if EQ_LCD_USE_CHINESE
        EQ_DrawCnValue(&EQ_UI_DYNAMIC_TOGGLE_RECTS[index],
                       CN_GUAN, EQ_COLOR_MUTED);
        EQ_DrawCnValue(&EQ_UI_DYNAMIC_STRENGTH_RECTS[index],
                       CN_ZHONG, EQ_COLOR_TEXT);
#else
        EQ_LcdDrawText(&EQ_UI_DYNAMIC_TOGGLE_RECTS[index], "OFF", 3,
                       EQ_FONT_SMALL, EQ_COLOR_MUTED, 1);
        EQ_LcdDrawText(&EQ_UI_DYNAMIC_STRENGTH_RECTS[index], "MID", 3,
                       EQ_FONT_SMALL, EQ_COLOR_TEXT, 1);
#endif
        EQ_LcdFillRect(EQ_UI_DYNAMIC_STATUS_RECTS[index].x + 15,
                       EQ_UI_DYNAMIC_STATUS_RECTS[index].y + 15,
                       10, 10, EQ_COLOR_BG);
    }
}

#if EQ_ENABLE_TEN_BAND_EDITOR != 0
static void EQ_DrawAnalyzerSnapshot(int band)
{
    const EQ_UI_RECT *rect;
    int inner_x;
    int pixel;
    int valid;

    rect = &EQ_UI_ANALYZER_RECTS[band];
    inner_x = rect->x + EQ_UI_ANALYZER_INNER_X_OFFSET;
    EQ_LcdFillRect(inner_x + 1, EQ_UI_ANALYZER_BAR_TOP + 1,
                   EQ_UI_ANALYZER_INNER_W - 2,
                   EQ_UI_ANALYZER_BAR_BOTTOM -
                   EQ_UI_ANALYZER_BAR_TOP - 1, EQ_COLOR_BG);
    pixel = EQ_ClampInt(s_ui_state.requested.band_pixel[band],
                        EQ_UI_ANALYZER_DRAW_TOP,
                        EQ_UI_ANALYZER_DRAW_BOTTOM);
    valid = (s_ui_state.requested.analyzer_valid != 0) &&
            (s_ui_state.requested.analyzer_warm != 0);
    if (valid && (pixel < EQ_UI_ANALYZER_ZERO_Y))
    {
        EQ_LcdFillRect(inner_x + 1, pixel,
                       EQ_UI_ANALYZER_INNER_W - 2,
                       EQ_UI_ANALYZER_ZERO_Y - pixel,
                       EQ_COLOR_BAR_POS);
    }
    else if (valid && (pixel > EQ_UI_ANALYZER_ZERO_Y))
    {
        EQ_LcdFillRect(inner_x + 1, EQ_UI_ANALYZER_ZERO_Y + 1,
                       EQ_UI_ANALYZER_INNER_W - 2,
                       pixel - EQ_UI_ANALYZER_ZERO_Y,
                       EQ_COLOR_BAR_NEG);
    }
}

static void EQ_DrawDynamicSnapshot(int index)
{
    int enabled;
    int strength;
    int active;

    EQ_ClearDynamicValue(&EQ_UI_DYNAMIC_TOGGLE_RECTS[index]);
    EQ_ClearDynamicValue(&EQ_UI_DYNAMIC_STRENGTH_RECTS[index]);
    EQ_ClearDynamicValue(&EQ_UI_DYNAMIC_STATUS_RECTS[index]);
    enabled = (index == 0) ? s_ui_state.requested.smart_enabled :
              ((index == 1) ? s_ui_state.requested.clarity_enabled :
                              s_ui_state.requested.guard_enabled);
    strength = (index == 0) ? s_ui_state.requested.smart_strength :
               ((index == 1) ? s_ui_state.requested.clarity_strength :
                               s_ui_state.requested.guard_strength);
    active = (index == 0) ? s_ui_state.requested.smart_active :
             ((index == 1) ? s_ui_state.requested.clarity_active :
                             s_ui_state.requested.guard_active);
#if EQ_LCD_USE_CHINESE
    EQ_DrawCnValue(&EQ_UI_DYNAMIC_TOGGLE_RECTS[index],
                   enabled ? CN_KAI : CN_GUAN,
                   enabled ? EQ_COLOR_ACTIVE : EQ_COLOR_MUTED);
    EQ_DrawCnValue(&EQ_UI_DYNAMIC_STRENGTH_RECTS[index],
                   (strength <= 1) ? CN_DI :
                   ((strength >= 3) ? CN_GAO : CN_ZHONG), EQ_COLOR_TEXT);
#else
    EQ_LcdDrawText(&EQ_UI_DYNAMIC_TOGGLE_RECTS[index],
                   enabled ? "ON" : "OFF", enabled ? 2 : 3,
                   EQ_FONT_SMALL,
                   enabled ? EQ_COLOR_ACTIVE : EQ_COLOR_MUTED, 1);
    if (strength <= 1)
        EQ_LcdDrawText(&EQ_UI_DYNAMIC_STRENGTH_RECTS[index],
                       "LOW", 3, EQ_FONT_SMALL, EQ_COLOR_TEXT, 1);
    else if (strength >= 3)
        EQ_LcdDrawText(&EQ_UI_DYNAMIC_STRENGTH_RECTS[index],
                       "HIGH", 4, EQ_FONT_SMALL, EQ_COLOR_TEXT, 1);
    else
        EQ_LcdDrawText(&EQ_UI_DYNAMIC_STRENGTH_RECTS[index],
                       "MID", 3, EQ_FONT_SMALL, EQ_COLOR_TEXT, 1);
#endif
    EQ_LcdFillRect(EQ_UI_DYNAMIC_STATUS_RECTS[index].x + 15,
                   EQ_UI_DYNAMIC_STATUS_RECTS[index].y + 15,
                   10, 10,
                   active ? EQ_COLOR_ACTIVE :
                   (enabled ? EQ_COLOR_MUTED : EQ_COLOR_BG));
}

static const char *EQ_EditorPresetText(int preset, int draft_dirty)
{
    if (draft_dirty != 0)
        return "CUSTOM";
    if (preset == EQ_PRESET_CUSTOM)
        return "CUSTOM";
    if (preset == EQ_PRESET_BASS_BOOST)
        return "BASS";
    if (preset == EQ_PRESET_VOCAL)
        return "VOCAL";
    if (preset == EQ_PRESET_TREBLE_BOOST)
        return "TREBLE";
    if (preset == EQ_PRESET_V_SHAPE)
        return "V-SHAPE";
    return "FLAT";
}

static const char *EQ_EditorApplyText(int status)
{
    status = EQ_ClampInt(status, EQ_UI_APPLY_EDITING, EQ_UI_APPLY_ERROR);
    if (status == EQ_UI_APPLY_ERROR)
        return "ERROR";
    if (status == EQ_UI_APPLY_APPLIED)
        return "APPLIED";
    return "BUILD";
}

static void EQ_DrawEditorFieldValue(unsigned int field)
{
    static const char * const frequencies[EQ_NUM_BANDS] =
    {
        "31", "62", "125", "250", "500",
        "1k", "2k", "4k", "8k", "16k"
    };
    static const int frequency_lengths[EQ_NUM_BANDS] =
    {
        2, 2, 3, 3, 3, 2, 2, 2, 2, 3
    };
    EQ_UI_RECT value_rect;
    const char *text;
    char buffer[8];
    int index;
    int band;
    int length;
    int color;

    if (field == EQ_UI_EDITOR_FIELD_SELECTED_BAND)
        index = 0;
    else if (field == EQ_UI_EDITOR_FIELD_DRAFT_GAIN)
        index = 1;
    else if (field == EQ_UI_EDITOR_FIELD_APPLIED_GAIN)
        index = 2;
    else if (field == EQ_UI_EDITOR_FIELD_CUSTOM_STATE)
        index = 3;
    else
        index = 4;
    value_rect.x = s_editor_field_rects[index].x +
        (s_editor_field_rects[index].w - EQ_UI_EDITOR_VALUE_CLEAR_W) / 2;
    value_rect.y = s_editor_field_rects[index].y + 26 +
        (s_editor_field_rects[index].h - 28 -
         EQ_UI_EDITOR_VALUE_CLEAR_H) / 2;
    value_rect.w = EQ_UI_EDITOR_VALUE_CLEAR_W;
    value_rect.h = EQ_UI_EDITOR_VALUE_CLEAR_H;
    EQ_LcdFillRect(value_rect.x, value_rect.y,
                   value_rect.w, value_rect.h, EQ_COLOR_BG);
    band = EQ_ClampInt(s_ui_state.requested.editor_selected_band,
                       0, EQ_NUM_BANDS - 1);
    color = EQ_COLOR_TEXT;
    if (index == 0)
    {
        text = frequencies[band];
        length = frequency_lengths[band];
    }
    else if (index == 1)
    {
        length = EQ_FormatHalfDb(
            buffer, (int)s_ui_state.requested.draft_gain_half_db[band]);
        text = buffer;
        color = (s_ui_state.requested.editor_draft_dirty != 0U) ?
            EQ_COLOR_HIGHLIGHT : EQ_COLOR_TEXT;
    }
    else if (index == 2)
    {
        length = EQ_FormatHalfDb(
            buffer, (int)s_ui_state.requested.applied_gain_half_db[band]);
        text = buffer;
    }
    else if (index == 3)
    {
        text = EQ_EditorPresetText(
            s_ui_state.requested.applied_preset,
            (int)s_ui_state.requested.editor_draft_dirty);
        length = (int)strlen(text);
    }
    else
    {
        text = EQ_EditorApplyText(
            s_ui_state.requested.editor_apply_status);
        length = (int)strlen(text);
        color = (s_ui_state.requested.editor_apply_status ==
                 EQ_UI_APPLY_ERROR) ? EQ_COLOR_BAR_NEG : EQ_COLOR_ACTIVE;
    }
    EQ_LcdDrawText(&value_rect, text, length,
                   EQ_FONT_SMALL, color, 1);
}

static void EQ_DrawEditorControlsFull(void)
{
    EQ_LcdDrawRect(EQ_UI_EDITOR_MINUS_RECT.x, EQ_UI_EDITOR_MINUS_RECT.y,
                   EQ_UI_EDITOR_MINUS_RECT.w, EQ_UI_EDITOR_MINUS_RECT.h,
                   EQ_COLOR_BORDER);
    EQ_LcdDrawText(&EQ_UI_EDITOR_MINUS_RECT, "-", 1,
                   EQ_FONT_LARGE, EQ_COLOR_TEXT, 1);
    EQ_LcdDrawRect(EQ_UI_EDITOR_PLUS_RECT.x, EQ_UI_EDITOR_PLUS_RECT.y,
                   EQ_UI_EDITOR_PLUS_RECT.w, EQ_UI_EDITOR_PLUS_RECT.h,
                   EQ_COLOR_BORDER);
    EQ_LcdDrawText(&EQ_UI_EDITOR_PLUS_RECT, "+", 1,
                   EQ_FONT_LARGE, EQ_COLOR_TEXT, 1);
    EQ_LcdDrawRect(EQ_UI_EDITOR_APPLY_RECT.x, EQ_UI_EDITOR_APPLY_RECT.y,
                   EQ_UI_EDITOR_APPLY_RECT.w, EQ_UI_EDITOR_APPLY_RECT.h,
                   EQ_COLOR_HIGHLIGHT);
    EQ_LcdDrawText(&EQ_UI_EDITOR_APPLY_RECT, "APPLY", 5,
                   EQ_FONT_SMALL, EQ_COLOR_TEXT, 1);
    EQ_LcdDrawRect(EQ_UI_EDITOR_RESET_RECT.x, EQ_UI_EDITOR_RESET_RECT.y,
                   EQ_UI_EDITOR_RESET_RECT.w, EQ_UI_EDITOR_RESET_RECT.h,
                   EQ_COLOR_BORDER);
#if EQ_LCD_USE_CHINESE
    EQ_DrawCnCentered(&EQ_UI_EDITOR_RESET_RECT,
                      s_preset_flat, 2, EQ_COLOR_TEXT);
#else
    EQ_LcdDrawText(&EQ_UI_EDITOR_RESET_RECT, "RESET FLAT", 10,
                   EQ_FONT_SMALL, EQ_COLOR_TEXT, 1);
#endif
}

static void EQ_DrawEditorFieldFull(int index)
{
    static const char * const labels[5] =
    {
        "BAND", "DRAFT", "APPLIED", "MODE", "STATE"
    };
    static const int lengths[5] = { 4, 5, 7, 4, 5 };
    unsigned int field;
    EQ_UI_RECT label_rect;

    EQ_LcdDrawRect(s_editor_field_rects[index].x,
                   s_editor_field_rects[index].y,
                   s_editor_field_rects[index].w,
                   s_editor_field_rects[index].h, EQ_COLOR_BORDER);
    label_rect = s_editor_field_rects[index];
    label_rect.h = 24;
    EQ_LcdDrawText(&label_rect, labels[index], lengths[index],
                   EQ_FONT_SMALL, EQ_COLOR_MUTED, 1);
    field = EQ_UI_EDITOR_FIELD_SELECTED_BAND << index;
    EQ_DrawEditorFieldValue(field);
}
#endif
#endif

static void EQ_DrawPresetJob(int index)
{
    const EQ_UI_RECT *rect;
    int selected;

    rect = &EQ_UI_PRESET_RECTS[index];
    selected = (s_ui_state.requested.applied_preset == index) ? 1 : 0;
    EQ_LcdDrawRect(rect->x, rect->y, rect->w, rect->h,
                   selected ? EQ_COLOR_HIGHLIGHT : EQ_COLOR_BORDER);
    EQ_LcdFillRect(rect->x + 2, rect->y + rect->h - 5,
                   rect->w - 4, 3,
                   selected ? EQ_COLOR_HIGHLIGHT : EQ_COLOR_BG);
}

static unsigned int EQ_DrawAnalyzerJob(int band)
{
    EQ_UI_RECT bar_rect;
    unsigned int field;
    int current;
    int next;
    int y;
    int height;
    int color;
    unsigned int operation;

    bar_rect.x = EQ_UI_ANALYZER_RECTS[band].x +
                 EQ_UI_ANALYZER_INNER_X_OFFSET + 1;
    bar_rect.y = EQ_UI_ANALYZER_DRAW_TOP;
    bar_rect.w = EQ_UI_ANALYZER_INNER_W - 2;
    bar_rect.h = EQ_UI_ANALYZER_DRAW_BOTTOM -
                 EQ_UI_ANALYZER_DRAW_TOP + 1;
    field = EqualizerUiLogic_AnalyzerNextField(&s_ui_state, band);
    EQ_DebugLcdAnalyzerLastBand = band;
    EQ_DebugLcdAnalyzerLastField = field;
    EQ_DebugLcdAnalyzerLastStripY = 0;
    EQ_DebugLcdAnalyzerLastStripHeight = 0U;
    EQ_DebugLcdAnalyzerLastStripOperation = EQ_LCD_ANALYZER_STRIP_NONE;
    if (field != EQ_UI_ANALYZER_FIELD_BAR)
    {
        return 0U;
    }
    current = EQ_ClampInt(s_ui_state.displayed.band_pixel[band],
                          EQ_UI_ANALYZER_DRAW_TOP,
                          EQ_UI_ANALYZER_DRAW_BOTTOM);
    next = EqualizerUiLogic_AnalyzerNextPixel(&s_ui_state, band);
    height = (next >= current) ? next - current : current - next;
    if (height <= 0)
    {
        return field;
    }
    if (next < current)
    {
        if (current <= EQ_UI_ANALYZER_ZERO_Y)
        {
            y = next;
            color = EQ_COLOR_BAR_POS;
            operation = EQ_LCD_ANALYZER_STRIP_POSITIVE_FILL;
        }
        else
        {
            y = next + 1;
            color = EQ_COLOR_BG;
            operation = EQ_LCD_ANALYZER_STRIP_CLEAR;
        }
    }
    else
    {
        if (current >= EQ_UI_ANALYZER_ZERO_Y)
        {
            y = current + 1;
            color = EQ_COLOR_BAR_NEG;
            operation = EQ_LCD_ANALYZER_STRIP_NEGATIVE_FILL;
        }
        else
        {
            y = current;
            color = EQ_COLOR_BG;
            operation = EQ_LCD_ANALYZER_STRIP_CLEAR;
        }
    }
    EQ_LcdFillRect(bar_rect.x, y, bar_rect.w, height, color);
    EQ_DebugLcdAnalyzerLastStripY = y;
    EQ_DebugLcdAnalyzerLastStripHeight = (unsigned int)height;
    EQ_DebugLcdAnalyzerLastStripOperation = operation;
    if ((unsigned int)height > EQ_DebugLcdAnalyzerMaxStripHeight)
    {
        EQ_DebugLcdAnalyzerMaxStripHeight = (unsigned int)height;
    }
    EQ_DebugLcdAnalyzerStripCount++;
    return field;
}

static int EQ_DynamicEnabled(int index)
{
    if (index == 0)
        return s_ui_state.requested.smart_enabled;
    if (index == 1)
        return s_ui_state.requested.clarity_enabled;
    return s_ui_state.requested.guard_enabled;
}

static int EQ_DynamicStrength(int index)
{
    if (index == 0)
        return s_ui_state.requested.smart_strength;
    if (index == 1)
        return s_ui_state.requested.clarity_strength;
    return s_ui_state.requested.guard_strength;
}

static unsigned int EQ_DrawDynamicJob(int index)
{
    unsigned int fields;
    unsigned int selected_field;
    int enabled;
    int strength;
    int active;

    fields = EqualizerUiLogic_DynamicFieldMask(&s_ui_state, index);
    if ((fields & EQ_UI_DYNAMIC_FIELD_ENABLED) != 0U)
        selected_field = EQ_UI_DYNAMIC_FIELD_ENABLED;
    else if ((fields & EQ_UI_DYNAMIC_FIELD_STRENGTH) != 0U)
        selected_field = EQ_UI_DYNAMIC_FIELD_STRENGTH;
    else
        selected_field = fields & EQ_UI_DYNAMIC_FIELD_ACTIVE;
    enabled = EQ_DynamicEnabled(index);
    strength = EQ_DynamicStrength(index);
    active = (index == 0) ? s_ui_state.requested.smart_active :
             ((index == 1) ? s_ui_state.requested.clarity_active :
                             s_ui_state.requested.guard_active);
    if ((selected_field & EQ_UI_DYNAMIC_FIELD_ENABLED) != 0U)
    {
        EQ_ClearDynamicValue(&EQ_UI_DYNAMIC_TOGGLE_RECTS[index]);
#if EQ_LCD_USE_CHINESE
        EQ_DrawCnValue(&EQ_UI_DYNAMIC_TOGGLE_RECTS[index],
                       enabled ? CN_KAI : CN_GUAN,
                       enabled ? EQ_COLOR_ACTIVE : EQ_COLOR_MUTED);
#else
        EQ_LcdDrawText(&EQ_UI_DYNAMIC_TOGGLE_RECTS[index],
                       enabled ? "ON" : "OFF", enabled ? 2 : 3,
                       EQ_FONT_SMALL,
                       enabled ? EQ_COLOR_ACTIVE : EQ_COLOR_MUTED, 1);
#endif
    }
    if ((selected_field & EQ_UI_DYNAMIC_FIELD_STRENGTH) != 0U)
    {
        EQ_ClearDynamicValue(&EQ_UI_DYNAMIC_STRENGTH_RECTS[index]);
#if EQ_LCD_USE_CHINESE
        EQ_DrawCnValue(&EQ_UI_DYNAMIC_STRENGTH_RECTS[index],
                       (strength <= 1) ? CN_DI :
                       ((strength >= 3) ? CN_GAO : CN_ZHONG),
                       EQ_COLOR_TEXT);
#else
        if (strength <= 1)
            EQ_LcdDrawText(&EQ_UI_DYNAMIC_STRENGTH_RECTS[index],
                           "LOW", 3, EQ_FONT_SMALL, EQ_COLOR_TEXT, 1);
        else if (strength >= 3)
            EQ_LcdDrawText(&EQ_UI_DYNAMIC_STRENGTH_RECTS[index],
                           "HIGH", 4, EQ_FONT_SMALL, EQ_COLOR_TEXT, 1);
        else
            EQ_LcdDrawText(&EQ_UI_DYNAMIC_STRENGTH_RECTS[index],
                           "MID", 3, EQ_FONT_SMALL, EQ_COLOR_TEXT, 1);
#endif
    }
    if ((selected_field & EQ_UI_DYNAMIC_FIELD_ACTIVE) != 0U)
    {
        EQ_ClearDynamicValue(&EQ_UI_DYNAMIC_STATUS_RECTS[index]);
        EQ_LcdFillRect(EQ_UI_DYNAMIC_STATUS_RECTS[index].x + 15,
                       EQ_UI_DYNAMIC_STATUS_RECTS[index].y + 15,
                       10, 10,
                       active ? EQ_COLOR_ACTIVE :
                       (enabled ? EQ_COLOR_MUTED : EQ_COLOR_BG));
    }
    return selected_field;
}

#if EQ_ENABLE_TEN_BAND_EDITOR != 0
static void EQ_DrawEditorBandJob(int band)
{
    const EQ_UI_RECT *rect;
    int inner_x;
    int current;
    int next;
    int y;
    int height;
    int color;
    int selected;

    rect = &EQ_UI_EDITOR_BAND_RECTS[band];
    selected = (s_ui_state.requested.editor_selected_band == band) ? 1 : 0;
    EQ_LcdDrawRect(rect->x, rect->y, rect->w, rect->h,
                   selected ? EQ_COLOR_HIGHLIGHT : EQ_COLOR_BORDER);
    inner_x = rect->x + EQ_UI_EDITOR_INNER_X_OFFSET;
    current = EQ_ClampInt(s_ui_state.editor_displayed_pixel[band],
                          EQ_UI_EDITOR_DRAW_TOP,
                          EQ_UI_EDITOR_DRAW_BOTTOM);
    next = EqualizerUiLogic_EditorNextPixel(&s_ui_state, band);
    height = (next >= current) ? next - current : current - next;
    if (height <= 0)
    {
        return;
    }
    if (next < current)
    {
        if (current <= EQ_UI_EDITOR_ZERO_PIXEL)
        {
            y = next;
            color = EQ_COLOR_BAR_POS;
        }
        else
        {
            y = next + 1;
            color = EQ_COLOR_BG;
        }
    }
    else
    {
        if (current >= EQ_UI_EDITOR_ZERO_PIXEL)
        {
            y = current + 1;
            color = EQ_COLOR_BAR_NEG;
        }
        else
        {
            y = current;
            color = EQ_COLOR_BG;
        }
    }
    EQ_LcdFillRect(inner_x + 1, y,
                   EQ_UI_EDITOR_INNER_W - 2, height, color);
}

static unsigned int EQ_DrawEditorFieldJob(void)
{
    unsigned int field;

    field = EqualizerUiLogic_EditorNextField(&s_ui_state);
    if (field != 0U)
    {
        EQ_DrawEditorFieldValue(field);
    }
    return field;
}
#endif

static unsigned int EQ_DrawDataJob(int job)
{
#if EQ_ENABLE_TEN_BAND_EDITOR != 0
    if ((job >= EQ_UI_JOB_EDITOR_BAND_0) &&
        (job <= EQ_UI_JOB_EDITOR_BAND_9))
    {
        EQ_DrawEditorBandJob(job - EQ_UI_JOB_EDITOR_BAND_0);
    }
    else if (job == EQ_UI_JOB_EDITOR_FIELDS)
    {
        return EQ_DrawEditorFieldJob();
    }
    else
#endif
    if ((job >= EQ_UI_JOB_PRESET_0) &&
             (job <= EQ_UI_JOB_PRESET_4))
    {
        EQ_DrawPresetJob(job - EQ_UI_JOB_PRESET_0);
    }
    else if ((job >= EQ_UI_JOB_ANALYZER_0) &&
             (job <= EQ_UI_JOB_ANALYZER_3))
    {
        return EQ_DrawAnalyzerJob(job - EQ_UI_JOB_ANALYZER_0);
    }
    else if ((job >= EQ_UI_JOB_DYNAMIC_0) &&
             (job <= EQ_UI_JOB_DYNAMIC_2))
    {
        return EQ_DrawDynamicJob(job - EQ_UI_JOB_DYNAMIC_0);
    }
    return 0U;
}

static unsigned int EQ_DrawJob(int job)
{
#if EQ_ENABLE_TEN_BAND_EDITOR != 0
    int data_job;

    if (job == EQ_UI_JOB_PAGE_SYNC)
    {
        data_job = EqualizerUiLogic_GetPageSyncJob(&s_ui_state);
        EQ_SetDrawPage(EqualizerUiLogic_GetPageTarget(&s_ui_state));
        return EQ_DrawDataJob(data_job);
    }
    EQ_SetDrawPage(EqualizerUiLogic_GetDisplayedPage(&s_ui_state));
#endif
    return EQ_DrawDataJob(job);
}

static unsigned long EQ_ReadCycles(void)
{
#if defined(__TI_COMPILER_VERSION__) || defined(__TMS320C6X__)
    return (unsigned long)TSCL;
#elif defined(EQ_ALGO_ONLY)
    unsigned long value;

    value = s_mock_cycle_clock;
    s_mock_cycle_clock += s_mock_forced_job_cycles;
    return value;
#else
    return 0UL;
#endif
}

static int EQ_JobCategory(int job)
{
#if EQ_ENABLE_TEN_BAND_EDITOR != 0
    if ((job == EQ_UI_JOB_PAGE_SYNC) ||
        (job == EQ_UI_JOB_PAGE_SWAP))
        return EQ_LCD_CATEGORY_PAGE;
    if ((job >= EQ_UI_JOB_EDITOR_BAND_0) &&
        (job <= EQ_UI_JOB_EDITOR_FIELDS))
        return EQ_LCD_CATEGORY_EDITOR;
#endif
    if ((job >= EQ_UI_JOB_PRESET_0) && (job <= EQ_UI_JOB_PRESET_4))
        return EQ_LCD_CATEGORY_PRESET;
    if ((job >= EQ_UI_JOB_DYNAMIC_0) && (job <= EQ_UI_JOB_DYNAMIC_2))
        return EQ_LCD_CATEGORY_DYNAMIC;
    return EQ_LCD_CATEGORY_ANALYZER;
}

void EqualizerDisplay_Init(void)
{
    int index;

    EqualizerUiLogic_Init(&s_ui_state);
    s_lcd_busy = 0;
    s_layout_drawn = 0;
    s_runtime_started = 0;
    EQ_DebugLcdEnabled = 1;
#if EQ_LCD_DIAGNOSTIC_ALIGNMENT_PATTERN
    EQ_DebugLcdRuntimeMask = 0U;
#else
    EQ_DebugLcdRuntimeMask = EQ_UI_RUNTIME_DEFAULT_MASK & EQ_UI_RUNTIME_ALL;
#endif
    EQ_DebugLcdPendingMask = 0UL;
    EQ_DebugLcdRefreshCount = 0UL;
    EQ_DebugLcdSkipBusyCount = 0UL;
    EQ_DebugLcdJobCount = 0UL;
    EQ_DebugLcdDeferredAudioCount = 0UL;
    EQ_DebugLcdAudioArrivedDuringDrawCount = 0UL;
    EQ_DebugLcdUnexpectedFullRedrawCount = 0UL;
    EQ_DebugLcdStaticDrawCount = 0UL;
    EQ_DebugLcdBoundsFailureCount = 0UL;
    EQ_DebugLcdBudgetExceededCount = 0UL;
    EQ_DebugLcdHardBudgetExceededCount = 0UL;
    EQ_DebugLcdOver1msCount = 0UL;
    EQ_DebugLcdOver2msCount = 0UL;
    EQ_DebugLcdOver5msCount = 0UL;
    EQ_DebugLcdLastJobStartCycles = 0UL;
    EQ_DebugLcdLastJobEndCycles = 0UL;
    EQ_DebugLcdLastJobCycles = 0UL;
    EQ_DebugLcdMaxJobCycles = 0UL;
    EQ_DebugLcdPageSyncMaxCycles = 0UL;
    EQ_DebugLcdPageSyncMaxJob = EQ_LCD_PAGE_SYNC_JOB_NONE;
    EQ_DebugLcdPageSyncLastOver2msJob = EQ_LCD_PAGE_SYNC_JOB_NONE;
    EQ_DebugLcdLastJobTenthsMs = 0UL;
    EQ_DebugLcdMaxJobTenthsMs = 0UL;
    EQ_DebugLcdLastJob = EQ_LCD_JOB_NONE;
    EQ_DebugLcdAnalyzerLastBand = -1;
    EQ_DebugLcdAnalyzerLastField = 0U;
    EQ_DebugLcdAnalyzerLastStripY = 0;
    EQ_DebugLcdAnalyzerLastStripHeight = 0U;
    EQ_DebugLcdAnalyzerLastStripOperation = EQ_LCD_ANALYZER_STRIP_NONE;
    EQ_DebugLcdAnalyzerMaxStripHeight = 0U;
    EQ_DebugLcdAnalyzerStripCount = 0UL;
    EQ_DebugLcdAutoDisabledCount = 0UL;
    EQ_DebugLcdAutoDisableReason = 0UL;
    EQ_DebugLcdPageRasterPaused = 0U;
    EQ_DebugLcdPageRasterPauseCount = 0UL;
    EQ_DebugLcdPageRasterResumeCount = 0UL;
    EQ_DebugLcdFrontPage = EQ_UI_PAGE_DYNAMIC_STATUS;
    EQ_DebugLcdSwapTargetPage = EQ_UI_PAGE_DYNAMIC_STATUS;
    EQ_DebugLcdSwapPending = 0U;
    EQ_DebugLcdSwapDescriptorMask = 0U;
    EQ_DebugLcdEofCount = 0UL;
    EQ_DebugLcdEof0Count = 0UL;
    EQ_DebugLcdEof1Count = 0UL;
    EQ_DebugLcdEofAmbiguousCount = 0UL;
    EQ_DebugLcdSwapRequestCount = 0UL;
    EQ_DebugLcdSwapCompleteCount = 0UL;
    EQ_DebugLcdCacheMode = EQ_LCD_CACHE_UNKNOWN;
    EQ_DebugLcdBufferMar = 0UL;
    EQ_DebugLcdEditorBufferMar = 0UL;
    EQ_DebugLcdWritebackCount = 0UL;
    EQ_DebugLcdWritebackBytes = 0UL;
    EQ_DebugLcdWritebackFailureCount = 0UL;
    EQ_DebugLcdPendingDirtyRegionCount = 0U;
    EQ_DebugLcdMaxPendingDirtyRegionCount = 0U;
    EQ_DebugLcdPagePhase = 0U;
    EQ_DebugLcdDynamicDirtyMask = 0UL;
    EQ_DebugLcdEditorDirtyMask = 0UL;
    EQ_DebugLcdPageRequestedVersion[0] = 0UL;
    EQ_DebugLcdPageRequestedVersion[1] = 0UL;
    EQ_DebugLcdPageRenderedVersion[0] = 0UL;
    EQ_DebugLcdPageRenderedVersion[1] = 0UL;
    EQ_DebugLcdSwapTraceWriteIndex = 0U;
    EQ_DebugLcdSwapTraceCount = 0U;
    EQ_DebugLcdSwapTraceWrapCount = 0UL;
    memset((void *)EQ_DebugLcdSwapTrace, 0,
           sizeof(EQ_DebugLcdSwapTrace));
    s_swap_trace_process_frame = 0UL;
    EQ_DebugLcdRasterStopTimeoutCount = 0UL;
    EQ_SetExpectedFrontPage(EQ_UI_PAGE_DYNAMIC_STATUS);
    EQ_DebugLcdBufferAddress =
        EQ_PageBufferAddress(EQ_UI_PAGE_DYNAMIC_STATUS);
    EQ_DebugLcdBufferEndAddress =
        EQ_DebugLcdBufferAddress + EQ_LCD_BUFFER_TOTAL_BYTES - 1UL;
#if EQ_ENABLE_TEN_BAND_EDITOR != 0
    EQ_DebugLcdEditorBufferAddress =
        EQ_PageBufferAddress(EQ_UI_PAGE_EQ_EDITOR);
    EQ_DebugLcdEditorBufferEndAddress =
        EQ_DebugLcdEditorBufferAddress + EQ_LCD_BUFFER_TOTAL_BYTES - 1UL;
#else
    EQ_DebugLcdEditorBufferAddress = 0UL;
    EQ_DebugLcdEditorBufferEndAddress = 0UL;
#endif
    EQ_DebugLcdCurrentFrameBase = 0UL;
    EQ_DebugLcdCurrentFrameEnd = 0UL;
    EQ_DebugLcdCurrentFrame1Base = 0UL;
    EQ_DebugLcdCurrentFrame1End = 0UL;
    EQ_DebugLcdRasterFaultCount = 0UL;
    EQ_DebugLcdSyncLostCount = 0UL;
    EQ_DebugLcdFifoUnderflowCount = 0UL;
    EQ_DebugLcdFrameAddressMismatchCount = 0UL;
    EQ_DebugLcdLastRasterStatus = 0UL;
    EQ_DebugLcdLastIrqStatus = 0UL;
    EQ_DebugLcdStartupRasterStatus = 0UL;
    EQ_DebugLcdStartupStatusAfterClear = 0UL;
    EQ_DebugLcdStartupFaultClearCount = 0UL;
    EQ_DebugLcdFirstFaultFrame = 0UL;
    EQ_DebugLcdFirstFaultJob = 0UL;
    EQ_DebugLcdFramebufferCanaryCheckCount = 0UL;
    EQ_DebugLcdFramebufferCanaryFailureCount = 0UL;
    EQ_DebugLcdFramebufferFirstFailureFrame = 0UL;
    EQ_DebugLcdFramebufferFirstFailureJob = 0UL;
    EQ_DebugLcdFaultLatched = 0U;
    EQ_DebugLcdHardwareAuditRequest = 0U;
    EQ_DebugLcdHwSnapshot.frame_base = 0UL;
    EQ_DebugLcdHwSnapshot.frame_end = 0UL;
    EQ_DebugLcdHwSnapshot.frame1_base = 0UL;
    EQ_DebugLcdHwSnapshot.frame1_end = 0UL;
    EQ_DebugLcdHwSnapshot.raster_control = 0UL;
    EQ_DebugLcdHwSnapshot.raster_status = 0UL;
    EQ_DebugLcdHwSnapshot.dma_control = 0UL;
    EQ_DebugLcdHwSnapshot.irq_status = 0UL;
    s_lcd_last_audit_job = 0UL;
#if EQ_LCD_DIAGNOSTIC_ALIGNMENT_PATTERN
    s_lcd_last_audit_frame = 0UL;
#endif
    s_lcd_canary_ready = 0;
    s_lcd_seen_sync_lost = 0;
    s_lcd_seen_fifo_underflow = 0;
    s_lcd_seen_frame_mismatch = 0;
    for (index = 0; index < EQ_LCD_CATEGORY_COUNT; index++)
    {
        EQ_DebugLcdCategoryCount[index] = 0UL;
        EQ_DebugLcdCategoryLastCycles[index] = 0UL;
        EQ_DebugLcdCategoryMaxCycles[index] = 0UL;
    }
    for (index = 0; index < EQ_LCD_JOB_COUNT; index++)
    {
        EQ_DebugLcdJobTypeCount[index] = 0UL;
        EQ_DebugLcdJobTypeLastCycles[index] = 0UL;
        EQ_DebugLcdJobTypeMaxCycles[index] = 0UL;
    }
#if EQ_ENABLE_LCD_JOB_TIMING_CAPTURE != 0
    memset((void *)EQ_DebugLcdTimingSamples, 0,
           sizeof(EQ_DebugLcdTimingSamples));
    memset((void *)EQ_DebugLcdTimingTotalCount, 0,
           sizeof(EQ_DebugLcdTimingTotalCount));
    memset((void *)EQ_DebugLcdTimingSampleCount, 0,
           sizeof(EQ_DebugLcdTimingSampleCount));
    memset((void *)EQ_DebugLcdTimingDroppedCount, 0,
           sizeof(EQ_DebugLcdTimingDroppedCount));
    memset((void *)EQ_DebugLcdTimingMinCycles, 0,
           sizeof(EQ_DebugLcdTimingMinCycles));
    memset((void *)EQ_DebugLcdTimingMaxCycles, 0,
           sizeof(EQ_DebugLcdTimingMaxCycles));
    memset((void *)EQ_DebugLcdTimingOver2msCount, 0,
           sizeof(EQ_DebugLcdTimingOver2msCount));
    memset((void *)EQ_DebugLcdTimingOver5msCount, 0,
           sizeof(EQ_DebugLcdTimingOver5msCount));
    memset((void *)EQ_DebugLcdTimingDeferredByAudioCount, 0,
           sizeof(EQ_DebugLcdTimingDeferredByAudioCount));
    memset((void *)EQ_DebugLcdTimingAudioArrivedDuringDrawCount, 0,
           sizeof(EQ_DebugLcdTimingAudioArrivedDuringDrawCount));
    EQ_DebugLcdTimingLastClass = EQ_LCD_TIMING_CLASS_NONE;
    s_timing_last_service_class = EQ_LCD_TIMING_CLASS_NONE;
#endif
#if defined(EQ_ALGO_ONLY)
    s_mock_primitive_count = 0UL;
    s_mock_cycle_clock = 0UL;
    s_mock_forced_job_cycles = 0UL;
    s_mock_trace_job = EQ_LCD_JOB_NONE;
    s_mock_trace_frame = 0UL;
    memset(&s_mock_hw_snapshot, 0, sizeof(s_mock_hw_snapshot));
    s_mock_hw_snapshot.frame_base = EQ_DebugLcdExpectedFrameBase;
    s_mock_hw_snapshot.frame_end = EQ_DebugLcdExpectedFrameEnd;
    s_mock_hw_snapshot.raster_control = EQ_LCD_RASTER_ENABLE_MASK;
    s_mock_canary_failed = 0;
#else
#if EQ_ENABLE_TEN_BAND_EDITOR != 0
    IntDisable(C674X_MASK_INT14);
    s_raster_reconfigure_ready = 1U;
#endif
    Lcd_Init();
    /* The board driver enables raster before the first frame is populated.
       Hold it off until the complete Project 3.3 startup frame is ready. */
#if EQ_ENABLE_TEN_BAND_EDITOR != 0
    s_raster_reconfigure_ready = (unsigned int)
        EQ_StopRasterForReconfigure(EQ_LCD_INIT_STOP_TIMEOUT_SPINS);
    if (s_raster_reconfigure_ready == 0U)
    {
        s_eof_fault_pending = 1U;
        EQ_DebugLcdHardwareAuditRequest = 1U;
        EQ_DebugLcdRuntimeMask = 0U;
    }
#else
    RasterDisable(SOC_LCDC_0_REGS);
#endif
#if EQ_ENABLE_TEN_BAND_EDITOR != 0
    GrOffScreen16BPPInit(&s_editor_display, EQ_LcdEditorBuffer,
                         LCD_WIDTH, LCD_HEIGHT);
    GrContextInit(&s_editor_context, &s_editor_display);
    memcpy(EQ_LcdEditorBuffer + EQ_LCD_PALETTE_OFFSET,
           Lcd_Buffer + EQ_LCD_PALETTE_OFFSET,
           (size_t)EQ_LCD_PALETTE_SIZE);
#endif
#endif
    EQ_DetectFramebufferCacheMode();
#if EQ_ENABLE_TEN_BAND_EDITOR != 0
    EQ_PageSwapInit();
#endif
    EQ_SyncPageStateDebug();
    EQ_CaptureFramebufferCanary();
}

int EqualizerDisplay_DrawStaticLayout(void)
{
#if !EQ_LCD_DIAGNOSTIC_ALIGNMENT_PATTERN
    int index;
#if EQ_ENABLE_TEN_BAND_EDITOR == 0
    EQ_UI_RECT title_rect;
#endif
#endif

    if (s_runtime_started != 0)
    {
        EQ_DebugLcdUnexpectedFullRedrawCount++;
        return -1;
    }
    if (s_layout_drawn != 0)
    {
        return 0;
    }
#if !defined(EQ_ALGO_ONLY) && (EQ_ENABLE_TEN_BAND_EDITOR != 0)
    if (s_raster_reconfigure_ready == 0U)
    {
        return -1;
    }
#endif
    if (EqualizerUi_ValidateLayout() == 0)
    {
        EQ_DebugLcdBoundsFailureCount++;
        return -1;
    }
    if (EQ_BeginDraw() == 0)
    {
        return 0;
    }
#if EQ_LCD_DIAGNOSTIC_ALIGNMENT_PATTERN
    EQ_DrawAlignmentPattern();
#else
#if EQ_ENABLE_TEN_BAND_EDITOR != 0
    EQ_SetDrawPage(EQ_UI_PAGE_DYNAMIC_STATUS);
#endif
    EQ_LcdFillRectStartup(0, 0, EQ_UI_SCREEN_WIDTH, EQ_UI_SCREEN_HEIGHT,
                          EQ_COLOR_BG);
#if EQ_ENABLE_TEN_BAND_EDITOR != 0
    EQ_DrawPageTitle(EQ_UI_PAGE_DYNAMIC_STATUS);
#else
    title_rect.x = EQ_UI_TITLE_X;
    title_rect.y = EQ_UI_TITLE_Y;
    title_rect.w = EQ_UI_TITLE_W;
    title_rect.h = EQ_UI_TITLE_H;
#if EQ_LCD_USE_CHINESE
    EQ_DrawCnCentered(&title_rect, s_title, 8, EQ_COLOR_TEXT);
#else
    EQ_LcdDrawText(&title_rect, "P3.3 Dynamic EQ", 14,
                    EQ_FONT_LARGE, EQ_COLOR_TEXT, 1);
#endif
#endif
    for (index = 0; index < EQ_UI_PRESET_COUNT; index++)
    {
        EQ_DrawPresetStatic(index);
    }
    EQ_DrawAnalyzerStatic();
    EQ_DrawDynamicStatic();
#if EQ_ENABLE_TEN_BAND_EDITOR != 0
    for (index = 0; index < EQ_UI_PRESET_COUNT; index++)
    {
        EQ_DrawPresetJob(index);
    }
    for (index = 0; index < EQ_UI_ANALYZER_COUNT; index++)
    {
        EQ_DrawAnalyzerSnapshot(index);
    }
    for (index = 0; index < EQ_UI_DYNAMIC_COUNT; index++)
    {
        EQ_DrawDynamicSnapshot(index);
    }
    EQ_DrawPageSwitch(EQ_UI_PAGE_DYNAMIC_STATUS);
    EQ_SetDrawPage(EQ_UI_PAGE_EQ_EDITOR);
    EQ_LcdFillRectStartup(0, 0, EQ_UI_SCREEN_WIDTH, EQ_UI_SCREEN_HEIGHT,
                          EQ_COLOR_BG);
    EQ_DrawPageTitle(EQ_UI_PAGE_EQ_EDITOR);
    for (index = 0; index < EQ_NUM_BANDS; index++)
    {
        EQ_DrawEditorBandFull(index);
    }
    EQ_DrawEditorControlsFull();
    for (index = 0; index < 5; index++)
    {
        EQ_DrawEditorFieldFull(index);
    }
    EQ_DrawPageSwitch(EQ_UI_PAGE_EQ_EDITOR);
    EQ_SetDrawPage(EQ_UI_PAGE_DYNAMIC_STATUS);
    EqualizerUiLogic_MarkStartupRendered(&s_ui_state);
    EQ_SyncPageStateDebug();
#endif
#endif
    s_layout_drawn = 1;
    EQ_DebugLcdStaticDrawCount++;
    EQ_DebugLcdRefreshCount++;
    EQ_EndDraw();
#if !defined(EQ_ALGO_ONLY)
#if EQ_ENABLE_TEN_BAND_EDITOR != 0
    if (s_raster_reconfigure_ready != 0U)
    {
        RasterEnable(SOC_LCDC_0_REGS);
    }
#else
    RasterEnable(SOC_LCDC_0_REGS);
#endif
#endif
    EQ_ClearStartupFaultStatus();
    EqualizerDisplay_AuditHardware(0UL, 1);
    return 1;
}

void EqualizerDisplay_BeginRuntime(void)
{
    s_runtime_started = 1;
}

void EqualizerDisplay_RequestSnapshot(const EQ_UI_SNAPSHOT *snapshot,
                                      unsigned long process_frame)
{
#if EQ_LCD_DIAGNOSTIC_ALIGNMENT_PATTERN
    (void)snapshot;
    (void)process_frame;
    EQ_DebugLcdRuntimeMask = 0U;
    EQ_DebugLcdPendingMask = 0UL;
#else
#if EQ_ENABLE_TEN_BAND_EDITOR != 0
    EQ_UI_SNAPSHOT effective_snapshot;
    const EQ_UI_SNAPSHOT *request_snapshot;
    int requested_page;

    s_swap_trace_process_frame = process_frame;
    EQ_SwapStateLock();
    request_snapshot = snapshot;
    if (snapshot != 0)
    {
        requested_page = snapshot->page;
        if (((EQ_DebugLcdRuntimeMask & EQ_UI_RUNTIME_PAGE) == 0U) ||
            ((EQ_DebugLcdRuntimeMask & EQ_UI_RUNTIME_EDITOR) == 0U))
        {
            requested_page = EQ_UI_PAGE_DYNAMIC_STATUS;
        }
        if ((s_swap_pending != 0U) &&
            (s_swap_descriptor_mask != 0U))
        {
            effective_snapshot = *snapshot;
            effective_snapshot.page = s_swap_target_page;
            request_snapshot = &effective_snapshot;
            if (requested_page != s_swap_target_page)
            {
                s_deferred_page = requested_page;
                s_deferred_page_valid = 1U;
            }
            else
            {
                s_deferred_page_valid = 0U;
            }
        }
    }
    EqualizerUiLogic_Request(&s_ui_state, request_snapshot,
                             EQ_DebugLcdRuntimeMask, process_frame);
    if ((EqualizerUiLogic_IsPageBuilding(&s_ui_state) == 0) &&
        (s_swap_pending != 0U))
    {
        EQ_CancelPageSwap();
    }
    EQ_SwapStateUnlock();
#else
    s_swap_trace_process_frame = process_frame;
    EqualizerUiLogic_Request(&s_ui_state, snapshot,
                             EQ_DebugLcdRuntimeMask, process_frame);
#endif
    EQ_SyncPageStateDebug();
    EQ_DebugLcdPendingMask = s_ui_state.dirty_mask;
#endif
}

int EqualizerDisplay_GetDisplayedPage(void)
{
#if EQ_ENABLE_TEN_BAND_EDITOR != 0
    return EqualizerUiLogic_GetDisplayedPage(&s_ui_state);
#else
    return EQ_UI_PAGE_DYNAMIC_STATUS;
#endif
}

int EqualizerDisplay_IsPageBuilding(void)
{
#if EQ_ENABLE_TEN_BAND_EDITOR != 0
    return EqualizerUiLogic_IsPageBuilding(&s_ui_state);
#else
    return 0;
#endif
}

int EqualizerDisplay_HasPendingJob(void)
{
#if EQ_LCD_DIAGNOSTIC_ALIGNMENT_PATTERN
    EQ_DebugLcdRuntimeMask = 0U;
    EQ_DebugLcdPendingMask = 0UL;
    return 0;
#else
    return EqualizerUiLogic_HasPending(&s_ui_state);
#endif
}

int EqualizerDisplay_HasEligibleJob(unsigned long process_frame)
{
#if EQ_LCD_DIAGNOSTIC_ALIGNMENT_PATTERN
    (void)process_frame;
    return 0;
#else
    return EqualizerUiLogic_HasEligibleJob(&s_ui_state, process_frame);
#endif
}

#if EQ_ENABLE_LCD_JOB_TIMING_CAPTURE != 0
void EqualizerDisplay_RecordDeferredByAudio(unsigned long process_frame)
{
    int job;
    unsigned int completed_fields;
    unsigned int timing_class;

    s_timing_peek_state = s_ui_state;
    job = EqualizerUiLogic_SelectJob(
        &s_timing_peek_state, process_frame);
    completed_fields = EQ_LcdTimingNextFieldForJob(
        &s_timing_peek_state, job);
    timing_class = EQ_LcdTimingClassForJob(job, completed_fields);
    if (timing_class < EQ_LCD_TIMING_CLASS_COUNT)
    {
        EQ_DebugLcdTimingDeferredByAudioCount[timing_class]++;
    }
}

void EqualizerDisplay_RecordAudioArrivalDuringDraw(void)
{
    if (s_timing_last_service_class < EQ_LCD_TIMING_CLASS_COUNT)
    {
        EQ_DebugLcdTimingAudioArrivedDuringDrawCount
            [s_timing_last_service_class]++;
    }
}
#endif

int EqualizerDisplay_ServiceOneJob(unsigned long process_frame)
{
    int job;
    int category;
    int index;
    unsigned long start_cycles;
    unsigned long end_cycles;
    unsigned long elapsed_cycles;
    unsigned long tenths_ms;
    unsigned int completed_fields;
    int force_hardware_audit;
#if EQ_ENABLE_LCD_JOB_TIMING_CAPTURE != 0
    unsigned int timing_class;
#endif
#if EQ_ENABLE_TEN_BAND_EDITOR != 0
    int swap_completed;
    int sync_data_job;
#endif

    s_swap_trace_process_frame = process_frame;
    job = EqualizerUiLogic_SelectJob(&s_ui_state, process_frame);
    if (job == EQ_LCD_JOB_NONE)
    {
        return EQ_LCD_JOB_NONE;
    }
#if EQ_ENABLE_TEN_BAND_EDITOR != 0
    sync_data_job = (job == EQ_UI_JOB_PAGE_SYNC) ?
        EqualizerUiLogic_GetPageSyncJob(&s_ui_state) : EQ_UI_JOB_NONE;
#endif
    completed_fields = 0U;
#if EQ_ENABLE_LCD_JOB_TIMING_CAPTURE != 0
    s_timing_last_service_class = EQ_LCD_TIMING_CLASS_NONE;
#endif
    start_cycles = EQ_ReadCycles();
#if EQ_ENABLE_TEN_BAND_EDITOR != 0
    if (job == EQ_UI_JOB_PAGE_SWAP)
    {
        completed_fields = (unsigned int)EQ_ServicePageSwap(
            EqualizerUiLogic_GetPageTarget(&s_ui_state));
    }
    else
#endif
    {
        if (EQ_BeginDraw() == 0)
        {
            return EQ_LCD_JOB_NONE;
        }
#if defined(EQ_ALGO_ONLY)
        s_mock_trace_job = job;
        s_mock_trace_frame = process_frame;
#endif
        completed_fields = EQ_DrawJob(job);
#if defined(EQ_ALGO_ONLY)
        s_mock_trace_job = EQ_LCD_JOB_NONE;
        s_mock_trace_frame = 0UL;
#endif
        EQ_EndDraw();
    }
    end_cycles = EQ_ReadCycles();
    elapsed_cycles = end_cycles - start_cycles;
#if EQ_ENABLE_LCD_JOB_TIMING_CAPTURE != 0
    timing_class = EQ_LcdTimingClassForJob(job, completed_fields);
    if ((timing_class < EQ_LCD_TIMING_CLASS_COUNT) &&
        (timing_class != EQ_LCD_TIMING_CLASS_PAGE_SWAP))
    {
        EQ_RecordLcdTimingSample(timing_class, elapsed_cycles);
        s_timing_last_service_class = timing_class;
    }
#endif
    force_hardware_audit = 0;
#if EQ_ENABLE_TEN_BAND_EDITOR != 0
    swap_completed = 0;
#endif
    if ((job >= EQ_UI_JOB_DYNAMIC_0) &&
        (job <= EQ_UI_JOB_DYNAMIC_2))
    {
        EqualizerUiLogic_CompleteDynamicField(
            &s_ui_state, job, completed_fields, process_frame);
    }
    else if ((job >= EQ_UI_JOB_ANALYZER_0) &&
             (job <= EQ_UI_JOB_ANALYZER_3))
    {
        EqualizerUiLogic_CompleteAnalyzerField(
            &s_ui_state, job, completed_fields, process_frame);
    }
#if EQ_ENABLE_TEN_BAND_EDITOR != 0
    else if (job == EQ_UI_JOB_PAGE_SYNC)
    {
        EqualizerUiLogic_CompletePageSync(
            &s_ui_state, completed_fields, process_frame);
    }
    else if (job == EQ_UI_JOB_PAGE_SWAP)
    {
        if (completed_fields != 0U)
        {
            EqualizerUiLogic_CompletePageSwap(
                &s_ui_state, process_frame);
            swap_completed = 1;
            EQ_ApplyDeferredPageRequest(process_frame);
        }
    }
#endif
    else
    {
        EqualizerUiLogic_CompleteJob(&s_ui_state, job, process_frame);
    }
#if EQ_ENABLE_TEN_BAND_EDITOR != 0
    if ((job == EQ_UI_JOB_PAGE_SWAP) && (swap_completed != 0))
    {
        force_hardware_audit = 1;
    }
#endif
    EQ_SyncPageStateDebug();
    EQ_DebugLcdPendingMask = s_ui_state.dirty_mask;
    category = EQ_JobCategory(job);
    index = job - 1;
    tenths_ms = elapsed_cycles / 45600UL;
    EQ_DebugLcdJobCount++;
    EQ_DebugLcdRefreshCount++;
    EQ_DebugLcdLastJob = job;
    EQ_DebugLcdLastJobStartCycles = start_cycles;
    EQ_DebugLcdLastJobEndCycles = end_cycles;
    EQ_DebugLcdLastJobCycles = elapsed_cycles;
    EQ_DebugLcdLastJobTenthsMs = tenths_ms;
    if (elapsed_cycles > EQ_DebugLcdMaxJobCycles)
        EQ_DebugLcdMaxJobCycles = elapsed_cycles;
#if EQ_ENABLE_TEN_BAND_EDITOR != 0
    if ((job == EQ_UI_JOB_PAGE_SYNC) &&
        (elapsed_cycles > EQ_DebugLcdPageSyncMaxCycles))
    {
        EQ_DebugLcdPageSyncMaxCycles = elapsed_cycles;
        EQ_DebugLcdPageSyncMaxJob = (unsigned int)sync_data_job;
    }
#endif
    if (tenths_ms > EQ_DebugLcdMaxJobTenthsMs)
        EQ_DebugLcdMaxJobTenthsMs = tenths_ms;
    EQ_DebugLcdCategoryCount[category]++;
    EQ_DebugLcdCategoryLastCycles[category] = elapsed_cycles;
    if (elapsed_cycles > EQ_DebugLcdCategoryMaxCycles[category])
        EQ_DebugLcdCategoryMaxCycles[category] = elapsed_cycles;
    EQ_DebugLcdJobTypeCount[index]++;
    EQ_DebugLcdJobTypeLastCycles[index] = elapsed_cycles;
    if (elapsed_cycles > EQ_DebugLcdJobTypeMaxCycles[index])
        EQ_DebugLcdJobTypeMaxCycles[index] = elapsed_cycles;
    if (elapsed_cycles > EQ_LCD_JOB_GOAL_CYCLES)
    {
        EQ_DebugLcdOver1msCount++;
    }
    if (elapsed_cycles > EQ_LCD_JOB_ACCEPTANCE_CYCLES)
    {
        EQ_DebugLcdOver2msCount++;
        EQ_DebugLcdBudgetExceededCount++;
#if EQ_ENABLE_TEN_BAND_EDITOR != 0
        if (job == EQ_UI_JOB_PAGE_SYNC)
        {
            EQ_DebugLcdPageSyncLastOver2msJob =
                (unsigned int)sync_data_job;
        }
#endif
    }
    if (elapsed_cycles > EQ_LCD_JOB_HARD_CYCLES)
    {
        EQ_DebugLcdOver5msCount++;
        EQ_DebugLcdHardBudgetExceededCount++;
        EqualizerDisplay_AuditHardware(process_frame, 1);
        EqualizerDisplay_AutoDisable(EQ_LCD_AUTO_DISABLE_JOB_OVER_5MS);
    }
    else
    {
        EqualizerDisplay_AuditHardware(process_frame,
                                       force_hardware_audit);
    }
    return job;
}

void EqualizerDisplay_CancelRuntimeJobs(void)
{
#if EQ_ENABLE_TEN_BAND_EDITOR != 0
    EQ_SwapStateLock();
    s_deferred_page_valid = 0U;
    if ((s_swap_pending != 0U) &&
        (s_swap_descriptor_mask != 0U))
    {
        s_cancel_after_swap = 1U;
        EQ_DebugLcdPendingMask = s_ui_state.dirty_mask;
        EQ_SwapStateUnlock();
        return;
    }
    if (s_swap_complete != 0U)
    {
        s_swap_complete = 0U;
        EqualizerUiLogic_CompletePageSwap(
            &s_ui_state, s_ui_state.request_frame);
    }
    s_cancel_after_swap = 0U;
    EQ_CancelPageSwap();
    EqualizerUiLogic_Cancel(&s_ui_state);
    EQ_SyncPageStateDebug();
    EQ_DebugLcdPendingMask = 0UL;
    EQ_SwapStateUnlock();
#else
    EqualizerUiLogic_Cancel(&s_ui_state);
    EQ_SyncPageStateDebug();
    EQ_DebugLcdPendingMask = 0UL;
#endif
}

void EqualizerDisplay_AutoDisable(unsigned long reason)
{
    EQ_DebugLcdAutoDisableReason |= reason;
    if (EQ_DebugLcdRuntimeMask != 0U)
    {
        EQ_DebugLcdRuntimeMask = 0U;
        EQ_DebugLcdAutoDisabledCount++;
    }
#if EQ_ENABLE_TEN_BAND_EDITOR != 0
    if ((reason & (EQ_LCD_AUTO_DISABLE_HW_FAULT |
                   EQ_LCD_AUTO_DISABLE_CANARY)) != 0UL)
    {
        EQ_SwapStateLock();
        s_deferred_page_valid = 0U;
        s_cancel_after_swap = 0U;
        if (s_swap_complete != 0U)
        {
            s_swap_complete = 0U;
            EqualizerUiLogic_CompletePageSwap(
                &s_ui_state, s_ui_state.request_frame);
        }
        EQ_ForceStopPageSwap();
        EqualizerUiLogic_Cancel(&s_ui_state);
        EQ_DebugLcdPendingMask = 0UL;
        EQ_SwapStateUnlock();
        return;
    }
#endif
    EqualizerDisplay_CancelRuntimeJobs();
}

#if defined(EQ_ALGO_ONLY)
unsigned long EqualizerDisplay_TestPrimitiveCount(void)
{
    return s_mock_primitive_count;
}

int EqualizerDisplay_TestTraceOpen(const char *path)
{
    if (s_mock_trace_file != 0)
    {
        fclose(s_mock_trace_file);
        s_mock_trace_file = 0;
    }
    s_mock_trace_record_count = 0UL;
    if (path == 0)
    {
        return 0;
    }
    s_mock_trace_file = fopen(path, "w");
    return (s_mock_trace_file != 0) ? 1 : 0;
}

void EqualizerDisplay_TestTraceClose(void)
{
    if (s_mock_trace_file != 0)
    {
        fflush(s_mock_trace_file);
        fclose(s_mock_trace_file);
        s_mock_trace_file = 0;
    }
}

unsigned long EqualizerDisplay_TestTraceRecordCount(void)
{
    return s_mock_trace_record_count;
}

void EqualizerDisplay_TestForceJobCycles(unsigned long cycles)
{
    s_mock_forced_job_cycles = cycles;
}

void EqualizerDisplay_TestSetHardwareSnapshot(
    const EQ_LCD_HW_SNAPSHOT *snapshot)
{
    if (snapshot != 0)
    {
        s_mock_hw_snapshot = *snapshot;
    }
}

void EqualizerDisplay_TestSetCanaryFailure(int failed)
{
    s_mock_canary_failed = (failed != 0) ? 1 : 0;
}

void EqualizerDisplay_TestSetCacheMode(unsigned int mode)
{
    EQ_DebugLcdCacheMode = mode;
}

void EqualizerDisplay_TestDrawRect(int x, int y, int w, int h)
{
    EQ_LcdFillRect(x, y, w, h, EQ_COLOR_BG);
}

#if EQ_ENABLE_LCD_JOB_TIMING_CAPTURE != 0
void EqualizerDisplay_TestRecordTimingSample(
    unsigned int timing_class, unsigned long cycles)
{
    EQ_RecordLcdTimingSample(timing_class, cycles);
    s_timing_last_service_class = timing_class;
}
#endif

#if EQ_ENABLE_TEN_BAND_EDITOR != 0
void EqualizerDisplay_TestInjectEofStatus(unsigned long status)
{
    EQ_HandleEofStatus(status);
}
#endif
#endif

#else

void EqualizerDisplay_Init(void)
{
}

int EqualizerDisplay_DrawStaticLayout(void)
{
    return 0;
}

void EqualizerDisplay_BeginRuntime(void)
{
}

void EqualizerDisplay_RequestSnapshot(const EQ_UI_SNAPSHOT *snapshot,
                                      unsigned long process_frame)
{
    (void)snapshot;
    (void)process_frame;
}

int EqualizerDisplay_GetDisplayedPage(void)
{
    return EQ_UI_PAGE_DYNAMIC_STATUS;
}

int EqualizerDisplay_IsPageBuilding(void)
{
    return 0;
}

int EqualizerDisplay_HasPendingJob(void)
{
    return 0;
}

int EqualizerDisplay_HasEligibleJob(unsigned long process_frame)
{
    (void)process_frame;
    return 0;
}

int EqualizerDisplay_ServiceOneJob(unsigned long process_frame)
{
    (void)process_frame;
    return EQ_LCD_JOB_NONE;
}

void EqualizerDisplay_AuditHardware(unsigned long process_frame, int force)
{
    (void)process_frame;
    (void)force;
}

void EqualizerDisplay_CancelRuntimeJobs(void)
{
}

void EqualizerDisplay_AutoDisable(unsigned long reason)
{
    (void)reason;
}

#endif
