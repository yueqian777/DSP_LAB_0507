#include "user_subband_ui.h"

#if SUBBAND_UI_ENABLE

#include "user_subband_ui_font.h"
#include "user_subband_ui_logic.h"
#include "user_subband_flow.h"
#include "user_subband_denoise.h"
#include "user_subband_codec_loopback.h"
#include "lcd_api.h"
#include "touch_api.h"
#include "pushbutton.h"
#include "widget.h"

#if defined(__TI_COMPILER_VERSION__) || defined(__TMS320C6X__)
#include "c6x.h"
#endif

#define UI_MODE_BUTTON_W 180
#define UI_MODE_BUTTON_H 52
#define UI_MODE_X0 16
#define UI_MODE_X1 212
#define UI_MODE_X2 408
#define UI_MODE_X3 604
#define UI_MODE_Y0 124
#define UI_MODE_Y1 188

#define UI_RATE_BUTTON_W 168
#define UI_RATE_BUTTON_H 46
#define UI_RATE_X0 116
#define UI_RATE_X1 316
#define UI_RATE_X2 516
#define UI_RATE_Y 258

#define UI_COLOR_BACKGROUND ClrBlack
#define UI_COLOR_PANEL ClrDarkSlateGray
#define UI_COLOR_TEXT ClrWhite
#define UI_COLOR_ACCENT ClrRoyalBlue
#define UI_COLOR_BUTTON ClrSteelBlue
#define UI_COLOR_DISABLED ClrDimGray
#define UI_COLOR_PROGRESS ClrLimeGreen

#define UI_COUNTDOWN_FRAME_INTERVAL 5UL
#define UI_LOAD_FRAME_INTERVAL 25UL
#define UI_FRAME_BUDGET_CYCLES 9338880UL

volatile unsigned long SUBBAND_UI_DebugTouchCount = 0UL;
volatile unsigned long SUBBAND_UI_DebugAcceptedTouchCount = 0UL;
volatile unsigned long SUBBAND_UI_DebugRefreshCount = 0UL;
volatile unsigned long SUBBAND_UI_DebugFullRedrawCount = 0UL;
volatile unsigned long SUBBAND_UI_DebugLastDrawCycles = 0UL;
volatile unsigned long SUBBAND_UI_DebugMaxDrawCycles = 0UL;
volatile unsigned long SUBBAND_UI_DebugLastTouchCycles = 0UL;
volatile unsigned long SUBBAND_UI_DebugMaxTouchCycles = 0UL;
volatile int SUBBAND_UI_DebugSelectedMode = SUBBAND_DEMO_DEFAULT_MODE;
volatile int SUBBAND_UI_DebugDisplayedMode = -1;
volatile int SUBBAND_UI_DebugSelectedCodecKbps = 240;
volatile int SUBBAND_UI_DebugDisplayedTargetKbps = 0;
volatile int SUBBAND_UI_DebugCountdownMs = 0;
volatile int SUBBAND_UI_DebugAlgoLoadPercent = 0;
volatile unsigned long SUBBAND_UI_DebugSkippedRefreshes = 0UL;
volatile unsigned long SUBBAND_UI_DebugDirtyFlags = UI_DIRTY_ALL;
volatile unsigned long SUBBAND_UI_DebugFontBytes = 0UL;

static unsigned char UI_Initialized = 0U;
static unsigned long UI_DirtyFlags = UI_DIRTY_ALL;
static unsigned long UI_LastCountdownFrame = 0UL;
static unsigned long UI_LastLoadFrame = 0UL;
static int UI_LastLearning = -1;
static int UI_LastReady = -1;
static unsigned long UI_LastLearnHops = 0xFFFFFFFFUL;
static unsigned long UI_LastTargetHops = 0xFFFFFFFFUL;
static int UI_LastCodecEnabled = -1;
static int UI_LastSelectedKbps = -1;
static int UI_LastTargetKbps = -1;
static int UI_LastLoadPercent = -1;
static SubbandUITouchLatch UI_RateLatch;

static void SubbandUI_OnModeButton(tWidget *widget);

RectangularButton(UI_ModeButton0, WIDGET_ROOT, 0, 0,
                  &Lcd_Display, UI_MODE_X0, UI_MODE_Y0,
                  UI_MODE_BUTTON_W, UI_MODE_BUTTON_H,
                  PB_STYLE_FILL | PB_STYLE_OUTLINE | PB_STYLE_RELEASE_NOTIFY,
                  UI_COLOR_BUTTON, UI_COLOR_ACCENT, ClrWhite, UI_COLOR_TEXT,
                  &g_sFontCmss18b, "", 0, 0, 0, 0, SubbandUI_OnModeButton);
RectangularButton(UI_ModeButton1, WIDGET_ROOT, 0, 0,
                  &Lcd_Display, UI_MODE_X1, UI_MODE_Y0,
                  UI_MODE_BUTTON_W, UI_MODE_BUTTON_H,
                  PB_STYLE_FILL | PB_STYLE_OUTLINE | PB_STYLE_RELEASE_NOTIFY,
                  UI_COLOR_BUTTON, UI_COLOR_ACCENT, ClrWhite, UI_COLOR_TEXT,
                  &g_sFontCmss18b, "", 0, 0, 0, 0, SubbandUI_OnModeButton);
RectangularButton(UI_ModeButton2, WIDGET_ROOT, 0, 0,
                  &Lcd_Display, UI_MODE_X2, UI_MODE_Y0,
                  UI_MODE_BUTTON_W, UI_MODE_BUTTON_H,
                  PB_STYLE_FILL | PB_STYLE_OUTLINE | PB_STYLE_RELEASE_NOTIFY,
                  UI_COLOR_BUTTON, UI_COLOR_ACCENT, ClrWhite, UI_COLOR_TEXT,
                  &g_sFontCmss18b, "", 0, 0, 0, 0, SubbandUI_OnModeButton);
RectangularButton(UI_ModeButton3, WIDGET_ROOT, 0, 0,
                  &Lcd_Display, UI_MODE_X3, UI_MODE_Y0,
                  UI_MODE_BUTTON_W, UI_MODE_BUTTON_H,
                  PB_STYLE_FILL | PB_STYLE_OUTLINE | PB_STYLE_RELEASE_NOTIFY,
                  UI_COLOR_BUTTON, UI_COLOR_ACCENT, ClrWhite, UI_COLOR_TEXT,
                  &g_sFontCmss18b, "", 0, 0, 0, 0, SubbandUI_OnModeButton);
RectangularButton(UI_ModeButton4, WIDGET_ROOT, 0, 0,
                  &Lcd_Display, UI_MODE_X0, UI_MODE_Y1,
                  UI_MODE_BUTTON_W, UI_MODE_BUTTON_H,
                  PB_STYLE_FILL | PB_STYLE_OUTLINE | PB_STYLE_RELEASE_NOTIFY,
                  UI_COLOR_BUTTON, UI_COLOR_ACCENT, ClrWhite, UI_COLOR_TEXT,
                  &g_sFontCmss18b, "", 0, 0, 0, 0, SubbandUI_OnModeButton);
RectangularButton(UI_ModeButton5, WIDGET_ROOT, 0, 0,
                  &Lcd_Display, UI_MODE_X1, UI_MODE_Y1,
                  UI_MODE_BUTTON_W, UI_MODE_BUTTON_H,
                  PB_STYLE_FILL | PB_STYLE_OUTLINE | PB_STYLE_RELEASE_NOTIFY,
                  UI_COLOR_BUTTON, UI_COLOR_ACCENT, ClrWhite, UI_COLOR_TEXT,
                  &g_sFontCmss18b, "", 0, 0, 0, 0, SubbandUI_OnModeButton);
RectangularButton(UI_ModeButton6, WIDGET_ROOT, 0, 0,
                  &Lcd_Display, UI_MODE_X2, UI_MODE_Y1,
                  UI_MODE_BUTTON_W, UI_MODE_BUTTON_H,
                  PB_STYLE_FILL | PB_STYLE_OUTLINE | PB_STYLE_RELEASE_NOTIFY,
                  UI_COLOR_BUTTON, UI_COLOR_ACCENT, ClrWhite, UI_COLOR_TEXT,
                  &g_sFontCmss18b, "", 0, 0, 0, 0, SubbandUI_OnModeButton);
RectangularButton(UI_ModeButton7, WIDGET_ROOT, 0, 0,
                  &Lcd_Display, UI_MODE_X3, UI_MODE_Y1,
                  UI_MODE_BUTTON_W, UI_MODE_BUTTON_H,
                  PB_STYLE_FILL | PB_STYLE_OUTLINE | PB_STYLE_RELEASE_NOTIFY,
                  UI_COLOR_BUTTON, UI_COLOR_ACCENT, ClrWhite, UI_COLOR_TEXT,
                  &g_sFontCmss18b, "", 0, 0, 0, 0, SubbandUI_OnModeButton);

static tPushButtonWidget *const UI_ModeButtons[8] =
{
    &UI_ModeButton0, &UI_ModeButton1, &UI_ModeButton2, &UI_ModeButton3,
    &UI_ModeButton4, &UI_ModeButton5, &UI_ModeButton6, &UI_ModeButton7
};

static const SubbandUIPhrase UI_ModePhrases[8] =
{
    SUBBAND_UI_PHRASE_MODE_RAW,
    SUBBAND_UI_PHRASE_MODE_WOLA,
    SUBBAND_UI_PHRASE_MODE_BASIC,
    SUBBAND_UI_PHRASE_MODE_MINSTAT,
    SUBBAND_UI_PHRASE_MODE_MCRA,
    SUBBAND_UI_PHRASE_MODE_STRONG,
    SUBBAND_UI_PHRASE_MODE_FULL,
    SUBBAND_UI_PHRASE_MODE_CODEC
};

static void UI_SetRect(tRectangle *rect, int x_min, int y_min,
                       int x_max, int y_max)
{
    rect->sXMin = (short)x_min;
    rect->sYMin = (short)y_min;
    rect->sXMax = (short)x_max;
    rect->sYMax = (short)y_max;
}

static void UI_FillRect(int x_min, int y_min, int x_max, int y_max,
                        unsigned long color)
{
    tRectangle rect;

    UI_SetRect(&rect, x_min, y_min, x_max, y_max);
    GrContextForegroundSet(&Lcd_Context, color);
    GrRectFill(&Lcd_Context, &rect);
}

static void UI_DrawAscii(const char *text, int x, int y,
                         unsigned long color)
{
    GrContextFontSet(&Lcd_Context, &g_sFontCmss18b);
    GrContextForegroundSet(&Lcd_Context, color);
    GrStringDraw(&Lcd_Context, text, -1, x, y, 0);
}

static char *UI_AppendText(char *destination, const char *source)
{
    while (*source != '\0')
    {
        *destination++ = *source++;
    }
    *destination = '\0';
    return destination;
}

static char *UI_AppendUnsigned(char *destination, unsigned long value)
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

static char *UI_AppendInt(char *destination, int value)
{
    unsigned long magnitude;

    if (value < 0)
    {
        *destination++ = '-';
        magnitude = (unsigned long)(-(value + 1)) + 1UL;
    }
    else
    {
        magnitude = (unsigned long)value;
    }
    return UI_AppendUnsigned(destination, magnitude);
}

static void UI_DrawCenteredPhrase(SubbandUIPhrase phrase,
                                  int x, int y, int width)
{
    unsigned int phrase_width;
    int draw_x;

    phrase_width = SubbandUIFont_PhraseWidth(phrase);
    draw_x = x + (width - (int)phrase_width) / 2;
    SubbandUIFont_DrawPhrase(&Lcd_Context, phrase, draw_x, y, UI_COLOR_TEXT);
}

static void UI_MarkDirty(unsigned long flags)
{
    UI_DirtyFlags |= flags;
    SUBBAND_UI_DebugDirtyFlags = UI_DirtyFlags;
}

static void UI_DrawStaticBackground(void)
{
    UI_FillRect(0, 0, 799, 479, UI_COLOR_BACKGROUND);
    UI_FillRect(0, 0, 799, 39, ClrMidnightBlue);
    UI_DrawCenteredPhrase(SUBBAND_UI_PHRASE_TITLE, 0, 11, 800);
    UI_FillRect(16, 44, 783, 115, UI_COLOR_PANEL);
    UI_FillRect(16, 316, 783, 383, UI_COLOR_PANEL);
    UI_FillRect(16, 420, 783, 463, UI_COLOR_PANEL);
    SUBBAND_UI_DebugFullRedrawCount++;
}

static void UI_DrawModePhrase(unsigned int index)
{
    int x;
    int y;

    if (index < 4U)
    {
        y = UI_MODE_Y0 + 18;
    }
    else
    {
        y = UI_MODE_Y1 + 18;
    }
    switch (index & 3U)
    {
    case 0U: x = UI_MODE_X0; break;
    case 1U: x = UI_MODE_X1; break;
    case 2U: x = UI_MODE_X2; break;
    default: x = UI_MODE_X3; break;
    }
    UI_DrawCenteredPhrase(UI_ModePhrases[index], x, y, UI_MODE_BUTTON_W);
}

static void UI_DrawModeButtons(void)
{
    unsigned int index;
    int mode;
    unsigned long fill_color;

    for (index = 0U; index < 8U; index++)
    {
        mode = SubbandUI_ModeForButton(index);
        fill_color = (mode == SUBBAND_DebugAppliedDemoMode) ?
                     UI_COLOR_ACCENT : UI_COLOR_BUTTON;
        PushButtonFillColorSet(UI_ModeButtons[index], fill_color);
        WidgetPaint((tWidget *)UI_ModeButtons[index]);
        UI_DrawModePhrase(index);
    }
    SUBBAND_UI_DebugDisplayedMode = SUBBAND_DebugAppliedDemoMode;
}

static void UI_DrawStatus(void)
{
    char text[48];
    char *cursor;
    int applied;
    int button_index;

    applied = SUBBAND_DebugAppliedDemoMode;
    UI_FillRect(17, 45, 782, 114, UI_COLOR_PANEL);
    SubbandUIFont_DrawPhrase(&Lcd_Context, SUBBAND_UI_PHRASE_LABEL_MODE,
                             28, 56, UI_COLOR_TEXT);
    cursor = UI_AppendInt(text, applied);
    *cursor = '\0';
    UI_DrawAscii(text, 72, 54, ClrLightSkyBlue);
    for (button_index = 0; button_index < 8; button_index++)
    {
        if (SubbandUI_ModeForButton((unsigned int)button_index) == applied)
        {
            SubbandUIFont_DrawPhrase(&Lcd_Context,
                                     UI_ModePhrases[button_index],
                                     126, 56, UI_COLOR_TEXT);
            break;
        }
    }
    SubbandUIFont_DrawPhrase(&Lcd_Context, SUBBAND_UI_PHRASE_LABEL_RUNNING,
                             650, 56, ClrLimeGreen);
    cursor = UI_AppendText(text, "REQ ");
    cursor = UI_AppendInt(cursor, SUBBAND_DebugDemoMode);
    cursor = UI_AppendText(cursor, "  APPLIED ");
    cursor = UI_AppendInt(cursor, applied);
    *cursor = '\0';
    UI_DrawAscii(text, 28, 84, ClrLightSteelBlue);
}

static void UI_DrawRateButton(int x, int kbps, int codec_active)
{
    char text[20];
    char *cursor;
    unsigned long color;

    if (codec_active == 0)
    {
        color = UI_COLOR_DISABLED;
    }
    else if (kbps == SUBBAND_UI_DebugSelectedCodecKbps)
    {
        color = UI_COLOR_ACCENT;
    }
    else
    {
        color = UI_COLOR_BUTTON;
    }
    UI_FillRect(x, UI_RATE_Y, x + UI_RATE_BUTTON_W - 1,
                UI_RATE_Y + UI_RATE_BUTTON_H - 1, color);
    cursor = UI_AppendInt(text, kbps);
    cursor = UI_AppendText(cursor, " kbps");
    *cursor = '\0';
    UI_DrawAscii(text, x + 39, UI_RATE_Y + 14, UI_COLOR_TEXT);
}

static void UI_DrawRate(void)
{
    char text[64];
    char *cursor;
    int codec_active;
    int estimated;

    codec_active = SubbandUI_IsCodecMode(SUBBAND_DebugAppliedDemoMode);
    UI_FillRect(16, 248, 783, 312, UI_COLOR_BACKGROUND);
    UI_DrawRateButton(UI_RATE_X0, 160, codec_active);
    UI_DrawRateButton(UI_RATE_X1, 240, codec_active);
    UI_DrawRateButton(UI_RATE_X2, 320, codec_active);
    SubbandUIFont_DrawPhrase(&Lcd_Context, SUBBAND_UI_PHRASE_LABEL_BITRATE,
                             18, 272, UI_COLOR_TEXT);
    estimated = (int)(SUBBAND_CODEC_LOOP_DebugEstimatedBitrateKbps + 0.5f);
    cursor = UI_AppendText(text, "SEL ");
    cursor = UI_AppendInt(cursor, SUBBAND_UI_DebugSelectedCodecKbps);
    cursor = UI_AppendText(cursor, "  TARGET ");
    cursor = UI_AppendInt(cursor, SUBBAND_CODEC_LOOP_DebugTargetKbps);
    cursor = UI_AppendText(cursor, "  EST ");
    cursor = UI_AppendInt(cursor, estimated);
    *cursor = '\0';
    UI_DrawAscii(text, 242, 296, ClrLightSteelBlue);
    SUBBAND_UI_DebugDisplayedTargetKbps =
        SUBBAND_CODEC_LOOP_DebugTargetKbps;
}

static void UI_DrawCountdown(void)
{
    char text[48];
    char *cursor;
    unsigned long remaining_ms;
    unsigned long target_hops;
    unsigned long learn_hops;
    unsigned long progress_width;
    int mode;

    mode = SUBBAND_DebugAppliedDemoMode;
    learn_hops = SUBBAND_DENOISE_DebugLearnHops;
    target_hops = SUBBAND_DENOISE_DebugTargetHops;
    remaining_ms = SubbandUI_RemainingMs(learn_hops, target_hops,
                                         SUBBAND_HOP,
                                         (unsigned long)SUBBAND_SAMPLE_RATE);
    SUBBAND_UI_DebugCountdownMs = (int)remaining_ms;
    UI_FillRect(17, 317, 782, 382, UI_COLOR_PANEL);
    UI_FillRect(24, 390, 775, 409, ClrDarkSlateGray);

    if (SubbandUI_ModeUsesLearning(mode) == 0)
    {
        SubbandUIFont_DrawPhrase(&Lcd_Context,
                                 SUBBAND_UI_PHRASE_LABEL_RUNNING,
                                 28, 338, UI_COLOR_TEXT);
        progress_width = 751UL;
    }
    else if (SUBBAND_DENOISE_DebugLearning != 0)
    {
        SubbandUIFont_DrawPhrase(&Lcd_Context,
                                 SUBBAND_UI_PHRASE_LABEL_LEARNING,
                                 28, 329, UI_COLOR_TEXT);
        SubbandUIFont_DrawPhrase(&Lcd_Context,
                                 SUBBAND_UI_PHRASE_LABEL_REMAINING,
                                 28, 357, UI_COLOR_TEXT);
        cursor = UI_AppendUnsigned(text, remaining_ms / 1000UL);
        cursor = UI_AppendText(cursor, ".");
        *cursor++ = (char)('0' + ((remaining_ms % 1000UL) / 100UL));
        cursor = UI_AppendText(cursor, " s");
        *cursor = '\0';
        UI_DrawAscii(text, 82, 354, ClrLightSkyBlue);
        progress_width = (target_hops > 0UL) ?
            (751UL * learn_hops) / target_hops : 0UL;
    }
    else if (SUBBAND_DENOISE_DebugReady != 0)
    {
        SubbandUIFont_DrawPhrase(&Lcd_Context,
                                 SUBBAND_UI_PHRASE_LABEL_READY,
                                 28, 338, ClrLimeGreen);
        progress_width = 751UL;
    }
    else
    {
        SubbandUIFont_DrawPhrase(&Lcd_Context,
                                 SUBBAND_UI_PHRASE_LABEL_LEARNING,
                                 28, 338, ClrLightSteelBlue);
        progress_width = 0UL;
    }

    if (progress_width > 751UL)
    {
        progress_width = 751UL;
    }
    if (progress_width > 0UL)
    {
        UI_FillRect(24, 390, 23 + (int)progress_width, 409,
                    UI_COLOR_PROGRESS);
    }
}

static int UI_ComputeAlgoLoad(void)
{
    unsigned long long numerator;
    unsigned long percent;

    numerator = (unsigned long long)SUBBAND_DebugMaxCycles * 100ULL;
    percent = (unsigned long)((numerator +
                               UI_FRAME_BUDGET_CYCLES / 2UL) /
                              UI_FRAME_BUDGET_CYCLES);
    if (percent > 999UL)
    {
        percent = 999UL;
    }
    return (int)percent;
}

static void UI_DrawLoad(void)
{
    char text[32];
    char *cursor;

    UI_FillRect(17, 421, 782, 462, UI_COLOR_PANEL);
    SubbandUIFont_DrawPhrase(&Lcd_Context, SUBBAND_UI_PHRASE_LABEL_LOAD,
                             28, 434, UI_COLOR_TEXT);
    cursor = UI_AppendInt(text, SUBBAND_UI_DebugAlgoLoadPercent);
    cursor = UI_AppendText(cursor, "%");
    *cursor = '\0';
    UI_DrawAscii(text, 150, 431, ClrLightSkyBlue);
}

static void UI_UpdateDirtyState(void)
{
    int codec_active;
    int load_percent;

    SUBBAND_UI_DebugSelectedMode = SUBBAND_DebugDemoMode;
    if (SUBBAND_UI_DebugDisplayedMode != SUBBAND_DebugAppliedDemoMode)
    {
        UI_MarkDirty(UI_DIRTY_MODE | UI_DIRTY_STATUS |
                     UI_DIRTY_COUNTDOWN | UI_DIRTY_RATE);
    }

    codec_active = SubbandUI_IsCodecMode(SUBBAND_DebugAppliedDemoMode);
    if ((codec_active != UI_LastCodecEnabled) ||
        (SUBBAND_UI_DebugSelectedCodecKbps != UI_LastSelectedKbps) ||
        (SUBBAND_CODEC_LOOP_DebugTargetKbps != UI_LastTargetKbps))
    {
        UI_LastCodecEnabled = codec_active;
        UI_LastSelectedKbps = SUBBAND_UI_DebugSelectedCodecKbps;
        UI_LastTargetKbps = SUBBAND_CODEC_LOOP_DebugTargetKbps;
        UI_MarkDirty(UI_DIRTY_RATE);
    }

    if ((SUBBAND_DENOISE_DebugLearning != UI_LastLearning) ||
        (SUBBAND_DENOISE_DebugReady != UI_LastReady) ||
        (SUBBAND_DENOISE_DebugLearnHops != UI_LastLearnHops) ||
        (SUBBAND_DENOISE_DebugTargetHops != UI_LastTargetHops))
    {
        UI_LastLearning = SUBBAND_DENOISE_DebugLearning;
        UI_LastReady = SUBBAND_DENOISE_DebugReady;
        UI_LastLearnHops = SUBBAND_DENOISE_DebugLearnHops;
        UI_LastTargetHops = SUBBAND_DENOISE_DebugTargetHops;
        UI_MarkDirty(UI_DIRTY_COUNTDOWN | UI_DIRTY_STATUS);
    }

#if SUBBAND_UI_SHOW_ALGO_LOAD
    load_percent = UI_ComputeAlgoLoad();
    if (load_percent != UI_LastLoadPercent)
    {
        UI_LastLoadPercent = load_percent;
        SUBBAND_UI_DebugAlgoLoadPercent = load_percent;
        UI_MarkDirty(UI_DIRTY_LOAD);
    }
#endif
}

static void UI_DrawOneDirtyRegion(unsigned long flag)
{
    switch (flag)
    {
    case UI_DIRTY_MODE: UI_DrawModeButtons(); break;
    case UI_DIRTY_STATUS: UI_DrawStatus(); break;
    case UI_DIRTY_COUNTDOWN: UI_DrawCountdown(); break;
    case UI_DIRTY_RATE: UI_DrawRate(); break;
    case UI_DIRTY_LOAD: UI_DrawLoad(); break;
    default: break;
    }
    UI_DirtyFlags &= ~flag;
    SUBBAND_UI_DebugDirtyFlags = UI_DirtyFlags;
    SUBBAND_UI_DebugRefreshCount++;
}

static void SubbandUI_OnModeButton(tWidget *widget)
{
    if (widget == (tWidget *)&UI_ModeButton0) FLAG_BUTTON_1 = 1U;
    else if (widget == (tWidget *)&UI_ModeButton1) FLAG_BUTTON_2 = 1U;
    else if (widget == (tWidget *)&UI_ModeButton2) FLAG_BUTTON_3 = 1U;
    else if (widget == (tWidget *)&UI_ModeButton3) FLAG_BUTTON_4 = 1U;
    else if (widget == (tWidget *)&UI_ModeButton4) FLAG_BUTTON_5 = 1U;
    else if (widget == (tWidget *)&UI_ModeButton5) FLAG_BUTTON_6 = 1U;
    else if (widget == (tWidget *)&UI_ModeButton6) FLAG_BUTTON_7 = 1U;
    else if (widget == (tWidget *)&UI_ModeButton7) FLAG_BUTTON_8 = 1U;
    FLAG_BUTTON = 1U;
}

static int UI_ModeFlagIndex(void)
{
    int index;

    index = -1;
    if (FLAG_BUTTON_1 != 0U) index = 0;
    else if (FLAG_BUTTON_2 != 0U) index = 1;
    else if (FLAG_BUTTON_3 != 0U) index = 2;
    else if (FLAG_BUTTON_4 != 0U) index = 3;
    else if (FLAG_BUTTON_5 != 0U) index = 4;
    else if (FLAG_BUTTON_6 != 0U) index = 5;
    else if (FLAG_BUTTON_7 != 0U) index = 6;
    else if (FLAG_BUTTON_8 != 0U) index = 7;

    FLAG_BUTTON = 0U;
    FLAG_BUTTON_1 = 0U;
    FLAG_BUTTON_2 = 0U;
    FLAG_BUTTON_3 = 0U;
    FLAG_BUTTON_4 = 0U;
    FLAG_BUTTON_5 = 0U;
    FLAG_BUTTON_6 = 0U;
    FLAG_BUTTON_7 = 0U;
    FLAG_BUTTON_8 = 0U;
    return index;
}

static int UI_HitRateButton(unsigned int touch_x, unsigned int touch_y)
{
    if ((touch_y < UI_RATE_Y) ||
        (touch_y >= UI_RATE_Y + UI_RATE_BUTTON_H))
    {
        return 0;
    }
    if ((touch_x >= UI_RATE_X0) &&
        (touch_x < UI_RATE_X0 + UI_RATE_BUTTON_W)) return 160;
    if ((touch_x >= UI_RATE_X1) &&
        (touch_x < UI_RATE_X1 + UI_RATE_BUTTON_W)) return 240;
    if ((touch_x >= UI_RATE_X2) &&
        (touch_x < UI_RATE_X2 + UI_RATE_BUTTON_W)) return 320;
    return 0;
}

void SubbandUI_Init(void)
{
    unsigned int index;

    if (UI_Initialized != 0U)
    {
        return;
    }
    Lcd_Init();
    Touch_Init();
    SubbandUI_LatchInit(&UI_RateLatch);
    SUBBAND_UI_DebugSelectedCodecKbps =
        SubbandUI_NormalizeCodecKbps(
            SUBBAND_DebugPersistentCodecKbps);
    SUBBAND_UI_DebugFontBytes = SubbandUIFont_StorageBytes();
    UI_DrawStaticBackground();
    for (index = 0U; index < 8U; index++)
    {
        WidgetAdd(WIDGET_ROOT, (tWidget *)UI_ModeButtons[index]);
    }
    UI_DrawModeButtons();
    UI_DrawStatus();
    UI_DrawRate();
    UI_DrawCountdown();
    UI_DrawLoad();
    UI_DirtyFlags = 0UL;
    SUBBAND_UI_DebugDirtyFlags = 0UL;
    UI_Initialized = 1U;
}

void SubbandUI_ServiceTouch(void)
{
    int mode_index;
    int requested_mode;
    int requested_kbps;
#if defined(__TI_COMPILER_VERSION__) || defined(__TMS320C6X__)
    unsigned int cycle_start;
    unsigned int cycle_end;
#endif

    if ((UI_Initialized == 0U) || (FLAG_TOUCH == 0U))
    {
        return;
    }
#if defined(__TI_COMPILER_VERSION__) || defined(__TMS320C6X__)
    cycle_start = TSCL;
#endif
    FLAG_TOUCH = 0U;
    Touch_Scan();
    SUBBAND_UI_DebugTouchCount++;

    mode_index = UI_ModeFlagIndex();
    if (mode_index >= 0)
    {
        requested_mode = SubbandUI_ModeForButton((unsigned int)mode_index);
        if (requested_mode != SUBBAND_DebugAppliedDemoMode)
        {
            SUBBAND_DebugDemoMode = requested_mode;
            SUBBAND_UI_DebugSelectedMode = requested_mode;
            SUBBAND_UI_DebugAcceptedTouchCount++;
            UI_MarkDirty(UI_DIRTY_STATUS);
        }
    }

    if (SubbandUI_LatchUpdate(&UI_RateLatch,
                              (Touch_Sta != 0U) ? 1 : 0) != 0)
    {
        requested_kbps = UI_HitRateButton(Touch_X, Touch_Y);
        if ((requested_kbps != 0) &&
            (SubbandUI_IsCodecMode(SUBBAND_DebugAppliedDemoMode) != 0) &&
            (requested_kbps != SUBBAND_UI_DebugSelectedCodecKbps))
        {
            SUBBAND_UI_DebugSelectedCodecKbps = requested_kbps;
            Subband_Request_Codec_Target(requested_kbps);
            SUBBAND_UI_DebugAcceptedTouchCount++;
            UI_MarkDirty(UI_DIRTY_RATE);
        }
    }

#if defined(__TI_COMPILER_VERSION__) || defined(__TMS320C6X__)
    cycle_end = TSCL;
    SUBBAND_UI_DebugLastTouchCycles =
        (unsigned long)(cycle_end - cycle_start);
    if (SUBBAND_UI_DebugLastTouchCycles >
        SUBBAND_UI_DebugMaxTouchCycles)
    {
        SUBBAND_UI_DebugMaxTouchCycles =
            SUBBAND_UI_DebugLastTouchCycles;
    }
#endif
}

void SubbandUI_ServiceDisplay(void)
{
    unsigned long frame;
    unsigned int regions_drawn;
    unsigned long flag;
#if defined(__TI_COMPILER_VERSION__) || defined(__TMS320C6X__)
    unsigned int cycle_start;
    unsigned int cycle_end;
#endif

    if (UI_Initialized == 0U)
    {
        return;
    }
    frame = SUBBAND_DebugAlgoFrames;
    if ((frame - UI_LastCountdownFrame) >= UI_COUNTDOWN_FRAME_INTERVAL)
    {
        UI_LastCountdownFrame = frame;
        UI_UpdateDirtyState();
    }
#if SUBBAND_UI_SHOW_ALGO_LOAD
    if ((frame - UI_LastLoadFrame) >= UI_LOAD_FRAME_INTERVAL)
    {
        UI_LastLoadFrame = frame;
        UI_MarkDirty(UI_DIRTY_LOAD);
    }
#endif
    if (UI_DirtyFlags == 0UL)
    {
        SUBBAND_UI_DebugSkippedRefreshes++;
        return;
    }

#if defined(__TI_COMPILER_VERSION__) || defined(__TMS320C6X__)
    cycle_start = TSCL;
#endif
    regions_drawn = 0U;
    for (flag = UI_DIRTY_MODE; flag <= UI_DIRTY_LOAD; flag <<= 1)
    {
        if ((UI_DirtyFlags & flag) != 0UL)
        {
            UI_DrawOneDirtyRegion(flag);
            regions_drawn++;
            if (regions_drawn >= 2U)
            {
                break;
            }
        }
    }
#if defined(__TI_COMPILER_VERSION__) || defined(__TMS320C6X__)
    cycle_end = TSCL;
    SUBBAND_UI_DebugLastDrawCycles =
        (unsigned long)(cycle_end - cycle_start);
    if (SUBBAND_UI_DebugLastDrawCycles > SUBBAND_UI_DebugMaxDrawCycles)
    {
        SUBBAND_UI_DebugMaxDrawCycles = SUBBAND_UI_DebugLastDrawCycles;
    }
#endif
}

void SubbandUI_NotifyModeChanged(void)
{
    UI_MarkDirty(UI_DIRTY_MODE | UI_DIRTY_STATUS |
                 UI_DIRTY_COUNTDOWN | UI_DIRTY_RATE);
}

#else

void SubbandUI_Init(void) {}
void SubbandUI_ServiceTouch(void) {}
void SubbandUI_ServiceDisplay(void) {}
void SubbandUI_NotifyModeChanged(void) {}

#endif
