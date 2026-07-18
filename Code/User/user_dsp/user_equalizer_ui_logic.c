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
#if EQ_ENABLE_TEN_BAND_EDITOR != 0
#define EQ_UI_EDITOR_BAND_MASK 0x01FF8000UL
#define EQ_UI_EDITOR_FIELDS_MASK 0x02000000UL
#define EQ_UI_PAGE_TILE_MASK 0x04000000UL
#define EQ_UI_EDITOR_MASK \
    (EQ_UI_EDITOR_BAND_MASK | EQ_UI_EDITOR_FIELDS_MASK)
#endif

#define EQ_UI_CATEGORY_PRESET   0
#define EQ_UI_CATEGORY_DYNAMIC  1
#define EQ_UI_CATEGORY_CHAIN    2
#define EQ_UI_CATEGORY_ANALYZER 3
#if EQ_ENABLE_TEN_BAND_EDITOR != 0
#define EQ_UI_CATEGORY_EDITOR   4
#define EQ_UI_CATEGORY_PAGE     5
#define EQ_UI_CATEGORY_COUNT    6
#else
#define EQ_UI_CATEGORY_COUNT    4
#endif

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

const EQ_UI_RECT EQ_UI_PAGE_SWITCH_RECT = { 670, 80, 104, 28 };

const EQ_UI_HITBOX
    EQ_UI_DYNAMIC_HITBOXES[EQ_UI_DYNAMIC_HITBOX_COUNT] =
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
    { { 286, 430, 96, 40 }, EQ_UI_ACTION_GUARD_STRENGTH },
    { { 670, 80, 104, 28 }, EQ_UI_ACTION_PAGE_SWITCH }
};

#if EQ_ENABLE_TEN_BAND_EDITOR != 0
const EQ_UI_RECT EQ_UI_EDITOR_BAND_RECTS[EQ_NUM_BANDS] =
{
    { 24, 112, 68, 218 },
    { 100, 112, 68, 218 },
    { 176, 112, 68, 218 },
    { 252, 112, 68, 218 },
    { 328, 112, 68, 218 },
    { 404, 112, 68, 218 },
    { 480, 112, 68, 218 },
    { 556, 112, 68, 218 },
    { 632, 112, 68, 218 },
    { 708, 112, 68, 218 }
};

const EQ_UI_RECT EQ_UI_EDITOR_MINUS_RECT = { 24, 344, 104, 42 };
const EQ_UI_RECT EQ_UI_EDITOR_PLUS_RECT = { 140, 344, 104, 42 };
const EQ_UI_RECT EQ_UI_EDITOR_APPLY_RECT = { 500, 344, 120, 42 };
const EQ_UI_RECT EQ_UI_EDITOR_RESET_RECT = { 632, 344, 144, 42 };
const EQ_UI_RECT EQ_UI_EDITOR_FIELDS_RECT = { 24, 398, 752, 70 };

const EQ_UI_HITBOX
    EQ_UI_EDITOR_HITBOXES[EQ_UI_EDITOR_HITBOX_COUNT] =
{
    { { 26, 34, 140, 40 }, EQ_UI_ACTION_PRESET_FLAT },
    { { 178, 34, 140, 40 }, EQ_UI_ACTION_PRESET_BASS },
    { { 330, 34, 140, 40 }, EQ_UI_ACTION_PRESET_VOCAL },
    { { 482, 34, 140, 40 }, EQ_UI_ACTION_PRESET_TREBLE },
    { { 634, 34, 140, 40 }, EQ_UI_ACTION_PRESET_V_SHAPE },
    { { 24, 112, 68, 218 }, EQ_UI_ACTION_EDITOR_BAND_0 },
    { { 100, 112, 68, 218 }, EQ_UI_ACTION_EDITOR_BAND_1 },
    { { 176, 112, 68, 218 }, EQ_UI_ACTION_EDITOR_BAND_2 },
    { { 252, 112, 68, 218 }, EQ_UI_ACTION_EDITOR_BAND_3 },
    { { 328, 112, 68, 218 }, EQ_UI_ACTION_EDITOR_BAND_4 },
    { { 404, 112, 68, 218 }, EQ_UI_ACTION_EDITOR_BAND_5 },
    { { 480, 112, 68, 218 }, EQ_UI_ACTION_EDITOR_BAND_6 },
    { { 556, 112, 68, 218 }, EQ_UI_ACTION_EDITOR_BAND_7 },
    { { 632, 112, 68, 218 }, EQ_UI_ACTION_EDITOR_BAND_8 },
    { { 708, 112, 68, 218 }, EQ_UI_ACTION_EDITOR_BAND_9 },
    { { 24, 344, 104, 42 }, EQ_UI_ACTION_EDITOR_MINUS },
    { { 140, 344, 104, 42 }, EQ_UI_ACTION_EDITOR_PLUS },
    { { 500, 344, 120, 42 }, EQ_UI_ACTION_EDITOR_APPLY },
    { { 632, 344, 144, 42 }, EQ_UI_ACTION_EDITOR_RESET_FLAT },
    { { 670, 80, 104, 28 }, EQ_UI_ACTION_PAGE_SWITCH }
};
#endif

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
#if EQ_ENABLE_TEN_BAND_EDITOR != 0
    if (job == EQ_UI_JOB_PAGE_TILE)
        return EQ_UI_CATEGORY_PAGE;
    if ((job >= EQ_UI_JOB_EDITOR_BAND_0) &&
        (job <= EQ_UI_JOB_EDITOR_FIELDS))
        return EQ_UI_CATEGORY_EDITOR;
#endif
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
#if EQ_ENABLE_TEN_BAND_EDITOR != 0
    if (category == EQ_UI_CATEGORY_PAGE)
        return EQ_UI_PAGE_MIN_GAP_FRAMES;
    if (category == EQ_UI_CATEGORY_EDITOR)
        return EQ_UI_EDITOR_MIN_GAP_FRAMES;
#endif
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
    global_gap = ((category == EQ_UI_CATEGORY_PRESET)
#if EQ_ENABLE_TEN_BAND_EDITOR != 0
                  || (category == EQ_UI_CATEGORY_PAGE) ||
                  (category == EQ_UI_CATEGORY_EDITOR)
#endif
                 ) ?
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
    if ((preset < EQ_PRESET_FLAT) || (preset > EQ_PRESET_CUSTOM))
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
#if EQ_ENABLE_TEN_BAND_EDITOR != 0
    snapshot->page = (snapshot->page == EQ_UI_PAGE_EQ_EDITOR) ?
        EQ_UI_PAGE_EQ_EDITOR : EQ_UI_PAGE_DYNAMIC_STATUS;
    snapshot->editor_selected_band = EQ_UI_ClampInt(
        snapshot->editor_selected_band, 0, EQ_NUM_BANDS - 1);
    snapshot->editor_apply_status = EQ_UI_ClampInt(
        snapshot->editor_apply_status,
        EQ_UI_APPLY_EDITING, EQ_UI_APPLY_ERROR);
    snapshot->editor_draft_dirty =
        (snapshot->editor_draft_dirty != 0U) ? 1U : 0U;
    snapshot->editor_submitted_valid =
        (snapshot->editor_submitted_valid != 0U) ? 1U : 0U;
    for (band = 0; band < EQ_NUM_BANDS; band++)
    {
        snapshot->applied_gain_half_db[band] =
            (EQ_UI_GAIN_HALF_DB)EQ_UI_ClampInt(
                (int)snapshot->applied_gain_half_db[band],
                EQ_UI_GAIN_HALF_DB_MIN, EQ_UI_GAIN_HALF_DB_MAX);
        snapshot->draft_gain_half_db[band] =
            (EQ_UI_GAIN_HALF_DB)EQ_UI_ClampInt(
                (int)snapshot->draft_gain_half_db[band],
                EQ_UI_GAIN_HALF_DB_MIN, EQ_UI_GAIN_HALF_DB_MAX);
        snapshot->submitted_gain_half_db[band] =
            (EQ_UI_GAIN_HALF_DB)EQ_UI_ClampInt(
                (int)snapshot->submitted_gain_half_db[band],
                EQ_UI_GAIN_HALF_DB_MIN, EQ_UI_GAIN_HALF_DB_MAX);
    }
#endif
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

#if EQ_ENABLE_TEN_BAND_EDITOR != 0
static int EQ_UI_EditorGainToPixel(EQ_UI_GAIN_HALF_DB gain_half_db)
{
    long numerator;
    int span;
    int gain;

    gain = EQ_UI_ClampInt((int)gain_half_db,
                          EQ_UI_GAIN_HALF_DB_MIN,
                          EQ_UI_GAIN_HALF_DB_MAX);
    span = EQ_UI_EDITOR_DRAW_BOTTOM - EQ_UI_EDITOR_DRAW_TOP;
    numerator = (long)(EQ_UI_GAIN_HALF_DB_MAX - gain) * (long)span;
    return EQ_UI_EDITOR_DRAW_TOP +
        (int)((numerator + 30L) / 60L);
}

static void EQ_UI_RecomputeEditor(EQ_UI_STATE *state)
{
    int band;
    int selected;
    int selected_displayed;
    int target_pixel;
    unsigned int fields;

    state->dirty_mask &= ~EQ_UI_EDITOR_MASK;
    state->editor_field_mask = 0U;
    if (((state->runtime_mask & EQ_UI_RUNTIME_EDITOR) == 0U) ||
        (state->requested_valid == 0U) ||
        (state->displayed_page != EQ_UI_PAGE_EQ_EDITOR) ||
        (state->page_building != 0U))
    {
        return;
    }
    for (band = 0; band < EQ_NUM_BANDS; band++)
    {
        selected = (state->requested.editor_selected_band == band) ? 1 : 0;
        selected_displayed =
            (int)state->editor_band_displayed_selected[band];
        target_pixel = EQ_UI_EditorGainToPixel(
            state->requested.applied_gain_half_db[band]);
        if ((state->editor_band_displayed_valid[band] == 0U) ||
            (state->editor_displayed_pixel[band] != target_pixel) ||
            (selected_displayed != selected))
        {
            state->dirty_mask |= EQ_UI_JOB_BIT(
                EQ_UI_JOB_EDITOR_BAND_0 + band);
        }
    }

    fields = 0U;
    if (((state->editor_displayed_field_valid &
          EQ_UI_EDITOR_FIELD_SELECTED_BAND) == 0U) ||
        (state->displayed.editor_selected_band !=
         state->requested.editor_selected_band))
    {
        fields |= EQ_UI_EDITOR_FIELD_SELECTED_BAND;
    }
    if (((state->editor_displayed_field_valid &
          EQ_UI_EDITOR_FIELD_DRAFT_GAIN) == 0U) ||
        (state->displayed.editor_draft_version !=
         state->requested.editor_draft_version))
    {
        fields |= EQ_UI_EDITOR_FIELD_DRAFT_GAIN;
    }
    if (((state->editor_displayed_field_valid &
          EQ_UI_EDITOR_FIELD_APPLIED_GAIN) == 0U) ||
        (state->displayed.editor_selected_band !=
         state->requested.editor_selected_band) ||
        (state->displayed.applied_gain_half_db[
             state->requested.editor_selected_band] !=
         state->requested.applied_gain_half_db[
             state->requested.editor_selected_band]))
    {
        fields |= EQ_UI_EDITOR_FIELD_APPLIED_GAIN;
    }
    if (((state->editor_displayed_field_valid &
          EQ_UI_EDITOR_FIELD_CUSTOM_STATE) == 0U) ||
        (state->displayed.applied_preset !=
         state->requested.applied_preset) ||
        (state->displayed.editor_draft_dirty !=
         state->requested.editor_draft_dirty))
    {
        fields |= EQ_UI_EDITOR_FIELD_CUSTOM_STATE;
    }
    if (((state->editor_displayed_field_valid &
          EQ_UI_EDITOR_FIELD_APPLY_STATE) == 0U) ||
        (state->displayed.editor_apply_status !=
         state->requested.editor_apply_status) ||
        (state->displayed.editor_submitted_sequence !=
         state->requested.editor_submitted_sequence) ||
        (state->displayed.editor_applied_sequence !=
         state->requested.editor_applied_sequence))
    {
        fields |= EQ_UI_EDITOR_FIELD_APPLY_STATE;
    }
    state->editor_field_mask = fields;
    if (fields != 0U)
    {
        state->dirty_mask |= EQ_UI_EDITOR_FIELDS_MASK;
    }
}

static unsigned int EQ_UI_PageTileCount(int page)
{
    return (page == EQ_UI_PAGE_EQ_EDITOR) ?
        EQ_UI_PAGE_TILE_EDITOR_COUNT : EQ_UI_PAGE_TILE_DYNAMIC_COUNT;
}

static void EQ_UI_StartPageBuild(EQ_UI_STATE *state, int page)
{
    int index;

    state->dirty_mask = EQ_UI_PAGE_TILE_MASK;
    state->page_target = page;
    state->page_tile_index = 0U;
    state->page_tile_count = EQ_UI_PageTileCount(page);
    state->page_building = 1U;
    state->editor_field_mask = 0U;
    for (index = 0; index < EQ_UI_DYNAMIC_COUNT; index++)
    {
        state->dynamic_field_mask[index] = 0U;
    }
    for (index = 0; index < EQ_UI_ANALYZER_COUNT; index++)
    {
        state->analyzer_field_mask[index] = 0U;
    }
}
#endif

static void EQ_UI_RecomputeAll(EQ_UI_STATE *state,
                               unsigned long process_frame)
{
#if EQ_ENABLE_TEN_BAND_EDITOR != 0
    if (state->page_building != 0U)
    {
        state->dirty_mask = EQ_UI_PAGE_TILE_MASK;
        return;
    }
    if (state->displayed_page == EQ_UI_PAGE_EQ_EDITOR)
    {
        state->dirty_mask &= ~(EQ_UI_CHAIN_MASK | EQ_UI_ANALYZER_MASK |
                               EQ_UI_DYNAMIC_MASK);
        EQ_UI_RecomputePreset(state);
        EQ_UI_RecomputeEditor(state);
        return;
    }
    state->dirty_mask &= ~EQ_UI_EDITOR_MASK;
#endif
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
#if EQ_ENABLE_TEN_BAND_EDITOR != 0
    state->requested.page = EQ_UI_PAGE_DYNAMIC_STATUS;
    state->displayed.page = EQ_UI_PAGE_DYNAMIC_STATUS;
    state->displayed_page = EQ_UI_PAGE_DYNAMIC_STATUS;
    state->requested_page = EQ_UI_PAGE_DYNAMIC_STATUS;
    state->page_target = EQ_UI_PAGE_DYNAMIC_STATUS;
    state->requested.editor_selected_band = 0;
    state->displayed.editor_selected_band = 0;
    state->requested.editor_apply_status = EQ_UI_APPLY_APPLIED;
    state->displayed.editor_apply_status = EQ_UI_APPLY_APPLIED;
    for (band = 0; band < EQ_NUM_BANDS; band++)
    {
        state->editor_displayed_pixel[band] = EQ_UI_EDITOR_ZERO_PIXEL;
    }
#endif
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
#if EQ_ENABLE_TEN_BAND_EDITOR != 0
    state->requested_page = state->requested.page;
    if (((state->runtime_mask & EQ_UI_RUNTIME_PAGE) == 0U) ||
        ((state->runtime_mask & EQ_UI_RUNTIME_EDITOR) == 0U))
    {
        state->requested_page = EQ_UI_PAGE_DYNAMIC_STATUS;
        state->requested.page = EQ_UI_PAGE_DYNAMIC_STATUS;
    }
    if ((state->page_building == 0U) &&
        (state->requested_page != state->displayed_page))
    {
        EQ_UI_StartPageBuild(state, state->requested_page);
        return;
    }
    if (state->page_building != 0U)
    {
        if (state->requested_page != state->page_target)
        {
            EQ_UI_StartPageBuild(state, state->requested_page);
        }
        else
        {
            state->dirty_mask = EQ_UI_PAGE_TILE_MASK;
        }
        return;
    }
#endif
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
#if EQ_ENABLE_TEN_BAND_EDITOR != 0
    if (((state->dirty_mask & EQ_UI_PAGE_TILE_MASK) != 0UL) &&
        EQ_UI_CategoryEligible(state, EQ_UI_CATEGORY_PAGE,
                               process_frame))
    {
        return 1;
    }
    if (((state->dirty_mask & EQ_UI_EDITOR_MASK) != 0UL) &&
        EQ_UI_CategoryEligible(state, EQ_UI_CATEGORY_EDITOR,
                               process_frame))
    {
        return 1;
    }
#endif
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
#if EQ_ENABLE_TEN_BAND_EDITOR != 0
    if (((state->dirty_mask & EQ_UI_PAGE_TILE_MASK) != 0UL) &&
        (EQ_UI_CategoryEligible(state, EQ_UI_CATEGORY_PAGE,
                                process_frame) != 0))
    {
        return EQ_UI_JOB_PAGE_TILE;
    }
#endif
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
#if EQ_ENABLE_TEN_BAND_EDITOR != 0
    if (EQ_UI_CategoryEligible(state, EQ_UI_CATEGORY_EDITOR,
                               process_frame) != 0)
    {
        job = EQ_UI_SelectFromRange(state->dirty_mask,
                                   EQ_UI_JOB_EDITOR_BAND_0,
                                   EQ_NUM_BANDS,
                                   &state->editor_band_cursor);
        if (job != EQ_UI_JOB_NONE)
        {
            return job;
        }
        if ((state->dirty_mask & EQ_UI_EDITOR_FIELDS_MASK) != 0UL)
        {
            return EQ_UI_JOB_EDITOR_FIELDS;
        }
    }
#endif
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
#if EQ_ENABLE_TEN_BAND_EDITOR != 0
    if (job == EQ_UI_JOB_PAGE_TILE)
    {
        EqualizerUiLogic_CompletePageTile(state, process_frame);
        return;
    }
    if ((job >= EQ_UI_JOB_EDITOR_BAND_0) &&
        (job <= EQ_UI_JOB_EDITOR_BAND_9))
    {
        EqualizerUiLogic_CompleteEditorBand(state, job, process_frame);
        return;
    }
    if (job == EQ_UI_JOB_EDITOR_FIELDS)
    {
        EqualizerUiLogic_CompleteEditorField(
            state, EqualizerUiLogic_EditorNextField(state), process_frame);
        return;
    }
#endif
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

#if EQ_ENABLE_TEN_BAND_EDITOR != 0
unsigned int EqualizerUiLogic_EditorFieldMask(const EQ_UI_STATE *state)
{
    return (state != 0) ? state->editor_field_mask : 0U;
}

unsigned int EqualizerUiLogic_EditorNextField(const EQ_UI_STATE *state)
{
    static const unsigned int order[] =
    {
        EQ_UI_EDITOR_FIELD_SELECTED_BAND,
        EQ_UI_EDITOR_FIELD_DRAFT_GAIN,
        EQ_UI_EDITOR_FIELD_APPLIED_GAIN,
        EQ_UI_EDITOR_FIELD_CUSTOM_STATE,
        EQ_UI_EDITOR_FIELD_APPLY_STATE
    };
    unsigned int index;

    if (state == 0)
    {
        return 0U;
    }
    for (index = 0U; index <
         (unsigned int)(sizeof(order) / sizeof(order[0])); index++)
    {
        if ((state->editor_field_mask & order[index]) != 0U)
        {
            return order[index];
        }
    }
    return 0U;
}

int EqualizerUiLogic_EditorNextPixel(const EQ_UI_STATE *state,
                                     int band_index)
{
    int current;
    int target;
    int phase_target;
    int delta;

    if ((state == 0) || (band_index < 0) ||
        (band_index >= EQ_NUM_BANDS))
    {
        return EQ_UI_EDITOR_ZERO_PIXEL;
    }
    current = EQ_UI_ClampInt(state->editor_displayed_pixel[band_index],
                             EQ_UI_EDITOR_DRAW_TOP,
                             EQ_UI_EDITOR_DRAW_BOTTOM);
    target = EQ_UI_EditorGainToPixel(
        state->requested.applied_gain_half_db[band_index]);
    phase_target = target;
    if (((current < EQ_UI_EDITOR_ZERO_PIXEL) &&
         (target > EQ_UI_EDITOR_ZERO_PIXEL)) ||
        ((current > EQ_UI_EDITOR_ZERO_PIXEL) &&
         (target < EQ_UI_EDITOR_ZERO_PIXEL)))
    {
        phase_target = EQ_UI_EDITOR_ZERO_PIXEL;
    }
    delta = EQ_UI_ClampInt(
        phase_target - current,
        -EQ_UI_EDITOR_MAX_STRIP_HEIGHT,
        EQ_UI_EDITOR_MAX_STRIP_HEIGHT);
    return current + delta;
}

void EqualizerUiLogic_CompleteEditorBand(
    EQ_UI_STATE *state, int job, unsigned long process_frame)
{
    int band;
    int next_pixel;
    int target_pixel;
    int selected;

    if ((state == 0) || (job < EQ_UI_JOB_EDITOR_BAND_0) ||
        (job > EQ_UI_JOB_EDITOR_BAND_9))
    {
        return;
    }
    band = job - EQ_UI_JOB_EDITOR_BAND_0;
    next_pixel = EqualizerUiLogic_EditorNextPixel(state, band);
    target_pixel = EQ_UI_EditorGainToPixel(
        state->requested.applied_gain_half_db[band]);
    selected = (state->requested.editor_selected_band == band) ? 1 : 0;
    state->editor_displayed_pixel[band] = next_pixel;
    state->editor_band_displayed_selected[band] =
        (unsigned char)selected;
    state->editor_band_displayed_valid[band] = 1U;
    if (next_pixel == target_pixel)
    {
        state->editor_displayed_applied_gain[band] =
            state->requested.applied_gain_half_db[band];
        state->dirty_mask &= ~EQ_UI_JOB_BIT(job);
        state->displayed_valid_mask |= EQ_UI_JOB_BIT(job);
    }
    EQ_UI_RecordService(state, job, process_frame);
}

static void EQ_UI_CopyEditorField(EQ_UI_STATE *state,
                                  unsigned int field)
{
    int band;

    if (field == EQ_UI_EDITOR_FIELD_SELECTED_BAND)
    {
        state->displayed.editor_selected_band =
            state->requested.editor_selected_band;
    }
    else if (field == EQ_UI_EDITOR_FIELD_DRAFT_GAIN)
    {
        for (band = 0; band < EQ_NUM_BANDS; band++)
        {
            state->displayed.draft_gain_half_db[band] =
                state->requested.draft_gain_half_db[band];
        }
        state->displayed.editor_draft_version =
            state->requested.editor_draft_version;
    }
    else if (field == EQ_UI_EDITOR_FIELD_APPLIED_GAIN)
    {
        for (band = 0; band < EQ_NUM_BANDS; band++)
        {
            state->displayed.applied_gain_half_db[band] =
                state->requested.applied_gain_half_db[band];
        }
    }
    else if (field == EQ_UI_EDITOR_FIELD_CUSTOM_STATE)
    {
        state->displayed.applied_preset =
            state->requested.applied_preset;
        state->displayed.editor_draft_dirty =
            state->requested.editor_draft_dirty;
    }
    else if (field == EQ_UI_EDITOR_FIELD_APPLY_STATE)
    {
        for (band = 0; band < EQ_NUM_BANDS; band++)
        {
            state->displayed.submitted_gain_half_db[band] =
                state->requested.submitted_gain_half_db[band];
        }
        state->displayed.editor_apply_status =
            state->requested.editor_apply_status;
        state->displayed.editor_applied_sequence =
            state->requested.editor_applied_sequence;
        state->displayed.editor_submitted_sequence =
            state->requested.editor_submitted_sequence;
        state->displayed.editor_submitted_valid =
            state->requested.editor_submitted_valid;
    }
}

void EqualizerUiLogic_CompleteEditorField(
    EQ_UI_STATE *state, unsigned int completed_field,
    unsigned long process_frame)
{
    unsigned int field;

    if (state == 0)
    {
        return;
    }
    field = completed_field & state->editor_field_mask &
            EQ_UI_EDITOR_FIELD_ALL;
    if (field == 0U)
    {
        return;
    }
    if ((field & (field - 1U)) != 0U)
    {
        field = EqualizerUiLogic_EditorNextField(state);
    }
    EQ_UI_CopyEditorField(state, field);
    state->editor_displayed_field_valid |= field;
    state->editor_field_mask &= ~field;
    if (state->editor_field_mask == 0U)
    {
        state->dirty_mask &= ~EQ_UI_EDITOR_FIELDS_MASK;
        state->displayed_valid_mask |= EQ_UI_EDITOR_FIELDS_MASK;
    }
    EQ_UI_RecordService(state, EQ_UI_JOB_EDITOR_FIELDS, process_frame);
}

static void EQ_UI_CompleteDynamicPageTile(EQ_UI_STATE *state,
                                          unsigned int tile)
{
    int index;

    if ((tile >= 1U) && (tile <= 5U))
    {
        index = (int)tile - 1;
        state->preset_displayed_selected[index] =
            (unsigned char)((state->requested.applied_preset == index) ?
                            1U : 0U);
    }
    else if (tile == 6U)
    {
        state->chain_displayed_enabled[0] =
            (unsigned char)state->requested.smart_enabled;
        state->chain_displayed_enabled[1] =
            (unsigned char)state->requested.clarity_enabled;
        state->chain_displayed_enabled[2] =
            (unsigned char)state->requested.guard_enabled;
    }
    else if ((tile >= 7U) && (tile <= 10U))
    {
        index = (int)tile - 7;
        state->displayed.band_value_db[index] =
            state->requested.band_value_db[index];
        state->displayed.band_pixel[index] =
            EQ_UI_AnalyzerTargetPixel(state, index);
        state->analyzer_displayed_valid[index] =
            (unsigned char)state->requested.analyzer_valid;
        state->analyzer_displayed_warm[index] =
            (unsigned char)state->requested.analyzer_warm;
        state->analyzer_displayed_field_valid[index] =
            EQ_UI_ANALYZER_FIELD_ALL;
    }
    else if ((tile >= 11U) && (tile <= 13U))
    {
        index = (int)tile - 11;
        if (index == 0)
        {
            state->displayed.smart_enabled = state->requested.smart_enabled;
            state->displayed.smart_strength = state->requested.smart_strength;
            state->displayed.smart_level = state->requested.smart_level;
        }
        else if (index == 1)
        {
            state->displayed.clarity_enabled =
                state->requested.clarity_enabled;
            state->displayed.clarity_strength =
                state->requested.clarity_strength;
            state->displayed.clarity_level =
                state->requested.clarity_level;
        }
        else
        {
            state->displayed.guard_enabled = state->requested.guard_enabled;
            state->displayed.guard_strength = state->requested.guard_strength;
            state->displayed.guard_level = state->requested.guard_level;
        }
        state->dynamic_displayed_field_valid[index] =
            EQ_UI_DYNAMIC_FIELD_ALL;
    }
}

static void EQ_UI_CompleteEditorPageTile(EQ_UI_STATE *state,
                                         unsigned int tile)
{
    int index;
    unsigned int field;

    if ((tile >= 1U) && (tile <= 5U))
    {
        index = (int)tile - 1;
        state->preset_displayed_selected[index] =
            (unsigned char)((state->requested.applied_preset == index) ?
                            1U : 0U);
    }
    else if ((tile >= 6U) && (tile <= 15U))
    {
        index = (int)tile - 6;
        state->editor_displayed_applied_gain[index] =
            state->requested.applied_gain_half_db[index];
        state->editor_displayed_pixel[index] = EQ_UI_EditorGainToPixel(
            state->requested.applied_gain_half_db[index]);
        state->editor_band_displayed_selected[index] =
            (unsigned char)((state->requested.editor_selected_band ==
                             index) ? 1U : 0U);
        state->editor_band_displayed_valid[index] = 1U;
    }
    else if (tile == 16U)
    {
        for (field = EQ_UI_EDITOR_FIELD_SELECTED_BAND;
             field <= EQ_UI_EDITOR_FIELD_APPLY_STATE; field <<= 1)
        {
            EQ_UI_CopyEditorField(state, field);
        }
        state->editor_displayed_field_valid = EQ_UI_EDITOR_FIELD_ALL;
    }
}

void EqualizerUiLogic_CompletePageTile(
    EQ_UI_STATE *state, unsigned long process_frame)
{
    unsigned int tile;

    if ((state == 0) || (state->page_building == 0U) ||
        ((state->dirty_mask & EQ_UI_PAGE_TILE_MASK) == 0UL))
    {
        return;
    }
    tile = state->page_tile_index;
    if (state->page_target == EQ_UI_PAGE_EQ_EDITOR)
    {
        EQ_UI_CompleteEditorPageTile(state, tile);
    }
    else
    {
        EQ_UI_CompleteDynamicPageTile(state, tile);
    }
    state->page_tile_index++;
    EQ_UI_RecordService(state, EQ_UI_JOB_PAGE_TILE, process_frame);
    if (state->page_tile_index < state->page_tile_count)
    {
        return;
    }
    state->displayed_page = state->page_target;
    state->displayed.page = state->displayed_page;
    state->displayed.applied_preset = state->requested.applied_preset;
    state->page_building = 0U;
    state->dirty_mask &= ~EQ_UI_PAGE_TILE_MASK;
    state->displayed_valid_mask |= EQ_UI_PAGE_TILE_MASK;
    EQ_UI_RecomputeAll(state, process_frame);
}

int EqualizerUiLogic_GetDisplayedPage(const EQ_UI_STATE *state)
{
    return (state != 0) ? state->displayed_page :
        EQ_UI_PAGE_DYNAMIC_STATUS;
}

int EqualizerUiLogic_IsPageBuilding(const EQ_UI_STATE *state)
{
    return ((state != 0) && (state->page_building != 0U)) ? 1 : 0;
}

unsigned int EqualizerUiLogic_GetPageTileIndex(const EQ_UI_STATE *state)
{
    return (state != 0) ? state->page_tile_index : 0U;
}

unsigned int EqualizerUiLogic_GetPageTileCount(const EQ_UI_STATE *state)
{
    return (state != 0) ? state->page_tile_count : 0U;
}
#endif

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
#if EQ_ENABLE_TEN_BAND_EDITOR != 0
    state->editor_field_mask = 0U;
    state->page_building = 0U;
    state->page_tile_index = 0U;
    state->page_tile_count = 0U;
#endif
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

static int EQ_UI_ValidateHitboxes(const EQ_UI_HITBOX *hitboxes, int count)
{
    int i;
    int j;

    for (i = 0; i < count; i++)
    {
        if (!EQ_UI_RectValid(&hitboxes[i].rect))
        {
            return 0;
        }
        for (j = i + 1; j < count; j++)
        {
            if (EQ_UI_RectsOverlap(&hitboxes[i].rect,
                                   &hitboxes[j].rect))
            {
                return 0;
            }
        }
    }
    return 1;
}

int EqualizerUi_ValidateLayout(void)
{
    int i;
    int j;

    if (!EQ_UI_ValidateHitboxes(EQ_UI_DYNAMIC_HITBOXES,
                                EQ_UI_DYNAMIC_HITBOX_COUNT))
    {
        return 0;
    }
#if EQ_ENABLE_TEN_BAND_EDITOR != 0
    if (!EQ_UI_ValidateHitboxes(EQ_UI_EDITOR_HITBOXES,
                                EQ_UI_EDITOR_HITBOX_COUNT) ||
        !EQ_UI_RectValid(&EQ_UI_EDITOR_FIELDS_RECT))
    {
        return 0;
    }
    for (i = 0; i < EQ_NUM_BANDS; i++)
    {
        if (!EQ_UI_RectValid(&EQ_UI_EDITOR_BAND_RECTS[i]))
        {
            return 0;
        }
        for (j = i + 1; j < EQ_NUM_BANDS; j++)
        {
            if (EQ_UI_RectsOverlap(&EQ_UI_EDITOR_BAND_RECTS[i],
                                   &EQ_UI_EDITOR_BAND_RECTS[j]))
            {
                return 0;
            }
        }
    }
#endif
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

int EqualizerUi_HitTest(int page, int screen_x, int screen_y)
{
    const EQ_UI_HITBOX *hitboxes;
    int count;
    int index;

#if EQ_ENABLE_TEN_BAND_EDITOR == 0
    (void)page;
#endif
#if EQ_ENABLE_TEN_BAND_EDITOR != 0
    if (page == EQ_UI_PAGE_EQ_EDITOR)
    {
        hitboxes = EQ_UI_EDITOR_HITBOXES;
        count = EQ_UI_EDITOR_HITBOX_COUNT;
    }
    else
#endif
    {
        hitboxes = EQ_UI_DYNAMIC_HITBOXES;
        count = EQ_UI_DYNAMIC_HITBOX_COUNT;
    }
    for (index = 0; index < count; index++)
    {
        if (EqualizerUi_RectContains(&hitboxes[index].rect,
                                     screen_x, screen_y))
        {
            return hitboxes[index].action;
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
                             int page, int page_building,
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
    action = EqualizerUi_HitTest(page, screen_x, screen_y);
    if ((page_building != 0) &&
        (action != EQ_UI_ACTION_PAGE_SWITCH))
    {
        action = EQ_UI_ACTION_NONE;
        if (rejected != 0)
        {
            *rejected = 1;
        }
    }
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

#if EQ_ENABLE_TEN_BAND_EDITOR != 0
static int EQ_UI_GainArraysEqual(
    const EQ_UI_GAIN_HALF_DB a[EQ_NUM_BANDS],
    const EQ_UI_GAIN_HALF_DB b[EQ_NUM_BANDS])
{
    int band;

    for (band = 0; band < EQ_NUM_BANDS; band++)
    {
        if (a[band] != b[band])
        {
            return 0;
        }
    }
    return 1;
}

static void EQ_UI_CopyGainArray(
    EQ_UI_GAIN_HALF_DB destination[EQ_NUM_BANDS],
    const EQ_UI_GAIN_HALF_DB source[EQ_NUM_BANDS])
{
    int band;

    for (band = 0; band < EQ_NUM_BANDS; band++)
    {
        destination[band] = source[band];
    }
}

void EqualizerUiEditor_Init(EQ_UI_EDITOR_STATE *state)
{
    if (state == 0)
    {
        return;
    }
    memset(state, 0, sizeof(*state));
    state->applied_preset = EQ_PRESET_FLAT;
    state->selected_band = 0;
    state->apply_status = EQ_UI_APPLY_APPLIED;
    state->submitted_valid = 1U;
}

int EqualizerUiEditor_SelectBand(EQ_UI_EDITOR_STATE *state, int band)
{
    if ((state == 0) || (band < 0) || (band >= EQ_NUM_BANDS) ||
        (state->selected_band == band))
    {
        return 0;
    }
    state->selected_band = band;
    return 1;
}

int EqualizerUiEditor_StepSelected(EQ_UI_EDITOR_STATE *state,
                                   int delta_half_db)
{
    int band;
    int value;

    if ((state == 0) || (delta_half_db == 0))
    {
        return 0;
    }
    band = state->selected_band;
    if ((band < 0) || (band >= EQ_NUM_BANDS))
    {
        return 0;
    }
    value = EQ_UI_ClampInt(
        (int)state->draft_gain_half_db[band] + delta_half_db,
        EQ_UI_GAIN_HALF_DB_MIN, EQ_UI_GAIN_HALF_DB_MAX);
    if (value == (int)state->draft_gain_half_db[band])
    {
        return 0;
    }
    state->draft_gain_half_db[band] = (EQ_UI_GAIN_HALF_DB)value;
    state->draft_version++;
    state->draft_dirty = (unsigned int)!EQ_UI_GainArraysEqual(
        state->draft_gain_half_db, state->applied_gain_half_db);
    state->apply_status = EQ_UI_APPLY_EDITING;
    return 1;
}

int EqualizerUiEditor_HasSubmittableDraft(
    const EQ_UI_EDITOR_STATE *state)
{
    if ((state == 0) || (state->draft_dirty == 0U))
    {
        return 0;
    }
    if ((state->submitted_valid != 0U) &&
        EQ_UI_GainArraysEqual(state->draft_gain_half_db,
                              state->submitted_gain_half_db) &&
        (state->submitted_sequence != state->applied_sequence))
    {
        return 0;
    }
    return 1;
}

void EqualizerUiEditor_CopyDraftDb(
    const EQ_UI_EDITOR_STATE *state, float gains_db[EQ_NUM_BANDS])
{
    int band;

    if ((state == 0) || (gains_db == 0))
    {
        return;
    }
    for (band = 0; band < EQ_NUM_BANDS; band++)
    {
        gains_db[band] =
            (float)state->draft_gain_half_db[band] * 0.5f;
    }
}

void EqualizerUiEditor_MarkSubmitted(
    EQ_UI_EDITOR_STATE *state, EQ_CONTROL_SEQUENCE sequence)
{
    if ((state == 0) || (sequence == 0U))
    {
        return;
    }
    EQ_UI_CopyGainArray(state->submitted_gain_half_db,
                        state->draft_gain_half_db);
    state->submitted_sequence = sequence;
    state->submitted_valid = 1U;
    state->apply_status = EQ_UI_APPLY_QUEUED;
}

void EqualizerUiEditor_ObserveApplied(
    EQ_UI_EDITOR_STATE *state,
    const EQ_UI_GAIN_HALF_DB gains[EQ_NUM_BANDS],
    int preset, EQ_CONTROL_SEQUENCE applied_sequence)
{
    int synchronize_draft;
    int draft_changed;

    if ((state == 0) || (gains == 0))
    {
        return;
    }
    synchronize_draft = (preset != EQ_PRESET_CUSTOM) ||
        (state->submitted_valid == 0U) ||
        (applied_sequence == state->submitted_sequence);
    draft_changed = !EQ_UI_GainArraysEqual(
        state->draft_gain_half_db, gains);
    EQ_UI_CopyGainArray(state->applied_gain_half_db, gains);
    state->applied_preset = EQ_UI_NormalizePreset(preset);
    state->applied_sequence = applied_sequence;
    if (synchronize_draft != 0)
    {
        EQ_UI_CopyGainArray(state->draft_gain_half_db, gains);
        EQ_UI_CopyGainArray(state->submitted_gain_half_db, gains);
        state->submitted_sequence = applied_sequence;
        state->submitted_valid = 1U;
        if (draft_changed != 0)
        {
            state->draft_version++;
        }
    }
    state->draft_dirty = (unsigned int)!EQ_UI_GainArraysEqual(
        state->draft_gain_half_db, state->applied_gain_half_db);
    if ((state->submitted_valid != 0U) &&
        (state->applied_sequence == state->submitted_sequence))
    {
        state->apply_status = EQ_UI_APPLY_APPLIED;
    }
}

void EqualizerUiEditor_SetApplyStatus(EQ_UI_EDITOR_STATE *state,
                                      int status)
{
    if (state == 0)
    {
        return;
    }
    state->apply_status = EQ_UI_ClampInt(
        status, EQ_UI_APPLY_EDITING, EQ_UI_APPLY_ERROR);
}

void EqualizerUiEditor_CopyToSnapshot(
    const EQ_UI_EDITOR_STATE *state, EQ_UI_SNAPSHOT *snapshot)
{
    int band;

    if ((state == 0) || (snapshot == 0))
    {
        return;
    }
    for (band = 0; band < EQ_NUM_BANDS; band++)
    {
        snapshot->applied_gain_half_db[band] =
            state->applied_gain_half_db[band];
        snapshot->draft_gain_half_db[band] =
            state->draft_gain_half_db[band];
        snapshot->submitted_gain_half_db[band] =
            state->submitted_gain_half_db[band];
    }
    snapshot->editor_applied_sequence = state->applied_sequence;
    snapshot->editor_submitted_sequence = state->submitted_sequence;
    snapshot->editor_draft_version = state->draft_version;
    snapshot->editor_selected_band = state->selected_band;
    snapshot->editor_apply_status = state->apply_status;
    snapshot->editor_draft_dirty = state->draft_dirty;
    snapshot->editor_submitted_valid = state->submitted_valid;
}

EQ_UI_GAIN_HALF_DB EqualizerUi_GainDbToHalf(float gain_db)
{
    float scaled;
    int value;

    scaled = gain_db * 2.0f;
    value = (scaled >= 0.0f) ?
        (int)(scaled + 0.5f) : (int)(scaled - 0.5f);
    value = EQ_UI_ClampInt(value,
                           EQ_UI_GAIN_HALF_DB_MIN,
                           EQ_UI_GAIN_HALF_DB_MAX);
    return (EQ_UI_GAIN_HALF_DB)value;
}
#endif

#endif
