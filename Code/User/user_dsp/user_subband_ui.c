#include "user_subband_ui.h"

#if SUBBAND_UI_ENABLE

#include "user_subband_ui_font.h"
#include "user_subband_ui_logic.h"
#include "user_subband_flow.h"
#include "user_subband_denoise.h"
#include "user_subband_codec_loopback.h"
#include "lcd_api.h"
#include "touch_api.h"

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

#define UI_CHAIN_X 28
#define UI_CHAIN_Y 84
#define UI_CHAIN_X_MAX 770
#define UI_CHAIN_Y_MIN 80
#define UI_CHAIN_Y_MAX 106
#define UI_CHAIN_TEXT_CAPACITY 96U

#define UI_PROGRESS_BLOCKS 10U
#define UI_PROGRESS_X 24
#define UI_PROGRESS_Y 390
#define UI_PROGRESS_BLOCK_W 68
#define UI_PROGRESS_BLOCK_H 20
#define UI_PROGRESS_BLOCK_GAP 7

#define UI_COLOR_BACKGROUND ClrBlack
#define UI_COLOR_PANEL ClrDarkSlateGray
#define UI_COLOR_TEXT ClrWhite
#define UI_COLOR_ACCENT ClrRoyalBlue
#define UI_COLOR_BUTTON ClrSteelBlue
#define UI_COLOR_DISABLED ClrDimGray
#define UI_COLOR_PROGRESS ClrLimeGreen

#define UI_COUNTDOWN_FRAME_INTERVAL 10UL
#define UI_LOAD_FRAME_INTERVAL 50UL
#define UI_FRAME_BUDGET_CYCLES 9338880UL
#define UI_LOAD_VALID_SAMPLES 5UL

#ifndef SUBBAND_UI_RUNTIME_DYNAMIC_DRAW
#define SUBBAND_UI_RUNTIME_DYNAMIC_DRAW 1
#endif

#define UI_DRAW_JOB_NONE      0UL
#define UI_DRAW_JOB_MODE      1UL
#define UI_DRAW_JOB_RATE      2UL
#define UI_DRAW_JOB_PROGRESS  3UL
#define UI_DRAW_JOB_COUNTDOWN 4UL
#define UI_DRAW_JOB_STATUS    5UL
#define UI_DRAW_JOB_LOAD      6UL
#define UI_DRAW_JOB_CHAIN     7UL

typedef struct
{
    short x;
    short y;
    short width;
    short height;
    int value;
    SubbandUIPhrase phrase;
} SubbandUIButtonDef;

typedef enum
{
    UI_TOUCH_IDLE = 0,
    UI_TOUCH_PRESSED
} SubbandUITouchState;

typedef enum
{
    UI_BUTTON_NONE = 0,
    UI_BUTTON_MODE,
    UI_BUTTON_RATE
} SubbandUIButtonType;

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
volatile int SUBBAND_UI_DebugAlgoLoadPercent = -1;
volatile unsigned long SUBBAND_UI_DebugSkippedRefreshes = 0UL;
volatile unsigned long SUBBAND_UI_DebugDirtyFlags = UI_DIRTY_ALL;
volatile unsigned long SUBBAND_UI_DebugFontBytes = 0UL;
volatile unsigned long SUBBAND_UI_DebugDrawOverBudgetCount = 0UL;
volatile unsigned long SUBBAND_UI_DebugLastDrawJob = UI_DRAW_JOB_NONE;
volatile unsigned long SUBBAND_UI_DebugMaxProgressDrawCycles = 0UL;
volatile unsigned long SUBBAND_UI_DebugMaxButtonDrawCycles = 0UL;
volatile unsigned long SUBBAND_UI_DebugMaxTextDrawCycles = 0UL;
volatile unsigned long SUBBAND_UI_DebugRollingCycles = 0UL;
volatile unsigned long SUBBAND_UI_DebugRollingLoadPercent = 0UL;
volatile unsigned long SUBBAND_UI_DebugLoadSampleCount = 0UL;
volatile int SUBBAND_UI_DebugDisplayedChainMode = -1;
volatile int SUBBAND_UI_DebugDisplayedChainKbps = 0;
volatile unsigned long SUBBAND_UI_DebugChainRefreshCount = 0UL;
volatile unsigned long SUBBAND_UI_DebugLastChainDrawCycles = 0UL;
volatile unsigned long SUBBAND_UI_DebugMaxChainDrawCycles = 0UL;
volatile unsigned long SUBBAND_UI_DebugChainDirtySetCount = 0UL;
volatile unsigned long SUBBAND_UI_DebugChainDrawCount = 0UL;
volatile unsigned long SUBBAND_UI_DebugChainDirtyWithoutDrawFrames = 0UL;
volatile unsigned long SUBBAND_UI_DebugLastDrawAlgoFrame = 0xFFFFFFFFUL;
volatile unsigned long SUBBAND_UI_DebugSkippedSameFrame = 0UL;
volatile unsigned long SUBBAND_UI_DebugMaxDrawJobsPerFrame = 0UL;
volatile unsigned long SUBBAND_UI_DebugSkippedDrawGap = 0UL;
volatile unsigned long SUBBAND_UI_DebugHoldoffSkipCount = 0UL;

static const SubbandUIButtonDef UI_ModeButtons[8] =
{
    {UI_MODE_X0, UI_MODE_Y0, UI_MODE_BUTTON_W, UI_MODE_BUTTON_H, 0,
     SUBBAND_UI_PHRASE_MODE_RAW},
    {UI_MODE_X1, UI_MODE_Y0, UI_MODE_BUTTON_W, UI_MODE_BUTTON_H, 1,
     SUBBAND_UI_PHRASE_MODE_WOLA},
    {UI_MODE_X2, UI_MODE_Y0, UI_MODE_BUTTON_W, UI_MODE_BUTTON_H, 2,
     SUBBAND_UI_PHRASE_MODE_BASIC},
    {UI_MODE_X3, UI_MODE_Y0, UI_MODE_BUTTON_W, UI_MODE_BUTTON_H, 4,
     SUBBAND_UI_PHRASE_MODE_MINSTAT},
    {UI_MODE_X0, UI_MODE_Y1, UI_MODE_BUTTON_W, UI_MODE_BUTTON_H, 6,
     SUBBAND_UI_PHRASE_MODE_MCRA},
    {UI_MODE_X1, UI_MODE_Y1, UI_MODE_BUTTON_W, UI_MODE_BUTTON_H, 7,
     SUBBAND_UI_PHRASE_MODE_STRONG},
    {UI_MODE_X2, UI_MODE_Y1, UI_MODE_BUTTON_W, UI_MODE_BUTTON_H, 8,
     SUBBAND_UI_PHRASE_MODE_FULL},
    {UI_MODE_X3, UI_MODE_Y1, UI_MODE_BUTTON_W, UI_MODE_BUTTON_H, 11,
     SUBBAND_UI_PHRASE_MODE_CODEC}
};

static const SubbandUIButtonDef UI_RateButtons[3] =
{
    {UI_RATE_X0, UI_RATE_Y, UI_RATE_BUTTON_W, UI_RATE_BUTTON_H, 160,
     SUBBAND_UI_PHRASE_LABEL_BITRATE},
    {UI_RATE_X1, UI_RATE_Y, UI_RATE_BUTTON_W, UI_RATE_BUTTON_H, 240,
     SUBBAND_UI_PHRASE_LABEL_BITRATE},
    {UI_RATE_X2, UI_RATE_Y, UI_RATE_BUTTON_W, UI_RATE_BUTTON_H, 320,
     SUBBAND_UI_PHRASE_LABEL_BITRATE}
};

static unsigned char UI_Initialized = 0U;
static unsigned long UI_DirtyFlags = UI_DIRTY_ALL;
static unsigned long UI_LastCountdownFrame = 0UL;
static unsigned long UI_LastLoadFrame = 0UL;
static int UI_LastLearning = -1;
static int UI_LastReady = -1;
static int UI_LastRemainingSeconds = -1;
static int UI_LastCodecEnabled = -1;
static int UI_LastSelectedKbps = -1;
static int UI_LastLoadPercent = -2;
static int UI_DisplayedAppliedMode = -1;
static int UI_PendingAppliedMode = -1;
static int UI_DisplayedChainMode = -1;
static int UI_DisplayedChainKbps = 0;
static unsigned int UI_ModeDirtyMask = 0U;
static unsigned int UI_RateDirtyMask = 0U;
#if SUBBAND_UI_PROGRESS_POLICY == SUBBAND_UI_PROGRESS_TEN_BLOCK
static int UI_DisplayedProgressStep = -1;
static int UI_TargetProgressStep = 0;
#endif
static unsigned long UI_LastDrawAlgoFrame = 0xFFFFFFFFUL;
static unsigned long UI_DrawHoldoffUntilFrame = 0UL;
static unsigned long UI_LastChainDirtyCountedFrame = 0xFFFFFFFFUL;
static SubbandUITouchState UI_TouchState = UI_TOUCH_IDLE;
static SubbandUIButtonType UI_PressedButtonType = UI_BUTTON_NONE;
static int UI_PressedButtonIndex = -1;
static unsigned int UI_LastTouchX = 0U;
static unsigned int UI_LastTouchY = 0U;

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

static void UI_DrawRect(int x_min, int y_min, int x_max, int y_max,
                        unsigned long color)
{
    tRectangle rect;

    UI_SetRect(&rect, x_min, y_min, x_max, y_max);
    GrContextForegroundSet(&Lcd_Context, color);
    GrRectDraw(&Lcd_Context, &rect);
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
                                  int x, int y, int width,
                                  unsigned long color)
{
    unsigned int phrase_width;
    int draw_x;

    phrase_width = SubbandUIFont_PhraseWidth(phrase);
    draw_x = x + (width - (int)phrase_width) / 2;
    SubbandUIFont_DrawPhrase(&Lcd_Context, phrase, draw_x, y, color);
}

static void UI_MarkDirty(unsigned long flags)
{
    UI_DirtyFlags |= flags;
    SUBBAND_UI_DebugDirtyFlags = UI_DirtyFlags;
}

static void UI_ClearDirty(unsigned long flags)
{
    UI_DirtyFlags &= ~flags;
    SUBBAND_UI_DebugDirtyFlags = UI_DirtyFlags;
}

static void UI_MarkChainDirty(void)
{
    if ((UI_DirtyFlags & UI_DIRTY_CHAIN) == 0UL)
    {
        SUBBAND_UI_DebugChainDirtySetCount++;
    }
    UI_MarkDirty(UI_DIRTY_CHAIN);
}

static void UI_RecordChainDirtyWait(unsigned long frame)
{
    if (((UI_DirtyFlags & UI_DIRTY_CHAIN) != 0UL) &&
        (frame != UI_LastChainDirtyCountedFrame))
    {
        UI_LastChainDirtyCountedFrame = frame;
        SUBBAND_UI_DebugChainDirtyWithoutDrawFrames++;
    }
}

static unsigned int UI_ButtonMaskBit(unsigned int index)
{
    return (unsigned int)(1U << index);
}

static int UI_FindModeButton(int mode)
{
    unsigned int index;

    for (index = 0U; index < 8U; index++)
    {
        if (UI_ModeButtons[index].value == mode)
        {
            return (int)index;
        }
    }
    return -1;
}

static int UI_FindRateButton(int kbps)
{
    unsigned int index;

    for (index = 0U; index < 3U; index++)
    {
        if (UI_RateButtons[index].value == kbps)
        {
            return (int)index;
        }
    }
    return -1;
}

static int UI_IsValidChainKbps(int kbps)
{
    return ((kbps == 160) || (kbps == 240) || (kbps == 320)) ? 1 : 0;
}

static int UI_CurrentChainKbps(int mode)
{
    int target_kbps;

    if (SubbandUI_IsCodecMode(mode) == 0)
    {
        return 0;
    }
    target_kbps = SUBBAND_CODEC_LOOP_DebugTargetKbps;
    if (UI_IsValidChainKbps(target_kbps) != 0)
    {
        return target_kbps;
    }
    return SubbandUI_NormalizeCodecKbps(SUBBAND_DebugPersistentCodecKbps);
}

static int UI_HitButton(const SubbandUIButtonDef *buttons,
                        unsigned int count,
                        unsigned int x,
                        unsigned int y)
{
    unsigned int index;

    for (index = 0U; index < count; index++)
    {
        const SubbandUIButtonDef *button;

        button = &buttons[index];
        if ((x >= (unsigned int)button->x) &&
            (x < (unsigned int)(button->x + button->width)) &&
            (y >= (unsigned int)button->y) &&
            (y < (unsigned int)(button->y + button->height)))
        {
            return (int)index;
        }
    }
    return -1;
}

static int UI_HitModeButton(unsigned int x, unsigned int y)
{
    return UI_HitButton(UI_ModeButtons, 8U, x, y);
}

static int UI_HitRateButton(unsigned int x, unsigned int y)
{
    return UI_HitButton(UI_RateButtons, 3U, x, y);
}

static void UI_DrawModeButton(unsigned int index, int selected)
{
    const SubbandUIButtonDef *button;
    unsigned long fill_color;
    unsigned long outline_color;

    if (index >= 8U)
    {
        return;
    }
    button = &UI_ModeButtons[index];
    fill_color = (selected != 0) ? UI_COLOR_ACCENT : UI_COLOR_BUTTON;
    outline_color = (selected != 0) ? ClrWhite : ClrLightSteelBlue;
    UI_FillRect(button->x, button->y,
                button->x + button->width - 1,
                button->y + button->height - 1,
                fill_color);
    UI_DrawRect(button->x, button->y,
                button->x + button->width - 1,
                button->y + button->height - 1,
                outline_color);
    UI_DrawCenteredPhrase(button->phrase, button->x,
                          button->y + 18, button->width, UI_COLOR_TEXT);
}

static void UI_DrawRateButton(unsigned int index, int selected,
                              int enabled)
{
    const SubbandUIButtonDef *button;
    char text[20];
    char *cursor;
    unsigned long fill_color;
    unsigned long outline_color;

    if (index >= 3U)
    {
        return;
    }
    button = &UI_RateButtons[index];
    if (enabled == 0)
    {
        fill_color = UI_COLOR_DISABLED;
    }
    else if (selected != 0)
    {
        fill_color = UI_COLOR_ACCENT;
    }
    else
    {
        fill_color = UI_COLOR_BUTTON;
    }
    outline_color = (selected != 0) ? ClrWhite : ClrLightSteelBlue;
    UI_FillRect(button->x, button->y,
                button->x + button->width - 1,
                button->y + button->height - 1,
                fill_color);
    UI_DrawRect(button->x, button->y,
                button->x + button->width - 1,
                button->y + button->height - 1,
                outline_color);
    cursor = UI_AppendInt(text, button->value);
    cursor = UI_AppendText(cursor, " kbps");
    *cursor = '\0';
    UI_DrawAscii(text, button->x + 39, button->y + 14, UI_COLOR_TEXT);
}

#if SUBBAND_UI_PROGRESS_POLICY == SUBBAND_UI_PROGRESS_TEN_BLOCK
static int UI_ComputeProgressStep(unsigned long learn_hops,
                                  unsigned long target_hops)
{
    unsigned long step;

    if (SubbandUI_ModeUsesLearning(SUBBAND_DebugAppliedDemoMode) == 0)
    {
        return (int)UI_PROGRESS_BLOCKS;
    }
    if (SUBBAND_DENOISE_DebugReady != 0)
    {
        return (int)UI_PROGRESS_BLOCKS;
    }
    if (target_hops == 0UL)
    {
        return 0;
    }
    step = (learn_hops * UI_PROGRESS_BLOCKS) / target_hops;
    if (step > UI_PROGRESS_BLOCKS)
    {
        step = UI_PROGRESS_BLOCKS;
    }
    return (int)step;
}

static void UI_SetProgressTarget(int step)
{
    if (step < 0)
    {
        step = 0;
    }
    if (step > (int)UI_PROGRESS_BLOCKS)
    {
        step = (int)UI_PROGRESS_BLOCKS;
    }
    if (UI_TargetProgressStep != step)
    {
        UI_TargetProgressStep = step;
        UI_MarkDirty(UI_DIRTY_PROGRESS);
    }
}

static void UI_DrawProgressBlock(unsigned int index, int filled)
{
    int x;
    unsigned long color;

    if (index >= UI_PROGRESS_BLOCKS)
    {
        return;
    }
    x = UI_PROGRESS_X +
        (int)index * (UI_PROGRESS_BLOCK_W + UI_PROGRESS_BLOCK_GAP);
    color = (filled != 0) ? UI_COLOR_PROGRESS : ClrDarkSlateGray;
    UI_FillRect(x, UI_PROGRESS_Y,
                x + UI_PROGRESS_BLOCK_W - 1,
                UI_PROGRESS_Y + UI_PROGRESS_BLOCK_H - 1,
                color);
    UI_DrawRect(x, UI_PROGRESS_Y,
                x + UI_PROGRESS_BLOCK_W - 1,
                UI_PROGRESS_Y + UI_PROGRESS_BLOCK_H - 1,
                ClrLightSteelBlue);
}

static void UI_DrawAllProgressBlocks(int step)
{
    unsigned int index;

    for (index = 0U; index < UI_PROGRESS_BLOCKS; index++)
    {
        UI_DrawProgressBlock(index, ((int)index < step) ? 1 : 0);
    }
    UI_DisplayedProgressStep = step;
}
#endif

static int UI_CurrentRemainingSeconds(void)
{
    if ((SubbandUI_ModeUsesLearning(SUBBAND_DebugAppliedDemoMode) == 0) ||
        (SUBBAND_DENOISE_DebugLearning == 0))
    {
        return 0;
    }
    return SubbandUI_RemainingSeconds(SUBBAND_DENOISE_DebugLearnHops,
                                      SUBBAND_DENOISE_DebugTargetHops,
                                      SUBBAND_HOP,
                                      (unsigned long)SUBBAND_SAMPLE_RATE);
}

static void UI_DrawLearningState(void)
{
    char text[8];
    char *cursor;
    int mode;
    int remaining_seconds;

    mode = SUBBAND_DebugAppliedDemoMode;
    remaining_seconds = UI_CurrentRemainingSeconds();
    SUBBAND_UI_DebugCountdownMs = remaining_seconds * 1000;
    UI_FillRect(28, 326, 222, 376, UI_COLOR_PANEL);
    if (SubbandUI_ModeUsesLearning(mode) == 0)
    {
        SubbandUIFont_DrawPhrase(&Lcd_Context,
                                 SUBBAND_UI_PHRASE_LABEL_RUNNING,
                                 28, 338, UI_COLOR_TEXT);
    }
    else if (SUBBAND_DENOISE_DebugLearning != 0)
    {
        SubbandUIFont_DrawPhrase(&Lcd_Context,
                                 SUBBAND_UI_PHRASE_LABEL_LEARNING,
                                 28, 329, UI_COLOR_TEXT);
        SubbandUIFont_DrawPhrase(&Lcd_Context,
                                 SUBBAND_UI_PHRASE_LABEL_REMAINING,
                                 28, 357, UI_COLOR_TEXT);
        cursor = UI_AppendInt(text, remaining_seconds);
        cursor = UI_AppendText(cursor, " s");
        *cursor = '\0';
        UI_DrawAscii(text, 82, 354, ClrLightSkyBlue);
    }
    else if (SUBBAND_DENOISE_DebugReady != 0)
    {
        SubbandUIFont_DrawPhrase(&Lcd_Context,
                                 SUBBAND_UI_PHRASE_LABEL_READY,
                                 28, 338, ClrLimeGreen);
    }
    else
    {
        SubbandUIFont_DrawPhrase(&Lcd_Context,
                                 SUBBAND_UI_PHRASE_LABEL_LEARNING,
                                 28, 338, ClrLightSteelBlue);
    }
}

static const tFont *UI_SelectChainFont(const char *text)
{
    const tFont *font;
    int available_width;

    available_width = UI_CHAIN_X_MAX - UI_CHAIN_X;
    font = &g_sFontCmss18b;
    GrContextFontSet(&Lcd_Context, font);
    if (GrStringWidthGet(&Lcd_Context, text, -1) <= available_width)
    {
        return font;
    }
    return &g_sFontCmtt12;
}

static void UI_DrawProcessingChain(void)
{
    char text[UI_CHAIN_TEXT_CAPACITY];
    const tFont *font;
    int applied_mode;
    int target_kbps;

    applied_mode = SUBBAND_DebugAppliedDemoMode;
    target_kbps = UI_CurrentChainKbps(applied_mode);
    (void)SubbandUI_BuildProcessingChain(text, sizeof(text),
                                         applied_mode, target_kbps);
    font = UI_SelectChainFont(text);
    UI_FillRect(UI_CHAIN_X, UI_CHAIN_Y_MIN,
                UI_CHAIN_X_MAX, UI_CHAIN_Y_MAX,
                UI_COLOR_PANEL);
    GrContextFontSet(&Lcd_Context, font);
    GrContextForegroundSet(&Lcd_Context, ClrLightSteelBlue);
    GrStringDraw(&Lcd_Context, text, -1, UI_CHAIN_X, UI_CHAIN_Y, 0);

    UI_DisplayedChainMode = applied_mode;
    UI_DisplayedChainKbps = target_kbps;
    SUBBAND_UI_DebugDisplayedChainMode = applied_mode;
    SUBBAND_UI_DebugDisplayedChainKbps = target_kbps;
    SUBBAND_UI_DebugDisplayedTargetKbps = target_kbps;
    SUBBAND_UI_DebugChainRefreshCount++;
    SUBBAND_UI_DebugChainDrawCount++;
    UI_ClearDirty(UI_DIRTY_CHAIN);
}

static int UI_ComputeAlgoLoad(void)
{
    unsigned long percent;

    if (SUBBAND_UI_DebugLoadSampleCount < UI_LOAD_VALID_SAMPLES)
    {
        return -1;
    }
    percent = SUBBAND_UI_DebugRollingLoadPercent;
    if (percent > 999UL)
    {
        percent = 999UL;
    }
    return (int)percent;
}

static void UI_DrawLoad(void)
{
    char text[16];
    char *cursor;
    int load_percent;

    load_percent = SUBBAND_UI_DebugAlgoLoadPercent;
    UI_FillRect(150, 431, 220, 451, UI_COLOR_PANEL);
    if (load_percent < 0)
    {
        UI_DrawAscii("--", 150, 431, ClrLightSkyBlue);
    }
    else
    {
        cursor = UI_AppendInt(text, load_percent);
        cursor = UI_AppendText(cursor, "%");
        *cursor = '\0';
        UI_DrawAscii(text, 150, 431, ClrLightSkyBlue);
    }
}

static void UI_DrawStatus(void)
{
    char text[48];
    char *cursor;
    int applied;
    int button_index;

    applied = SUBBAND_DebugAppliedDemoMode;
    UI_FillRect(72, 54, 240, 76, UI_COLOR_PANEL);
    UI_FillRect(650, 56, 760, 76, UI_COLOR_PANEL);
    cursor = UI_AppendInt(text, applied);
    *cursor = '\0';
    UI_DrawAscii(text, 72, 54, ClrLightSkyBlue);
    button_index = UI_FindModeButton(applied);
    if (button_index >= 0)
    {
        SubbandUIFont_DrawPhrase(&Lcd_Context,
                                 UI_ModeButtons[button_index].phrase,
                                 126, 56, UI_COLOR_TEXT);
    }
    SubbandUIFont_DrawPhrase(&Lcd_Context, SUBBAND_UI_PHRASE_LABEL_RUNNING,
                             650, 56, ClrLimeGreen);
}

static void UI_DrawStaticBackground(void)
{
    UI_FillRect(0, 0, 799, 479, UI_COLOR_BACKGROUND);
    UI_FillRect(0, 0, 799, 39, ClrMidnightBlue);
    UI_DrawCenteredPhrase(SUBBAND_UI_PHRASE_TITLE, 0, 11, 800,
                          UI_COLOR_TEXT);
    UI_FillRect(16, 44, 783, 115, UI_COLOR_PANEL);
    UI_FillRect(16, 316, 783, 383, UI_COLOR_PANEL);
    UI_FillRect(16, 420, 783, 463, UI_COLOR_PANEL);
    SubbandUIFont_DrawPhrase(&Lcd_Context, SUBBAND_UI_PHRASE_LABEL_MODE,
                             28, 56, UI_COLOR_TEXT);
    SubbandUIFont_DrawPhrase(&Lcd_Context, SUBBAND_UI_PHRASE_LABEL_BITRATE,
                             18, 272, UI_COLOR_TEXT);
    SubbandUIFont_DrawPhrase(&Lcd_Context, SUBBAND_UI_PHRASE_LABEL_LOAD,
                             28, 434, UI_COLOR_TEXT);
    SUBBAND_UI_DebugFullRedrawCount++;
}

static void UI_ScheduleAppliedModeRedraw(int applied)
{
    int old_index;
    int new_index;

    if (UI_PendingAppliedMode == applied)
    {
        return;
    }
    old_index = UI_FindModeButton(UI_DisplayedAppliedMode);
    new_index = UI_FindModeButton(applied);
    if (old_index >= 0)
    {
        UI_ModeDirtyMask |= UI_ButtonMaskBit((unsigned int)old_index);
    }
    if (new_index >= 0)
    {
        UI_ModeDirtyMask |= UI_ButtonMaskBit((unsigned int)new_index);
    }
    UI_PendingAppliedMode = applied;
    if (UI_ModeDirtyMask != 0U)
    {
        UI_MarkDirty(UI_DIRTY_MODE);
    }
    else
    {
        UI_DisplayedAppliedMode = applied;
        SUBBAND_UI_DebugDisplayedMode = applied;
    }
}

static void UI_ScheduleRateButtons(unsigned int mask)
{
    UI_RateDirtyMask |= (mask & 0x7U);
    if (UI_RateDirtyMask != 0U)
    {
        UI_MarkDirty(UI_DIRTY_RATE);
    }
}

static void UI_UpdateChainDirtyState(void)
{
    int applied;
    int target;

    applied = SUBBAND_DebugAppliedDemoMode;
    target = SUBBAND_CODEC_LOOP_DebugTargetKbps;
    if (UI_IsValidChainKbps(target) == 0)
    {
        target = UI_CurrentChainKbps(applied);
    }
    if ((UI_DisplayedChainMode != applied) ||
        ((SubbandUI_IsCodecMode(applied) != 0) &&
         (UI_DisplayedChainKbps != target)))
    {
        UI_MarkChainDirty();
    }
}

static void UI_UpdateDirtyState(void)
{
    int applied;
    int codec_active;
    int selected;
    int target;
    int old_rate_index;
    int new_rate_index;
    int remaining_seconds;
    int learning_state_changed;
    unsigned int rate_mask;

    SUBBAND_UI_DebugSelectedMode = SUBBAND_DebugDemoMode;
    applied = SUBBAND_DebugAppliedDemoMode;
    if (UI_DisplayedAppliedMode != applied)
    {
        UI_ScheduleAppliedModeRedraw(applied);
        UI_MarkDirty(UI_DIRTY_STATUS | UI_DIRTY_COUNTDOWN | UI_DIRTY_LOAD);
    }

    codec_active = SubbandUI_IsCodecMode(applied);
    selected = SUBBAND_UI_DebugSelectedCodecKbps;
    target = SUBBAND_CODEC_LOOP_DebugTargetKbps;
    if ((codec_active != 0) && (UI_IsValidChainKbps(target) != 0) &&
        (selected != target))
    {
        selected = target;
        SUBBAND_UI_DebugSelectedCodecKbps = target;
    }
    rate_mask = 0U;
    if (codec_active != UI_LastCodecEnabled)
    {
        rate_mask = 0x7U;
        UI_MarkDirty(UI_DIRTY_STATUS);
    }
    if (selected != UI_LastSelectedKbps)
    {
        old_rate_index = UI_FindRateButton(UI_LastSelectedKbps);
        new_rate_index = UI_FindRateButton(selected);
        if (old_rate_index >= 0)
        {
            rate_mask |= UI_ButtonMaskBit((unsigned int)old_rate_index);
        }
        if (new_rate_index >= 0)
        {
            rate_mask |= UI_ButtonMaskBit((unsigned int)new_rate_index);
        }
    }
    if (rate_mask != 0U)
    {
        UI_ScheduleRateButtons(rate_mask);
    }
    UI_LastCodecEnabled = codec_active;
    UI_LastSelectedKbps = selected;
    UI_UpdateChainDirtyState();

    remaining_seconds = UI_CurrentRemainingSeconds();
    learning_state_changed =
        ((SUBBAND_DENOISE_DebugLearning != UI_LastLearning) ||
         (SUBBAND_DENOISE_DebugReady != UI_LastReady)) ? 1 : 0;
    if ((learning_state_changed != 0) ||
        (remaining_seconds != UI_LastRemainingSeconds))
    {
        UI_LastLearning = SUBBAND_DENOISE_DebugLearning;
        UI_LastReady = SUBBAND_DENOISE_DebugReady;
        UI_LastRemainingSeconds = remaining_seconds;
        UI_MarkDirty(UI_DIRTY_COUNTDOWN);
    }
    if (learning_state_changed != 0)
    {
        UI_MarkDirty(UI_DIRTY_STATUS);
    }
#if SUBBAND_UI_PROGRESS_POLICY == SUBBAND_UI_PROGRESS_TEN_BLOCK
    UI_SetProgressTarget(
        UI_ComputeProgressStep(SUBBAND_DENOISE_DebugLearnHops,
                               SUBBAND_DENOISE_DebugTargetHops));
#endif
}

static void UI_UpdateLoadDirtyState(void)
{
#if SUBBAND_UI_SHOW_ALGO_LOAD
    int load_percent;
    unsigned long percent;

    percent = (SUBBAND_UI_DebugRollingCycles * 100UL +
               UI_FRAME_BUDGET_CYCLES / 2UL) /
              UI_FRAME_BUDGET_CYCLES;
    if (percent > 999UL)
    {
        percent = 999UL;
    }
    SUBBAND_UI_DebugRollingLoadPercent = percent;

    load_percent = UI_ComputeAlgoLoad();
    if (load_percent != UI_LastLoadPercent)
    {
        UI_LastLoadPercent = load_percent;
        SUBBAND_UI_DebugAlgoLoadPercent = load_percent;
        UI_MarkDirty(UI_DIRTY_LOAD);
    }
#endif
}

static unsigned long UI_DrawNextModeButton(void)
{
    unsigned int index;

    for (index = 0U; index < 8U; index++)
    {
        if ((UI_ModeDirtyMask & UI_ButtonMaskBit(index)) != 0U)
        {
            UI_ModeDirtyMask &= ~UI_ButtonMaskBit(index);
            UI_DrawModeButton(index,
                              (UI_ModeButtons[index].value ==
                               SUBBAND_DebugAppliedDemoMode) ? 1 : 0);
            if (UI_ModeDirtyMask == 0U)
            {
                UI_DisplayedAppliedMode = SUBBAND_DebugAppliedDemoMode;
                UI_PendingAppliedMode = UI_DisplayedAppliedMode;
                SUBBAND_UI_DebugDisplayedMode = UI_DisplayedAppliedMode;
                UI_ClearDirty(UI_DIRTY_MODE);
            }
            return UI_DRAW_JOB_MODE;
        }
    }
    UI_ClearDirty(UI_DIRTY_MODE);
    return UI_DRAW_JOB_NONE;
}

static unsigned long UI_DrawNextRateJob(void)
{
    unsigned int index;
    int codec_active;
    int selected;

    codec_active = SubbandUI_IsCodecMode(SUBBAND_DebugAppliedDemoMode);
    selected = SUBBAND_UI_DebugSelectedCodecKbps;
    for (index = 0U; index < 3U; index++)
    {
        if ((UI_RateDirtyMask & UI_ButtonMaskBit(index)) != 0U)
        {
            UI_RateDirtyMask &= ~UI_ButtonMaskBit(index);
            UI_DrawRateButton(index,
                              (UI_RateButtons[index].value == selected) ?
                              1 : 0,
                              codec_active);
            if (UI_RateDirtyMask == 0U)
            {
                UI_ClearDirty(UI_DIRTY_RATE);
            }
            return UI_DRAW_JOB_RATE;
        }
    }
    UI_ClearDirty(UI_DIRTY_RATE);
    return UI_DRAW_JOB_NONE;
}

#if SUBBAND_UI_PROGRESS_POLICY == SUBBAND_UI_PROGRESS_TEN_BLOCK
static unsigned long UI_DrawNextProgressJob(void)
{
    if (UI_DisplayedProgressStep < 0)
    {
        UI_DisplayedProgressStep = 0;
    }
    if (UI_DisplayedProgressStep < UI_TargetProgressStep)
    {
        UI_DrawProgressBlock((unsigned int)UI_DisplayedProgressStep, 1);
        UI_DisplayedProgressStep++;
    }
    else if (UI_DisplayedProgressStep > UI_TargetProgressStep)
    {
        UI_DisplayedProgressStep--;
        UI_DrawProgressBlock((unsigned int)UI_DisplayedProgressStep, 0);
    }
    if (UI_DisplayedProgressStep == UI_TargetProgressStep)
    {
        UI_ClearDirty(UI_DIRTY_PROGRESS);
    }
    return UI_DRAW_JOB_PROGRESS;
}
#endif

static unsigned long UI_DrawNextCountdownJob(void)
{
    UI_DrawLearningState();
    UI_ClearDirty(UI_DIRTY_COUNTDOWN);
    return UI_DRAW_JOB_COUNTDOWN;
}

static unsigned long UI_DrawOneJob(void)
{
    unsigned long job;

    if ((UI_DirtyFlags & UI_DIRTY_MODE) != 0UL)
    {
        job = UI_DrawNextModeButton();
        if (job != UI_DRAW_JOB_NONE) return job;
    }
    if ((UI_DirtyFlags & UI_DIRTY_STATUS) != 0UL)
    {
        UI_DrawStatus();
        UI_ClearDirty(UI_DIRTY_STATUS);
        return UI_DRAW_JOB_STATUS;
    }
    if ((UI_DirtyFlags & UI_DIRTY_CHAIN) != 0UL)
    {
        UI_DrawProcessingChain();
        return UI_DRAW_JOB_CHAIN;
    }
    if ((UI_DirtyFlags & UI_DIRTY_RATE) != 0UL)
    {
        job = UI_DrawNextRateJob();
        if (job != UI_DRAW_JOB_NONE) return job;
    }
#if SUBBAND_UI_PROGRESS_POLICY == SUBBAND_UI_PROGRESS_TEN_BLOCK
    if ((UI_DirtyFlags & UI_DIRTY_COUNTDOWN) != 0UL)
    {
        job = UI_DrawNextCountdownJob();
        if (job != UI_DRAW_JOB_NONE) return job;
    }
    if ((UI_DirtyFlags & UI_DIRTY_PROGRESS) != 0UL)
    {
        return UI_DrawNextProgressJob();
    }
#else
    if ((UI_DirtyFlags & UI_DIRTY_COUNTDOWN) != 0UL)
    {
        job = UI_DrawNextCountdownJob();
        if (job != UI_DRAW_JOB_NONE) return job;
    }
#endif
    if ((UI_DirtyFlags & UI_DIRTY_LOAD) != 0UL)
    {
        UI_DrawLoad();
        UI_ClearDirty(UI_DIRTY_LOAD);
        return UI_DRAW_JOB_LOAD;
    }
    return UI_DRAW_JOB_NONE;
}

static void UI_RecordDrawCycles(unsigned long job, unsigned long cycles)
{
    SUBBAND_UI_DebugLastDrawJob = job;
    SUBBAND_UI_DebugLastDrawCycles = cycles;
    if (cycles > SUBBAND_UI_DebugMaxDrawCycles)
    {
        SUBBAND_UI_DebugMaxDrawCycles = cycles;
    }
    if (cycles > SUBBAND_UI_RUNTIME_DRAW_BUDGET_CYCLES)
    {
        SUBBAND_UI_DebugDrawOverBudgetCount++;
    }
    if (job == UI_DRAW_JOB_PROGRESS)
    {
        if (cycles > SUBBAND_UI_DebugMaxProgressDrawCycles)
        {
            SUBBAND_UI_DebugMaxProgressDrawCycles = cycles;
        }
    }
    else if ((job == UI_DRAW_JOB_MODE) || (job == UI_DRAW_JOB_RATE))
    {
        if (cycles > SUBBAND_UI_DebugMaxButtonDrawCycles)
        {
            SUBBAND_UI_DebugMaxButtonDrawCycles = cycles;
        }
    }
    else if (job == UI_DRAW_JOB_CHAIN)
    {
        SUBBAND_UI_DebugLastChainDrawCycles = cycles;
        if (cycles > SUBBAND_UI_DebugMaxChainDrawCycles)
        {
            SUBBAND_UI_DebugMaxChainDrawCycles = cycles;
        }
    }
    else if (cycles > SUBBAND_UI_DebugMaxTextDrawCycles)
    {
        SUBBAND_UI_DebugMaxTextDrawCycles = cycles;
    }
}

static void UI_DrawInitialPage(void)
{
    unsigned int index;
    int codec_active;
    int selected;
#if SUBBAND_UI_PROGRESS_POLICY == SUBBAND_UI_PROGRESS_TEN_BLOCK
    int progress_step;
#endif

    codec_active = SubbandUI_IsCodecMode(SUBBAND_DebugAppliedDemoMode);
    selected = SUBBAND_UI_DebugSelectedCodecKbps;
    for (index = 0U; index < 8U; index++)
    {
        UI_DrawModeButton(index,
                          (UI_ModeButtons[index].value ==
                           SUBBAND_DebugAppliedDemoMode) ? 1 : 0);
    }
    for (index = 0U; index < 3U; index++)
    {
        UI_DrawRateButton(index,
                          (UI_RateButtons[index].value == selected) ? 1 : 0,
                          codec_active);
    }
#if SUBBAND_UI_PROGRESS_POLICY == SUBBAND_UI_PROGRESS_TEN_BLOCK
    progress_step = UI_ComputeProgressStep(SUBBAND_DENOISE_DebugLearnHops,
                                           SUBBAND_DENOISE_DebugTargetHops);
    UI_DrawAllProgressBlocks(progress_step);
    UI_TargetProgressStep = progress_step;
#endif
    UI_DrawStatus();
    UI_DrawProcessingChain();
    UI_DrawLearningState();
    UI_DrawLoad();
    UI_DisplayedAppliedMode = SUBBAND_DebugAppliedDemoMode;
    UI_PendingAppliedMode = UI_DisplayedAppliedMode;
    SUBBAND_UI_DebugDisplayedMode = UI_DisplayedAppliedMode;
    UI_LastLearning = SUBBAND_DENOISE_DebugLearning;
    UI_LastReady = SUBBAND_DENOISE_DebugReady;
    UI_LastRemainingSeconds = UI_CurrentRemainingSeconds();
}

static void UI_AcceptModeClick(unsigned int index)
{
    int requested_mode;

    if (index >= 8U)
    {
        return;
    }
    requested_mode = UI_ModeButtons[index].value;
    if ((requested_mode == SUBBAND_DebugAppliedDemoMode) ||
        (requested_mode == SUBBAND_DebugDemoMode))
    {
        return;
    }
    SUBBAND_DebugDemoMode = requested_mode;
    SUBBAND_UI_DebugSelectedMode = requested_mode;
    SUBBAND_UI_DebugAcceptedTouchCount++;
    UI_MarkDirty(UI_DIRTY_STATUS);
}

static void UI_AcceptRateClick(unsigned int index)
{
    int requested_kbps;

    if (index >= 3U)
    {
        return;
    }
    requested_kbps = UI_RateButtons[index].value;
    if ((SubbandUI_IsCodecMode(SUBBAND_DebugAppliedDemoMode) == 0) ||
        (requested_kbps == SUBBAND_UI_DebugSelectedCodecKbps))
    {
        return;
    }
    SUBBAND_UI_DebugSelectedCodecKbps = requested_kbps;
    Subband_Request_Codec_Target(requested_kbps);
    SUBBAND_UI_DebugAcceptedTouchCount++;
    UI_ScheduleRateButtons(0x7U);
}

static void UI_HandleAcceptedClick(SubbandUIButtonType type, int index)
{
    if (type == UI_BUTTON_MODE)
    {
        UI_AcceptModeClick((unsigned int)index);
    }
    else if (type == UI_BUTTON_RATE)
    {
        UI_AcceptRateClick((unsigned int)index);
    }
}

void SubbandUI_RecordAlgoCycles(unsigned long cycles)
{
    unsigned long rolling;

    rolling = SUBBAND_UI_DebugRollingCycles;
    if (SUBBAND_UI_DebugLoadSampleCount == 0UL)
    {
        rolling = cycles;
    }
    else
    {
        rolling = (rolling * 7UL + cycles + 4UL) / 8UL;
    }
    SUBBAND_UI_DebugRollingCycles = rolling;
    if (SUBBAND_UI_DebugLoadSampleCount < 0xFFFFFFFFUL)
    {
        SUBBAND_UI_DebugLoadSampleCount++;
    }
}

void SubbandUI_ResetLoadWindow(void)
{
    SUBBAND_UI_DebugRollingCycles = 0UL;
    SUBBAND_UI_DebugRollingLoadPercent = 0UL;
    SUBBAND_UI_DebugLoadSampleCount = 0UL;
    SUBBAND_UI_DebugAlgoLoadPercent = -1;
    UI_LastLoadPercent = -2;
    UI_MarkDirty(UI_DIRTY_LOAD);
}

void SubbandUI_Init(void)
{
    if (UI_Initialized != 0U)
    {
        return;
    }
    Lcd_Init();
    Touch_Init();
    SUBBAND_UI_DebugSelectedCodecKbps =
        SubbandUI_NormalizeCodecKbps(SUBBAND_DebugPersistentCodecKbps);
    SUBBAND_UI_DebugFontBytes = SubbandUIFont_StorageBytes();
    UI_DrawStaticBackground();
    UI_DrawInitialPage();
    UI_DirtyFlags = 0UL;
    UI_ModeDirtyMask = 0U;
    UI_RateDirtyMask = 0U;
    UI_DisplayedChainMode = SUBBAND_DebugAppliedDemoMode;
    UI_DisplayedChainKbps =
        UI_CurrentChainKbps(SUBBAND_DebugAppliedDemoMode);
    SUBBAND_UI_DebugDirtyFlags = 0UL;
    UI_LastDrawAlgoFrame = 0xFFFFFFFFUL;
    UI_DrawHoldoffUntilFrame = 0UL;
    UI_LastChainDirtyCountedFrame = 0xFFFFFFFFUL;
    SUBBAND_UI_DebugLastDrawAlgoFrame = 0xFFFFFFFFUL;
    SUBBAND_UI_DebugSkippedSameFrame = 0UL;
    SUBBAND_UI_DebugMaxDrawJobsPerFrame = 0UL;
    SUBBAND_UI_DebugSkippedDrawGap = 0UL;
    SUBBAND_UI_DebugHoldoffSkipCount = 0UL;
    UI_Initialized = 1U;
}

void SubbandUI_ServiceTouch(unsigned char force_scan)
{
    TouchScanResult result;
    SubbandUIButtonType press_type;
    SubbandUIButtonType release_type;
    int press_button;
    int release_button;
#if defined(__TI_COMPILER_VERSION__) || defined(__TMS320C6X__)
    unsigned int cycle_start;
    unsigned int cycle_end;
#endif

    if (UI_Initialized == 0U)
    {
        return;
    }
    if ((FLAG_TOUCH == 0U) && (force_scan == 0U))
    {
        return;
    }
#if defined(__TI_COMPILER_VERSION__) || defined(__TMS320C6X__)
    cycle_start = TSCL;
#endif
    FLAG_TOUCH = 0U;
    result = Touch_ScanRaw();
    SUBBAND_UI_DebugTouchCount++;

    if (result == TOUCH_SCAN_DOWN)
    {
        UI_LastTouchX = Touch_X;
        UI_LastTouchY = Touch_Y;
        if (UI_TouchState == UI_TOUCH_IDLE)
        {
            press_type = UI_BUTTON_NONE;
            press_button = UI_HitModeButton(Touch_X, Touch_Y);
            if (press_button >= 0)
            {
                press_type = UI_BUTTON_MODE;
            }
            else
            {
                press_button = UI_HitRateButton(Touch_X, Touch_Y);
                if (press_button >= 0)
                {
                    press_type = UI_BUTTON_RATE;
                }
            }
            UI_PressedButtonType = press_type;
            UI_PressedButtonIndex = press_button;
            UI_TouchState = UI_TOUCH_PRESSED;
        }
    }
    else if (result == TOUCH_SCAN_RELEASE)
    {
        release_type = UI_BUTTON_NONE;
        release_button = UI_HitModeButton(UI_LastTouchX, UI_LastTouchY);
        if (release_button >= 0)
        {
            release_type = UI_BUTTON_MODE;
        }
        else
        {
            release_button = UI_HitRateButton(UI_LastTouchX, UI_LastTouchY);
            if (release_button >= 0)
            {
                release_type = UI_BUTTON_RATE;
            }
        }
        if ((UI_TouchState == UI_TOUCH_PRESSED) &&
            (release_type == UI_PressedButtonType) &&
            (release_button == UI_PressedButtonIndex))
        {
            UI_HandleAcceptedClick(release_type, release_button);
        }
        UI_TouchState = UI_TOUCH_IDLE;
        UI_PressedButtonType = UI_BUTTON_NONE;
        UI_PressedButtonIndex = -1;
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
    unsigned long job;
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
        UI_UpdateLoadDirtyState();
    }
#endif
    if (UI_DirtyFlags == 0UL)
    {
        SUBBAND_UI_DebugSkippedRefreshes++;
        return;
    }
#if SUBBAND_UI_RUNTIME_DYNAMIC_DRAW == 0
    SUBBAND_UI_DebugSkippedRefreshes++;
    UI_RecordChainDirtyWait(frame);
    return;
#else
    if ((UI_LastDrawAlgoFrame != 0xFFFFFFFFUL) &&
        (frame == UI_LastDrawAlgoFrame))
    {
        SUBBAND_UI_DebugSkippedSameFrame++;
        UI_RecordChainDirtyWait(frame);
        return;
    }
    if ((UI_LastDrawAlgoFrame != 0xFFFFFFFFUL) &&
        ((frame - UI_LastDrawAlgoFrame) <
         SUBBAND_UI_RUNTIME_DRAW_GAP_FRAMES))
    {
        SUBBAND_UI_DebugSkippedDrawGap++;
        UI_RecordChainDirtyWait(frame);
        return;
    }
    if ((UI_DrawHoldoffUntilFrame != 0UL) &&
        ((long)(frame - UI_DrawHoldoffUntilFrame) < 0L))
    {
        SUBBAND_UI_DebugHoldoffSkipCount++;
        UI_RecordChainDirtyWait(frame);
        return;
    }
#if defined(__TI_COMPILER_VERSION__) || defined(__TMS320C6X__)
    cycle_start = TSCL;
#endif
    job = UI_DrawOneJob();
#if defined(__TI_COMPILER_VERSION__) || defined(__TMS320C6X__)
    cycle_end = TSCL;
    UI_RecordDrawCycles(job, (unsigned long)(cycle_end - cycle_start));
#else
    UI_RecordDrawCycles(job, 0UL);
#endif
    if (job != UI_DRAW_JOB_NONE)
    {
        SUBBAND_UI_DebugRefreshCount++;
        UI_LastDrawAlgoFrame = frame;
        SUBBAND_UI_DebugLastDrawAlgoFrame = frame;
        if (SUBBAND_UI_DebugMaxDrawJobsPerFrame < 1UL)
        {
            SUBBAND_UI_DebugMaxDrawJobsPerFrame = 1UL;
        }
    }
    UI_RecordChainDirtyWait(frame);
#endif
}

void SubbandUI_NotifyModeChanged(void)
{
    if (UI_Initialized == 0U)
    {
        UI_MarkDirty(UI_DIRTY_ALL);
        return;
    }
    UI_ScheduleAppliedModeRedraw(SUBBAND_DebugAppliedDemoMode);
    UI_ScheduleRateButtons(0x7U);
#if SUBBAND_UI_PROGRESS_POLICY == SUBBAND_UI_PROGRESS_TEN_BLOCK
    UI_SetProgressTarget(UI_ComputeProgressStep(SUBBAND_DENOISE_DebugLearnHops,
                                                SUBBAND_DENOISE_DebugTargetHops));
#endif
    UI_LastLearning = -1;
    UI_LastReady = -1;
    UI_LastRemainingSeconds = -1;
    UI_MarkChainDirty();
    UI_MarkDirty(UI_DIRTY_STATUS | UI_DIRTY_COUNTDOWN | UI_DIRTY_LOAD);
    UI_DrawHoldoffUntilFrame = SUBBAND_DebugAlgoFrames +
                               SUBBAND_UI_MODE_CHANGE_HOLDOFF_FRAMES;
}

#else

void SubbandUI_Init(void) {}
void SubbandUI_ServiceTouch(unsigned char force_scan) { (void)force_scan; }
void SubbandUI_ServiceDisplay(void) {}
void SubbandUI_NotifyModeChanged(void) {}
void SubbandUI_RecordAlgoCycles(unsigned long cycles) { (void)cycles; }
void SubbandUI_ResetLoadWindow(void) {}

#endif
