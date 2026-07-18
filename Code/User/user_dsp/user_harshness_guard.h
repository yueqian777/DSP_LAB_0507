/**
 * user_harshness_guard.h
 *
 * Bounded high-frequency excess attenuation driven by an analyzer snapshot.
 */

#ifndef _USER_HARSHNESS_GUARD_H_
#define _USER_HARSHNESS_GUARD_H_

#include "user_audio_feature_analyzer.h"
#include "user_equalizer.h"

#ifndef EQ_ENABLE_HARSHNESS_GUARD
#define EQ_ENABLE_HARSHNESS_GUARD 0
#endif

#ifndef EQ_ENABLE_HARSHNESS_GUARD_TRANSITION_CAPTURE
#define EQ_ENABLE_HARSHNESS_GUARD_TRANSITION_CAPTURE 0
#endif

#if (EQ_ENABLE_HARSHNESS_GUARD != 0) && \
    (EQ_ENABLE_AUDIO_FEATURE_ANALYZER == 0)
#error Harshness Guard requires the audio feature analyzer.
#endif

#define HARSHNESS_GUARD_SAMPLE_RATE          50000.0f
#define HARSHNESS_GUARD_CENTER_HZ             6000.0f
#define HARSHNESS_GUARD_BANDWIDTH_OCTAVES        1.0f
#define HARSHNESS_GUARD_LEVEL_COUNT                 5
#define HARSHNESS_GUARD_LEVEL_STEP_DB              0.5f
#define HARSHNESS_GUARD_TRANSITION_MS             80.0f
#define HARSHNESS_GUARD_MAX_FRAME_LEN             1024
#define HARSHNESS_GUARD_TRANSITION_SAMPLES \
    ((int)(HARSHNESS_GUARD_SAMPLE_RATE * \
           HARSHNESS_GUARD_TRANSITION_MS / 1000.0f + 0.5f))

#define HARSHNESS_GUARD_START_DB            6.0f
#define HARSHNESS_GUARD_FULL_DB            18.0f
#define HARSHNESS_GUARD_MIN_RMS_DBFS      (-45.0f)
#define HARSHNESS_GUARD_MIN_BRIGHTNESS_DB   0.0f

#define HARSHNESS_GUARD_STRENGTH_OFF        0
#define HARSHNESS_GUARD_STRENGTH_LOW        1
#define HARSHNESS_GUARD_STRENGTH_MEDIUM     2
#define HARSHNESS_GUARD_STRENGTH_HIGH       3

#define HARSHNESS_GUARD_REASON_DISABLED              0
#define HARSHNESS_GUARD_REASON_NOT_WARM              1
#define HARSHNESS_GUARD_REASON_INVALID               2
#define HARSHNESS_GUARD_REASON_LOW_LEVEL             3
#define HARSHNESS_GUARD_REASON_WEAK_BRIGHTNESS       4
#define HARSHNESS_GUARD_REASON_BELOW_THRESHOLD       5
#define HARSHNESS_GUARD_REASON_HIGH_FREQUENCY_EXCESS 6
#define HARSHNESS_GUARD_REASON_RELEASING             7

#define HARSHNESS_GUARD_RESULT_ERROR     (-1)
#define HARSHNESS_GUARD_RESULT_NO_CHANGE 0
#define HARSHNESS_GUARD_RESULT_UPDATED   1

typedef EQ_BIQUAD HARSHNESS_GUARD_BIQUAD;

typedef struct
{
    float s1;
    float s2;
} HARSHNESS_GUARD_DF2T_STATE;

typedef struct
{
    HARSHNESS_GUARD_BIQUAD coefficient_table[HARSHNESS_GUARD_LEVEL_COUNT];
    HARSHNESS_GUARD_DF2T_STATE active_state;
    HARSHNESS_GUARD_DF2T_STATE pending_state;

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

    float latest_brightness_relative_db;
    float latest_presence_relative_db;
    float latest_harshness_db;
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
} HARSHNESS_GUARD_STATE;

void HarshnessGuard_Init(HARSHNESS_GUARD_STATE *state);
void HarshnessGuard_Reset(HARSHNESS_GUARD_STATE *state);
void HarshnessGuard_InvalidateAnalysisEpoch(HARSHNESS_GUARD_STATE *state);
int HarshnessGuard_SetEnabled(HARSHNESS_GUARD_STATE *state, int enabled);
int HarshnessGuard_SetStrength(HARSHNESS_GUARD_STATE *state, int strength);
int HarshnessGuard_UpdateFromFeature(
    HARSHNESS_GUARD_STATE *state,
    const AUDIO_FEATURE_SNAPSHOT *snapshot);
int HarshnessGuard_ProcessFrame(
    HARSHNESS_GUARD_STATE *state,
    const short *input,
    short *output,
    int sample_count);
float HarshnessGuard_GetAppliedGainDb(
    const HARSHNESS_GUARD_STATE *state);
float HarshnessGuard_GetRequestedGainDb(
    const HARSHNESS_GUARD_STATE *state);
float HarshnessGuard_GetTransitionProgress(
    const HARSHNESS_GUARD_STATE *state);
int HarshnessGuard_GetReason(const HARSHNESS_GUARD_STATE *state);
#if EQ_ENABLE_HARSHNESS_GUARD_TRANSITION_CAPTURE != 0
int HarshnessGuard_DiagnosticForceStableLevel(
    HARSHNESS_GUARD_STATE *state, int level);
int HarshnessGuard_DiagnosticRequestLevel(
    HARSHNESS_GUARD_STATE *state, int level);
#endif

#endif /* _USER_HARSHNESS_GUARD_H_ */
