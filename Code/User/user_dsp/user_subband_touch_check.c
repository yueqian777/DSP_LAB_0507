#include "user_subband_touch_check.h"

#include "system.h"
#include "lcd_api.h"
#include "touch_api.h"
#include "timer_api.h"

#define TOUCH_CHECK_LCD_WIDTH 800U
#define TOUCH_CHECK_LCD_HEIGHT 480U
#define TOUCH_CHECK_PANEL_RIGHT 352
#define TOUCH_CHECK_MAP_LEFT 384
#define TOUCH_CHECK_MAP_TOP 64
#define TOUCH_CHECK_MAP_RIGHT 783
#define TOUCH_CHECK_MAP_BOTTOM 463

static void TouchCheck_SetRect(tRectangle *rect,
                               int x_min,
                               int y_min,
                               int x_max,
                               int y_max)
{
    rect->sXMin = (short)x_min;
    rect->sYMin = (short)y_min;
    rect->sXMax = (short)x_max;
    rect->sYMax = (short)y_max;
}

static void TouchCheck_FillRect(int x_min,
                                int y_min,
                                int x_max,
                                int y_max,
                                unsigned long color)
{
    tRectangle rect;

    TouchCheck_SetRect(&rect, x_min, y_min, x_max, y_max);
    GrContextForegroundSet(&Lcd_Context, color);
    GrRectFill(&Lcd_Context, &rect);
}

static char *TouchCheck_AppendText(char *destination, const char *source)
{
    while (*source != '\0')
    {
        *destination++ = *source++;
    }
    *destination = '\0';
    return destination;
}

static char *TouchCheck_AppendUnsigned(char *destination,
                                       unsigned long value)
{
    char reversed[11];
    unsigned int count;

    count = 0U;
    do
    {
        reversed[count++] = (char)('0' + (value % 10UL));
        value /= 10UL;
    }
    while (value != 0UL);

    while (count > 0U)
    {
        *destination++ = reversed[--count];
    }
    *destination = '\0';
    return destination;
}

static char TouchCheck_HexDigit(unsigned int value)
{
    value &= 0x0FU;
    if (value < 10U)
    {
        return (char)('0' + value);
    }
    return (char)('A' + (value - 10U));
}

static char *TouchCheck_AppendHexByte(char *destination,
                                      unsigned int value)
{
    destination = TouchCheck_AppendText(destination, "0x");
    *destination++ = TouchCheck_HexDigit(value >> 4);
    *destination++ = TouchCheck_HexDigit(value);
    *destination = '\0';
    return destination;
}

static void TouchCheck_DrawText(const char *text,
                                int x,
                                int y,
                                unsigned long color)
{
    GrContextForegroundSet(&Lcd_Context, color);
    GrStringDraw(&Lcd_Context, text, -1, x, y, 0);
}

static void TouchCheck_DrawValueLine(const char *label,
                                     unsigned long value,
                                     int y)
{
    char line[64];
    char *cursor;

    cursor = TouchCheck_AppendText(line, label);
    cursor = TouchCheck_AppendText(cursor, ": ");
    cursor = TouchCheck_AppendUnsigned(cursor, value);
    *cursor = '\0';
    TouchCheck_DrawText(line, 18, y, ClrWhite);
}

static void TouchCheck_DrawRawStateLine(int y)
{
    char line[64];
    char *cursor;

    cursor = TouchCheck_AppendText(line, "Raw state: ");
    cursor = TouchCheck_AppendHexByte(cursor, Touch_DebugRawState);
    *cursor = '\0';
    TouchCheck_DrawText(line, 18, y, ClrWhite);
}

static unsigned int TouchCheck_ClampX(unsigned int x)
{
    if (x >= TOUCH_CHECK_LCD_WIDTH)
    {
        return (TOUCH_CHECK_LCD_WIDTH - 1U);
    }
    return x;
}

static unsigned int TouchCheck_ClampY(unsigned int y)
{
    if (y >= TOUCH_CHECK_LCD_HEIGHT)
    {
        return (TOUCH_CHECK_LCD_HEIGHT - 1U);
    }
    return y;
}

static int TouchCheck_MapX(unsigned int x)
{
    unsigned long span;

    x = TouchCheck_ClampX(x);
    span = (unsigned long)(TOUCH_CHECK_MAP_RIGHT - TOUCH_CHECK_MAP_LEFT);
    return TOUCH_CHECK_MAP_LEFT +
           (int)((span * (unsigned long)x) /
                 (unsigned long)(TOUCH_CHECK_LCD_WIDTH - 1U));
}

static int TouchCheck_MapY(unsigned int y)
{
    unsigned long span;

    y = TouchCheck_ClampY(y);
    span = (unsigned long)(TOUCH_CHECK_MAP_BOTTOM - TOUCH_CHECK_MAP_TOP);
    return TOUCH_CHECK_MAP_TOP +
           (int)((span * (unsigned long)y) /
                 (unsigned long)(TOUCH_CHECK_LCD_HEIGHT - 1U));
}

static void TouchCheck_DrawStatic(void)
{
    tRectangle map_rect;

    TouchCheck_FillRect(0, 0, 799, 479, ClrBlack);
    TouchCheck_FillRect(0, 0, 799, 42, ClrDarkSlateGray);
    GrContextFontSet(&Lcd_Context, &g_sFontCmss18b);
    TouchCheck_DrawText("Touch Check", 18, 12, ClrWhite);
    GrContextFontSet(&Lcd_Context, &g_sFontFixed6x8);
    TouchCheck_DrawText("Raw GT1151 status and scaled coordinate map",
                        384, 18, ClrLightSteelBlue);

    TouchCheck_SetRect(&map_rect,
                       TOUCH_CHECK_MAP_LEFT,
                       TOUCH_CHECK_MAP_TOP,
                       TOUCH_CHECK_MAP_RIGHT,
                       TOUCH_CHECK_MAP_BOTTOM);
    GrContextForegroundSet(&Lcd_Context, ClrLightSteelBlue);
    GrRectDraw(&Lcd_Context, &map_rect);
}

static void TouchCheck_DrawStatus(void)
{
    GrContextFontSet(&Lcd_Context, &g_sFontFixed6x8);
    TouchCheck_FillRect(0, 44, TOUCH_CHECK_PANEL_RIGHT, 479, ClrBlack);
    TouchCheck_DrawValueLine("INT count", Touch_DebugInterruptCount, 58);
    TouchCheck_DrawValueLine("Scan count", Touch_DebugScanCount, 82);
    TouchCheck_DrawRawStateLine(106);
    TouchCheck_DrawValueLine("Ready count", Touch_DebugReadyCount, 130);
    TouchCheck_DrawValueLine("No-data count", Touch_DebugNoDataCount, 154);
    TouchCheck_DrawValueLine("Down count", Touch_DebugDownCount, 178);
    TouchCheck_DrawValueLine("Release count", Touch_DebugReleaseCount, 202);
    TouchCheck_DrawValueLine("I2C error count", Touch_DebugI2cErrorCount, 226);
    TouchCheck_DrawValueLine("Touch_Sta", Touch_Sta, 250);
    TouchCheck_DrawValueLine("Touch_X", Touch_X, 274);
    TouchCheck_DrawValueLine("Touch_Y", Touch_Y, 298);
    TouchCheck_DrawValueLine("Last_X", Touch_DebugLastX, 322);
    TouchCheck_DrawValueLine("Last_Y", Touch_DebugLastY, 346);
    TouchCheck_DrawValueLine("PID read ok", Touch_DebugProductIdReadOk, 370);
    TouchCheck_DrawValueLine("Config sent", Touch_DebugConfigSent, 394);
    TouchCheck_DrawValueLine("Ctrl write ok", Touch_DebugCtrlWriteOk, 418);
}

static void TouchCheck_DrawMarker(void)
{
    int x;
    int y;

    TouchCheck_FillRect(TOUCH_CHECK_MAP_LEFT + 1,
                        TOUCH_CHECK_MAP_TOP + 1,
                        TOUCH_CHECK_MAP_RIGHT - 1,
                        TOUCH_CHECK_MAP_BOTTOM - 1,
                        ClrBlack);
    GrContextForegroundSet(&Lcd_Context, ClrDarkSlateGray);
    GrLineDrawH(&Lcd_Context,
                TOUCH_CHECK_MAP_LEFT + 1,
                TOUCH_CHECK_MAP_RIGHT - 1,
                (TOUCH_CHECK_MAP_TOP + TOUCH_CHECK_MAP_BOTTOM) / 2);
    GrLineDrawV(&Lcd_Context,
                (TOUCH_CHECK_MAP_LEFT + TOUCH_CHECK_MAP_RIGHT) / 2,
                TOUCH_CHECK_MAP_TOP + 1,
                TOUCH_CHECK_MAP_BOTTOM - 1);

    if ((Touch_Sta != 0U) || (Touch_DebugDownCount != 0UL))
    {
        x = TouchCheck_MapX(Touch_DebugLastX);
        y = TouchCheck_MapY(Touch_DebugLastY);
        GrContextForegroundSet(&Lcd_Context,
                               (Touch_Sta != 0U) ? ClrLime : ClrYellow);
        GrLineDrawH(&Lcd_Context, x - 10, x + 10, y);
        GrLineDrawV(&Lcd_Context, x, y - 10, y + 10);
        GrCircleDraw(&Lcd_Context, x, y, 12);
    }
}

void Subband_Touch_Check_Example(void)
{
    TouchScanResult result;

    Sys_Init();
    Lcd_Init();
    Touch_Init();
    Tim2_Init(TIMER_20HZ);

    TouchCheck_DrawStatic();
    TouchCheck_DrawStatus();
    TouchCheck_DrawMarker();

    while (1)
    {
        if ((FLAG_TOUCH != 0U) || (FLAG_TIM2 != 0U))
        {
            FLAG_TOUCH = 0U;
            FLAG_TIM2 = 0U;
            result = Touch_Scan();
            (void)result;
            TouchCheck_DrawStatus();
            TouchCheck_DrawMarker();
        }
    }
}
