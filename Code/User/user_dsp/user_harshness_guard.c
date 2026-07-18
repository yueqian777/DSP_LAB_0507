/**
 * user_harshness_guard.c
 *
 * Harshness Guard V1 applies only a bounded 6 kHz peaking cut after the
 * existing Project 3.3 dynamic stages. Coefficients are fixed at init;
 * runtime decisions only select adjacent cached levels.
 */

#include "user_harshness_guard.h"

#if EQ_ENABLE_HARSHNESS_GUARD != 0

#include "float.h"
#include "string.h"

static int HarshnessGuard_IsFinite(float value)
{
    return ((value == value) && (value <= FLT_MAX) &&
            (value >= -FLT_MAX));
}

static int HarshnessGuard_ClampLevel(int level)
{
    if (level < 0)
    {
        return 0;
    }
    if (level >= HARSHNESS_GUARD_LEVEL_COUNT)
    {
        return HARSHNESS_GUARD_LEVEL_COUNT - 1;
    }
    return level;
}

static int HarshnessGuard_ClampStrength(int strength)
{
    if (strength < HARSHNESS_GUARD_STRENGTH_OFF)
    {
        return HARSHNESS_GUARD_STRENGTH_OFF;
    }
    if (strength > HARSHNESS_GUARD_STRENGTH_HIGH)
    {
        return HARSHNESS_GUARD_STRENGTH_HIGH;
    }
    return strength;
}

static int HarshnessGuard_MaxLevelForStrength(int strength)
{
    static const int maximum_level[4] = { 0, 2, 3, 4 };

    return maximum_level[HarshnessGuard_ClampStrength(strength)];
}

static float HarshnessGuard_LevelGainDb(int level)
{
    return -HARSHNESS_GUARD_LEVEL_STEP_DB *
           (float)HarshnessGuard_ClampLevel(level);
}

static void HarshnessGuard_ClearFilterState(
    HARSHNESS_GUARD_DF2T_STATE *filter_state)
{
    filter_state->s1 = 0.0f;
    filter_state->s2 = 0.0f;
}

static void HarshnessGuard_ClearRuntime(HARSHNESS_GUARD_STATE *state)
{
    HarshnessGuard_ClearFilterState(&state->active_state);
    HarshnessGuard_ClearFilterState(&state->pending_state);
    state->active_level = 0;
    state->target_level = 0;
    state->pending_level = 0;
    state->queued_level = 0;
    state->queued_level_valid = 0;
    state->transition_active = 0;
    state->transition_remaining = 0;
    state->transition_total = HARSHNESS_GUARD_TRANSITION_SAMPLES;
    state->requested_enabled = 0;
    state->processing_active = 0;
    state->strength = HARSHNESS_GUARD_STRENGTH_MEDIUM;
    state->force_release = 0;
    state->reason = HARSHNESS_GUARD_REASON_DISABLED;
    state->latest_brightness_relative_db = 0.0f;
    state->latest_presence_relative_db = 0.0f;
    state->latest_harshness_db = 0.0f;
    state->latest_rms_dbfs = -240.0f;
    state->latest_analysis_count = 0UL;
    state->decision_count = 0UL;
    state->transition_count = 0UL;
    state->level_change_count = 0UL;
    state->invalid_release_count = 0UL;
    state->saturation_count = 0UL;
    state->nonfinite_count = 0UL;
    state->release_confirmation_count = 0;
}

static int HarshnessGuard_StartTransition(
    HARSHNESS_GUARD_STATE *state,
    int requested_level)
{
    int next_level;

    if ((state == 0) || (state->initialized == 0) ||
        (state->transition_active != 0))
    {
        return 0;
    }

    next_level = HarshnessGuard_ClampLevel(requested_level);
    if (next_level > (state->active_level + 1))
    {
        next_level = state->active_level + 1;
    }
    else if (next_level < (state->active_level - 1))
    {
        next_level = state->active_level - 1;
    }
    if (next_level == state->active_level)
    {
        return 0;
    }

    state->pending_level = next_level;
    if (state->active_level != 0)
    {
        state->pending_state = state->active_state;
    }
    else
    {
        HarshnessGuard_ClearFilterState(&state->pending_state);
    }
    state->transition_total = HARSHNESS_GUARD_TRANSITION_SAMPLES;
    state->transition_remaining = state->transition_total;
    state->transition_active = 1;
    state->processing_active = 1;
    state->transition_count++;
    return 1;
}

static void HarshnessGuard_RequestAdjacentStep(
    HARSHNESS_GUARD_STATE *state,
    int endpoint_level,
    int direction)
{
    int next_level;

    next_level = HarshnessGuard_ClampLevel(endpoint_level + direction);
    if (state->transition_active != 0)
    {
        state->queued_level = next_level;
        state->queued_level_valid =
            (next_level != endpoint_level) ? 1 : 0;
    }
    else
    {
        (void)HarshnessGuard_StartTransition(state, next_level);
    }
}

static void HarshnessGuard_FinishTransition(
    HARSHNESS_GUARD_STATE *state)
{
    int queued_level;
    int have_queued;

    state->active_level = state->pending_level;
    state->active_state = state->pending_state;
    state->transition_active = 0;
    state->transition_remaining = 0;
    state->pending_level = state->active_level;
    state->level_change_count++;

    have_queued = state->queued_level_valid;
    queued_level = state->queued_level;
    state->queued_level_valid = 0;

    if (state->force_release != 0)
    {
        if (state->active_level > state->target_level)
        {
            (void)HarshnessGuard_StartTransition(
                state, state->active_level - 1);
        }
        else
        {
            state->force_release = 0;
            state->processing_active =
                (state->active_level != 0) ? 1 : 0;
        }
    }
    else if ((have_queued != 0) &&
             (queued_level != state->active_level))
    {
        (void)HarshnessGuard_StartTransition(state, queued_level);
    }
    else
    {
        state->processing_active =
            (state->active_level != 0) ? 1 : 0;
    }
}

static float HarshnessGuard_ProcessBiquad(
    const HARSHNESS_GUARD_BIQUAD *coefficient,
    HARSHNESS_GUARD_DF2T_STATE *filter_state,
    float input)
{
    float output;

    output = coefficient->b0 * input + filter_state->s1;
    filter_state->s1 = coefficient->b1 * input -
                       coefficient->a1 * output + filter_state->s2;
    filter_state->s2 = coefficient->b2 * input -
                       coefficient->a2 * output;
    return output;
}

static float HarshnessGuard_ProcessPath(
    const HARSHNESS_GUARD_STATE *state,
    int level,
    HARSHNESS_GUARD_DF2T_STATE *filter_state,
    float input)
{
    if (level == 0)
    {
        return input;
    }
    return HarshnessGuard_ProcessBiquad(
        &state->coefficient_table[level], filter_state, input);
}

static short HarshnessGuard_Saturate(HARSHNESS_GUARD_STATE *state,
                                     float value)
{
    long rounded;

    if (value > 32767.0f)
    {
        state->saturation_count++;
        return (short)32767;
    }
    if (value < -32768.0f)
    {
        state->saturation_count++;
        return (short)-32768;
    }
    rounded = (value >= 0.0f) ?
        (long)(value + 0.5f) : (long)(value - 0.5f);
    return (short)rounded;
}

static void HarshnessGuard_EnterSafeIdentity(
    HARSHNESS_GUARD_STATE *state)
{
    state->nonfinite_count++;
    HarshnessGuard_ClearFilterState(&state->active_state);
    HarshnessGuard_ClearFilterState(&state->pending_state);
    state->active_level = 0;
    state->target_level = 0;
    state->pending_level = 0;
    state->queued_level_valid = 0;
    state->transition_active = 0;
    state->transition_remaining = 0;
    state->force_release = 0;
    state->processing_active = 0;
    state->reason = HARSHNESS_GUARD_REASON_INVALID;
}

void HarshnessGuard_Init(HARSHNESS_GUARD_STATE *state)
{
    int level;

    if (state == 0)
    {
        return;
    }
    memset(state, 0, sizeof(*state));
    for (level = 0; level < HARSHNESS_GUARD_LEVEL_COUNT; level++)
    {
        if (!Equalizer_DesignRbjPeakingAt(
                &state->coefficient_table[level],
                HARSHNESS_GUARD_CENTER_HZ,
                HarshnessGuard_LevelGainDb(level),
                HARSHNESS_GUARD_BANDWIDTH_OCTAVES))
        {
            memset(state, 0, sizeof(*state));
            return;
        }
    }
    HarshnessGuard_ClearRuntime(state);
    state->initialized = 1;
}

void HarshnessGuard_Reset(HARSHNESS_GUARD_STATE *state)
{
    if ((state == 0) || (state->initialized == 0))
    {
        return;
    }
    HarshnessGuard_ClearRuntime(state);
    state->initialized = 1;
}

void HarshnessGuard_InvalidateAnalysisEpoch(HARSHNESS_GUARD_STATE *state)
{
    if ((state != 0) && (state->initialized != 0))
    {
        state->latest_analysis_count = ~0UL;
    }
}

int HarshnessGuard_SetEnabled(HARSHNESS_GUARD_STATE *state, int enabled)
{
    int normalized;

    if ((state == 0) || (state->initialized == 0))
    {
        return HARSHNESS_GUARD_RESULT_ERROR;
    }
    normalized = (enabled != 0) ? 1 : 0;
    if (normalized == state->requested_enabled)
    {
        return HARSHNESS_GUARD_RESULT_NO_CHANGE;
    }

    state->requested_enabled = normalized;
    state->release_confirmation_count = 0;
    if (normalized != 0)
    {
        state->force_release = 0;
        return HARSHNESS_GUARD_RESULT_UPDATED;
    }

    state->target_level = 0;
    state->queued_level_valid = 0;
    state->reason = HARSHNESS_GUARD_REASON_DISABLED;
    if ((state->transition_active != 0) || (state->active_level > 0))
    {
        state->force_release = 1;
        if (state->transition_active == 0)
        {
            (void)HarshnessGuard_StartTransition(
                state, state->active_level - 1);
        }
    }
    else
    {
        state->force_release = 0;
        state->processing_active = 0;
    }
    return HARSHNESS_GUARD_RESULT_UPDATED;
}

int HarshnessGuard_SetStrength(HARSHNESS_GUARD_STATE *state, int strength)
{
    int endpoint_level;
    int limit_level;
    int maximum_level;
    int normalized;

    if ((state == 0) || (state->initialized == 0))
    {
        return HARSHNESS_GUARD_RESULT_ERROR;
    }
    normalized = HarshnessGuard_ClampStrength(strength);
    if (normalized == state->strength)
    {
        return HARSHNESS_GUARD_RESULT_NO_CHANGE;
    }
    state->strength = normalized;
    maximum_level = HarshnessGuard_MaxLevelForStrength(normalized);
    limit_level = (state->requested_enabled != 0) ? maximum_level : 0;
    if (state->target_level > limit_level)
    {
        state->target_level = limit_level;
    }
    if ((state->queued_level_valid != 0) &&
        (state->queued_level > limit_level))
    {
        state->queued_level_valid = 0;
    }
    endpoint_level = (state->transition_active != 0) ?
        state->pending_level : state->active_level;
    if (endpoint_level > limit_level)
    {
        state->target_level = limit_level;
        state->reason = (limit_level == 0) ?
            HARSHNESS_GUARD_REASON_DISABLED :
            HARSHNESS_GUARD_REASON_RELEASING;
        state->queued_level_valid = 0;
        state->force_release = 1;
        if (state->transition_active == 0)
        {
            (void)HarshnessGuard_StartTransition(
                state, state->active_level - 1);
        }
    }
    else if (state->requested_enabled != 0)
    {
        state->force_release = 0;
    }
    return HARSHNESS_GUARD_RESULT_UPDATED;
}

int HarshnessGuard_UpdateFromFeature(
    HARSHNESS_GUARD_STATE *state,
    const AUDIO_FEATURE_SNAPSHOT *snapshot)
{
    float brightness_db;
    float harshness_db;
    float presence_db;
    float rms_dbfs;
    float scaled_level;
    int desired_level;
    int endpoint_level;
    int maximum_level;
    int release_cause;

    if ((state == 0) || (snapshot == 0) ||
        (state->initialized == 0))
    {
        return HARSHNESS_GUARD_RESULT_ERROR;
    }
    if (snapshot->analysis_count == state->latest_analysis_count)
    {
        return HARSHNESS_GUARD_RESULT_NO_CHANGE;
    }

    state->latest_analysis_count = snapshot->analysis_count;
    state->decision_count++;
    brightness_db = snapshot->relative_db[AUDIO_FEATURE_BRIGHTNESS];
    presence_db = snapshot->relative_db[AUDIO_FEATURE_PRESENCE];
    rms_dbfs = snapshot->rms_dbfs;
    maximum_level = HarshnessGuard_MaxLevelForStrength(state->strength);
    desired_level = 0;
    release_cause = 0;

    if ((HarshnessGuard_IsFinite(brightness_db) == 0) ||
        (HarshnessGuard_IsFinite(presence_db) == 0) ||
        (HarshnessGuard_IsFinite(rms_dbfs) == 0))
    {
        state->nonfinite_count++;
        state->latest_brightness_relative_db = 0.0f;
        state->latest_presence_relative_db = 0.0f;
        state->latest_harshness_db = 0.0f;
        state->latest_rms_dbfs = -240.0f;
        state->reason = HARSHNESS_GUARD_REASON_INVALID;
        release_cause = 1;
    }
    else
    {
        harshness_db = brightness_db - presence_db;
        if (HarshnessGuard_IsFinite(harshness_db) == 0)
        {
            state->nonfinite_count++;
            state->latest_brightness_relative_db = 0.0f;
            state->latest_presence_relative_db = 0.0f;
            state->latest_harshness_db = 0.0f;
            state->latest_rms_dbfs = -240.0f;
            state->reason = HARSHNESS_GUARD_REASON_INVALID;
            release_cause = 1;
        }
        else
        {
            state->latest_brightness_relative_db = brightness_db;
            state->latest_presence_relative_db = presence_db;
            state->latest_harshness_db = harshness_db;
            state->latest_rms_dbfs = rms_dbfs;
            if (snapshot->valid == 0)
            {
                state->reason = HARSHNESS_GUARD_REASON_INVALID;
                release_cause = 1;
            }
            else if (snapshot->warmup_complete == 0)
            {
                state->reason = HARSHNESS_GUARD_REASON_NOT_WARM;
                release_cause = 1;
            }
            else if (state->requested_enabled == 0)
            {
                state->reason = HARSHNESS_GUARD_REASON_DISABLED;
            }
            else if (rms_dbfs < HARSHNESS_GUARD_MIN_RMS_DBFS)
            {
                state->reason = HARSHNESS_GUARD_REASON_LOW_LEVEL;
                release_cause = 1;
            }
            else if (brightness_db <
                     HARSHNESS_GUARD_MIN_BRIGHTNESS_DB)
            {
                state->reason = HARSHNESS_GUARD_REASON_WEAK_BRIGHTNESS;
                release_cause = 1;
            }
            else if ((maximum_level == 0) ||
                     (harshness_db <= HARSHNESS_GUARD_START_DB))
            {
                state->reason = (maximum_level == 0) ?
                    HARSHNESS_GUARD_REASON_DISABLED :
                    HARSHNESS_GUARD_REASON_BELOW_THRESHOLD;
            }
            else
            {
                state->reason =
                    HARSHNESS_GUARD_REASON_HIGH_FREQUENCY_EXCESS;
                if (harshness_db >= HARSHNESS_GUARD_FULL_DB)
                {
                    desired_level = maximum_level;
                }
                else
                {
                    scaled_level =
                        ((harshness_db - HARSHNESS_GUARD_START_DB) /
                         (HARSHNESS_GUARD_FULL_DB -
                          HARSHNESS_GUARD_START_DB)) *
                        (float)maximum_level;
                    desired_level = (int)(scaled_level + 0.5f);
                    if (desired_level > maximum_level)
                    {
                        desired_level = maximum_level;
                    }
                }
            }
        }
    }

    if ((state->requested_enabled == 0) || (maximum_level == 0))
    {
        desired_level = 0;
    }
    state->target_level = HarshnessGuard_ClampLevel(desired_level);
    endpoint_level = (state->transition_active != 0) ?
        state->pending_level : state->active_level;
    if ((release_cause != 0) && (endpoint_level > 0))
    {
        state->invalid_release_count++;
    }

    if (state->force_release != 0)
    {
        return HARSHNESS_GUARD_RESULT_UPDATED;
    }
    if (state->target_level > endpoint_level)
    {
        state->release_confirmation_count = 0;
        HarshnessGuard_RequestAdjacentStep(state, endpoint_level, 1);
    }
    else if (state->target_level < endpoint_level)
    {
        if ((state->transition_active != 0) &&
            (state->queued_level_valid != 0) &&
            (state->queued_level > endpoint_level))
        {
            state->queued_level_valid = 0;
        }
        state->release_confirmation_count++;
        if (state->release_confirmation_count >= 2)
        {
            state->release_confirmation_count = 0;
            state->reason = HARSHNESS_GUARD_REASON_RELEASING;
            HarshnessGuard_RequestAdjacentStep(state, endpoint_level, -1);
        }
    }
    else
    {
        state->release_confirmation_count = 0;
        if (state->transition_active != 0)
        {
            state->queued_level_valid = 0;
        }
    }
    return HARSHNESS_GUARD_RESULT_UPDATED;
}

int HarshnessGuard_ProcessFrame(
    HARSHNESS_GUARD_STATE *state,
    const short *input,
    short *output,
    int sample_count)
{
    int index;

    if ((state == 0) || (input == 0) || (output == 0) ||
        (sample_count <= 0) ||
        (sample_count > HARSHNESS_GUARD_MAX_FRAME_LEN) ||
        (state->initialized == 0))
    {
        return HARSHNESS_GUARD_RESULT_ERROR;
    }

    if ((state->transition_active == 0) && (state->active_level == 0))
    {
        if (input != output)
        {
            memcpy(output, input,
                   (unsigned int)sample_count * sizeof(short));
        }
        state->processing_active = 0;
        return HARSHNESS_GUARD_RESULT_UPDATED;
    }

    for (index = 0; index < sample_count; index++)
    {
        float input_value;
        float output_value;

        input_value = (float)input[index];
        if (state->transition_active != 0)
        {
            float active_output;
            float pending_output;
            float progress;

            active_output = HarshnessGuard_ProcessPath(
                state, state->active_level, &state->active_state,
                input_value);
            pending_output = HarshnessGuard_ProcessPath(
                state, state->pending_level, &state->pending_state,
                input_value);
            progress = (float)(state->transition_total -
                               state->transition_remaining + 1) /
                       (float)state->transition_total;
            output_value = active_output +
                           (pending_output - active_output) * progress;
            state->transition_remaining--;
            if (state->transition_remaining <= 0)
            {
                HarshnessGuard_FinishTransition(state);
            }
        }
        else
        {
            output_value = HarshnessGuard_ProcessPath(
                state, state->active_level, &state->active_state,
                input_value);
        }

        if (HarshnessGuard_IsFinite(output_value) == 0)
        {
            HarshnessGuard_EnterSafeIdentity(state);
            output[index] = input[index];
        }
        else
        {
            output[index] = HarshnessGuard_Saturate(state, output_value);
        }
    }
    return HARSHNESS_GUARD_RESULT_UPDATED;
}

float HarshnessGuard_GetTransitionProgress(
    const HARSHNESS_GUARD_STATE *state)
{
    if ((state == 0) || (state->initialized == 0) ||
        (state->transition_active == 0) ||
        (state->transition_total <= 0))
    {
        return 0.0f;
    }
    return (float)(state->transition_total - state->transition_remaining) /
           (float)state->transition_total;
}

float HarshnessGuard_GetAppliedGainDb(
    const HARSHNESS_GUARD_STATE *state)
{
    float level;

    if ((state == 0) || (state->initialized == 0))
    {
        return 0.0f;
    }
    level = (float)state->active_level;
    if (state->transition_active != 0)
    {
        level += ((float)state->pending_level -
                  (float)state->active_level) *
                 HarshnessGuard_GetTransitionProgress(state);
    }
    return -HARSHNESS_GUARD_LEVEL_STEP_DB * level;
}

float HarshnessGuard_GetRequestedGainDb(
    const HARSHNESS_GUARD_STATE *state)
{
    if ((state == 0) || (state->initialized == 0))
    {
        return 0.0f;
    }
    return HarshnessGuard_LevelGainDb(state->target_level);
}

int HarshnessGuard_GetReason(const HARSHNESS_GUARD_STATE *state)
{
    if ((state == 0) || (state->initialized == 0))
    {
        return HARSHNESS_GUARD_REASON_INVALID;
    }
    return state->reason;
}

#if EQ_ENABLE_HARSHNESS_GUARD_TRANSITION_CAPTURE != 0
int HarshnessGuard_DiagnosticForceStableLevel(
    HARSHNESS_GUARD_STATE *state, int level)
{
    int normalized;

    if ((state == 0) || (state->initialized == 0))
    {
        return HARSHNESS_GUARD_RESULT_ERROR;
    }
    normalized = HarshnessGuard_ClampLevel(level);
    HarshnessGuard_ClearFilterState(&state->active_state);
    HarshnessGuard_ClearFilterState(&state->pending_state);
    state->active_level = normalized;
    state->target_level = normalized;
    state->pending_level = normalized;
    state->queued_level = normalized;
    state->queued_level_valid = 0;
    state->transition_active = 0;
    state->transition_remaining = 0;
    state->requested_enabled = 1;
    state->processing_active = (normalized != 0) ? 1 : 0;
    state->force_release = 0;
    state->release_confirmation_count = 0;
    return HARSHNESS_GUARD_RESULT_UPDATED;
}

int HarshnessGuard_DiagnosticRequestLevel(
    HARSHNESS_GUARD_STATE *state, int level)
{
    int normalized;

    if ((state == 0) || (state->initialized == 0))
    {
        return HARSHNESS_GUARD_RESULT_ERROR;
    }
    normalized = HarshnessGuard_ClampLevel(level);
    if ((normalized > (state->active_level + 1)) ||
        (normalized < (state->active_level - 1)))
    {
        return HARSHNESS_GUARD_RESULT_ERROR;
    }
    state->target_level = normalized;
    state->queued_level_valid = 0;
    state->force_release = 0;
    return HarshnessGuard_StartTransition(state, normalized) ?
        HARSHNESS_GUARD_RESULT_UPDATED :
        HARSHNESS_GUARD_RESULT_NO_CHANGE;
}
#endif

#endif /* EQ_ENABLE_HARSHNESS_GUARD */
