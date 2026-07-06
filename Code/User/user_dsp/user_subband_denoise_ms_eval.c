/**
 * user_subband_denoise_ms_eval.c
 *
 * PC algo-only evaluation for the minimum-statistics noise tracker.
 */

#include "user_subband_denoise_ms_eval.h"
#include "user_subband_denoise.h"
#include "user_subband_wola.h"

#ifdef SUBBAND_ALGO_ONLY

#include <math.h>
#include <stdio.h>
#include <string.h>

#define MS_EVAL_FRAMES 488
#define MS_EVAL_SAMPLES (MS_EVAL_FRAMES * SUBBAND_FRAME_LEN)
#define MS_EVAL_SKIP_TAIL (2 * SUBBAND_NFFT)
#define MS_EVAL_DELAY SUBBAND_EXPECTED_DELAY
#define MS_EVAL_REPORT "subband_denoise_ms_eval_report.csv"

typedef struct
{
    const char *name;
    int kind;
    float target_snr_db;
    int pass;
    double input_snr_db;
    double fixed_output_snr_db;
    double ms_output_snr_db;
    double fixed_snr_improvement_db;
    double ms_snr_improvement_db;
    double ms_vs_fixed_delta_db;
    double fixed_speech_preservation_ratio;
    double ms_speech_preservation_ratio;
    double fixed_clean_output_corr;
    double ms_clean_output_corr;
    float fixed_gain_avg;
    float ms_gain_avg;
    float fixed_noise_psd_avg;
    float ms_noise_psd_avg;
    float ms_min_psd_avg;
    unsigned long ms_update_count;
} MsEvalRow;

static short MsEval_Noisy[MS_EVAL_SAMPLES];
static short MsEval_Clean[MS_EVAL_SAMPLES];
static short MsEval_FixedOut[MS_EVAL_SAMPLES];
static short MsEval_MsOut[MS_EVAL_SAMPLES];
static float MsEval_NoiseUnit[MS_EVAL_SAMPLES];

static short MsEval_Saturate(float v)
{
    if (v > 32767.0f)
    {
        return 32767;
    }
    if (v < -32768.0f)
    {
        return -32768;
    }
    return (short)v;
}

static float MsEval_Noise_Next(unsigned long *seed)
{
    *seed = (1103515245UL * (*seed) + 12345UL) & 0x7fffffffUL;
    return ((float)(*seed) / 1073741824.0f) - 1.0f;
}

static int MsEval_Sample_From_Seconds(float seconds)
{
    return (int)(seconds * SUBBAND_SAMPLE_RATE + 0.5f);
}

static void MsEval_Fill_Noise_Unit(unsigned long seed)
{
    int i;
    unsigned long state;

    state = seed;
    for (i = 0; i < MS_EVAL_SAMPLES; i++)
    {
        float a;
        float b;
        float c;

        a = MsEval_Noise_Next(&state);
        b = MsEval_Noise_Next(&state);
        c = MsEval_Noise_Next(&state);
        MsEval_NoiseUnit[i] = 0.50f * a + 0.35f * b + 0.15f * c;
    }
}

static void MsEval_Fill_Clean_Speechlike(void)
{
    int i;
    int learn_samples;

    learn_samples = MsEval_Sample_From_Seconds(2.0f);
    for (i = 0; i < MS_EVAL_SAMPLES; i++)
    {
        float t;
        float env;
        float s;

        if (i < learn_samples)
        {
            MsEval_Clean[i] = 0;
        }
        else
        {
            t = (float)i / SUBBAND_SAMPLE_RATE;
            env = 0.72f +
                  0.18f * sinf(2.0f * 3.14159265358979323846f * 2.3f * t) +
                  0.10f * sinf(2.0f * 3.14159265358979323846f * 4.7f * t);
            s = 0.55f * sinf(2.0f * 3.14159265358979323846f * 270.0f * t) +
                0.30f * sinf(2.0f * 3.14159265358979323846f * 730.0f * t) +
                0.18f * sinf(2.0f * 3.14159265358979323846f * 1540.0f * t) +
                0.08f * sinf(2.0f * 3.14159265358979323846f * 3180.0f * t);
            MsEval_Clean[i] = MsEval_Saturate(5200.0f * env * s);
        }
    }
}

static double MsEval_Clean_Energy(int start, int end)
{
    int i;
    double energy;

    energy = 0.0;
    for (i = start; i < end; i++)
    {
        double v;

        v = (double)MsEval_Clean[i];
        energy += v * v;
    }
    return energy;
}

static double MsEval_Noise_Energy(int start, int end)
{
    int i;
    double energy;

    energy = 0.0;
    for (i = start; i < end; i++)
    {
        double v;

        v = (double)MsEval_NoiseUnit[i];
        energy += v * v;
    }
    return energy;
}

static float MsEval_Noise_Scale_For_Snr(float snr_db,
                                        int start, int end)
{
    double sig_energy;
    double noise_energy;
    double target_linear;
    double scale;

    sig_energy = MsEval_Clean_Energy(start, end);
    noise_energy = MsEval_Noise_Energy(start, end);
    target_linear = pow(10.0, (double)snr_db / 10.0);
    scale = sqrt(sig_energy / ((noise_energy + 1.0e-20) * target_linear));
    return (float)scale;
}

static void MsEval_Make_Stationary(float snr_db)
{
    int i;
    int learn_samples;
    int eval_start;
    int eval_end;
    float scale;

    memset(MsEval_Noisy, 0, sizeof(MsEval_Noisy));
    MsEval_Fill_Clean_Speechlike();
    MsEval_Fill_Noise_Unit(301UL + (unsigned long)(snr_db * 17.0f));

    learn_samples = MsEval_Sample_From_Seconds(2.0f);
    eval_start = MsEval_Sample_From_Seconds(3.0f);
    eval_end = MS_EVAL_SAMPLES - MS_EVAL_SKIP_TAIL - MS_EVAL_DELAY;
    scale = MsEval_Noise_Scale_For_Snr(snr_db, eval_start, eval_end);

    for (i = 0; i < MS_EVAL_SAMPLES; i++)
    {
        float noise;

        noise = scale * MsEval_NoiseUnit[i];
        if (i < learn_samples)
        {
            MsEval_Noisy[i] = MsEval_Saturate(noise);
        }
        else
        {
            MsEval_Noisy[i] =
                MsEval_Saturate((float)MsEval_Clean[i] + noise);
        }
    }
}

static void MsEval_Make_Noise_Step(float snr_db)
{
    int i;
    int learn_samples;
    int step_samples;
    int eval_start;
    int eval_end;
    float scale;
    float high_scale;
    float prev_noise;

    memset(MsEval_Noisy, 0, sizeof(MsEval_Noisy));
    MsEval_Fill_Clean_Speechlike();
    MsEval_Fill_Noise_Unit(911UL);

    learn_samples = MsEval_Sample_From_Seconds(2.0f);
    step_samples = MsEval_Sample_From_Seconds(5.0f);
    eval_start = MsEval_Sample_From_Seconds(2.5f);
    eval_end = step_samples;
    scale = MsEval_Noise_Scale_For_Snr(snr_db, eval_start, eval_end);
    high_scale = scale * 1.99526231f;
    prev_noise = 0.0f;

    for (i = 0; i < MS_EVAL_SAMPLES; i++)
    {
        float noise_scale;
        float noise;
        float unit_noise;

        if (i >= step_samples)
        {
            noise_scale = high_scale;
            unit_noise = 0.78f * (MsEval_NoiseUnit[i] - 0.85f * prev_noise);
        }
        else
        {
            noise_scale = scale;
            unit_noise = MsEval_NoiseUnit[i];
        }
        prev_noise = MsEval_NoiseUnit[i];
        noise = noise_scale * unit_noise;

        if (i < learn_samples)
        {
            MsEval_Noisy[i] = MsEval_Saturate(noise);
        }
        else
        {
            MsEval_Noisy[i] =
                MsEval_Saturate((float)MsEval_Clean[i] + noise);
        }
    }
}

static void MsEval_Make_Noise_Only_Ramp(void)
{
    int i;
    float total;

    memset(MsEval_Clean, 0, sizeof(MsEval_Clean));
    memset(MsEval_Noisy, 0, sizeof(MsEval_Noisy));
    MsEval_Fill_Noise_Unit(707UL);

    total = (float)MS_EVAL_SAMPLES;
    for (i = 0; i < MS_EVAL_SAMPLES; i++)
    {
        float ratio;
        float scale;

        ratio = (float)i / total;
        scale = 900.0f + 6400.0f * ratio;
        MsEval_Noisy[i] = MsEval_Saturate(scale * MsEval_NoiseUnit[i]);
    }
}

static void MsEval_Process_Mode(int mode, short *out,
                                float *gain_avg, float *noise_psd_avg,
                                float *min_psd_avg,
                                unsigned long *update_count)
{
    int frame;

    memset(out, 0, (size_t)MS_EVAL_SAMPLES * sizeof(short));
    SubbandWOLA_Init();
    SubbandWOLA_ResetStream();
    SubbandWOLA_ResetAllGains();
    SubbandWOLA_SetBypass(0);
    SubbandDenoise_Reset();
    SubbandDenoise_SetNoiseTrackMode(mode);
    SubbandDenoise_StartNoiseLearning();

    for (frame = 0; frame < MS_EVAL_FRAMES; frame++)
    {
        SubbandWOLA_ProcessFrame(&MsEval_Noisy[frame * SUBBAND_FRAME_LEN],
                                 &out[frame * SUBBAND_FRAME_LEN]);
    }

    *gain_avg = SubbandDenoise_GetGainAvg();
    *noise_psd_avg = SubbandDenoise_GetNoisePsdAvg();
    *min_psd_avg = SUBBAND_DENOISE_DebugMsMinPsdAvg;
    *update_count = SUBBAND_DENOISE_DebugMsUpdateCount;
}

static double MsEval_Input_Snr(int start, int end)
{
    int i;
    double sig;
    double noise;

    sig = 0.0;
    noise = 0.0;
    for (i = start; i < end; i++)
    {
        double c;
        double e;

        c = (double)MsEval_Clean[i];
        e = (double)MsEval_Noisy[i] - c;
        sig += c * c;
        noise += e * e;
    }
    return 10.0 * log10((sig + 1.0e-20) / (noise + 1.0e-20));
}

static double MsEval_Output_Snr(const short *out, int start, int end)
{
    int i;
    double sig;
    double err;

    sig = 0.0;
    err = 0.0;
    for (i = start; i < end; i++)
    {
        double c;
        double y;
        double e;

        c = (double)MsEval_Clean[i];
        y = (double)out[i + MS_EVAL_DELAY];
        e = y - c;
        sig += c * c;
        err += e * e;
    }
    return 10.0 * log10((sig + 1.0e-20) / (err + 1.0e-20));
}

static void MsEval_Preservation_And_Corr(const short *out, int start, int end,
                                         double *preservation,
                                         double *corr)
{
    int i;
    double xx;
    double yy;
    double xy;

    xx = 0.0;
    yy = 0.0;
    xy = 0.0;
    for (i = start; i < end; i++)
    {
        double c;
        double y;

        c = (double)MsEval_Clean[i];
        y = (double)out[i + MS_EVAL_DELAY];
        xx += c * c;
        yy += y * y;
        xy += c * y;
    }

    if (xx > 1.0e-20)
    {
        *preservation = xy / xx;
        if (*preservation < 0.0)
        {
            *preservation = 0.0;
        }
    }
    else
    {
        *preservation = 1.0;
    }

    if ((xx > 1.0e-20) && (yy > 1.0e-20))
    {
        *corr = xy / sqrt(xx * yy);
    }
    else
    {
        *corr = 1.0;
    }
}

static int MsEval_Is_Valid(double x)
{
    if (x != x)
    {
        return 0;
    }
    if ((x > 1.0e30) || (x < -1.0e30))
    {
        return 0;
    }
    return 1;
}

static int MsEval_Compute_Pass(const MsEvalRow *row)
{
    if ((MsEval_Is_Valid(row->fixed_output_snr_db) == 0) ||
        (MsEval_Is_Valid(row->ms_output_snr_db) == 0) ||
        (MsEval_Is_Valid(row->fixed_clean_output_corr) == 0) ||
        (MsEval_Is_Valid(row->ms_clean_output_corr) == 0))
    {
        return 0;
    }

    if (row->kind == 4)
    {
        if ((row->ms_noise_psd_avg <= row->fixed_noise_psd_avg * 1.20f) ||
            (row->ms_update_count == 0UL))
        {
            return 0;
        }
        return 1;
    }

    if ((row->fixed_speech_preservation_ratio < 0.75) ||
        (row->ms_speech_preservation_ratio < 0.75) ||
        (row->fixed_clean_output_corr < 0.95) ||
        (row->ms_clean_output_corr < 0.95))
    {
        return 0;
    }

    if (row->kind == 3)
    {
        if (row->ms_vs_fixed_delta_db < 1.5)
        {
            return 0;
        }
    }
    else
    {
        if (row->ms_vs_fixed_delta_db < -0.5)
        {
            return 0;
        }
    }

    return 1;
}

static void MsEval_Run_Case(MsEvalRow *row)
{
    int start;
    int end;
    float min_psd_fixed_unused;
    unsigned long update_fixed_unused;

    if (row->kind == 0)
    {
        MsEval_Make_Stationary(row->target_snr_db);
        start = MsEval_Sample_From_Seconds(3.0f);
    }
    else if (row->kind == 3)
    {
        MsEval_Make_Noise_Step(row->target_snr_db);
        start = MsEval_Sample_From_Seconds(5.0f);
    }
    else
    {
        MsEval_Make_Noise_Only_Ramp();
        start = MsEval_Sample_From_Seconds(3.0f);
    }
    end = MS_EVAL_SAMPLES - MS_EVAL_SKIP_TAIL - MS_EVAL_DELAY;

    MsEval_Process_Mode(SUBBAND_DENOISE_TRACK_FIXED, MsEval_FixedOut,
                        &row->fixed_gain_avg,
                        &row->fixed_noise_psd_avg,
                        &min_psd_fixed_unused,
                        &update_fixed_unused);
    MsEval_Process_Mode(SUBBAND_DENOISE_TRACK_HYBRID, MsEval_MsOut,
                        &row->ms_gain_avg,
                        &row->ms_noise_psd_avg,
                        &row->ms_min_psd_avg,
                        &row->ms_update_count);

    if (row->kind == 4)
    {
        row->input_snr_db = 0.0;
        row->fixed_output_snr_db = 0.0;
        row->ms_output_snr_db = 0.0;
        row->fixed_snr_improvement_db = 0.0;
        row->ms_snr_improvement_db = 0.0;
        row->ms_vs_fixed_delta_db = 0.0;
        row->fixed_speech_preservation_ratio = 1.0;
        row->ms_speech_preservation_ratio = 1.0;
        row->fixed_clean_output_corr = 1.0;
        row->ms_clean_output_corr = 1.0;
    }
    else
    {
        row->input_snr_db = MsEval_Input_Snr(start, end);
        row->fixed_output_snr_db = MsEval_Output_Snr(MsEval_FixedOut,
                                                     start, end);
        row->ms_output_snr_db = MsEval_Output_Snr(MsEval_MsOut,
                                                  start, end);
        row->fixed_snr_improvement_db =
            row->fixed_output_snr_db - row->input_snr_db;
        row->ms_snr_improvement_db =
            row->ms_output_snr_db - row->input_snr_db;
        row->ms_vs_fixed_delta_db =
            row->ms_output_snr_db - row->fixed_output_snr_db;
        MsEval_Preservation_And_Corr(MsEval_FixedOut, start, end,
                                     &row->fixed_speech_preservation_ratio,
                                     &row->fixed_clean_output_corr);
        MsEval_Preservation_And_Corr(MsEval_MsOut, start, end,
                                     &row->ms_speech_preservation_ratio,
                                     &row->ms_clean_output_corr);
    }

    row->pass = MsEval_Compute_Pass(row);
}

static void MsEval_Write_Csv(FILE *fp, const MsEvalRow *row)
{
    fprintf(fp,
            "%s,%d,%.3f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,"
            "%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%lu,%s\n",
            row->name,
            SUBBAND_DENOISE_TRACK_HYBRID,
            row->input_snr_db,
            row->fixed_output_snr_db,
            row->ms_output_snr_db,
            row->fixed_snr_improvement_db,
            row->ms_snr_improvement_db,
            row->ms_vs_fixed_delta_db,
            row->fixed_speech_preservation_ratio,
            row->ms_speech_preservation_ratio,
            row->fixed_clean_output_corr,
            row->ms_clean_output_corr,
            row->fixed_gain_avg,
            row->ms_gain_avg,
            row->fixed_noise_psd_avg,
            row->ms_noise_psd_avg,
            row->ms_min_psd_avg,
            row->ms_update_count,
            row->pass ? "PASS" : "FAIL");
}

int SubbandDenoiseMsEval_OfflineTest_All(void)
{
    MsEvalRow rows[5];
    int i;
    int failures;
    FILE *fp;

    memset(rows, 0, sizeof(rows));
    rows[0].name = "stationary_snr5";
    rows[0].kind = 0;
    rows[0].target_snr_db = 5.0f;
    rows[1].name = "stationary_snr10";
    rows[1].kind = 0;
    rows[1].target_snr_db = 10.0f;
    rows[2].name = "stationary_snr20";
    rows[2].kind = 0;
    rows[2].target_snr_db = 20.0f;
    rows[3].name = "noise_step_up_snr10";
    rows[3].kind = 3;
    rows[3].target_snr_db = 10.0f;
    rows[4].name = "noise_only_ramp";
    rows[4].kind = 4;
    rows[4].target_snr_db = 0.0f;

    failures = 0;
    fp = fopen(MS_EVAL_REPORT, "w");
    if (fp == 0)
    {
        printf("denoise_ms_eval FAIL open_report=%s\n", MS_EVAL_REPORT);
        return 1;
    }

    fprintf(fp,
            "eval_case,track_mode,input_snr_db,fixed_output_snr_db,"
            "ms_output_snr_db,fixed_snr_improvement_db,"
            "ms_snr_improvement_db,ms_vs_fixed_delta_db,"
            "fixed_speech_preservation_ratio,ms_speech_preservation_ratio,"
            "fixed_clean_output_corr,ms_clean_output_corr,fixed_gain_avg,"
            "ms_gain_avg,fixed_noise_psd_avg,ms_noise_psd_avg,"
            "ms_min_psd_avg,ms_update_count,pass\n");

    for (i = 0; i < 5; i++)
    {
        MsEval_Run_Case(&rows[i]);
        MsEval_Write_Csv(fp, &rows[i]);
        printf("denoise_ms_eval %-22s input_snr=%.3f fixed_out=%.3f "
               "ms_out=%.3f delta=%.3f fixed_pres=%.3f ms_pres=%.3f "
               "fixed_corr=%.3f ms_corr=%.3f fixed_noise=%.3f "
               "ms_noise=%.3f ms_min=%.3f updates=%lu %s\n",
               rows[i].name,
               rows[i].input_snr_db,
               rows[i].fixed_output_snr_db,
               rows[i].ms_output_snr_db,
               rows[i].ms_vs_fixed_delta_db,
               rows[i].fixed_speech_preservation_ratio,
               rows[i].ms_speech_preservation_ratio,
               rows[i].fixed_clean_output_corr,
               rows[i].ms_clean_output_corr,
               rows[i].fixed_noise_psd_avg,
               rows[i].ms_noise_psd_avg,
               rows[i].ms_min_psd_avg,
               rows[i].ms_update_count,
               rows[i].pass ? "PASS" : "FAIL");
        if (rows[i].pass == 0)
        {
            failures++;
        }
    }

    fclose(fp);
    printf("denoise_ms_eval report=%s failures=%d\n",
           MS_EVAL_REPORT, failures);
    return failures;
}

#else

int SubbandDenoiseMsEval_OfflineTest_All(void)
{
    return 0;
}

#endif /* SUBBAND_ALGO_ONLY */
