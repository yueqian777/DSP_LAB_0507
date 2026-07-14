/**
 * user_equalizer_control.c
 *
 * Single-writer seqlock mailbox and one-payload-per-call RBJ builder.
 */

#include "user_equalizer_control.h"
#include "float.h"
#include "string.h"

#ifdef EQ_ALGO_ONLY
static EQ_CONTROL_TEST_HOOK EQ_ControlReadHook = 0;

void EqualizerControl_SetReadHook(EQ_CONTROL_TEST_HOOK hook)
{
    EQ_ControlReadHook = hook;
}
#endif

static int EQ_ControlIsFinite(float value)
{
    return ((value == value) && (value <= FLT_MAX) &&
            (value >= -FLT_MAX));
}

static float EQ_ControlClampDb(float value)
{
    if (value < EQ_GAIN_MIN_DB)
    {
        return EQ_GAIN_MIN_DB;
    }
    if (value > EQ_GAIN_MAX_DB)
    {
        return EQ_GAIN_MAX_DB;
    }
    return value;
}

static int EQ_ControlGainsMatch(const float *a, const float *b)
{
    int band;

    for (band = 0; band < EQ_NUM_BANDS; band++)
    {
        float difference;

        difference = a[band] - b[band];
        if ((difference > 1.0e-7f) || (difference < -1.0e-7f))
        {
            return 0;
        }
    }
    return 1;
}

static int EQ_ControlGainsAreFlat(const float *gains_db)
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

static int EQ_ControlBuilderHasUninstalledWork(
    const EQ_CONTROL_STATE *control)
{
    return ((control->builder.state == EQ_BUILDER_DESIGN_SECTION) ||
            (control->builder.state == EQ_BUILDER_SCAN_HEADROOM) ||
            (control->builder.state == EQ_BUILDER_FINALIZE) ||
            (control->builder.state == EQ_BUILDER_READY) ||
            (control->ready_candidate.valid != 0)) ? 1 : 0;
}

static void EQ_ControlCancelUninstalled(EQ_CONTROL_STATE *control)
{
    if (EQ_ControlBuilderHasUninstalledWork(control) != 0)
    {
        control->builder.cancel_count++;
        control->builder.state = EQ_BUILDER_CANCELLED;
    }
    control->ready_candidate.valid = 0;
}

static void EQ_ControlStartBuilder(EQ_CONTROL_STATE *control)
{
    unsigned long payload_count;
    unsigned long cancel_count;
    unsigned long restart_count;
    int band;
    int had_work;

    had_work = EQ_ControlBuilderHasUninstalledWork(control);
    payload_count = control->builder.payload_slice_count;
    cancel_count = control->builder.cancel_count;
    restart_count = control->builder.restart_count;
    if (had_work != 0)
    {
        cancel_count++;
        restart_count++;
    }
    memset(&control->builder, 0, sizeof(control->builder));
    control->builder.payload_slice_count = payload_count;
    control->builder.cancel_count = cancel_count;
    control->builder.restart_count = restart_count;
    control->builder.state = EQ_BUILDER_DESIGN_SECTION;
    control->builder.generation = control->target_generation;
    control->builder.request_sequence = control->target_sequence;
    control->builder.peak_db = -120.0f;
    for (band = 0; band < EQ_NUM_BANDS; band++)
    {
        control->builder.gains_db[band] = control->target_gain_db[band];
    }
    control->ready_candidate.valid = 0;
}

static void EQ_ControlRecordCoalesced(EQ_CONTROL_STATE *control,
                                      EQ_CONTROL_SEQUENCE sequence)
{
    EQ_CONTROL_SEQUENCE delta;

    if ((control->observed_sequence == 0U) ||
        (control->observed_sequence == sequence))
    {
        return;
    }
    delta = sequence - control->observed_sequence;
    if (((delta & 1U) == 0U) && (delta > 2U))
    {
        control->coalesced_count += (unsigned long)(delta / 2U - 1U);
    }
}

void EqualizerControl_Init(EQ_CONTROL_STATE *control,
                           const EQ_STATE *equalizer)
{
    int band;

    if (control == 0)
    {
        return;
    }
    memset(control, 0, sizeof(*control));
    control->builder.state = EQ_BUILDER_IDLE;
    control->target_preset = EQ_PRESET_NONE;
    control->target_bank_id = EQ_RBJ_BANK_CUSTOM;
    control->next_install_transition_kind = EQ_TRANSITION_BANK_TO_BANK;
    if (equalizer == 0)
    {
        return;
    }
    for (band = 0; band < EQ_NUM_BANDS; band++)
    {
        control->shadow_gain_db[band] = equalizer->active_gain_db[band];
        control->target_gain_db[band] = equalizer->active_gain_db[band];
    }
}

EQ_CONTROL_SEQUENCE EqualizerControl_SubmitRequest(
    EQ_CONTROL_MAILBOX *mailbox,
    const EQ_CONTROL_REQUEST *request)
{
    EQ_CONTROL_SEQUENCE current;
    EQ_CONTROL_SEQUENCE odd_sequence;
    EQ_CONTROL_SEQUENCE even_sequence;
    int band;

    if ((mailbox == 0) || (request == 0))
    {
        return 0U;
    }
    current = mailbox->sequence;
    if ((current & 1U) != 0U)
    {
        return 0U;
    }
    if (current >= 0xFFFFFFFEU)
    {
        odd_sequence = 1U;
        even_sequence = 2U;
    }
    else
    {
        odd_sequence = current + 1U;
        even_sequence = current + 2U;
    }
    mailbox->sequence = odd_sequence;
    mailbox->command = request->command;
    mailbox->band = request->band;
    mailbox->preset = request->preset;
    mailbox->value_db = request->value_db;
    mailbox->step_db = request->step_db;
    for (band = 0; band < EQ_NUM_BANDS; band++)
    {
        mailbox->shadow_gain_db[band] = request->shadow_gain_db[band];
    }
    mailbox->sequence = even_sequence;
    return even_sequence;
}

static int EQ_ControlValidateAndBuildShadow(
    const EQ_CONTROL_STATE *control,
    const EQ_CONTROL_REQUEST *request,
    float gains_out[EQ_NUM_BANDS],
    int *preset_out,
    int *target_bearing,
    int *error_out)
{
    int band;
    float step;

    for (band = 0; band < EQ_NUM_BANDS; band++)
    {
        gains_out[band] = control->shadow_gain_db[band];
    }
    *preset_out = EQ_PRESET_CUSTOM;
    *target_bearing = 0;
    *error_out = EQ_CONTROL_ERROR_NONE;
    switch (request->command)
    {
        case EQ_CONTROL_NONE:
        case EQ_CONTROL_COPY_ACTIVE_TO_SHADOW:
        case EQ_CONTROL_CANCEL_PENDING:
            return 1;
        case EQ_CONTROL_APPLY_PRESET:
            if ((request->preset < EQ_PRESET_FLAT) ||
                (request->preset >= EQ_PRESET_COUNT))
            {
                *error_out = EQ_CONTROL_ERROR_PRESET;
                return 0;
            }
            if (Equalizer_CopyPresetGainsDb(request->preset, gains_out) == 0)
            {
                *error_out = EQ_CONTROL_ERROR_PRESET;
                return 0;
            }
            *preset_out = request->preset;
            *target_bearing = 1;
            return 1;
        case EQ_CONTROL_SET_BAND_ABS:
            if ((request->band < 0) || (request->band >= EQ_NUM_BANDS))
            {
                *error_out = EQ_CONTROL_ERROR_BAND;
                return 0;
            }
            if (EQ_ControlIsFinite(request->value_db) == 0)
            {
                *error_out = EQ_CONTROL_ERROR_NONFINITE;
                return 0;
            }
            gains_out[request->band] =
                EQ_ControlClampDb(request->value_db);
            *target_bearing = 1;
            return 1;
        case EQ_CONTROL_STEP_BAND:
            if ((request->band < 0) || (request->band >= EQ_NUM_BANDS))
            {
                *error_out = EQ_CONTROL_ERROR_BAND;
                return 0;
            }
            step = request->step_db;
            if (EQ_ControlIsFinite(step) == 0)
            {
                *error_out = EQ_CONTROL_ERROR_NONFINITE;
                return 0;
            }
            if ((step < 1.0e-7f) && (step > -1.0e-7f))
            {
                step = 0.5f;
            }
            gains_out[request->band] = EQ_ControlClampDb(
                gains_out[request->band] + step);
            *target_bearing = 1;
            return 1;
        case EQ_CONTROL_SET_ALL:
            for (band = 0; band < EQ_NUM_BANDS; band++)
            {
                if (EQ_ControlIsFinite(request->shadow_gain_db[band]) == 0)
                {
                    *error_out = EQ_CONTROL_ERROR_NONFINITE;
                    return 0;
                }
            }
            for (band = 0; band < EQ_NUM_BANDS; band++)
            {
                gains_out[band] =
                    EQ_ControlClampDb(request->shadow_gain_db[band]);
            }
            *target_bearing = 1;
            return 1;
        case EQ_CONTROL_RESET_FLAT:
            for (band = 0; band < EQ_NUM_BANDS; band++)
            {
                gains_out[band] = 0.0f;
            }
            *preset_out = EQ_PRESET_FLAT;
            *target_bearing = 1;
            return 1;
        default:
            *error_out = EQ_CONTROL_ERROR_COMMAND;
            return 0;
    }
}

static int EQ_ControlPublishTarget(EQ_CONTROL_STATE *control,
                                   EQ_STATE *equalizer,
                                   const EQ_CONTROL_REQUEST *request,
                                   const float gains_db[EQ_NUM_BANDS],
                                   int preset)
{
    unsigned long generation;
    EQ_PREPARED_BANK prepared;
    int bank_id;
    int band;

    memset(&prepared, 0, sizeof(prepared));
    if (Equalizer_PreviewLogicalTarget(equalizer, gains_db, preset,
                                       &generation, &bank_id) == 0)
    {
        control->last_error = EQ_CONTROL_ERROR_CORE;
        return 0;
    }
    if ((bank_id != EQ_RBJ_BANK_CUSTOM) &&
        (Equalizer_CopyCachedPreparedBank(
            bank_id, preset, generation, request->sequence,
            &prepared) == 0))
    {
        control->last_error = EQ_CONTROL_ERROR_CORE;
        return 0;
    }

    Equalizer_CommitLogicalTarget(
        equalizer, gains_db, preset, generation, bank_id);
    control->target_sequence = request->sequence;
    control->target_generation = generation;
    control->target_preset = preset;
    control->target_bank_id = bank_id;
    control->target_valid = 1;
    for (band = 0; band < EQ_NUM_BANDS; band++)
    {
        control->shadow_gain_db[band] = gains_db[band];
        control->target_gain_db[band] = gains_db[band];
    }
    if (bank_id != EQ_RBJ_BANK_CUSTOM)
    {
        EQ_ControlCancelUninstalled(control);
        control->ready_candidate = prepared;
        control->prepared_sequence = request->sequence;
        control->builder.state = EQ_BUILDER_IDLE;
    }
    else
    {
        EQ_ControlStartBuilder(control);
    }
    return 1;
}

int EqualizerControl_ServiceMailbox(EQ_CONTROL_STATE *control,
                                    const EQ_CONTROL_MAILBOX *mailbox,
                                    EQ_STATE *equalizer)
{
    EQ_CONTROL_SEQUENCE seq_before;
    EQ_CONTROL_SEQUENCE seq_after;
    EQ_CONTROL_REQUEST request;
    float gains[EQ_NUM_BANDS];
    int preset;
    int target_bearing;
    int error;
    int band;

    if ((control == 0) || (mailbox == 0) || (equalizer == 0))
    {
        return EQ_CONTROL_REJECTED;
    }
    seq_before = mailbox->sequence;
    if ((seq_before == 0U) || ((seq_before & 1U) != 0U))
    {
        return EQ_CONTROL_DEFERRED;
    }
    request.command = mailbox->command;
    request.band = mailbox->band;
    request.preset = mailbox->preset;
    request.value_db = mailbox->value_db;
    request.step_db = mailbox->step_db;
    for (band = 0; band < EQ_NUM_BANDS; band++)
    {
        request.shadow_gain_db[band] = mailbox->shadow_gain_db[band];
    }
#ifdef EQ_ALGO_ONLY
    if (EQ_ControlReadHook != 0)
    {
        EQ_ControlReadHook((EQ_CONTROL_MAILBOX *)mailbox);
    }
#endif
    seq_after = mailbox->sequence;
    if ((seq_before != seq_after) || (seq_after == 0U) ||
        ((seq_after & 1U) != 0U))
    {
        return EQ_CONTROL_DEFERRED;
    }
    if (seq_after == control->observed_sequence)
    {
        return EQ_CONTROL_DUPLICATE;
    }
    request.sequence = seq_after;
    EQ_ControlRecordCoalesced(control, seq_after);
    if (EQ_ControlValidateAndBuildShadow(control, &request, gains, &preset,
                                         &target_bearing, &error) == 0)
    {
        control->observed_sequence = seq_after;
        control->rejected_count++;
        control->last_error = error;
        return EQ_CONTROL_REJECTED;
    }
    control->observed_sequence = seq_after;
    control->last_error = EQ_CONTROL_ERROR_NONE;
    if (request.command == EQ_CONTROL_COPY_ACTIVE_TO_SHADOW)
    {
        for (band = 0; band < EQ_NUM_BANDS; band++)
        {
            control->shadow_gain_db[band] = equalizer->active_gain_db[band];
        }
        control->accepted_sequence = seq_after;
        return EQ_CONTROL_ACCEPTED;
    }
    if (request.command == EQ_CONTROL_CANCEL_PENDING)
    {
        if ((control->target_valid != 0) &&
            ((control->installed_pair_valid == 0) ||
             (control->installed_sequence != control->target_sequence)))
        {
            control->target_valid = 0;
            EQ_ControlCancelUninstalled(control);
        }
        control->accepted_sequence = seq_after;
        return EQ_CONTROL_ACCEPTED;
    }
    if (target_bearing == 0)
    {
        control->accepted_sequence = seq_after;
        return EQ_CONTROL_ACCEPTED;
    }
    if (EQ_ControlPublishTarget(control, equalizer, &request,
                                gains, preset) == 0)
    {
        control->rejected_count++;
        return EQ_CONTROL_REJECTED;
    }
    control->accepted_sequence = seq_after;
    return EQ_CONTROL_ACCEPTED;
}

void EqualizerControl_InvalidateStaleWork(EQ_CONTROL_STATE *control)
{
    int stale_builder;
    int stale_ready;

    if (control == 0)
    {
        return;
    }
    stale_builder =
        ((control->builder.state == EQ_BUILDER_DESIGN_SECTION) ||
         (control->builder.state == EQ_BUILDER_SCAN_HEADROOM) ||
         (control->builder.state == EQ_BUILDER_FINALIZE)) &&
        ((control->target_valid == 0) ||
         (control->builder.generation != control->target_generation) ||
         (control->builder.request_sequence != control->target_sequence));
    stale_ready = (control->ready_candidate.valid != 0) &&
        ((control->target_valid == 0) ||
         (control->ready_candidate.generation != control->target_generation) ||
         (control->ready_candidate.request_sequence !=
          control->target_sequence));
    if ((stale_builder != 0) || (stale_ready != 0))
    {
        EQ_ControlCancelUninstalled(control);
    }
}

int EqualizerControl_BuilderEligible(const EQ_CONTROL_STATE *control,
                                     const EQ_STATE *equalizer)
{
    if ((control == 0) || (equalizer == 0) ||
        (Equalizer_HasPendingTransition(equalizer) != 0) ||
        (control->target_valid == 0))
    {
        return 0;
    }
    return ((control->builder.state == EQ_BUILDER_DESIGN_SECTION) ||
            (control->builder.state == EQ_BUILDER_SCAN_HEADROOM) ||
            (control->builder.state == EQ_BUILDER_FINALIZE)) ? 1 : 0;
}

int EqualizerControl_ServiceOneBuilderSlice(EQ_CONTROL_STATE *control)
{
    EQ_BANK_BUILDER *builder;
    int points;
    int index;
    float magnitude_db;
    int band;

    if (control == 0)
    {
        return EQ_BUILDER_FAILED;
    }
    builder = &control->builder;
    if ((control->target_valid == 0) ||
        (builder->generation != control->target_generation) ||
        (builder->request_sequence != control->target_sequence))
    {
        EqualizerControl_InvalidateStaleWork(control);
        return EQ_BUILDER_DEFERRED;
    }
    switch (builder->state)
    {
        case EQ_BUILDER_DESIGN_SECTION:
            if (Equalizer_DesignRbjSection(
                    &builder->candidate.section[builder->section_index],
                    builder->section_index,
                    builder->gains_db[builder->section_index]) == 0)
            {
                builder->state = EQ_BUILDER_ERROR;
                builder->last_error = EQ_CONTROL_ERROR_BUILDER;
                return EQ_BUILDER_FAILED;
            }
            builder->section_index++;
            builder->payload_slice_count++;
            if (builder->section_index >= EQ_NUM_BANDS)
            {
                builder->state = EQ_BUILDER_SCAN_HEADROOM;
                builder->scan_index = 0;
                builder->peak_db = -120.0f;
            }
            return EQ_BUILDER_WORKED;
        case EQ_BUILDER_SCAN_HEADROOM:
            points = EQ_HEADROOM_POINT_COUNT - builder->scan_index;
            if (points > 16)
            {
                points = 16;
            }
            for (index = 0; index < points; index++)
            {
                if (Equalizer_EvaluateHeadroomPointDb(
                        &builder->candidate,
                        builder->scan_index + index,
                        &magnitude_db) == 0)
                {
                    builder->state = EQ_BUILDER_ERROR;
                    builder->last_error = EQ_CONTROL_ERROR_BUILDER;
                    return EQ_BUILDER_FAILED;
                }
                if (magnitude_db > builder->peak_db)
                {
                    builder->peak_db = magnitude_db;
                }
            }
            builder->scan_index += points;
            builder->payload_slice_count++;
            if (builder->scan_index >= EQ_HEADROOM_POINT_COUNT)
            {
                EQ_DebugHeadroomScanCount++;
                builder->state = EQ_BUILDER_FINALIZE;
            }
            return EQ_BUILDER_WORKED;
        case EQ_BUILDER_FINALIZE:
            Equalizer_FinalizeRbjBank(
                &builder->candidate,
                EQ_ControlGainsAreFlat(builder->gains_db),
                builder->peak_db);
            memset(&control->ready_candidate, 0,
                   sizeof(control->ready_candidate));
            control->ready_candidate.bank = builder->candidate;
            for (band = 0; band < EQ_NUM_BANDS; band++)
            {
                control->ready_candidate.gains_db[band] =
                    builder->gains_db[band];
            }
            control->ready_candidate.bank_id = control->target_bank_id;
            control->ready_candidate.preset = control->target_preset;
            control->ready_candidate.generation = builder->generation;
            control->ready_candidate.request_sequence =
                builder->request_sequence;
            control->ready_candidate.valid = 1;
            control->prepared_sequence = builder->request_sequence;
            builder->payload_slice_count++;
            builder->state = EQ_BUILDER_READY;
            return EQ_BUILDER_WORKED;
        default:
            return EQ_BUILDER_NO_WORK;
    }
}

int EqualizerControl_TryInstallReady(EQ_CONTROL_STATE *control,
                                     EQ_STATE *equalizer)
{
    int result;

    if ((control == 0) || (equalizer == 0) ||
        (control->ready_candidate.valid == 0))
    {
        return EQ_INSTALL_INVALID;
    }
    if (Equalizer_HasPendingTransition(equalizer) != 0)
    {
        return EQ_INSTALL_BUSY;
    }
    if ((control->target_valid == 0) ||
        (control->ready_candidate.generation != control->target_generation) ||
        (control->ready_candidate.request_sequence !=
         control->target_sequence) ||
        (control->ready_candidate.preset != control->target_preset) ||
        (control->ready_candidate.bank_id != control->target_bank_id) ||
        (EQ_ControlGainsMatch(control->ready_candidate.gains_db,
                              control->target_gain_db) == 0))
    {
        control->ready_candidate.valid = 0;
        control->last_error = EQ_CONTROL_ERROR_STALE;
        return EQ_INSTALL_STALE;
    }
    result = Equalizer_InstallPreparedBank(
        equalizer, &control->ready_candidate,
        control->next_install_transition_kind);
    if (result == EQ_INSTALL_INSTALLED)
    {
        control->installed_sequence = control->target_sequence;
        control->installed_generation = control->target_generation;
        control->installed_pair_valid = 1;
        control->ready_candidate.valid = 0;
        control->builder.state = EQ_BUILDER_IDLE;
        control->return_from_identity_pending = 0;
        control->next_install_transition_kind =
            EQ_TRANSITION_BANK_TO_BANK;
    }
    else if (result == EQ_INSTALL_STALE)
    {
        control->ready_candidate.valid = 0;
        control->last_error = EQ_CONTROL_ERROR_STALE;
    }
    return result;
}

void EqualizerControl_ObserveFrameBoundary(EQ_CONTROL_STATE *control,
                                           const EQ_STATE *equalizer)
{
    if ((control == 0) || (equalizer == 0) ||
        (control->installed_pair_valid == 0) ||
        (Equalizer_HasPendingTransition(equalizer) != 0))
    {
        return;
    }
    if (Equalizer_GetActiveGeneration(equalizer) ==
        control->installed_generation)
    {
        control->applied_sequence = control->installed_sequence;
    }
}

void EqualizerControl_RebaseAfterDirectPathChange(
    EQ_CONTROL_STATE *control,
    const EQ_STATE *equalizer)
{
    int band;

    if ((control == 0) || (equalizer == 0))
    {
        return;
    }
    EQ_ControlCancelUninstalled(control);
    control->target_valid = 0;
    control->installed_pair_valid = 0;
    control->installed_generation = 0UL;
    control->builder.state = EQ_BUILDER_IDLE;
    control->return_from_identity_pending =
        ((Equalizer_GetCoreMode(equalizer) == EQ_CORE_RAW_COPY) ||
         (Equalizer_GetCoreMode(equalizer) == EQ_CORE_FLOAT_COPY) ||
         (Equalizer_GetBypass(equalizer) != 0)) ? 1 : 0;
    control->next_install_transition_kind =
        (control->return_from_identity_pending != 0) ?
        EQ_TRANSITION_DRY_TO_BANK : EQ_TRANSITION_BANK_TO_BANK;
    for (band = 0; band < EQ_NUM_BANDS; band++)
    {
        control->shadow_gain_db[band] = equalizer->active_gain_db[band];
    }
}
