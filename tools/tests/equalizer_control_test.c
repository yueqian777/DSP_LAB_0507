#include "user_equalizer_control.h"
#include "user_equalizer_flow.h"

#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

static int failures = 0;
static EQ_CONTROL_MAILBOX *hook_mailbox = 0;

#define CHECK(condition) do { \
    if (!(condition)) { \
        printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
        failures++; \
    } \
} while (0)

static void fill_request(EQ_CONTROL_REQUEST *request, int command)
{
    int band;

    memset(request, 0, sizeof(*request));
    request->command = command;
    request->preset = EQ_PRESET_NONE;
    for (band = 0; band < EQ_NUM_BANDS; band++)
    {
        request->shadow_gain_db[band] = 0.0f;
    }
}

static void replace_mailbox_during_read(EQ_CONTROL_MAILBOX *mailbox)
{
    EQ_CONTROL_REQUEST replacement;

    fill_request(&replacement, EQ_CONTROL_APPLY_PRESET);
    replacement.preset = EQ_PRESET_TREBLE_BOOST;
    CHECK(mailbox == hook_mailbox);
    CHECK(EqualizerControl_SubmitRequest(mailbox, &replacement) != 0U);
    EqualizerControl_SetReadHook(0);
}

static void process_until_settled(EQ_STATE *equalizer)
{
    float input[EQ_FRAME_LEN];
    float output[EQ_FRAME_LEN];
    int guard;

    memset(input, 0, sizeof(input));
    guard = 0;
    while ((Equalizer_HasPendingTransition(equalizer) != 0) &&
           (guard < 16))
    {
        Equalizer_ProcessFrameFloat(equalizer, input, output, EQ_FRAME_LEN);
        guard++;
    }
    CHECK(Equalizer_HasPendingTransition(equalizer) == 0);
}

static void test_mailbox_protocol(void)
{
    EQ_STATE equalizer;
    EQ_CONTROL_STATE control;
    EQ_CONTROL_MAILBOX mailbox;
    EQ_CONTROL_REQUEST request;
    EQ_CONTROL_SEQUENCE token;
    unsigned long rejected;

    Equalizer_Init(&equalizer);
    EqualizerControl_Init(&control, &equalizer);
    memset(&mailbox, 0, sizeof(mailbox));

    CHECK(sizeof(EQ_CONTROL_SEQUENCE) == 4U);
    CHECK(offsetof(EQ_CONTROL_MAILBOX, sequence) == 0U);
    mailbox.sequence = 1U;
    CHECK(EqualizerControl_ServiceMailbox(
        &control, &mailbox, &equalizer) == EQ_CONTROL_DEFERRED);
    CHECK(control.observed_sequence == 0U);

    mailbox.sequence = 0U;
    fill_request(&request, EQ_CONTROL_APPLY_PRESET);
    request.preset = EQ_PRESET_BASS_BOOST;
    token = EqualizerControl_SubmitRequest(&mailbox, &request);
    CHECK(token == 2U);
    CHECK(EqualizerControl_ServiceMailbox(
        &control, &mailbox, &equalizer) == EQ_CONTROL_ACCEPTED);
    CHECK(control.observed_sequence == token);
    CHECK(control.accepted_sequence == token);
    CHECK(control.target_sequence == token);
    CHECK(control.ready_candidate.valid == 1);
    CHECK(EqualizerControl_ServiceMailbox(
        &control, &mailbox, &equalizer) == EQ_CONTROL_DUPLICATE);

    fill_request(&request, 999);
    token = EqualizerControl_SubmitRequest(&mailbox, &request);
    rejected = control.rejected_count;
    CHECK(EqualizerControl_ServiceMailbox(
        &control, &mailbox, &equalizer) == EQ_CONTROL_REJECTED);
    CHECK(control.rejected_count == rejected + 1UL);
    CHECK(control.accepted_sequence != token);
    CHECK(EqualizerControl_ServiceMailbox(
        &control, &mailbox, &equalizer) == EQ_CONTROL_DUPLICATE);
    CHECK(control.rejected_count == rejected + 1UL);

    fill_request(&request, EQ_CONTROL_STEP_BAND);
    request.band = 2;
    request.step_db = (float)NAN;
    token = EqualizerControl_SubmitRequest(&mailbox, &request);
    CHECK(EqualizerControl_ServiceMailbox(
        &control, &mailbox, &equalizer) == EQ_CONTROL_REJECTED);
    CHECK(control.observed_sequence == token);
    CHECK(control.last_error == EQ_CONTROL_ERROR_NONFINITE);

    fill_request(&request, EQ_CONTROL_APPLY_PRESET);
    request.preset = EQ_PRESET_VOCAL;
    token = EqualizerControl_SubmitRequest(&mailbox, &request);
    hook_mailbox = &mailbox;
    EqualizerControl_SetReadHook(replace_mailbox_during_read);
    CHECK(EqualizerControl_ServiceMailbox(
        &control, &mailbox, &equalizer) == EQ_CONTROL_DEFERRED);
    CHECK(control.observed_sequence != token);
    CHECK(EqualizerControl_ServiceMailbox(
        &control, &mailbox, &equalizer) == EQ_CONTROL_ACCEPTED);
    CHECK(control.target_preset == EQ_PRESET_TREBLE_BOOST);
}

static void test_custom_builder_and_tokens(void)
{
    static const float custom[EQ_NUM_BANDS] =
    {
        -3.0f, -1.0f, 1.0f, 3.0f, 4.0f,
         2.0f,  0.0f, -2.0f, 1.0f, 3.0f
    };
    EQ_STATE equalizer;
    EQ_STATE reference;
    EQ_CONTROL_STATE control;
    EQ_CONTROL_MAILBOX mailbox;
    EQ_CONTROL_REQUEST request;
    unsigned long scans_before;
    int design_slices;
    int scan_slices;
    int finalize_slices;
    int previous_state;
    int band;

    Equalizer_Init(&equalizer);
    Equalizer_Init(&reference);
    EqualizerControl_Init(&control, &equalizer);
    memset(&mailbox, 0, sizeof(mailbox));
    fill_request(&request, EQ_CONTROL_SET_ALL);
    for (band = 0; band < EQ_NUM_BANDS; band++)
    {
        request.shadow_gain_db[band] = custom[band];
    }
    CHECK(EqualizerControl_SubmitRequest(&mailbox, &request) == 2U);
    scans_before = EQ_DebugHeadroomScanCount;
    CHECK(EqualizerControl_ServiceMailbox(
        &control, &mailbox, &equalizer) == EQ_CONTROL_ACCEPTED);
    CHECK(EQ_DebugHeadroomScanCount == scans_before);
    CHECK(control.builder.payload_slice_count == 0UL);
    CHECK(control.builder.state == EQ_BUILDER_DESIGN_SECTION);
    CHECK(control.ready_candidate.valid == 0);

    design_slices = 0;
    scan_slices = 0;
    finalize_slices = 0;
    while (EqualizerControl_BuilderEligible(&control, &equalizer) != 0)
    {
        previous_state = control.builder.state;
        CHECK(EqualizerControl_ServiceOneBuilderSlice(&control) ==
              EQ_BUILDER_WORKED);
        if (previous_state == EQ_BUILDER_DESIGN_SECTION)
        {
            design_slices++;
        }
        else if (previous_state == EQ_BUILDER_SCAN_HEADROOM)
        {
            scan_slices++;
        }
        else if (previous_state == EQ_BUILDER_FINALIZE)
        {
            finalize_slices++;
        }
    }
    CHECK(design_slices == 10);
    CHECK(scan_slices == 32);
    CHECK(finalize_slices == 1);
    CHECK(control.builder.payload_slice_count == 43UL);
    CHECK(control.builder.state == EQ_BUILDER_READY);
    CHECK(control.ready_candidate.valid == 1);
    CHECK(control.prepared_sequence == control.target_sequence);
    CHECK(EQ_DebugHeadroomScanCount == scans_before + 1UL);

    Equalizer_SetAllGainsDb(&reference, custom);
    CHECK(reference.pending_bank_valid == 1);
    for (band = 0; band < EQ_NUM_BANDS; band++)
    {
        const EQ_BIQUAD *actual =
            &control.ready_candidate.bank.section[band];
        const EQ_BIQUAD *expected = &reference.pending_bank.section[band];

        CHECK(fabsf(actual->b0 - expected->b0) < 1.0e-6f);
        CHECK(fabsf(actual->b1 - expected->b1) < 1.0e-6f);
        CHECK(fabsf(actual->b2 - expected->b2) < 1.0e-6f);
        CHECK(fabsf(actual->a1 - expected->a1) < 1.0e-6f);
        CHECK(fabsf(actual->a2 - expected->a2) < 1.0e-6f);
    }
    CHECK(fabsf(control.ready_candidate.bank.predicted_peak_db -
                reference.pending_bank.predicted_peak_db) < 1.0e-4f);
    CHECK(fabsf(control.ready_candidate.bank.preamp_db -
                reference.pending_bank.preamp_db) < 1.0e-4f);

    CHECK(EqualizerControl_TryInstallReady(&control, &equalizer) ==
          EQ_INSTALL_INSTALLED);
    CHECK(equalizer.pending_bank_valid == 1);
    CHECK(control.installed_sequence == control.target_sequence);
    CHECK(control.applied_sequence != control.installed_sequence);
    process_until_settled(&equalizer);
    EqualizerControl_ObserveFrameBoundary(&control, &equalizer);
    CHECK(control.applied_sequence == control.installed_sequence);
    CHECK(Equalizer_GetActiveGeneration(&equalizer) ==
          control.installed_generation);
}

static void test_latest_wins_and_cache(void)
{
    EQ_STATE equalizer;
    EQ_CONTROL_STATE control;
    EQ_CONTROL_MAILBOX mailbox;
    EQ_CONTROL_REQUEST request;
    EQ_CONTROL_SEQUENCE bass_token;
    EQ_CONTROL_SEQUENCE vocal_token;
    EQ_CONTROL_SEQUENCE treble_token;
    unsigned long scans;

    Equalizer_Init(&equalizer);
    EqualizerControl_Init(&control, &equalizer);
    memset(&mailbox, 0, sizeof(mailbox));
    CHECK(Equalizer_IsPresetCacheReady() == 1);
    scans = EQ_DebugHeadroomScanCount;

    fill_request(&request, EQ_CONTROL_APPLY_PRESET);
    request.preset = EQ_PRESET_BASS_BOOST;
    bass_token = EqualizerControl_SubmitRequest(&mailbox, &request);
    CHECK(EqualizerControl_ServiceMailbox(
        &control, &mailbox, &equalizer) == EQ_CONTROL_ACCEPTED);
    CHECK(EqualizerControl_TryInstallReady(&control, &equalizer) ==
          EQ_INSTALL_INSTALLED);

    request.preset = EQ_PRESET_VOCAL;
    vocal_token = EqualizerControl_SubmitRequest(&mailbox, &request);
    CHECK(EqualizerControl_ServiceMailbox(
        &control, &mailbox, &equalizer) == EQ_CONTROL_ACCEPTED);
    CHECK(control.target_sequence == vocal_token);
    CHECK(EqualizerControl_TryInstallReady(&control, &equalizer) ==
          EQ_INSTALL_BUSY);

    request.preset = EQ_PRESET_TREBLE_BOOST;
    treble_token = EqualizerControl_SubmitRequest(&mailbox, &request);
    CHECK(EqualizerControl_ServiceMailbox(
        &control, &mailbox, &equalizer) == EQ_CONTROL_ACCEPTED);
    CHECK(control.target_sequence == treble_token);
    CHECK(control.ready_candidate.request_sequence == treble_token);
    CHECK(control.ready_candidate.request_sequence != vocal_token);
    CHECK(control.builder.payload_slice_count == 0UL);
    CHECK(control.builder.restart_count == 0UL);
    CHECK(EQ_DebugHeadroomScanCount == scans);

    process_until_settled(&equalizer);
    EqualizerControl_ObserveFrameBoundary(&control, &equalizer);
    CHECK(control.applied_sequence == bass_token);
    CHECK(EqualizerControl_TryInstallReady(&control, &equalizer) ==
          EQ_INSTALL_INSTALLED);
    process_until_settled(&equalizer);
    EqualizerControl_ObserveFrameBoundary(&control, &equalizer);
    CHECK(control.applied_sequence == treble_token);
    CHECK(control.applied_sequence != vocal_token);
}

static void test_direct_setter_rejection(void)
{
    EQ_STATE equalizer;
    EQ_STATE before;
    float gains[EQ_NUM_BANDS];
    int band;

    Equalizer_Init(&equalizer);
    Equalizer_ApplyPreset(&equalizer, EQ_PRESET_BASS_BOOST);
    CHECK(equalizer.pending_bank_valid == 1);
    before = equalizer;
    for (band = 0; band < EQ_NUM_BANDS; band++)
    {
        gains[band] = (float)band - 4.0f;
    }
    Equalizer_SetAllGainsDb(&equalizer, gains);
    CHECK(memcmp(&before, &equalizer, sizeof(equalizer)) == 0);
    Equalizer_ApplyPreset(&equalizer, EQ_PRESET_VOCAL);
    CHECK(memcmp(&before, &equalizer, sizeof(equalizer)) == 0);
}

static void test_background_budget(void)
{
    EQ_BACKGROUND_SERVICE_STATE state;

    EqualizerBackgroundService_Init(&state);
    CHECK(EqualizerBackgroundService_Decide(&state, 0UL, 1, 1) ==
          EQ_BACKGROUND_BUILDER);
    EqualizerBackgroundService_Record(
        &state, 0UL, EQ_BACKGROUND_BUILDER);
    CHECK(EqualizerBackgroundService_Decide(&state, 0UL, 1, 1) ==
          EQ_BACKGROUND_NONE);
    CHECK(EqualizerBackgroundService_Decide(&state, 1UL, 1, 1) ==
          EQ_BACKGROUND_LCD);
    EqualizerBackgroundService_Record(&state, 1UL, EQ_BACKGROUND_LCD);
    CHECK(EqualizerBackgroundService_Decide(&state, 2UL, 1, 1) ==
          EQ_BACKGROUND_BUILDER);
    CHECK(EqualizerBackgroundService_Decide(&state, 2UL, 0, 1) ==
          EQ_BACKGROUND_LCD);
    CHECK(EqualizerBackgroundService_Decide(&state, 2UL, 1, 0) ==
          EQ_BACKGROUND_BUILDER);
}

int main(void)
{
    test_mailbox_protocol();
    test_custom_builder_and_tokens();
    test_latest_wins_and_cache();
    test_direct_setter_rejection();
    test_background_budget();
    printf("equalizer_control_test failures=%d\n", failures);
    return (failures == 0) ? 0 : 1;
}
