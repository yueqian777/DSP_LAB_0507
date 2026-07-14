#include "user_equalizer_response.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int failures = 0;

#define CHECK(condition) do { \
    if (!(condition)) { \
        printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
        failures++; \
    } \
} while (0)

static void settle(EQ_STATE *equalizer)
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

static void test_identity_paths(void)
{
    EQ_STATE equalizer;
    EQ_RESPONSE_SNAPSHOT snapshot;

    Equalizer_Init(&equalizer);
    CHECK(EqualizerResponse_CopyActive(&equalizer, &snapshot) == 1);
    CHECK(snapshot.path_type == EQ_RESPONSE_PATH_IDENTITY_RBJ_FLAT_COPY);
    CHECK(snapshot.identity == 1);

    Equalizer_SetCoreMode(&equalizer, EQ_CORE_RAW_COPY);
    CHECK(EqualizerResponse_CopyActive(&equalizer, &snapshot) == 1);
    CHECK(snapshot.path_type == EQ_RESPONSE_PATH_IDENTITY_RAW_COPY);
    Equalizer_SetCoreMode(&equalizer, EQ_CORE_FLOAT_COPY);
    CHECK(EqualizerResponse_CopyActive(&equalizer, &snapshot) == 1);
    CHECK(snapshot.path_type == EQ_RESPONSE_PATH_IDENTITY_FLOAT_COPY);
    Equalizer_SetBypass(&equalizer, 1);
    CHECK(EqualizerResponse_CopyActive(&equalizer, &snapshot) == 1);
    CHECK(snapshot.path_type == EQ_RESPONSE_PATH_IDENTITY_HARD_BYPASS);
}

static void test_transition_and_immutability(void)
{
    EQ_STATE equalizer;
    EQ_STATE before;
    EQ_RESPONSE_SNAPSHOT active;
    EQ_RESPONSE_SNAPSHOT pending;
    EQ_RESPONSE_COMPLEX complex_response;
    float magnitude_db;

    Equalizer_Init(&equalizer);
    Equalizer_ApplyPreset(&equalizer, EQ_PRESET_BASS_BOOST);
    CHECK(EqualizerResponse_CopyActive(&equalizer, &active) == 1);
    CHECK(active.path_type == EQ_RESPONSE_PATH_BANK_TO_BANK_TRANSITION);
    CHECK(active.role == EQ_RESPONSE_ROLE_ACTIVE);
    CHECK(active.transition_active == 1);
    CHECK(EqualizerResponse_CopyPending(&equalizer, &pending) == 1);
    CHECK(pending.role == EQ_RESPONSE_ROLE_PENDING);
    CHECK(pending.preset == EQ_PRESET_BASS_BOOST);
    before = equalizer;
    CHECK(EqualizerResponse_GetSectionComplex(
        &pending, 0, 1000.0f, &complex_response) == 1);
    CHECK(EqualizerResponse_GetMagnitudeDb(
        &pending, 1000.0f, 1, &magnitude_db) == 1);
    CHECK(isfinite(magnitude_db));
    CHECK(memcmp(&before, &equalizer, sizeof(equalizer)) == 0);

    settle(&equalizer);
    CHECK(EqualizerResponse_CopyActive(&equalizer, &active) == 1);
    CHECK(active.path_type == EQ_RESPONSE_PATH_ACTIVE_BANK);
    CHECK(active.preset == EQ_PRESET_BASS_BOOST);

    Equalizer_SetBypass(&equalizer, 1);
    Equalizer_SetBypass(&equalizer, 0);
    CHECK(EqualizerResponse_CopyActive(&equalizer, &active) == 1);
    CHECK(active.path_type == EQ_RESPONSE_PATH_DRY_TO_BANK_TRANSITION);
    CHECK(active.identity == 1);
    CHECK(EqualizerResponse_CopyPending(&equalizer, &pending) == 1);
    CHECK(pending.path_type == EQ_RESPONSE_PATH_DRY_TO_BANK_TRANSITION);
    CHECK(pending.identity == 0);
}

static void test_command_and_prepared_snapshots(void)
{
    static const float custom[EQ_NUM_BANDS] =
    {
        -3.0f, -1.0f, 1.0f, 3.0f, 4.0f,
         2.0f,  0.0f, -2.0f, 1.0f, 3.0f
    };
    EQ_STATE equalizer;
    EQ_CONTROL_STATE control;
    EQ_CONTROL_MAILBOX mailbox;
    EQ_CONTROL_REQUEST request;
    EQ_COMMAND_SNAPSHOT command;
    EQ_RESPONSE_SNAPSHOT response;
    float desired;
    int band;

    Equalizer_Init(&equalizer);
    EqualizerControl_Init(&control, &equalizer);
    memset(&mailbox, 0, sizeof(mailbox));
    memset(&request, 0, sizeof(request));
    request.command = EQ_CONTROL_SET_ALL;
    request.preset = EQ_PRESET_CUSTOM;
    for (band = 0; band < EQ_NUM_BANDS; band++)
    {
        request.shadow_gain_db[band] = custom[band];
    }
    CHECK(EqualizerControl_SubmitRequest(&mailbox, &request) == 2U);
    CHECK(EqualizerControl_ServiceMailbox(
        &control, &mailbox, &equalizer) == EQ_CONTROL_ACCEPTED);
    CHECK(EqualizerResponse_CopyCommand(&control, &command) == 1);
    CHECK(command.target_response_valid == 0);
    CHECK(EqualizerResponse_GetDesiredVisualDb(
        &command, 31.25f, &desired) == 1);
    CHECK(fabsf(desired - custom[0]) < 1.0e-6f);
    CHECK(EqualizerResponse_GetDesiredVisualDb(
        &command, 16000.0f, &desired) == 1);
    CHECK(fabsf(desired - custom[9]) < 1.0e-6f);
    CHECK(EqualizerResponse_CopyTarget(
        &equalizer, &control, &response) == 0);
    CHECK(response.role == EQ_RESPONSE_ROLE_LOGICAL_TARGET);
    CHECK(response.valid == 0);

    while (EqualizerControl_BuilderEligible(&control, &equalizer) != 0)
    {
        CHECK(EqualizerControl_ServiceOneBuilderSlice(&control) ==
              EQ_BUILDER_WORKED);
    }
    CHECK(EqualizerResponse_CopyPrepared(
        &control.ready_candidate, &response) == 1);
    CHECK(response.role == EQ_RESPONSE_ROLE_PREPARED_TARGET);
    CHECK(EqualizerResponse_CopyCommand(&control, &command) == 1);
    CHECK(command.target_response_valid == 1);
    CHECK(EqualizerResponse_CopyTarget(
        &equalizer, &control, &response) == 1);
    CHECK(response.role == EQ_RESPONSE_ROLE_PREPARED_TARGET);
}

int main(void)
{
    test_identity_paths();
    test_transition_and_immutability();
    test_command_and_prepared_snapshots();
    printf("equalizer_response_test failures=%d\n", failures);
    return (failures == 0) ? 0 : 1;
}
