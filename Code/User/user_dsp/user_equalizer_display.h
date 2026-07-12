/**
 * user_equalizer_display.h
 *
 * Non-invasive LCD display helper for Project 3.3 graphic equalizer.
 */

#ifndef _USER_EQUALIZER_DISPLAY_H_
#define _USER_EQUALIZER_DISPLAY_H_

#include "user_equalizer.h"

#ifndef EQ_ENABLE_LCD_DISPLAY
#define EQ_ENABLE_LCD_DISPLAY 0
#endif

/*
 * Use the local 16x16 bitmap glyph path by default. This does not depend on
 * Grlib Chinese font support and keeps source strings ASCII-only.
 */
#ifndef EQ_LCD_USE_CHINESE
#define EQ_LCD_USE_CHINESE 1
#endif

#ifndef EQ_LCD_REFRESH_FRAMES
#define EQ_LCD_REFRESH_FRAMES 25UL
#endif

#define EQ_LCD_RUNTIME_STATUS 0x01U
#define EQ_LCD_RUNTIME_GAINS  0x02U

#define EQ_LCD_JOB_NONE       0
#define EQ_LCD_JOB_MODE       1
#define EQ_LCD_JOB_FRAMES     2
#define EQ_LCD_JOB_ALGO_LAST  3
#define EQ_LCD_JOB_ALGO_MAX   4
#define EQ_LCD_JOB_CLIP       5
#define EQ_LCD_JOB_BAND_0     6
#define EQ_LCD_JOB_BAND_9     (EQ_LCD_JOB_BAND_0 + 9)
#define EQ_LCD_JOB_COUNT      15

#define EQ_LCD_AUTO_DISABLE_JOB_OVER_5MS 0x01UL
#define EQ_LCD_AUTO_DISABLE_LATENCY_MISS 0x02UL
#define EQ_LCD_AUTO_DISABLE_OVERLAP      0x04UL
#define EQ_LCD_AUTO_DISABLE_DROPPED      0x08UL

#define EQ_LCD_JOB_TARGET_CYCLES 912000UL
#define EQ_LCD_JOB_HARD_CYCLES   2280000UL
#define EQ_LCD_STATUS_MAX_TENTHS 999999UL

#define EQ_LCD_CATEGORY_BAND      0
#define EQ_LCD_CATEGORY_MODE      1
#define EQ_LCD_CATEGORY_FRAMES    2
#define EQ_LCD_CATEGORY_ALGO_LAST 3
#define EQ_LCD_CATEGORY_ALGO_MAX  4
#define EQ_LCD_CATEGORY_CLIP      5
#define EQ_LCD_CATEGORY_COUNT     6

extern volatile unsigned long EQ_DebugLcdRefreshCount;
extern volatile unsigned long EQ_DebugLcdSkipBusyCount;
extern volatile unsigned long EQ_DebugLcdGainRedrawCount;
extern volatile unsigned long EQ_DebugLcdStatusRedrawCount;
extern volatile int EQ_DebugLcdEnabled;
extern volatile int EQ_DebugLcdLastMode;
extern volatile unsigned int EQ_DebugLcdRuntimeMask;
#define EQ_LcdRuntimeMask EQ_DebugLcdRuntimeMask
extern volatile unsigned long EQ_DebugLcdPendingMask;
extern volatile unsigned long EQ_DebugLcdJobCount;
extern volatile unsigned long EQ_DebugLcdDeferredAudioCount;
extern volatile unsigned long EQ_DebugLcdAudioArrivedDuringDrawCount;
extern volatile unsigned long EQ_DebugLcdUnexpectedFullRedrawCount;
extern volatile unsigned long EQ_DebugLcdBudgetExceededCount;
extern volatile unsigned long EQ_DebugLcdLastJobCycles;
extern volatile unsigned long EQ_DebugLcdMaxJobCycles;
extern volatile unsigned long EQ_DebugLcdLastJobTenthsMs;
extern volatile unsigned long EQ_DebugLcdMaxJobTenthsMs;
extern volatile float EQ_DebugLcdLastJobMs;
extern volatile float EQ_DebugLcdMaxJobMs;
extern volatile int EQ_DebugLcdLastJob;
extern volatile unsigned long EQ_DebugLcdAutoDisabledCount;
extern volatile unsigned long EQ_DebugLcdAutoDisableReason;
extern volatile unsigned long EQ_DebugLcdCategoryCount[EQ_LCD_CATEGORY_COUNT];
extern volatile unsigned long EQ_DebugLcdCategoryLastCycles[EQ_LCD_CATEGORY_COUNT];
extern volatile unsigned long EQ_DebugLcdCategoryMaxCycles[EQ_LCD_CATEGORY_COUNT];

void EqualizerDisplay_Init(void);
void EqualizerDisplay_DrawStaticLayout(void);
void EqualizerDisplay_BeginRuntime(void);
void EqualizerDisplay_RequestGains(const EQ_STATE *st);
void EqualizerDisplay_RequestStatus(unsigned long frames,
                                    float algo_last_ms,
                                    float algo_max_ms,
                                    unsigned long clip_count,
                                    int requested_mode,
                                    int transition_target_mode,
                                    int applied_mode);
int EqualizerDisplay_HasPendingJob(void);
int EqualizerDisplay_ServiceOneJob(void);
void EqualizerDisplay_CancelRuntimeJobs(void);
void EqualizerDisplay_AutoDisable(unsigned long reason);
void EqualizerDisplay_UpdateAll(const EQ_STATE *st);
void EqualizerDisplay_UpdateGains(const EQ_STATE *st);
void EqualizerDisplay_UpdateStatus(unsigned long frames,
                                   float last_ms,
                                   float max_ms,
                                   unsigned long clip_count,
                                   int mode);

#endif /* _USER_EQUALIZER_DISPLAY_H_ */
