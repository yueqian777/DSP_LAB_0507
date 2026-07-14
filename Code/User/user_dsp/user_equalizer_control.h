/**
 * user_equalizer_control.h
 *
 * Bounded Project 3.3 command mailbox and incremental custom-bank builder.
 */

#ifndef _USER_EQUALIZER_CONTROL_H_
#define _USER_EQUALIZER_CONTROL_H_

#include "user_equalizer.h"

#define EQ_CONTROL_NONE                  0
#define EQ_CONTROL_APPLY_PRESET          1
#define EQ_CONTROL_SET_BAND_ABS          2
#define EQ_CONTROL_STEP_BAND             3
#define EQ_CONTROL_SET_ALL               4
#define EQ_CONTROL_RESET_FLAT            5
#define EQ_CONTROL_COPY_ACTIVE_TO_SHADOW 6
#define EQ_CONTROL_CANCEL_PENDING        7

#define EQ_CONTROL_ACCEPTED 1
#define EQ_CONTROL_NO_REQUEST 0
#define EQ_CONTROL_DEFERRED (-1)
#define EQ_CONTROL_DUPLICATE (-2)
#define EQ_CONTROL_REJECTED (-3)

#define EQ_CONTROL_ERROR_NONE 0
#define EQ_CONTROL_ERROR_COMMAND 1
#define EQ_CONTROL_ERROR_BAND 2
#define EQ_CONTROL_ERROR_PRESET 3
#define EQ_CONTROL_ERROR_NONFINITE 4
#define EQ_CONTROL_ERROR_CORE 5
#define EQ_CONTROL_ERROR_STALE 6
#define EQ_CONTROL_ERROR_BUILDER 7

#define EQ_BUILDER_IDLE 0
#define EQ_BUILDER_DESIGN_SECTION 1
#define EQ_BUILDER_SCAN_HEADROOM 2
#define EQ_BUILDER_FINALIZE 3
#define EQ_BUILDER_READY 4
#define EQ_BUILDER_CANCELLED 5
#define EQ_BUILDER_ERROR 6

#define EQ_BUILDER_NO_WORK 0
#define EQ_BUILDER_WORKED 1
#define EQ_BUILDER_DEFERRED (-1)
#define EQ_BUILDER_FAILED (-2)

typedef struct
{
    volatile EQ_CONTROL_SEQUENCE sequence;
    volatile int command;
    volatile int band;
    volatile int preset;
    volatile float value_db;
    volatile float step_db;
    volatile float shadow_gain_db[EQ_NUM_BANDS];
} EQ_CONTROL_MAILBOX;

typedef struct
{
    EQ_CONTROL_SEQUENCE sequence;
    int command;
    int band;
    int preset;
    float value_db;
    float step_db;
    float shadow_gain_db[EQ_NUM_BANDS];
} EQ_CONTROL_REQUEST;

typedef struct
{
    int state;
    int section_index;
    int scan_index;
    unsigned long generation;
    EQ_CONTROL_SEQUENCE request_sequence;
    float gains_db[EQ_NUM_BANDS];
    EQ_FILTER_BANK candidate;
    float peak_db;
    unsigned long payload_slice_count;
    unsigned long cancel_count;
    unsigned long restart_count;
    int last_error;
} EQ_BANK_BUILDER;

typedef struct
{
    float shadow_gain_db[EQ_NUM_BANDS];
    float target_gain_db[EQ_NUM_BANDS];
    EQ_CONTROL_SEQUENCE observed_sequence;
    EQ_CONTROL_SEQUENCE accepted_sequence;
    EQ_CONTROL_SEQUENCE target_sequence;
    EQ_CONTROL_SEQUENCE prepared_sequence;
    EQ_CONTROL_SEQUENCE installed_sequence;
    EQ_CONTROL_SEQUENCE applied_sequence;
    unsigned long target_generation;
    unsigned long installed_generation;
    int target_preset;
    int target_bank_id;
    int target_valid;
    int installed_pair_valid;
    int return_from_identity_pending;
    int next_install_transition_kind;
    EQ_BANK_BUILDER builder;
    EQ_PREPARED_BANK ready_candidate;
    unsigned long rejected_count;
    unsigned long coalesced_count;
    int last_error;
} EQ_CONTROL_STATE;

void EqualizerControl_Init(EQ_CONTROL_STATE *control,
                           const EQ_STATE *equalizer);
EQ_CONTROL_SEQUENCE EqualizerControl_SubmitRequest(
    EQ_CONTROL_MAILBOX *mailbox,
    const EQ_CONTROL_REQUEST *request);
int EqualizerControl_ServiceMailbox(EQ_CONTROL_STATE *control,
                                    const EQ_CONTROL_MAILBOX *mailbox,
                                    EQ_STATE *equalizer);
void EqualizerControl_InvalidateStaleWork(EQ_CONTROL_STATE *control);
int EqualizerControl_BuilderEligible(const EQ_CONTROL_STATE *control,
                                     const EQ_STATE *equalizer);
int EqualizerControl_ServiceOneBuilderSlice(EQ_CONTROL_STATE *control);
int EqualizerControl_TryInstallReady(EQ_CONTROL_STATE *control,
                                     EQ_STATE *equalizer);
void EqualizerControl_ObserveFrameBoundary(EQ_CONTROL_STATE *control,
                                           const EQ_STATE *equalizer);
void EqualizerControl_RebaseAfterDirectPathChange(
    EQ_CONTROL_STATE *control,
    const EQ_STATE *equalizer);

#ifdef EQ_ALGO_ONLY
typedef void (*EQ_CONTROL_TEST_HOOK)(EQ_CONTROL_MAILBOX *mailbox);
void EqualizerControl_SetReadHook(EQ_CONTROL_TEST_HOOK hook);
#endif

#endif /* _USER_EQUALIZER_CONTROL_H_ */
