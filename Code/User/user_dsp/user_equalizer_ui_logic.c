/**
 * user_equalizer_ui_logic.c
 *
 * Hardware-independent Project 3.3 UI state, layout, and touch logic.
 */

#include "user_equalizer_ui_logic.h"
#include "string.h"

#if defined(EQ_ALGO_ONLY) || !defined(DSP_LAB_PROJECT_SELECT) || \
    (DSP_LAB_PROJECT_SELECT == 33)

#define EQ_UI_JOB_BIT(job_) (1UL << ((job_) - 1))
#define EQ_UI_PRESET_MASK 0x001FUL
#define EQ_UI_CHAIN_MASK 0x00E0UL
#define EQ_UI_ANALYZER_MASK 0x0F00UL
#define EQ_UI_DYNAMIC_MASK 0x7000UL

#define EQ_UI_CATEGORY_PRESET   0
#define EQ_UI_CATEGORY_DYNAMIC  1
#define EQ_UI_CATEGORY_CHAIN    2
#define EQ_UI_CATEGORY_ANALYZER 3
#define EQ_UI_CATEGORY_COUNT    4

const EQ_UI_RECT EQ_UI_PRESET_RECTS[EQ_UI_PRESET_COUNT] =
{
    { 26, 34, 140, 40 },
    { 178, 34, 140, 40 },
    { 330, 34, 140, 40 },
    { 482, 34, 140, 40 },
    { 634, 34, 140, 40 }
};

const EQ_UI_RECT EQ_UI_CHAIN_RECTS[EQ_UI_CHAIN_COUNT] =
{
    { 342, 82, 52, 26 },
    { 438, 82, 44, 26 },
    { 526, 82, 36, 26 }
};

const EQ_UI_RECT EQ_UI_ANALYZER_RECTS[EQ_UI_ANALYZER_COUNT] =
{
    { 60, 116, 125, 190 },
    { 245, 116, 125, 190 },
    { 430, 116, 125, 190 },
    { 615, 116, 125, 190 }
};

const EQ_UI_RECT EQ_UI_DYNAMIC_RECTS[EQ_UI_DYNAMIC_COUNT] =
{
    { 20, 342, 760, 40 },
    { 20, 386, 760, 40 },
    { 20, 430, 760, 40 }
};

const EQ_UI_RECT EQ_UI_DYNAMIC_TOGGLE_RECTS[EQ_UI_DYNAMIC_COUNT] =
{
    { 166, 342, 96, 40 },
    { 166, 386, 96, 40 },
    { 166, 430, 96, 40 }
};

const EQ_UI_RECT EQ_UI_DYNAMIC_STRENGTH_RECTS[EQ_UI_DYNAMIC_COUNT] =
{
    { 286, 342, 96, 40 },
    { 286, 386, 96, 40 },
    { 286, 430, 96, 40 }
};

const EQ_UI_RECT EQ_UI_DYNAMIC_LEVEL_RECTS[EQ_UI_DYNAMIC_COUNT] =
{
    { 406, 342, 80, 40 },
    { 406, 386, 80, 40 },
    { 406, 430, 80, 40 }
};

const EQ_UI_HITBOX EQ_UI_HITBOXES[EQ_UI_HITBOX_COUNT] =
{
    { { 26, 34, 140, 40 }, EQ_UI_ACTION_PRESET_FLAT },
    { { 178, 34, 140, 40 }, EQ_UI_ACTION_PRESET_BASS },
    { { 330, 34, 140, 40 }, EQ_UI_ACTION_PRESET_VOCAL },
    { { 482, 34, 140, 40 }, EQ_UI_ACTION_PRESET_TREBLE },
    { { 634, 34, 140, 40 }, EQ_UI_ACTION_PRESET_V_SHAPE },
    { { 166, 342, 96, 40 }, EQ_UI_ACTION_SMART_TOGGLE },
    { { 286, 342, 96, 40 }, EQ_UI_ACTION_SMART_STRENGTH },
    { { 166, 386, 96, 40 }, EQ_UI_ACTION_CLARITY_TOGGLE },
    { { 286, 386, 96, 40 }, EQ_UI_ACTION_CLARITY_STRENGTH },
    { { 166, 430, 96, 40 }, EQ_UI_ACTION_GUARD_TOGGLE },
    { { 286, 430, 96, 40 }, EQ_UI_ACTION_GUARD_STRENGTH }
};

static int EQ_UI_ClampInt(int value, int minimum, int maximum)
{
    if (value < minimum)
    {
        return minimum;
    }
    if (value > maximum)
    {
        return maximum;
    }
    return value;
}

static int EQ_UI_AbsInt(int value)
{
    return (value < 0) ? -value : value;
}

static int EQ_UI_AnalyzerTargetPixel(const EQ_UI_STATE *state, int band)
{
    if ((state->requested.analyzer_valid == 0) ||
        (state->requested.analyzer_warm == 0))
    {
        return EQ_UI_ANALYZER_ZERO_PIXEL;
    }
    return EQ_UI_ClampInt(state->requested.band_pixel[band],
                          EQ_UI_ANALYZER_DRAW_TOP,
                          EQ_UI_ANALYZER_DRAW_BOTTOM);
}

static int EQ_UI_JobCategory(int job)
{
    if ((job >= EQ_UI_JOB_PRESET_0) && (job <= EQ_UI_JOB_PRESET_4))
        return EQ_UI_CATEGORY_PRESET;
    if ((job >= EQ_UI_JOB_DYNAMIC_0) && (job <= EQ_UI_JOB_DYNAMIC_2))
        return EQ_UI_CATEGORY_DYNAMIC;
    if ((job >= EQ_UI_JOB_CHAIN_0) && (job <= EQ_UI_JOB_CHAIN_2))
        return EQ_UI_CATEGORY_CHAIN;
    return EQ_UI_CATEGORY_ANALYZER;
}

static unsigned long EQ_UI_CategoryMinGap(int category)
{
    if (category == EQ_UI_CATEGORY_PRESET)
        return EQ_UI_PRESET_MIN_GAP_FRAMES;
    if (category == EQ_UI_CATEGORY_DYNAMIC)
        return EQ_UI_DYNAMIC_MIN_GAP_FRAMES;
    if (category == EQ_UI_CATEGORY_CHAIN)
        return EQ_UI_CHAIN_MIN_GAP_FRAMES;
    return EQ_UI_ANALYZER_MIN_GAP_FRAMES;
}

static int EQ_UI_CategoryEligible(const EQ_UI_STATE *state, int category,
                                  unsigned long process_frame)
{
    unsigned long gap;
    unsigned long global_gap;

    gap = EQ_UI_CategoryMinGap(category);
    global_gap = (category == EQ_UI_CATEGORY_PRESET) ?
        EQ_UI_PRESET_MIN_GAP_FRAMES : EQ_UI_STEADY_MIN_GAP_FRAMES;
    if ((state->last_service_frame_valid != 0U) &&
        ((process_frame - state->last_service_frame) < global_gap))
    {
        return 0;
    }
    if (((state->category_last_service_valid_mask &
          (1U << category)) != 0U) &&
        ((process_frame - state->category_last_service_frame[category]) <
         gap))
    {
        return 0;
    }
    return 1;
}

static void EQ_UI_RecordService(EQ_UI_STATE *state, int job,
                                unsigned long process_frame)
{
    int category;

    category = EQ_UI_JobCategory(job);
    state->last_service_frame = process_frame;
    state->last_service_frame_valid = 1U;
    state->category_last_service_frame[category] = process_frame;
    state->category_last_service_valid_mask |= (1U << category);
}

static int EQ_UI_NormalizePreset(int preset)
{
    if ((preset < EQ_PRESET_FLAT) || (preset > EQ_PRESET_V_SHAPE))
    {
        return EQ_PRESET_FLAT;
    }
    return preset;
}

static void EQ_UI_NormalizeSnapshot(EQ_UI_SNAPSHOT *snapshot)
{
    int band;

    snapshot->applied_preset =
        EQ_UI_NormalizePreset(snapshot->applied_preset);
    snapshot->smart_enabled = (snapshot->smart_enabled != 0) ? 1 : 0;
    snapshot->clarity_enabled = (snapshot->clarity_enabled != 0) ? 1 : 0;
    snapshot->guard_enabled = (snapshot->guard_enabled != 0) ? 1 : 0;
    snapshot->analyzer_valid = (snapshot->analyzer_valid != 0) ? 1 : 0;
    snapshot->analyzer_warm = (snapshot->analyzer_warm != 0) ? 1 : 0;
    for (band = 0; band < EQ_UI_ANALYZER_COUNT; band++)
    {
        snapshot->band_value_db[band] = EQ_UI_ClampInt(
            snapshot->band_value_db[band], -20, 20);
        snapshot->band_pixel[band] = EQ_UI_ClampInt(
            snapshot->band_pixel[band],
            EQ_UI_ANALYZER_DRAW_TOP, EQ_UI_ANALYZER_DRAW_BOTTOM);
    }
}

static int EQ_UI_JobDisplayed(const EQ_UI_STATE *state, int job)
{
    return ((state->displayed_valid_mask & EQ_UI_JOB_BIT(job)) != 0UL) ?
        1 : 0;
}

static void EQ_UI_RecomputePreset(EQ_UI_STATE *state)
{
    int button;
    int selected;
    unsigned long bit;

    state->dirty_mask &= ~EQ_UI_PRESET_MASK;
    if (((state->runtime_mask & EQ_UI_RUNTIME_PRESET) == 0U) ||
        (state->requested_valid == 0U))
    {
        return;
    }
    for (button = 0; button < EQ_UI_PRESET_COUNT; button++)
    {
        bit = EQ_UI_JOB_BIT(EQ_UI_JOB_PRESET_0 + button);
        selected = (state->requested.applied_preset == button) ? 1 : 0;
        if ((EQ_UI_JobDisplayed(state, EQ_UI_JOB_PRESET_0 + button) == 0) ||
            ((int)state->preset_displayed_selected[button] != selected))
        {
            state->dirty_mask |= bit;
        }
    }
}

static void EQ_UI_RecomputeChain(EQ_UI_STATE *state)
{
    int requested_enabled[EQ_UI_CHAIN_COUNT];
    int index;
    int job;

    state->dirty_mask &= ~EQ_UI_CHAIN_MASK;
    if (((state->runtime_mask & EQ_UI_RUNTIME_CHAIN) == 0U) ||
        (state->requested_valid == 0U))
    {
        return;
    }
    requested_enabled[0] = state->requested.smart_enabled;
    requested_enabled[1] = state->requested.clarity_enabled;
    requested_enabled[2] = state->requested.guard_enabled;
    for (index = 0; index < EQ_UI_CHAIN_COUNT; index++)
    {
        job = EQ_UI_JOB_CHAIN_0 + index;
        if ((EQ_UI_JobDisplayed(state, job) == 0) ||
            (requested_enabled[index] !=
             (int)state->chain_displayed_enabled[index]))
        {
            state->dirty_mask |= EQ_UI_JOB_BIT(job);
        }
    }
}

static void EQ_UI_RecomputeDynamics(EQ_UI_STATE *state)
{
    int requested_enabled[EQ_UI_DYNAMIC_COUNT];
    int requested_strength[EQ_UI_DYNAMIC_COUNT];
    int requested_level[EQ_UI_DYNAMIC_COUNT];
    int displayed_enabled[EQ_UI_DYNAMIC_COUNT];
    int displayed_strength[EQ_UI_DYNAMIC_COUNT];
    int displayed_level[EQ_UI_DYNAMIC_COUNT];
    int index;
    int job;
    unsigned int fields;

    state->dirty_mask &= ~EQ_UI_DYNAMIC_MASK;
    requested_enabled[0] = state->requested.smart_enabled;
    requested_enabled[1] = state->requested.clarity_enabled;
    requested_enabled[2] = state->requested.guard_enabled;
    requested_strength[0] = state->requested.smart_strength;
    requested_strength[1] = state->requested.clarity_strength;
    requested_strength[2] = state->requested.guard_strength;
    requested_level[0] = state->requested.smart_level;
    requested_level[1] = state->requested.clarity_level;
    requested_level[2] = state->requested.guard_level;
    displayed_enabled[0] = state->displayed.smart_enabled;
    displayed_enabled[1] = state->displayed.clarity_enabled;
    displayed_enabled[2] = state->displayed.guard_enabled;
    displayed_strength[0] = state->displayed.smart_strength;
    displayed_strength[1] = state->displayed.clarity_strength;
    displayed_strength[2] = state->displayed.guard_strength;
    displayed_level[0] = state->displayed.smart_level;
    displayed_level[1] = state->displayed.clarity_level;
    displayed_level[2] = state->displayed.guard_level;

    for (index = 0; index < EQ_UI_DYNAMIC_COUNT; index++)
    {
        fields = 0U;
        job = EQ_UI_JOB_DYNAMIC_0 + index;
        if (((state->runtime_mask & EQ_UI_RUNTIME_DYNAMICS) != 0U) &&
            (state->requested_valid != 0U))
        {
            if (((state->dynamic_displayed_field_valid[index] &
                  EQ_UI_DYNAMIC_FIELD_ENABLED) == 0U) ||
                (requested_enabled[index] != displayed_enabled[index]))
            {
                fields |= EQ_UI_DYNAMIC_FIELD_ENABLED;
            }
            if (((state->dynamic_displayed_field_valid[index] &
                  EQ_UI_DYNAMIC_FIELD_STRENGTH) == 0U) ||
                (requested_strength[index] != displayed_strength[index]))
            {
                fields |= EQ_UI_DYNAMIC_FIELD_STRENGTH;
            }
            if (((state->dynamic_displayed_field_valid[index] &
                  EQ_UI_DYNAMIC_FIELD_LEVEL) == 0U) ||
                (requested_level[index] != displayed_level[index]))
            {
                fields |= EQ_UI_DYNAMIC_FIELD_LEVEL;
            }
        }
        state->dynamic_field_mask[index] = fields;
        if (fields != 0U)
        {
            state->dirty_mask |= EQ_UI_JOB_BIT(job);
        }
    }
}

static void EQ_UI_RecomputeAnalyzer(EQ_UI_STATE *state,
                                    unsigned long process_frame)
{
    int band;
    int job;
    int current_pixel;
    int target_pixel;
    int validity_changed;
    int pixel_changed;
    int value_changed;
    int valid_and_warm;
    int bar_was_pending;
    unsigned long bar_age;
    unsigned long value_age;
    unsigned int fields;

    state->dirty_mask &= ~EQ_UI_ANALYZER_MASK;
    if (((state->runtime_mask & EQ_UI_RUNTIME_ANALYZER) == 0U) ||
        (state->requested_valid == 0U))
    {
        for (band = 0; band < EQ_UI_ANALYZER_COUNT; band++)
        {
            state->analyzer_field_mask[band] = 0U;
        }
        return;
    }
    for (band = 0; band < EQ_UI_ANALYZER_COUNT; band++)
    {
        job = EQ_UI_JOB_ANALYZER_0 + band;
        fields = 0U;
        bar_was_pending =
            ((state->analyzer_field_mask[band] &
              EQ_UI_ANALYZER_FIELD_BAR) != 0U) ? 1 : 0;
        validity_changed =
            (state->requested.analyzer_valid !=
             (int)state->analyzer_displayed_valid[band]) ||
            (state->requested.analyzer_warm !=
             (int)state->analyzer_displayed_warm[band]);
        valid_and_warm = (state->requested.analyzer_valid != 0) &&
                         (state->requested.analyzer_warm != 0);
        current_pixel = EQ_UI_ClampInt(state->displayed.band_pixel[band],
                                       EQ_UI_ANALYZER_BAR_TOP,
                                       EQ_UI_ANALYZER_BAR_BOTTOM);
        target_pixel = EQ_UI_AnalyzerTargetPixel(state, band);
        pixel_changed = target_pixel - current_pixel;
        value_changed = state->requested.band_value_db[band] -
                        state->displayed.band_value_db[band];
        bar_age = process_frame - state->band_last_display_frame[band];
        value_age = process_frame - state->band_last_value_frame[band];
        if ((pixel_changed != 0) &&
            (bar_was_pending || validity_changed ||
             (EQ_UI_AbsInt(pixel_changed) >=
              EQ_UI_ANALYZER_HYSTERESIS_PX) ||
             (bar_age >= EQ_UI_ANALYZER_VALUE_MAX_AGE_FRAMES)))
        {
            fields |= EQ_UI_ANALYZER_FIELD_BAR;
        }
        if (((state->analyzer_displayed_field_valid[band] &
              EQ_UI_ANALYZER_FIELD_VALUE) == 0U) ||
            validity_changed ||
            (valid_and_warm &&
             (value_changed != 0) &&
             ((EQ_UI_AbsInt(value_changed) >=
                EQ_UI_ANALYZER_VALUE_DELTA_DB) ||
              (value_age >= EQ_UI_ANALYZER_VALUE_MAX_AGE_FRAMES))))
        {
            fields |= EQ_UI_ANALYZER_FIELD_VALUE;
        }
        state->analyzer_field_mask[band] = fields;
        if (fields != 0U)
        {
            state->dirty_mask |= EQ_UI_JOB_BIT(job);
        }
    }
}

static void EQ_UI_RecomputeAll(EQ_UI_STATE *state,
                               unsigned long process_frame)
{
    EQ_UI_RecomputePreset(state);
    EQ_UI_RecomputeDynamics(state);
    EQ_UI_RecomputeChain(state);
    EQ_UI_RecomputeAnalyzer(state, process_frame);
}

void EqualizerUiLogic_Init(EQ_UI_STATE *state)
{
    int band;

    if (state == 0)
    {
        return;
    }
    memset(state, 0, sizeof(*state));
    state->requested.applied_preset = EQ_PRESET_FLAT;
    state->displayed.applied_preset = EQ_PRESET_FLAT;
    for (band = 0; band < EQ_UI_ANALYZER_COUNT; band++)
    {
        state->requested.band_pixel[band] = EQ_UI_ANALYZER_ZERO_PIXEL;
        state->displayed.band_pixel[band] = EQ_UI_ANALYZER_ZERO_PIXEL;
        state->analyzer_displayed_field_valid[band] =
            EQ_UI_ANALYZER_FIELD_BAR;
    }
}

void EqualizerUiLogic_Request(EQ_UI_STATE *state,
                              const EQ_UI_SNAPSHOT *snapshot,
                              unsigned int runtime_mask,
                              unsigned long process_frame)
{
    if ((state == 0) || (snapshot == 0))
    {
        return;
    }
    state->runtime_mask = runtime_mask & EQ_UI_RUNTIME_ALL;
    state->requested = *snapshot;
    state->request_frame = process_frame;
    EQ_UI_NormalizeSnapshot(&state->requested);
    state->requested_valid = 1U;
    EQ_UI_RecomputeAll(state, process_frame);
}

int EqualizerUiLogic_HasPending(const EQ_UI_STATE *state)
{
    return ((state != 0) && (state->dirty_mask != 0UL)) ? 1 : 0;
}

int EqualizerUiLogic_HasEligibleJob(const EQ_UI_STATE *state,
                                    unsigned long process_frame)
{
    if (state == 0)
    {
        return 0;
    }
    if (((state->dirty_mask & EQ_UI_PRESET_MASK) != 0UL) &&
        EQ_UI_CategoryEligible(state, EQ_UI_CATEGORY_PRESET,
                               process_frame))
    {
        return 1;
    }
    if (((state->dirty_mask & EQ_UI_DYNAMIC_MASK) != 0UL) &&
        EQ_UI_CategoryEligible(state, EQ_UI_CATEGORY_DYNAMIC,
                               process_frame))
    {
        return 1;
    }
    if (((state->dirty_mask & EQ_UI_CHAIN_MASK) != 0UL) &&
        EQ_UI_CategoryEligible(state, EQ_UI_CATEGORY_CHAIN,
                               process_frame))
    {
        return 1;
    }
    return (((state->dirty_mask & EQ_UI_ANALYZER_MASK) != 0UL) &&
            EQ_UI_CategoryEligible(state, EQ_UI_CATEGORY_ANALYZER,
                                   process_frame)) ? 1 : 0;
}

static int EQ_UI_SelectFromRange(unsigned long dirty_mask,
                                 int first_job, int count,
                                 unsigned int *cursor)
{
    int checked;
    int index;
    int job;

    for (checked = 0; checked < count; checked++)
    {
        index = ((int)(*cursor) + checked) % count;
        job = first_job + index;
        if ((dirty_mask & EQ_UI_JOB_BIT(job)) != 0UL)
        {
            *cursor = (unsigned int)((index + 1) % count);
            return job;
        }
    }
    return EQ_UI_JOB_NONE;
}

int EqualizerUiLogic_SelectJob(EQ_UI_STATE *state,
                               unsigned long process_frame)
{
    int job;

    if (state == 0)
    {
        return EQ_UI_JOB_NONE;
    }
    if (EQ_UI_CategoryEligible(state, EQ_UI_CATEGORY_PRESET,
                               process_frame) != 0)
    {
        job = EQ_UI_SelectFromRange(state->dirty_mask,
                                   EQ_UI_JOB_PRESET_0,
                                   EQ_UI_PRESET_COUNT,
                                   &state->preset_cursor);
        if (job != EQ_UI_JOB_NONE)
        {
            state->non_analyzer_streak++;
            return job;
        }
    }
    if ((state->non_analyzer_streak >= 3U) &&
        ((state->dirty_mask & EQ_UI_ANALYZER_MASK) != 0UL) &&
        (EQ_UI_CategoryEligible(state, EQ_UI_CATEGORY_ANALYZER,
                                process_frame) != 0))
    {
        job = EQ_UI_SelectFromRange(state->dirty_mask,
                                   EQ_UI_JOB_ANALYZER_0,
                                   EQ_UI_ANALYZER_COUNT,
                                   &state->analyzer_cursor);
        state->non_analyzer_streak = 0U;
        return job;
    }
    if (EQ_UI_CategoryEligible(state, EQ_UI_CATEGORY_DYNAMIC,
                               process_frame) != 0)
    {
        job = EQ_UI_SelectFromRange(state->dirty_mask,
                                   EQ_UI_JOB_DYNAMIC_0,
                                   EQ_UI_DYNAMIC_COUNT,
                                   &state->dynamic_cursor);
        if (job != EQ_UI_JOB_NONE)
        {
            state->non_analyzer_streak++;
            return job;
        }
    }
    if (EQ_UI_CategoryEligible(state, EQ_UI_CATEGORY_CHAIN,
                               process_frame) != 0)
    {
        job = EQ_UI_SelectFromRange(state->dirty_mask,
                                   EQ_UI_JOB_CHAIN_0,
                                   EQ_UI_CHAIN_COUNT,
                                   &state->chain_cursor);
        if (job != EQ_UI_JOB_NONE)
        {
            state->non_analyzer_streak++;
            return job;
        }
    }
    job = EQ_UI_JOB_NONE;
    if (EQ_UI_CategoryEligible(state, EQ_UI_CATEGORY_ANALYZER,
                               process_frame) != 0)
    {
        job = EQ_UI_SelectFromRange(state->dirty_mask,
                                   EQ_UI_JOB_ANALYZER_0,
                                   EQ_UI_ANALYZER_COUNT,
                                   &state->analyzer_cursor);
        if (job != EQ_UI_JOB_NONE)
        {
            state->non_analyzer_streak = 0U;
        }
    }
    return job;
}

unsigned int EqualizerUiLogic_DynamicFieldMask(
    const EQ_UI_STATE *state, int dynamic_index)
{
    if ((state == 0) || (dynamic_index < 0) ||
        (dynamic_index >= EQ_UI_DYNAMIC_COUNT))
    {
        return 0U;
    }
    return state->dynamic_field_mask[dynamic_index];
}

unsigned int EqualizerUiLogic_AnalyzerFieldMask(
    const EQ_UI_STATE *state, int analyzer_index)
{
    if ((state == 0) || (analyzer_index < 0) ||
        (analyzer_index >= EQ_UI_ANALYZER_COUNT))
    {
        return 0U;
    }
    return state->analyzer_field_mask[analyzer_index];
}

unsigned int EqualizerUiLogic_AnalyzerNextField(
    const EQ_UI_STATE *state, int analyzer_index)
{
    unsigned int fields;
    int validity_changed;

    if ((state == 0) || (analyzer_index < 0) ||
        (analyzer_index >= EQ_UI_ANALYZER_COUNT))
    {
        return 0U;
    }
    fields = state->analyzer_field_mask[analyzer_index];
    validity_changed =
        (state->requested.analyzer_valid !=
         (int)state->analyzer_displayed_valid[analyzer_index]) ||
        (state->requested.analyzer_warm !=
         (int)state->analyzer_displayed_warm[analyzer_index]);
    if (((fields & EQ_UI_ANALYZER_FIELD_VALUE) != 0U) &&
        validity_changed)
    {
        return EQ_UI_ANALYZER_FIELD_VALUE;
    }
    if (((fields & EQ_UI_ANALYZER_FIELD_VALUE) != 0U) &&
        ((state->request_frame -
          state->band_last_value_frame[analyzer_index]) >=
         EQ_UI_ANALYZER_VALUE_MAX_AGE_FRAMES))
    {
        return EQ_UI_ANALYZER_FIELD_VALUE;
    }
    if ((fields & EQ_UI_ANALYZER_FIELD_BAR) != 0U)
    {
        return EQ_UI_ANALYZER_FIELD_BAR;
    }
    return fields & EQ_UI_ANALYZER_FIELD_VALUE;
}

int EqualizerUiLogic_AnalyzerNextPixel(
    const EQ_UI_STATE *state, int analyzer_index)
{
    int current;
    int target;
    int phase_target;
    int delta;

    if ((state == 0) || (analyzer_index < 0) ||
        (analyzer_index >= EQ_UI_ANALYZER_COUNT))
    {
        return EQ_UI_ANALYZER_ZERO_PIXEL;
    }
    current = EQ_UI_ClampInt(state->displayed.band_pixel[analyzer_index],
                             EQ_UI_ANALYZER_DRAW_TOP,
                             EQ_UI_ANALYZER_DRAW_BOTTOM);
    target = EQ_UI_AnalyzerTargetPixel(state, analyzer_index);
    phase_target = target;
    if (((current < EQ_UI_ANALYZER_ZERO_PIXEL) &&
         (target > EQ_UI_ANALYZER_ZERO_PIXEL)) ||
        ((current > EQ_UI_ANALYZER_ZERO_PIXEL) &&
         (target < EQ_UI_ANALYZER_ZERO_PIXEL)))
    {
        phase_target = EQ_UI_ANALYZER_ZERO_PIXEL;
    }
    delta = phase_target - current;
    delta = EQ_UI_ClampInt(delta, -EQ_UI_ANALYZER_MAX_STRIP_HEIGHT,
                           EQ_UI_ANALYZER_MAX_STRIP_HEIGHT);
    return current + delta;
}

void EqualizerUiLogic_CompleteJob(EQ_UI_STATE *state, int job,
                                  unsigned long process_frame)
{
    int index;
    unsigned int field;

    if ((state == 0) || (job < 1) || (job > EQ_UI_JOB_COUNT))
    {
        return;
    }
    if ((job >= EQ_UI_JOB_DYNAMIC_0) &&
        (job <= EQ_UI_JOB_DYNAMIC_2))
    {
        index = job - EQ_UI_JOB_DYNAMIC_0;
        field = state->dynamic_field_mask[index];
        if ((field & EQ_UI_DYNAMIC_FIELD_ENABLED) != 0U)
            field = EQ_UI_DYNAMIC_FIELD_ENABLED;
        else if ((field & EQ_UI_DYNAMIC_FIELD_STRENGTH) != 0U)
            field = EQ_UI_DYNAMIC_FIELD_STRENGTH;
        else
            field &= EQ_UI_DYNAMIC_FIELD_LEVEL;
        EqualizerUiLogic_CompleteDynamicField(
            state, job, field, process_frame);
        return;
    }
    if ((job >= EQ_UI_JOB_ANALYZER_0) &&
        (job <= EQ_UI_JOB_ANALYZER_3))
    {
        index = job - EQ_UI_JOB_ANALYZER_0;
        EqualizerUiLogic_CompleteAnalyzerField(
            state, job,
            EqualizerUiLogic_AnalyzerNextField(state, index),
            process_frame);
        return;
    }
    state->dirty_mask &= ~EQ_UI_JOB_BIT(job);
    state->displayed_valid_mask |= EQ_UI_JOB_BIT(job);
    if ((job >= EQ_UI_JOB_PRESET_0) && (job <= EQ_UI_JOB_PRESET_4))
    {
        index = job - EQ_UI_JOB_PRESET_0;
        state->preset_displayed_selected[index] =
            (unsigned char)((state->requested.applied_preset == index) ?
                            1U : 0U);
        if ((state->dirty_mask & EQ_UI_PRESET_MASK) == 0UL)
        {
            state->displayed.applied_preset =
                state->requested.applied_preset;
        }
    }
    else if ((job >= EQ_UI_JOB_CHAIN_0) && (job <= EQ_UI_JOB_CHAIN_2))
    {
        index = job - EQ_UI_JOB_CHAIN_0;
        state->chain_displayed_enabled[index] =
            (unsigned char)((index == 0) ? state->requested.smart_enabled :
                            ((index == 1) ?
                             state->requested.clarity_enabled :
                             state->requested.guard_enabled));
    }
    EQ_UI_RecordService(state, job, process_frame);
}

void EqualizerUiLogic_CompleteDynamicField(
    EQ_UI_STATE *state, int job, unsigned int completed_fields,
    unsigned long process_frame)
{
    int index;
    unsigned int fields;

    if ((state == 0) || (job < EQ_UI_JOB_DYNAMIC_0) ||
        (job > EQ_UI_JOB_DYNAMIC_2))
    {
        return;
    }
    index = job - EQ_UI_JOB_DYNAMIC_0;
    fields = completed_fields & state->dynamic_field_mask[index] &
             EQ_UI_DYNAMIC_FIELD_ALL;
    if (fields == 0U)
    {
        return;
    }
    if (index == 0)
    {
        if ((fields & EQ_UI_DYNAMIC_FIELD_ENABLED) != 0U)
            state->displayed.smart_enabled = state->requested.smart_enabled;
        if ((fields & EQ_UI_DYNAMIC_FIELD_STRENGTH) != 0U)
            state->displayed.smart_strength = state->requested.smart_strength;
        if ((fields & EQ_UI_DYNAMIC_FIELD_LEVEL) != 0U)
            state->displayed.smart_level = state->requested.smart_level;
    }
    else if (index == 1)
    {
        if ((fields & EQ_UI_DYNAMIC_FIELD_ENABLED) != 0U)
        {
            state->displayed.clarity_enabled =
                state->requested.clarity_enabled;
        }
        if ((fields & EQ_UI_DYNAMIC_FIELD_STRENGTH) != 0U)
        {
            state->displayed.clarity_strength =
                state->requested.clarity_strength;
        }
        if ((fields & EQ_UI_DYNAMIC_FIELD_LEVEL) != 0U)
        {
            state->displayed.clarity_level =
                state->requested.clarity_level;
        }
    }
    else
    {
        if ((fields & EQ_UI_DYNAMIC_FIELD_ENABLED) != 0U)
            state->displayed.guard_enabled = state->requested.guard_enabled;
        if ((fields & EQ_UI_DYNAMIC_FIELD_STRENGTH) != 0U)
            state->displayed.guard_strength = state->requested.guard_strength;
        if ((fields & EQ_UI_DYNAMIC_FIELD_LEVEL) != 0U)
            state->displayed.guard_level = state->requested.guard_level;
    }
    state->dynamic_displayed_field_valid[index] |=
        (unsigned char)fields;
    state->dynamic_field_mask[index] &= ~fields;
    if (state->dynamic_field_mask[index] == 0U)
    {
        state->dirty_mask &= ~EQ_UI_JOB_BIT(job);
        state->displayed_valid_mask |= EQ_UI_JOB_BIT(job);
    }
    EQ_UI_RecordService(state, job, process_frame);
}

void EqualizerUiLogic_CompleteAnalyzerField(
    EQ_UI_STATE *state, int job, unsigned int completed_field,
    unsigned long process_frame)
{
    int index;
    int next_pixel;
    int target_pixel;
    int pending;
    int band;
    unsigned int field;

    if ((state == 0) || (job < EQ_UI_JOB_ANALYZER_0) ||
        (job > EQ_UI_JOB_ANALYZER_3))
    {
        return;
    }
    index = job - EQ_UI_JOB_ANALYZER_0;
    field = completed_field & state->analyzer_field_mask[index] &
            EQ_UI_ANALYZER_FIELD_ALL;
    if (field == 0U)
    {
        return;
    }
    if ((field & (field - 1U)) != 0U)
    {
        field = EqualizerUiLogic_AnalyzerNextField(state, index);
    }
    if (field == EQ_UI_ANALYZER_FIELD_BAR)
    {
        next_pixel = EqualizerUiLogic_AnalyzerNextPixel(state, index);
        target_pixel = EQ_UI_AnalyzerTargetPixel(state, index);
        state->displayed.band_pixel[index] = next_pixel;
        state->band_last_display_frame[index] = process_frame;
        state->analyzer_displayed_field_valid[index] |=
            EQ_UI_ANALYZER_FIELD_BAR;
        if (next_pixel == target_pixel)
        {
            state->analyzer_field_mask[index] &=
                ~EQ_UI_ANALYZER_FIELD_BAR;
        }
    }
    else
    {
        state->displayed.band_value_db[index] =
            state->requested.band_value_db[index];
        state->analyzer_displayed_valid[index] =
            (unsigned char)state->requested.analyzer_valid;
        state->analyzer_displayed_warm[index] =
            (unsigned char)state->requested.analyzer_warm;
        state->band_last_value_frame[index] = process_frame;
        state->analyzer_displayed_field_valid[index] |=
            EQ_UI_ANALYZER_FIELD_VALUE;
        state->analyzer_field_mask[index] &=
            ~EQ_UI_ANALYZER_FIELD_VALUE;
    }
    if (state->analyzer_field_mask[index] == 0U)
    {
        state->dirty_mask &= ~EQ_UI_JOB_BIT(job);
        state->displayed_valid_mask |= EQ_UI_JOB_BIT(job);
    }
    pending = 0;
    for (band = 0; band < EQ_UI_ANALYZER_COUNT; band++)
    {
        if (state->analyzer_field_mask[band] != 0U)
        {
            pending = 1;
        }
    }
    if (pending == 0)
    {
        state->displayed.analyzer_valid =
            state->requested.analyzer_valid;
        state->displayed.analyzer_warm =
            state->requested.analyzer_warm;
    }
    EQ_UI_RecordService(state, job, process_frame);
}

void EqualizerUiLogic_Cancel(EQ_UI_STATE *state)
{
    int index;

    if (state == 0)
    {
        return;
    }
    state->dirty_mask = 0UL;
    for (index = 0; index < EQ_UI_DYNAMIC_COUNT; index++)
    {
        state->dynamic_field_mask[index] = 0U;
    }
    for (index = 0; index < EQ_UI_ANALYZER_COUNT; index++)
    {
        state->analyzer_field_mask[index] = 0U;
    }
}

int EqualizerUi_DbTenthsToPixel(int tenths_db)
{
    long numerator;
    int span;
    int pixel;

    tenths_db = EQ_UI_ClampInt(tenths_db,
                               EQ_UI_ANALYZER_MIN_TENTHS_DB,
                               EQ_UI_ANALYZER_MAX_TENTHS_DB);
    span = EQ_UI_ANALYZER_BAR_BOTTOM - EQ_UI_ANALYZER_BAR_TOP;
    numerator = (long)(EQ_UI_ANALYZER_MAX_TENTHS_DB - tenths_db) *
                (long)span;
    pixel = EQ_UI_ANALYZER_BAR_TOP +
            (int)((numerator + 200L) / 400L);
    return EQ_UI_ClampInt(pixel, EQ_UI_ANALYZER_BAR_TOP,
                          EQ_UI_ANALYZER_BAR_BOTTOM);
}

int EqualizerUi_RoundTenthsToDb(int tenths_db)
{
    if (tenths_db >= 0)
    {
        return (tenths_db + 5) / 10;
    }
    return -(((-tenths_db) + 5) / 10);
}

int EqualizerUi_RectContains(const EQ_UI_RECT *rect, int x, int y)
{
    if (rect == 0)
    {
        return 0;
    }
    return (x >= rect->x) && (x < (rect->x + rect->w)) &&
           (y >= rect->y) && (y < (rect->y + rect->h));
}

static int EQ_UI_RectValid(const EQ_UI_RECT *rect)
{
    return (rect->x >= 0) && (rect->y >= 0) &&
           (rect->w > 0) && (rect->h > 0) &&
           ((rect->x + rect->w) <= EQ_UI_SCREEN_WIDTH) &&
           ((rect->y + rect->h) <= EQ_UI_SCREEN_HEIGHT);
}

static int EQ_UI_RectsOverlap(const EQ_UI_RECT *a, const EQ_UI_RECT *b)
{
    return (a->x < (b->x + b->w)) &&
           (b->x < (a->x + a->w)) &&
           (a->y < (b->y + b->h)) &&
           (b->y < (a->y + a->h));
}

static int EQ_UI_RectContainsRect(const EQ_UI_RECT *outer,
                                  const EQ_UI_RECT *inner)
{
    return (inner->x >= outer->x) && (inner->y >= outer->y) &&
           ((inner->x + inner->w) <= (outer->x + outer->w)) &&
           ((inner->y + inner->h) <= (outer->y + outer->h));
}

int EqualizerUi_ValidateLayout(void)
{
    int i;
    int j;

    for (i = 0; i < EQ_UI_HITBOX_COUNT; i++)
    {
        if (!EQ_UI_RectValid(&EQ_UI_HITBOXES[i].rect))
        {
            return 0;
        }
        for (j = i + 1; j < EQ_UI_HITBOX_COUNT; j++)
        {
            if (EQ_UI_RectsOverlap(&EQ_UI_HITBOXES[i].rect,
                                   &EQ_UI_HITBOXES[j].rect))
            {
                return 0;
            }
        }
    }
    for (i = 0; i < EQ_UI_ANALYZER_COUNT; i++)
    {
        if (!EQ_UI_RectValid(&EQ_UI_ANALYZER_RECTS[i]))
        {
            return 0;
        }
        for (j = i + 1; j < EQ_UI_ANALYZER_COUNT; j++)
        {
            if (EQ_UI_RectsOverlap(&EQ_UI_ANALYZER_RECTS[i],
                                   &EQ_UI_ANALYZER_RECTS[j]))
            {
                return 0;
            }
        }
    }
    for (i = 0; i < EQ_UI_PRESET_COUNT; i++)
    {
        if (!EQ_UI_RectValid(&EQ_UI_PRESET_RECTS[i]))
        {
            return 0;
        }
    }
    for (i = 0; i < EQ_UI_CHAIN_COUNT; i++)
    {
        if (!EQ_UI_RectValid(&EQ_UI_CHAIN_RECTS[i]))
        {
            return 0;
        }
    }
    for (i = 0; i < EQ_UI_DYNAMIC_COUNT; i++)
    {
        if (!EQ_UI_RectValid(&EQ_UI_DYNAMIC_RECTS[i]) ||
            !EQ_UI_RectValid(&EQ_UI_DYNAMIC_TOGGLE_RECTS[i]) ||
            !EQ_UI_RectValid(&EQ_UI_DYNAMIC_STRENGTH_RECTS[i]) ||
            !EQ_UI_RectValid(&EQ_UI_DYNAMIC_LEVEL_RECTS[i]) ||
            !EQ_UI_RectContainsRect(&EQ_UI_DYNAMIC_RECTS[i],
                                    &EQ_UI_DYNAMIC_TOGGLE_RECTS[i]) ||
            !EQ_UI_RectContainsRect(&EQ_UI_DYNAMIC_RECTS[i],
                                    &EQ_UI_DYNAMIC_STRENGTH_RECTS[i]) ||
            !EQ_UI_RectContainsRect(&EQ_UI_DYNAMIC_RECTS[i],
                                    &EQ_UI_DYNAMIC_LEVEL_RECTS[i]))
        {
            return 0;
        }
    }
    return 1;
}

void EqualizerUi_DefaultTouchTransform(EQ_UI_TOUCH_TRANSFORM *transform)
{
    if (transform == 0)
    {
        return;
    }
    transform->raw_x_min = 0U;
    transform->raw_x_max = 799U;
    transform->raw_y_min = 0U;
    transform->raw_y_max = 479U;
    transform->swap_xy = 0U;
    transform->flip_x = 0U;
    transform->flip_y = 0U;
}

static int EQ_UI_ScaleRaw(unsigned int raw_value,
                          unsigned int raw_min,
                          unsigned int raw_max,
                          int screen_max)
{
    unsigned long delta;
    unsigned long span;
    unsigned long mapped;

    if (raw_max <= raw_min)
    {
        return 0;
    }
    if (raw_value < raw_min)
    {
        raw_value = raw_min;
    }
    if (raw_value > raw_max)
    {
        raw_value = raw_max;
    }
    delta = (unsigned long)(raw_value - raw_min);
    span = (unsigned long)(raw_max - raw_min);
    mapped = (delta * (unsigned long)screen_max + (span / 2UL)) / span;
    return EQ_UI_ClampInt((int)mapped, 0, screen_max);
}

int EqualizerUi_MapTouchRawToScreen(
    const EQ_UI_TOUCH_TRANSFORM *transform,
    unsigned int raw_x, unsigned int raw_y,
    int *screen_x, int *screen_y)
{
    unsigned int source_x;
    unsigned int source_y;
    unsigned int source_x_min;
    unsigned int source_x_max;
    unsigned int source_y_min;
    unsigned int source_y_max;
    int mapped_x;
    int mapped_y;

    if ((transform == 0) || (screen_x == 0) || (screen_y == 0))
    {
        return 0;
    }
    if (transform->swap_xy != 0U)
    {
        source_x = raw_y;
        source_y = raw_x;
        source_x_min = transform->raw_y_min;
        source_x_max = transform->raw_y_max;
        source_y_min = transform->raw_x_min;
        source_y_max = transform->raw_x_max;
    }
    else
    {
        source_x = raw_x;
        source_y = raw_y;
        source_x_min = transform->raw_x_min;
        source_x_max = transform->raw_x_max;
        source_y_min = transform->raw_y_min;
        source_y_max = transform->raw_y_max;
    }
    if ((source_x_max <= source_x_min) ||
        (source_y_max <= source_y_min))
    {
        return 0;
    }
    mapped_x = EQ_UI_ScaleRaw(source_x, source_x_min, source_x_max,
                              EQ_UI_SCREEN_WIDTH - 1);
    mapped_y = EQ_UI_ScaleRaw(source_y, source_y_min, source_y_max,
                              EQ_UI_SCREEN_HEIGHT - 1);
    if (transform->flip_x != 0U)
    {
        mapped_x = EQ_UI_SCREEN_WIDTH - 1 - mapped_x;
    }
    if (transform->flip_y != 0U)
    {
        mapped_y = EQ_UI_SCREEN_HEIGHT - 1 - mapped_y;
    }
    *screen_x = EQ_UI_ClampInt(mapped_x, 0, EQ_UI_SCREEN_WIDTH - 1);
    *screen_y = EQ_UI_ClampInt(mapped_y, 0, EQ_UI_SCREEN_HEIGHT - 1);
    return 1;
}

int EqualizerUi_HitTest(int screen_x, int screen_y)
{
    int index;

    for (index = 0; index < EQ_UI_HITBOX_COUNT; index++)
    {
        if (EqualizerUi_RectContains(&EQ_UI_HITBOXES[index].rect,
                                     screen_x, screen_y))
        {
            return EQ_UI_HITBOXES[index].action;
        }
    }
    return EQ_UI_ACTION_NONE;
}

void EqualizerUiTouch_Init(EQ_UI_TOUCH_STATE *state)
{
    if (state != 0)
    {
        state->press_latched = 0U;
        state->latched_action = EQ_UI_ACTION_NONE;
    }
}

int EqualizerUiTouch_Process(EQ_UI_TOUCH_STATE *state,
                             int pressed, int screen_x, int screen_y,
                             int *rejected)
{
    int action;

    if (rejected != 0)
    {
        *rejected = 0;
    }
    if (state == 0)
    {
        return EQ_UI_ACTION_NONE;
    }
    if (pressed == 0)
    {
        state->press_latched = 0U;
        state->latched_action = EQ_UI_ACTION_NONE;
        return EQ_UI_ACTION_NONE;
    }
    if (state->press_latched != 0U)
    {
        return EQ_UI_ACTION_NONE;
    }
    state->press_latched = 1U;
    action = EqualizerUi_HitTest(screen_x, screen_y);
    state->latched_action = action;
    if ((action == EQ_UI_ACTION_NONE) && (rejected != 0))
    {
        *rejected = 1;
    }
    return action;
}

int EqualizerUi_ActionToPreset(int action)
{
    switch (action)
    {
        case EQ_UI_ACTION_PRESET_FLAT:
            return EQ_PRESET_FLAT;
        case EQ_UI_ACTION_PRESET_BASS:
            return EQ_PRESET_BASS_BOOST;
        case EQ_UI_ACTION_PRESET_VOCAL:
            return EQ_PRESET_VOCAL;
        case EQ_UI_ACTION_PRESET_TREBLE:
            return EQ_PRESET_TREBLE_BOOST;
        case EQ_UI_ACTION_PRESET_V_SHAPE:
            return EQ_PRESET_V_SHAPE;
        default:
            return EQ_PRESET_NONE;
    }
}

#endif
