/**
 * user_equalizer_display.c
 *
 * Fixed-region LCD renderer for the Project 3.3 status UI.
 */

#include "user_equalizer_display.h"

#if defined(EQ_ALGO_ONLY) || (EQ_ENABLE_LCD_DISPLAY != 0)

#include "string.h"

#if (!defined(EQ_ALGO_ONLY))
#include "lcd_api.h"
#include "lcd_dma.h"
#include "hw_lcdc.h"
#include "hw_types.h"
#include "raster.h"
#include "soc_C6748.h"
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
#define EQ_UI_ANALYZER_VALUE_X_OFFSET 76
#define EQ_UI_ANALYZER_VALUE_Y_OFFSET 80
#define EQ_UI_ANALYZER_VALUE_W 42
#define EQ_UI_ANALYZER_VALUE_H 22
#define EQ_UI_ANALYZER_ZERO_Y \
    ((EQ_UI_ANALYZER_BAR_TOP + EQ_UI_ANALYZER_BAR_BOTTOM) / 2)

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
#if defined(EQ_ALGO_ONLY)
#define EQ_LCD_STATUS_SYNC_MASK 0x00000004UL
#define EQ_LCD_STATUS_FIFO_UNDERFLOW_MASK 0x00000020UL
#define EQ_LCD_RASTER_ENABLE_MASK 0x00000001UL
#else
#define EQ_LCD_STATUS_SYNC_MASK ((unsigned long)LCDC_LCD_STAT_SYNC)
#define EQ_LCD_STATUS_FIFO_UNDERFLOW_MASK \
    ((unsigned long)LCDC_LCD_STAT_FUF)
#define EQ_LCD_RASTER_ENABLE_MASK \
    ((unsigned long)LCDC_RASTER_CTRL_RASTER_EN)
#endif
#define EQ_LCD_CLEARABLE_FAULT_MASK \
    (EQ_LCD_STATUS_SYNC_MASK | EQ_LCD_STATUS_FIFO_UNDERFLOW_MASK)

#if !defined(EQ_ALGO_ONLY)
#if (PALETTE_OFFSET != 4) || (PALETTE_SIZE != 32)
#error Project 3.3 LCD audit assumes the current 4-byte header and 32-byte palette.
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
static const unsigned char s_preset_flat[] = { CN_PING, CN_ZHI };
static const unsigned char s_preset_bass[] = { CN_DI, CN_YIN };
static const unsigned char s_preset_vocal[] = { CN_REN, CN_SHENG };
static const unsigned char s_preset_treble[] = { CN_GAO, CN_YIN };
static const unsigned char s_analyzer_bass[] = { CN_DI, CN_PIN };
static const unsigned char s_analyzer_mud[] = { CN_HUN, CN_ZHUO };
static const unsigned char s_analyzer_presence[] = { CN_QING, CN_XI_CLEAR };
static const unsigned char s_analyzer_brightness[] = { CN_GAO, CN_PIN };
static const unsigned char s_dynamic_smart[] =
{
    CN_ZHI_WISE, CN_NENG, CN_DI, CN_PIN
};
static const unsigned char s_dynamic_clarity[] =
{
    CN_QING, CN_XI_CLEAR, CN_DU
};
static const unsigned char s_dynamic_guard[] =
{
    CN_GAO, CN_PIN, CN_BAO, CN_HU
};
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
volatile unsigned long EQ_DebugLcdLastJobTenthsMs = 0UL;
volatile unsigned long EQ_DebugLcdMaxJobTenthsMs = 0UL;
volatile int EQ_DebugLcdLastJob = EQ_LCD_JOB_NONE;
volatile unsigned long EQ_DebugLcdAutoDisabledCount = 0UL;
volatile unsigned long EQ_DebugLcdAutoDisableReason = 0UL;
volatile unsigned long EQ_DebugLcdCategoryCount[EQ_LCD_CATEGORY_COUNT];
volatile unsigned long EQ_DebugLcdCategoryLastCycles[EQ_LCD_CATEGORY_COUNT];
volatile unsigned long EQ_DebugLcdCategoryMaxCycles[EQ_LCD_CATEGORY_COUNT];
volatile unsigned long EQ_DebugLcdJobTypeCount[EQ_LCD_JOB_COUNT];
volatile unsigned long EQ_DebugLcdJobTypeLastCycles[EQ_LCD_JOB_COUNT];
volatile unsigned long EQ_DebugLcdJobTypeMaxCycles[EQ_LCD_JOB_COUNT];
volatile unsigned long EQ_DebugLcdExpectedFrameBase = 0UL;
volatile unsigned long EQ_DebugLcdExpectedFrameEnd = 0UL;
volatile unsigned long EQ_DebugLcdBufferAddress = 0UL;
volatile unsigned long EQ_DebugLcdBufferEndAddress = 0UL;
volatile unsigned long EQ_DebugLcdCurrentFrameBase = 0UL;
volatile unsigned long EQ_DebugLcdCurrentFrameEnd = 0UL;
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
#pragma RETAIN(EQ_DebugUiStateBytes)
#endif
volatile const unsigned long EQ_DebugUiStateBytes =
    (unsigned long)sizeof(EQ_UI_STATE);

static EQ_UI_STATE s_ui_state;
static volatile int s_lcd_busy = 0;
static int s_layout_drawn = 0;
static int s_runtime_started = 0;
static unsigned long s_lcd_last_audit_job = 0UL;
static unsigned long s_lcd_last_audit_frame = 0UL;
static unsigned int s_lcd_canary_front = 0U;
static int s_lcd_canary_ready = 0;
static int s_lcd_seen_sync_lost = 0;
static int s_lcd_seen_fifo_underflow = 0;
static int s_lcd_seen_frame_mismatch = 0;

#if defined(EQ_ALGO_ONLY)
static unsigned long s_mock_primitive_count = 0UL;
static unsigned long s_mock_cycle_clock = 0UL;
static unsigned long s_mock_forced_job_cycles = 0UL;
static EQ_LCD_HW_SNAPSHOT s_mock_hw_snapshot;
static int s_mock_canary_failed = 0;
#endif

static unsigned long EQ_BufferAddress(void)
{
#if defined(EQ_ALGO_ONLY)
    return EQ_LCD_HOST_BUFFER_ADDRESS;
#else
    return (unsigned long)Lcd_Buffer;
#endif
}

static unsigned long EQ_ExpectedFrameBase(void)
{
    return (EQ_BufferAddress() + EQ_LCD_PALETTE_OFFSET) &
           EQ_LCD_DMA_ADDRESS_MASK;
}

static unsigned long EQ_ExpectedFrameEnd(void)
{
    return (EQ_ExpectedFrameBase() + EQ_LCD_FRAME_DMA_BYTES - 2UL) &
           EQ_LCD_DMA_ADDRESS_MASK;
}

static void EQ_CopyHardwareSnapshotToDebug(
    const EQ_LCD_HW_SNAPSHOT *snapshot)
{
    EQ_DebugLcdHwSnapshot.frame_base = snapshot->frame_base;
    EQ_DebugLcdHwSnapshot.frame_end = snapshot->frame_end;
    EQ_DebugLcdHwSnapshot.raster_control = snapshot->raster_control;
    EQ_DebugLcdHwSnapshot.raster_status = snapshot->raster_status;
    EQ_DebugLcdHwSnapshot.dma_control = snapshot->dma_control;
    EQ_DebugLcdHwSnapshot.irq_status = snapshot->irq_status;
    EQ_DebugLcdCurrentFrameBase = snapshot->frame_base;
    EQ_DebugLcdCurrentFrameEnd = snapshot->frame_end;
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

#if EQ_LCD_DIAGNOSTIC_ALIGNMENT_PATTERN
static void EQ_ClearAlignmentStartupFaultStatus(void)
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
#endif

static void EQ_CaptureFramebufferCanary(void)
{
#if defined(EQ_ALGO_ONLY)
    s_lcd_canary_front = 0U;
#else
    volatile unsigned int *front;

    front = (volatile unsigned int *)(Lcd_Buffer);
    s_lcd_canary_front = *front;
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
            (EQ_DebugLcdBufferAddress != EQ_BufferAddress()) ||
            (EQ_DebugLcdBufferEndAddress !=
             (EQ_BufferAddress() + EQ_LCD_BUFFER_TOTAL_BYTES - 1UL)) ||
            (EQ_DebugLcdExpectedFrameBase != EQ_ExpectedFrameBase()) ||
            (EQ_DebugLcdExpectedFrameEnd != EQ_ExpectedFrameEnd()))
        {
            failed = 1;
        }
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
        EQ_LatchHardwareFault(process_frame,
                              EQ_LCD_AUTO_DISABLE_CANARY);
    }
    return failed;
}

void EqualizerDisplay_AuditHardware(unsigned long process_frame, int force)
{
    EQ_LCD_HW_SNAPSHOT snapshot;
    unsigned long status;
    int due;
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
    s_lcd_last_audit_frame = process_frame;

    (void)EQ_CheckFramebufferCanary(process_frame);
    EQ_ReadHardwareSnapshot(&snapshot);
    EQ_CopyHardwareSnapshotToDebug(&snapshot);
    status = snapshot.raster_status;
    frame_mismatch =
        (snapshot.frame_base != EQ_DebugLcdExpectedFrameBase) ||
        (snapshot.frame_end != EQ_DebugLcdExpectedFrameEnd);
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
    if (hardware_fault != 0)
    {
        if (EQ_DebugLcdRasterFaultCount == 0UL)
        {
            EQ_DebugLcdRasterFaultCount++;
        }
        EQ_LatchHardwareFault(process_frame,
                              EQ_LCD_AUTO_DISABLE_HW_FAULT);
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

static int EQ_BeginDraw(void)
{
    if (s_lcd_busy != 0)
    {
        EQ_DebugLcdSkipBusyCount++;
        return 0;
    }
    s_lcd_busy = 1;
    return 1;
}

static void EQ_EndDraw(void)
{
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
        GrContextFontSet(&Lcd_Context, &g_sFontCm24);
    }
    else if (font == EQ_FONT_MEDIUM)
    {
        GrContextFontSet(&Lcd_Context, &g_sFontCm18);
    }
    else
    {
        GrContextFontSet(&Lcd_Context, &g_sFontCm12);
    }
}
#else
static void EQ_SetFont(int font)
{
    (void)font;
}
#endif

static void EQ_LcdFillRect(int x, int y, int w, int h, int color)
{
#if !defined(EQ_ALGO_ONLY)
    tRectangle rect;
#endif

    if (EQ_CheckRect(x, y, w, h) == 0)
    {
        return;
    }
#if !defined(EQ_ALGO_ONLY)
    rect.sXMin = x;
    rect.sYMin = y;
    rect.sXMax = x + w - 1;
    rect.sYMax = y + h - 1;
    GrContextForegroundSet(&Lcd_Context, EQ_MapColor(color));
    GrRectFill(&Lcd_Context, &rect);
#else
    (void)color;
    s_mock_primitive_count++;
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
#if !defined(EQ_ALGO_ONLY)
    rect.sXMin = x;
    rect.sYMin = y;
    rect.sXMax = x + w - 1;
    rect.sYMax = y + h - 1;
    GrContextForegroundSet(&Lcd_Context, EQ_MapColor(color));
    GrRectDraw(&Lcd_Context, &rect);
#else
    (void)color;
    s_mock_primitive_count++;
#endif
}

static void EQ_LcdDrawLine(int x1, int y1, int x2, int y2, int color)
{
    x1 = EQ_ClampInt(x1, 0, EQ_UI_SCREEN_WIDTH - 1);
    x2 = EQ_ClampInt(x2, 0, EQ_UI_SCREEN_WIDTH - 1);
    y1 = EQ_ClampInt(y1, 0, EQ_UI_SCREEN_HEIGHT - 1);
    y2 = EQ_ClampInt(y2, 0, EQ_UI_SCREEN_HEIGHT - 1);
#if !defined(EQ_ALGO_ONLY)
    GrContextForegroundSet(&Lcd_Context, EQ_MapColor(color));
    GrLineDraw(&Lcd_Context, x1, y1, x2, y2);
#else
    (void)color;
    s_mock_primitive_count++;
#endif
}

#if EQ_LCD_USE_CHINESE
static void EQ_LcdDrawHLine(int x1, int x2, int y, int color)
{
    x1 = EQ_ClampInt(x1, 0, EQ_UI_SCREEN_WIDTH - 1);
    x2 = EQ_ClampInt(x2, 0, EQ_UI_SCREEN_WIDTH - 1);
    y = EQ_ClampInt(y, 0, EQ_UI_SCREEN_HEIGHT - 1);
#if !defined(EQ_ALGO_ONLY)
    GrContextForegroundSet(&Lcd_Context, EQ_MapColor(color));
    GrLineDrawH(&Lcd_Context, x1, x2, y);
#else
    (void)color;
    s_mock_primitive_count++;
#endif
}
#endif

static void EQ_LcdDrawText(const EQ_UI_RECT *rect, const char *text,
                           int length, int font, int color, int centered)
{
    int x;
    int y;

    if (EQ_CheckRect(rect->x, rect->y, rect->w, rect->h) == 0)
    {
        return;
    }
    EQ_SetFont(font);
    x = rect->x + 2;
    y = rect->y + ((rect->h - 12) / 2);
#if !defined(EQ_ALGO_ONLY)
    GrContextForegroundSet(&Lcd_Context, EQ_MapColor(color));
    if (centered != 0)
    {
        GrStringDrawCentered(&Lcd_Context, text, length,
                             rect->x + rect->w / 2, y + 5, 0);
    }
    else
    {
        GrStringDraw(&Lcd_Context, text, length, x, y, 0);
    }
#else
    (void)text;
    (void)length;
    (void)color;
    (void)centered;
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

    EQ_LcdFillRect(0, 0, EQ_UI_SCREEN_WIDTH, EQ_UI_SCREEN_HEIGHT,
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

static int EQ_FormatSignedDb(char *buffer, int value)
{
    int length;

    value = EQ_ClampInt(value, -20, 20);
    length = 0;
    if (value >= 0)
    {
        buffer[length++] = '+';
    }
    else
    {
        buffer[length++] = '-';
        value = -value;
    }
    if (value >= 10)
    {
        buffer[length++] = (char)('0' + (value / 10));
    }
    buffer[length++] = (char)('0' + (value % 10));
    buffer[length] = '\0';
    return length;
}

static int EQ_FormatLevel(char *buffer, int level)
{
    level = EQ_ClampInt(level, 0, 9);
    buffer[0] = 'L';
    buffer[1] = (char)('0' + level);
    buffer[2] = '\0';
    return 2;
}

static void EQ_ClearInside(const EQ_UI_RECT *rect)
{
    EQ_LcdFillRect(rect->x + 1, rect->y + 1,
                   rect->w - 2, rect->h - 2, EQ_COLOR_BG);
}

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

static void EQ_DrawChainStatic(void)
{
    EQ_UI_RECT rect;
    int index;

    rect.x = 190; rect.y = 82; rect.w = 38; rect.h = 26;
    EQ_LcdDrawText(&rect, "ADC", 3, EQ_FONT_SMALL, EQ_COLOR_TEXT, 1);
    rect.x = 228; rect.w = 42;
    EQ_LcdDrawText(&rect, " -> ", 4, EQ_FONT_SMALL, EQ_COLOR_MUTED, 1);
    rect.x = 270; rect.w = 30;
    EQ_LcdDrawText(&rect, "EQ", 2, EQ_FONT_SMALL, EQ_COLOR_TEXT, 1);
    rect.x = 300; rect.w = 42;
    EQ_LcdDrawText(&rect, " -> ", 4, EQ_FONT_SMALL, EQ_COLOR_MUTED, 1);
    rect.x = 394; rect.w = 44;
    EQ_LcdDrawText(&rect, " -> ", 4, EQ_FONT_SMALL, EQ_COLOR_MUTED, 1);
    rect.x = 482; rect.w = 44;
    EQ_LcdDrawText(&rect, " -> ", 4, EQ_FONT_SMALL, EQ_COLOR_MUTED, 1);
    rect.x = 562; rect.w = 44;
    EQ_LcdDrawText(&rect, " -> ", 4, EQ_FONT_SMALL, EQ_COLOR_MUTED, 1);
    rect.x = 606; rect.w = 42;
    EQ_LcdDrawText(&rect, "DAC", 3, EQ_FONT_SMALL, EQ_COLOR_TEXT, 1);
    for (index = 0; index < EQ_UI_CHAIN_COUNT; index++)
    {
        EQ_LcdDrawRect(EQ_UI_CHAIN_RECTS[index].x,
                       EQ_UI_CHAIN_RECTS[index].y,
                       EQ_UI_CHAIN_RECTS[index].w,
                       EQ_UI_CHAIN_RECTS[index].h,
                       EQ_COLOR_BORDER);
    }
    EQ_LcdDrawText(&EQ_UI_CHAIN_RECTS[0], "BASS", 4,
                   EQ_FONT_SMALL, EQ_COLOR_MUTED, 1);
    EQ_LcdDrawText(&EQ_UI_CHAIN_RECTS[1], "CLR", 3,
                   EQ_FONT_SMALL, EQ_COLOR_MUTED, 1);
    EQ_LcdDrawText(&EQ_UI_CHAIN_RECTS[2], "HF", 2,
                   EQ_FONT_SMALL, EQ_COLOR_MUTED, 1);
}

static void EQ_DrawAnalyzerStatic(void)
{
#if EQ_LCD_USE_CHINESE
    static const unsigned char * const cn_labels[EQ_UI_ANALYZER_COUNT] =
    {
        s_analyzer_bass, s_analyzer_mud,
        s_analyzer_presence, s_analyzer_brightness
    };
#else
    static const char * const en_labels[EQ_UI_ANALYZER_COUNT] =
    {
        "BASS", "MUD", "PRES", "BRIGHT"
    };
    static const int en_lengths[EQ_UI_ANALYZER_COUNT] = { 4, 3, 4, 6 };
#endif
    int band;
    int inner_x;
    EQ_UI_RECT label_rect;
    EQ_UI_RECT scale_rect;

    scale_rect.x = 8; scale_rect.y = 116; scale_rect.w = 46; scale_rect.h = 18;
    EQ_LcdDrawText(&scale_rect, "+20", 3, EQ_FONT_SMALL,
                   EQ_COLOR_MUTED, 1);
    scale_rect.y = EQ_UI_ANALYZER_ZERO_Y - 9;
    EQ_LcdDrawText(&scale_rect, "0", 1, EQ_FONT_SMALL,
                   EQ_COLOR_ZERO, 1);
    scale_rect.y = 288;
    EQ_LcdDrawText(&scale_rect, "-20", 3, EQ_FONT_SMALL,
                   EQ_COLOR_MUTED, 1);
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
#if EQ_LCD_USE_CHINESE
        EQ_DrawCnCentered(&label_rect, cn_labels[band], 2, EQ_COLOR_TEXT);
#else
        EQ_LcdDrawText(&label_rect, en_labels[band], en_lengths[band],
                       EQ_FONT_SMALL, EQ_COLOR_TEXT, 1);
#endif
    }
}

static void EQ_DrawDynamicStatic(void)
{
#if EQ_LCD_USE_CHINESE
    static const unsigned char * const cn_labels[EQ_UI_DYNAMIC_COUNT] =
    {
        s_dynamic_smart, s_dynamic_clarity, s_dynamic_guard
    };
    static const int cn_counts[EQ_UI_DYNAMIC_COUNT] = { 4, 3, 4 };
#else
    static const char * const en_labels[EQ_UI_DYNAMIC_COUNT] =
    {
        "SMART BASS", "CLARITY", "HF GUARD"
    };
    static const int en_lengths[EQ_UI_DYNAMIC_COUNT] = { 10, 7, 8 };
#endif
    EQ_UI_RECT label_rect;
    int index;

    for (index = 0; index < EQ_UI_DYNAMIC_COUNT; index++)
    {
        label_rect.x = 24;
        label_rect.y = EQ_UI_DYNAMIC_RECTS[index].y;
        label_rect.w = 124;
        label_rect.h = EQ_UI_DYNAMIC_RECTS[index].h;
#if EQ_LCD_USE_CHINESE
        EQ_DrawCnCentered(&label_rect, cn_labels[index],
                          cn_counts[index], EQ_COLOR_TEXT);
#else
        EQ_LcdDrawText(&label_rect, en_labels[index], en_lengths[index],
                       EQ_FONT_SMALL, EQ_COLOR_TEXT, 0);
#endif
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
        EQ_LcdDrawRect(EQ_UI_DYNAMIC_LEVEL_RECTS[index].x,
                       EQ_UI_DYNAMIC_LEVEL_RECTS[index].y,
                       EQ_UI_DYNAMIC_LEVEL_RECTS[index].w,
                       EQ_UI_DYNAMIC_LEVEL_RECTS[index].h,
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
        EQ_LcdDrawText(&EQ_UI_DYNAMIC_LEVEL_RECTS[index], "L0", 2,
                       EQ_FONT_SMALL, EQ_COLOR_TEXT, 1);
    }
}
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

static void EQ_DrawChainJob(int index)
{
    static const char * const labels[EQ_UI_CHAIN_COUNT] =
    {
        "BASS", "CLR", "HF"
    };
    static const int lengths[EQ_UI_CHAIN_COUNT] = { 4, 3, 2 };
    int enabled;

    if (index == 0)
        enabled = s_ui_state.requested.smart_enabled;
    else if (index == 1)
        enabled = s_ui_state.requested.clarity_enabled;
    else
        enabled = s_ui_state.requested.guard_enabled;
    EQ_ClearInside(&EQ_UI_CHAIN_RECTS[index]);
    EQ_LcdDrawText(&EQ_UI_CHAIN_RECTS[index], labels[index], lengths[index],
                   EQ_FONT_SMALL,
                   enabled ? EQ_COLOR_ACTIVE : EQ_COLOR_MUTED, 1);
}

static void EQ_DrawAnalyzerJob(int band)
{
    EQ_UI_RECT bar_rect;
    EQ_UI_RECT value_rect;
    char buffer[5];
    int length;
    int pixel;
    int top;
    int height;
    int valid;

    bar_rect.x = EQ_UI_ANALYZER_RECTS[band].x +
                 EQ_UI_ANALYZER_INNER_X_OFFSET + 1;
    bar_rect.y = EQ_UI_ANALYZER_BAR_TOP + 1;
    bar_rect.w = EQ_UI_ANALYZER_INNER_W - 2;
    bar_rect.h = EQ_UI_ANALYZER_BAR_BOTTOM -
                 EQ_UI_ANALYZER_BAR_TOP - 1;
    value_rect.x = EQ_UI_ANALYZER_RECTS[band].x +
                   EQ_UI_ANALYZER_VALUE_X_OFFSET;
    value_rect.y = EQ_UI_ANALYZER_RECTS[band].y +
                   EQ_UI_ANALYZER_VALUE_Y_OFFSET;
    value_rect.w = EQ_UI_ANALYZER_VALUE_W;
    value_rect.h = EQ_UI_ANALYZER_VALUE_H;
    EQ_LcdFillRect(bar_rect.x, bar_rect.y, bar_rect.w, bar_rect.h,
                   EQ_COLOR_BG);
    EQ_LcdFillRect(value_rect.x, value_rect.y,
                   value_rect.w, value_rect.h, EQ_COLOR_BG);
    valid = (s_ui_state.requested.analyzer_valid != 0) &&
            (s_ui_state.requested.analyzer_warm != 0);
    if (valid == 0)
    {
        EQ_LcdDrawText(&value_rect, "--", 2, EQ_FONT_SMALL,
                       EQ_COLOR_MUTED, 1);
        return;
    }
    pixel = EQ_ClampInt(s_ui_state.requested.band_pixel[band],
                        EQ_UI_ANALYZER_BAR_TOP,
                        EQ_UI_ANALYZER_BAR_BOTTOM);
    if (pixel <= EQ_UI_ANALYZER_ZERO_Y)
    {
        top = pixel;
        height = EQ_UI_ANALYZER_ZERO_Y - pixel + 1;
        EQ_LcdFillRect(bar_rect.x, top, bar_rect.w, height,
                       EQ_COLOR_BAR_POS);
    }
    else
    {
        top = EQ_UI_ANALYZER_ZERO_Y;
        height = pixel - EQ_UI_ANALYZER_ZERO_Y + 1;
        EQ_LcdFillRect(bar_rect.x, top, bar_rect.w, height,
                       EQ_COLOR_BAR_NEG);
    }
    length = EQ_FormatSignedDb(buffer,
                               s_ui_state.requested.band_value_db[band]);
    EQ_LcdDrawText(&value_rect, buffer, length,
                   EQ_FONT_SMALL, EQ_COLOR_TEXT, 1);
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

static int EQ_DynamicLevel(int index)
{
    if (index == 0)
        return s_ui_state.requested.smart_level;
    if (index == 1)
        return s_ui_state.requested.clarity_level;
    return s_ui_state.requested.guard_level;
}

static unsigned int EQ_DrawDynamicJob(int index)
{
    unsigned int fields;
    unsigned int selected_field;
    int enabled;
    int strength;
    char buffer[3];
    int length;

    fields = EqualizerUiLogic_DynamicFieldMask(&s_ui_state, index);
    if ((fields & EQ_UI_DYNAMIC_FIELD_ENABLED) != 0U)
        selected_field = EQ_UI_DYNAMIC_FIELD_ENABLED;
    else if ((fields & EQ_UI_DYNAMIC_FIELD_STRENGTH) != 0U)
        selected_field = EQ_UI_DYNAMIC_FIELD_STRENGTH;
    else
        selected_field = fields & EQ_UI_DYNAMIC_FIELD_LEVEL;
    enabled = EQ_DynamicEnabled(index);
    strength = EQ_DynamicStrength(index);
    if ((selected_field & EQ_UI_DYNAMIC_FIELD_ENABLED) != 0U)
    {
        EQ_ClearInside(&EQ_UI_DYNAMIC_TOGGLE_RECTS[index]);
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
        EQ_ClearInside(&EQ_UI_DYNAMIC_STRENGTH_RECTS[index]);
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
    if ((selected_field & EQ_UI_DYNAMIC_FIELD_LEVEL) != 0U)
    {
        EQ_ClearInside(&EQ_UI_DYNAMIC_LEVEL_RECTS[index]);
        length = EQ_FormatLevel(buffer, EQ_DynamicLevel(index));
        EQ_LcdDrawText(&EQ_UI_DYNAMIC_LEVEL_RECTS[index],
                        buffer, length, EQ_FONT_SMALL, EQ_COLOR_TEXT, 1);
    }
    return selected_field;
}

static unsigned int EQ_DrawJob(int job)
{
    if ((job >= EQ_UI_JOB_PRESET_0) && (job <= EQ_UI_JOB_PRESET_4))
    {
        EQ_DrawPresetJob(job - EQ_UI_JOB_PRESET_0);
    }
    else if ((job >= EQ_UI_JOB_CHAIN_0) && (job <= EQ_UI_JOB_CHAIN_2))
    {
        EQ_DrawChainJob(job - EQ_UI_JOB_CHAIN_0);
    }
    else if ((job >= EQ_UI_JOB_ANALYZER_0) &&
             (job <= EQ_UI_JOB_ANALYZER_3))
    {
        EQ_DrawAnalyzerJob(job - EQ_UI_JOB_ANALYZER_0);
    }
    else if ((job >= EQ_UI_JOB_DYNAMIC_0) &&
             (job <= EQ_UI_JOB_DYNAMIC_2))
    {
        return EQ_DrawDynamicJob(job - EQ_UI_JOB_DYNAMIC_0);
    }
    return 0U;
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
    if ((job >= EQ_UI_JOB_PRESET_0) && (job <= EQ_UI_JOB_PRESET_4))
        return EQ_LCD_CATEGORY_PRESET;
    if ((job >= EQ_UI_JOB_DYNAMIC_0) && (job <= EQ_UI_JOB_DYNAMIC_2))
        return EQ_LCD_CATEGORY_DYNAMIC;
    if ((job >= EQ_UI_JOB_CHAIN_0) && (job <= EQ_UI_JOB_CHAIN_2))
        return EQ_LCD_CATEGORY_CHAIN;
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
    EQ_DebugLcdLastJobTenthsMs = 0UL;
    EQ_DebugLcdMaxJobTenthsMs = 0UL;
    EQ_DebugLcdLastJob = EQ_LCD_JOB_NONE;
    EQ_DebugLcdAutoDisabledCount = 0UL;
    EQ_DebugLcdAutoDisableReason = 0UL;
    EQ_DebugLcdExpectedFrameBase = EQ_ExpectedFrameBase();
    EQ_DebugLcdExpectedFrameEnd = EQ_ExpectedFrameEnd();
    EQ_DebugLcdBufferAddress = EQ_BufferAddress();
    EQ_DebugLcdBufferEndAddress =
        EQ_DebugLcdBufferAddress + EQ_LCD_BUFFER_TOTAL_BYTES - 1UL;
    EQ_DebugLcdCurrentFrameBase = 0UL;
    EQ_DebugLcdCurrentFrameEnd = 0UL;
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
    EQ_DebugLcdHwSnapshot.raster_control = 0UL;
    EQ_DebugLcdHwSnapshot.raster_status = 0UL;
    EQ_DebugLcdHwSnapshot.dma_control = 0UL;
    EQ_DebugLcdHwSnapshot.irq_status = 0UL;
    s_lcd_last_audit_job = 0UL;
    s_lcd_last_audit_frame = 0UL;
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
#if defined(EQ_ALGO_ONLY)
    s_mock_primitive_count = 0UL;
    s_mock_cycle_clock = 0UL;
    s_mock_forced_job_cycles = 0UL;
    memset(&s_mock_hw_snapshot, 0, sizeof(s_mock_hw_snapshot));
    s_mock_hw_snapshot.frame_base = EQ_DebugLcdExpectedFrameBase;
    s_mock_hw_snapshot.frame_end = EQ_DebugLcdExpectedFrameEnd;
    s_mock_hw_snapshot.raster_control = EQ_LCD_RASTER_ENABLE_MASK;
    s_mock_canary_failed = 0;
#else
    Lcd_Init();
#endif
    EQ_CaptureFramebufferCanary();
}

int EqualizerDisplay_DrawStaticLayout(void)
{
#if !EQ_LCD_DIAGNOSTIC_ALIGNMENT_PATTERN
    int index;
    EQ_UI_RECT title_rect;
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
    EQ_LcdFillRect(0, 0, EQ_UI_SCREEN_WIDTH, EQ_UI_SCREEN_HEIGHT,
                   EQ_COLOR_BG);
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
    for (index = 0; index < EQ_UI_PRESET_COUNT; index++)
    {
        EQ_DrawPresetStatic(index);
    }
    EQ_DrawChainStatic();
    EQ_DrawAnalyzerStatic();
    EQ_DrawDynamicStatic();
#endif
    s_layout_drawn = 1;
    EQ_DebugLcdStaticDrawCount++;
    EQ_DebugLcdRefreshCount++;
    EQ_EndDraw();
#if EQ_LCD_DIAGNOSTIC_ALIGNMENT_PATTERN
    EQ_ClearAlignmentStartupFaultStatus();
#endif
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
    EqualizerUiLogic_Request(&s_ui_state, snapshot,
                             EQ_DebugLcdRuntimeMask, process_frame);
    EQ_DebugLcdPendingMask = s_ui_state.dirty_mask;
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

    job = EqualizerUiLogic_SelectJob(&s_ui_state);
    if (job == EQ_LCD_JOB_NONE)
    {
        return EQ_LCD_JOB_NONE;
    }
    if (EQ_BeginDraw() == 0)
    {
        return EQ_LCD_JOB_NONE;
    }
    start_cycles = EQ_ReadCycles();
    completed_fields = EQ_DrawJob(job);
    end_cycles = EQ_ReadCycles();
    EQ_EndDraw();
    elapsed_cycles = end_cycles - start_cycles;
    if ((job >= EQ_UI_JOB_DYNAMIC_0) &&
        (job <= EQ_UI_JOB_DYNAMIC_2))
    {
        EqualizerUiLogic_CompleteDynamicField(
            &s_ui_state, job, completed_fields, process_frame);
    }
    else
    {
        EqualizerUiLogic_CompleteJob(&s_ui_state, job, process_frame);
    }
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
        EqualizerDisplay_AuditHardware(process_frame, 0);
    }
    return job;
}

void EqualizerDisplay_CancelRuntimeJobs(void)
{
    EqualizerUiLogic_Cancel(&s_ui_state);
    EQ_DebugLcdPendingMask = 0UL;
}

void EqualizerDisplay_AutoDisable(unsigned long reason)
{
    EQ_DebugLcdAutoDisableReason |= reason;
    if (EQ_DebugLcdRuntimeMask != 0U)
    {
        EQ_DebugLcdRuntimeMask = 0U;
        EQ_DebugLcdAutoDisabledCount++;
    }
    EqualizerDisplay_CancelRuntimeJobs();
}

#if defined(EQ_ALGO_ONLY)
unsigned long EqualizerDisplay_TestPrimitiveCount(void)
{
    return s_mock_primitive_count;
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

void EqualizerDisplay_TestDrawRect(int x, int y, int w, int h)
{
    EQ_LcdFillRect(x, y, w, h, EQ_COLOR_BG);
}
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

int EqualizerDisplay_HasPendingJob(void)
{
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
