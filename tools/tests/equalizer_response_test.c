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

#define TEST_PI 3.14159265358979323846

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

static float reference_process_bank(EQ_FILTER_BANK *bank, float input)
{
    float value;
    int section;

    value = input * bank->preamp_gain;
    for (section = 0; section < EQ_NUM_BANDS; section++)
    {
        const EQ_BIQUAD *coeff = &bank->section[section];
        float output = coeff->b0 * value + bank->s1[section];

        bank->s1[section] = coeff->b1 * value - coeff->a1 * output +
                            bank->s2[section];
        bank->s2[section] = coeff->b2 * value - coeff->a2 * output;
        value = output;
    }
    return value;
}

static float reference_process_transition_sample(EQ_STATE *state,
                                                 float input)
{
    float active;

    if (state->pending_bank_valid == 0)
    {
        return reference_process_bank(&state->active_bank, input);
    }
    active = reference_process_bank(&state->active_bank, input);
    {
        float pending = reference_process_bank(&state->pending_bank, input);
        float mix = 1.0f - (float)state->transition_remaining /
                              (float)state->transition_total;

        active += (pending - active) * mix;
    }
    state->transition_remaining--;
    if (state->transition_remaining <= 0)
    {
        state->active_bank = state->pending_bank;
        state->active_bank_id = state->pending_bank_id;
        state->active_preset = state->pending_preset;
        state->active_generation = state->pending_generation;
        memcpy(state->active_gain_db, state->pending_gain_db,
               sizeof(state->active_gain_db));
        memset(&state->pending_bank, 0, sizeof(state->pending_bank));
        state->pending_bank_valid = 0;
        state->pending_bank_id = EQ_RBJ_BANK_CUSTOM;
        state->pending_preset = EQ_PRESET_NONE;
        state->pending_generation = 0UL;
        memset(state->pending_gain_db, 0, sizeof(state->pending_gain_db));
        state->transition_remaining = 0;
        state->transition_total = 0;
        state->transition_kind = EQ_TRANSITION_NONE;
    }
    return active;
}

static void check_bank_pair_transition(int active_preset, int target_preset)
{
    EQ_STATE actual;
    EQ_STATE reference;
    float input[EQ_FRAME_LEN];
    float actual_output[EQ_FRAME_LEN];
    float expected_output[EQ_FRAME_LEN];
    int frame;
    int sample;
    int section;

    Equalizer_Init(&actual);
    if (active_preset != EQ_PRESET_FLAT)
    {
        Equalizer_ApplyPreset(&actual, active_preset);
        settle(&actual);
    }
    Equalizer_ApplyPreset(&actual, target_preset);
    CHECK(actual.pending_bank_valid == 1);
    CHECK(actual.transition_kind == EQ_TRANSITION_BANK_TO_BANK);
    reference = actual;
    frame = 0;
    while ((actual.pending_bank_valid != 0) && (frame < 8))
    {
        for (sample = 0; sample < EQ_FRAME_LEN; sample++)
        {
            float position = (float)(frame * EQ_FRAME_LEN + sample);

            input[sample] = 0.45f * sinf(position * 0.071f) +
                            0.10f * cosf(position * 0.193f);
            expected_output[sample] = reference_process_transition_sample(
                &reference, input[sample]);
        }
        Equalizer_ProcessFrameFloat(&actual, input, actual_output,
                                    EQ_FRAME_LEN);
        for (sample = 0; sample < EQ_FRAME_LEN; sample++)
        {
            CHECK(fabsf(actual_output[sample] - expected_output[sample]) <
                  1.0e-6f);
        }
        frame++;
    }
    CHECK(actual.pending_bank_valid == 0);
    CHECK(reference.pending_bank_valid == 0);
    CHECK(actual.transition_remaining == reference.transition_remaining);
    for (section = 0; section < EQ_NUM_BANDS; section++)
    {
        CHECK(fabsf(actual.active_bank.s1[section] -
                    reference.active_bank.s1[section]) < 1.0e-6f);
        CHECK(fabsf(actual.active_bank.s2[section] -
                    reference.active_bank.s2[section]) < 1.0e-6f);
    }
}

static void test_bank_pair_transition_matches_reference(void)
{
    check_bank_pair_transition(EQ_PRESET_FLAT, EQ_PRESET_BASS_BOOST);
    check_bank_pair_transition(EQ_PRESET_BASS_BOOST, EQ_PRESET_VOCAL);
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

typedef struct
{
    float frequency_hz;
    float shape_db;
    float total_db;
    double phase_rad;
    double unwrapped_phase_rad;
    double group_delay_samples;
    int response_valid;
    int valid;
} C_RESPONSE_POINT;

static int export_response_grid(FILE *file, const char *preset,
                                const EQ_RESPONSE_SNAPSHOT *snapshot)
{
    C_RESPONSE_POINT points[EQ_HEADROOM_POINT_COUNT];
    float log_min;
    float log_max;
    int previous_valid;
    double previous_unwrapped;
    int point;

    log_min = logf(20.0f);
    log_max = logf(20000.0f);
    previous_valid = 0;
    previous_unwrapped = 0.0;
    for (point = 0; point < EQ_HEADROOM_POINT_COUNT; point++)
    {
        EQ_RESPONSE_COMPLEX response;
        double phase;

        memset(&points[point], 0, sizeof(points[point]));
        points[point].shape_db = (float)NAN;
        points[point].total_db = (float)NAN;
        points[point].phase_rad = NAN;
        points[point].unwrapped_phase_rad = NAN;
        points[point].group_delay_samples = NAN;
        points[point].frequency_hz = expf(log_min +
            (log_max - log_min) * (float)point /
            (float)(EQ_HEADROOM_POINT_COUNT - 1));
        if ((EqualizerResponse_GetCascadeComplex(
                 snapshot, points[point].frequency_hz, 0, &response) == 0) ||
            (EqualizerResponse_GetMagnitudeDb(
                 snapshot, points[point].frequency_hz, 0,
                 &points[point].shape_db) == 0) ||
            (EqualizerResponse_GetMagnitudeDb(
                 snapshot, points[point].frequency_hz, 1,
                 &points[point].total_db) == 0))
        {
            previous_valid = 0;
            continue;
        }
        phase = atan2(response.imag, response.real);
        if (isfinite(phase) == 0)
        {
            previous_valid = 0;
            continue;
        }
        points[point].phase_rad = phase;
        if (previous_valid != 0)
        {
            while ((phase - previous_unwrapped) > TEST_PI)
            {
                phase -= 2.0 * TEST_PI;
            }
            while ((phase - previous_unwrapped) < -TEST_PI)
            {
                phase += 2.0 * TEST_PI;
            }
        }
        points[point].unwrapped_phase_rad = phase;
        points[point].response_valid = 1;
        previous_unwrapped = phase;
        previous_valid = 1;
    }
    for (point = 1; point < (EQ_HEADROOM_POINT_COUNT - 1); point++)
    {
        double omega_delta;

        if ((points[point - 1].response_valid == 0) ||
            (points[point].response_valid == 0) ||
            (points[point + 1].response_valid == 0))
        {
            continue;
        }
        omega_delta = 2.0 * TEST_PI *
            ((double)points[point + 1].frequency_hz -
             (double)points[point - 1].frequency_hz) /
            (double)EQ_SAMPLE_RATE;
        if ((omega_delta == 0.0) || (isfinite(omega_delta) == 0))
        {
            continue;
        }
        points[point].group_delay_samples =
            -(points[point + 1].unwrapped_phase_rad -
              points[point - 1].unwrapped_phase_rad) / omega_delta;
        if (isfinite(points[point].group_delay_samples) != 0)
        {
            points[point].valid = 1;
        }
    }
    for (point = 0; point < EQ_HEADROOM_POINT_COUNT; point++)
    {
        fprintf(file,
            "%s,%d,%.9g,%.9f,%.9f,%.17g,%.17g,%d,%.9g,%.9g,%.9g\n",
            preset, point, points[point].frequency_hz,
            points[point].shape_db, points[point].total_db,
            points[point].phase_rad, points[point].group_delay_samples,
            points[point].valid, snapshot->predicted_peak_db,
            snapshot->preamp_db, snapshot->preamp_gain);
    }
    return 1;
}

static int export_c_reference(const char *directory)
{
    EQ_STATE equalizer;
    EQ_STATE synchronous;
    EQ_CONTROL_STATE control;
    EQ_CONTROL_MAILBOX mailbox;
    EQ_CONTROL_REQUEST request;
    EQ_RESPONSE_SNAPSHOT flat_snapshot;
    EQ_RESPONSE_SNAPSHOT custom_snapshot;
    FILE *response_file;
    FILE *section_file;
    char response_path[512];
    char section_path[512];
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
    if (EqualizerResponse_CopyActive(&equalizer, &flat_snapshot) == 0)
    {
        fclose(response_file);
        fclose(section_file);
        return 0;
    }
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
            &control.ready_candidate, &custom_snapshot) == 0)
    {
        fclose(response_file);
        fclose(section_file);
        return 0;
    }
    Equalizer_SetAllGainsDb(&synchronous, report_custom);

    fprintf(response_file,
        "preset,point,frequency_hz,shape_db,total_db,phase_rad,"
        "group_delay_samples,valid,predicted_peak_db,preamp_db,preamp_gain\n");
    if ((export_response_grid(response_file, "flat", &flat_snapshot) == 0) ||
        (export_response_grid(response_file, "custom", &custom_snapshot) == 0))
    {
        fclose(response_file);
        fclose(section_file);
        return 0;
    }

    fprintf(section_file,
        "section,builder_b0,builder_b1,builder_b2,builder_a1,builder_a2,"
        "sync_b0,sync_b1,sync_b2,sync_a1,sync_a2,"
        "builder_peak_db,sync_peak_db,builder_preamp_db,sync_preamp_db\n");
    for (band = 0; band < EQ_NUM_BANDS; band++)
    {
        const EQ_BIQUAD *builder_section = &custom_snapshot.section[band];
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
            custom_snapshot.predicted_peak_db,
            synchronous.pending_bank.predicted_peak_db,
            custom_snapshot.preamp_db, synchronous.pending_bank.preamp_db);
    }
    fclose(response_file);
    fclose(section_file);
    return 1;
}

int main(int argc, char **argv)
{
    test_identity_paths();
    test_transition_and_immutability();
    test_bank_pair_transition_matches_reference();
    test_command_and_prepared_snapshots();
    if ((argc == 3) && (strcmp(argv[1], "--export") == 0))
    {
        CHECK(export_c_reference(argv[2]) != 0);
    }
    printf("equalizer_response_test failures=%d\n", failures);
    return (failures == 0) ? 0 : 1;
}
