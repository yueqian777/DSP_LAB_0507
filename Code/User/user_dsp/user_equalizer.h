/**
 * user_equalizer.h
 *
 * Project 3.3 graphic equalizer algorithm interface.
 */

#ifndef _USER_EQUALIZER_H_
#define _USER_EQUALIZER_H_

#ifndef EQ_NUM_BANDS
#define EQ_NUM_BANDS 10
#endif

#ifndef EQ_SAMPLE_RATE
#define EQ_SAMPLE_RATE 50000.0f
#endif

#ifndef EQ_FRAME_LEN
#ifdef EQ_ALGO_ONLY
#define EQ_FRAME_LEN 1024
#else
#include "adc_api.h"
#define EQ_FRAME_LEN ADC_SAMPLE_1024
#endif
#endif

#ifndef EQ_FADE_SECONDS
#define EQ_FADE_SECONDS 0.1f
#endif

#define EQ_GAIN_MIN_DB (-18.0f)
#define EQ_GAIN_MAX_DB (12.0f)

#define EQ_PRESET_FLAT         0
#define EQ_PRESET_BASS_BOOST   1
#define EQ_PRESET_VOCAL        2
#define EQ_PRESET_TREBLE_BOOST 3
#define EQ_PRESET_V_SHAPE      4

typedef struct
{
    float x1[EQ_NUM_BANDS];
    float x2[EQ_NUM_BANDS];
    float y1[EQ_NUM_BANDS];
    float y2[EQ_NUM_BANDS];
    float current_gain[EQ_NUM_BANDS];
    float target_gain[EQ_NUM_BANDS];
    float gain_step[EQ_NUM_BANDS];
    unsigned long clip_count;
} EQ_STATE;

extern volatile unsigned long EQ_DebugClipCount;

void Equalizer_Init(EQ_STATE *st);
void Equalizer_Reset(EQ_STATE *st);
void Equalizer_SetBandGainDb(EQ_STATE *st, int band, float gain_db);
void Equalizer_SetAllGainsDb(EQ_STATE *st,
                             const float gains_db[EQ_NUM_BANDS]);
void Equalizer_ApplyPreset(EQ_STATE *st, int preset);
void Equalizer_ProcessFrame(EQ_STATE *st, const short *in, short *out, int n);
void Equalizer_ProcessFrameFloat(EQ_STATE *st, const float *in,
                                 float *out, int n);

float Equalizer_GetBandCenterHz(int band);
float Equalizer_GetBandTargetGainDb(const EQ_STATE *st, int band);
float Equalizer_GetBandMagnitudeDb(int band, float freq_hz);

#endif /* _USER_EQUALIZER_H_ */
