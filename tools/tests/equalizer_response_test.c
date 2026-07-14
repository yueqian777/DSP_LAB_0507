#include "user_equalizer_response.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures = 0;
static const float report_custom[EQ_NUM_BANDS] =
{
    -3.0f, -1.0f, 1.0f, 3.0f, 4.0f,
     2.0f,  0.0f, -2.0f, 1.0f, 3.0f
};

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

    Equalizer_SetIdentityHold(&equalizer, 1);
    CHECK(EqualizerResponse_CopyActive(&equalizer, &snapshot) == 1);
    CHECK(snapshot.path_type == EQ_RESPONSE_PATH_IDENTITY_RETURN_HOLD);
    CHECK(snapshot.identity == 1);
    Equalizer_SetIdentityHold(&equalizer, 0);

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

static int export_c_reference(const char *directory)
{
    EQ_STATE equalizer;
    EQ_STATE synchronous;
    EQ_CONTROL_STATE control;
    EQ_CONTROL_MAILBOX mailbox;
    EQ_CONTROL_REQUEST request;
    EQ_RESPONSE_SNAPSHOT snapshot;
    FILE *response_file;
    FILE *section_file;
    char response_path[512];
    char section_path[512];
    float shape_db;
    float total_db;
    float log_min;
    float log_max;
    float frequency_hz;
    int point;
    int band;

    if (directory == 0)
    {
        return 0;
    }
    snprintf(response_path, sizeof(response_path),
             "%s/c_response_512.csv", directory);
    snprintf(section_path, sizeof(section_path),
             "%s/c_sections.csv", directory);
    response_file = fopen(response_path, "w");
    section_file = fopen(section_path, "w");
    if ((response_file == 0) || (section_file == 0))
    {
        if (response_file != 0)
        {
            fclose(response_file);
        }
        if (section_file != 0)
        {
            fclose(section_file);
        }
        return 0;
    }

    Equalizer_Init(&equalizer);
    Equalizer_Init(&synchronous);
    EqualizerControl_Init(&control, &equalizer);
    memset(&mailbox, 0, sizeof(mailbox));
    memset(&request, 0, sizeof(request));
    request.command = EQ_CONTROL_SET_ALL;
    request.preset = EQ_PRESET_CUSTOM;
    for (band = 0; band < EQ_NUM_BANDS; band++)
    {
        request.shadow_gain_db[band] = report_custom[band];
    }
    if ((EqualizerControl_SubmitRequest(&mailbox, &request) == 0U) ||
        (EqualizerControl_ServiceMailbox(
            &control, &mailbox, &equalizer) != EQ_CONTROL_ACCEPTED))
    {
        fclose(response_file);
        fclose(section_file);
        return 0;
    }
    while (EqualizerControl_BuilderEligible(&control, &equalizer) != 0)
    {
        if (EqualizerControl_ServiceOneBuilderSlice(&control) !=
            EQ_BUILDER_WORKED)
        {
            fclose(response_file);
            fclose(section_file);
            return 0;
        }
    }
    if (EqualizerResponse_CopyPrepared(
            &control.ready_candidate, &snapshot) == 0)
    {
        fclose(response_file);
        fclose(section_file);
        return 0;
    }
    Equalizer_SetAllGainsDb(&synchronous, report_custom);

    fprintf(response_file, "point,frequency_hz,shape_db,total_db\n");
    log_min = logf(20.0f);
    log_max = logf(20000.0f);
    for (point = 0; point < EQ_HEADROOM_POINT_COUNT; point++)
    {
        frequency_hz = expf(log_min +
            (log_max - log_min) * (float)point /
            (float)(EQ_HEADROOM_POINT_COUNT - 1));
        if ((EqualizerResponse_GetMagnitudeDb(
                &snapshot, frequency_hz, 0, &shape_db) == 0) ||
            (EqualizerResponse_GetMagnitudeDb(
                &snapshot, frequency_hz, 1, &total_db) == 0))
        {
            fclose(response_file);
            fclose(section_file);
            return 0;
        }
        fprintf(response_file, "%d,%.9g,%.9f,%.9f\n",
                point, frequency_hz, shape_db, total_db);
    }

    fprintf(section_file,
        "section,builder_b0,builder_b1,builder_b2,builder_a1,builder_a2,"
        "sync_b0,sync_b1,sync_b2,sync_a1,sync_a2,"
        "builder_peak_db,sync_peak_db,builder_preamp_db,sync_preamp_db\n");
    for (band = 0; band < EQ_NUM_BANDS; band++)
    {
        const EQ_BIQUAD *builder_section = &snapshot.section[band];
        const EQ_BIQUAD *sync_section =
            &synchronous.pending_bank.section[band];

        fprintf(section_file,
            "%d,%.9g,%.9g,%.9g,%.9g,%.9g,"
            "%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g\n",
            band,
            builder_section->b0, builder_section->b1,
            builder_section->b2, builder_section->a1,
            builder_section->a2,
            sync_section->b0, sync_section->b1, sync_section->b2,
            sync_section->a1, sync_section->a2,
            snapshot.predicted_peak_db,
            synchronous.pending_bank.predicted_peak_db,
            snapshot.preamp_db, synchronous.pending_bank.preamp_db);
    }
    fclose(response_file);
    fclose(section_file);
    return 1;
}

int main(int argc, char **argv)
{
    test_identity_paths();
    test_transition_and_immutability();
    test_command_and_prepared_snapshots();
    if ((argc == 3) && (strcmp(argv[1], "--export") == 0))
    {
        CHECK(export_c_reference(argv[2]) != 0);
    }
    printf("equalizer_response_test failures=%d\n", failures);
    return (failures == 0) ? 0 : 1;
}
