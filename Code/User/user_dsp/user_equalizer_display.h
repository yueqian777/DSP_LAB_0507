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

extern volatile unsigned long EQ_DebugLcdRefreshCount;
extern volatile unsigned long EQ_DebugLcdSkipBusyCount;
extern volatile unsigned long EQ_DebugLcdGainRedrawCount;
extern volatile unsigned long EQ_DebugLcdStatusRedrawCount;
extern volatile int EQ_DebugLcdEnabled;
extern volatile int EQ_DebugLcdLastMode;

void EqualizerDisplay_Init(void);
void EqualizerDisplay_DrawStaticLayout(void);
void EqualizerDisplay_UpdateAll(const EQ_STATE *st);
void EqualizerDisplay_UpdateGains(const EQ_STATE *st);
void EqualizerDisplay_UpdateStatus(unsigned long frames,
                                   float last_ms,
                                   float max_ms,
                                   unsigned long clip_count,
                                   int mode);

#endif /* _USER_EQUALIZER_DISPLAY_H_ */
