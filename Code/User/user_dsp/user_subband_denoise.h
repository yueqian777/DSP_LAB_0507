/**
 * user_subband_denoise.h
 *
 * Fixed-noise-learning Wiener denoise module for the WOLA subband path.
 */

#ifndef _USER_SUBBAND_DENOISE_H_
#define _USER_SUBBAND_DENOISE_H_

#include "user_subband_wola.h"

#ifndef SUBBAND_DENOISE_ENABLE_DEFAULT
#define SUBBAND_DENOISE_ENABLE_DEFAULT 0
#endif

#ifndef SUBBAND_DENOISE_AUTO_LEARN_ON_INIT
#define SUBBAND_DENOISE_AUTO_LEARN_ON_INIT 1
#endif

#ifndef SUBBAND_DENOISE_AUTO_ENABLE_AFTER_LEARN
#define SUBBAND_DENOISE_AUTO_ENABLE_AFTER_LEARN 1
#endif

#ifndef SUBBAND_DENOISE_DEFAULT_LEARN_SECONDS
#define SUBBAND_DENOISE_DEFAULT_LEARN_SECONDS 2.0f
#endif

#ifndef SUBBAND_DENOISE_EPS
#define SUBBAND_DENOISE_EPS 1.0f
#endif

#ifndef SUBBAND_DENOISE_ALPHA
#define SUBBAND_DENOISE_ALPHA 0.96f
#endif

#ifndef SUBBAND_DENOISE_GAIN_FLOOR
#define SUBBAND_DENOISE_GAIN_FLOOR 0.15f
#endif

#ifndef SUBBAND_DENOISE_GAIN_SMOOTH_UP
#define SUBBAND_DENOISE_GAIN_SMOOTH_UP 0.85f
#endif

#ifndef SUBBAND_DENOISE_GAIN_SMOOTH_DOWN
#define SUBBAND_DENOISE_GAIN_SMOOTH_DOWN 0.60f
#endif

#ifndef SUBBAND_DENOISE_FREQ_SMOOTH
#define SUBBAND_DENOISE_FREQ_SMOOTH 1
#endif

#ifndef SUBBAND_DENOISE_FADE_SECONDS
#define SUBBAND_DENOISE_FADE_SECONDS 0.5f
#endif

extern volatile int SUBBAND_DENOISE_DebugEnabled;
extern volatile int SUBBAND_DENOISE_DebugLearning;
extern volatile int SUBBAND_DENOISE_DebugReady;
extern volatile unsigned long SUBBAND_DENOISE_DebugLearnHops;
extern volatile unsigned long SUBBAND_DENOISE_DebugTargetHops;
extern volatile float SUBBAND_DENOISE_DebugLearnProgress;
extern volatile float SUBBAND_DENOISE_DebugInputPowerAvg;
extern volatile float SUBBAND_DENOISE_DebugOutputPowerAvg;
extern volatile float SUBBAND_DENOISE_DebugGainAvg;
extern volatile float SUBBAND_DENOISE_DebugMinGain;
extern volatile float SUBBAND_DENOISE_DebugMaxGain;
extern volatile float SUBBAND_DENOISE_DebugNoisePsdAvg;
extern volatile float SUBBAND_DENOISE_DebugFade;

void SubbandDenoise_Init(void);
void SubbandDenoise_Reset(void);
void SubbandDenoise_SetEnabled(int enable);
int SubbandDenoise_IsEnabled(void);
void SubbandDenoise_StartNoiseLearning(void);
void SubbandDenoise_StopLearning(void);
int SubbandDenoise_IsLearning(void);
int SubbandDenoise_IsReady(void);
float SubbandDenoise_GetLearnProgress(void);
void SubbandDenoise_SetParams(float alpha, float gain_floor,
                              float gain_smooth_up, float gain_smooth_down);
void SubbandDenoise_ProcessSpectrum(float *re, float *im);

unsigned long SubbandDenoise_GetLearnCount(void);
unsigned long SubbandDenoise_GetTargetHops(void);
float SubbandDenoise_GetNoisePsdAvg(void);
float SubbandDenoise_GetGainAvg(void);
float SubbandDenoise_GetGainMin(void);
float SubbandDenoise_GetGainMax(void);
float SubbandDenoise_GetFade(void);

#endif /* _USER_SUBBAND_DENOISE_H_ */
