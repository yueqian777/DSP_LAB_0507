/**
 * user_equalizer_response.c
 *
 * Copies immutable coefficient endpoints and evaluates them without touching
 * runtime filter or transition state.
 */

#include "user_equalizer_response.h"
#include "math.h"
#include "string.h"

#define EQ_RESPONSE_EPS 1.0e-20

static const float EQ_ResponseCentersHz[EQ_NUM_BANDS] =
{
    31.25f, 62.5f, 125.0f, 250.0f, 500.0f,
    1000.0f, 2000.0f, 4000.0f, 8000.0f, 16000.0f
};

static int EQ_ResponseGainsAreFlat(const float gains_db[EQ_NUM_BANDS])
{
    int band;

    for (band = 0; band < EQ_NUM_BANDS; band++)
    {
        if ((gains_db[band] > 1.0e-7f) ||
            (gains_db[band] < -1.0e-7f))
        {
            return 0;
        }
    }
    return 1;
}

static void EQ_ResponseSetIdentitySection(EQ_BIQUAD *section)
{
    section->b0 = 1.0f;
    section->b1 = 0.0f;
    section->b2 = 0.0f;
    section->a1 = 0.0f;
    section->a2 = 0.0f;
}

static float EQ_ResponseTransitionProgress(const EQ_STATE *st)
{
    float progress;

    if ((st->pending_bank_valid == 0) || (st->transition_total <= 0))
    {
        return 0.0f;
    }
    progress = 1.0f - (float)st->transition_remaining /
                        (float)st->transition_total;
    if (progress < 0.0f)
    {
        progress = 0.0f;
    }
    if (progress > 1.0f)
    {
        progress = 1.0f;
    }
    return progress;
}

static void EQ_ResponseCopyBank(const EQ_FILTER_BANK *bank,
                                const float gains_db[EQ_NUM_BANDS],
                                EQ_RESPONSE_SNAPSHOT *out)
{
    int section;

    for (section = 0; section < EQ_NUM_BANDS; section++)
    {
        out->section[section] = bank->section[section];
        out->gain_db[section] = gains_db[section];
    }
    out->predicted_peak_db = bank->predicted_peak_db;
    out->preamp_db = bank->preamp_db;
    out->preamp_gain = bank->preamp_gain;
    out->valid = 1;
    out->identity = 0;
}

static void EQ_ResponseMakeIdentity(EQ_RESPONSE_SNAPSHOT *out,
                                    int path_type,
                                    int role)
{
    int section;

    memset(out, 0, sizeof(*out));
    for (section = 0; section < EQ_NUM_BANDS; section++)
    {
        EQ_ResponseSetIdentitySection(&out->section[section]);
    }
    out->preamp_gain = 1.0f;
    out->path_type = path_type;
    out->role = role;
    out->bank_id = EQ_RBJ_BANK_CUSTOM;
    out->preset = EQ_PRESET_NONE;
    out->valid = 1;
    out->identity = 1;
}

int EqualizerResponse_CopyActive(const EQ_STATE *st,
                                 EQ_RESPONSE_SNAPSHOT *out)
{
    if ((st == 0) || (out == 0))
    {
        return 0;
    }
    if (st->bypass != 0)
    {
        EQ_ResponseMakeIdentity(out,
            EQ_RESPONSE_PATH_IDENTITY_HARD_BYPASS,
            EQ_RESPONSE_ROLE_ACTIVE);
        return 1;
    }
    if (st->core_mode == EQ_CORE_RAW_COPY)
    {
        EQ_ResponseMakeIdentity(out,
            EQ_RESPONSE_PATH_IDENTITY_RAW_COPY,
            EQ_RESPONSE_ROLE_ACTIVE);
        return 1;
    }
    if (st->core_mode == EQ_CORE_FLOAT_COPY)
    {
        EQ_ResponseMakeIdentity(out,
            EQ_RESPONSE_PATH_IDENTITY_FLOAT_COPY,
            EQ_RESPONSE_ROLE_ACTIVE);
        return 1;
    }
    if (st->core_mode == EQ_CORE_LEGACY)
    {
        memset(out, 0, sizeof(*out));
        out->path_type = EQ_RESPONSE_PATH_UNSUPPORTED_LEGACY;
        out->role = EQ_RESPONSE_ROLE_ACTIVE;
        out->valid = 0;
        return 0;
    }
    if ((st->pending_bank_valid != 0) &&
        (st->transition_kind == EQ_TRANSITION_DRY_TO_BANK))
    {
        EQ_ResponseMakeIdentity(out,
            EQ_RESPONSE_PATH_DRY_TO_BANK_TRANSITION,
            EQ_RESPONSE_ROLE_ACTIVE);
        out->transition_active = 1;
        out->transition_progress = EQ_ResponseTransitionProgress(st);
        return 1;
    }
    if ((st->pending_bank_valid == 0) &&
        (EQ_ResponseGainsAreFlat(st->active_gain_db) != 0))
    {
        EQ_ResponseMakeIdentity(out,
            EQ_RESPONSE_PATH_IDENTITY_RBJ_FLAT_COPY,
            EQ_RESPONSE_ROLE_ACTIVE);
        out->bank_id = st->active_bank_id;
        out->preset = st->active_preset;
        out->generation = st->active_generation;
        return 1;
    }
    memset(out, 0, sizeof(*out));
    EQ_ResponseCopyBank(&st->active_bank, st->active_gain_db, out);
    out->path_type = (st->pending_bank_valid != 0) ?
        EQ_RESPONSE_PATH_BANK_TO_BANK_TRANSITION :
        EQ_RESPONSE_PATH_ACTIVE_BANK;
    out->role = EQ_RESPONSE_ROLE_ACTIVE;
    out->bank_id = st->active_bank_id;
    out->preset = st->active_preset;
    out->generation = st->active_generation;
    out->transition_active = st->pending_bank_valid;
    out->transition_progress = EQ_ResponseTransitionProgress(st);
    return 1;
}

int EqualizerResponse_CopyPending(const EQ_STATE *st,
                                  EQ_RESPONSE_SNAPSHOT *out)
{
    if ((st == 0) || (out == 0) || (st->pending_bank_valid == 0) ||
        (st->core_mode != EQ_CORE_RBJ_CASCADE))
    {
        if (out != 0)
        {
            memset(out, 0, sizeof(*out));
        }
        return 0;
    }
    memset(out, 0, sizeof(*out));
    EQ_ResponseCopyBank(&st->pending_bank, st->pending_gain_db, out);
    out->path_type =
        (st->transition_kind == EQ_TRANSITION_DRY_TO_BANK) ?
        EQ_RESPONSE_PATH_DRY_TO_BANK_TRANSITION :
        EQ_RESPONSE_PATH_BANK_TO_BANK_TRANSITION;
    out->role = EQ_RESPONSE_ROLE_PENDING;
    out->bank_id = st->pending_bank_id;
    out->preset = st->pending_preset;
    out->generation = st->pending_generation;
    out->transition_active = 1;
    out->transition_progress = EQ_ResponseTransitionProgress(st);
    return 1;
}

int EqualizerResponse_CopyPrepared(const EQ_PREPARED_BANK *prepared,
                                   EQ_RESPONSE_SNAPSHOT *out)
{
    if ((prepared == 0) || (out == 0) || (prepared->valid == 0))
    {
        if (out != 0)
        {
            memset(out, 0, sizeof(*out));
        }
        return 0;
    }
    memset(out, 0, sizeof(*out));
    EQ_ResponseCopyBank(&prepared->bank, prepared->gains_db, out);
    out->path_type = EQ_RESPONSE_PATH_ACTIVE_BANK;
    out->role = EQ_RESPONSE_ROLE_PREPARED_TARGET;
    out->bank_id = prepared->bank_id;
    out->preset = prepared->preset;
    out->generation = prepared->generation;
    return 1;
}

int EqualizerResponse_CopyCommand(const EQ_CONTROL_STATE *control,
                                  EQ_COMMAND_SNAPSHOT *out)
{
    int band;

    if ((control == 0) || (out == 0))
    {
        return 0;
    }
    memset(out, 0, sizeof(*out));
    for (band = 0; band < EQ_NUM_BANDS; band++)
    {
        out->gain_db[band] = control->target_gain_db[band];
    }
    out->request_sequence = control->target_sequence;
    out->generation = control->target_generation;
    out->preset = control->target_preset;
    out->bank_id = control->target_bank_id;
    out->valid = control->target_valid;
    out->target_response_valid =
        (((control->ready_candidate.valid != 0) &&
          (control->ready_candidate.request_sequence ==
           control->target_sequence) &&
          (control->ready_candidate.generation ==
           control->target_generation)) ||
         ((control->installed_pair_valid != 0) &&
          (control->installed_sequence == control->target_sequence) &&
          (control->installed_generation ==
           control->target_generation))) ? 1 : 0;
    return (out->valid != 0) ? 1 : 0;
}

int EqualizerResponse_CopyTarget(const EQ_STATE *st,
                                 const EQ_CONTROL_STATE *control,
                                 EQ_RESPONSE_SNAPSHOT *out)
{
    int band;

    if ((st == 0) || (control == 0) || (out == 0) ||
        (control->target_valid == 0))
    {
        if (out != 0)
        {
            memset(out, 0, sizeof(*out));
        }
        return 0;
    }
    if ((st->pending_bank_valid != 0) &&
        (st->pending_generation == control->target_generation))
    {
        return EqualizerResponse_CopyPending(st, out);
    }
    if ((st->pending_bank_valid == 0) &&
        (st->active_generation == control->target_generation))
    {
        return EqualizerResponse_CopyActive(st, out);
    }
    if ((control->ready_candidate.valid != 0) &&
        (control->ready_candidate.generation ==
         control->target_generation) &&
        (control->ready_candidate.request_sequence ==
         control->target_sequence))
    {
        return EqualizerResponse_CopyPrepared(
            &control->ready_candidate, out);
    }
    memset(out, 0, sizeof(*out));
    for (band = 0; band < EQ_NUM_BANDS; band++)
    {
        out->gain_db[band] = control->target_gain_db[band];
    }
    out->path_type = EQ_RESPONSE_PATH_ACTIVE_BANK;
    out->role = EQ_RESPONSE_ROLE_LOGICAL_TARGET;
    out->bank_id = control->target_bank_id;
    out->preset = control->target_preset;
    out->generation = control->target_generation;
    out->valid = 0;
    return 0;
}

int EqualizerResponse_GetSectionComplex(const EQ_RESPONSE_SNAPSHOT *snapshot,
                                        int section,
                                        float frequency_hz,
                                        EQ_RESPONSE_COMPLEX *out)
{
    if ((snapshot == 0) || (out == 0) || (snapshot->valid == 0) ||
        (section < 0) || (section >= EQ_NUM_BANDS) ||
        (frequency_hz <= 0.0f) ||
        (frequency_hz >= EQ_SAMPLE_RATE * 0.5f))
    {
        if (out != 0)
        {
            out->real = 0.0;
            out->imag = 0.0;
            out->valid = 0;
        }
        return 0;
    }
    if (Equalizer_GetBiquadResponseComplex(
            &snapshot->section[section], frequency_hz,
            &out->real, &out->imag) == 0)
    {
        out->real = 0.0;
        out->imag = 0.0;
        out->valid = 0;
        return 0;
    }
    out->valid = 1;
    return 1;
}

int EqualizerResponse_GetCascadeComplex(const EQ_RESPONSE_SNAPSHOT *snapshot,
                                        float frequency_hz,
                                        int include_preamp,
                                        EQ_RESPONSE_COMPLEX *out)
{
    EQ_RESPONSE_COMPLEX section_response;
    double next_real;
    int section;

    if ((snapshot == 0) || (out == 0) || (snapshot->valid == 0))
    {
        return 0;
    }
    out->real = 1.0;
    out->imag = 0.0;
    out->valid = 1;
    for (section = 0; section < EQ_NUM_BANDS; section++)
    {
        if (EqualizerResponse_GetSectionComplex(
                snapshot, section, frequency_hz, &section_response) == 0)
        {
            out->valid = 0;
            return 0;
        }
        next_real = out->real * section_response.real -
                    out->imag * section_response.imag;
        out->imag = out->real * section_response.imag +
                    out->imag * section_response.real;
        out->real = next_real;
    }
    if (include_preamp != 0)
    {
        out->real *= snapshot->preamp_gain;
        out->imag *= snapshot->preamp_gain;
    }
    return 1;
}

int EqualizerResponse_GetMagnitudeDb(const EQ_RESPONSE_SNAPSHOT *snapshot,
                                     float frequency_hz,
                                     int include_preamp,
                                     float *magnitude_db)
{
    EQ_RESPONSE_COMPLEX response;
    double magnitude;

    if ((magnitude_db == 0) ||
        (EqualizerResponse_GetCascadeComplex(
            snapshot, frequency_hz, include_preamp, &response) == 0))
    {
        return 0;
    }
    magnitude = sqrt(response.real * response.real +
                     response.imag * response.imag);
    if (magnitude < EQ_RESPONSE_EPS)
    {
        return 0;
    }
    *magnitude_db = (float)(20.0 * log10(magnitude));
    return 1;
}

int EqualizerResponse_GetDesiredVisualDb(
    const EQ_COMMAND_SNAPSHOT *command,
    float frequency_hz,
    float *desired_db)
{
    double log_frequency;
    double log_left;
    double log_right;
    double mix;
    int band;

    if ((command == 0) || (desired_db == 0) ||
        (command->valid == 0) || (frequency_hz <= 0.0f))
    {
        return 0;
    }
    if (frequency_hz <= EQ_ResponseCentersHz[0])
    {
        *desired_db = command->gain_db[0];
        return 1;
    }
    if (frequency_hz >= EQ_ResponseCentersHz[EQ_NUM_BANDS - 1])
    {
        *desired_db = command->gain_db[EQ_NUM_BANDS - 1];
        return 1;
    }
    log_frequency = log((double)frequency_hz) / log(2.0);
    for (band = 0; band < (EQ_NUM_BANDS - 1); band++)
    {
        if (frequency_hz <= EQ_ResponseCentersHz[band + 1])
        {
            log_left = log((double)EQ_ResponseCentersHz[band]) / log(2.0);
            log_right = log((double)EQ_ResponseCentersHz[band + 1]) /
                        log(2.0);
            mix = (log_frequency - log_left) / (log_right - log_left);
            *desired_db = command->gain_db[band] +
                (float)mix * (command->gain_db[band + 1] -
                              command->gain_db[band]);
            return 1;
        }
    }
    return 0;
}
