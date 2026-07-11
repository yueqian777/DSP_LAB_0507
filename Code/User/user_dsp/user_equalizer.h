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

#ifndef EQ_TRANSITION_MS
#define EQ_TRANSITION_MS 120.0f
#endif

#ifndef EQ_ENABLE_BANK_CROSSFADE
#define EQ_ENABLE_BANK_CROSSFADE 1
#endif

#define EQ_GAIN_MIN_DB (-18.0f)
#define EQ_GAIN_MAX_DB (12.0f)

#define EQ_PRESET_FLAT         0
#define EQ_PRESET_BASS_BOOST   1
#define EQ_PRESET_VOCAL        2
#define EQ_PRESET_TREBLE_BOOST 3
#define EQ_PRESET_V_SHAPE      4
#define EQ_PRESET_COUNT        5
#define EQ_PRESET_CUSTOM       EQ_PRESET_COUNT

#define EQ_CORE_RAW_COPY       0
#define EQ_CORE_FLOAT_COPY     1
#define EQ_CORE_LEGACY         2
#define EQ_CORE_RBJ_CASCADE    3

#define EQ_SECTION_LEGACY_BANDPASS 0
#define EQ_SECTION_LOW_SHELF       1
#define EQ_SECTION_PEAKING         2
#define EQ_SECTION_HIGH_SHELF      3

/* Historical fixed-Q preset bank: keep for comparison, not board default. */
#define EQ_LEGACY_UNSAFE_PRESET 1

typedef struct
{
    float b0;
    float b1;
    float b2;
    float a1;
    float a2;
} EQ_BIQUAD;

typedef struct
{
    EQ_BIQUAD section[EQ_NUM_BANDS];
    float s1[EQ_NUM_BANDS];
    float s2[EQ_NUM_BANDS];
    float predicted_peak_db;
    float preamp_db;
    float preamp_gain;
} EQ_FILTER_BANK;

typedef struct
{
    int core;
    int section;
    int type;
    float f0_hz;
    float gain_db;
    float q_or_bw;
    float b0;
    float b1;
    float b2;
    float a1;
    float a2;
    float pole_radius_1;
    float pole_radius_2;
} EQ_SECTION_INFO;

typedef struct
{
    /* Preserved fixed-Q parallel band-pass legacy state. */
    float legacy_x1[EQ_NUM_BANDS];
    float legacy_x2[EQ_NUM_BANDS];
    float legacy_y1[EQ_NUM_BANDS];
    float legacy_y2[EQ_NUM_BANDS];
    float current_gain[EQ_NUM_BANDS];
    float target_gain[EQ_NUM_BANDS];
    float gain_step[EQ_NUM_BANDS];

    /* The requested gain surface is shared by legacy and RBJ cores. */
    float band_gain_db[EQ_NUM_BANDS];
    EQ_FILTER_BANK active_bank;
    EQ_FILTER_BANK pending_bank;
    int pending_bank_valid;
    int transition_remaining;
    int transition_total;

    unsigned long clip_count;
    int bypass;
    int core_mode;
    int preset;
    int rbj_bank_id;
    int latest_preset;
    int latest_preset_valid;
} EQ_STATE;

extern volatile unsigned long EQ_DebugClipCount;
extern volatile float EQ_DebugPredictedPeakDb;
extern volatile float EQ_DebugPreampDb;
extern volatile float EQ_DebugFloatCopyMaxError;
extern volatile unsigned long EQ_DebugHeadroomScanCount;
extern volatile unsigned long EQ_DebugRawCopyMismatchCount;
extern volatile float EQ_DebugInputPeak;
extern volatile float EQ_DebugOutputPeak;

void Equalizer_Init(EQ_STATE *st);
void Equalizer_Reset(EQ_STATE *st);
void Equalizer_SetBypass(EQ_STATE *st, int enable);
int Equalizer_GetBypass(const EQ_STATE *st);
void Equalizer_SetCoreMode(EQ_STATE *st, int mode);
int Equalizer_GetCoreMode(const EQ_STATE *st);
void Equalizer_SetBandGainDb(EQ_STATE *st, int band, float gain_db);
void Equalizer_SetAllGainsDb(EQ_STATE *st,
                             const float gains_db[EQ_NUM_BANDS]);
void Equalizer_ApplyPreset(EQ_STATE *st, int preset);
void Equalizer_ApplySingleBand1kPlus3Db(EQ_STATE *st);
void Equalizer_ProcessFrame(EQ_STATE *st, const short *in, short *out, int n);
void Equalizer_ProcessFrameFloat(EQ_STATE *st, const float *in,
                                 float *out, int n);

float Equalizer_GetBandCenterHz(int band);
float Equalizer_GetBandTargetGainDb(const EQ_STATE *st, int band);
float Equalizer_GetBandMagnitudeDb(int band, float freq_hz);
float Equalizer_GetPredictedPeakDb(const EQ_STATE *st);
float Equalizer_GetPreampDb(const EQ_STATE *st);
int Equalizer_IsFlat(const EQ_STATE *st);
int Equalizer_HasPendingTransition(const EQ_STATE *st);
int Equalizer_GetTransitionRemaining(const EQ_STATE *st);
int Equalizer_GetCoefficientInfo(const EQ_STATE *st, int section,
                                 EQ_SECTION_INFO *info);
void Equalizer_GetSystemResponse(const EQ_STATE *st, float freq_hz,
                                 float *real_out, float *imag_out);
float Equalizer_GetSystemMagnitudeDb(const EQ_STATE *st, float freq_hz);
float Equalizer_GetSystemPhaseDeg(const EQ_STATE *st, float freq_hz);
float Equalizer_GetSystemGroupDelaySamples(const EQ_STATE *st,
                                           float freq_hz);

#endif /* _USER_EQUALIZER_H_ */
