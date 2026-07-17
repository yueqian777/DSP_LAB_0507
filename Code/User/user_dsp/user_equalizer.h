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
#define EQ_PRESET_NONE         (-1)

#define EQ_HEADROOM_POINT_COUNT 512
#define EQ_RBJ_BANK_SINGLE_1K EQ_PRESET_COUNT
#define EQ_RBJ_BANK_CACHE_COUNT (EQ_PRESET_COUNT + 1)
#define EQ_RBJ_BANK_CUSTOM (-1)

#define EQ_CORE_RAW_COPY       0
#define EQ_CORE_FLOAT_COPY     1
#define EQ_CORE_LEGACY         2
#define EQ_CORE_RBJ_CASCADE    3

#define EQ_TRANSITION_NONE          0
#define EQ_TRANSITION_BANK_TO_BANK  1
#define EQ_TRANSITION_DRY_TO_BANK   2

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

typedef unsigned int EQ_CONTROL_SEQUENCE;
typedef char EQ_CONTROL_SEQUENCE_MUST_BE_32_BITS[
    (sizeof(EQ_CONTROL_SEQUENCE) == 4U) ? 1 : -1];

typedef struct
{
    EQ_FILTER_BANK bank;
    float gains_db[EQ_NUM_BANDS];
    int bank_id;
    int preset;
    unsigned long generation;
    EQ_CONTROL_SEQUENCE request_sequence;
    int valid;
} EQ_PREPARED_BANK;

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
    float active_gain_db[EQ_NUM_BANDS];
    float pending_gain_db[EQ_NUM_BANDS];
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
    int requested_preset;
    int active_preset;
    int pending_preset;
    int active_bank_id;
    int pending_bank_id;
    int transition_kind;
    int latest_preset_valid;
    int identity_hold;
    unsigned long target_generation;
    unsigned long active_generation;
    unsigned long pending_generation;
    unsigned long mode_change_count;
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
void Equalizer_SetIdentityHold(EQ_STATE *st, int enable);
int Equalizer_GetIdentityHold(const EQ_STATE *st);
void Equalizer_SetBandGainDb(EQ_STATE *st, int band, float gain_db);
void Equalizer_SetAllGainsDb(EQ_STATE *st,
                             const float gains_db[EQ_NUM_BANDS]);
void Equalizer_ApplyPreset(EQ_STATE *st, int preset);
void Equalizer_ApplySingleBand1kPlus3Db(EQ_STATE *st);
void Equalizer_ProcessFrame(EQ_STATE *st, const short *in, short *out, int n);
void Equalizer_ProcessFrameFloat(EQ_STATE *st, const float *in,
                                 float *out, int n);

int Equalizer_PublishLogicalTarget(
    EQ_STATE *st,
    const float gains_db[EQ_NUM_BANDS],
    int preset,
    unsigned long *generation_out,
    int *bank_id_out);
int Equalizer_PreviewLogicalTarget(
    const EQ_STATE *st,
    const float gains_db[EQ_NUM_BANDS],
    int preset,
    unsigned long *generation_out,
    int *bank_id_out);
void Equalizer_CommitLogicalTarget(
    EQ_STATE *st,
    const float gains_db[EQ_NUM_BANDS],
    int preset,
    unsigned long generation,
    int bank_id);
int Equalizer_CopyPresetGainsDb(int preset,
                                float gains_out[EQ_NUM_BANDS]);
int Equalizer_CopyCachedPreparedBank(
    int bank_id,
    int preset,
    unsigned long generation,
    EQ_CONTROL_SEQUENCE request_sequence,
    EQ_PREPARED_BANK *out);
int Equalizer_DesignRbjSection(EQ_BIQUAD *section_out,
                               int section,
                               float gain_db);
int Equalizer_DesignRbjLowShelfAt(EQ_BIQUAD *section_out,
                                  float frequency_hz,
                                  float gain_db,
                                  float shelf_slope);
int Equalizer_GetBiquadResponseComplex(const EQ_BIQUAD *section,
                                       float frequency_hz,
                                       double *real_out,
                                       double *imag_out);
int Equalizer_EvaluateHeadroomPointDb(const EQ_FILTER_BANK *bank,
                                      int point_index,
                                      float *magnitude_db);
void Equalizer_FinalizeRbjBank(EQ_FILTER_BANK *bank,
                               int gains_are_flat,
                               float peak_db);
int Equalizer_IsPresetCacheReady(void);
int Equalizer_InstallPreparedBank(EQ_STATE *st,
                                  const EQ_PREPARED_BANK *prepared,
                                  int transition_kind);
unsigned long Equalizer_GetActiveGeneration(const EQ_STATE *st);

#define EQ_INSTALL_INSTALLED 1
#define EQ_INSTALL_BUSY 0
#define EQ_INSTALL_STALE (-1)
#define EQ_INSTALL_INVALID (-2)

float Equalizer_GetBandCenterHz(int band);
float Equalizer_GetBandTargetGainDb(const EQ_STATE *st, int band);
float Equalizer_GetBandMagnitudeDb(int band, float freq_hz);
float Equalizer_GetPredictedPeakDb(const EQ_STATE *st);
float Equalizer_GetPreampDb(const EQ_STATE *st);
int Equalizer_IsFlat(const EQ_STATE *st);
int Equalizer_HasPendingTransition(const EQ_STATE *st);
int Equalizer_GetTransitionRemaining(const EQ_STATE *st);
int Equalizer_GetRequestedPreset(const EQ_STATE *st);
int Equalizer_GetTransitionTargetPreset(const EQ_STATE *st);
int Equalizer_GetAppliedPreset(const EQ_STATE *st);
int Equalizer_GetLatestPresetPending(const EQ_STATE *st);
unsigned long Equalizer_GetModeChangeCount(const EQ_STATE *st);
int Equalizer_ActiveBankMatchesTarget(const EQ_STATE *st);
int Equalizer_GetCoefficientInfo(const EQ_STATE *st, int section,
                                 EQ_SECTION_INFO *info);
void Equalizer_GetSystemResponse(const EQ_STATE *st, float freq_hz,
                                 float *real_out, float *imag_out);
float Equalizer_GetSystemMagnitudeDb(const EQ_STATE *st, float freq_hz);
float Equalizer_GetSystemPhaseDeg(const EQ_STATE *st, float freq_hz);
float Equalizer_GetSystemGroupDelaySamples(const EQ_STATE *st,
                                           float freq_hz);

#endif /* _USER_EQUALIZER_H_ */
