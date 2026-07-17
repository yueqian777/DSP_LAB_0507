#include <math.h>
#include <stdio.h>
#include <string.h>

#include "user_dynamic_clarity.h"

#define EVAL_RESPONSE_POINTS 129
#define EVAL_CONTROL_STEPS   82
#define EVAL_EVIDENCE_LABEL  "HOST_SIMULATED_CONTROL"
#define EVAL_PI 3.14159265358979323846

static float ResponseFrequency(int point)
{
    double ratio;

    ratio = (double)point / (double)(EVAL_RESPONSE_POINTS - 1);
    return (float)(20.0 * pow(1000.0, ratio));
}

static int WriteResponseCsv(const char *path,
                            const DYNAMIC_CLARITY_STATE *state)
{
    FILE *file;
    int level;
    int point;

    file = fopen(path, "w");
    if (file == 0)
    {
        return 0;
    }
    fprintf(file, "evidence_label,frequency_hz,level,gain_db,phase_deg\n");
    for (level = 0; level < DYNAMIC_CLARITY_LEVEL_COUNT; level++)
    {
        for (point = 0; point < EVAL_RESPONSE_POINTS; point++)
        {
            double real_value;
            double imag_value;
            double magnitude;
            double gain_db;
            double phase_deg;
            float frequency_hz;

            frequency_hz = ResponseFrequency(point);
            if (!Equalizer_GetBiquadResponseComplex(
                    &state->coefficient_table[level], frequency_hz,
                    &real_value, &imag_value))
            {
                fclose(file);
                return 0;
            }
            magnitude = sqrt(real_value * real_value +
                             imag_value * imag_value);
            if ((!isfinite(magnitude)) || (magnitude <= 0.0))
            {
                fclose(file);
                return 0;
            }
            gain_db = 20.0 * log10(magnitude);
            phase_deg = atan2(imag_value, real_value) * 180.0 / EVAL_PI;
            fprintf(file, "%s,%.8f,%d,%.8f,%.8f\n",
                    EVAL_EVIDENCE_LABEL, frequency_hz, level,
                    gain_db, phase_deg);
        }
    }
    fclose(file);
    return 1;
}

static void ControlStimulus(int index,
                            float *mud_db,
                            float *presence_db,
                            float *rms_dbfs,
                            int *valid,
                            int *warmup)
{
    if (index < 19)
    {
        *mud_db = (float)index;
        *presence_db = 0.0f;
        *rms_dbfs = -18.0f;
        *valid = 1;
        *warmup = 1;
    }
    else if (index < 27)
    {
        *mud_db = 18.0f;
        *presence_db = 0.0f;
        *rms_dbfs = -18.0f;
        *valid = 1;
        *warmup = 1;
    }
    else if (index < 46)
    {
        *mud_db = 18.0f - (float)(index - 27);
        *presence_db = 0.0f;
        *rms_dbfs = -18.0f;
        *valid = 1;
        *warmup = 1;
    }
    else if (index < 54)
    {
        *mud_db = ((index & 1) == 0) ? 3.9f : 5.4f;
        *presence_db = 0.0f;
        *rms_dbfs = -18.0f;
        *valid = 1;
        *warmup = 1;
    }
    else if (index < 60)
    {
        *mud_db = -1.0f;
        *presence_db = -10.0f;
        *rms_dbfs = -18.0f;
        *valid = 1;
        *warmup = 1;
    }
    else if (index < 66)
    {
        *mud_db = 4.0f;
        *presence_db = -13.0f;
        *rms_dbfs = -18.0f;
        *valid = 1;
        *warmup = 1;
    }
    else if (index < 74)
    {
        *mud_db = 0.0f;
        *presence_db = 0.0f;
        *rms_dbfs = -90.0f;
        *valid = 1;
        *warmup = 1;
    }
    else
    {
        *mud_db = 12.0f;
        *presence_db = 0.0f;
        *rms_dbfs = -18.0f;
        *valid = 0;
        *warmup = 1;
    }
}

static int WriteControlCsv(const char *path,
                           DYNAMIC_CLARITY_STATE *state)
{
    AUDIO_FEATURE_SNAPSHOT snapshot;
    short frame[DYNAMIC_CLARITY_MAX_FRAME_LEN];
    FILE *file;
    int analysis;
    int frame_index;

    file = fopen(path, "w");
    if (file == 0)
    {
        return 0;
    }
    memset(frame, 0, sizeof(frame));
    fprintf(file, "evidence_label,analysis_index,mud_db,presence_db,"
                  "masking_db,rms_dbfs,requested_level,applied_level,"
                  "transition_active,reason\n");
    (void)DynamicClarity_SetEnabled(state, 1);
    for (analysis = 0; analysis < EVAL_CONTROL_STEPS; analysis++)
    {
        float mud_db;
        float presence_db;
        float masking_db;
        float rms_dbfs;
        int valid;
        int warmup;

        ControlStimulus(analysis, &mud_db, &presence_db, &rms_dbfs,
                        &valid, &warmup);
        masking_db = mud_db - presence_db;
        memset(&snapshot, 0, sizeof(snapshot));
        snapshot.analysis_count = (unsigned long)(analysis + 1);
        snapshot.relative_db[AUDIO_FEATURE_MUD] = mud_db;
        snapshot.relative_db[AUDIO_FEATURE_PRESENCE] = presence_db;
        snapshot.rms_dbfs = rms_dbfs;
        snapshot.valid = valid;
        snapshot.warmup_complete = warmup;
        if (DynamicClarity_UpdateFromFeature(state, &snapshot) < 0)
        {
            fclose(file);
            return 0;
        }
        fprintf(file, "%s,%d,%.6f,%.6f,%.6f,%.6f,%d,%d,%d,%d\n",
                EVAL_EVIDENCE_LABEL, analysis, mud_db, presence_db,
                masking_db, rms_dbfs, state->target_level,
                state->active_level, state->transition_active,
                DynamicClarity_GetReason(state));

        for (frame_index = 0; frame_index < 8; frame_index++)
        {
            if (DynamicClarity_ProcessFrame(
                    state, frame, frame,
                    DYNAMIC_CLARITY_MAX_FRAME_LEN) < 0)
            {
                fclose(file);
                return 0;
            }
        }
    }
    fclose(file);
    return 1;
}

int main(int argc, char **argv)
{
    DYNAMIC_CLARITY_STATE state;

    if (argc != 3)
    {
        fprintf(stderr,
                "usage: dynamic_clarity_response_eval response.csv control.csv\n");
        return 2;
    }
    DynamicClarity_Init(&state);
    if (state.initialized == 0)
    {
        fprintf(stderr, "DynamicClarity_Init failed\n");
        return 3;
    }
    if (!WriteResponseCsv(argv[1], &state))
    {
        fprintf(stderr, "response CSV failed\n");
        return 4;
    }
    DynamicClarity_Reset(&state);
    if (!WriteControlCsv(argv[2], &state))
    {
        fprintf(stderr, "control CSV failed\n");
        return 5;
    }
    printf("dynamic_clarity_response_rows=%d control_rows=%d "
           "label=HOST_SIMULATED_CONTROL\n",
           DYNAMIC_CLARITY_LEVEL_COUNT * EVAL_RESPONSE_POINTS,
           EVAL_CONTROL_STEPS);
    return 0;
}
