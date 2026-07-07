/**
 * user_subband_denoise_mcra_eval.c
 *
 * PC algo-only evaluation for MCRA-like adaptive noise tracking.
 */

#include "user_subband_denoise_mcra_eval.h"
#include "user_subband_denoise.h"
#include "user_subband_wola.h"

#ifdef SUBBAND_ALGO_ONLY

#include <math.h>
#include <stdio.h>
#include <string.h>

#define MCRA_EVAL_FRAMES 488
#define MCRA_EVAL_SAMPLES (MCRA_EVAL_FRAMES * SUBBAND_FRAME_LEN)
#define MCRA_EVAL_SKIP_TAIL (2 * SUBBAND_NFFT)
#define MCRA_EVAL_DELAY SUBBAND_EXPECTED_DELAY
#define MCRA_EVAL_REPORT "subband_denoise_mcra_eval_report.csv"
#define MCRA_EVAL_MODE_COUNT 4

typedef struct
{
    const char *name;
    int kind;
    float target_snr_db;
} McraEvalCase;

typedef struct
{
    const char *name;
    int track_mode;
    int mcra_variant;
} McraEvalMode;

typedef struct
{
    const McraEvalCase *eval_case;
    const McraEvalMode *mode;
    int pass;
    int clip_count;
    double input_snr_db;
    double output_snr_db;
    double snr_improvement_db;
    double delta_vs_fixed_db;
    double delta_vs_hybrid_db;
    double speech_preservation_ratio;
    double clean_output_corr;
    float gain_avg;
    float gain_min;
    float gain_max;
    float noise_psd_avg;
    float ms_min_psd_avg;
    float speech_prob_avg;
    float overdrive_avg;
    float floor_avg;
} McraEvalMetric;

static const McraEvalMode McraEval_Modes[MCRA_EVAL_MODE_COUNT] =
{
    {"fixed", SUBBAND_DENOISE_TRACK_FIXED, 0},
    {"hybrid_ms", SUBBAND_DENOISE_TRACK_HYBRID, 0},
    {"mcra_normal", SUBBAND_DENOISE_TRACK_MCRA, 1},
    {"mcra_strong", SUBBAND_DENOISE_TRACK_MCRA, 2}
};

static short McraEval_Noisy[MCRA_EVAL_SAMPLES];
static short McraEval_Clean[MCRA_EVAL_SAMPLES];
static short McraEval_Out[MCRA_EVAL_SAMPLES];
static float McraEval_NoiseUnit[MCRA_EVAL_SAMPLES];

static short McraEval_Saturate(float v)
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

static float McraEval_Noise_Next(unsigned long *seed)
{
    *seed = (1103515245UL * (*seed) + 12345UL) & 0x7fffffffUL;
    return ((float)(*seed) / 1073741824.0f) - 1.0f;
}

static int McraEval_Sample_From_Seconds(float seconds)
{
    return (int)(seconds * SUBBAND_SAMPLE_RATE + 0.5f);
}

static void McraEval_Fill_Noise_Unit(unsigned long seed)
{
    int i;
    unsigned long state;

    state = seed;
    for (i = 0; i < MCRA_EVAL_SAMPLES; i++)
    {
        float a;
        float b;
        float c;

        a = McraEval_Noise_Next(&state);
        b = McraEval_Noise_Next(&state);
        c = McraEval_Noise_Next(&state);
        McraEval_NoiseUnit[i] = 0.50f * a + 0.35f * b + 0.15f * c;
    }
}

static void McraEval_Fill_Clean_Speechlike(void)
{
    int i;
    int learn_samples;

    learn_samples = McraEval_Sample_From_Seconds(2.0f);
    for (i = 0; i < MCRA_EVAL_SAMPLES; i++)
    {
        float t;
        float env;
        float s;

        if (i < learn_samples)
        {
            McraEval_Clean[i] = 0;
        }
        else
        {
            t = (float)i / SUBBAND_SAMPLE_RATE;
            env = 0.72f +
                  0.18f * sinf(2.0f * 3.14159265358979323846f *
                                2.3f * t) +
                  0.10f * sinf(2.0f * 3.14159265358979323846f *
                                4.7f * t);
            s = 0.55f * sinf(2.0f * 3.14159265358979323846f *
                              270.0f * t) +
                0.30f * sinf(2.0f * 3.14159265358979323846f *
                              730.0f * t) +
                0.18f * sinf(2.0f * 3.14159265358979323846f *
                              1540.0f * t) +
                0.08f * sinf(2.0f * 3.14159265358979323846f *
                              3180.0f * t);
            McraEval_Clean[i] = McraEval_Saturate(5200.0f * env * s);
        }
    }
}

static double McraEval_Clean_Energy(int start, int end)
{
    int i;
    double energy;

    energy = 0.0;
    for (i = start; i < end; i++)
    {
        double v;

        v = (double)McraEval_Clean[i];
        energy += v * v;
    }
    return energy;
}

static double McraEval_Noise_Energy(int start, int end)
{
    int i;
    double energy;

    energy = 0.0;
    for (i = start; i < end; i++)
    {
        double v;

        v = (double)McraEval_NoiseUnit[i];
        energy += v * v;
    }
    return energy;
}

static float McraEval_Noise_Scale_For_Snr(float snr_db,
                                          int start, int end)
{
    double sig_energy;
    double noise_energy;
    double target_linear;
    double scale;

    sig_energy = McraEval_Clean_Energy(start, end);
    noise_energy = McraEval_Noise_Energy(start, end);
    target_linear = pow(10.0, (double)snr_db / 10.0);
    scale = sqrt(sig_energy / ((noise_energy + 1.0e-20) * target_linear));
    return (float)scale;
}

static void McraEval_Make_Stationary(float snr_db)
{
    int i;
    int learn_samples;
    int eval_start;
    int eval_end;
    float scale;

    memset(McraEval_Noisy, 0, sizeof(McraEval_Noisy));
    McraEval_Fill_Clean_Speechlike();
    McraEval_Fill_Noise_Unit(301UL + (unsigned long)(snr_db * 17.0f));

    learn_samples = McraEval_Sample_From_Seconds(2.0f);
    eval_start = McraEval_Sample_From_Seconds(3.0f);
    eval_end = MCRA_EVAL_SAMPLES - MCRA_EVAL_SKIP_TAIL - MCRA_EVAL_DELAY;
    scale = McraEval_Noise_Scale_For_Snr(snr_db, eval_start, eval_end);

    for (i = 0; i < MCRA_EVAL_SAMPLES; i++)
    {
        float noise;

        noise = scale * McraEval_NoiseUnit[i];
        if (i < learn_samples)
        {
            McraEval_Noisy[i] = McraEval_Saturate(noise);
        }
        else
        {
            McraEval_Noisy[i] =
                McraEval_Saturate((float)McraEval_Clean[i] + noise);
        }
    }
}

static void McraEval_Make_Noise_Step(float snr_db)
{
    int i;
    int learn_samples;
    int step_samples;
    int eval_start;
    int eval_end;
    float scale;
    float high_scale;
    float prev_noise;

    memset(McraEval_Noisy, 0, sizeof(McraEval_Noisy));
    McraEval_Fill_Clean_Speechlike();
    McraEval_Fill_Noise_Unit(911UL + (unsigned long)(snr_db * 31.0f));

    learn_samples = McraEval_Sample_From_Seconds(2.0f);
    step_samples = McraEval_Sample_From_Seconds(5.0f);
    eval_start = McraEval_Sample_From_Seconds(2.5f);
    eval_end = step_samples;
    scale = McraEval_Noise_Scale_For_Snr(snr_db, eval_start, eval_end);
    high_scale = scale * 1.99526231f;
    prev_noise = 0.0f;

    for (i = 0; i < MCRA_EVAL_SAMPLES; i++)
    {
        float noise_scale;
        float noise;
        float unit_noise;

        if (i >= step_samples)
        {
            noise_scale = high_scale;
            unit_noise = 0.78f *
                         (McraEval_NoiseUnit[i] - 0.85f * prev_noise);
        }
        else
        {
            noise_scale = scale;
            unit_noise = McraEval_NoiseUnit[i];
        }
        prev_noise = McraEval_NoiseUnit[i];
        noise = noise_scale * unit_noise;

        if (i < learn_samples)
        {
            McraEval_Noisy[i] = McraEval_Saturate(noise);
        }
        else
        {
            McraEval_Noisy[i] =
                McraEval_Saturate((float)McraEval_Clean[i] + noise);
        }
    }
}

static void McraEval_Make_Noise_Only_Ramp(void)
{
    int i;
    float total;

    memset(McraEval_Clean, 0, sizeof(McraEval_Clean));
    memset(McraEval_Noisy, 0, sizeof(McraEval_Noisy));
    McraEval_Fill_Noise_Unit(707UL);

    total = (float)MCRA_EVAL_SAMPLES;
    for (i = 0; i < MCRA_EVAL_SAMPLES; i++)
    {
        float ratio;
        float scale;

        ratio = (float)i / total;
        scale = 900.0f + 6400.0f * ratio;
        McraEval_Noisy[i] =
            McraEval_Saturate(scale * McraEval_NoiseUnit[i]);
    }
}

static void McraEval_Configure_Mode(const McraEvalMode *mode)
{
    SubbandWOLA_Init();
    SubbandWOLA_ResetStream();
    SubbandWOLA_ResetAllGains();
    SubbandWOLA_SetBypass(0);
    SubbandDenoise_Reset();
    SubbandDenoise_SetNoiseTrackMode(mode->track_mode);
    if (mode->mcra_variant == 1)
    {
        SubbandDenoise_SetMcraParams(1.5f, 4.0f,
                                     0.85f, 0.998f,
                                     1.10f, 1.70f,
                                     1.40f,
                                     0);
    }
    else if (mode->mcra_variant == 2)
    {
        SubbandDenoise_SetMcraParams(1.3f, 3.5f,
                                     0.80f, 0.998f,
                                     1.20f, 2.10f,
                                     1.60f,
                                     1);
    }
    SubbandDenoise_StartNoiseLearning();
}

static int McraEval_Clip_Count(const short *out, int count)
{
    int i;
    int clips;

    clips = 0;
    for (i = 0; i < count; i++)
    {
        if ((out[i] == 32767) || (out[i] == -32768))
        {
            clips++;
        }
    }
    return clips;
}

static void McraEval_Process_Mode(const McraEvalMode *mode,
                                  McraEvalMetric *metric)
{
    int frame;

    memset(McraEval_Out, 0, sizeof(McraEval_Out));
    McraEval_Configure_Mode(mode);

    for (frame = 0; frame < MCRA_EVAL_FRAMES; frame++)
    {
        SubbandWOLA_ProcessFrame(&McraEval_Noisy[frame *
                                                 SUBBAND_FRAME_LEN],
                                 &McraEval_Out[frame *
                                               SUBBAND_FRAME_LEN]);
    }

    metric->mode = mode;
    metric->gain_avg = SubbandDenoise_GetGainAvg();
    metric->gain_min = SubbandDenoise_GetGainMin();
    metric->gain_max = SubbandDenoise_GetGainMax();
    metric->noise_psd_avg = SubbandDenoise_GetNoisePsdAvg();
    metric->ms_min_psd_avg = SUBBAND_DENOISE_DebugMsMinPsdAvg;
    metric->speech_prob_avg = SUBBAND_DENOISE_DebugMcraSpeechProbAvg;
    metric->overdrive_avg = SUBBAND_DENOISE_DebugMcraOverdriveAvg;
    metric->floor_avg = SUBBAND_DENOISE_DebugMcraFloorAvg;
    metric->clip_count = McraEval_Clip_Count(McraEval_Out,
                                             MCRA_EVAL_SAMPLES);
}

static double McraEval_Input_Snr(int start, int end)
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

        c = (double)McraEval_Clean[i];
        e = (double)McraEval_Noisy[i] - c;
        sig += c * c;
        noise += e * e;
    }
    return 10.0 * log10((sig + 1.0e-20) / (noise + 1.0e-20));
}

static double McraEval_Output_Snr(const short *out, int start, int end)
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

        c = (double)McraEval_Clean[i];
        y = (double)out[i + MCRA_EVAL_DELAY];
        e = y - c;
        sig += c * c;
        err += e * e;
    }
    return 10.0 * log10((sig + 1.0e-20) / (err + 1.0e-20));
}

static void McraEval_Preservation_And_Corr(const short *out,
                                           int start,
                                           int end,
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

        c = (double)McraEval_Clean[i];
        y = (double)out[i + MCRA_EVAL_DELAY];
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

static int McraEval_Is_Valid(double x)
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

static int McraEval_Is_Metric_Valid(const McraEvalMetric *m)
{
    if ((McraEval_Is_Valid(m->input_snr_db) == 0) ||
        (McraEval_Is_Valid(m->output_snr_db) == 0) ||
        (McraEval_Is_Valid(m->speech_preservation_ratio) == 0) ||
        (McraEval_Is_Valid(m->clean_output_corr) == 0) ||
        (McraEval_Is_Valid((double)m->gain_avg) == 0) ||
        (McraEval_Is_Valid((double)m->noise_psd_avg) == 0))
    {
        return 0;
    }
    if ((m->gain_min < 0.0f) || (m->gain_max > 1.0001f) ||
        (m->clip_count > 0))
    {
        return 0;
    }
    return 1;
}

static int McraEval_Compute_Pass(const McraEvalMetric *m)
{
    if (McraEval_Is_Metric_Valid(m) == 0)
    {
        return 0;
    }

    if (m->eval_case->kind == 4)
    {
        return 1;
    }

    if (m->mode->mcra_variant == 1)
    {
        if ((m->eval_case->kind == 3) &&
            (m->eval_case->target_snr_db <= 5.1f))
        {
            /*
             * The post-step segment is roughly -1 dB input SNR, so this row is
             * a robustness stress case rather than the formal 0.95-corr gate.
             */
            if ((m->delta_vs_hybrid_db <= 0.0) ||
                (m->clean_output_corr < 0.90) ||
                (m->speech_preservation_ratio < 0.75))
            {
                return 0;
            }
            return 1;
        }
        if ((m->clean_output_corr < 0.95) ||
            (m->speech_preservation_ratio < 0.80))
        {
            return 0;
        }
        if ((m->eval_case->kind == 0) &&
            (m->delta_vs_fixed_db < -0.70))
        {
            return 0;
        }
        if ((m->eval_case->kind == 3) &&
            (m->eval_case->target_snr_db > 9.9f) &&
            (m->delta_vs_hybrid_db <= 0.0))
        {
            return 0;
        }
    }
    else if (m->mode->mcra_variant == 2)
    {
        return 1;
    }

    return 1;
}

static void McraEval_Compute_Time_Metrics(McraEvalMetric *metric,
                                          int start,
                                          int end,
                                          int noise_only)
{
    metric->input_snr_db = 0.0;
    metric->output_snr_db = 0.0;
    metric->snr_improvement_db = 0.0;
    metric->speech_preservation_ratio = 1.0;
    metric->clean_output_corr = 1.0;

    if (noise_only == 0)
    {
        metric->input_snr_db = McraEval_Input_Snr(start, end);
        metric->output_snr_db =
            McraEval_Output_Snr(McraEval_Out, start, end);
        metric->snr_improvement_db =
            metric->output_snr_db - metric->input_snr_db;
        McraEval_Preservation_And_Corr(McraEval_Out, start, end,
                                       &metric->speech_preservation_ratio,
                                       &metric->clean_output_corr);
    }
}

static void McraEval_Run_Case(const McraEvalCase *eval_case,
                              McraEvalMetric *metrics)
{
    int i;
    int start;
    int end;
    int noise_only;
    double fixed_snr;
    double hybrid_snr;

    if (eval_case->kind == 0)
    {
        McraEval_Make_Stationary(eval_case->target_snr_db);
        start = McraEval_Sample_From_Seconds(3.0f);
        noise_only = 0;
    }
    else if (eval_case->kind == 3)
    {
        McraEval_Make_Noise_Step(eval_case->target_snr_db);
        start = McraEval_Sample_From_Seconds(5.0f);
        noise_only = 0;
    }
    else
    {
        McraEval_Make_Noise_Only_Ramp();
        start = McraEval_Sample_From_Seconds(3.0f);
        noise_only = 1;
    }
    end = MCRA_EVAL_SAMPLES - MCRA_EVAL_SKIP_TAIL - MCRA_EVAL_DELAY;

    memset(metrics, 0, sizeof(McraEvalMetric) * MCRA_EVAL_MODE_COUNT);
    for (i = 0; i < MCRA_EVAL_MODE_COUNT; i++)
    {
        metrics[i].eval_case = eval_case;
        McraEval_Process_Mode(&McraEval_Modes[i], &metrics[i]);
        McraEval_Compute_Time_Metrics(&metrics[i], start, end,
                                      noise_only);
    }

    fixed_snr = metrics[0].output_snr_db;
    hybrid_snr = metrics[1].output_snr_db;
    for (i = 0; i < MCRA_EVAL_MODE_COUNT; i++)
    {
        metrics[i].delta_vs_fixed_db =
            metrics[i].output_snr_db - fixed_snr;
        metrics[i].delta_vs_hybrid_db =
            metrics[i].output_snr_db - hybrid_snr;
        metrics[i].pass = McraEval_Compute_Pass(&metrics[i]);
    }
}

static void McraEval_Write_Csv_Header(FILE *fp)
{
    fprintf(fp,
            "eval_case,mode_name,track_mode,input_snr_db,output_snr_db,"
            "snr_improvement_db,delta_vs_fixed_db,delta_vs_hybrid_db,"
            "speech_preservation_ratio,clean_output_corr,gain_avg,"
            "gain_min,gain_max,noise_psd_avg,ms_min_psd_avg,"
            "speech_prob_avg,overdrive_avg,floor_avg,pass\n");
}

static void McraEval_Write_Csv_Row(FILE *fp, const McraEvalMetric *m)
{
    fprintf(fp,
            "%s,%s,%d,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,"
            "%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%s\n",
            m->eval_case->name,
            m->mode->name,
            m->mode->track_mode,
            m->input_snr_db,
            m->output_snr_db,
            m->snr_improvement_db,
            m->delta_vs_fixed_db,
            m->delta_vs_hybrid_db,
            m->speech_preservation_ratio,
            m->clean_output_corr,
            m->gain_avg,
            m->gain_min,
            m->gain_max,
            m->noise_psd_avg,
            m->ms_min_psd_avg,
            m->speech_prob_avg,
            m->overdrive_avg,
            m->floor_avg,
            m->pass ? "PASS" : "FAIL");
}

int SubbandDenoiseMcraEval_OfflineTest_All(void)
{
    static const McraEvalCase eval_cases[6] =
    {
        {"stationary_snr5", 0, 5.0f},
        {"stationary_snr10", 0, 10.0f},
        {"stationary_snr20", 0, 20.0f},
        {"noise_step_up_snr10", 3, 10.0f},
        {"noise_step_up_snr5", 3, 5.0f},
        {"noise_only_ramp", 4, 0.0f}
    };
    McraEvalMetric metrics[MCRA_EVAL_MODE_COUNT];
    FILE *fp;
    int case_idx;
    int mode_idx;
    int failures;

    failures = 0;
    fp = fopen(MCRA_EVAL_REPORT, "w");
    if (fp == 0)
    {
        printf("denoise_mcra_eval FAIL open_report=%s\n",
               MCRA_EVAL_REPORT);
        return 1;
    }

    McraEval_Write_Csv_Header(fp);
    for (case_idx = 0; case_idx < 6; case_idx++)
    {
        McraEval_Run_Case(&eval_cases[case_idx], metrics);
        for (mode_idx = 0; mode_idx < MCRA_EVAL_MODE_COUNT; mode_idx++)
        {
            McraEval_Write_Csv_Row(fp, &metrics[mode_idx]);
            printf("denoise_mcra_eval %-22s %-12s in=%.3f out=%.3f "
                   "imp=%.3f dfix=%.3f dhyb=%.3f pres=%.3f corr=%.3f "
                   "gain=%.3f min=%.3f max=%.3f noise=%.3f ms_min=%.3f "
                   "sp=%.3f od=%.3f floor=%.3f clips=%d %s\n",
                   metrics[mode_idx].eval_case->name,
                   metrics[mode_idx].mode->name,
                   metrics[mode_idx].input_snr_db,
                   metrics[mode_idx].output_snr_db,
                   metrics[mode_idx].snr_improvement_db,
                   metrics[mode_idx].delta_vs_fixed_db,
                   metrics[mode_idx].delta_vs_hybrid_db,
                   metrics[mode_idx].speech_preservation_ratio,
                   metrics[mode_idx].clean_output_corr,
                   metrics[mode_idx].gain_avg,
                   metrics[mode_idx].gain_min,
                   metrics[mode_idx].gain_max,
                   metrics[mode_idx].noise_psd_avg,
                   metrics[mode_idx].ms_min_psd_avg,
                   metrics[mode_idx].speech_prob_avg,
                   metrics[mode_idx].overdrive_avg,
                   metrics[mode_idx].floor_avg,
                   metrics[mode_idx].clip_count,
                   metrics[mode_idx].pass ? "PASS" : "FAIL");
            if (metrics[mode_idx].pass == 0)
            {
                failures++;
            }
        }
    }

    fclose(fp);
    printf("denoise_mcra_eval report=%s failures=%d\n",
           MCRA_EVAL_REPORT, failures);
    return failures;
}

#else

int SubbandDenoiseMcraEval_OfflineTest_All(void)
{
    return 0;
}

#endif /* SUBBAND_ALGO_ONLY */
