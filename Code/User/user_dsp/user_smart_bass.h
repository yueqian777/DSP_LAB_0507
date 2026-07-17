/**
 * user_smart_bass.h
 *
 * Bounded low-frequency attenuation driven by a read-only analyzer snapshot.
 */

#ifndef _USER_SMART_BASS_H_
#define _USER_SMART_BASS_H_

#include "user_audio_feature_analyzer.h"
#include "user_equalizer.h"

#ifndef EQ_ENABLE_SMART_BASS
#define EQ_ENABLE_SMART_BASS 0
#endif

#if (EQ_ENABLE_SMART_BASS != 0) && \
    (EQ_ENABLE_AUDIO_FEATURE_ANALYZER == 0)
#error Smart Bass requires the audio feature analyzer.
#endif

#define SMART_BASS_SAMPLE_RATE       50000.0f
#define SMART_BASS_SHELF_HZ          125.0f
#define SMART_BASS_SHELF_SLOPE       1.0f
#define SMART_BASS_LEVEL_COUNT       7
#define SMART_BASS_LEVEL_STEP_DB     0.5f
#define SMART_BASS_TRANSITION_MS     80.0f
#define SMART_BASS_MAX_FRAME_LEN     1024
#define SMART_BASS_TRANSITION_SAMPLES \
    ((int)(SMART_BASS_SAMPLE_RATE * SMART_BASS_TRANSITION_MS / 1000.0f + 0.5f))

#define SMART_BASS_START_DB          4.0f
#define SMART_BASS_FULL_DB           16.0f
#define SMART_BASS_MIN_RMS_DBFS      (-45.0f)

#define SMART_BASS_STRENGTH_OFF      0
#define SMART_BASS_STRENGTH_LOW      1
#define SMART_BASS_STRENGTH_MEDIUM   2
#define SMART_BASS_STRENGTH_HIGH     3

#define SMART_BASS_REASON_DISABLED        0
#define SMART_BASS_REASON_NOT_WARM        1
#define SMART_BASS_REASON_INVALID         2
#define SMART_BASS_REASON_LOW_LEVEL       3
#define SMART_BASS_REASON_BELOW_THRESHOLD 4
#define SMART_BASS_REASON_EXCESS_BASS     5
#define SMART_BASS_REASON_RELEASING       6

#define SMART_BASS_RESULT_ERROR     (-1)
#define SMART_BASS_RESULT_NO_CHANGE 0
#define SMART_BASS_RESULT_UPDATED   1

typedef EQ_BIQUAD SMART_BASS_BIQUAD;

typedef struct
{
    float s1;
    float s2;
} SMART_BASS_DF2T_STATE;

typedef struct
{
    SMART_BASS_BIQUAD coefficient_table[SMART_BASS_LEVEL_COUNT];
    SMART_BASS_DF2T_STATE active_state;
    SMART_BASS_DF2T_STATE pending_state;

    int active_level;
    int target_level;
    int pending_level;
    int queued_level;
    int queued_level_valid;

    int transition_active;
    int transition_remaining;
    int transition_total;

    int requested_enabled;
    int processing_active;
    int strength;
    int force_release;
    int reason;

    float latest_bass_relative_db;
    float latest_rms_dbfs;

    unsigned long latest_analysis_count;
    unsigned long decision_count;
    unsigned long transition_count;
    unsigned long invalid_release_count;
    unsigned long level_change_count;
    unsigned long saturation_count;
    unsigned long nonfinite_count;

    int release_confirmation_count;
    int initialized;
} SMART_BASS_STATE;

void SmartBass_Init(SMART_BASS_STATE *state);
void SmartBass_Reset(SMART_BASS_STATE *state);
void SmartBass_InvalidateAnalysisEpoch(SMART_BASS_STATE *state);
int SmartBass_SetEnabled(SMART_BASS_STATE *state, int enabled);
int SmartBass_SetStrength(SMART_BASS_STATE *state, int strength);
int SmartBass_UpdateFromFeature(
    SMART_BASS_STATE *state,
    const AUDIO_FEATURE_SNAPSHOT *snapshot);
int SmartBass_ProcessFrame(
    SMART_BASS_STATE *state,
    const short *input,
    short *output,
    int sample_count);
float SmartBass_GetAppliedGainDb(const SMART_BASS_STATE *state);
float SmartBass_GetRequestedGainDb(const SMART_BASS_STATE *state);
float SmartBass_GetTransitionProgress(const SMART_BASS_STATE *state);
int SmartBass_GetReason(const SMART_BASS_STATE *state);

#endif /* _USER_SMART_BASS_H_ */
