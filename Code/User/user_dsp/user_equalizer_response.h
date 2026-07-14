/**
 * user_equalizer_response.h
 *
 * Immutable path-aware response snapshots for Project 3.3.
 */

#ifndef _USER_EQUALIZER_RESPONSE_H_
#define _USER_EQUALIZER_RESPONSE_H_

#include "user_equalizer_control.h"

#define EQ_RESPONSE_PATH_IDENTITY_RAW_COPY 0
#define EQ_RESPONSE_PATH_IDENTITY_FLOAT_COPY 1
#define EQ_RESPONSE_PATH_IDENTITY_HARD_BYPASS 2
#define EQ_RESPONSE_PATH_IDENTITY_RBJ_FLAT_COPY 3
#define EQ_RESPONSE_PATH_DRY_TO_BANK_TRANSITION 4
#define EQ_RESPONSE_PATH_ACTIVE_BANK 5
#define EQ_RESPONSE_PATH_BANK_TO_BANK_TRANSITION 6
#define EQ_RESPONSE_PATH_UNSUPPORTED_LEGACY 7
#define EQ_RESPONSE_PATH_IDENTITY_RETURN_HOLD 8

#define EQ_RESPONSE_ROLE_ACTIVE 0
#define EQ_RESPONSE_ROLE_PENDING 1
#define EQ_RESPONSE_ROLE_PREPARED_TARGET 2
#define EQ_RESPONSE_ROLE_LOGICAL_TARGET 3

typedef struct
{
    EQ_BIQUAD section[EQ_NUM_BANDS];
    float gain_db[EQ_NUM_BANDS];
    float predicted_peak_db;
    float preamp_db;
    float preamp_gain;
    int path_type;
    int role;
    int bank_id;
    int preset;
    unsigned long generation;
    int valid;
    int identity;
    int transition_active;
    float transition_progress;
} EQ_RESPONSE_SNAPSHOT;

typedef struct
{
    float gain_db[EQ_NUM_BANDS];
    EQ_CONTROL_SEQUENCE request_sequence;
    unsigned long generation;
    int preset;
    int bank_id;
    int valid;
    int target_response_valid;
} EQ_COMMAND_SNAPSHOT;

int EqualizerResponse_CopyActive(const EQ_STATE *st,
                                 EQ_RESPONSE_SNAPSHOT *out);
int EqualizerResponse_CopyPending(const EQ_STATE *st,
                                  EQ_RESPONSE_SNAPSHOT *out);
int EqualizerResponse_CopyPrepared(const EQ_PREPARED_BANK *prepared,
                                   EQ_RESPONSE_SNAPSHOT *out);
int EqualizerResponse_CopyCommand(const EQ_CONTROL_STATE *control,
                                  EQ_COMMAND_SNAPSHOT *out);
int EqualizerResponse_CopyTarget(const EQ_STATE *st,
                                 const EQ_CONTROL_STATE *control,
                                 EQ_RESPONSE_SNAPSHOT *out);
#ifdef EQ_ALGO_ONLY
typedef struct
{
    double real;
    double imag;
    int valid;
} EQ_RESPONSE_COMPLEX;

int EqualizerResponse_GetSectionComplex(const EQ_RESPONSE_SNAPSHOT *snapshot,
                                        int section,
                                        float frequency_hz,
                                        EQ_RESPONSE_COMPLEX *out);
int EqualizerResponse_GetCascadeComplex(const EQ_RESPONSE_SNAPSHOT *snapshot,
                                        float frequency_hz,
                                        int include_preamp,
                                        EQ_RESPONSE_COMPLEX *out);
int EqualizerResponse_GetMagnitudeDb(const EQ_RESPONSE_SNAPSHOT *snapshot,
                                     float frequency_hz,
                                     int include_preamp,
                                     float *magnitude_db);
int EqualizerResponse_GetDesiredVisualDb(
    const EQ_COMMAND_SNAPSHOT *command,
    float frequency_hz,
    float *desired_db);
#endif

#endif /* _USER_EQUALIZER_RESPONSE_H_ */
