#ifndef _USER_SUBBAND_UI_H_
#define _USER_SUBBAND_UI_H_

#define SUBBAND_UI_LCD_WIDTH 800
#define SUBBAND_UI_LCD_HEIGHT 480

#ifndef SUBBAND_UI_ENABLE
#define SUBBAND_UI_ENABLE 1
#endif

#ifndef SUBBAND_UI_SHOW_ALGO_LOAD
#define SUBBAND_UI_SHOW_ALGO_LOAD 1
#endif

#define UI_DIRTY_MODE       0x01UL
#define UI_DIRTY_STATUS     0x02UL
#define UI_DIRTY_COUNTDOWN  0x04UL
#define UI_DIRTY_RATE       0x08UL
#define UI_DIRTY_LOAD       0x10UL
#define UI_DIRTY_ALL        0x1FUL

extern volatile unsigned long SUBBAND_UI_DebugTouchCount;
extern volatile unsigned long SUBBAND_UI_DebugAcceptedTouchCount;
extern volatile unsigned long SUBBAND_UI_DebugRefreshCount;
extern volatile unsigned long SUBBAND_UI_DebugFullRedrawCount;
extern volatile unsigned long SUBBAND_UI_DebugLastDrawCycles;
extern volatile unsigned long SUBBAND_UI_DebugMaxDrawCycles;
extern volatile unsigned long SUBBAND_UI_DebugLastTouchCycles;
extern volatile unsigned long SUBBAND_UI_DebugMaxTouchCycles;
extern volatile int SUBBAND_UI_DebugSelectedMode;
extern volatile int SUBBAND_UI_DebugDisplayedMode;
extern volatile int SUBBAND_UI_DebugSelectedCodecKbps;
extern volatile int SUBBAND_UI_DebugDisplayedTargetKbps;
extern volatile int SUBBAND_UI_DebugCountdownMs;
extern volatile int SUBBAND_UI_DebugAlgoLoadPercent;
extern volatile unsigned long SUBBAND_UI_DebugSkippedRefreshes;
extern volatile unsigned long SUBBAND_UI_DebugDirtyFlags;
extern volatile unsigned long SUBBAND_UI_DebugFontBytes;

void SubbandUI_Init(void);
void SubbandUI_ServiceTouch(void);
void SubbandUI_ServiceDisplay(void);
void SubbandUI_NotifyModeChanged(void);

#endif
