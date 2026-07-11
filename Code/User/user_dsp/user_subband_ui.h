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

#define SUBBAND_UI_PROGRESS_DISABLED  0
#define SUBBAND_UI_PROGRESS_COARSE    1
#define SUBBAND_UI_PROGRESS_TEN_BLOCK 2

#ifndef SUBBAND_UI_PROGRESS_POLICY
#define SUBBAND_UI_PROGRESS_POLICY SUBBAND_UI_PROGRESS_COARSE
#endif

#ifndef SUBBAND_UI_RUNTIME_DRAW_GAP_FRAMES
#define SUBBAND_UI_RUNTIME_DRAW_GAP_FRAMES 2UL
#endif

#ifndef SUBBAND_UI_MODE_CHANGE_HOLDOFF_FRAMES
#define SUBBAND_UI_MODE_CHANGE_HOLDOFF_FRAMES 3UL
#endif

#define UI_DIRTY_MODE       0x01UL
#define UI_DIRTY_STATUS     0x02UL
#define UI_DIRTY_COUNTDOWN  0x04UL
#define UI_DIRTY_RATE       0x08UL
#define UI_DIRTY_LOAD       0x10UL
#define UI_DIRTY_PROGRESS   0x20UL
#define UI_DIRTY_CHAIN      0x40UL
#define UI_DIRTY_REMAINING_DIGIT 0x80UL
#define UI_DIRTY_ALL        0xFFUL

#define SUBBAND_UI_RUNTIME_DRAW_BUDGET_CYCLES 912000UL

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
extern volatile unsigned long SUBBAND_UI_DebugDrawOverBudgetCount;
extern volatile unsigned long SUBBAND_UI_DebugLastDrawJob;
extern volatile unsigned long SUBBAND_UI_DebugMaxProgressDrawCycles;
extern volatile unsigned long SUBBAND_UI_DebugMaxButtonDrawCycles;
extern volatile unsigned long SUBBAND_UI_DebugMaxTextDrawCycles;
extern volatile unsigned long SUBBAND_UI_DebugRollingCycles;
extern volatile unsigned long SUBBAND_UI_DebugRollingLoadPercent;
extern volatile unsigned long SUBBAND_UI_DebugLoadSampleCount;
extern volatile int SUBBAND_UI_DebugDisplayedChainMode;
extern volatile int SUBBAND_UI_DebugDisplayedChainKbps;
extern volatile unsigned long SUBBAND_UI_DebugChainRefreshCount;
extern volatile unsigned long SUBBAND_UI_DebugLastChainDrawCycles;
extern volatile unsigned long SUBBAND_UI_DebugMaxChainDrawCycles;
extern volatile unsigned long SUBBAND_UI_DebugChainDirtySetCount;
extern volatile unsigned long SUBBAND_UI_DebugChainDrawCount;
extern volatile unsigned long SUBBAND_UI_DebugChainDirtyWithoutDrawFrames;
extern volatile unsigned long SUBBAND_UI_DebugLastDrawAlgoFrame;
extern volatile unsigned long SUBBAND_UI_DebugSkippedSameFrame;
extern volatile unsigned long SUBBAND_UI_DebugMaxDrawJobsPerFrame;
extern volatile unsigned long SUBBAND_UI_DebugSkippedDrawGap;
extern volatile unsigned long SUBBAND_UI_DebugHoldoffSkipCount;
extern volatile unsigned long SUBBAND_UI_DebugLearningStateDrawCount;
extern volatile unsigned long SUBBAND_UI_DebugRemainingDigitDrawCount;
extern volatile unsigned long SUBBAND_UI_DebugLastLearningStateDrawCycles;
extern volatile unsigned long SUBBAND_UI_DebugMaxLearningStateDrawCycles;
extern volatile unsigned long SUBBAND_UI_DebugLastRemainingDigitDrawCycles;
extern volatile unsigned long SUBBAND_UI_DebugMaxRemainingDigitDrawCycles;
extern volatile unsigned long SUBBAND_UI_DebugCancelledDigitJobs;

void SubbandUI_Init(void);
void SubbandUI_ServiceTouch(unsigned char force_scan);
void SubbandUI_ServiceDisplay(void);
void SubbandUI_NotifyModeChanged(void);
void SubbandUI_RecordAlgoCycles(unsigned long cycles);
void SubbandUI_ResetLoadWindow(void);

#endif
