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

#define SUBBAND_DENOISE_TRACK_FIXED    0
#define SUBBAND_DENOISE_TRACK_MINSTAT  1
#define SUBBAND_DENOISE_TRACK_HYBRID   2
#define SUBBAND_DENOISE_TRACK_MCRA     3

#ifndef SUBBAND_DENOISE_TRACK_DEFAULT
#define SUBBAND_DENOISE_TRACK_DEFAULT SUBBAND_DENOISE_TRACK_FIXED
#endif

#ifndef SUBBAND_DENOISE_MS_NUM_BLOCKS
#define SUBBAND_DENOISE_MS_NUM_BLOCKS 8
#endif

#ifndef SUBBAND_DENOISE_MS_BLOCK_HOPS
#define SUBBAND_DENOISE_MS_BLOCK_HOPS 49
#endif

#ifndef SUBBAND_DENOISE_MS_POWER_ALPHA
#define SUBBAND_DENOISE_MS_POWER_ALPHA 0.92f
#endif

#ifndef SUBBAND_DENOISE_MS_BIAS
#define SUBBAND_DENOISE_MS_BIAS 1.40f
#endif

#ifndef SUBBAND_DENOISE_MS_NOISE_UP_ALPHA
#define SUBBAND_DENOISE_MS_NOISE_UP_ALPHA 0.90f
#endif

#ifndef SUBBAND_DENOISE_MS_NOISE_DOWN_ALPHA
#define SUBBAND_DENOISE_MS_NOISE_DOWN_ALPHA 0.995f
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
extern volatile int SUBBAND_DENOISE_DebugNoiseTrackMode;
extern volatile unsigned long SUBBAND_DENOISE_DebugMsUpdateCount;
extern volatile float SUBBAND_DENOISE_DebugMsNoisePsdAvg;
extern volatile float SUBBAND_DENOISE_DebugMsMinPsdAvg;
extern volatile float SUBBAND_DENOISE_DebugMsBias;
extern volatile float SUBBAND_DENOISE_DebugMsWindowSeconds;
extern volatile float SUBBAND_DENOISE_DebugMcraSpeechProbAvg;
extern volatile float SUBBAND_DENOISE_DebugMcraOverdriveAvg;
extern volatile float SUBBAND_DENOISE_DebugMcraDeltaLow;
extern volatile float SUBBAND_DENOISE_DebugMcraDeltaHigh;
extern volatile float SUBBAND_DENOISE_DebugMcraAlphaNoise;
extern volatile float SUBBAND_DENOISE_DebugMcraAlphaSpeech;
extern volatile float SUBBAND_DENOISE_DebugMcraFloorAvg;
extern volatile int SUBBAND_DENOISE_DebugMcraStrongMode;

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
void SubbandDenoise_SetNoiseTrackMode(int mode);
int SubbandDenoise_GetNoiseTrackMode(void);
void SubbandDenoise_ResetNoiseTracker(void);
void SubbandDenoise_SetMcraParams(float delta_low,
                                  float delta_high,
                                  float alpha_noise,
                                  float alpha_speech,
                                  float overdrive_speech,
                                  float overdrive_noise,
                                  float bias,
                                  int strong_mode);
void SubbandDenoise_ResetMcraState(void);
void SubbandDenoise_ProcessSpectrum(float *re, float *im);

unsigned long SubbandDenoise_GetLearnCount(void);
unsigned long SubbandDenoise_GetTargetHops(void);
float SubbandDenoise_GetNoisePsdAvg(void);
float SubbandDenoise_GetGainAvg(void);
float SubbandDenoise_GetGainMin(void);
float SubbandDenoise_GetGainMax(void);
float SubbandDenoise_GetFade(void);

#endif /* _USER_SUBBAND_DENOISE_H_ */
