#include "user_equalizer.h"
#include "user_equalizer_control.h"
#include "user_equalizer_display.h"
#include "user_equalizer_flow.h"
#include "user_equalizer_ui_logic.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define STRESS_SEED 0x33E71801U
#define STRESS_ACTIONS 12000UL
#define STRESS_AUDIO_SAMPLES 64

#define TEST_JOB_BIT(job_) (1UL << ((job_) - 1))
#define TEST_PRESET_MASK 0x001FUL
#define TEST_CHAIN_MASK 0x00E0UL
#define TEST_ANALYZER_MASK 0x0F00UL
#define TEST_DYNAMIC_MASK 0x7000UL
#define TEST_EDITOR_MASK 0x03FF8000UL
#define TEST_PAGE_MASK 0x04000000UL
#define TEST_VALID_JOB_MASK ((1UL << EQ_UI_JOB_COUNT) - 1UL)

typedef enum
{
    STRESS_SELECT_BAND = 0,
    STRESS_PLUS,
    STRESS_MINUS,
    STRESS_APPLY,
    STRESS_RESET,
    STRESS_PRESET,
    STRESS_PAGE_SWITCH,
    STRESS_DYNAMIC_TOGGLE,
    STRESS_STRENGTH,
    STRESS_BUILDER_SERVICE,
    STRESS_FRAME_BOUNDARY,
    STRESS_ANALYZER_PUBLICATION,
    STRESS_LCD_SERVICE,
    STRESS_ACTION_KIND_COUNT
} STRESS_ACTION_KIND;

typedef struct
{
    EQ_STATE equalizer;
    EQ_CONTROL_STATE control;
    EQ_CONTROL_MAILBOX mailbox;
    EQ_UI_EDITOR_STATE editor;
    EQ_UI_SNAPSHOT snapshot;
    EQ_UI_TOUCH_STATE touch;
    EQ_BACKGROUND_SERVICE_STATE background;
    EQ_LCD_SERVICE_POLICY lcd_policy;
    unsigned int random_state;
    unsigned long frame;
    unsigned long failures;
    unsigned long nonfinite_count;
    unsigned long stale_count;
    unsigned long stale_install_count;
    unsigned long applied_count;
    unsigned long builder_service_count;
    unsigned long analyzer_publication_count;
    unsigned long lcd_service_count;
    unsigned long action_count[STRESS_ACTION_KIND_COUNT];
    unsigned long accepted_count[STRESS_ACTION_KIND_COUNT];
    unsigned long cancel_baseline;
    EQ_CONTROL_SEQUENCE last_applied_sequence;
    int requested_page;
    int dynamic_enabled[EQ_UI_DYNAMIC_COUNT];
    int dynamic_strength[EQ_UI_DYNAMIC_COUNT];
    int analyzer_db[EQ_UI_ANALYZER_COUNT];
    unsigned int analyzer_pending;
    unsigned int analyzer_valid;
    unsigned int analyzer_warm;
    unsigned int wrap_verified;
} STRESS_CONTEXT;

static void stress_fail(STRESS_CONTEXT *context, int line,
                        const char *condition)
{
    context->failures++;
    if (context->failures <= 20UL)
    {
        printf("FAIL line=%d condition=%s frame=%lu\n",
               line, condition, context->frame);
    }
}

#define CHECK(context_, condition_) do { \
    if (!(condition_)) { \
        stress_fail((context_), __LINE__, #condition_); \
    } \
} while (0)

static unsigned int stress_random(STRESS_CONTEXT *context)
{
    context->random_state = context->random_state * 1664525U + 1013904223U;
    return context->random_state;
}

static int sequence_forward_or_equal(EQ_CONTROL_SEQUENCE previous,
                                     EQ_CONTROL_SEQUENCE current)
{
    EQ_CONTROL_SEQUENCE delta;

    if ((previous == 0U) || (current == previous))
    {
        return 1;
    }
    delta = current - previous;
    return ((delta != 0U) && ((delta & 1U) == 0U) &&
            (delta < 0x80000000U)) ? 1 : 0;
}

static int float_is_finite(float value)
{
    return isfinite((double)value) ? 1 : 0;
}

static int bank_is_finite(const EQ_FILTER_BANK *bank)
{
    int section;

    if ((bank == 0) || !float_is_finite(bank->predicted_peak_db) ||
        !float_is_finite(bank->preamp_db) ||
        !float_is_finite(bank->preamp_gain))
    {
        return 0;
    }
    for (section = 0; section < EQ_NUM_BANDS; section++)
    {
        const EQ_BIQUAD *biquad;

        biquad = &bank->section[section];
        if (!float_is_finite(biquad->b0) ||
            !float_is_finite(biquad->b1) ||
            !float_is_finite(biquad->b2) ||
            !float_is_finite(biquad->a1) ||
            !float_is_finite(biquad->a2) ||
            !float_is_finite(bank->s1[section]) ||
            !float_is_finite(bank->s2[section]))
        {
            return 0;
        }
    }
    return 1;
}

static void fill_control_request(EQ_CONTROL_REQUEST *request, int command)
{
    memset(request, 0, sizeof(*request));
    request->command = command;
    request->preset = EQ_PRESET_NONE;
}

static EQ_CONTROL_SEQUENCE submit_preset(STRESS_CONTEXT *context, int preset)
{
    EQ_CONTROL_REQUEST request;

    fill_control_request(&request, EQ_CONTROL_APPLY_PRESET);
    request.preset = preset;
    return EqualizerControl_SubmitRequest(&context->mailbox, &request);
}

static EQ_CONTROL_SEQUENCE submit_editor(STRESS_CONTEXT *context)
{
    EQ_CONTROL_REQUEST request;
    EQ_CONTROL_SEQUENCE token;

    if (!EqualizerUiEditor_HasSubmittableDraft(&context->editor))
    {
        return 0U;
    }
    fill_control_request(&request, EQ_CONTROL_SET_ALL);
    request.preset = EQ_PRESET_CUSTOM;
    EqualizerUiEditor_CopyDraftDb(&context->editor,
                                  request.shadow_gain_db);
    token = EqualizerControl_SubmitRequest(&context->mailbox, &request);
    EqualizerUiEditor_MarkSubmitted(&context->editor, token);
    return token;
}

static EQ_CONTROL_SEQUENCE submit_reset(STRESS_CONTEXT *context)
{
    EQ_CONTROL_REQUEST request;
    EQ_CONTROL_SEQUENCE token;

    (void)EqualizerUiEditor_SetDraftFlat(&context->editor);
    fill_control_request(&request, EQ_CONTROL_RESET_FLAT);
    request.preset = EQ_PRESET_FLAT;
    token = EqualizerControl_SubmitRequest(&context->mailbox, &request);
    EqualizerUiEditor_MarkSubmitted(&context->editor, token);
    return token;
}

static void observe_applied_sequence(STRESS_CONTEXT *context)
{
    EQ_CONTROL_SEQUENCE current;
    EQ_UI_GAIN_HALF_DB gains[EQ_NUM_BANDS];
    int band;

    current = context->control.applied_sequence;
    if (current == context->last_applied_sequence)
    {
        return;
    }
    CHECK(context, sequence_forward_or_equal(
        context->last_applied_sequence, current));
    if ((context->last_applied_sequence == 0xFFFFFFFEU) &&
        (current == 2U))
    {
        context->wrap_verified = 1U;
    }
    context->last_applied_sequence = current;
    context->applied_count++;
    for (band = 0; band < EQ_NUM_BANDS; band++)
    {
        gains[band] = EqualizerUi_GainDbToHalf(
            context->equalizer.active_gain_db[band]);
    }
    EqualizerUiEditor_ObserveApplied(
        &context->editor, gains,
        Equalizer_GetAppliedPreset(&context->equalizer), current);
}

static void update_apply_status(STRESS_CONTEXT *context)
{
    int status;

    if ((context->editor.submitted_valid == 0U) ||
        (context->editor.submitted_sequence ==
         context->control.applied_sequence))
    {
        status = EQ_UI_APPLY_APPLIED;
    }
    else if ((context->control.installed_sequence ==
              context->editor.submitted_sequence) &&
             Equalizer_HasPendingTransition(&context->equalizer))
    {
        status = EQ_UI_APPLY_TRANSITION;
    }
    else if ((context->control.ready_candidate.valid != 0) &&
             (context->control.ready_candidate.request_sequence ==
              context->editor.submitted_sequence))
    {
        status = EQ_UI_APPLY_READY;
    }
    else if ((context->control.builder.request_sequence ==
              context->editor.submitted_sequence) &&
             ((context->control.builder.state ==
               EQ_BUILDER_DESIGN_SECTION) ||
              (context->control.builder.state ==
               EQ_BUILDER_SCAN_HEADROOM) ||
              (context->control.builder.state == EQ_BUILDER_FINALIZE)))
    {
        status = EQ_UI_APPLY_BUILDING;
    }
    else
    {
        status = EQ_UI_APPLY_QUEUED;
    }
    EqualizerUiEditor_SetApplyStatus(&context->editor, status);
}

static void fill_snapshot(STRESS_CONTEXT *context)
{
    int band;

    memset(&context->snapshot, 0, sizeof(context->snapshot));
    context->snapshot.applied_preset =
        Equalizer_GetAppliedPreset(&context->equalizer);
    context->snapshot.smart_enabled = context->dynamic_enabled[0];
    context->snapshot.clarity_enabled = context->dynamic_enabled[1];
    context->snapshot.guard_enabled = context->dynamic_enabled[2];
    context->snapshot.smart_strength = context->dynamic_strength[0];
    context->snapshot.clarity_strength = context->dynamic_strength[1];
    context->snapshot.guard_strength = context->dynamic_strength[2];
    context->snapshot.smart_level = context->dynamic_strength[0];
    context->snapshot.clarity_level = context->dynamic_strength[1];
    context->snapshot.guard_level = context->dynamic_strength[2];
    context->snapshot.analyzer_valid = (int)context->analyzer_valid;
    context->snapshot.analyzer_warm = (int)context->analyzer_warm;
    for (band = 0; band < EQ_UI_ANALYZER_COUNT; band++)
    {
        context->snapshot.band_value_db[band] = context->analyzer_db[band];
        context->snapshot.band_pixel[band] = EqualizerUi_DbTenthsToPixel(
            context->analyzer_db[band] * 10);
    }
    context->snapshot.page = context->requested_page;
    EqualizerUiEditor_CopyToSnapshot(&context->editor,
                                     &context->snapshot);
}

static void check_display_pending(STRESS_CONTEXT *context)
{
    unsigned long mask;
    int displayed_page;

    mask = EQ_DebugLcdPendingMask;
    displayed_page = EqualizerDisplay_GetDisplayedPage();
    CHECK(context, (mask & ~TEST_VALID_JOB_MASK) == 0UL);
    CHECK(context, EQ_UI_JOB_COUNT <= 32);
    CHECK(context, (displayed_page == EQ_UI_PAGE_DYNAMIC_STATUS) ||
                   (displayed_page == EQ_UI_PAGE_EQ_EDITOR));
    if (EqualizerDisplay_IsPageBuilding())
    {
        CHECK(context, (mask & ~TEST_PAGE_MASK) == 0UL);
    }
    else if (displayed_page == EQ_UI_PAGE_EQ_EDITOR)
    {
        CHECK(context, (mask & (TEST_CHAIN_MASK | TEST_ANALYZER_MASK |
                                TEST_DYNAMIC_MASK)) == 0UL);
    }
    else
    {
        CHECK(context, (mask & TEST_EDITOR_MASK) == 0UL);
    }
}

static void request_display_snapshot(STRESS_CONTEXT *context)
{
    fill_snapshot(context);
    EqualizerDisplay_RequestSnapshot(&context->snapshot, context->frame);
    check_display_pending(context);
}

static int touch_rect(STRESS_CONTEXT *context, const EQ_UI_RECT *rect,
                      int expected_action)
{
    int action;
    int rejected;
    int x;
    int y;

    x = rect->x + rect->w / 2;
    y = rect->y + rect->h / 2;
    action = EqualizerUiTouch_Process(
        &context->touch, EqualizerDisplay_GetDisplayedPage(),
        EqualizerDisplay_IsPageBuilding(), 1, x, y, &rejected);
    CHECK(context, action == expected_action);
    CHECK(context, rejected == 0);
    (void)EqualizerUiTouch_Process(
        &context->touch, EqualizerDisplay_GetDisplayedPage(),
        EqualizerDisplay_IsPageBuilding(), 0, x, y, &rejected);
    return action;
}

static void copy_active_gains(const STRESS_CONTEXT *context,
                              float gains[EQ_NUM_BANDS])
{
    int band;

    for (band = 0; band < EQ_NUM_BANDS; band++)
    {
        gains[band] = context->equalizer.active_gain_db[band];
    }
}

static void check_active_gains_unchanged(STRESS_CONTEXT *context,
                                         const float before[EQ_NUM_BANDS])
{
    int band;

    for (band = 0; band < EQ_NUM_BANDS; band++)
    {
        CHECK(context, context->equalizer.active_gain_db[band] ==
                       before[band]);
    }
}

static int perform_action(STRESS_CONTEXT *context, int action_kind)
{
    EQ_CONTROL_SEQUENCE sequence_before;
    EQ_CONTROL_SEQUENCE token;
    EQ_UI_GAIN_HALF_DB applied_before[EQ_NUM_BANDS];
    float active_before[EQ_NUM_BANDS];
    unsigned int random_value;
    int displayed_page;
    int page_building;
    int index;
    int band;
    int expected;
    int touch_serviced;

    touch_serviced = 0;
    displayed_page = EqualizerDisplay_GetDisplayedPage();
    page_building = EqualizerDisplay_IsPageBuilding();
    sequence_before = context->mailbox.sequence;
    copy_active_gains(context, active_before);
    memcpy(applied_before, context->editor.applied_gain_half_db,
           sizeof(applied_before));
    random_value = stress_random(context);

    switch (action_kind)
    {
        case STRESS_SELECT_BAND:
            if ((displayed_page == EQ_UI_PAGE_EQ_EDITOR) &&
                (page_building == 0))
            {
                band = (int)(random_value % EQ_NUM_BANDS);
                expected = EQ_UI_ACTION_EDITOR_BAND_0 + band;
                (void)touch_rect(context, &EQ_UI_EDITOR_BAND_RECTS[band],
                                 expected);
                (void)EqualizerUiEditor_SelectBand(&context->editor, band);
                touch_serviced = 1;
            }
            break;
        case STRESS_PLUS:
        case STRESS_MINUS:
            if ((displayed_page == EQ_UI_PAGE_EQ_EDITOR) &&
                (page_building == 0))
            {
                expected = (action_kind == STRESS_PLUS) ?
                    EQ_UI_ACTION_EDITOR_PLUS : EQ_UI_ACTION_EDITOR_MINUS;
                (void)touch_rect(
                    context,
                    (action_kind == STRESS_PLUS) ?
                        &EQ_UI_EDITOR_PLUS_RECT : &EQ_UI_EDITOR_MINUS_RECT,
                    expected);
                (void)EqualizerUiEditor_StepSelected(
                    &context->editor,
                    (action_kind == STRESS_PLUS) ? 1 : -1);
                touch_serviced = 1;
            }
            break;
        case STRESS_APPLY:
            if ((displayed_page == EQ_UI_PAGE_EQ_EDITOR) &&
                (page_building == 0))
            {
                (void)touch_rect(context, &EQ_UI_EDITOR_APPLY_RECT,
                                 EQ_UI_ACTION_EDITOR_APPLY);
                token = submit_editor(context);
                if (token != 0U)
                {
                    CHECK(context, token == context->mailbox.sequence);
                }
                touch_serviced = 1;
            }
            break;
        case STRESS_RESET:
            if ((displayed_page == EQ_UI_PAGE_EQ_EDITOR) &&
                (page_building == 0))
            {
                (void)touch_rect(context, &EQ_UI_EDITOR_RESET_RECT,
                                 EQ_UI_ACTION_EDITOR_RESET_FLAT);
                token = submit_reset(context);
                CHECK(context, token != 0U);
                touch_serviced = 1;
            }
            break;
        case STRESS_PRESET:
            if (page_building == 0)
            {
                index = (int)(random_value % EQ_PRESET_COUNT);
                expected = EQ_UI_ACTION_PRESET_FLAT + index;
                (void)touch_rect(context, &EQ_UI_PRESET_RECTS[index],
                                 expected);
                token = submit_preset(context, index);
                CHECK(context, token != 0U);
                touch_serviced = 1;
            }
            break;
        case STRESS_PAGE_SWITCH:
            (void)touch_rect(context, &EQ_UI_PAGE_SWITCH_RECT,
                             EQ_UI_ACTION_PAGE_SWITCH);
            context->requested_page =
                (context->requested_page == EQ_UI_PAGE_DYNAMIC_STATUS) ?
                EQ_UI_PAGE_EQ_EDITOR : EQ_UI_PAGE_DYNAMIC_STATUS;
            touch_serviced = 1;
            break;
        case STRESS_DYNAMIC_TOGGLE:
            if ((displayed_page == EQ_UI_PAGE_DYNAMIC_STATUS) &&
                (page_building == 0))
            {
                index = (int)(random_value % EQ_UI_DYNAMIC_COUNT);
                expected = EQ_UI_ACTION_SMART_TOGGLE + index * 2;
                (void)touch_rect(context,
                                 &EQ_UI_DYNAMIC_TOGGLE_RECTS[index],
                                 expected);
                context->dynamic_enabled[index] =
                    !context->dynamic_enabled[index];
                touch_serviced = 1;
            }
            break;
        case STRESS_STRENGTH:
            if ((displayed_page == EQ_UI_PAGE_DYNAMIC_STATUS) &&
                (page_building == 0))
            {
                index = (int)(random_value % EQ_UI_DYNAMIC_COUNT);
                expected = EQ_UI_ACTION_SMART_STRENGTH + index * 2;
                (void)touch_rect(context,
                                 &EQ_UI_DYNAMIC_STRENGTH_RECTS[index],
                                 expected);
                context->dynamic_strength[index] =
                    (context->dynamic_strength[index] >= 3) ?
                        1 : context->dynamic_strength[index] + 1;
                touch_serviced = 1;
            }
            break;
        case STRESS_ANALYZER_PUBLICATION:
            context->analyzer_pending = 1U;
            break;
        case STRESS_BUILDER_SERVICE:
        case STRESS_FRAME_BOUNDARY:
        case STRESS_LCD_SERVICE:
        default:
            break;
    }

    if ((action_kind == STRESS_SELECT_BAND) ||
        (action_kind == STRESS_PLUS) ||
        (action_kind == STRESS_MINUS))
    {
        CHECK(context, context->mailbox.sequence == sequence_before);
        CHECK(context, memcmp(applied_before,
                              context->editor.applied_gain_half_db,
                              sizeof(applied_before)) == 0);
    }
    check_active_gains_unchanged(context, active_before);
    if (touch_serviced != 0)
    {
        context->accepted_count[action_kind]++;
        EqualizerLcdPolicy_RecordControlChange(
            &context->lcd_policy, context->frame);
    }
    return touch_serviced;
}

static void service_control_boundary(STRESS_CONTEXT *context)
{
    EQ_CONTROL_SEQUENCE old_target_sequence;
    EQ_CONTROL_SEQUENCE old_installed_sequence;
    EQ_CONTROL_SEQUENCE ready_sequence;
    int old_target_valid;
    int old_target_uninstalled;
    int ready_valid;
    int mailbox_result;
    int install_result;

    old_target_sequence = context->control.target_sequence;
    old_installed_sequence = context->control.installed_sequence;
    old_target_valid = context->control.target_valid;
    old_target_uninstalled = old_target_valid &&
        (old_target_sequence != context->control.installed_sequence);
    mailbox_result = EqualizerControl_ServiceMailbox(
        &context->control, &context->mailbox, &context->equalizer);
    if ((mailbox_result == EQ_CONTROL_ACCEPTED) &&
        (context->control.target_sequence != old_target_sequence) &&
        old_target_uninstalled)
    {
        context->stale_count++;
    }
    EqualizerControl_InvalidateStaleWork(&context->control);
    ready_valid = context->control.ready_candidate.valid;
    ready_sequence = context->control.ready_candidate.request_sequence;
    install_result = EqualizerControl_TryInstallReady(
        &context->control, &context->equalizer);
    if (install_result == EQ_INSTALL_STALE)
    {
        context->stale_install_count++;
    }
    if (context->control.installed_sequence != old_installed_sequence)
    {
        CHECK(context, ready_valid != 0);
        CHECK(context, install_result == EQ_INSTALL_INSTALLED);
        CHECK(context, context->control.installed_sequence ==
                       ready_sequence);
        CHECK(context, context->control.installed_sequence ==
                       context->control.target_sequence);
    }
    CHECK(context, (install_result == EQ_INSTALL_INSTALLED) ||
                   (install_result == EQ_INSTALL_BUSY) ||
                   (install_result == EQ_INSTALL_INVALID) ||
                   (install_result == EQ_INSTALL_STALE));
}

static void publish_analyzer_snapshot(STRESS_CONTEXT *context)
{
    int band;

    context->analyzer_valid = 1U;
    context->analyzer_warm = 1U;
    for (band = 0; band < EQ_UI_ANALYZER_COUNT; band++)
    {
        context->analyzer_db[band] =
            (int)((context->frame * (unsigned long)(band + 3) +
                   (unsigned long)(band * 11)) % 41UL) - 20;
    }
    context->analyzer_pending = 0U;
    context->analyzer_publication_count++;
}

static void service_one_background(STRESS_CONTEXT *context,
                                   int touch_serviced)
{
    unsigned long payload_before;
    EQ_CONTROL_SEQUENCE installed_before;
    int pending_before;
    int ready_before;
    int builder_eligible;
    int analyzer_eligible;
    int lcd_policy_result;
    int lcd_eligible;
    int background_kind;
    int builder_result;
    int lcd_job;
    int completed_kind;
    int background_count;

    builder_eligible = EqualizerControl_BuilderEligible(
        &context->control, &context->equalizer);
    analyzer_eligible = EqualizerAnalyzer_CanService(
        0, 0, 0, 0, builder_eligible, 1,
        (int)context->analyzer_pending);
    lcd_policy_result = EqualizerLcdPolicy_Decide(
        &context->lcd_policy, context->frame,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, touch_serviced, 0, 0,
        EqualizerDisplay_HasEligibleJob(context->frame));
    lcd_eligible = (lcd_policy_result == EQ_LCD_POLICY_SERVICE) ? 1 : 0;
    completed_kind = EQ_BACKGROUND_NONE;
    background_count = 0;

    if (touch_serviced != 0)
    {
        CHECK(context, lcd_eligible == 0);
        background_kind = EQ_BACKGROUND_NONE;
    }
    else
    {
        background_kind = EqualizerBackgroundService_Decide(
            &context->background, context->frame,
            0, 0, 0, 0,
            builder_eligible, analyzer_eligible, 0, lcd_eligible);
    }

    if (builder_eligible != 0 && touch_serviced == 0)
    {
        CHECK(context, background_kind == EQ_BACKGROUND_BUILDER);
    }
    else if ((analyzer_eligible != 0) && (touch_serviced == 0))
    {
        CHECK(context, background_kind == EQ_BACKGROUND_ANALYZER);
    }

    if (background_kind == EQ_BACKGROUND_BUILDER)
    {
        payload_before = context->control.builder.payload_slice_count;
        ready_before = context->control.ready_candidate.valid;
        pending_before = context->equalizer.pending_bank_valid;
        installed_before = context->control.installed_sequence;
        builder_result = EqualizerControl_ServiceOneBuilderSlice(
            &context->control);
        CHECK(context, builder_result == EQ_BUILDER_WORKED);
        CHECK(context,
              context->control.builder.payload_slice_count ==
              payload_before + 1UL);
        if ((ready_before == 0) &&
            (context->control.ready_candidate.valid != 0))
        {
            CHECK(context, context->equalizer.pending_bank_valid ==
                           pending_before);
            CHECK(context, context->control.installed_sequence ==
                           installed_before);
        }
        context->builder_service_count++;
        completed_kind = EQ_BACKGROUND_BUILDER;
        background_count++;
    }
    else if (background_kind == EQ_BACKGROUND_ANALYZER)
    {
        publish_analyzer_snapshot(context);
        completed_kind = EQ_BACKGROUND_ANALYZER;
        background_count++;
    }
    else if (background_kind == EQ_BACKGROUND_LCD)
    {
        lcd_job = EqualizerDisplay_ServiceOneJob(context->frame);
        CHECK(context, lcd_job > EQ_LCD_JOB_NONE);
        CHECK(context, lcd_job <= EQ_UI_JOB_COUNT);
        CHECK(context, lcd_job <= 32);
        if (lcd_job != EQ_LCD_JOB_NONE)
        {
            EqualizerLcdPolicy_RecordService(
                &context->lcd_policy, context->frame, 1);
            context->lcd_service_count++;
            completed_kind = EQ_BACKGROUND_LCD;
            background_count++;
        }
    }
    CHECK(context, background_count <= 1);
    if (completed_kind != EQ_BACKGROUND_NONE)
    {
        EqualizerBackgroundService_Record(
            &context->background, context->frame, completed_kind);
        CHECK(context, EqualizerBackgroundService_Decide(
            &context->background, context->frame,
            0, 0, 0, 0, 1, 1, 0, 1) == EQ_BACKGROUND_NONE);
    }
    if (background_kind == EQ_BACKGROUND_ANALYZER)
    {
        request_display_snapshot(context);
    }
}

static void check_builder_state(STRESS_CONTEXT *context)
{
    const EQ_BANK_BUILDER *builder;

    builder = &context->control.builder;
    CHECK(context, builder->state >= EQ_BUILDER_IDLE);
    CHECK(context, builder->state <= EQ_BUILDER_ERROR);
    CHECK(context, builder->section_index >= 0);
    CHECK(context, builder->section_index <= EQ_NUM_BANDS);
    CHECK(context, builder->scan_index >= 0);
    CHECK(context, builder->scan_index <= EQ_HEADROOM_POINT_COUNT);
    CHECK(context, (context->control.target_valid == 0) ||
                   (context->control.target_valid == 1));
    if ((builder->state == EQ_BUILDER_DESIGN_SECTION) ||
        (builder->state == EQ_BUILDER_SCAN_HEADROOM) ||
        (builder->state == EQ_BUILDER_FINALIZE))
    {
        CHECK(context, context->control.target_valid != 0);
        CHECK(context, builder->generation ==
                       context->control.target_generation);
        CHECK(context, builder->request_sequence ==
                       context->control.target_sequence);
    }
    if (context->control.ready_candidate.valid != 0)
    {
        CHECK(context, context->control.ready_candidate.generation ==
                       context->control.target_generation);
        CHECK(context,
              context->control.ready_candidate.request_sequence ==
              context->control.target_sequence);
    }
}

static void check_state_invariants(STRESS_CONTEXT *context)
{
    int finite;
    int band;

    CHECK(context, context->editor.selected_band >= 0);
    CHECK(context, context->editor.selected_band < EQ_NUM_BANDS);
    CHECK(context, context->editor.draft_dirty <= 1U);
    CHECK(context, context->editor.submitted_valid <= 1U);
    for (band = 0; band < EQ_NUM_BANDS; band++)
    {
        CHECK(context, context->editor.applied_gain_half_db[band] >=
                       EQ_UI_GAIN_HALF_DB_MIN);
        CHECK(context, context->editor.applied_gain_half_db[band] <=
                       EQ_UI_GAIN_HALF_DB_MAX);
        CHECK(context, context->editor.draft_gain_half_db[band] >=
                       EQ_UI_GAIN_HALF_DB_MIN);
        CHECK(context, context->editor.draft_gain_half_db[band] <=
                       EQ_UI_GAIN_HALF_DB_MAX);
        CHECK(context, context->editor.submitted_gain_half_db[band] >=
                       EQ_UI_GAIN_HALF_DB_MIN);
        CHECK(context, context->editor.submitted_gain_half_db[band] <=
                       EQ_UI_GAIN_HALF_DB_MAX);
        CHECK(context, context->equalizer.band_gain_db[band] >=
                       EQ_GAIN_MIN_DB);
        CHECK(context, context->equalizer.band_gain_db[band] <=
                       EQ_GAIN_MAX_DB);
        CHECK(context, context->equalizer.active_gain_db[band] >=
                       EQ_GAIN_MIN_DB);
        CHECK(context, context->equalizer.active_gain_db[band] <=
                       EQ_GAIN_MAX_DB);
        CHECK(context, context->control.shadow_gain_db[band] >=
                       EQ_GAIN_MIN_DB);
        CHECK(context, context->control.shadow_gain_db[band] <=
                       EQ_GAIN_MAX_DB);
        CHECK(context, context->control.target_gain_db[band] >=
                       EQ_GAIN_MIN_DB);
        CHECK(context, context->control.target_gain_db[band] <=
                       EQ_GAIN_MAX_DB);
    }
    CHECK(context, (context->equalizer.pending_bank_valid == 0) ||
                   (context->equalizer.pending_bank_valid == 1));
    if (context->equalizer.pending_bank_valid != 0)
    {
        CHECK(context, context->equalizer.transition_total > 0);
        CHECK(context, context->equalizer.transition_remaining > 0);
        CHECK(context, context->equalizer.transition_remaining <=
                       context->equalizer.transition_total);
    }
    CHECK(context, context->equalizer.clip_count == 0UL);
    CHECK(context, EQ_DebugClipCount == 0UL);
    CHECK(context, EQ_DebugLcdBoundsFailureCount == 0UL);
    CHECK(context, EQ_DebugLcdUnexpectedFullRedrawCount == 0UL);
    CHECK(context, EQ_DebugLcdAutoDisabledCount == 0UL);
    check_builder_state(context);
    finite = bank_is_finite(&context->equalizer.active_bank);
    if ((finite != 0) && (context->equalizer.pending_bank_valid != 0))
    {
        finite = bank_is_finite(&context->equalizer.pending_bank);
    }
    if ((finite != 0) &&
        (context->control.ready_candidate.valid != 0))
    {
        finite = bank_is_finite(&context->control.ready_candidate.bank);
    }
    if (finite == 0)
    {
        context->nonfinite_count++;
        stress_fail(context, __LINE__, "all filter state finite");
    }
    check_display_pending(context);
}

static void process_audio_and_boundary(STRESS_CONTEXT *context)
{
    short input[STRESS_AUDIO_SAMPLES];
    short output[STRESS_AUDIO_SAMPLES];
    int sample;

    for (sample = 0; sample < STRESS_AUDIO_SAMPLES; sample++)
    {
        input[sample] = (short)(((context->frame * 17UL +
                                  (unsigned long)(sample * 29)) %
                                 513UL) - 256L);
    }
    Equalizer_ProcessFrame(&context->equalizer, input, output,
                           STRESS_AUDIO_SAMPLES);
    EqualizerControl_ObserveFrameBoundary(
        &context->control, &context->equalizer);
    observe_applied_sequence(context);
}

static void run_frame(STRESS_CONTEXT *context, int action_kind,
                      int count_action)
{
    int touch_serviced;

    context->frame++;
    process_audio_and_boundary(context);
    if (count_action != 0)
    {
        context->action_count[action_kind]++;
    }
    touch_serviced = perform_action(context, action_kind);
    service_control_boundary(context);
    update_apply_status(context);
    request_display_snapshot(context);
    service_one_background(context, touch_serviced);
    check_state_invariants(context);
}

static int choose_action(STRESS_CONTEXT *context)
{
    unsigned int value;

    value = stress_random(context) & 0xFFU;
    if (value < 24U) return STRESS_SELECT_BAND;
    if (value < 48U) return STRESS_PLUS;
    if (value < 72U) return STRESS_MINUS;
    if (value < 80U) return STRESS_APPLY;
    if (value < 84U) return STRESS_RESET;
    if (value < 92U) return STRESS_PRESET;
    if (value < 94U) return STRESS_PAGE_SWITCH;
    if (value < 110U) return STRESS_DYNAMIC_TOGGLE;
    if (value < 126U) return STRESS_STRENGTH;
    if (value < 158U) return STRESS_BUILDER_SERVICE;
    if (value < 206U) return STRESS_FRAME_BOUNDARY;
    if (value < 230U) return STRESS_ANALYZER_PUBLICATION;
    return STRESS_LCD_SERVICE;
}

static int settle_sequence(STRESS_CONTEXT *context,
                           EQ_CONTROL_SEQUENCE sequence)
{
    int guard;

    for (guard = 0; guard < 4096; guard++)
    {
        run_frame(context, STRESS_FRAME_BOUNDARY, 0);
        if ((context->control.applied_sequence == sequence) &&
            !Equalizer_HasPendingTransition(&context->equalizer))
        {
            return 1;
        }
    }
    return 0;
}

static void run_wrap_and_custom_probe(STRESS_CONTEXT *context)
{
    EQ_CONTROL_SEQUENCE maximum_token;
    EQ_CONTROL_SEQUENCE wrapped_token;
    EQ_CONTROL_SEQUENCE custom_token;
    int guard;

    context->mailbox.sequence = 0xFFFFFFFCU;
    maximum_token = submit_preset(context, EQ_PRESET_BASS_BOOST);
    CHECK(context, maximum_token == 0xFFFFFFFEU);
    CHECK(context, settle_sequence(context, maximum_token));
    wrapped_token = submit_preset(context, EQ_PRESET_VOCAL);
    CHECK(context, wrapped_token == 2U);
    CHECK(context, settle_sequence(context, wrapped_token));
    CHECK(context, context->wrap_verified != 0U);

    context->requested_page = EQ_UI_PAGE_EQ_EDITOR;
    request_display_snapshot(context);
    for (guard = 0; guard < 512; guard++)
    {
        if (!EqualizerDisplay_IsPageBuilding() &&
            (EqualizerDisplay_GetDisplayedPage() ==
             EQ_UI_PAGE_EQ_EDITOR))
        {
            break;
        }
        run_frame(context, STRESS_LCD_SERVICE, 0);
    }
    CHECK(context, guard < 512);
    CHECK(context, EqualizerUiEditor_SelectBand(&context->editor, 3));
    CHECK(context, EqualizerUiEditor_StepSelected(&context->editor, 6));
    custom_token = submit_editor(context);
    CHECK(context, custom_token != 0U);
    CHECK(context, settle_sequence(context, custom_token));
    CHECK(context, Equalizer_GetAppliedPreset(&context->equalizer) ==
                   EQ_PRESET_CUSTOM);
}

static int wait_for_displayed_page(STRESS_CONTEXT *context, int page)
{
    int guard;

    context->requested_page = page;
    request_display_snapshot(context);
    for (guard = 0; guard < 512; guard++)
    {
        if (!EqualizerDisplay_IsPageBuilding() &&
            (EqualizerDisplay_GetDisplayedPage() == page))
        {
            return 1;
        }
        run_frame(context, STRESS_LCD_SERVICE, 0);
    }
    return 0;
}

static void run_ui_action_coverage(STRESS_CONTEXT *context)
{
    unsigned long accepted_before;
    int kind;

    CHECK(context, wait_for_displayed_page(
        context, EQ_UI_PAGE_EQ_EDITOR));
    for (kind = STRESS_SELECT_BAND; kind <= STRESS_PRESET; kind++)
    {
        accepted_before = context->accepted_count[kind];
        run_frame(context, kind, 0);
        CHECK(context, context->accepted_count[kind] ==
                       accepted_before + 1UL);
    }

    accepted_before = context->accepted_count[STRESS_PAGE_SWITCH];
    run_frame(context, STRESS_PAGE_SWITCH, 0);
    CHECK(context, context->accepted_count[STRESS_PAGE_SWITCH] ==
                   accepted_before + 1UL);
    CHECK(context, wait_for_displayed_page(
        context, EQ_UI_PAGE_DYNAMIC_STATUS));

    for (kind = STRESS_DYNAMIC_TOGGLE; kind <= STRESS_STRENGTH; kind++)
    {
        accepted_before = context->accepted_count[kind];
        run_frame(context, kind, 0);
        CHECK(context, context->accepted_count[kind] ==
                       accepted_before + 1UL);
    }
    if ((context->control.target_valid != 0) &&
        (context->control.target_sequence != 0U))
    {
        CHECK(context, settle_sequence(
            context, context->control.target_sequence));
    }
}

static void check_scheduler_contract(STRESS_CONTEXT *context)
{
    EQ_BACKGROUND_SERVICE_STATE background;
    EQ_LCD_SERVICE_POLICY lcd_policy;
    int kind;
    int lcd_decision;

    EqualizerBackgroundService_Init(&background);
    EqualizerLcdPolicy_Init(&lcd_policy);
    lcd_decision = EqualizerLcdPolicy_Decide(
        &lcd_policy, 10UL,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 1, 0, 0, 1);
    CHECK(context, lcd_decision == EQ_LCD_POLICY_NONE);

    kind = EqualizerBackgroundService_Decide(
        &background, 11UL, 0, 0, 0, 0, 1, 1, 0, 1);
    CHECK(context, kind == EQ_BACKGROUND_BUILDER);
    EqualizerBackgroundService_Record(&background, 11UL, kind);
    CHECK(context, EqualizerBackgroundService_Decide(
        &background, 11UL, 0, 0, 0, 0, 1, 1, 0, 1) ==
        EQ_BACKGROUND_NONE);

    kind = EqualizerBackgroundService_Decide(
        &background, 12UL, 0, 0, 0, 0, 0, 1, 0, 1);
    CHECK(context, kind == EQ_BACKGROUND_ANALYZER);
    EqualizerBackgroundService_Record(&background, 12UL, kind);
    kind = EqualizerBackgroundService_Decide(
        &background, 13UL, 0, 0, 0, 0, 0, 0, 0, 1);
    CHECK(context, kind == EQ_BACKGROUND_LCD);
    EqualizerBackgroundService_Record(&background, 13UL, kind);
    CHECK(context, EqualizerBackgroundService_Decide(
        &background, 14UL, 1, 0, 0, 0, 1, 1, 0, 1) ==
        EQ_BACKGROUND_NONE);
}

static void initialize_context(STRESS_CONTEXT *context)
{
    EQ_LCD_HW_SNAPSHOT hardware;
    int index;

    memset(context, 0, sizeof(*context));
    context->random_state = STRESS_SEED;
    context->requested_page = EQ_UI_PAGE_DYNAMIC_STATUS;
    for (index = 0; index < EQ_UI_DYNAMIC_COUNT; index++)
    {
        context->dynamic_strength[index] = 1;
    }
    Equalizer_Init(&context->equalizer);
    EqualizerControl_Init(&context->control, &context->equalizer);
    EqualizerUiEditor_Init(&context->editor);
    EqualizerUiTouch_Init(&context->touch);
    EqualizerBackgroundService_Init(&context->background);
    EqualizerLcdPolicy_Init(&context->lcd_policy);
    EqualizerDisplay_Init();
    memset(&hardware, 0, sizeof(hardware));
    hardware.frame_base = EQ_DebugLcdExpectedFrameBase;
    hardware.frame_end = EQ_DebugLcdExpectedFrameEnd;
    hardware.raster_control = 1UL;
    EqualizerDisplay_TestSetHardwareSnapshot(&hardware);
    CHECK(context, EqualizerDisplay_DrawStaticLayout() == 1);
    CHECK(context, EQ_DebugLcdBoundsFailureCount == 0UL);
    EqualizerDisplay_BeginRuntime();
    EQ_DebugLcdRuntimeMask = EQ_UI_RUNTIME_ALL;
    request_display_snapshot(context);
}

int main(void)
{
    STRESS_CONTEXT context;
    unsigned long action;
    unsigned long cancel_count;
    int kind;

    initialize_context(&context);
    check_scheduler_contract(&context);
    run_wrap_and_custom_probe(&context);

    memset(context.action_count, 0, sizeof(context.action_count));
    memset(context.accepted_count, 0, sizeof(context.accepted_count));
    context.stale_count = 0UL;
    context.stale_install_count = 0UL;
    context.applied_count = 0UL;
    context.builder_service_count = 0UL;
    context.analyzer_publication_count = 0UL;
    context.lcd_service_count = 0UL;
    context.cancel_baseline = context.control.builder.cancel_count;

    for (action = 0UL; action < STRESS_ACTIONS; action++)
    {
        kind = choose_action(&context);
        run_frame(&context, kind, 1);
    }

    if ((context.control.target_valid != 0) &&
        (context.control.target_sequence != 0U))
    {
        int band;

        CHECK(&context, settle_sequence(
            &context, context.control.target_sequence));
        CHECK(&context, context.control.applied_sequence ==
                        context.control.target_sequence);
        CHECK(&context, !Equalizer_HasPendingTransition(
                            &context.equalizer));
        for (band = 0; band < EQ_NUM_BANDS; band++)
        {
            CHECK(&context,
                  fabsf(context.equalizer.active_gain_db[band] -
                        context.control.target_gain_db[band]) < 1.0e-6f);
        }
    }

    run_ui_action_coverage(&context);

    for (kind = 0; kind < STRESS_ACTION_KIND_COUNT; kind++)
    {
        CHECK(&context, context.action_count[kind] != 0UL);
    }
    for (kind = STRESS_SELECT_BAND;
         kind <= STRESS_STRENGTH; kind++)
    {
        CHECK(&context, context.accepted_count[kind] != 0UL);
    }
    CHECK(&context, context.stale_count != 0UL);
    CHECK(&context, context.stale_install_count == 0UL);
    CHECK(&context, context.builder_service_count != 0UL);
    CHECK(&context, context.analyzer_publication_count != 0UL);
    CHECK(&context, context.lcd_service_count != 0UL);
    CHECK(&context, context.applied_count != 0UL);
    CHECK(&context, context.wrap_verified != 0U);
    CHECK(&context, context.nonfinite_count == 0UL);
    cancel_count = context.control.builder.cancel_count -
                   context.cancel_baseline;
    CHECK(&context, cancel_count != 0UL);

    printf("equalizer_editor_stress seed=0x%08X actions=%lu "
           "stale=%lu stale_install=%lu cancel=%lu applied=%lu builder=%lu "
           "analyzer=%lu lcd=%lu wrap=%u nonfinite=%lu failures=%lu\n",
           STRESS_SEED, STRESS_ACTIONS,
           context.stale_count, context.stale_install_count,
           cancel_count, context.applied_count,
           context.builder_service_count,
           context.analyzer_publication_count,
           context.lcd_service_count, context.wrap_verified,
           context.nonfinite_count, context.failures);
    return (context.failures == 0UL) ? 0 : 1;
}
