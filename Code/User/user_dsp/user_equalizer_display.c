/**
 * user_equalizer_display.c
 *
 * Fixed-layout LCD bar chart for Project 3.3 graphic equalizer gains.
 */

#include "user_equalizer_display.h"
#include "stdio.h"
#include "string.h"

#if (!defined(EQ_ALGO_ONLY)) && (EQ_ENABLE_LCD_DISPLAY != 0)
#include "lcd_api.h"
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

static volatile int s_lcd_busy = 0;
static int s_layout_drawn = 0;
static int s_have_last_gains = 0;
static float s_last_gains_db[EQ_NUM_BANDS];

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
static const unsigned char s_cn_mode_flat[] =
{
    CN_PING, CN_ZHI
};
static const unsigned char s_cn_mode_bass[] =
{
    CN_DI, CN_YIN, CN_ZENG, CN_QIANG
};
static const unsigned char s_cn_mode_vocal[] =
{
    CN_REN, CN_SHENG, CN_ZENG, CN_QIANG
};
static const unsigned char s_cn_mode_treble[] =
{
    CN_GAO, CN_YIN, CN_ZENG, CN_QIANG
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
    return EQ_ClampInt(y, EQ_BAR_AREA_Y, EQ_BAR_AREA_Y + EQ_BAR_AREA_H);
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

static int EQ_DrawCnModeName(int x, int y, int mode, int color)
{
    switch (mode)
    {
        case EQ_PRESET_FLAT:
            return EQ_DrawCnSeq(x, y, s_cn_mode_flat,
                                (int)sizeof(s_cn_mode_flat), color);
        case EQ_PRESET_BASS_BOOST:
            return EQ_DrawCnSeq(x, y, s_cn_mode_bass,
                                (int)sizeof(s_cn_mode_bass), color);
        case EQ_PRESET_VOCAL:
            return EQ_DrawCnSeq(x, y, s_cn_mode_vocal,
                                (int)sizeof(s_cn_mode_vocal), color);
        case EQ_PRESET_TREBLE_BOOST:
            return EQ_DrawCnSeq(x, y, s_cn_mode_treble,
                                (int)sizeof(s_cn_mode_treble), color);
        case EQ_PRESET_V_SHAPE:
        default:
            EQ_LcdDrawText(x, y + 2, EQ_ModeName(mode),
                           EQ_FONT_SMALL, color, 0, 80);
            return x + 70;
    }
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

static void EQ_DrawCnModeRegion(int mode)
{
    int x;
    int y;

    EQ_LcdClearRect(EQ_MODE_X, EQ_MODE_Y, EQ_MODE_W, EQ_MODE_H);
#ifdef EQ_ALGO_ONLY
    s_mock_text_clear_count++;
#endif
    x = EQ_MODE_X + 2;
    y = EQ_MODE_Y + 4;
    x = EQ_DrawCnSeq(x, y, s_cn_mode_label,
                     (int)sizeof(s_cn_mode_label), EQ_COLOR_TEXT);
    EQ_LcdDrawText(x, EQ_MODE_Y + 3, ":", EQ_FONT_MEDIUM,
                   EQ_COLOR_TEXT, 0, 12);
    x += 12;
    (void)EQ_DrawCnModeName(x, y, mode, EQ_COLOR_TEXT);
}

static void EQ_DrawCnValueRegion(int x, int y, int w, int h,
                                 const unsigned char *label,
                                 int label_count, const char *value)
{
    int label_x;

    EQ_LcdClearRect(x, y, w, h);
#ifdef EQ_ALGO_ONLY
    s_mock_text_clear_count++;
#endif
    label_x = EQ_DrawCnSeq(x + 2, y + 4, label, label_count, EQ_COLOR_TEXT);
    EQ_LcdDrawText(label_x + 4, y + 3, ":", EQ_FONT_MEDIUM,
                   EQ_COLOR_TEXT, 0, 12);
    EQ_LcdDrawText(label_x + 16, y + 3, value, EQ_FONT_MEDIUM,
                   EQ_COLOR_TEXT, 0, w - (label_x - x) - 16);
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

static void EQ_DrawBand(int band, float gain_db)
{
    int slot_x;
    int bar_x;
    int y;
    int h;
    int color;

    slot_x = EQ_BAR_AREA_X + band * EQ_BAR_PITCH;
    bar_x = slot_x + ((EQ_BAR_SLOT_W - EQ_BAR_WIDTH) / 2);
    y = EQ_DbToY(gain_db);

    EQ_LcdClearRect(slot_x, EQ_BAR_AREA_Y, EQ_BAR_SLOT_W, EQ_BAR_SLOT_H);
#ifdef EQ_ALGO_ONLY
    s_mock_bar_slot_clear_count++;
#endif

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
        EQ_LcdFillRect(bar_x, EQ_BAR_ZERO_Y, EQ_BAR_WIDTH, h, color);
    }
}

#if !EQ_LCD_USE_CHINESE
static void EQ_FormatMs(char *buf, const char *label, float ms)
{
    unsigned long tenths;

    if (ms < 0.0f)
    {
        ms = 0.0f;
    }
    tenths = (unsigned long)(ms * 10.0f + 0.5f);
    sprintf(buf, "%s: %lu.%lu ms", label, tenths / 10UL, tenths % 10UL);
}
#endif

#if EQ_LCD_USE_CHINESE
static void EQ_FormatMsValue(char *buf, float ms)
{
    unsigned long tenths;

    if (ms < 0.0f)
    {
        ms = 0.0f;
    }
    tenths = (unsigned long)(ms * 10.0f + 0.5f);
    sprintf(buf, "%lu.%lu ms", tenths / 10UL, tenths % 10UL);
}
#endif

void EqualizerDisplay_Init(void)
{
    int band;

    s_lcd_busy = 0;
    s_layout_drawn = 0;
    s_have_last_gains = 0;
    for (band = 0; band < EQ_NUM_BANDS; band++)
    {
        s_last_gains_db[band] = 999.0f;
    }

#if EQ_ENABLE_LCD_DISPLAY == 0
    EQ_DebugLcdEnabled = 0;
    return;
#else
    EQ_DebugLcdEnabled = 1;
#if (!defined(EQ_ALGO_ONLY))
    Lcd_Init();
#endif
    EqualizerDisplay_DrawStaticLayout();
#endif
}

void EqualizerDisplay_DrawStaticLayout(void)
{
    int band;

    if (EQ_BeginDraw() == 0)
    {
        return;
    }

    EQ_LcdClearRect(0, 0, EQ_LCD_W, EQ_LCD_H);
#if EQ_LCD_USE_CHINESE
    EQ_DrawCnTitleRegion();
#else
    EQ_DrawTextRegion(EQ_TITLE_X, EQ_TITLE_Y, EQ_TITLE_W, EQ_TITLE_H,
                      "P3.3 Graphic EQ",
                      EQ_FONT_LARGE, EQ_COLOR_TEXT, 0);
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

void EqualizerDisplay_UpdateAll(const EQ_STATE *st)
{
    EqualizerDisplay_DrawStaticLayout();
    EqualizerDisplay_UpdateGains(st);
}

void EqualizerDisplay_UpdateGains(const EQ_STATE *st)
{
    int band;
    int redraw;
    float gains_db[EQ_NUM_BANDS];

    if (st == 0)
    {
        return;
    }
    if (s_layout_drawn == 0)
    {
        EqualizerDisplay_DrawStaticLayout();
    }

    redraw = (s_have_last_gains == 0) ? 1 : 0;
    for (band = 0; band < EQ_NUM_BANDS; band++)
    {
        gains_db[band] = Equalizer_GetBandTargetGainDb(st, band);
        if (EQ_DisplayAbs(gains_db[band] - s_last_gains_db[band]) >= 0.05f)
        {
            redraw = 1;
        }
    }
    if (redraw == 0)
    {
        return;
    }
    if (EQ_BeginDraw() == 0)
    {
        return;
    }

    for (band = 0; band < EQ_NUM_BANDS; band++)
    {
        EQ_DrawBand(band, gains_db[band]);
        s_last_gains_db[band] = gains_db[band];
    }
    EQ_DrawAxes();

    s_have_last_gains = 1;
    EQ_DebugLcdGainRedrawCount++;
    EQ_DebugLcdRefreshCount++;
    EQ_EndDraw();
}

void EqualizerDisplay_UpdateStatus(unsigned long frames,
                                   float last_ms,
                                   float max_ms,
                                   unsigned long clip_count,
                                   int mode)
{
    char buf[48];

    if (s_layout_drawn == 0)
    {
        EqualizerDisplay_DrawStaticLayout();
    }
    if (EQ_BeginDraw() == 0)
    {
        return;
    }

#if EQ_LCD_USE_CHINESE
    EQ_DrawCnModeRegion(mode);

    sprintf(buf, "%lu", frames);
    EQ_DrawCnValueRegion(EQ_FRAMES_X, EQ_FRAMES_Y,
                         EQ_FRAMES_W, EQ_FRAMES_H,
                         s_cn_label_frames,
                         (int)sizeof(s_cn_label_frames),
                         buf);

    EQ_FormatMsValue(buf, last_ms);
    EQ_DrawCnValueRegion(EQ_LAST_X, EQ_LAST_Y,
                         EQ_LAST_W, EQ_LAST_H,
                         s_cn_label_last,
                         (int)sizeof(s_cn_label_last),
                         buf);

    EQ_FormatMsValue(buf, max_ms);
    EQ_DrawCnValueRegion(EQ_MAX_X, EQ_MAX_Y,
                         EQ_MAX_W, EQ_MAX_H,
                         s_cn_label_max,
                         (int)sizeof(s_cn_label_max),
                         buf);

    sprintf(buf, "%lu", clip_count);
    EQ_DrawCnValueRegion(EQ_CLIP_X, EQ_CLIP_Y,
                         EQ_CLIP_W, EQ_CLIP_H,
                         s_cn_label_clip,
                         (int)sizeof(s_cn_label_clip),
                         buf);
#else
    sprintf(buf, "Mode: %s", EQ_ModeName(mode));
    EQ_DrawTextRegion(EQ_MODE_X, EQ_MODE_Y, EQ_MODE_W, EQ_MODE_H,
                      buf, EQ_FONT_MEDIUM, EQ_COLOR_TEXT, 0);

    sprintf(buf, "Frames: %lu", frames);
    EQ_DrawTextRegion(EQ_FRAMES_X, EQ_FRAMES_Y, EQ_FRAMES_W, EQ_FRAMES_H,
                      buf, EQ_FONT_MEDIUM, EQ_COLOR_TEXT, 0);

    EQ_FormatMs(buf, "Last", last_ms);
    EQ_DrawTextRegion(EQ_LAST_X, EQ_LAST_Y, EQ_LAST_W, EQ_LAST_H,
                      buf, EQ_FONT_MEDIUM, EQ_COLOR_TEXT, 0);

    EQ_FormatMs(buf, "Max", max_ms);
    EQ_DrawTextRegion(EQ_MAX_X, EQ_MAX_Y, EQ_MAX_W, EQ_MAX_H,
                      buf, EQ_FONT_MEDIUM, EQ_COLOR_TEXT, 0);

    sprintf(buf, "Clip: %lu", clip_count);
    EQ_DrawTextRegion(EQ_CLIP_X, EQ_CLIP_Y, EQ_CLIP_W, EQ_CLIP_H,
                      buf, EQ_FONT_MEDIUM, EQ_COLOR_TEXT, 0);
#endif

    EQ_DebugLcdLastMode = mode;
    EQ_DebugLcdStatusRedrawCount++;
    EQ_DebugLcdRefreshCount++;
    EQ_EndDraw();
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
    unsigned long gain_redraws;
    unsigned long status_redraws;
    unsigned long bar_clears;
    int failures;

    failures = 0;
    Equalizer_Init(&st);
    EqualizerDisplay_Init();

    failures += EQ_DisplayTestRequire(EQ_DebugLcdEnabled == 1,
                                      "LCD display not enabled");
    failures += EQ_DisplayTestRequire(s_layout_drawn == 1,
                                      "static layout not drawn");
    failures += EQ_DisplayTestRequire(s_mock_bounds_failures == 0UL,
                                      "layout draw exceeded LCD bounds");
#if EQ_LCD_USE_CHINESE
    failures += EQ_DisplayTestRequire(s_mock_cn_glyph_count >= 7UL,
                                      "Chinese title glyphs not drawn");
#endif

    s_mock_bar_slot_clear_count = 0UL;
    Equalizer_ApplyPreset(&st, EQ_PRESET_BASS_BOOST);
    EqualizerDisplay_UpdateGains(&st);
    failures += EQ_DisplayTestRequire(s_mock_bar_slot_clear_count ==
                                      (unsigned long)EQ_NUM_BANDS,
                                      "bar slots not cleared before redraw");
    failures += EQ_DisplayTestRequire(s_mock_fill_count >=
                                      (unsigned long)EQ_NUM_BANDS,
                                      "bars not filled");
    failures += EQ_DisplayTestRequire(s_mock_bounds_failures == 0UL,
                                      "gain redraw exceeded LCD bounds");

    gain_redraws = EQ_DebugLcdGainRedrawCount;
    bar_clears = s_mock_bar_slot_clear_count;
    EqualizerDisplay_UpdateGains(&st);
    failures += EQ_DisplayTestRequire(EQ_DebugLcdGainRedrawCount ==
                                      gain_redraws,
                                      "unchanged gains redrawn");
    failures += EQ_DisplayTestRequire(s_mock_bar_slot_clear_count ==
                                      bar_clears,
                                      "unchanged gains cleared bar slots");

    Equalizer_ApplyPreset(&st, EQ_PRESET_TREBLE_BOOST);
    EqualizerDisplay_UpdateGains(&st);
    failures += EQ_DisplayTestRequire(EQ_DebugLcdGainRedrawCount ==
                                      gain_redraws + 1UL,
                                      "changed gains not redrawn");

    status_redraws = EQ_DebugLcdStatusRedrawCount;
    s_mock_text_clear_count = 0UL;
#if EQ_LCD_USE_CHINESE
    s_mock_cn_glyph_count = 0UL;
#endif
    EqualizerDisplay_UpdateStatus(25UL, 1.2f, 3.4f, 7UL,
                                  EQ_PRESET_TREBLE_BOOST);
    failures += EQ_DisplayTestRequire(EQ_DebugLcdStatusRedrawCount ==
                                      status_redraws + 1UL,
                                      "status not redrawn");
    failures += EQ_DisplayTestRequire(s_mock_text_clear_count >= 5UL,
                                      "status text regions not cleared");
    failures += EQ_DisplayTestRequire(EQ_DebugLcdLastMode ==
                                      EQ_PRESET_TREBLE_BOOST,
                                      "status mode not recorded");
#if EQ_LCD_USE_CHINESE
    failures += EQ_DisplayTestRequire(s_mock_cn_glyph_count >= 14UL,
                                      "Chinese status glyphs not drawn");
#endif
    failures += EQ_DisplayTestRequire(s_mock_bounds_failures == 0UL,
                                      "status redraw exceeded LCD bounds");

    if (failures != 0)
    {
        return 1;
    }

    printf("equalizer_display_mock PASS\n");
    return 0;
}
#endif
