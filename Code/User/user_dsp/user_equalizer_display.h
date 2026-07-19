/**
 * user_equalizer_display.h
 *
 * Audio-safe LCD renderer for the Project 3.3 status UI.
 */

#ifndef _USER_EQUALIZER_DISPLAY_H_
#define _USER_EQUALIZER_DISPLAY_H_

#include "user_equalizer_ui_logic.h"

#ifndef EQ_ENABLE_LCD_DISPLAY
#define EQ_ENABLE_LCD_DISPLAY 0
#endif

#ifndef EQ_ENABLE_PROJECT33_TOUCH
#define EQ_ENABLE_PROJECT33_TOUCH 0
#endif

#if (EQ_ENABLE_PROJECT33_TOUCH != 0) && (EQ_ENABLE_LCD_DISPLAY == 0)
#error Project 3.3 Touch requires the Project 3.3 LCD UI.
#endif

#if (EQ_ENABLE_TEN_BAND_EDITOR != 0) && \
    (EQ_ENABLE_LCD_DISPLAY == 0) && !defined(EQ_ALGO_ONLY)
#error Project 3.3 ten-band editor requires the Project 3.3 LCD UI.
#endif

#if (EQ_ENABLE_TEN_BAND_EDITOR_TOUCH != 0) && \
    ((EQ_ENABLE_TEN_BAND_EDITOR == 0) || \
     (EQ_ENABLE_PROJECT33_TOUCH == 0))
#error Ten-band editor Touch requires both editor and Project 3.3 Touch.
#endif

#ifndef EQ_LCD_USE_CHINESE
#define EQ_LCD_USE_CHINESE 1
#endif

#ifndef EQ_UI_RUNTIME_DEFAULT_MASK
#define EQ_UI_RUNTIME_DEFAULT_MASK 0U
#endif

#ifndef EQ_LCD_DIAGNOSTIC_ALIGNMENT_PATTERN
#define EQ_LCD_DIAGNOSTIC_ALIGNMENT_PATTERN 0
#endif

#ifndef EQ_UI_TOUCH_SWAP_XY
#define EQ_UI_TOUCH_SWAP_XY 0U
#endif

#ifndef EQ_UI_TOUCH_FLIP_X
#define EQ_UI_TOUCH_FLIP_X 0U
#endif

#ifndef EQ_UI_TOUCH_FLIP_Y
#define EQ_UI_TOUCH_FLIP_Y 0U
#endif

#ifndef EQ_UI_TOUCH_RAW_X_MIN
#define EQ_UI_TOUCH_RAW_X_MIN 0U
#endif

#ifndef EQ_UI_TOUCH_RAW_X_MAX
#define EQ_UI_TOUCH_RAW_X_MAX 799U
#endif

#ifndef EQ_UI_TOUCH_RAW_Y_MIN
#define EQ_UI_TOUCH_RAW_Y_MIN 0U
#endif

#ifndef EQ_UI_TOUCH_RAW_Y_MAX
#define EQ_UI_TOUCH_RAW_Y_MAX 479U
#endif

#define EQ_LCD_MIN_JOB_GAP_FRAMES 2UL
#define EQ_LCD_CONTROL_QUIET_FRAMES 3UL

#define EQ_LCD_JOB_NONE EQ_UI_JOB_NONE
#define EQ_LCD_JOB_COUNT EQ_UI_JOB_COUNT

#define EQ_LCD_AUTO_DISABLE_JOB_OVER_5MS 0x01UL
#define EQ_LCD_AUTO_DISABLE_LATENCY_MISS 0x02UL
#define EQ_LCD_AUTO_DISABLE_OVERLAP      0x04UL
#define EQ_LCD_AUTO_DISABLE_DROPPED      0x08UL
#define EQ_LCD_AUTO_DISABLE_HW_FAULT     0x10UL
#define EQ_LCD_AUTO_DISABLE_CANARY       0x20UL

#define EQ_LCD_JOB_GOAL_CYCLES       456000UL
#define EQ_LCD_JOB_ACCEPTANCE_CYCLES 912000UL
#define EQ_LCD_JOB_TARGET_CYCLES     EQ_LCD_JOB_ACCEPTANCE_CYCLES
#define EQ_LCD_JOB_HARD_CYCLES       2280000UL
#define EQ_LCD_HW_AUDIT_JOB_INTERVAL 64UL
#define EQ_LCD_HW_AUDIT_FRAME_INTERVAL 256UL

#define EQ_LCD_CATEGORY_PRESET   0
#define EQ_LCD_CATEGORY_DYNAMIC  1
#define EQ_LCD_CATEGORY_CHAIN    2
#define EQ_LCD_CATEGORY_ANALYZER 3
#if EQ_ENABLE_TEN_BAND_EDITOR != 0
#define EQ_LCD_CATEGORY_EDITOR   4
#define EQ_LCD_CATEGORY_PAGE     5
#define EQ_LCD_CATEGORY_COUNT    6
#else
#define EQ_LCD_CATEGORY_COUNT    4
#endif

#define EQ_LCD_ANALYZER_STRIP_NONE          0U
#define EQ_LCD_ANALYZER_STRIP_CLEAR         1U
#define EQ_LCD_ANALYZER_STRIP_POSITIVE_FILL 2U
#define EQ_LCD_ANALYZER_STRIP_NEGATIVE_FILL 3U

typedef struct
{
    unsigned long frame_base;
    unsigned long frame_end;
    unsigned long raster_control;
    unsigned long raster_status;
    unsigned long dma_control;
    unsigned long irq_status;
} EQ_LCD_HW_SNAPSHOT;

#if defined(EQ_ALGO_ONLY) || (EQ_ENABLE_LCD_DISPLAY != 0)
extern volatile unsigned long EQ_DebugLcdRefreshCount;
extern volatile unsigned long EQ_DebugLcdSkipBusyCount;
extern volatile int EQ_DebugLcdEnabled;
extern volatile unsigned int EQ_DebugLcdRuntimeMask;
#define EQ_LcdRuntimeMask EQ_DebugLcdRuntimeMask
extern volatile unsigned long EQ_DebugLcdPendingMask;
extern volatile unsigned long EQ_DebugLcdJobCount;
extern volatile unsigned long EQ_DebugLcdDeferredAudioCount;
extern volatile unsigned long EQ_DebugLcdAudioArrivedDuringDrawCount;
extern volatile unsigned long EQ_DebugLcdUnexpectedFullRedrawCount;
extern volatile unsigned long EQ_DebugLcdStaticDrawCount;
extern volatile unsigned long EQ_DebugLcdBoundsFailureCount;
extern volatile unsigned long EQ_DebugLcdBudgetExceededCount;
extern volatile unsigned long EQ_DebugLcdHardBudgetExceededCount;
extern volatile unsigned long EQ_DebugLcdOver1msCount;
extern volatile unsigned long EQ_DebugLcdOver2msCount;
extern volatile unsigned long EQ_DebugLcdOver5msCount;
extern volatile unsigned long EQ_DebugLcdLastJobStartCycles;
extern volatile unsigned long EQ_DebugLcdLastJobEndCycles;
extern volatile unsigned long EQ_DebugLcdLastJobCycles;
extern volatile unsigned long EQ_DebugLcdMaxJobCycles;
extern volatile unsigned long EQ_DebugLcdLastJobTenthsMs;
extern volatile unsigned long EQ_DebugLcdMaxJobTenthsMs;
extern volatile int EQ_DebugLcdLastJob;
extern volatile unsigned long EQ_DebugLcdAutoDisabledCount;
extern volatile unsigned long EQ_DebugLcdAutoDisableReason;
extern volatile const unsigned int EQ_DebugLcdCategoryCountSize;
extern volatile const unsigned int EQ_DebugLcdJobTypeCountSize;
extern volatile const unsigned int EQ_DebugUiJobCountSize;
extern volatile const unsigned int EQ_DebugDynamicHitboxCountSize;
extern volatile const unsigned int EQ_DebugEditorHitboxCountSize;
extern volatile unsigned long EQ_DebugLcdCategoryCount[EQ_LCD_CATEGORY_COUNT];
extern volatile unsigned long
    EQ_DebugLcdCategoryLastCycles[EQ_LCD_CATEGORY_COUNT];
extern volatile unsigned long
    EQ_DebugLcdCategoryMaxCycles[EQ_LCD_CATEGORY_COUNT];
extern volatile unsigned long EQ_DebugLcdJobTypeCount[EQ_LCD_JOB_COUNT];
extern volatile unsigned long EQ_DebugLcdJobTypeLastCycles[EQ_LCD_JOB_COUNT];
extern volatile unsigned long EQ_DebugLcdJobTypeMaxCycles[EQ_LCD_JOB_COUNT];
extern volatile int EQ_DebugLcdAnalyzerLastBand;
extern volatile unsigned int EQ_DebugLcdAnalyzerLastField;
extern volatile int EQ_DebugLcdAnalyzerLastStripY;
extern volatile unsigned int EQ_DebugLcdAnalyzerLastStripHeight;
extern volatile unsigned int EQ_DebugLcdAnalyzerLastStripOperation;
extern volatile unsigned int EQ_DebugLcdAnalyzerMaxStripHeight;
extern volatile unsigned long EQ_DebugLcdAnalyzerStripCount;
extern volatile unsigned long EQ_DebugLcdAnalyzerValueCount;
extern volatile const unsigned long EQ_DebugUiStateBytes;
extern volatile unsigned long EQ_DebugLcdExpectedFrameBase;
extern volatile unsigned long EQ_DebugLcdExpectedFrameEnd;
extern volatile unsigned long EQ_DebugLcdBufferAddress;
extern volatile unsigned long EQ_DebugLcdBufferEndAddress;
extern volatile unsigned long EQ_DebugLcdCurrentFrameBase;
extern volatile unsigned long EQ_DebugLcdCurrentFrameEnd;
extern volatile unsigned long EQ_DebugLcdRasterFaultCount;
extern volatile unsigned long EQ_DebugLcdSyncLostCount;
extern volatile unsigned long EQ_DebugLcdFifoUnderflowCount;
extern volatile unsigned long EQ_DebugLcdFrameAddressMismatchCount;
extern volatile unsigned long EQ_DebugLcdLastRasterStatus;
extern volatile unsigned long EQ_DebugLcdLastIrqStatus;
extern volatile unsigned long EQ_DebugLcdStartupRasterStatus;
extern volatile unsigned long EQ_DebugLcdStartupStatusAfterClear;
extern volatile unsigned long EQ_DebugLcdStartupFaultClearCount;
extern volatile unsigned long EQ_DebugLcdFirstFaultFrame;
extern volatile unsigned long EQ_DebugLcdFirstFaultJob;
extern volatile unsigned long EQ_DebugLcdFramebufferCanaryCheckCount;
extern volatile unsigned long EQ_DebugLcdFramebufferCanaryFailureCount;
extern volatile unsigned long EQ_DebugLcdFramebufferFirstFailureFrame;
extern volatile unsigned long EQ_DebugLcdFramebufferFirstFailureJob;
extern volatile unsigned int EQ_DebugLcdFaultLatched;
extern volatile unsigned int EQ_DebugLcdHardwareAuditRequest;
extern volatile EQ_LCD_HW_SNAPSHOT EQ_DebugLcdHwSnapshot;
extern volatile const unsigned int EQ_DebugLcdAlignmentPatternEnabled;
#endif

void EqualizerDisplay_Init(void);
int EqualizerDisplay_DrawStaticLayout(void);
void EqualizerDisplay_BeginRuntime(void);
void EqualizerDisplay_RequestSnapshot(const EQ_UI_SNAPSHOT *snapshot,
                                      unsigned long process_frame);
int EqualizerDisplay_GetDisplayedPage(void);
int EqualizerDisplay_IsPageBuilding(void);
int EqualizerDisplay_HasPendingJob(void);
int EqualizerDisplay_HasEligibleJob(unsigned long process_frame);
int EqualizerDisplay_ServiceOneJob(unsigned long process_frame);
void EqualizerDisplay_AuditHardware(unsigned long process_frame, int force);
void EqualizerDisplay_CancelRuntimeJobs(void);
void EqualizerDisplay_AutoDisable(unsigned long reason);

#if defined(EQ_ALGO_ONLY)
unsigned long EqualizerDisplay_TestPrimitiveCount(void);
int EqualizerDisplay_TestTraceOpen(const char *path);
void EqualizerDisplay_TestTraceClose(void);
unsigned long EqualizerDisplay_TestTraceRecordCount(void);
void EqualizerDisplay_TestForceJobCycles(unsigned long cycles);
void EqualizerDisplay_TestSetHardwareSnapshot(
    const EQ_LCD_HW_SNAPSHOT *snapshot);
void EqualizerDisplay_TestSetCanaryFailure(int failed);
void EqualizerDisplay_TestDrawRect(int x, int y, int w, int h);
#endif

#endif /* _USER_EQUALIZER_DISPLAY_H_ */
