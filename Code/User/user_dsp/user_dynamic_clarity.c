/**
 * user_dynamic_clarity.c
 *
 * Dynamic Clarity V1 applies only a bounded 400 Hz peaking cut after the
 * static equalizer and optional Smart Bass stage. Coefficients are fixed at
 * initialization; runtime decisions only select adjacent cached levels.
 */

#include "user_dynamic_clarity.h"

#if EQ_ENABLE_DYNAMIC_CLARITY != 0

#include "float.h"
#include "string.h"

static int DynamicClarity_IsFinite(float value)
{
    return ((value == value) && (value <= FLT_MAX) &&
            (value >= -FLT_MAX));
}

static int DynamicClarity_ClampLevel(int level)
{
    if (level < 0)
    {
        return 0;
    }
    if (level >= DYNAMIC_CLARITY_LEVEL_COUNT)
    {
        return DYNAMIC_CLARITY_LEVEL_COUNT - 1;
    }
    return level;
}

static int DynamicClarity_ClampStrength(int strength)
{
    if (strength < DYNAMIC_CLARITY_STRENGTH_OFF)
    {
        return DYNAMIC_CLARITY_STRENGTH_OFF;
    }
    if (strength > DYNAMIC_CLARITY_STRENGTH_HIGH)
    {
        return DYNAMIC_CLARITY_STRENGTH_HIGH;
    }
    return strength;
}

static int DynamicClarity_MaxLevelForStrength(int strength)
{
    static const int maximum_level[4] = { 0, 2, 4, 6 };

    return maximum_level[DynamicClarity_ClampStrength(strength)];
}

static float DynamicClarity_LevelGainDb(int level)
{
    return -DYNAMIC_CLARITY_LEVEL_STEP_DB *
           (float)DynamicClarity_ClampLevel(level);
}

static void DynamicClarity_ClearFilterState(
    DYNAMIC_CLARITY_DF2T_STATE *filter_state)
{
    filter_state->s1 = 0.0f;
    filter_state->s2 = 0.0f;
}

static void DynamicClarity_ClearRuntime(DYNAMIC_CLARITY_STATE *state)
{
    DynamicClarity_ClearFilterState(&state->active_state);
    DynamicClarity_ClearFilterState(&state->pending_state);
    state->active_level = 0;
    state->target_level = 0;
    state->pending_level = 0;
    state->queued_level = 0;
    state->queued_level_valid = 0;
    state->transition_active = 0;
    state->transition_remaining = 0;
    state->transition_total = DYNAMIC_CLARITY_TRANSITION_SAMPLES;
    state->requested_enabled = 0;
    state->processing_active = 0;
    state->strength = DYNAMIC_CLARITY_STRENGTH_MEDIUM;
    state->force_release = 0;
    state->reason = DYNAMIC_CLARITY_REASON_DISABLED;
    state->latest_mud_relative_db = 0.0f;
    state->latest_presence_relative_db = 0.0f;
    state->latest_masking_db = 0.0f;
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

static int DynamicClarity_StartTransition(
    DYNAMIC_CLARITY_STATE *state,
    int requested_level)
{
    int next_level;

    if ((state == 0) || (state->initialized == 0) ||
        (state->transition_active != 0))
    {
        return 0;
    }

    next_level = DynamicClarity_ClampLevel(requested_level);
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
        DynamicClarity_ClearFilterState(&state->pending_state);
    }
    state->transition_total = DYNAMIC_CLARITY_TRANSITION_SAMPLES;
    state->transition_remaining = state->transition_total;
    state->transition_active = 1;
    state->processing_active = 1;
    state->transition_count++;
    return 1;
}

static void DynamicClarity_RequestAdjacentStep(
    DYNAMIC_CLARITY_STATE *state,
    int endpoint_level,
    int direction)
{
    int next_level;

    next_level = DynamicClarity_ClampLevel(endpoint_level + direction);
    if (state->transition_active != 0)
    {
        state->queued_level = next_level;
        state->queued_level_valid =
            (next_level != endpoint_level) ? 1 : 0;
    }
    else
    {
        (void)DynamicClarity_StartTransition(state, next_level);
    }
}

static void DynamicClarity_FinishTransition(
    DYNAMIC_CLARITY_STATE *state)
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
            (void)DynamicClarity_StartTransition(
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
        (void)DynamicClarity_StartTransition(state, queued_level);
    }
    else
    {
        state->processing_active =
            (state->active_level != 0) ? 1 : 0;
    }
}

static float DynamicClarity_ProcessBiquad(
    const DYNAMIC_CLARITY_BIQUAD *coefficient,
    DYNAMIC_CLARITY_DF2T_STATE *filter_state,
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

static float DynamicClarity_ProcessPath(
    const DYNAMIC_CLARITY_STATE *state,
    int level,
    DYNAMIC_CLARITY_DF2T_STATE *filter_state,
    float input)
{
    if (level == 0)
    {
        return input;
    }
    return DynamicClarity_ProcessBiquad(
        &state->coefficient_table[level], filter_state, input);
}

static short DynamicClarity_Saturate(DYNAMIC_CLARITY_STATE *state,
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

static void DynamicClarity_EnterSafeIdentity(
    DYNAMIC_CLARITY_STATE *state)
{
    state->nonfinite_count++;
    DynamicClarity_ClearFilterState(&state->active_state);
    DynamicClarity_ClearFilterState(&state->pending_state);
    state->active_level = 0;
    state->target_level = 0;
    state->pending_level = 0;
    state->queued_level_valid = 0;
    state->transition_active = 0;
    state->transition_remaining = 0;
    state->force_release = 0;
    state->processing_active = 0;
    state->reason = DYNAMIC_CLARITY_REASON_INVALID;
}

void DynamicClarity_Init(DYNAMIC_CLARITY_STATE *state)
{
    int level;

    if (state == 0)
    {
        return;
    }
    memset(state, 0, sizeof(*state));
    for (level = 0; level < DYNAMIC_CLARITY_LEVEL_COUNT; level++)
    {
        if (!Equalizer_DesignRbjPeakingAt(
                &state->coefficient_table[level],
                DYNAMIC_CLARITY_CENTER_HZ,
                DynamicClarity_LevelGainDb(level),
                DYNAMIC_CLARITY_BANDWIDTH_OCTAVES))
        {
            memset(state, 0, sizeof(*state));
            return;
        }
    }
    DynamicClarity_ClearRuntime(state);
    state->initialized = 1;
}

void DynamicClarity_Reset(DYNAMIC_CLARITY_STATE *state)
{
    if ((state == 0) || (state->initialized == 0))
    {
        return;
    }
    DynamicClarity_ClearRuntime(state);
    state->initialized = 1;
}

void DynamicClarity_InvalidateAnalysisEpoch(DYNAMIC_CLARITY_STATE *state)
{
    if ((state != 0) && (state->initialized != 0))
    {
        state->latest_analysis_count = ~0UL;
    }
}

int DynamicClarity_SetEnabled(DYNAMIC_CLARITY_STATE *state, int enabled)
{
    int normalized;

    if ((state == 0) || (state->initialized == 0))
    {
        return DYNAMIC_CLARITY_RESULT_ERROR;
    }
    normalized = (enabled != 0) ? 1 : 0;
    if (normalized == state->requested_enabled)
    {
        return DYNAMIC_CLARITY_RESULT_NO_CHANGE;
    }

    state->requested_enabled = normalized;
    state->release_confirmation_count = 0;
    if (normalized != 0)
    {
        state->force_release = 0;
        return DYNAMIC_CLARITY_RESULT_UPDATED;
    }

    state->target_level = 0;
    state->queued_level_valid = 0;
    state->reason = DYNAMIC_CLARITY_REASON_DISABLED;
    if ((state->transition_active != 0) || (state->active_level > 0))
    {
        state->force_release = 1;
        if (state->transition_active == 0)
        {
            (void)DynamicClarity_StartTransition(
                state, state->active_level - 1);
        }
    }
    else
    {
        state->force_release = 0;
        state->processing_active = 0;
    }
    return DYNAMIC_CLARITY_RESULT_UPDATED;
}

int DynamicClarity_SetStrength(DYNAMIC_CLARITY_STATE *state, int strength)
{
    int endpoint_level;
    int limit_level;
    int maximum_level;
    int normalized;

    if ((state == 0) || (state->initialized == 0))
    {
        return DYNAMIC_CLARITY_RESULT_ERROR;
    }
    normalized = DynamicClarity_ClampStrength(strength);
    if (normalized == state->strength)
    {
        return DYNAMIC_CLARITY_RESULT_NO_CHANGE;
    }
    state->strength = normalized;
    maximum_level = DynamicClarity_MaxLevelForStrength(normalized);
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
            DYNAMIC_CLARITY_REASON_DISABLED :
            DYNAMIC_CLARITY_REASON_RELEASING;
        state->queued_level_valid = 0;
        state->force_release = 1;
        if (state->transition_active == 0)
        {
            (void)DynamicClarity_StartTransition(
                state, state->active_level - 1);
        }
    }
    else if (state->requested_enabled != 0)
    {
        state->force_release = 0;
    }
    return DYNAMIC_CLARITY_RESULT_UPDATED;
}

int DynamicClarity_UpdateFromFeature(
    DYNAMIC_CLARITY_STATE *state,
    const AUDIO_FEATURE_SNAPSHOT *snapshot)
{
    float masking_db;
    float mud_db;
    float presence_db;
    float rms_dbfs;
    float scaled_level;
    int endpoint_level;
    int desired_level;
    int maximum_level;
    int release_cause;

    if ((state == 0) || (snapshot == 0) ||
        (state->initialized == 0))
    {
        return DYNAMIC_CLARITY_RESULT_ERROR;
    }
    if (snapshot->analysis_count == state->latest_analysis_count)
    {
        return DYNAMIC_CLARITY_RESULT_NO_CHANGE;
    }

    state->latest_analysis_count = snapshot->analysis_count;
    state->decision_count++;
    mud_db = snapshot->relative_db[AUDIO_FEATURE_MUD];
    presence_db = snapshot->relative_db[AUDIO_FEATURE_PRESENCE];
    rms_dbfs = snapshot->rms_dbfs;
    maximum_level = DynamicClarity_MaxLevelForStrength(state->strength);
    desired_level = 0;
    release_cause = 0;

    if ((DynamicClarity_IsFinite(mud_db) == 0) ||
        (DynamicClarity_IsFinite(presence_db) == 0) ||
        (DynamicClarity_IsFinite(rms_dbfs) == 0))
    {
        state->nonfinite_count++;
        state->latest_mud_relative_db = 0.0f;
        state->latest_presence_relative_db = 0.0f;
        state->latest_masking_db = 0.0f;
        state->latest_rms_dbfs = -240.0f;
        state->reason = DYNAMIC_CLARITY_REASON_INVALID;
        release_cause = 1;
    }
    else
    {
        masking_db = mud_db - presence_db;
        if (DynamicClarity_IsFinite(masking_db) == 0)
        {
            state->nonfinite_count++;
            state->latest_mud_relative_db = 0.0f;
            state->latest_presence_relative_db = 0.0f;
            state->latest_masking_db = 0.0f;
            state->latest_rms_dbfs = -240.0f;
            state->reason = DYNAMIC_CLARITY_REASON_INVALID;
            release_cause = 1;
        }
        else
        {
            state->latest_mud_relative_db = mud_db;
            state->latest_presence_relative_db = presence_db;
            state->latest_masking_db = masking_db;
            state->latest_rms_dbfs = rms_dbfs;
            if (snapshot->valid == 0)
            {
                state->reason = DYNAMIC_CLARITY_REASON_INVALID;
                release_cause = 1;
            }
            else if (state->requested_enabled == 0)
            {
                state->reason = DYNAMIC_CLARITY_REASON_DISABLED;
            }
            else if (snapshot->warmup_complete == 0)
            {
                state->reason = DYNAMIC_CLARITY_REASON_NOT_WARM;
                release_cause = 1;
            }
            else if (rms_dbfs < DYNAMIC_CLARITY_MIN_RMS_DBFS)
            {
                state->reason = DYNAMIC_CLARITY_REASON_LOW_LEVEL;
                release_cause = 1;
            }
            else if (mud_db < DYNAMIC_CLARITY_MIN_MUD_DB)
            {
                state->reason = DYNAMIC_CLARITY_REASON_WEAK_MUD;
                release_cause = 1;
            }
            else if (presence_db < DYNAMIC_CLARITY_MIN_PRESENCE_DB)
            {
                state->reason = DYNAMIC_CLARITY_REASON_WEAK_PRESENCE;
                release_cause = 1;
            }
            else if ((maximum_level == 0) ||
                     (masking_db <= DYNAMIC_CLARITY_START_DB))
            {
                state->reason = (maximum_level == 0) ?
                    DYNAMIC_CLARITY_REASON_DISABLED :
                    DYNAMIC_CLARITY_REASON_BELOW_THRESHOLD;
            }
            else
            {
                state->reason = DYNAMIC_CLARITY_REASON_MASKING;
                if (masking_db >= DYNAMIC_CLARITY_FULL_DB)
                {
                    desired_level = maximum_level;
                }
                else
                {
                    scaled_level =
                        ((masking_db - DYNAMIC_CLARITY_START_DB) /
                         (DYNAMIC_CLARITY_FULL_DB -
                          DYNAMIC_CLARITY_START_DB)) *
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
    state->target_level = DynamicClarity_ClampLevel(desired_level);
    endpoint_level = (state->transition_active != 0) ?
        state->pending_level : state->active_level;
    if ((release_cause != 0) && (endpoint_level > 0))
    {
        state->invalid_release_count++;
    }

    if (state->force_release != 0)
    {
        return DYNAMIC_CLARITY_RESULT_UPDATED;
    }
    if (state->target_level > endpoint_level)
    {
        state->release_confirmation_count = 0;
        DynamicClarity_RequestAdjacentStep(state, endpoint_level, 1);
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
            state->reason = DYNAMIC_CLARITY_REASON_RELEASING;
            DynamicClarity_RequestAdjacentStep(state, endpoint_level, -1);
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
    return DYNAMIC_CLARITY_RESULT_UPDATED;
}

int DynamicClarity_ProcessFrame(
    DYNAMIC_CLARITY_STATE *state,
    const short *input,
    short *output,
    int sample_count)
{
    int index;

    if ((state == 0) || (input == 0) || (output == 0) ||
        (sample_count <= 0) ||
        (sample_count > DYNAMIC_CLARITY_MAX_FRAME_LEN) ||
        (state->initialized == 0))
    {
        return DYNAMIC_CLARITY_RESULT_ERROR;
    }

    if ((state->transition_active == 0) && (state->active_level == 0))
    {
        if (input != output)
        {
            memcpy(output, input,
                   (unsigned int)sample_count * sizeof(short));
        }
        state->processing_active = 0;
        return DYNAMIC_CLARITY_RESULT_UPDATED;
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

            active_output = DynamicClarity_ProcessPath(
                state, state->active_level, &state->active_state,
                input_value);
            pending_output = DynamicClarity_ProcessPath(
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
                DynamicClarity_FinishTransition(state);
            }
        }
        else
        {
            output_value = DynamicClarity_ProcessPath(
                state, state->active_level, &state->active_state,
                input_value);
        }

        if (DynamicClarity_IsFinite(output_value) == 0)
        {
            DynamicClarity_EnterSafeIdentity(state);
            output[index] = input[index];
        }
        else
        {
            output[index] = DynamicClarity_Saturate(state, output_value);
        }
    }
    return DYNAMIC_CLARITY_RESULT_UPDATED;
}

float DynamicClarity_GetTransitionProgress(
    const DYNAMIC_CLARITY_STATE *state)
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

float DynamicClarity_GetAppliedGainDb(
    const DYNAMIC_CLARITY_STATE *state)
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
                 DynamicClarity_GetTransitionProgress(state);
    }
    return -DYNAMIC_CLARITY_LEVEL_STEP_DB * level;
}

float DynamicClarity_GetRequestedGainDb(
    const DYNAMIC_CLARITY_STATE *state)
{
    if ((state == 0) || (state->initialized == 0))
    {
        return 0.0f;
    }
    return DynamicClarity_LevelGainDb(state->target_level);
}

int DynamicClarity_GetReason(const DYNAMIC_CLARITY_STATE *state)
{
    if ((state == 0) || (state->initialized == 0))
    {
        return DYNAMIC_CLARITY_REASON_INVALID;
    }
    return state->reason;
}

#endif /* EQ_ENABLE_DYNAMIC_CLARITY */
