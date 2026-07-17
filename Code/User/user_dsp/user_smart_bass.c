/**
 * user_smart_bass.c
 *
 * Smart Bass V1 applies only bounded low-shelf attenuation after the static
 * ten-band equalizer. All filter coefficients are designed once at init.
 */

#include "user_smart_bass.h"

#if EQ_ENABLE_SMART_BASS != 0

#include "float.h"
#include "string.h"

static int SmartBass_IsFinite(float value)
{
    return ((value == value) && (value <= FLT_MAX) &&
            (value >= -FLT_MAX));
}

static int SmartBass_ClampLevel(int level)
{
    if (level < 0)
    {
        return 0;
    }
    if (level >= SMART_BASS_LEVEL_COUNT)
    {
        return SMART_BASS_LEVEL_COUNT - 1;
    }
    return level;
}

static int SmartBass_ClampStrength(int strength)
{
    if (strength < SMART_BASS_STRENGTH_OFF)
    {
        return SMART_BASS_STRENGTH_OFF;
    }
    if (strength > SMART_BASS_STRENGTH_HIGH)
    {
        return SMART_BASS_STRENGTH_HIGH;
    }
    return strength;
}

static int SmartBass_MaxLevelForStrength(int strength)
{
    static const int maximum_level[4] = { 0, 2, 4, 6 };

    return maximum_level[SmartBass_ClampStrength(strength)];
}

static float SmartBass_LevelGainDb(int level)
{
    return -SMART_BASS_LEVEL_STEP_DB * (float)SmartBass_ClampLevel(level);
}

static void SmartBass_ClearFilterState(SMART_BASS_DF2T_STATE *filter_state)
{
    filter_state->s1 = 0.0f;
    filter_state->s2 = 0.0f;
}

static void SmartBass_ClearRuntime(SMART_BASS_STATE *state)
{
    SmartBass_ClearFilterState(&state->active_state);
    SmartBass_ClearFilterState(&state->pending_state);
    state->active_level = 0;
    state->target_level = 0;
    state->pending_level = 0;
    state->queued_level = 0;
    state->queued_level_valid = 0;
    state->transition_active = 0;
    state->transition_remaining = 0;
    state->transition_total = SMART_BASS_TRANSITION_SAMPLES;
    state->requested_enabled = 0;
    state->processing_active = 0;
    state->strength = SMART_BASS_STRENGTH_MEDIUM;
    state->force_release = 0;
    state->reason = SMART_BASS_REASON_DISABLED;
    state->latest_bass_relative_db = 0.0f;
    state->latest_rms_dbfs = -240.0f;
    state->latest_analysis_count = 0UL;
    state->decision_count = 0UL;
    state->transition_count = 0UL;
    state->invalid_release_count = 0UL;
    state->level_change_count = 0UL;
    state->saturation_count = 0UL;
    state->nonfinite_count = 0UL;
    state->release_confirmation_count = 0;
}

static int SmartBass_StartTransition(SMART_BASS_STATE *state,
                                     int requested_level)
{
    int next_level;

    if ((state == 0) || (state->initialized == 0) ||
        (state->transition_active != 0))
    {
        return 0;
    }

    next_level = SmartBass_ClampLevel(requested_level);
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
        SmartBass_ClearFilterState(&state->pending_state);
    }
    state->transition_total = SMART_BASS_TRANSITION_SAMPLES;
    state->transition_remaining = state->transition_total;
    state->transition_active = 1;
    state->processing_active = 1;
    state->transition_count++;
    return 1;
}

static void SmartBass_RequestAdjacentStep(SMART_BASS_STATE *state,
                                          int endpoint_level,
                                          int direction)
{
    int next_level;

    next_level = SmartBass_ClampLevel(endpoint_level + direction);
    if (state->transition_active != 0)
    {
        state->queued_level = next_level;
        state->queued_level_valid = (next_level != endpoint_level) ? 1 : 0;
    }
    else
    {
        (void)SmartBass_StartTransition(state, next_level);
    }
}

static void SmartBass_FinishTransition(SMART_BASS_STATE *state)
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
            (void)SmartBass_StartTransition(state,
                                             state->active_level - 1);
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
        (void)SmartBass_StartTransition(state, queued_level);
    }
    else
    {
        state->processing_active = (state->active_level != 0) ? 1 : 0;
    }
}

static float SmartBass_ProcessBiquad(
    const SMART_BASS_BIQUAD *coefficient,
    SMART_BASS_DF2T_STATE *filter_state,
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

static float SmartBass_ProcessPath(
    const SMART_BASS_STATE *state,
    int level,
    SMART_BASS_DF2T_STATE *filter_state,
    float input)
{
    if (level == 0)
    {
        return input;
    }
    return SmartBass_ProcessBiquad(
        &state->coefficient_table[level], filter_state, input);
}

static short SmartBass_Saturate(SMART_BASS_STATE *state, float value)
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

static void SmartBass_EnterSafeIdentity(SMART_BASS_STATE *state)
{
    state->nonfinite_count++;
    SmartBass_ClearFilterState(&state->active_state);
    SmartBass_ClearFilterState(&state->pending_state);
    state->active_level = 0;
    state->target_level = 0;
    state->pending_level = 0;
    state->queued_level_valid = 0;
    state->transition_active = 0;
    state->transition_remaining = 0;
    state->force_release = 0;
    state->processing_active = 0;
    state->reason = SMART_BASS_REASON_INVALID;
}

void SmartBass_Init(SMART_BASS_STATE *state)
{
    int level;

    if (state == 0)
    {
        return;
    }
    memset(state, 0, sizeof(*state));
    for (level = 0; level < SMART_BASS_LEVEL_COUNT; level++)
    {
        if (!Equalizer_DesignRbjLowShelfAt(
                &state->coefficient_table[level],
                SMART_BASS_SHELF_HZ,
                SmartBass_LevelGainDb(level),
                SMART_BASS_SHELF_SLOPE))
        {
            memset(state, 0, sizeof(*state));
            return;
        }
    }
    SmartBass_ClearRuntime(state);
    state->initialized = 1;
}

void SmartBass_Reset(SMART_BASS_STATE *state)
{
    if ((state == 0) || (state->initialized == 0))
    {
        return;
    }
    SmartBass_ClearRuntime(state);
    state->initialized = 1;
}

void SmartBass_InvalidateAnalysisEpoch(SMART_BASS_STATE *state)
{
    if ((state != 0) && (state->initialized != 0))
    {
        state->latest_analysis_count = ~0UL;
    }
}

int SmartBass_SetEnabled(SMART_BASS_STATE *state, int enabled)
{
    int normalized;

    if ((state == 0) || (state->initialized == 0))
    {
        return SMART_BASS_RESULT_ERROR;
    }
    normalized = (enabled != 0) ? 1 : 0;
    if (normalized == state->requested_enabled)
    {
        return SMART_BASS_RESULT_NO_CHANGE;
    }

    state->requested_enabled = normalized;
    state->release_confirmation_count = 0;
    if (normalized != 0)
    {
        state->force_release = 0;
        return SMART_BASS_RESULT_UPDATED;
    }

    state->target_level = 0;
    state->queued_level_valid = 0;
    state->reason = SMART_BASS_REASON_DISABLED;
    if ((state->transition_active != 0) || (state->active_level > 0))
    {
        state->force_release = 1;
        if (state->transition_active == 0)
        {
            (void)SmartBass_StartTransition(state,
                                             state->active_level - 1);
        }
    }
    else
    {
        state->force_release = 0;
        state->processing_active = 0;
    }
    return SMART_BASS_RESULT_UPDATED;
}

int SmartBass_SetStrength(SMART_BASS_STATE *state, int strength)
{
    int endpoint_level;
    int limit_level;
    int maximum_level;
    int normalized;

    if ((state == 0) || (state->initialized == 0))
    {
        return SMART_BASS_RESULT_ERROR;
    }
    normalized = SmartBass_ClampStrength(strength);
    if (normalized == state->strength)
    {
        return SMART_BASS_RESULT_NO_CHANGE;
    }
    state->strength = normalized;
    maximum_level = SmartBass_MaxLevelForStrength(normalized);
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
            SMART_BASS_REASON_DISABLED : SMART_BASS_REASON_RELEASING;
        state->queued_level_valid = 0;
        state->force_release = 1;
        if (state->transition_active == 0)
        {
            (void)SmartBass_StartTransition(
                state, state->active_level - 1);
        }
    }
    else if (state->requested_enabled != 0)
    {
        state->force_release = 0;
    }
    return SMART_BASS_RESULT_UPDATED;
}

int SmartBass_UpdateFromFeature(
    SMART_BASS_STATE *state,
    const AUDIO_FEATURE_SNAPSHOT *snapshot)
{
    float bass_db;
    float rms_dbfs;
    float scaled_level;
    int endpoint_level;
    int desired_level;
    int maximum_level;
    int release_cause;

    if ((state == 0) || (snapshot == 0) ||
        (state->initialized == 0))
    {
        return SMART_BASS_RESULT_ERROR;
    }
    if (snapshot->analysis_count == state->latest_analysis_count)
    {
        return SMART_BASS_RESULT_NO_CHANGE;
    }

    state->latest_analysis_count = snapshot->analysis_count;
    state->decision_count++;
    bass_db = snapshot->relative_db[AUDIO_FEATURE_BASS];
    rms_dbfs = snapshot->rms_dbfs;
    maximum_level = SmartBass_MaxLevelForStrength(state->strength);
    desired_level = 0;
    release_cause = 0;

    if ((SmartBass_IsFinite(bass_db) == 0) ||
        (SmartBass_IsFinite(rms_dbfs) == 0))
    {
        state->nonfinite_count++;
        state->latest_bass_relative_db = 0.0f;
        state->latest_rms_dbfs = -240.0f;
        state->reason = SMART_BASS_REASON_INVALID;
        release_cause = 1;
    }
    else
    {
        state->latest_bass_relative_db = bass_db;
        state->latest_rms_dbfs = rms_dbfs;
        if (state->requested_enabled == 0)
        {
            state->reason = SMART_BASS_REASON_DISABLED;
        }
        else if (snapshot->valid == 0)
        {
            state->reason = SMART_BASS_REASON_INVALID;
            release_cause = 1;
        }
        else if (snapshot->warmup_complete == 0)
        {
            state->reason = SMART_BASS_REASON_NOT_WARM;
            release_cause = 1;
        }
        else if (rms_dbfs < SMART_BASS_MIN_RMS_DBFS)
        {
            state->reason = SMART_BASS_REASON_LOW_LEVEL;
            release_cause = 1;
        }
        else if ((maximum_level == 0) ||
                 (bass_db <= SMART_BASS_START_DB))
        {
            state->reason = (maximum_level == 0) ?
                SMART_BASS_REASON_DISABLED :
                SMART_BASS_REASON_BELOW_THRESHOLD;
        }
        else
        {
            state->reason = SMART_BASS_REASON_EXCESS_BASS;
            if (bass_db >= SMART_BASS_FULL_DB)
            {
                desired_level = maximum_level;
            }
            else
            {
                scaled_level =
                    ((bass_db - SMART_BASS_START_DB) /
                     (SMART_BASS_FULL_DB - SMART_BASS_START_DB)) *
                    (float)maximum_level;
                desired_level = (int)(scaled_level + 0.5f);
                if (desired_level > maximum_level)
                {
                    desired_level = maximum_level;
                }
            }
        }
    }

    if ((state->requested_enabled == 0) || (maximum_level == 0))
    {
        desired_level = 0;
    }
    state->target_level = SmartBass_ClampLevel(desired_level);
    endpoint_level = (state->transition_active != 0) ?
        state->pending_level : state->active_level;
    if ((release_cause != 0) && (endpoint_level > 0))
    {
        state->invalid_release_count++;
    }

    if (state->force_release != 0)
    {
        return SMART_BASS_RESULT_UPDATED;
    }
    if (state->target_level > endpoint_level)
    {
        state->release_confirmation_count = 0;
        SmartBass_RequestAdjacentStep(state, endpoint_level, 1);
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
            state->reason = SMART_BASS_REASON_RELEASING;
            SmartBass_RequestAdjacentStep(state, endpoint_level, -1);
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
    return SMART_BASS_RESULT_UPDATED;
}

int SmartBass_ProcessFrame(
    SMART_BASS_STATE *state,
    const short *input,
    short *output,
    int sample_count)
{
    int index;

    if ((state == 0) || (input == 0) || (output == 0) ||
        (sample_count <= 0) ||
        (sample_count > SMART_BASS_MAX_FRAME_LEN) ||
        (state->initialized == 0))
    {
        return SMART_BASS_RESULT_ERROR;
    }

    if ((state->transition_active == 0) && (state->active_level == 0))
    {
        if (input != output)
        {
            memcpy(output, input, (unsigned int)sample_count * sizeof(short));
        }
        state->processing_active = 0;
        return SMART_BASS_RESULT_UPDATED;
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

            active_output = SmartBass_ProcessPath(
                state, state->active_level, &state->active_state,
                input_value);
            pending_output = SmartBass_ProcessPath(
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
                SmartBass_FinishTransition(state);
            }
        }
        else
        {
            output_value = SmartBass_ProcessPath(
                state, state->active_level, &state->active_state,
                input_value);
        }

        if (SmartBass_IsFinite(output_value) == 0)
        {
            SmartBass_EnterSafeIdentity(state);
            output[index] = input[index];
        }
        else
        {
            output[index] = SmartBass_Saturate(state, output_value);
        }
    }
    return SMART_BASS_RESULT_UPDATED;
}

float SmartBass_GetTransitionProgress(const SMART_BASS_STATE *state)
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

float SmartBass_GetAppliedGainDb(const SMART_BASS_STATE *state)
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
                 SmartBass_GetTransitionProgress(state);
    }
    return -SMART_BASS_LEVEL_STEP_DB * level;
}

float SmartBass_GetRequestedGainDb(const SMART_BASS_STATE *state)
{
    if ((state == 0) || (state->initialized == 0))
    {
        return 0.0f;
    }
    return SmartBass_LevelGainDb(state->target_level);
}

int SmartBass_GetReason(const SMART_BASS_STATE *state)
{
    if ((state == 0) || (state->initialized == 0))
    {
        return SMART_BASS_REASON_INVALID;
    }
    return state->reason;
}

#endif /* EQ_ENABLE_SMART_BASS */
