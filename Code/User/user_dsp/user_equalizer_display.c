/**
 * user_equalizer_display.c
 *
 * Fixed-layout LCD bar chart for Project 3.3 graphic equalizer gains.
 */

#include "user_equalizer_display.h"
#include "stdio.h"
#include "string.h"

#ifdef EQUALIZER_DISPLAY_TEST_MAIN
#include "math.h"
#endif

#if (!defined(EQ_ALGO_ONLY)) && (EQ_ENABLE_LCD_DISPLAY != 0)
#include "lcd_api.h"
#endif

#if defined(__TI_COMPILER_VERSION__) || defined(__TMS320C6X__)
#include "c6x.h"
#endif

#define EQ_LCD_W 800
#define EQ_LCD_H 480

#define EQ_TITLE_X 24
#define EQ_TITLE_Y 10
#define EQ_TITLE_W 360
#define EQ_TITLE_H 30

#define EQ_MODE_X 24
#define EQ_MODE_Y 52
#define EQ_MODE_W 360
#define EQ_MODE_H 24

#define EQ_FRAMES_X 420
#define EQ_FRAMES_Y 52
#define EQ_FRAMES_W 170
#define EQ_FRAMES_H 24

#define EQ_LAST_X 600
#define EQ_LAST_Y 52
#define EQ_LAST_W 176
#define EQ_LAST_H 24

#define EQ_MAX_X 420
#define EQ_MAX_Y 82
#define EQ_MAX_W 170
#define EQ_MAX_H 24

#define EQ_CLIP_X 600
#define EQ_CLIP_Y 82
#define EQ_CLIP_W 176
#define EQ_CLIP_H 24

#define EQ_MODE_VALUE_X 76
#define EQ_MODE_VALUE_W (EQ_MODE_X + EQ_MODE_W - EQ_MODE_VALUE_X)
#define EQ_FRAMES_VALUE_X 472
#define EQ_FRAMES_VALUE_W (EQ_FRAMES_X + EQ_FRAMES_W - EQ_FRAMES_VALUE_X)
#define EQ_LAST_VALUE_X 652
#define EQ_LAST_VALUE_W (EQ_LAST_X + EQ_LAST_W - EQ_LAST_VALUE_X)
#define EQ_MAX_VALUE_X 504
#define EQ_MAX_VALUE_W (EQ_MAX_X + EQ_MAX_W - EQ_MAX_VALUE_X)
#define EQ_CLIP_VALUE_X 652
#define EQ_CLIP_VALUE_W (EQ_CLIP_X + EQ_CLIP_W - EQ_CLIP_VALUE_X)

#define EQ_BAR_AREA_X 80
#define EQ_BAR_AREA_Y 130
#define EQ_BAR_AREA_W 640
#define EQ_BAR_AREA_H 230
#define EQ_BAR_ZERO_Y 222
#define EQ_BAR_PITCH 64
#define EQ_BAR_WIDTH 30
#define EQ_BAR_SLOT_W 52
#define EQ_BAR_SLOT_H EQ_BAR_AREA_H

#define EQ_LABEL_Y 382
#define EQ_LABEL_W 52
#define EQ_LABEL_H 18
#define EQ_DB_LABEL_X 24
#define EQ_DB_LABEL_W 44
#define EQ_DB_LABEL_H 18

#define EQ_FONT_SMALL 0
#define EQ_FONT_MEDIUM 1
#define EQ_FONT_LARGE 2

#define EQ_COLOR_BG 0
#define EQ_COLOR_TEXT 1
#define EQ_COLOR_MUTED 2
#define EQ_COLOR_AXIS 3
#define EQ_COLOR_BAR_POS 4
#define EQ_COLOR_BAR_NEG 5

volatile unsigned long EQ_DebugLcdRefreshCount = 0UL;
volatile unsigned long EQ_DebugLcdSkipBusyCount = 0UL;
volatile unsigned long EQ_DebugLcdGainRedrawCount = 0UL;
volatile unsigned long EQ_DebugLcdStatusRedrawCount = 0UL;
volatile int EQ_DebugLcdEnabled = 0;
volatile int EQ_DebugLcdLastMode = EQ_PRESET_FLAT;
volatile unsigned int EQ_DebugLcdRuntimeMask = 0U;
volatile unsigned long EQ_DebugLcdPendingMask = 0UL;
volatile unsigned long EQ_DebugLcdJobCount = 0UL;
volatile unsigned long EQ_DebugLcdDeferredAudioCount = 0UL;
volatile unsigned long EQ_DebugLcdAudioArrivedDuringDrawCount = 0UL;
volatile unsigned long EQ_DebugLcdUnexpectedFullRedrawCount = 0UL;
volatile unsigned long EQ_DebugLcdBudgetExceededCount = 0UL;
volatile unsigned long EQ_DebugLcdLastJobCycles = 0UL;
volatile unsigned long EQ_DebugLcdMaxJobCycles = 0UL;
volatile unsigned long EQ_DebugLcdLastJobTenthsMs = 0UL;
volatile unsigned long EQ_DebugLcdMaxJobTenthsMs = 0UL;
volatile float EQ_DebugLcdLastJobMs = 0.0f;
volatile float EQ_DebugLcdMaxJobMs = 0.0f;
volatile int EQ_DebugLcdLastJob = EQ_LCD_JOB_NONE;
volatile unsigned long EQ_DebugLcdAutoDisabledCount = 0UL;
volatile unsigned long EQ_DebugLcdAutoDisableReason = 0UL;
volatile unsigned long EQ_DebugLcdCategoryCount[EQ_LCD_CATEGORY_COUNT] =
{
    0UL, 0UL, 0UL, 0UL, 0UL, 0UL
};
volatile unsigned long EQ_DebugLcdCategoryLastCycles[EQ_LCD_CATEGORY_COUNT] =
{
    0UL, 0UL, 0UL, 0UL, 0UL, 0UL
};
volatile unsigned long EQ_DebugLcdCategoryMaxCycles[EQ_LCD_CATEGORY_COUNT] =
{
    0UL, 0UL, 0UL, 0UL, 0UL, 0UL
};

static volatile int s_lcd_busy = 0;
static int s_layout_drawn = 0;
static int s_runtime_started = 0;
static unsigned long s_dirty_mask = 0UL;
static unsigned long s_forced_dirty_mask = 0UL;
static int s_job_cursor = 0;
static unsigned int s_applied_runtime_mask = 0U;
static int s_have_requested_gains = 0;
static int s_have_requested_status = 0;
static float s_requested_gains_db[EQ_NUM_BANDS];
static float s_displayed_gains_db[EQ_NUM_BANDS];
static unsigned char s_gain_displayed_valid[EQ_NUM_BANDS];
static unsigned long s_requested_frames = 0UL;
static unsigned long s_displayed_frames = 0UL;
static unsigned long s_requested_algo_last_tenths = 0UL;
static unsigned long s_displayed_algo_last_tenths = 0UL;
static unsigned long s_requested_algo_max_tenths = 0UL;
static unsigned long s_displayed_algo_max_tenths = 0UL;
static unsigned long s_requested_clip_count = 0UL;
static unsigned long s_displayed_clip_count = 0UL;
static int s_requested_transition_target_mode = EQ_PRESET_NONE;
static int s_requested_applied_mode = EQ_PRESET_FLAT;
static int s_displayed_transition_target_mode = EQ_PRESET_NONE;
static int s_status_displayed_valid[5] = { 0, 0, 0, 0, 0 };

#define EQ_LCD_STATUS_DIRTY_MASK 0x001FUL
#define EQ_LCD_GAINS_DIRTY_MASK  0x7FE0UL

static const char * const s_band_labels[EQ_NUM_BANDS] =
{
    "31", "63", "125", "250", "500",
    "1k", "2k", "4k", "8k", "16k"
};

#if EQ_LCD_USE_CHINESE
typedef enum
{
    CN_XIANG = 0,
    CN_MU,
    CN_TU,
    CN_SHI,
    CN_JUN,
    CN_HENG,
    CN_QI,
    CN_MO,
    CN_SHI_MODE,
    CN_PING,
    CN_ZHI,
    CN_DI,
    CN_YIN,
    CN_ZENG,
    CN_QIANG,
    CN_REN,
    CN_SHENG,
    CN_GAO,
    CN_ZHEN,
    CN_SHU,
    CN_HAO,
    CN_SHI_TIME,
    CN_ZUI,
    CN_DA,
    CN_XIAO,
    CN_DING,
    CN_COUNT
} EQ_CN_GLYPH;

#define EQ_CN_GLYPH_SIZE 16
#define EQ_CN_GLYPH_ADVANCE 17

static const unsigned short s_cn_glyph_bits[CN_COUNT][EQ_CN_GLYPH_SIZE] =
{
    { /* CN_XIANG U+9879 */
        0x03FEU, 0x7C20U, 0x1020U, 0x11FCU,
        0x1104U, 0x1124U, 0x1124U, 0x1124U,
        0x1D24U, 0x6164U, 0x0058U, 0x018CU,
        0x0706U, 0x0000U, 0x0000U, 0x0000U
    },
    { /* CN_MU U+76EE */
        0x1FF8U, 0x1008U, 0x1008U, 0x1FF8U,
        0x1FF8U, 0x1008U, 0x1008U, 0x1FF8U,
        0x1008U, 0x1008U, 0x1008U, 0x1FF8U,
        0x1008U, 0x0000U, 0x0000U, 0x0000U
    },
    { /* CN_TU U+56FE */
        0x3FFCU, 0x2304U, 0x23E4U, 0x2664U,
        0x2F44U, 0x2184U, 0x2674U, 0x2994U,
        0x20C4U, 0x2304U, 0x20C4U, 0x2004U,
        0x3FFCU, 0x2006U, 0x0000U, 0x0000U
    },
    { /* CN_SHI U+793A */
        0x0000U, 0x1FF8U, 0x0000U, 0x0000U,
        0x0000U, 0x7FFEU, 0x0082U, 0x0080U,
        0x0C90U, 0x1898U, 0x1088U, 0x3084U,
        0x6084U, 0x0380U, 0x0200U, 0x0000U
    },
    { /* CN_JUN U+5747 */
        0x1040U, 0x10C0U, 0x1080U, 0x10FEU,
        0x7D86U, 0x1146U, 0x1366U, 0x1026U,
        0x1016U, 0x1036U, 0x1CC4U, 0x7184U,
        0x0004U, 0x001CU, 0x0000U, 0x0000U
    },
    { /* CN_HENG U+8861 */
        0x0100U, 0x1200U, 0x35CEU, 0x6080U,
        0x17E0U, 0x14BEU, 0x37E4U, 0x74A4U,
        0x77E4U, 0x3104U, 0x3FF4U, 0x3184U,
        0x3664U, 0x3C2CU, 0x3008U, 0x0000U
    },
    { /* CN_QI U+5668 */
        0x3EFCU, 0x22C4U, 0x22C4U, 0x3EFCU,
        0x0190U, 0x7FFEU, 0x0380U, 0x0660U,
        0x781EU, 0x3E7CU, 0x2244U, 0x2244U,
        0x3E7CU, 0x2244U, 0x0000U, 0x0000U
    },
    { /* CN_MO U+6A21 */
        0x10D8U, 0x13FEU, 0x10D8U, 0x7C00U,
        0x11FCU, 0x1104U, 0x39FCU, 0x3504U,
        0x75FCU, 0x5020U, 0x13FEU, 0x1050U,
        0x10C8U, 0x1707U, 0x0002U, 0x0000U
    },
    { /* CN_SHI_MODE U+5F0F */
        0x00C0U, 0x00D8U, 0x00C8U, 0x7FFEU,
        0x00C0U, 0x0040U, 0x0040U, 0x3F40U,
        0x0C60U, 0x0C20U, 0x0C20U, 0x0FB2U,
        0x7C1AU, 0x200EU, 0x0000U, 0x0000U
    },
    { /* CN_PING U+5E73 */
        0x3FFCU, 0x0180U, 0x1188U, 0x1998U,
        0x0D90U, 0x05A0U, 0x0180U, 0x7FFEU,
        0x0180U, 0x0180U, 0x0180U, 0x0180U,
        0x0180U, 0x0180U, 0x0000U, 0x0000U
    },
    { /* CN_ZHI U+76F4 */
        0x0000U, 0x0180U, 0x0180U, 0x7FFCU,
        0x0100U, 0x1FF8U, 0x1818U, 0x1FF8U,
        0x1818U, 0x1FF8U, 0x1818U, 0x1818U,
        0x1FF8U, 0x1818U, 0x7FFEU, 0x0000U
    },
    { /* CN_DI U+4F4E */
        0x0800U, 0x18FCU, 0x13A0U, 0x1220U,
        0x3220U, 0x3220U, 0x53FEU, 0x5220U,
        0x1220U, 0x1230U, 0x1290U, 0x1392U,
        0x138AU, 0x124EU, 0x1000U, 0x0000U
    },
    { /* CN_YIN U+97F3 */
        0x0100U, 0x0180U, 0x3FFCU, 0x0C30U,
        0x0420U, 0x0460U, 0x7FFEU, 0x0000U,
        0x1FF8U, 0x1818U, 0x1FF8U, 0x1818U,
        0x1818U, 0x1FF8U, 0x1818U, 0x0000U
    },
    { /* CN_ZENG U+589E */
        0x018CU, 0x1088U, 0x13FEU, 0x1222U,
        0x12AAU, 0x7EAAU, 0x1222U, 0x13FEU,
        0x1000U, 0x11FCU, 0x1D04U, 0x71FCU,
        0x0104U, 0x01FCU, 0x0104U, 0x0000U
    },
    { /* CN_QIANG U+5F3A */
        0x01FCU, 0x7D04U, 0x0D04U, 0x0DFCU,
        0x7C20U, 0x6020U, 0x61FCU, 0x7924U,
        0x0924U, 0x09FCU, 0x0920U, 0x0824U,
        0x083EU, 0x3BC2U, 0x0000U, 0x0000U
    },
    { /* CN_REN U+4EBA */
        0x0180U, 0x0180U, 0x0180U, 0x0180U,
        0x0180U, 0x0180U, 0x0180U, 0x03C0U,
        0x0240U, 0x0660U, 0x0430U, 0x0818U,
        0x300CU, 0x6006U, 0x0000U, 0x0000U
    },
    { /* CN_SHENG U+58F0 */
        0x0080U, 0x3FFEU, 0x0080U, 0x0080U,
        0x1FFCU, 0x0000U, 0x1FFCU, 0x108CU,
        0x108CU, 0x1FFCU, 0x100CU, 0x3000U,
        0x2000U, 0x6000U, 0x0000U, 0x0000U
    },
    { /* CN_GAO U+9AD8 */
        0x0100U, 0x0180U, 0x7FFEU, 0x1FF0U,
        0x0810U, 0x0810U, 0x1FF0U, 0x7FFEU,
        0x2006U, 0x27E6U, 0x2426U, 0x2426U,
        0x27E6U, 0x200EU, 0x0008U, 0x0000U
    },
    { /* CN_ZHEN U+5E27 */
        0x1020U, 0x103EU, 0x1020U, 0x7E20U,
        0x57FCU, 0x5704U, 0x5724U, 0x5724U,
        0x5724U, 0x5724U, 0x5524U, 0x5078U,
        0x10CCU, 0x1382U, 0x0000U, 0x0000U
    },
    { /* CN_SHU U+6570 */
        0x0C20U, 0x2D20U, 0x0F60U, 0x7FFEU,
        0x1C44U, 0x3E44U, 0x6DCCU, 0x08C8U,
        0x7F28U, 0x1128U, 0x3238U, 0x0E10U,
        0x0D68U, 0x30C6U, 0x4000U, 0x0000U
    },
    { /* CN_HAO U+8017 */
        0x0800U, 0x081CU, 0x7EF0U, 0x0820U,
        0x7E20U, 0x08FCU, 0x0820U, 0x7E20U,
        0x18FEU, 0x1FA0U, 0x2A20U, 0x6820U,
        0x4822U, 0x083EU, 0x0000U, 0x0000U
    },
    { /* CN_SHI_TIME U+65F6 */
        0x0008U, 0x0008U, 0x7C08U, 0x65FEU,
        0x65FEU, 0x6408U, 0x7C88U, 0x64C8U,
        0x6448U, 0x6408U, 0x7C08U, 0x6408U,
        0x6008U, 0x0038U, 0x0000U, 0x0000U
    },
    { /* CN_ZUI U+6700 */
        0x1FF8U, 0x1FF8U, 0x1018U, 0x1FF8U,
        0x0000U, 0x7FFEU, 0x1100U, 0x1FF8U,
        0x1148U, 0x1F48U, 0x11B0U, 0x7F38U,
        0x014EU, 0x0000U, 0x0000U, 0x0000U
    },
    { /* CN_DA U+5927 */
        0x0180U, 0x0180U, 0x0180U, 0x0180U,
        0x7FFEU, 0x7FFEU, 0x0180U, 0x0180U,
        0x03C0U, 0x0260U, 0x0420U, 0x0C10U,
        0x381CU, 0x6006U, 0x0000U, 0x0000U
    },
    { /* CN_XIAO U+524A */
        0x0C04U, 0x4C84U, 0x2D24U, 0x2D24U,
        0x0C24U, 0x3F24U, 0x2124U, 0x3F24U,
        0x2124U, 0x2124U, 0x3F24U, 0x2104U,
        0x2104U, 0x230CU, 0x0000U, 0x0000U
    },
    { /* CN_DING U+9876 */
        0x7FFEU, 0x1060U, 0x1040U, 0x11FCU,
        0x1104U, 0x1124U, 0x1124U, 0x1164U,
        0x1164U, 0x1144U, 0x1050U, 0x718CU,
        0x4706U, 0x0000U, 0x0000U, 0x0000U
    }
};

static const unsigned char s_cn_title_left[] =
{
    CN_XIANG, CN_MU
};
static const unsigned char s_cn_title_right[] =
{
    CN_TU, CN_SHI, CN_JUN, CN_HENG, CN_QI
};
static const unsigned char s_cn_mode_label[] =
{
    CN_MO, CN_SHI_MODE
};
static const unsigned char s_cn_label_frames[] =
{
    CN_ZHEN, CN_SHU
};
static const unsigned char s_cn_label_last[] =
{
    CN_HAO, CN_SHI_TIME
};
static const unsigned char s_cn_label_max[] =
{
    CN_ZUI, CN_DA, CN_HAO, CN_SHI_TIME
};
static const unsigned char s_cn_label_clip[] =
{
    CN_XIAO, CN_DING
};
#endif

#if defined(EQ_ALGO_ONLY)
static unsigned long s_mock_clear_count = 0UL;
static unsigned long s_mock_fill_count = 0UL;
static unsigned long s_mock_line_count = 0UL;
static unsigned long s_mock_text_count = 0UL;
#if EQ_LCD_USE_CHINESE
static unsigned long s_mock_cn_glyph_count = 0UL;
#endif
static unsigned long s_mock_text_clear_count = 0UL;
static unsigned long s_mock_bar_slot_clear_count = 0UL;
static unsigned long s_mock_bounds_failures = 0UL;
static unsigned long s_mock_chart_border_segment_count = 0UL;
static char s_mock_last_text[48];
static int s_mock_last_bar_bottom_y = -1;
static unsigned long s_mock_cycle_clock = 0UL;
static unsigned long s_mock_forced_job_cycles = 0UL;
#elif EQ_ENABLE_LCD_DISPLAY == 0
static unsigned long s_mock_clear_count = 0UL;
static unsigned long s_mock_fill_count = 0UL;
static unsigned long s_mock_line_count = 0UL;
static unsigned long s_mock_text_count = 0UL;
#endif

static float EQ_DisplayAbs(float x)
{
    if (x < 0.0f)
    {
        return -x;
    }
    return x;
}

static int EQ_ClampInt(int v, int lo, int hi)
{
    if (v < lo)
    {
        return lo;
    }
    if (v > hi)
    {
        return hi;
    }
    return v;
}

static float EQ_ClampDbForDisplay(float db)
{
    if (db < EQ_GAIN_MIN_DB)
    {
        return EQ_GAIN_MIN_DB;
    }
    if (db > EQ_GAIN_MAX_DB)
    {
        return EQ_GAIN_MAX_DB;
    }
    return db;
}

static int EQ_DbToY(float db)
{
    float span;
    float offset;
    int y;

    db = EQ_ClampDbForDisplay(db);
    span = EQ_GAIN_MAX_DB - EQ_GAIN_MIN_DB;
    offset = (EQ_GAIN_MAX_DB - db) * (float)EQ_BAR_AREA_H / span;
    y = EQ_BAR_AREA_Y + (int)(offset + 0.5f);
    return EQ_ClampInt(y, EQ_BAR_AREA_Y,
                       EQ_BAR_AREA_Y + EQ_BAR_AREA_H - 1);
}

static const char *EQ_ModeName(int mode)
{
    switch (mode)
    {
        case EQ_PRESET_FLAT:
            return "FLAT";
        case EQ_PRESET_BASS_BOOST:
            return "BASS";
        case EQ_PRESET_VOCAL:
            return "VOCAL";
        case EQ_PRESET_TREBLE_BOOST:
            return "TREBLE";
        case EQ_PRESET_V_SHAPE:
            return "V-SHAPE";
        default:
            return "FLAT";
    }
}

static int EQ_BeginDraw(void)
{
#if EQ_ENABLE_LCD_DISPLAY == 0
    EQ_DebugLcdEnabled = 0;
    return 0;
#else
    if (s_lcd_busy != 0)
    {
        EQ_DebugLcdSkipBusyCount++;
        return 0;
    }
    s_lcd_busy = 1;
    return 1;
#endif
}

static void EQ_EndDraw(void)
{
#if EQ_ENABLE_LCD_DISPLAY != 0
    s_lcd_busy = 0;
#endif
}

static void EQ_ClipRect(int *x, int *y, int *w, int *h)
{
    int x2;
    int y2;

    if (*w <= 0)
    {
        *w = 0;
        return;
    }
    if (*h <= 0)
    {
        *h = 0;
        return;
    }

    x2 = *x + *w - 1;
    y2 = *y + *h - 1;
    if (*x < 0)
    {
        *x = 0;
    }
    if (*y < 0)
    {
        *y = 0;
    }
    if (x2 >= EQ_LCD_W)
    {
        x2 = EQ_LCD_W - 1;
    }
    if (y2 >= EQ_LCD_H)
    {
        y2 = EQ_LCD_H - 1;
    }
    if ((x2 < *x) || (y2 < *y))
    {
        *w = 0;
        *h = 0;
        return;
    }
    *w = x2 - *x + 1;
    *h = y2 - *y + 1;
}

#if (!defined(EQ_ALGO_ONLY)) && (EQ_ENABLE_LCD_DISPLAY != 0)
static unsigned int EQ_MapColor(int color)
{
    switch (color)
    {
        case EQ_COLOR_TEXT:
            return ClrWhite;
        case EQ_COLOR_MUTED:
            return ClrLightSteelBlue;
        case EQ_COLOR_AXIS:
            return ClrGray;
        case EQ_COLOR_BAR_POS:
            return ClrLimeGreen;
        case EQ_COLOR_BAR_NEG:
            return ClrOrangeRed;
        case EQ_COLOR_BG:
        default:
            return ClrBlack;
    }
}

static void EQ_SetFont(int font)
{
    switch (font)
    {
        case EQ_FONT_LARGE:
            GrContextFontSet(&Lcd_Context, &g_sFontCm24);
            break;
        case EQ_FONT_MEDIUM:
            GrContextFontSet(&Lcd_Context, &g_sFontCm18);
            break;
        case EQ_FONT_SMALL:
        default:
            GrContextFontSet(&Lcd_Context, &g_sFontCm12);
            break;
    }
}
#else
static void EQ_SetFont(int font)
{
    (void)font;
}
#endif

static void EQ_MockCheckBounds(int x, int y, int w, int h)
{
#ifdef EQ_ALGO_ONLY
    if ((w <= 0) || (h <= 0) ||
        (x < 0) || (y < 0) ||
        ((x + w) > EQ_LCD_W) || ((y + h) > EQ_LCD_H))
    {
        s_mock_bounds_failures++;
    }
#else
    (void)x;
    (void)y;
    (void)w;
    (void)h;
#endif
}

static void EQ_LcdClearRect(int x, int y, int w, int h)
{
    EQ_ClipRect(&x, &y, &w, &h);
    if ((w <= 0) || (h <= 0))
    {
        return;
    }
    EQ_MockCheckBounds(x, y, w, h);
#if (!defined(EQ_ALGO_ONLY)) && (EQ_ENABLE_LCD_DISPLAY != 0)
    Lcd_Rectangle.sXMin = x;
    Lcd_Rectangle.sYMin = y;
    Lcd_Rectangle.sXMax = x + w - 1;
    Lcd_Rectangle.sYMax = y + h - 1;
    GrContextForegroundSet(&Lcd_Context, EQ_MapColor(EQ_COLOR_BG));
    GrRectFill(&Lcd_Context, &Lcd_Rectangle);
#else
    s_mock_clear_count++;
#endif
}

static void EQ_LcdFillRect(int x, int y, int w, int h, int color)
{
    EQ_ClipRect(&x, &y, &w, &h);
    if ((w <= 0) || (h <= 0))
    {
        return;
    }
    EQ_MockCheckBounds(x, y, w, h);
#if (!defined(EQ_ALGO_ONLY)) && (EQ_ENABLE_LCD_DISPLAY != 0)
    Lcd_Rectangle.sXMin = x;
    Lcd_Rectangle.sYMin = y;
    Lcd_Rectangle.sXMax = x + w - 1;
    Lcd_Rectangle.sYMax = y + h - 1;
    GrContextForegroundSet(&Lcd_Context, EQ_MapColor(color));
    GrRectFill(&Lcd_Context, &Lcd_Rectangle);
#else
    (void)color;
    s_mock_fill_count++;
#endif
}

static void EQ_LcdDrawRect(int x, int y, int w, int h, int color)
{
    EQ_ClipRect(&x, &y, &w, &h);
    if ((w <= 0) || (h <= 0))
    {
        return;
    }
    EQ_MockCheckBounds(x, y, w, h);
#if (!defined(EQ_ALGO_ONLY)) && (EQ_ENABLE_LCD_DISPLAY != 0)
    Lcd_Rectangle.sXMin = x;
    Lcd_Rectangle.sYMin = y;
    Lcd_Rectangle.sXMax = x + w - 1;
    Lcd_Rectangle.sYMax = y + h - 1;
    GrContextForegroundSet(&Lcd_Context, EQ_MapColor(color));
    GrRectDraw(&Lcd_Context, &Lcd_Rectangle);
#else
    (void)color;
    s_mock_line_count += 4UL;
#endif
}

static void EQ_LcdDrawLine(int x1, int y1, int x2, int y2, int color)
{
    x1 = EQ_ClampInt(x1, 0, EQ_LCD_W - 1);
    x2 = EQ_ClampInt(x2, 0, EQ_LCD_W - 1);
    y1 = EQ_ClampInt(y1, 0, EQ_LCD_H - 1);
    y2 = EQ_ClampInt(y2, 0, EQ_LCD_H - 1);
#if (!defined(EQ_ALGO_ONLY)) && (EQ_ENABLE_LCD_DISPLAY != 0)
    GrContextForegroundSet(&Lcd_Context, EQ_MapColor(color));
    GrLineDraw(&Lcd_Context, x1, y1, x2, y2);
#else
    (void)color;
    s_mock_line_count++;
#ifdef EQ_ALGO_ONLY
    if (((y1 == EQ_BAR_AREA_Y) ||
         (y1 == EQ_BAR_AREA_Y + EQ_BAR_AREA_H - 1)) &&
        (y1 == y2))
    {
        s_mock_chart_border_segment_count++;
    }
    if ((x1 == EQ_BAR_AREA_X) && (x2 == EQ_BAR_AREA_X) &&
        (y1 == EQ_BAR_AREA_Y) &&
        (y2 == EQ_BAR_AREA_Y + EQ_BAR_AREA_H - 1))
    {
        s_mock_chart_border_segment_count++;
    }
#endif
#endif
}

static void EQ_LcdDrawText(int x, int y, const char *text,
                           int font, int color, int centered, int region_w)
{
    int draw_x;
    int draw_y;

    draw_x = EQ_ClampInt(x, 0, EQ_LCD_W - 1);
    draw_y = EQ_ClampInt(y, 0, EQ_LCD_H - 1);
    EQ_SetFont(font);
#if (!defined(EQ_ALGO_ONLY)) && (EQ_ENABLE_LCD_DISPLAY != 0)
    GrContextForegroundSet(&Lcd_Context, EQ_MapColor(color));
    if (centered != 0)
    {
        GrStringDrawCentered(&Lcd_Context, text, -1,
                             draw_x + (region_w / 2), draw_y, 0);
    }
    else
    {
        GrStringDraw(&Lcd_Context, text, -1, draw_x, draw_y, 0);
    }
#else
    (void)draw_x;
    (void)draw_y;
    (void)text;
    (void)color;
    (void)centered;
    (void)region_w;
    s_mock_text_count++;
#ifdef EQ_ALGO_ONLY
    strncpy(s_mock_last_text, text, sizeof(s_mock_last_text) - 1U);
    s_mock_last_text[sizeof(s_mock_last_text) - 1U] = '\0';
#endif
#endif
}

static void EQ_DrawTextRegion(int x, int y, int w, int h, const char *text,
                              int font, int color, int centered)
{
    EQ_LcdClearRect(x, y, w, h);
#ifdef EQ_ALGO_ONLY
    s_mock_text_clear_count++;
#endif
    if (centered != 0)
    {
        EQ_LcdDrawText(x, y + 3, text, font, color, 1, w);
    }
    else
    {
        EQ_LcdDrawText(x + 2, y + 3, text, font, color, 0, w);
    }
}

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

        bits = s_cn_glyph_bits[glyph][row];
        col = 0;
        while (col < EQ_CN_GLYPH_SIZE)
        {
            if ((bits & (unsigned short)(0x8000U >> col)) != 0U)
            {
                int start;

                start = col;
                while ((col < EQ_CN_GLYPH_SIZE) &&
                       ((bits & (unsigned short)(0x8000U >> col)) != 0U))
                {
                    col++;
                }
                EQ_LcdFillRect(x + start, y + row, col - start, 1, color);
            }
            else
            {
                col++;
            }
        }
    }

#ifdef EQ_ALGO_ONLY
    s_mock_cn_glyph_count++;
#endif
    return x + EQ_CN_GLYPH_ADVANCE;
}

static int EQ_DrawCnSeq(int x, int y,
                        const unsigned char *seq, int count, int color)
{
    int i;

    for (i = 0; i < count; i++)
    {
        x = EQ_DrawCnGlyph(x, y, seq[i], color);
    }
    return x;
}

static void EQ_DrawCnTitleRegion(void)
{
    int x;
    int y;

    EQ_LcdClearRect(EQ_TITLE_X, EQ_TITLE_Y, EQ_TITLE_W, EQ_TITLE_H);
#ifdef EQ_ALGO_ONLY
    s_mock_text_clear_count++;
#endif
    x = EQ_TITLE_X + 2;
    y = EQ_TITLE_Y + 7;
    x = EQ_DrawCnSeq(x, y, s_cn_title_left,
                     (int)sizeof(s_cn_title_left), EQ_COLOR_TEXT);
    EQ_LcdDrawText(x + 2, EQ_TITLE_Y + 6, "3.3",
                   EQ_FONT_MEDIUM, EQ_COLOR_TEXT, 0, 36);
    x += 38;
    (void)EQ_DrawCnSeq(x, y, s_cn_title_right,
                       (int)sizeof(s_cn_title_right), EQ_COLOR_TEXT);
}

static void EQ_DrawCnStaticLabel(int x, int y,
                                 const unsigned char *label,
                                 int label_count)
{
    int label_x;

    label_x = EQ_DrawCnSeq(x + 2, y + 4, label, label_count,
                           EQ_COLOR_TEXT);
    EQ_LcdDrawText(label_x + 4, y + 3, ":", EQ_FONT_MEDIUM,
                   EQ_COLOR_TEXT, 0, 12);
}

static void EQ_DrawCnStaticStatusLabels(void)
{
    EQ_DrawCnStaticLabel(EQ_MODE_X, EQ_MODE_Y, s_cn_mode_label,
                         (int)sizeof(s_cn_mode_label));
    EQ_DrawCnStaticLabel(EQ_FRAMES_X, EQ_FRAMES_Y, s_cn_label_frames,
                         (int)sizeof(s_cn_label_frames));
    EQ_DrawCnStaticLabel(EQ_LAST_X, EQ_LAST_Y, s_cn_label_last,
                         (int)sizeof(s_cn_label_last));
    EQ_DrawCnStaticLabel(EQ_MAX_X, EQ_MAX_Y, s_cn_label_max,
                         (int)sizeof(s_cn_label_max));
    EQ_DrawCnStaticLabel(EQ_CLIP_X, EQ_CLIP_Y, s_cn_label_clip,
                         (int)sizeof(s_cn_label_clip));
}
#endif

static void EQ_DrawAxes(void)
{
    EQ_LcdDrawRect(EQ_BAR_AREA_X, EQ_BAR_AREA_Y,
                   EQ_BAR_AREA_W, EQ_BAR_AREA_H, EQ_COLOR_AXIS);
    EQ_LcdDrawLine(EQ_BAR_AREA_X,
                   EQ_BAR_ZERO_Y,
                   EQ_BAR_AREA_X + EQ_BAR_AREA_W - 1,
                   EQ_BAR_ZERO_Y,
                   EQ_COLOR_AXIS);
}

static void EQ_DrawBandLabel(int band)
{
    int x;

    x = EQ_BAR_AREA_X + band * EQ_BAR_PITCH;
    EQ_DrawTextRegion(x, EQ_LABEL_Y, EQ_LABEL_W, EQ_LABEL_H,
                      s_band_labels[band],
                      EQ_FONT_SMALL, EQ_COLOR_MUTED, 1);
}

static void EQ_DrawBandBarOnly(int band, float gain_db)
{
    int slot_x;
    int bar_x;
    int y;
    int h;
    int color;

    slot_x = EQ_BAR_AREA_X + band * EQ_BAR_PITCH;
    bar_x = slot_x + ((EQ_BAR_SLOT_W - EQ_BAR_WIDTH) / 2);
    y = EQ_DbToY(gain_db);

    if (gain_db >= 0.0f)
    {
        h = EQ_BAR_ZERO_Y - y + 1;
        if (h < 2)
        {
            h = 2;
        }
        color = EQ_COLOR_BAR_POS;
        EQ_LcdFillRect(bar_x, EQ_BAR_ZERO_Y - h + 1, EQ_BAR_WIDTH, h, color);
    }
    else
    {
        h = y - EQ_BAR_ZERO_Y + 1;
        if (h < 2)
        {
            h = 2;
        }
        color = EQ_COLOR_BAR_NEG;
#ifdef EQ_ALGO_ONLY
        s_mock_last_bar_bottom_y = EQ_BAR_ZERO_Y + h - 1;
#endif
        EQ_LcdFillRect(bar_x, EQ_BAR_ZERO_Y, EQ_BAR_WIDTH, h, color);
    }
}

static void EQ_DrawBandJob(int band, float gain_db)
{
    int slot_x;

    slot_x = EQ_BAR_AREA_X + band * EQ_BAR_PITCH;
    EQ_LcdClearRect(slot_x, EQ_BAR_AREA_Y, EQ_BAR_SLOT_W, EQ_BAR_SLOT_H);
#ifdef EQ_ALGO_ONLY
    s_mock_bar_slot_clear_count++;
#endif
    EQ_LcdDrawLine(slot_x, EQ_BAR_AREA_Y,
                   slot_x + EQ_BAR_SLOT_W - 1, EQ_BAR_AREA_Y,
                   EQ_COLOR_AXIS);
    EQ_LcdDrawLine(slot_x, EQ_BAR_AREA_Y + EQ_BAR_AREA_H - 1,
                   slot_x + EQ_BAR_SLOT_W - 1,
                   EQ_BAR_AREA_Y + EQ_BAR_AREA_H - 1,
                   EQ_COLOR_AXIS);
    if (band == 0)
    {
        EQ_LcdDrawLine(EQ_BAR_AREA_X, EQ_BAR_AREA_Y,
                       EQ_BAR_AREA_X,
                       EQ_BAR_AREA_Y + EQ_BAR_AREA_H - 1,
                       EQ_COLOR_AXIS);
    }
    EQ_LcdDrawLine(slot_x, EQ_BAR_ZERO_Y,
                   slot_x + EQ_BAR_SLOT_W - 1, EQ_BAR_ZERO_Y,
                   EQ_COLOR_AXIS);
    EQ_DrawBandBarOnly(band, gain_db);
}

static int EQ_AppendChar(char *buf, int capacity, int length, char value)
{
    if (length < capacity - 1)
    {
        buf[length] = value;
        length++;
        buf[length] = '\0';
    }
    return length;
}

static int EQ_AppendText(char *buf, int capacity, int length,
                         const char *text)
{
    while (*text != '\0')
    {
        length = EQ_AppendChar(buf, capacity, length, *text);
        text++;
    }
    return length;
}

static int EQ_AppendUnsigned(char *buf, int capacity, int length,
                             unsigned long value)
{
    char reversed[11];
    int count;

    count = 0;
    do
    {
        reversed[count] = (char)('0' + (value % 10UL));
        count++;
        value /= 10UL;
    }
    while ((value != 0UL) && (count < (int)sizeof(reversed)));

    while (count > 0)
    {
        count--;
        length = EQ_AppendChar(buf, capacity, length, reversed[count]);
    }
    return length;
}

static void EQ_FormatUnsignedValue(char *buf, int capacity,
                                   unsigned long value)
{
    buf[0] = '\0';
    (void)EQ_AppendUnsigned(buf, capacity, 0, value);
}

static void EQ_FormatTenthsMsValue(char *buf, int capacity,
                                   unsigned long tenths)
{
    int length;

    buf[0] = '\0';
    length = EQ_AppendUnsigned(buf, capacity, 0, tenths / 10UL);
    length = EQ_AppendChar(buf, capacity, length, '.');
    length = EQ_AppendChar(buf, capacity, length,
                           (char)('0' + (tenths % 10UL)));
    (void)EQ_AppendText(buf, capacity, length, " ms");
}

static void EQ_FormatModeValue(char *buf, int capacity,
                               int applied_mode, int transition_target_mode)
{
    int length;

    buf[0] = '\0';
    length = EQ_AppendText(buf, capacity, 0, EQ_ModeName(applied_mode));
    if (transition_target_mode != EQ_PRESET_NONE)
    {
        length = EQ_AppendChar(buf, capacity, length, '>');
        (void)EQ_AppendText(buf, capacity, length,
                            EQ_ModeName(transition_target_mode));
    }
}

void EqualizerDisplay_Init(void)
{
    int band;
    int category;

    s_lcd_busy = 0;
    s_layout_drawn = 0;
    s_runtime_started = 0;
    s_dirty_mask = 0UL;
    s_forced_dirty_mask = 0UL;
    s_job_cursor = 0;
    s_applied_runtime_mask = 0U;
    s_have_requested_gains = 0;
    s_have_requested_status = 0;
    EQ_LcdRuntimeMask = 0U;
    EQ_DebugLcdPendingMask = 0UL;
    EQ_DebugLcdJobCount = 0UL;
    EQ_DebugLcdUnexpectedFullRedrawCount = 0UL;
    EQ_DebugLcdBudgetExceededCount = 0UL;
    EQ_DebugLcdLastJobCycles = 0UL;
    EQ_DebugLcdMaxJobCycles = 0UL;
    EQ_DebugLcdLastJobTenthsMs = 0UL;
    EQ_DebugLcdMaxJobTenthsMs = 0UL;
    EQ_DebugLcdLastJobMs = 0.0f;
    EQ_DebugLcdMaxJobMs = 0.0f;
    EQ_DebugLcdLastJob = EQ_LCD_JOB_NONE;
    EQ_DebugLcdAutoDisabledCount = 0UL;
    EQ_DebugLcdAutoDisableReason = 0UL;
    for (band = 0; band < EQ_NUM_BANDS; band++)
    {
        s_requested_gains_db[band] = 0.0f;
        s_displayed_gains_db[band] = 0.0f;
        s_gain_displayed_valid[band] = 0U;
    }
    for (category = 0; category < EQ_LCD_CATEGORY_COUNT; category++)
    {
        EQ_DebugLcdCategoryCount[category] = 0UL;
        EQ_DebugLcdCategoryLastCycles[category] = 0UL;
        EQ_DebugLcdCategoryMaxCycles[category] = 0UL;
    }
    for (category = 0; category < 5; category++)
    {
        s_status_displayed_valid[category] = 0;
    }

#if EQ_ENABLE_LCD_DISPLAY == 0
    EQ_DebugLcdEnabled = 0;
    return;
#else
    EQ_DebugLcdEnabled = 1;
#if (!defined(EQ_ALGO_ONLY))
    Lcd_Init();
#endif
#endif
}

void EqualizerDisplay_DrawStaticLayout(void)
{
    int band;

    if (s_runtime_started != 0)
    {
        EQ_DebugLcdUnexpectedFullRedrawCount++;
        return;
    }
    if (s_layout_drawn != 0)
    {
        return;
    }
    if (EQ_BeginDraw() == 0)
    {
        return;
    }

    EQ_LcdClearRect(0, 0, EQ_LCD_W, EQ_LCD_H);
#if EQ_LCD_USE_CHINESE
    EQ_DrawCnTitleRegion();
    EQ_DrawCnStaticStatusLabels();
#else
    EQ_DrawTextRegion(EQ_TITLE_X, EQ_TITLE_Y, EQ_TITLE_W, EQ_TITLE_H,
                      "P3.3 Graphic EQ",
                      EQ_FONT_LARGE, EQ_COLOR_TEXT, 0);
    EQ_LcdDrawText(EQ_MODE_X + 2, EQ_MODE_Y + 3, "Mode:",
                   EQ_FONT_MEDIUM, EQ_COLOR_TEXT, 0, 48);
    EQ_LcdDrawText(EQ_FRAMES_X + 2, EQ_FRAMES_Y + 3, "Frames:",
                   EQ_FONT_MEDIUM, EQ_COLOR_TEXT, 0, 50);
    EQ_LcdDrawText(EQ_LAST_X + 2, EQ_LAST_Y + 3, "Last:",
                   EQ_FONT_MEDIUM, EQ_COLOR_TEXT, 0, 48);
    EQ_LcdDrawText(EQ_MAX_X + 2, EQ_MAX_Y + 3, "Max:",
                   EQ_FONT_MEDIUM, EQ_COLOR_TEXT, 0, 82);
    EQ_LcdDrawText(EQ_CLIP_X + 2, EQ_CLIP_Y + 3, "Clip:",
                   EQ_FONT_MEDIUM, EQ_COLOR_TEXT, 0, 48);
#endif
    EQ_DrawTextRegion(EQ_DB_LABEL_X, EQ_BAR_AREA_Y - 6,
                      EQ_DB_LABEL_W, EQ_DB_LABEL_H,
                      "+12", EQ_FONT_SMALL, EQ_COLOR_MUTED, 0);
    EQ_DrawTextRegion(EQ_DB_LABEL_X, EQ_BAR_ZERO_Y - 9,
                      EQ_DB_LABEL_W, EQ_DB_LABEL_H,
                      "0", EQ_FONT_SMALL, EQ_COLOR_MUTED, 0);
    EQ_DrawTextRegion(EQ_DB_LABEL_X, EQ_BAR_AREA_Y + EQ_BAR_AREA_H - 16,
                      EQ_DB_LABEL_W, EQ_DB_LABEL_H,
                      "-18", EQ_FONT_SMALL, EQ_COLOR_MUTED, 0);
    for (band = 0; band < EQ_NUM_BANDS; band++)
    {
        EQ_DrawBandLabel(band);
    }
    EQ_DrawAxes();

    s_layout_drawn = 1;
    EQ_DebugLcdRefreshCount++;
    EQ_EndDraw();
}

static unsigned long EQ_StatusFloatToTenths(float value)
{
    float max_ms;

    if ((value != value) || (value <= 0.0f))
    {
        return 0UL;
    }
    max_ms = (float)EQ_LCD_STATUS_MAX_TENTHS / 10.0f;
    if (value >= max_ms)
    {
        return EQ_LCD_STATUS_MAX_TENTHS;
    }
    return (unsigned long)(value * 10.0f + 0.5f);
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

static void EQ_UpdatePendingDiagnostic(void)
{
    EQ_DebugLcdPendingMask = s_dirty_mask;
}

static void EQ_SyncRuntimeMask(void)
{
    unsigned int current;
    unsigned int rising;
    unsigned int falling;

    current = EQ_LcdRuntimeMask &
              (EQ_LCD_RUNTIME_STATUS | EQ_LCD_RUNTIME_GAINS);
    rising = current & ~s_applied_runtime_mask;
    falling = s_applied_runtime_mask & ~current;

    if ((falling & EQ_LCD_RUNTIME_STATUS) != 0U)
    {
        s_dirty_mask &= ~EQ_LCD_STATUS_DIRTY_MASK;
        s_forced_dirty_mask &= ~EQ_LCD_STATUS_DIRTY_MASK;
    }
    if ((falling & EQ_LCD_RUNTIME_GAINS) != 0U)
    {
        s_dirty_mask &= ~EQ_LCD_GAINS_DIRTY_MASK;
        s_forced_dirty_mask &= ~EQ_LCD_GAINS_DIRTY_MASK;
    }
    if (((rising & EQ_LCD_RUNTIME_STATUS) != 0U) &&
        (s_have_requested_status != 0))
    {
        s_dirty_mask |= EQ_LCD_STATUS_DIRTY_MASK;
        s_forced_dirty_mask |= EQ_LCD_STATUS_DIRTY_MASK;
    }
    if (((rising & EQ_LCD_RUNTIME_GAINS) != 0U) &&
        (s_have_requested_gains != 0))
    {
        s_dirty_mask |= EQ_LCD_GAINS_DIRTY_MASK;
        s_forced_dirty_mask |= EQ_LCD_GAINS_DIRTY_MASK;
    }

    s_applied_runtime_mask = current;
    EQ_UpdatePendingDiagnostic();
}

void EqualizerDisplay_BeginRuntime(void)
{
    s_runtime_started = 1;
}

void EqualizerDisplay_RequestGains(const EQ_STATE *st)
{
    int band;
    float gain_db;
    unsigned long bit;

    if (st == 0)
    {
        return;
    }
    s_have_requested_gains = 1;
    for (band = 0; band < EQ_NUM_BANDS; band++)
    {
        gain_db = Equalizer_GetBandTargetGainDb(st, band);
        s_requested_gains_db[band] = gain_db;
        bit = 1UL << (EQ_LCD_JOB_BAND_0 - 1 + band);
        if (((s_applied_runtime_mask & EQ_LCD_RUNTIME_GAINS) != 0U) &&
            ((s_gain_displayed_valid[band] == 0U) ||
             (EQ_DisplayAbs(gain_db - s_displayed_gains_db[band]) >= 0.05f)))
        {
            s_dirty_mask |= bit;
        }
        else if ((s_forced_dirty_mask & bit) == 0UL)
        {
            s_dirty_mask &= ~bit;
        }
    }
    EQ_UpdatePendingDiagnostic();
}

void EqualizerDisplay_RequestStatus(unsigned long frames,
                                    float algo_last_ms,
                                    float algo_max_ms,
                                    unsigned long clip_count,
                                    int requested_mode,
                                    int transition_target_mode,
                                    int applied_mode)
{
    unsigned long mode_bit;
    unsigned long last_tenths;
    unsigned long max_tenths;

    last_tenths = EQ_StatusFloatToTenths(algo_last_ms);
    max_tenths = EQ_StatusFloatToTenths(algo_max_ms);
    s_requested_frames = frames;
    s_requested_algo_last_tenths = last_tenths;
    s_requested_algo_max_tenths = max_tenths;
    s_requested_clip_count = clip_count;
    (void)requested_mode;
    s_requested_transition_target_mode = transition_target_mode;
    s_requested_applied_mode = applied_mode;
    s_have_requested_status = 1;

    mode_bit = 1UL << (EQ_LCD_JOB_MODE - 1);
    if (((s_applied_runtime_mask & EQ_LCD_RUNTIME_STATUS) != 0U) &&
        ((s_status_displayed_valid[0] == 0) ||
         (applied_mode != EQ_DebugLcdLastMode) ||
         (transition_target_mode != s_displayed_transition_target_mode)))
    {
        s_dirty_mask |= mode_bit;
    }
    else if ((s_forced_dirty_mask & mode_bit) == 0UL)
    {
        s_dirty_mask &= ~mode_bit;
    }
#define EQ_SET_STATUS_DIRTY(index_, changed_) \
    do { \
        unsigned long status_bit_; \
        status_bit_ = 1UL << (index_); \
        if (((s_applied_runtime_mask & EQ_LCD_RUNTIME_STATUS) != 0U) && \
            ((s_status_displayed_valid[index_] == 0) || (changed_))) \
        { \
            s_dirty_mask |= status_bit_; \
        } \
        else if ((s_forced_dirty_mask & status_bit_) == 0UL) \
        { \
            s_dirty_mask &= ~status_bit_; \
        } \
    } while (0)
    EQ_SET_STATUS_DIRTY(1, frames != s_displayed_frames);
    EQ_SET_STATUS_DIRTY(2, last_tenths != s_displayed_algo_last_tenths);
    EQ_SET_STATUS_DIRTY(3, max_tenths != s_displayed_algo_max_tenths);
    EQ_SET_STATUS_DIRTY(4, clip_count != s_displayed_clip_count);
#undef EQ_SET_STATUS_DIRTY
    EQ_UpdatePendingDiagnostic();
}

int EqualizerDisplay_HasPendingJob(void)
{
    unsigned int current;

    current = EQ_LcdRuntimeMask &
              (EQ_LCD_RUNTIME_STATUS | EQ_LCD_RUNTIME_GAINS);
    return ((s_dirty_mask != 0UL) ||
            ((current & ~s_applied_runtime_mask) != 0U)) ? 1 : 0;
}

void EqualizerDisplay_CancelRuntimeJobs(void)
{
    s_dirty_mask = 0UL;
    s_forced_dirty_mask = 0UL;
    EQ_UpdatePendingDiagnostic();
}

void EqualizerDisplay_AutoDisable(unsigned long reason)
{
    EQ_DebugLcdAutoDisableReason |= reason;
    if (EQ_LcdRuntimeMask != 0U)
    {
        EQ_LcdRuntimeMask = 0U;
        EQ_DebugLcdAutoDisabledCount++;
    }
    EqualizerDisplay_CancelRuntimeJobs();
}

static int EQ_JobCategory(int job)
{
    if ((job >= EQ_LCD_JOB_BAND_0) && (job <= EQ_LCD_JOB_BAND_9))
    {
        return EQ_LCD_CATEGORY_BAND;
    }
    return job - EQ_LCD_JOB_MODE + EQ_LCD_CATEGORY_MODE;
}

static void EQ_DrawDynamicText(int x, int y, int w, int h, const char *text)
{
    EQ_DrawTextRegion(x, y, w, h, text,
                      EQ_FONT_MEDIUM, EQ_COLOR_TEXT, 0);
}

static void EQ_DrawSelectedJob(int job)
{
    char buf[48];
    int band;

    if ((job >= EQ_LCD_JOB_BAND_0) && (job <= EQ_LCD_JOB_BAND_9))
    {
        band = job - EQ_LCD_JOB_BAND_0;
        EQ_DrawBandJob(band, s_requested_gains_db[band]);
        s_displayed_gains_db[band] = s_requested_gains_db[band];
        s_gain_displayed_valid[band] = 1U;
        EQ_DebugLcdGainRedrawCount++;
        return;
    }

    switch (job)
    {
        case EQ_LCD_JOB_MODE:
            EQ_FormatModeValue(buf, (int)sizeof(buf),
                               s_requested_applied_mode,
                               s_requested_transition_target_mode);
            EQ_DrawDynamicText(EQ_MODE_VALUE_X, EQ_MODE_Y,
                               EQ_MODE_VALUE_W, EQ_MODE_H, buf);
            EQ_DebugLcdLastMode = s_requested_applied_mode;
            s_displayed_transition_target_mode =
                s_requested_transition_target_mode;
            s_status_displayed_valid[0] = 1;
            break;
        case EQ_LCD_JOB_FRAMES:
            EQ_FormatUnsignedValue(buf, (int)sizeof(buf), s_requested_frames);
            EQ_DrawDynamicText(EQ_FRAMES_VALUE_X, EQ_FRAMES_Y,
                               EQ_FRAMES_VALUE_W, EQ_FRAMES_H, buf);
            s_displayed_frames = s_requested_frames;
            s_status_displayed_valid[1] = 1;
            break;
        case EQ_LCD_JOB_ALGO_LAST:
            EQ_FormatTenthsMsValue(buf, (int)sizeof(buf),
                                   s_requested_algo_last_tenths);
            EQ_DrawDynamicText(EQ_LAST_VALUE_X, EQ_LAST_Y,
                               EQ_LAST_VALUE_W, EQ_LAST_H, buf);
            s_displayed_algo_last_tenths = s_requested_algo_last_tenths;
            s_status_displayed_valid[2] = 1;
            break;
        case EQ_LCD_JOB_ALGO_MAX:
            EQ_FormatTenthsMsValue(buf, (int)sizeof(buf),
                                   s_requested_algo_max_tenths);
            EQ_DrawDynamicText(EQ_MAX_VALUE_X, EQ_MAX_Y,
                               EQ_MAX_VALUE_W, EQ_MAX_H, buf);
            s_displayed_algo_max_tenths = s_requested_algo_max_tenths;
            s_status_displayed_valid[3] = 1;
            break;
        case EQ_LCD_JOB_CLIP:
            EQ_FormatUnsignedValue(buf, (int)sizeof(buf),
                                   s_requested_clip_count);
            EQ_DrawDynamicText(EQ_CLIP_VALUE_X, EQ_CLIP_Y,
                               EQ_CLIP_VALUE_W, EQ_CLIP_H, buf);
            s_displayed_clip_count = s_requested_clip_count;
            s_status_displayed_valid[4] = 1;
            break;
        default:
            break;
    }
    EQ_DebugLcdStatusRedrawCount++;
}

int EqualizerDisplay_ServiceOneJob(void)
{
    int checked;
    int index;
    int job;
    int category;
    unsigned long bit;
    unsigned long start_cycles;
    unsigned long elapsed_cycles;
    unsigned long tenths_ms;

    EQ_SyncRuntimeMask();
    for (checked = 0; checked < EQ_LCD_JOB_COUNT; checked++)
    {
        index = (s_job_cursor + checked) % EQ_LCD_JOB_COUNT;
        bit = 1UL << index;
        if ((s_dirty_mask & bit) != 0UL)
        {
            if (EQ_BeginDraw() == 0)
            {
                return EQ_LCD_JOB_NONE;
            }
            s_job_cursor = (index + 1) % EQ_LCD_JOB_COUNT;
            job = index + 1;
            start_cycles = EQ_ReadCycles();
            EQ_DrawSelectedJob(job);
            elapsed_cycles = EQ_ReadCycles() - start_cycles;
            EQ_EndDraw();

            s_dirty_mask &= ~bit;
            s_forced_dirty_mask &= ~bit;
            EQ_UpdatePendingDiagnostic();
            category = EQ_JobCategory(job);
            tenths_ms = elapsed_cycles / 45600UL;
            EQ_DebugLcdJobCount++;
            EQ_DebugLcdRefreshCount++;
            EQ_DebugLcdLastJob = job;
            EQ_DebugLcdLastJobCycles = elapsed_cycles;
            EQ_DebugLcdLastJobTenthsMs = tenths_ms;
            EQ_DebugLcdLastJobMs = (float)elapsed_cycles / 456000.0f;
            if (elapsed_cycles > EQ_DebugLcdMaxJobCycles)
            {
                EQ_DebugLcdMaxJobCycles = elapsed_cycles;
            }
            if (tenths_ms > EQ_DebugLcdMaxJobTenthsMs)
            {
                EQ_DebugLcdMaxJobTenthsMs = tenths_ms;
            }
            if (EQ_DebugLcdLastJobMs > EQ_DebugLcdMaxJobMs)
            {
                EQ_DebugLcdMaxJobMs = EQ_DebugLcdLastJobMs;
            }
            EQ_DebugLcdCategoryCount[category]++;
            EQ_DebugLcdCategoryLastCycles[category] = elapsed_cycles;
            if (elapsed_cycles > EQ_DebugLcdCategoryMaxCycles[category])
            {
                EQ_DebugLcdCategoryMaxCycles[category] = elapsed_cycles;
            }
            if (elapsed_cycles > EQ_LCD_JOB_TARGET_CYCLES)
            {
                EQ_DebugLcdBudgetExceededCount++;
            }
            if (elapsed_cycles > EQ_LCD_JOB_HARD_CYCLES)
            {
                EqualizerDisplay_AutoDisable(
                    EQ_LCD_AUTO_DISABLE_JOB_OVER_5MS);
            }
            return job;
        }
    }
    return EQ_LCD_JOB_NONE;
}

void EqualizerDisplay_UpdateAll(const EQ_STATE *st)
{
    EqualizerDisplay_DrawStaticLayout();
    EqualizerDisplay_RequestGains(st);
}

void EqualizerDisplay_UpdateGains(const EQ_STATE *st)
{
    EqualizerDisplay_RequestGains(st);
}

void EqualizerDisplay_UpdateStatus(unsigned long frames,
                                   float last_ms,
                                   float max_ms,
                                   unsigned long clip_count,
                                   int mode)
{
    EqualizerDisplay_RequestStatus(frames, last_ms, max_ms, clip_count,
                                   mode, EQ_PRESET_NONE, mode);
}

#ifdef EQUALIZER_DISPLAY_TEST_MAIN
static int EQ_DisplayTestRequire(int condition, const char *message)
{
    if (condition == 0)
    {
        printf("equalizer_display_mock FAIL: %s\n", message);
        return 1;
    }
    return 0;
}

int main(void)
{
    EQ_STATE st;
    unsigned long before;
    unsigned long pending_before;
    unsigned long border_before;
    unsigned long full_axis_lines;
    unsigned long auto_disabled;
    int job;
    int job_count;
    int i;
    int failures;

    failures = 0;
    Equalizer_Init(&st);
    EqualizerDisplay_Init();

    failures += EQ_DisplayTestRequire(EQ_DebugLcdEnabled == 1,
                                      "LCD display not enabled");
    failures += EQ_DisplayTestRequire(s_layout_drawn == 0,
                                      "init drew static layout");
    failures += EQ_DisplayTestRequire(EQ_DebugLcdRuntimeMask == 0U,
                                      "runtime mask not disabled by default");
    EqualizerDisplay_DrawStaticLayout();
    failures += EQ_DisplayTestRequire(s_layout_drawn == 1,
                                      "static layout not drawn");
    failures += EQ_DisplayTestRequire(s_mock_bounds_failures == 0UL,
                                      "layout draw exceeded LCD bounds");
#if EQ_LCD_USE_CHINESE
    failures += EQ_DisplayTestRequire(s_mock_cn_glyph_count >= 7UL,
                                      "Chinese title glyphs not drawn");
#endif

    EqualizerDisplay_BeginRuntime();
    EQ_LcdRuntimeMask = EQ_LCD_RUNTIME_GAINS;
    s_mock_bar_slot_clear_count = 0UL;
    full_axis_lines = s_mock_line_count;
    Equalizer_ApplyPreset(&st, EQ_PRESET_BASS_BOOST);
    EqualizerDisplay_RequestGains(&st);
    for (job_count = 0; job_count < EQ_NUM_BANDS; job_count++)
    {
        before = s_mock_bar_slot_clear_count;
        border_before = s_mock_chart_border_segment_count;
        full_axis_lines = s_mock_line_count;
        pending_before = EQ_DebugLcdPendingMask;
        job = EqualizerDisplay_ServiceOneJob();
        failures += EQ_DisplayTestRequire(job >= EQ_LCD_JOB_BAND_0 &&
                                          job <= EQ_LCD_JOB_BAND_9,
                                          "gain request returned non-band job");
        failures += EQ_DisplayTestRequire(s_mock_bar_slot_clear_count ==
                                          before + 1UL,
                                          "band job did not clear one slot");
        failures += EQ_DisplayTestRequire(
            EQ_DebugLcdPendingMask ==
            (((job_count == 0) ? EQ_LCD_GAINS_DIRTY_MASK : pending_before) &
             ~(1UL << (job - 1))),
                                          "service changed more than one dirty bit");
        failures += EQ_DisplayTestRequire(s_mock_line_count ==
                                          full_axis_lines +
                                          ((job == EQ_LCD_JOB_BAND_0) ?
                                           4UL : 3UL),
                                          "band job line work was not bounded");
        failures += EQ_DisplayTestRequire(
            s_mock_chart_border_segment_count == border_before +
            ((job == EQ_LCD_JOB_BAND_0) ? 3UL : 2UL),
            "band job did not restore erased chart border");
    }
    failures += EQ_DisplayTestRequire(EqualizerDisplay_HasPendingJob() == 0,
                                      "ten gain jobs did not drain request");
    failures += EQ_DisplayTestRequire(s_mock_bounds_failures == 0UL,
                                      "gain redraw exceeded LCD bounds");

    before = s_mock_bar_slot_clear_count;
    EqualizerDisplay_RequestGains(&st);
    failures += EQ_DisplayTestRequire(EqualizerDisplay_HasPendingJob() == 0,
                                      "unchanged gains created dirty work");
    failures += EQ_DisplayTestRequire(s_mock_bar_slot_clear_count == before,
                                      "gain request performed LCD work");

    Equalizer_SetBandGainDb(&st, 4, 3.0f);
    EqualizerDisplay_RequestGains(&st);
    failures += EQ_DisplayTestRequire(EqualizerDisplay_ServiceOneJob() ==
                                      EQ_LCD_JOB_BAND_0 + 4,
                                      "single changed band selected wrong job");
    failures += EQ_DisplayTestRequire(s_mock_bar_slot_clear_count == before + 1UL,
                                      "single band touched multiple slots");
    failures += EQ_DisplayTestRequire(EqualizerDisplay_HasPendingJob() == 0,
                                      "single band left dirty work");

    Equalizer_ApplyPreset(&st, EQ_PRESET_BASS_BOOST);
    EqualizerDisplay_RequestGains(&st);
    Equalizer_ApplyPreset(&st, EQ_PRESET_TREBLE_BOOST);
    EqualizerDisplay_RequestGains(&st);
    job_count = 0;
    while (EqualizerDisplay_HasPendingJob() != 0)
    {
        failures += EQ_DisplayTestRequire(EqualizerDisplay_ServiceOneJob() !=
                                          EQ_LCD_JOB_NONE,
                                          "latest gain snapshot stalled");
        job_count++;
    }
    failures += EQ_DisplayTestRequire(job_count <= EQ_NUM_BANDS,
                                      "obsolete gain snapshot was queued");
    for (i = 0; i < EQ_NUM_BANDS; i++)
    {
        failures += EQ_DisplayTestRequire(
            EQ_DisplayAbs(s_displayed_gains_db[i] -
                          Equalizer_GetBandTargetGainDb(&st, i)) < 0.05f,
            "latest gain snapshot was not displayed");
    }

    EQ_LcdRuntimeMask = EQ_LCD_RUNTIME_STATUS;
    s_mock_text_clear_count = 0UL;
#if EQ_LCD_USE_CHINESE
    s_mock_cn_glyph_count = 0UL;
#endif
    EqualizerDisplay_RequestStatus(25UL, 1.2f, 3.4f, 7UL,
                                   EQ_PRESET_TREBLE_BOOST,
                                   EQ_PRESET_NONE,
                                   EQ_PRESET_TREBLE_BOOST);
    for (job_count = 0; job_count < 5; job_count++)
    {
        before = s_mock_text_clear_count;
        failures += EQ_DisplayTestRequire(EqualizerDisplay_ServiceOneJob() !=
                                          EQ_LCD_JOB_NONE,
                                          "status request returned no job");
        failures += EQ_DisplayTestRequire(s_mock_text_clear_count == before + 1UL,
                                          "status job did not clear one region");
    }
    failures += EQ_DisplayTestRequire(EqualizerDisplay_HasPendingJob() == 0,
                                      "five status jobs did not drain request");
    failures += EQ_DisplayTestRequire(EQ_DebugLcdLastMode ==
                                      EQ_PRESET_TREBLE_BOOST,
                                      "status mode not recorded");
    before = s_mock_text_clear_count;
    EqualizerDisplay_RequestStatus(25UL, 1.2f, 3.4f, 7UL,
                                   EQ_PRESET_TREBLE_BOOST,
                                   EQ_PRESET_NONE,
                                   EQ_PRESET_TREBLE_BOOST);
    failures += EQ_DisplayTestRequire(EqualizerDisplay_HasPendingJob() == 0,
                                      "unchanged status created dirty work");
    failures += EQ_DisplayTestRequire(s_mock_text_clear_count == before,
                                      "status request performed LCD work");

    EqualizerDisplay_RequestStatus(25UL, 1.2f, 3.4f, 7UL,
                                   EQ_PRESET_BASS_BOOST,
                                   EQ_PRESET_NONE,
                                   EQ_PRESET_BASS_BOOST);
    failures += EQ_DisplayTestRequire(EqualizerDisplay_ServiceOneJob() ==
                                      EQ_LCD_JOB_MODE,
                                      "steady mode job not selected");
    failures += EQ_DisplayTestRequire(strcmp(s_mock_last_text, "BASS") == 0,
                                      "steady mode did not show applied state");
    EqualizerDisplay_RequestStatus(25UL, 1.2f, 3.4f, 7UL,
                                   EQ_PRESET_VOCAL,
                                   EQ_PRESET_TREBLE_BOOST,
                                   EQ_PRESET_BASS_BOOST);
    failures += EQ_DisplayTestRequire(EqualizerDisplay_ServiceOneJob() ==
                                      EQ_LCD_JOB_MODE,
                                      "transition mode job not selected");
    failures += EQ_DisplayTestRequire(strcmp(s_mock_last_text,
                                             "BASS>TREBLE") == 0,
                                      "transition did not show applied>target");
    EqualizerDisplay_RequestStatus(25UL, 1.2f, 3.4f, 7UL,
                                   EQ_PRESET_V_SHAPE,
                                   EQ_PRESET_TREBLE_BOOST,
                                   EQ_PRESET_BASS_BOOST);
    failures += EQ_DisplayTestRequire(EqualizerDisplay_HasPendingJob() == 0,
                                      "queued request changed displayed mode");
    failures += EQ_DisplayTestRequire(strcmp(s_mock_last_text,
                                             "BASS>TREBLE") == 0,
                                      "queued request was shown as applied");

    for (i = 0; i < 1000; i++)
    {
        int applied;
        int target;

        applied = (i & 1) ? EQ_PRESET_BASS_BOOST : EQ_PRESET_FLAT;
        target = (i & 1) ? EQ_PRESET_VOCAL : EQ_PRESET_TREBLE_BOOST;
        EqualizerDisplay_RequestStatus(25UL, 1.2f, 3.4f, 7UL,
                                       target, target, applied);
        failures += EQ_DisplayTestRequire(EqualizerDisplay_ServiceOneJob() ==
                                          EQ_LCD_JOB_MODE,
                                          "transition mode job not selected");
    }
    failures += EQ_DisplayTestRequire(s_mock_bounds_failures == 0UL,
                                      "mode transitions exceeded LCD bounds");

    EqualizerDisplay_RequestStatus(25UL, NAN, INFINITY, 7UL,
                                   EQ_PRESET_BASS_BOOST,
                                   EQ_PRESET_NONE,
                                   EQ_PRESET_BASS_BOOST);
    failures += EQ_DisplayTestRequire(s_requested_algo_last_tenths == 0UL,
                                      "NaN status time was not clamped");
    failures += EQ_DisplayTestRequire(s_requested_algo_max_tenths ==
                                      EQ_LCD_STATUS_MAX_TENTHS,
                                      "infinite status time was not saturated");
    EqualizerDisplay_RequestStatus(25UL, -INFINITY, 3.4e38f, 7UL,
                                   EQ_PRESET_BASS_BOOST,
                                   EQ_PRESET_NONE,
                                   EQ_PRESET_BASS_BOOST);
    failures += EQ_DisplayTestRequire(s_requested_algo_last_tenths == 0UL,
                                      "negative infinite time was not clamped");
    failures += EQ_DisplayTestRequire(s_requested_algo_max_tenths ==
                                      EQ_LCD_STATUS_MAX_TENTHS,
                                      "large finite status time was not saturated");

    before = EQ_DebugLcdUnexpectedFullRedrawCount;
    EqualizerDisplay_DrawStaticLayout();
    failures += EQ_DisplayTestRequire(EQ_DebugLcdUnexpectedFullRedrawCount ==
                                      before + 1UL,
                                      "runtime static layout was not rejected");

    before = s_mock_clear_count + s_mock_fill_count + s_mock_line_count +
             s_mock_text_count;
    EqualizerDisplay_UpdateAll(&st);
    EqualizerDisplay_UpdateGains(&st);
    EqualizerDisplay_UpdateStatus(25UL, 1.2f, 3.4f, 7UL,
                                  EQ_PRESET_TREBLE_BOOST);
    failures += EQ_DisplayTestRequire(s_mock_clear_count + s_mock_fill_count +
                                      s_mock_line_count + s_mock_text_count ==
                                      before,
                                      "legacy runtime API performed LCD work");

    EQ_LcdRuntimeMask = 0U;
    (void)EqualizerDisplay_HasPendingJob();
    Equalizer_SetBandGainDb(&st, 2, -18.0f);
    EqualizerDisplay_RequestGains(&st);
    EQ_LcdRuntimeMask = EQ_LCD_RUNTIME_GAINS;
    job = EQ_LCD_JOB_NONE;
    while ((job != EQ_LCD_JOB_BAND_0 + 2) &&
           (EqualizerDisplay_HasPendingJob() != 0))
    {
        job = EqualizerDisplay_ServiceOneJob();
    }
    failures += EQ_DisplayTestRequire(job == EQ_LCD_JOB_BAND_0 + 2,
                                      "-18 dB band was not serviced");
    failures += EQ_DisplayTestRequire(s_mock_last_bar_bottom_y <=
                                      EQ_BAR_AREA_Y + EQ_BAR_AREA_H - 1,
                                      "-18 dB bar exceeded slot bottom");
    failures += EQ_DisplayTestRequire(s_mock_last_bar_bottom_y ==
                                      EQ_BAR_AREA_Y + EQ_BAR_AREA_H - 1,
                                      "-18 dB bar did not reach clamped bottom");
    EqualizerDisplay_RequestStatus(26UL, 1.3f, 3.5f, 8UL,
                                   EQ_PRESET_FLAT, EQ_PRESET_NONE,
                                   EQ_PRESET_FLAT);
    EqualizerDisplay_CancelRuntimeJobs();
    failures += EQ_DisplayTestRequire(EqualizerDisplay_HasPendingJob() == 0,
                                      "cancel left runtime jobs pending");

    EQ_LcdRuntimeMask = 0U;
    EqualizerDisplay_RequestGains(&st);
    EqualizerDisplay_RequestStatus(27UL, 1.4f, 3.6f, 9UL,
                                   EQ_PRESET_VOCAL, EQ_PRESET_NONE,
                                   EQ_PRESET_VOCAL);
    failures += EQ_DisplayTestRequire(EqualizerDisplay_HasPendingJob() == 0,
                                      "mask zero exposed dynamic jobs");
    EQ_LcdRuntimeMask = EQ_LCD_RUNTIME_STATUS;
    EqualizerDisplay_RequestStatus(27UL, 1.4f, 3.6f, 9UL,
                                   EQ_PRESET_VOCAL, EQ_PRESET_NONE,
                                   EQ_PRESET_VOCAL);
    job_count = 0;
    while (EqualizerDisplay_HasPendingJob() != 0)
    {
        failures += EQ_DisplayTestRequire(EqualizerDisplay_ServiceOneJob() >=
                                          EQ_LCD_JOB_MODE &&
                                          EQ_DebugLcdLastJob <= EQ_LCD_JOB_CLIP,
                                          "status rising edge exposed wrong job");
        job_count++;
    }
    failures += EQ_DisplayTestRequire(job_count == 5,
                                      "status rising edge did not refresh snapshot");
    EQ_LcdRuntimeMask = EQ_LCD_RUNTIME_GAINS;
    EqualizerDisplay_RequestGains(&st);
    job_count = 0;
    while (EqualizerDisplay_HasPendingJob() != 0)
    {
        job = EqualizerDisplay_ServiceOneJob();
        failures += EQ_DisplayTestRequire(job >= EQ_LCD_JOB_BAND_0 &&
                                          job <= EQ_LCD_JOB_BAND_9,
                                          "gains rising edge exposed wrong job");
        job_count++;
    }
    failures += EQ_DisplayTestRequire(job_count == EQ_NUM_BANDS,
                                      "gains rising edge did not refresh snapshot");

    /* Polling and snapshot updates must not consume an unsynced mask edge. */
    EQ_LcdRuntimeMask = 0U;
    (void)EqualizerDisplay_ServiceOneJob();
    EqualizerDisplay_RequestStatus(50UL, 1.7f, 3.9f, 12UL,
                                   EQ_PRESET_BASS_BOOST, EQ_PRESET_NONE,
                                   EQ_PRESET_BASS_BOOST);
    EQ_LcdRuntimeMask = EQ_LCD_RUNTIME_STATUS;
    pending_before = EQ_DebugLcdPendingMask;
    before = EQ_DebugLcdJobCount;
    EqualizerDisplay_RequestStatus(51UL, 1.8f, 4.0f, 13UL,
                                   EQ_PRESET_VOCAL, EQ_PRESET_NONE,
                                   EQ_PRESET_VOCAL);
    failures += EQ_DisplayTestRequire(EqualizerDisplay_HasPendingJob() != 0,
                                      "status rising edge was not observable");
    EqualizerDisplay_RequestStatus(52UL, 1.9f, 4.1f, 14UL,
                                   EQ_PRESET_TREBLE_BOOST, EQ_PRESET_NONE,
                                   EQ_PRESET_TREBLE_BOOST);
    failures += EQ_DisplayTestRequire(EqualizerDisplay_HasPendingJob() != 0,
                                      "status polling consumed rising edge");
    failures += EQ_DisplayTestRequire(EQ_DebugLcdPendingMask == pending_before,
                                      "status polling changed pending mask");
    failures += EQ_DisplayTestRequire(EQ_DebugLcdJobCount == before,
                                      "status polling changed job count");
    failures += EQ_DisplayTestRequire(s_applied_runtime_mask == 0U,
                                      "status polling synchronized mask");
    job = EqualizerDisplay_ServiceOneJob();
    failures += EQ_DisplayTestRequire(job >= EQ_LCD_JOB_MODE &&
                                      job <= EQ_LCD_JOB_CLIP,
                                      "status service lost rising edge");
    failures += EQ_DisplayTestRequire(s_applied_runtime_mask ==
                                      EQ_LCD_RUNTIME_STATUS,
                                      "status service did not synchronize mask");

    EQ_LcdRuntimeMask = 0U;
    (void)EqualizerDisplay_ServiceOneJob();
    Equalizer_ApplyPreset(&st, EQ_PRESET_FLAT);
    EqualizerDisplay_RequestGains(&st);
    EQ_LcdRuntimeMask = EQ_LCD_RUNTIME_GAINS;
    pending_before = EQ_DebugLcdPendingMask;
    before = EQ_DebugLcdJobCount;
    Equalizer_ApplyPreset(&st, EQ_PRESET_V_SHAPE);
    EqualizerDisplay_RequestGains(&st);
    failures += EQ_DisplayTestRequire(EqualizerDisplay_HasPendingJob() != 0,
                                      "gains rising edge was not observable");
    Equalizer_ApplyPreset(&st, EQ_PRESET_TREBLE_BOOST);
    EqualizerDisplay_RequestGains(&st);
    failures += EQ_DisplayTestRequire(EqualizerDisplay_HasPendingJob() != 0,
                                      "gains polling consumed rising edge");
    failures += EQ_DisplayTestRequire(EQ_DebugLcdPendingMask == pending_before,
                                      "gains polling changed pending mask");
    failures += EQ_DisplayTestRequire(EQ_DebugLcdJobCount == before,
                                      "gains polling changed job count");
    failures += EQ_DisplayTestRequire(s_applied_runtime_mask == 0U,
                                      "gains polling synchronized mask");
    job = EqualizerDisplay_ServiceOneJob();
    failures += EQ_DisplayTestRequire(job >= EQ_LCD_JOB_BAND_0 &&
                                      job <= EQ_LCD_JOB_BAND_9,
                                      "gains service lost rising edge");
    failures += EQ_DisplayTestRequire(s_applied_runtime_mask ==
                                      EQ_LCD_RUNTIME_GAINS,
                                      "gains service did not synchronize mask");

    Equalizer_SetBandGainDb(&st, 3, -9.0f);
    EqualizerDisplay_RequestGains(&st);
    failures += EQ_DisplayTestRequire(EQ_DebugLcdPendingMask != 0UL,
                                      "falling-edge setup created no dirty job");
    EQ_LcdRuntimeMask = 0U;
    pending_before = EQ_DebugLcdPendingMask;
    failures += EQ_DisplayTestRequire(EqualizerDisplay_HasPendingJob() != 0,
                                      "falling-edge poll hid existing dirty job");
    failures += EQ_DisplayTestRequire(EQ_DebugLcdPendingMask == pending_before,
                                      "falling-edge poll cancelled jobs");
    failures += EQ_DisplayTestRequire(s_applied_runtime_mask ==
                                      EQ_LCD_RUNTIME_GAINS,
                                      "falling-edge poll synchronized mask");
    failures += EQ_DisplayTestRequire(EqualizerDisplay_ServiceOneJob() ==
                                      EQ_LCD_JOB_NONE,
                                      "falling-edge service drew cancelled job");
    failures += EQ_DisplayTestRequire(EQ_DebugLcdPendingMask == 0UL,
                                      "falling-edge service did not cancel jobs");
    failures += EQ_DisplayTestRequire(s_applied_runtime_mask == 0U,
                                      "falling-edge service did not sync mask");

    EQ_LcdRuntimeMask = EQ_LCD_RUNTIME_STATUS | EQ_LCD_RUNTIME_GAINS;
    Equalizer_SetBandGainDb(&st, 0, -6.0f);
    EqualizerDisplay_RequestGains(&st);
    EqualizerDisplay_RequestStatus(40UL, 1.4f, 3.6f, 9UL,
                                   EQ_PRESET_VOCAL, EQ_PRESET_NONE,
                                   EQ_PRESET_VOCAL);
    job_count = 0;
    job = EQ_LCD_JOB_NONE;
    while ((job != EQ_LCD_JOB_BAND_0) && (job_count < EQ_LCD_JOB_COUNT))
    {
        EqualizerDisplay_RequestStatus(40UL + (unsigned long)job_count,
                                       1.4f, 3.6f, 9UL,
                                       EQ_PRESET_VOCAL, EQ_PRESET_NONE,
                                       EQ_PRESET_VOCAL);
        job = EqualizerDisplay_ServiceOneJob();
        job_count++;
    }
    failures += EQ_DisplayTestRequire(job == EQ_LCD_JOB_BAND_0,
                                      "repeated frames starved a band job");
    EQ_LcdRuntimeMask = EQ_LCD_RUNTIME_STATUS | EQ_LCD_RUNTIME_GAINS;
    EqualizerDisplay_RequestStatus(28UL, 1.5f, 3.7f, 10UL,
                                   EQ_PRESET_FLAT, EQ_PRESET_NONE,
                                   EQ_PRESET_FLAT);
    EqualizerDisplay_RequestGains(&st);
    EQ_LcdRuntimeMask = 0U;
    failures += EQ_DisplayTestRequire(EqualizerDisplay_HasPendingJob() != 0,
                                      "mask falling edge hid pending work");
    failures += EQ_DisplayTestRequire(EqualizerDisplay_ServiceOneJob() ==
                                      EQ_LCD_JOB_NONE,
                                      "mask falling edge serviced cancelled job");
    failures += EQ_DisplayTestRequire(EqualizerDisplay_HasPendingJob() == 0,
                                      "mask falling edge did not cancel jobs");

    EQ_LcdRuntimeMask = EQ_LCD_RUNTIME_GAINS;
    EqualizerDisplay_RequestGains(&st);
    while (EqualizerDisplay_HasPendingJob() != 0)
    {
        (void)EqualizerDisplay_ServiceOneJob();
    }
    Equalizer_SetBandGainDb(&st, 7, -7.0f);
    EqualizerDisplay_RequestGains(&st);
    before = EQ_DebugLcdBudgetExceededCount;
    pending_before = EQ_DebugLcdCategoryCount[EQ_LCD_CATEGORY_BAND];
    s_mock_forced_job_cycles = EQ_LCD_JOB_TARGET_CYCLES + 1UL;
    failures += EQ_DisplayTestRequire(EqualizerDisplay_ServiceOneJob() ==
                                      EQ_LCD_JOB_BAND_0 + 7,
                                      "target-budget band job was not serviced");
    s_mock_forced_job_cycles = 0UL;
    failures += EQ_DisplayTestRequire(EQ_DebugLcdBudgetExceededCount ==
                                      before + 1UL,
                                      "2 ms budget exceed was not counted");
    failures += EQ_DisplayTestRequire(
        EQ_DebugLcdCategoryCount[EQ_LCD_CATEGORY_BAND] == pending_before + 1UL,
        "band category count was not updated");
    failures += EQ_DisplayTestRequire(
        EQ_DebugLcdCategoryLastCycles[EQ_LCD_CATEGORY_BAND] ==
        EQ_LCD_JOB_TARGET_CYCLES + 1UL,
        "band category last timing was not updated");
    failures += EQ_DisplayTestRequire(
        EQ_DebugLcdCategoryMaxCycles[EQ_LCD_CATEGORY_BAND] >=
        EQ_LCD_JOB_TARGET_CYCLES + 1UL,
        "band category max timing was not updated");

    EQ_LcdRuntimeMask = EQ_LCD_RUNTIME_STATUS;
    EqualizerDisplay_RequestStatus(29UL, 1.6f, 3.8f, 11UL,
                                   EQ_PRESET_FLAT, EQ_PRESET_NONE,
                                   EQ_PRESET_FLAT);
    auto_disabled = EQ_DebugLcdAutoDisabledCount;
    s_mock_forced_job_cycles = EQ_LCD_JOB_HARD_CYCLES + 1UL;
    failures += EQ_DisplayTestRequire(EqualizerDisplay_ServiceOneJob() !=
                                      EQ_LCD_JOB_NONE,
                                      "hard-budget job was not serviced");
    s_mock_forced_job_cycles = 0UL;
    failures += EQ_DisplayTestRequire(EQ_LcdRuntimeMask == 0U,
                                      "hard-budget job did not auto-disable");
    failures += EQ_DisplayTestRequire(EQ_DebugLcdAutoDisabledCount ==
                                      auto_disabled + 1UL,
                                      "hard-budget auto-disable count wrong");
    failures += EQ_DisplayTestRequire((EQ_DebugLcdAutoDisableReason &
                                      EQ_LCD_AUTO_DISABLE_JOB_OVER_5MS) != 0UL,
                                      "hard-budget reason not recorded");
    failures += EQ_DisplayTestRequire(EqualizerDisplay_HasPendingJob() == 0,
                                      "hard-budget auto-disable left jobs");

    EQ_LcdRuntimeMask = EQ_LCD_RUNTIME_STATUS | EQ_LCD_RUNTIME_GAINS;
    EqualizerDisplay_RequestGains(&st);
    auto_disabled = EQ_DebugLcdAutoDisabledCount;
    EqualizerDisplay_AutoDisable(EQ_LCD_AUTO_DISABLE_JOB_OVER_5MS);
    failures += EQ_DisplayTestRequire(EQ_LcdRuntimeMask == 0U,
                                      "auto-disable left runtime enabled");
    failures += EQ_DisplayTestRequire(EQ_DebugLcdAutoDisabledCount ==
                                      auto_disabled + 1UL,
                                      "auto-disable did not count transition");
    failures += EQ_DisplayTestRequire(EqualizerDisplay_HasPendingJob() == 0,
                                      "auto-disable left dirty jobs");
    EqualizerDisplay_AutoDisable(EQ_LCD_AUTO_DISABLE_LATENCY_MISS);
    failures += EQ_DisplayTestRequire(EQ_DebugLcdAutoDisabledCount ==
                                      auto_disabled + 1UL,
                                      "repeated auto-disable counted twice");
    failures += EQ_DisplayTestRequire((EQ_DebugLcdAutoDisableReason &
                                      (EQ_LCD_AUTO_DISABLE_JOB_OVER_5MS |
                                       EQ_LCD_AUTO_DISABLE_LATENCY_MISS)) ==
                                      (EQ_LCD_AUTO_DISABLE_JOB_OVER_5MS |
                                       EQ_LCD_AUTO_DISABLE_LATENCY_MISS),
                                      "auto-disable reasons were not retained");

    if (failures != 0)
    {
        return 1;
    }

    printf("equalizer_display_mock PASS\n");
    return 0;
}
#endif
