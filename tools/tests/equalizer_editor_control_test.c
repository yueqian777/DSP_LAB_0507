#include "user_equalizer.h"
#include "user_equalizer_control.h"
#include "user_equalizer_ui_logic.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(condition) do { \
    if (!(condition)) { \
        printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
        failures++; \
    } \
} while (0)

static void fill_request(EQ_CONTROL_REQUEST *request, int command)
{
    memset(request, 0, sizeof(*request));
    request->command = command;
    request->preset = EQ_PRESET_NONE;
}

static EQ_CONTROL_SEQUENCE submit_editor(
    EQ_UI_EDITOR_STATE *editor, EQ_CONTROL_MAILBOX *mailbox)
{
    EQ_CONTROL_REQUEST request;
    EQ_CONTROL_SEQUENCE token;

    if (!EqualizerUiEditor_HasSubmittableDraft(editor))
    {
        return 0U;
    }
    fill_request(&request, EQ_CONTROL_SET_ALL);
    request.preset = EQ_PRESET_CUSTOM;
    EqualizerUiEditor_CopyDraftDb(editor, request.shadow_gain_db);
    token = EqualizerControl_SubmitRequest(mailbox, &request);
    EqualizerUiEditor_MarkSubmitted(editor, token);
    return token;
}

static EQ_CONTROL_SEQUENCE submit_preset(
    EQ_CONTROL_MAILBOX *mailbox, int preset)
{
    EQ_CONTROL_REQUEST request;

    fill_request(&request, EQ_CONTROL_APPLY_PRESET);
    request.preset = preset;
    return EqualizerControl_SubmitRequest(mailbox, &request);
}

static EQ_CONTROL_SEQUENCE submit_reset(
    EQ_UI_EDITOR_STATE *editor, EQ_CONTROL_MAILBOX *mailbox)
{
    EQ_CONTROL_REQUEST request;
    EQ_CONTROL_SEQUENCE token;

    (void)EqualizerUiEditor_SetDraftFlat(editor);
    fill_request(&request, EQ_CONTROL_RESET_FLAT);
    request.preset = EQ_PRESET_FLAT;
    token = EqualizerControl_SubmitRequest(mailbox, &request);
    EqualizerUiEditor_MarkSubmitted(editor, token);
    return token;
}

static void service_safe_boundary(EQ_CONTROL_STATE *control,
                                  EQ_CONTROL_MAILBOX *mailbox,
                                  EQ_STATE *equalizer)
{
    EqualizerControl_ObserveFrameBoundary(control, equalizer);
    (void)EqualizerControl_ServiceMailbox(control, mailbox, equalizer);
    EqualizerControl_InvalidateStaleWork(control);
    (void)EqualizerControl_TryInstallReady(control, equalizer);
    if (EqualizerControl_BuilderEligible(control, equalizer))
    {
        (void)EqualizerControl_ServiceOneBuilderSlice(control);
    }
}

static int settle_token(EQ_CONTROL_STATE *control,
                        EQ_CONTROL_MAILBOX *mailbox,
                        EQ_STATE *equalizer,
                        EQ_CONTROL_SEQUENCE token)
{
    float input[EQ_FRAME_LEN];
    float output[EQ_FRAME_LEN];
    int guard;

    memset(input, 0, sizeof(input));
    for (guard = 0; guard < 2048; guard++)
    {
        service_safe_boundary(control, mailbox, equalizer);
        if (Equalizer_HasPendingTransition(equalizer))
        {
            Equalizer_ProcessFrameFloat(
                equalizer, input, output, EQ_FRAME_LEN);
        }
        if ((control->applied_sequence == token) &&
            !Equalizer_HasPendingTransition(equalizer))
        {
            return guard + 1;
        }
    }
    return -1;
}

static void observe_editor(EQ_UI_EDITOR_STATE *editor,
                           const EQ_CONTROL_STATE *control,
                           const EQ_STATE *equalizer)
{
    EQ_UI_GAIN_HALF_DB gains[EQ_NUM_BANDS];
    int band;

    for (band = 0; band < EQ_NUM_BANDS; band++)
    {
        gains[band] = EqualizerUi_GainDbToHalf(
            equalizer->active_gain_db[band]);
    }
    EqualizerUiEditor_ObserveApplied(
        editor, gains, Equalizer_GetAppliedPreset(equalizer),
        control->applied_sequence);
}

static void init_all(EQ_STATE *equalizer,
                     EQ_CONTROL_STATE *control,
                     EQ_CONTROL_MAILBOX *mailbox,
                     EQ_UI_EDITOR_STATE *editor)
{
    Equalizer_Init(equalizer);
    EqualizerControl_Init(control, equalizer);
    memset(mailbox, 0, sizeof(*mailbox));
    EqualizerUiEditor_Init(editor);
    observe_editor(editor, control, equalizer);
}

static void test_apply_and_reset_contract(void)
{
    EQ_STATE equalizer;
    EQ_CONTROL_STATE control;
    EQ_CONTROL_MAILBOX mailbox;
    EQ_UI_EDITOR_STATE editor;
    EQ_CONTROL_SEQUENCE token;
    EQ_CONTROL_SEQUENCE sequence_before;
    int band;

    init_all(&equalizer, &control, &mailbox, &editor);
    sequence_before = mailbox.sequence;
    CHECK(EqualizerUiEditor_SelectBand(&editor, 2));
    for (band = 0; band < 4; band++)
    {
        CHECK(EqualizerUiEditor_StepSelected(&editor, 1));
    }
    CHECK(mailbox.sequence == sequence_before);
    CHECK(control.builder.state == EQ_BUILDER_IDLE);
    CHECK(equalizer.active_gain_db[2] == 0.0f);

    token = submit_editor(&editor, &mailbox);
    CHECK(token != 0U);
    CHECK(mailbox.command == EQ_CONTROL_SET_ALL);
    CHECK(mailbox.preset == EQ_PRESET_CUSTOM);
    CHECK(mailbox.shadow_gain_db[2] == 2.0f);
    for (band = 0; band < 10; band++)
    {
        CHECK(submit_editor(&editor, &mailbox) == 0U);
    }
    CHECK(mailbox.sequence == token);
    CHECK(settle_token(&control, &mailbox, &equalizer, token) > 0);
    observe_editor(&editor, &control, &equalizer);
    CHECK(editor.applied_preset == EQ_PRESET_CUSTOM);
    CHECK(editor.applied_gain_half_db[2] == 4);
    CHECK(editor.draft_dirty == 0U);
    for (band = 0; band < EQ_NUM_BANDS; band++)
    {
        if (band != 2)
        {
            CHECK(editor.applied_gain_half_db[band] == 0);
        }
    }

    token = submit_preset(&mailbox, EQ_PRESET_VOCAL);
    CHECK(settle_token(&control, &mailbox, &equalizer, token) > 0);
    observe_editor(&editor, &control, &equalizer);
    CHECK(editor.applied_preset == EQ_PRESET_VOCAL);
    CHECK(editor.draft_dirty == 0U);
    CHECK(memcmp(editor.draft_gain_half_db,
                 editor.submitted_gain_half_db,
                 sizeof(editor.draft_gain_half_db)) == 0);

    CHECK(EqualizerUiEditor_SelectBand(&editor, 4));
    CHECK(EqualizerUiEditor_StepSelected(&editor, 1));
    CHECK(editor.draft_dirty != 0U);
    token = submit_preset(&mailbox, EQ_PRESET_BASS_BOOST);
    CHECK(settle_token(&control, &mailbox, &equalizer, token) > 0);
    observe_editor(&editor, &control, &equalizer);
    CHECK(editor.applied_preset == EQ_PRESET_BASS_BOOST);
    CHECK(editor.draft_dirty == 0U);

    token = submit_reset(&editor, &mailbox);
    CHECK(mailbox.command == EQ_CONTROL_RESET_FLAT);
    CHECK(settle_token(&control, &mailbox, &equalizer, token) > 0);
    observe_editor(&editor, &control, &equalizer);
    CHECK(editor.applied_preset == EQ_PRESET_FLAT);
    CHECK(editor.applied_sequence == token);
    for (band = 0; band < EQ_NUM_BANDS; band++)
    {
        CHECK(editor.applied_gain_half_db[band] == 0);
    }
}

static void set_editor_pattern(EQ_UI_EDITOR_STATE *editor,
                               int first, int second)
{
    int band;

    for (band = 0; band < EQ_NUM_BANDS; band++)
    {
        editor->draft_gain_half_db[band] =
            (EQ_UI_GAIN_HALF_DB)((band & 1) ? first : second);
    }
    editor->draft_version++;
    editor->draft_dirty = 1U;
}

static void test_latest_wins_control(void)
{
    EQ_STATE equalizer;
    EQ_CONTROL_STATE control;
    EQ_CONTROL_MAILBOX mailbox;
    EQ_UI_EDITOR_STATE editor;
    EQ_CONTROL_SEQUENCE token_a;
    EQ_CONTROL_SEQUENCE token_b;
    unsigned long cancel_before;
    int band;

    init_all(&equalizer, &control, &mailbox, &editor);
    set_editor_pattern(&editor, 2, -2);
    token_a = submit_editor(&editor, &mailbox);
    for (band = 0; band < 8; band++)
    {
        service_safe_boundary(&control, &mailbox, &equalizer);
    }
    cancel_before = control.builder.cancel_count;
    set_editor_pattern(&editor, 6, -4);
    token_b = submit_editor(&editor, &mailbox);
    CHECK(token_b != token_a);
    CHECK(settle_token(&control, &mailbox, &equalizer, token_b) > 0);
    CHECK(control.applied_sequence == token_b);
    CHECK(control.builder.cancel_count > cancel_before);
    for (band = 0; band < EQ_NUM_BANDS; band++)
    {
        float expected = (float)((band & 1) ? 6 : -4) * 0.5f;
        CHECK(fabsf(equalizer.active_gain_db[band] - expected) < 1.0e-6f);
    }

    init_all(&equalizer, &control, &mailbox, &editor);
    set_editor_pattern(&editor, 8, -8);
    token_a = submit_editor(&editor, &mailbox);
    for (band = 0; band < 5; band++)
    {
        service_safe_boundary(&control, &mailbox, &equalizer);
    }
    token_b = submit_preset(&mailbox, EQ_PRESET_TREBLE_BOOST);
    CHECK(token_b != token_a);
    CHECK(settle_token(&control, &mailbox, &equalizer, token_b) > 0);
    CHECK(Equalizer_GetAppliedPreset(&equalizer) ==
          EQ_PRESET_TREBLE_BOOST);
    CHECK(control.applied_sequence == token_b);

    init_all(&equalizer, &control, &mailbox, &editor);
    token_a = submit_preset(&mailbox, EQ_PRESET_BASS_BOOST);
    service_safe_boundary(&control, &mailbox, &equalizer);
    CHECK(Equalizer_HasPendingTransition(&equalizer));
    set_editor_pattern(&editor, 4, -6);
    token_b = submit_editor(&editor, &mailbox);
    CHECK(token_b != token_a);
    CHECK(settle_token(&control, &mailbox, &equalizer, token_b) > 0);
    CHECK(control.applied_sequence == token_b);
    CHECK(Equalizer_GetAppliedPreset(&equalizer) == EQ_PRESET_CUSTOM);
}

int main(void)
{
    failures = 0;
    test_apply_and_reset_contract();
    test_latest_wins_control();
    printf("equalizer_editor_control failures=%d\n", failures);
    return (failures == 0) ? 0 : 1;
}
