/**
 * user_dynamic_clarity.h
 *
 * Bounded content-aware low-mid de-masking driven by an analyzer snapshot.
 */

#ifndef _USER_DYNAMIC_CLARITY_H_
#define _USER_DYNAMIC_CLARITY_H_

#include "user_audio_feature_analyzer.h"
#include "user_equalizer.h"

#ifndef EQ_ENABLE_DYNAMIC_CLARITY
#define EQ_ENABLE_DYNAMIC_CLARITY 0
#endif

#ifndef EQ_ENABLE_DYNAMIC_CLARITY_TRANSITION_CAPTURE
#define EQ_ENABLE_DYNAMIC_CLARITY_TRANSITION_CAPTURE 0
#endif

#if (EQ_ENABLE_DYNAMIC_CLARITY != 0) && \
    (EQ_ENABLE_AUDIO_FEATURE_ANALYZER == 0)
#error Dynamic Clarity requires the audio feature analyzer.
#endif

#define DYNAMIC_CLARITY_SAMPLE_RATE       50000.0f
#define DYNAMIC_CLARITY_CENTER_HZ         400.0f
#define DYNAMIC_CLARITY_BANDWIDTH_OCTAVES 1.0f
#define DYNAMIC_CLARITY_LEVEL_COUNT       7
#define DYNAMIC_CLARITY_LEVEL_STEP_DB     0.5f
#define DYNAMIC_CLARITY_TRANSITION_MS     80.0f
#define DYNAMIC_CLARITY_MAX_FRAME_LEN     1024
#define DYNAMIC_CLARITY_TRANSITION_SAMPLES \
    ((int)(DYNAMIC_CLARITY_SAMPLE_RATE * \
           DYNAMIC_CLARITY_TRANSITION_MS / 1000.0f + 0.5f))

#define DYNAMIC_CLARITY_START_DB          4.0f
#define DYNAMIC_CLARITY_FULL_DB           14.0f
#define DYNAMIC_CLARITY_MIN_RMS_DBFS      (-45.0f)
#define DYNAMIC_CLARITY_MIN_MUD_DB        0.0f
#define DYNAMIC_CLARITY_MIN_PRESENCE_DB   (-12.0f)

#define DYNAMIC_CLARITY_STRENGTH_OFF      0
#define DYNAMIC_CLARITY_STRENGTH_LOW      1
#define DYNAMIC_CLARITY_STRENGTH_MEDIUM   2
#define DYNAMIC_CLARITY_STRENGTH_HIGH     3

#define DYNAMIC_CLARITY_REASON_DISABLED        0
#define DYNAMIC_CLARITY_REASON_NOT_WARM        1
#define DYNAMIC_CLARITY_REASON_INVALID         2
#define DYNAMIC_CLARITY_REASON_LOW_LEVEL       3
#define DYNAMIC_CLARITY_REASON_WEAK_MUD        4
#define DYNAMIC_CLARITY_REASON_WEAK_PRESENCE   5
#define DYNAMIC_CLARITY_REASON_BELOW_THRESHOLD 6
#define DYNAMIC_CLARITY_REASON_MASKING         7
#define DYNAMIC_CLARITY_REASON_RELEASING       8

#define DYNAMIC_CLARITY_RESULT_ERROR     (-1)
#define DYNAMIC_CLARITY_RESULT_NO_CHANGE 0
#define DYNAMIC_CLARITY_RESULT_UPDATED   1

typedef EQ_BIQUAD DYNAMIC_CLARITY_BIQUAD;

typedef struct
{
    float s1;
    float s2;
} DYNAMIC_CLARITY_DF2T_STATE;

typedef struct
{
    DYNAMIC_CLARITY_BIQUAD coefficient_table[DYNAMIC_CLARITY_LEVEL_COUNT];
    DYNAMIC_CLARITY_DF2T_STATE active_state;
    DYNAMIC_CLARITY_DF2T_STATE pending_state;

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

    float latest_mud_relative_db;
    float latest_presence_relative_db;
    float latest_masking_db;
    float latest_rms_dbfs;

    unsigned long latest_analysis_count;
    unsigned long decision_count;
    unsigned long transition_count;
    unsigned long level_change_count;
    unsigned long invalid_release_count;
    unsigned long saturation_count;
    unsigned long nonfinite_count;

    int release_confirmation_count;
    int initialized;
} DYNAMIC_CLARITY_STATE;

void DynamicClarity_Init(DYNAMIC_CLARITY_STATE *state);
void DynamicClarity_Reset(DYNAMIC_CLARITY_STATE *state);
void DynamicClarity_InvalidateAnalysisEpoch(DYNAMIC_CLARITY_STATE *state);
int DynamicClarity_SetEnabled(DYNAMIC_CLARITY_STATE *state, int enabled);
int DynamicClarity_SetStrength(DYNAMIC_CLARITY_STATE *state, int strength);
int DynamicClarity_UpdateFromFeature(
    DYNAMIC_CLARITY_STATE *state,
    const AUDIO_FEATURE_SNAPSHOT *snapshot);
int DynamicClarity_ProcessFrame(
    DYNAMIC_CLARITY_STATE *state,
    const short *input,
    short *output,
    int sample_count);
float DynamicClarity_GetAppliedGainDb(
    const DYNAMIC_CLARITY_STATE *state);
float DynamicClarity_GetRequestedGainDb(
    const DYNAMIC_CLARITY_STATE *state);
float DynamicClarity_GetTransitionProgress(
    const DYNAMIC_CLARITY_STATE *state);
int DynamicClarity_GetReason(const DYNAMIC_CLARITY_STATE *state);
#if EQ_ENABLE_DYNAMIC_CLARITY_TRANSITION_CAPTURE != 0
int DynamicClarity_DiagnosticForceStableLevel(
    DYNAMIC_CLARITY_STATE *state, int level);
int DynamicClarity_DiagnosticRequestLevel(
    DYNAMIC_CLARITY_STATE *state, int level);
#endif

#endif /* _USER_DYNAMIC_CLARITY_H_ */
