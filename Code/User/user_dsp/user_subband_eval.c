/**
 * user_subband_eval.c
 *
 * PC algo-only evaluation for WOLA passthrough and fixed-noise Wiener denoise.
 */

#include "user_subband_eval.h"
#include "user_subband_wola.h"
#include "user_subband_denoise.h"

volatile unsigned long SUBBAND_EVAL_DebugAdFrames = 0;
volatile unsigned long SUBBAND_EVAL_DebugDaFrames = 0;
volatile unsigned long SUBBAND_EVAL_DebugWolaFrames = 0;
volatile unsigned long SUBBAND_EVAL_DebugDenoiseFrames = 0;
volatile float SUBBAND_EVAL_DebugFrameBudgetMs =
    ((float)SUBBAND_FRAME_LEN / SUBBAND_SAMPLE_RATE) * 1000.0f;
volatile float SUBBAND_EVAL_DebugDenoiseLastMs = 0.0f;
volatile float SUBBAND_EVAL_DebugDenoiseMaxMs = 0.0f;
volatile float SUBBAND_EVAL_DebugCpuUsagePercent = 0.0f;

#ifdef SUBBAND_ALGO_ONLY

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

/*
 * Default PC test-vector location. Extract
 * subband_eval_audio_50k_with_2s_noise.zip to:
 *   test_vectors/subband_eval_audio/
 *
 * A PC test build can override it, for example:
 *   -DSUBBAND_EVAL_AUDIO_DIR=\"D:/NDM/subband_eval_audio_50k_with_2s_noise\"
 */
#ifndef SUBBAND_EVAL_AUDIO_DIR
#define SUBBAND_EVAL_AUDIO_DIR "test_vectors/subband_eval_audio"
#endif

#define EVAL_MAX_SAMPLES 500000
#define EVAL_MAX_FRAMES (EVAL_MAX_SAMPLES / SUBBAND_FRAME_LEN)
#define EVAL_MAX_PROCESS_SAMPLES (EVAL_MAX_FRAMES * SUBBAND_FRAME_LEN)
#define EVAL_HOPS_PER_FRAME (SUBBAND_FRAME_LEN / SUBBAND_HOP)
#define EVAL_SKIP_TAIL (2 * SUBBAND_NFFT)
#define EVAL_WAV_HEADER_MAX 4096

static short Eval_Input[EVAL_MAX_SAMPLES];
static short Eval_Clean[EVAL_MAX_SAMPLES];
static short Eval_Output[EVAL_MAX_SAMPLES];

static float Eval_BandBefore[SUBBAND_NUM_BANDS];
static float Eval_BandAfter[SUBBAND_NUM_BANDS];

static void Eval_Print_Band_Energy_Line(const char *name);
static void Eval_Compute_Band_Energy(short *before, short *after,
                                     int count, int delay, int start);

static float Eval_Abs_Float(float x)
{
    if (x < 0.0f)
    {
        return -x;
    }
    return x;
}

static int Eval_Abs_Int(int x)
{
    if (x < 0)
    {
        return -x;
    }
    return x;
}

static double Eval_Now_Ms(void)
{
    return ((double)clock() * 1000.0) / (double)CLOCKS_PER_SEC;
}

static void Eval_Clear_Result(SubbandEvalResult *r)
{
    if (r != 0)
    {
        memset(r, 0, sizeof(*r));
        r->frame_budget_ms =
            ((float)SUBBAND_FRAME_LEN / SUBBAND_SAMPLE_RATE) * 1000.0f;
        r->gain_avg = 1.0f;
        r->gain_min = 1.0f;
        r->gain_max = 1.0f;
        r->expected_delay_samples = SUBBAND_EXPECTED_DELAY;
        r->board_realtime_status = SUBBAND_EVAL_BOARD_NOT_MEASURED;
    }
}

static void Eval_Reset_For_Passthrough(void)
{
    SubbandWOLA_Init();
    SubbandWOLA_ResetStream();
    SubbandWOLA_ResetAllGains();
    SubbandWOLA_SetBypass(0);
    SubbandDenoise_Reset();
    SubbandDenoise_StopLearning();
    SubbandDenoise_SetEnabled(0);
}

static void Eval_Reset_For_Denoise(void)
{
    SubbandWOLA_Init();
    SubbandWOLA_ResetStream();
    SubbandWOLA_ResetAllGains();
    SubbandWOLA_SetBypass(0);
    SubbandDenoise_Reset();
    SubbandDenoise_StartNoiseLearning();
}

static void Eval_Fill_Noise(short *dst, int count, unsigned long seed,
                            float amplitude)
{
    int i;
    unsigned long state;

    state = seed;
    for (i = 0; i < count; i++)
    {
        float centered;

        state = (1103515245UL * state + 12345UL) & 0x7fffffffUL;
        centered = ((float)state / 1073741824.0f) - 1.0f;
        dst[i] = (short)(amplitude * centered);
    }
}

static void Eval_Fill_Sine(short *dst, int count, float freq_hz,
                           float amplitude)
{
    int i;

    for (i = 0; i < count; i++)
    {
        float phase;

        phase = (2.0f * 3.14159265358979323846f * freq_hz * (float)i) /
                SUBBAND_SAMPLE_RATE;
        dst[i] = (short)(amplitude * sinf(phase));
    }
}

static void Eval_Fill_Speechlike(short *dst, int count)
{
    int i;

    for (i = 0; i < count; i++)
    {
        float t;
        float env;
        float sample;

        t = (float)i / SUBBAND_SAMPLE_RATE;
        env = 0.65f + 0.35f * sinf(2.0f * 3.14159265358979323846f * 3.2f * t);
        sample =
            5200.0f * sinf(2.0f * 3.14159265358979323846f * 220.0f * t) +
            3600.0f * sinf(2.0f * 3.14159265358979323846f * 790.0f * t) +
            2200.0f * sinf(2.0f * 3.14159265358979323846f * 1850.0f * t) +
            1200.0f * sinf(2.0f * 3.14159265358979323846f * 3300.0f * t);
        sample *= env;
        if (sample > 24000.0f)
        {
            sample = 24000.0f;
        }
        if (sample < -24000.0f)
        {
            sample = -24000.0f;
        }
        dst[i] = (short)sample;
    }
}

static void Eval_Process_Frames(short *input, short *output, int frames)
{
    int frame;

    for (frame = 0; frame < frames; frame++)
    {
        SubbandWOLA_ProcessFrame(&input[frame * SUBBAND_FRAME_LEN],
                                 &output[frame * SUBBAND_FRAME_LEN]);
    }
}

static int Eval_Find_Aligned_Lag_From(short *input, short *output,
                                      int count, int start)
{
    int lag;
    int best_lag;
    double best_err;

    best_lag = SUBBAND_EXPECTED_DELAY;
    best_err = 1.0e300;
    if (start < 0)
    {
        start = 0;
    }
    if ((count - start) <= (SUBBAND_NFFT + EVAL_SKIP_TAIL + 1))
    {
        return best_lag;
    }

    for (lag = 0; lag <= SUBBAND_NFFT; lag++)
    {
        int i;
        double err;

        err = 0.0;
        for (i = start; i < count - lag - EVAL_SKIP_TAIL; i++)
        {
            double d;

            d = (double)output[i + lag] - (double)input[i];
            err += d * d;
        }
        if (err < best_err)
        {
            best_err = err;
            best_lag = lag;
        }
    }

    return best_lag;
}

static int Eval_Find_Aligned_Lag(short *input, short *output, int count)
{
    return Eval_Find_Aligned_Lag_From(input, output, count,
                                      4 * SUBBAND_FRAME_LEN);
}

static float Eval_Aligned_Snr(short *ref, short *out, int count,
                              int delay, int start)
{
    int i;
    double sig;
    double err;

    sig = 0.0;
    err = 0.0;
    for (i = start; i < count - delay - EVAL_SKIP_TAIL; i++)
    {
        double x;
        double y;
        double d;

        x = (double)ref[i];
        y = (double)out[i + delay];
        d = y - x;
        sig += x * x;
        err += d * d;
    }

    return (float)(10.0 * log10((sig + 1.0e-20) / (err + 1.0e-20)));
}

static float Eval_Aligned_Energy_Ratio(short *input, short *output, int count,
                                       int delay, int start)
{
    int i;
    double in_energy;
    double out_energy;

    in_energy = 0.0;
    out_energy = 0.0;
    for (i = start; i < count - delay - EVAL_SKIP_TAIL; i++)
    {
        double x;
        double y;

        x = (double)input[i];
        y = (double)output[i + delay];
        in_energy += x * x;
        out_energy += y * y;
    }

    return (float)(out_energy / (in_energy + 1.0e-20));
}

static float Eval_Input_Snr(short *noisy, short *clean, int count, int start)
{
    int i;
    double sig;
    double noise;

    sig = 0.0;
    noise = 0.0;
    for (i = start; i < count - EVAL_SKIP_TAIL; i++)
    {
        double c;
        double n;

        c = (double)clean[i];
        n = (double)noisy[i] - c;
        sig += c * c;
        noise += n * n;
    }

    return (float)(10.0 * log10((sig + 1.0e-20) / (noise + 1.0e-20)));
}

static float Eval_Output_Snr(short *output, short *clean, int count,
                             int delay, int start)
{
    int i;
    double sig;
    double err;

    sig = 0.0;
    err = 0.0;
    for (i = start; i < count - delay - EVAL_SKIP_TAIL; i++)
    {
        double c;
        double y;
        double d;

        c = (double)clean[i];
        y = (double)output[i + delay];
        d = y - c;
        sig += c * c;
        err += d * d;
    }

    return (float)(10.0 * log10((sig + 1.0e-20) / (err + 1.0e-20)));
}

static void Eval_Preservation_And_Corr(short *output, short *clean, int count,
                                       int delay, int start, float *preserve,
                                       float *corr)
{
    int i;
    double clean_energy;
    double out_energy;
    double xy;

    clean_energy = 0.0;
    out_energy = 0.0;
    xy = 0.0;
    for (i = start; i < count - delay - EVAL_SKIP_TAIL; i++)
    {
        double c;
        double y;

        c = (double)clean[i];
        y = (double)output[i + delay];
        clean_energy += c * c;
        out_energy += y * y;
        xy += c * y;
    }

    *preserve = (float)sqrt(out_energy / (clean_energy + 1.0e-20));
    *corr = (float)(xy / sqrt((clean_energy + 1.0e-20) *
                              (out_energy + 1.0e-20)));
}

static float Eval_Noise_Reduction(short *input, short *output, int count,
                                  int delay, int start)
{
    int i;
    double in_energy;
    double out_energy;

    in_energy = 0.0;
    out_energy = 0.0;
    for (i = start; i < count - delay - EVAL_SKIP_TAIL; i++)
    {
        double x;
        double y;

        x = (double)input[i];
        y = (double)output[i + delay];
        in_energy += x * x;
        out_energy += y * y;
    }

    return (float)(10.0 * log10((out_energy + 1.0e-20) /
                                (in_energy + 1.0e-20)));
}

static unsigned int Eval_Read_U32_LE(const unsigned char *p)
{
    return ((unsigned int)p[0]) |
           ((unsigned int)p[1] << 8) |
           ((unsigned int)p[2] << 16) |
           ((unsigned int)p[3] << 24);
}

static unsigned short Eval_Read_U16_LE(const unsigned char *p)
{
    return (unsigned short)(((unsigned short)p[0]) |
                            ((unsigned short)p[1] << 8));
}

static int Eval_Read_Wav(const char *path, short *dst, int max_samples,
                         int *sample_rate, int *sample_count)
{
    FILE *fp;
    unsigned char header[EVAL_WAV_HEADER_MAX];
    size_t got;
    int offset;
    int data_offset;
    int data_size;
    int fmt_channels;
    int fmt_bits;
    int fmt_rate;
    int samples;

    fp = fopen(path, "rb");
    if (fp == 0)
    {
        printf("wav_read FAIL open=%s\n", path);
        return 0;
    }

    got = fread(header, 1, sizeof(header), fp);
    if (got < 44U)
    {
        fclose(fp);
        printf("wav_read FAIL short_header=%s\n", path);
        return 0;
    }

    fmt_channels = 0;
    fmt_bits = 0;
    fmt_rate = 0;
    data_offset = -1;
    data_size = 0;
    offset = 12;
    while (offset + 8 < (int)got)
    {
        unsigned int chunk_size;

        chunk_size = Eval_Read_U32_LE(&header[offset + 4]);
        if ((header[offset] == 'f') &&
            (header[offset + 1] == 'm') &&
            (header[offset + 2] == 't') &&
            (header[offset + 3] == ' '))
        {
            fmt_channels = (int)Eval_Read_U16_LE(&header[offset + 10]);
            fmt_rate = (int)Eval_Read_U32_LE(&header[offset + 12]);
            fmt_bits = (int)Eval_Read_U16_LE(&header[offset + 22]);
        }
        else if ((header[offset] == 'd') &&
                 (header[offset + 1] == 'a') &&
                 (header[offset + 2] == 't') &&
                 (header[offset + 3] == 'a'))
        {
            data_offset = offset + 8;
            data_size = (int)chunk_size;
            break;
        }
        offset += 8 + (int)chunk_size;
        if ((offset & 1) != 0)
        {
            offset++;
        }
    }

    if ((data_offset < 0) || (fmt_channels != 1) ||
        (fmt_bits != 16) || (fmt_rate != 50000))
    {
        fclose(fp);
        printf("wav_read FAIL format path=%s rate=%d ch=%d bits=%d\n",
               path, fmt_rate, fmt_channels, fmt_bits);
        return 0;
    }

    samples = data_size / 2;
    if (samples > max_samples)
    {
        samples = max_samples;
    }

    fseek(fp, data_offset, SEEK_SET);
    for (offset = 0; offset < samples; offset++)
    {
        unsigned char b[2];

        if (fread(b, 1, 2, fp) != 2U)
        {
            samples = offset;
            break;
        }
        dst[offset] = (short)Eval_Read_U16_LE(b);
    }
    fclose(fp);

    *sample_rate = fmt_rate;
    *sample_count = samples;
    return 1;
}

static void Eval_Join_Path(char *dst, int dst_len, const char *dir,
                           const char *name)
{
    int n;

    n = snprintf(dst, (size_t)dst_len, "%s/%s", dir, name);
    if ((n < 0) || (n >= dst_len))
    {
        dst[0] = '\0';
    }
}

static void Eval_Write_Csv_Header(FILE *fp)
{
    if (fp != 0)
    {
        fprintf(fp, "eval_case,wola_passthrough_snr_db,wola_energy_ratio,"
                    "wola_delay_samples,learn_target_hops,"
                    "learn_actual_hops,learn_progress_final,"
                    "noise_psd_avg,input_snr_db,output_snr_db,"
                    "snr_improvement_db,noise_reduction_db,"
                    "output_input_energy_ratio,speech_preservation_ratio,"
                    "clean_output_corr,gain_avg,gain_min,gain_max,"
                    "pc_runtime_ms,board_last_ms,board_max_ms,"
                    "cpu_usage_percent,board_realtime_status,pass\n");
    }
}

static void Eval_Write_Csv_Row(FILE *fp, const char *name,
                               const SubbandEvalResult *r)
{
    if ((fp != 0) && (r != 0))
    {
        fprintf(fp, "%s,%.3f,%.6f,%d,%lu,%lu,%.6f,%.3f,"
                    "%.3f,%.3f,%.3f,%.3f,%.6f,%.6f,%.6f,"
                    "%.6f,%.6f,%.6f,%.3f,%.3f,%.3f,%.3f,%s,%d\n",
                name,
                r->wola_passthrough_snr_db,
                r->wola_energy_ratio,
                r->wola_delay_samples,
                r->learn_target_hops,
                r->learn_actual_hops,
                r->learn_progress_final,
                r->noise_psd_avg,
                r->input_snr_db,
                r->output_snr_db,
                r->snr_improvement_db,
                r->noise_reduction_db,
                r->output_input_energy_ratio,
                r->speech_preservation_ratio,
                r->clean_output_corr,
                r->gain_avg,
                r->gain_min,
                r->gain_max,
                r->pc_runtime_ms,
                r->board_last_ms,
                r->board_max_ms,
                r->cpu_usage_percent,
                (r->board_realtime_status != 0) ?
                    r->board_realtime_status : SUBBAND_EVAL_BOARD_NOT_MEASURED,
                r->pass_overall);
    }
}

void SubbandEval_PrintReport(const SubbandEvalResult *r)
{
    if (r == 0)
    {
        return;
    }

    printf("========== SUBBAND DENOISE EVALUATION REPORT ==========\n");
    printf("WOLA passthrough SNR: %.2f dB\n", r->wola_passthrough_snr_db);
    printf("WOLA energy ratio: %.6f\n", r->wola_energy_ratio);
    printf("WOLA delay samples: %d\n", r->wola_delay_samples);
    printf("Expected delay samples: %d\n", r->expected_delay_samples);
    printf("Measured delay samples: %d\n", r->measured_delay_samples);
    printf("Noise learning target hops: %lu\n", r->learn_target_hops);
    printf("Noise learning actual hops: %lu\n", r->learn_actual_hops);
    printf("Noise PSD avg: %.3f\n", r->noise_psd_avg);
    if (r->learn_target_hops == 0UL)
    {
        printf("Learning pass: N/A\n");
    }
    else
    {
        printf("Learning pass: %s\n", r->pass_learning ? "PASS" : "FAIL");
    }
    printf("Input SNR: %.2f dB\n", r->input_snr_db);
    printf("Output SNR: %.2f dB\n", r->output_snr_db);
    printf("SNR improvement: %.2f dB\n", r->snr_improvement_db);
    printf("Noise reduction: %.2f dB\n", r->noise_reduction_db);
    printf("Speech preservation ratio: %.3f\n", r->speech_preservation_ratio);
    printf("Clean-output correlation: %.3f\n", r->clean_output_corr);
    printf("Gain avg/min/max: %.3f / %.3f / %.3f\n",
           r->gain_avg, r->gain_min, r->gain_max);
    printf("Frame budget: %.2f ms\n", r->frame_budget_ms);
    printf("PC runtime: %.3f ms\n", r->pc_runtime_ms);
    printf("Board last/max time: %.3f / %.3f ms\n",
           r->board_last_ms, r->board_max_ms);
    printf("CPU usage: %.2f %%\n", r->cpu_usage_percent);
    printf("Board realtime: %s\n",
           (r->board_realtime_status != 0) ?
               r->board_realtime_status : SUBBAND_EVAL_BOARD_NOT_MEASURED);
    printf("Overall result: %s\n", r->pass_overall ? "PASS" : "FAIL");
    printf("=======================================================\n");
}

int SubbandEval_RunWolaPassthroughTest(SubbandEvalResult *r)
{
    int cases;
    int frames;
    int samples;
    int lag;
    int fail;
    double begin_ms;
    float worst_snr;
    float worst_ratio;

    Eval_Clear_Result(r);
    begin_ms = Eval_Now_Ms();
    frames = 48;
    samples = frames * SUBBAND_FRAME_LEN;
    fail = 0;
    worst_snr = 1000.0f;
    worst_ratio = 1.0f;

    for (cases = 0; cases < 3; cases++)
    {
        float snr;
        float ratio;

        if (cases == 0)
        {
            Eval_Fill_Sine(Eval_Input, samples, 1000.0f, 12000.0f);
        }
        else if (cases == 1)
        {
            Eval_Fill_Noise(Eval_Input, samples, 1234UL, 10000.0f);
        }
        else
        {
            Eval_Fill_Speechlike(Eval_Input, samples);
        }
        memset(Eval_Output, 0, sizeof(Eval_Output));

        Eval_Reset_For_Passthrough();
        Eval_Process_Frames(Eval_Input, Eval_Output, frames);
        lag = Eval_Find_Aligned_Lag(Eval_Input, Eval_Output, samples);
        snr = Eval_Aligned_Snr(Eval_Input, Eval_Output, samples, lag,
                               4 * SUBBAND_FRAME_LEN);
        ratio = Eval_Aligned_Energy_Ratio(Eval_Input, Eval_Output, samples,
                                          lag, 4 * SUBBAND_FRAME_LEN);
        if (snr < worst_snr)
        {
            worst_snr = snr;
        }
        if (Eval_Abs_Float(ratio - 1.0f) > Eval_Abs_Float(worst_ratio - 1.0f))
        {
            worst_ratio = ratio;
        }
        if ((snr <= 60.0f) || (ratio < 0.90f) || (ratio > 1.10f))
        {
            fail = 1;
        }
        if (Eval_Abs_Int(lag - SUBBAND_EXPECTED_DELAY) > 2)
        {
            fail = 1;
        }
        printf("eval_wola_passthrough case=%d snr_db=%.3f energy_ratio=%.6f lag=%d\n",
               cases, snr, ratio, lag);
    }

    if (r != 0)
    {
        r->wola_passthrough_snr_db = worst_snr;
        r->wola_energy_ratio = worst_ratio;
        r->wola_delay_samples = lag;
        r->expected_delay_samples = SUBBAND_EXPECTED_DELAY;
        r->measured_delay_samples = lag;
        r->pc_runtime_ms = (float)(Eval_Now_Ms() - begin_ms);
        r->pass_wola = (fail == 0);
        r->pass_overall = r->pass_wola;
    }

    return fail;
}

int SubbandEval_RunNoiseLearningTest(SubbandEvalResult *r)
{
    int frames;
    int samples;
    int fail;
    unsigned long target;
    unsigned long actual;
    float progress;
    float psd_avg;
    double begin_ms;

    Eval_Clear_Result(r);
    begin_ms = Eval_Now_Ms();

    target = (unsigned long)
        ((SUBBAND_DENOISE_DEFAULT_LEARN_SECONDS * SUBBAND_SAMPLE_RATE /
          (float)SUBBAND_HOP) + 0.5f);
    frames = (int)((target + EVAL_HOPS_PER_FRAME - 1) /
                   EVAL_HOPS_PER_FRAME);
    samples = frames * SUBBAND_FRAME_LEN;
    Eval_Fill_Noise(Eval_Input, samples, 55UL, 9000.0f);
    memset(Eval_Output, 0, sizeof(Eval_Output));

    Eval_Reset_For_Denoise();
    Eval_Process_Frames(Eval_Input, Eval_Output, frames);

    actual = SubbandDenoise_GetLearnCount();
    progress = SubbandDenoise_GetLearnProgress();
    psd_avg = SubbandDenoise_GetNoisePsdAvg();

    fail = 0;
    if ((actual + 1UL < target) || (actual > target + 1UL))
    {
        fail = 1;
    }
    if ((progress < 0.999f) || (SubbandDenoise_IsReady() == 0) ||
        (SubbandDenoise_IsEnabled() == 0) ||
        (psd_avg <= SUBBAND_DENOISE_EPS))
    {
        fail = 1;
    }

    if (r != 0)
    {
        r->learn_target_hops = target;
        r->learn_actual_hops = actual;
        r->learn_progress_final = progress;
        r->noise_psd_avg = psd_avg;
        r->pc_runtime_ms = (float)(Eval_Now_Ms() - begin_ms);
        r->pass_learning = (fail == 0);
        r->pass_overall = r->pass_learning;
    }

    printf("eval_noise_learning target=%lu actual=%lu progress=%.6f "
           "ready=%d enabled=%d noise_psd_avg=%.3f %s\n",
           target, actual, progress, SubbandDenoise_IsReady(),
           SubbandDenoise_IsEnabled(), psd_avg, fail ? "FAIL" : "PASS");
    return fail;
}

int SubbandEval_RunNoiseSuppressionTest(SubbandEvalResult *r,
                                        const char *audio_dir)
{
    char path[512];
    int rate;
    int count;
    int frames;
    int samples;
    int start;
    int fail;
    int measured_delay;
    float reduction;
    double begin_ms;

    Eval_Clear_Result(r);
    begin_ms = Eval_Now_Ms();
    Eval_Join_Path(path, sizeof(path), audio_dir,
                   "noise_only_50k_10s_for_suppression_test.wav");
    if (!Eval_Read_Wav(path, Eval_Input, EVAL_MAX_SAMPLES, &rate, &count))
    {
        return 1;
    }

    frames = count / SUBBAND_FRAME_LEN;
    samples = frames * SUBBAND_FRAME_LEN;
    memset(Eval_Output, 0, sizeof(Eval_Output));
    Eval_Reset_For_Denoise();
    Eval_Process_Frames(Eval_Input, Eval_Output, frames);

    start = (int)(2.5f * SUBBAND_SAMPLE_RATE);
    measured_delay = Eval_Find_Aligned_Lag_From(Eval_Input, Eval_Output,
                                                samples, start);
    reduction = Eval_Noise_Reduction(Eval_Input, Eval_Output, samples,
                                     measured_delay, start);
    fail = (reduction < -3.0f) ? 0 : 1;

    if (r != 0)
    {
        r->expected_delay_samples = SUBBAND_EXPECTED_DELAY;
        r->measured_delay_samples = measured_delay;
        r->noise_reduction_db = reduction;
        r->output_input_energy_ratio = (float)pow(10.0, reduction / 10.0);
        r->gain_avg = SubbandDenoise_GetGainAvg();
        r->gain_min = SubbandDenoise_GetGainMin();
        r->gain_max = SubbandDenoise_GetGainMax();
        r->learn_target_hops = SubbandDenoise_GetTargetHops();
        r->learn_actual_hops = SubbandDenoise_GetLearnCount();
        r->learn_progress_final = SubbandDenoise_GetLearnProgress();
        r->noise_psd_avg = SubbandDenoise_GetNoisePsdAvg();
        r->pc_runtime_ms = (float)(Eval_Now_Ms() - begin_ms);
        r->pass_noise_reduction = (fail == 0);
        r->pass_learning =
            ((r->learn_actual_hops == r->learn_target_hops) &&
             (r->learn_progress_final >= 0.999f) &&
             (r->noise_psd_avg > SUBBAND_DENOISE_EPS));
        r->pass_overall = (fail == 0);
    }

    printf("eval_noise_suppression noise_reduction_db=%.3f measured_delay=%d "
           "gain_avg=%.3f gain_min=%.3f gain_max=%.3f %s\n",
           reduction,
           measured_delay,
           SubbandDenoise_GetGainAvg(),
           SubbandDenoise_GetGainMin(),
           SubbandDenoise_GetGainMax(),
           fail ? "FAIL" : "PASS");
    return fail;
}

static const char *Eval_Noisy_Name(float target_snr_db)
{
    if (target_snr_db < 7.5f)
    {
        return "noisy_speechlike_snr5dB_50k_10s_first2s_noise.wav";
    }
    if (target_snr_db < 15.0f)
    {
        return "noisy_speechlike_snr10dB_50k_10s_first2s_noise.wav";
    }
    return "noisy_speechlike_snr20dB_50k_10s_first2s_noise.wav";
}

static const char *Eval_Clean_Name(float target_snr_db)
{
    if (target_snr_db < 7.5f)
    {
        return "clean_reference_for_snr5dB_50k_10s_first2s_silence.wav";
    }
    if (target_snr_db < 15.0f)
    {
        return "clean_reference_for_snr10dB_50k_10s_first2s_silence.wav";
    }
    return "clean_reference_for_snr20dB_50k_10s_first2s_silence.wav";
}

int SubbandEval_RunSNRImprovementTest(float target_snr_db,
                                      SubbandEvalResult *r,
                                      const char *audio_dir)
{
    char noisy_path[512];
    char clean_path[512];
    int rate;
    int noisy_count;
    int clean_count;
    int frames;
    int samples;
    int start;
    int fail;
    int measured_delay;
    double begin_ms;

    Eval_Clear_Result(r);
    begin_ms = Eval_Now_Ms();
    Eval_Join_Path(noisy_path, sizeof(noisy_path), audio_dir,
                   Eval_Noisy_Name(target_snr_db));
    Eval_Join_Path(clean_path, sizeof(clean_path), audio_dir,
                   Eval_Clean_Name(target_snr_db));
    if (!Eval_Read_Wav(noisy_path, Eval_Input, EVAL_MAX_SAMPLES,
                       &rate, &noisy_count))
    {
        return 1;
    }
    if (!Eval_Read_Wav(clean_path, Eval_Clean, EVAL_MAX_SAMPLES,
                       &rate, &clean_count))
    {
        return 1;
    }

    frames = noisy_count / SUBBAND_FRAME_LEN;
    if (clean_count / SUBBAND_FRAME_LEN < frames)
    {
        frames = clean_count / SUBBAND_FRAME_LEN;
    }
    samples = frames * SUBBAND_FRAME_LEN;
    memset(Eval_Output, 0, sizeof(Eval_Output));
    Eval_Reset_For_Denoise();
    Eval_Process_Frames(Eval_Input, Eval_Output, frames);

    start = (int)(2.5f * SUBBAND_SAMPLE_RATE);
    measured_delay = Eval_Find_Aligned_Lag_From(Eval_Clean, Eval_Output,
                                                samples, start);
    r->expected_delay_samples = SUBBAND_EXPECTED_DELAY;
    r->measured_delay_samples = measured_delay;
    r->input_snr_db = Eval_Input_Snr(Eval_Input, Eval_Clean, samples, start);
    r->output_snr_db =
        Eval_Output_Snr(Eval_Output, Eval_Clean, samples,
                        measured_delay, start);
    r->snr_improvement_db = r->output_snr_db - r->input_snr_db;
    r->output_input_energy_ratio =
        Eval_Aligned_Energy_Ratio(Eval_Input, Eval_Output, samples,
                                  measured_delay, start);
    Eval_Preservation_And_Corr(Eval_Output, Eval_Clean, samples,
                               measured_delay, start,
                               &r->speech_preservation_ratio,
                               &r->clean_output_corr);
    r->gain_avg = SubbandDenoise_GetGainAvg();
    r->gain_min = SubbandDenoise_GetGainMin();
    r->gain_max = SubbandDenoise_GetGainMax();
    r->noise_psd_avg = SubbandDenoise_GetNoisePsdAvg();
    r->learn_target_hops = SubbandDenoise_GetTargetHops();
    r->learn_actual_hops = SubbandDenoise_GetLearnCount();
    r->learn_progress_final = SubbandDenoise_GetLearnProgress();
    r->pc_runtime_ms = (float)(Eval_Now_Ms() - begin_ms);

    fail = 0;
    if (Eval_Abs_Int(measured_delay - SUBBAND_EXPECTED_DELAY) > 2)
    {
        fail = 1;
    }
    if (target_snr_db < 15.0f)
    {
        if (r->snr_improvement_db <= 2.0f)
        {
            fail = 1;
        }
    }
    else
    {
        if (r->snr_improvement_db < -0.5f)
        {
            fail = 1;
        }
    }
    if ((r->speech_preservation_ratio < 0.75f) ||
        (r->speech_preservation_ratio > 1.20f) ||
        (r->clean_output_corr <= 0.80f))
    {
        fail = 1;
    }

    r->pass_snr_improvement = (fail == 0);
    r->pass_speech_preservation =
        ((r->speech_preservation_ratio >= 0.75f) &&
         (r->speech_preservation_ratio <= 1.20f) &&
         (r->clean_output_corr > 0.80f));
    r->pass_learning =
        ((r->learn_actual_hops == r->learn_target_hops) &&
         (r->learn_progress_final >= 0.999f) &&
         (r->noise_psd_avg > SUBBAND_DENOISE_EPS));
    r->pass_overall = (fail == 0);
    Eval_Compute_Band_Energy(Eval_Input, Eval_Output, samples,
                             measured_delay, start);
    if (target_snr_db < 7.5f)
    {
        Eval_Print_Band_Energy_Line("snr5");
    }
    else if (target_snr_db < 15.0f)
    {
        Eval_Print_Band_Energy_Line("snr10");
    }
    else
    {
        Eval_Print_Band_Energy_Line("snr20");
    }

    printf("eval_snr target=%.0f input_snr_db=%.3f output_snr_db=%.3f "
           "improvement_db=%.3f expected_delay=%d measured_delay=%d "
           "preserve=%.3f corr=%.3f gain_avg=%.3f gain_min=%.3f "
           "gain_max=%.3f %s\n",
           target_snr_db,
           r->input_snr_db,
           r->output_snr_db,
           r->snr_improvement_db,
           SUBBAND_EXPECTED_DELAY,
           measured_delay,
           r->speech_preservation_ratio,
           r->clean_output_corr,
           r->gain_avg,
           r->gain_min,
           r->gain_max,
           fail ? "FAIL" : "PASS");

    return fail;
}

int SubbandEval_RunImpulseAndSilenceRobustnessTest(void)
{
    int i;
    int frames;
    int samples;
    int fail;

    frames = 16;
    samples = frames * SUBBAND_FRAME_LEN;
    memset(Eval_Input, 0, sizeof(Eval_Input));
    memset(Eval_Output, 0, sizeof(Eval_Output));
    Eval_Reset_For_Passthrough();
    Eval_Process_Frames(Eval_Input, Eval_Output, frames);
    fail = 0;
    for (i = 0; i < samples; i++)
    {
        if (Eval_Output[i] != 0)
        {
            fail = 1;
            break;
        }
    }

    memset(Eval_Input, 0, sizeof(Eval_Input));
    Eval_Input[4 * SUBBAND_FRAME_LEN + 21] = 28000;
    Eval_Input[4 * SUBBAND_FRAME_LEN + 22] = -28000;
    Eval_Reset_For_Passthrough();
    Eval_Process_Frames(Eval_Input, Eval_Output, frames);

    for (i = 0; i < samples; i++)
    {
        if ((Eval_Output[i] == 32767) || (Eval_Output[i] == -32768))
        {
            fail = 1;
        }
    }

    printf("eval_robustness %s\n", fail ? "FAIL" : "PASS");
    return fail;
}

static void Eval_Print_Band_Energy_Line(const char *name)
{
    int band;

    printf("band_energy %s", name);
    for (band = 0; band < SUBBAND_NUM_BANDS; band++)
    {
        printf(" Band%d_before=%.6f Band%d_after=%.6f",
               band, Eval_BandBefore[band], band, Eval_BandAfter[band]);
    }
    printf("\n");
}

static double Eval_Window_Bin_Energy(short *x, int start, int bin)
{
    int i;
    double re;
    double im;

    re = 0.0;
    im = 0.0;
    for (i = 0; i < SUBBAND_NFFT; i++)
    {
        double phase;
        double w;
        double v;

        phase = (2.0 * 3.14159265358979323846 * (double)i) /
                (double)SUBBAND_NFFT;
        w = 0.5 - 0.5 * cos(phase);
        phase = (2.0 * 3.14159265358979323846 * (double)bin * (double)i) /
                (double)SUBBAND_NFFT;
        v = (double)x[start + i] * w;
        re += v * cos(phase);
        im -= v * sin(phase);
    }

    return re * re + im * im;
}

static void Eval_Compute_Band_Energy(short *before, short *after,
                                     int count, int delay, int start)
{
    int band;
    int pos;
    int windows;
    int width;

    width = (SUBBAND_NFFT / 2) / SUBBAND_NUM_BANDS;
    for (band = 0; band < SUBBAND_NUM_BANDS; band++)
    {
        Eval_BandBefore[band] = 0.0f;
        Eval_BandAfter[band] = 0.0f;
    }

    windows = 0;
    for (pos = start; pos + delay + SUBBAND_NFFT < count - EVAL_SKIP_TAIL;
         pos += SUBBAND_NFFT)
    {
        for (band = 0; band < SUBBAND_NUM_BANDS; band++)
        {
            int center;
            int bin;
            double before_energy;
            double after_energy;

            center = band * width + width / 2;
            if (center < 1)
            {
                center = 1;
            }
            if (center > SUBBAND_NFFT / 2 - 1)
            {
                center = SUBBAND_NFFT / 2 - 1;
            }

            before_energy = 0.0;
            after_energy = 0.0;
            for (bin = center - 1; bin <= center + 1; bin++)
            {
                before_energy += Eval_Window_Bin_Energy(before, pos, bin);
                after_energy += Eval_Window_Bin_Energy(after, pos + delay, bin);
            }
            Eval_BandBefore[band] += (float)before_energy;
            Eval_BandAfter[band] += (float)after_energy;
        }
        windows++;
    }

    if (windows > 0)
    {
        for (band = 0; band < SUBBAND_NUM_BANDS; band++)
        {
            Eval_BandBefore[band] /= (float)windows;
            Eval_BandAfter[band] /= (float)windows;
        }
    }
}

int SubbandEval_OfflineTest_All(void)
{
    int failures;
    int snr5_fail;
    int snr10_fail;
    int snr20_fail;
    SubbandEvalResult wola;
    SubbandEvalResult learning;
    SubbandEvalResult noise;
    SubbandEvalResult snr5;
    SubbandEvalResult snr10;
    SubbandEvalResult snr20;
    FILE *csv;
    const char *audio_dir;

    failures = 0;
    snr5_fail = 0;
    snr10_fail = 0;
    snr20_fail = 0;
    audio_dir = SUBBAND_EVAL_AUDIO_DIR;

    csv = fopen("subband_eval_report.csv", "w");
    Eval_Write_Csv_Header(csv);

    failures += SubbandEval_RunWolaPassthroughTest(&wola);
    Eval_Write_Csv_Row(csv, "wola_passthrough", &wola);

    failures += SubbandEval_RunNoiseLearningTest(&learning);
    Eval_Write_Csv_Row(csv, "noise_learning", &learning);

    failures += SubbandEval_RunNoiseSuppressionTest(&noise, audio_dir);
    Eval_Write_Csv_Row(csv, "noise_only", &noise);

    snr5_fail = SubbandEval_RunSNRImprovementTest(5.0f, &snr5, audio_dir);
    snr10_fail = SubbandEval_RunSNRImprovementTest(10.0f, &snr10, audio_dir);
    snr20_fail = SubbandEval_RunSNRImprovementTest(20.0f, &snr20, audio_dir);
    failures += snr5_fail;
    failures += snr10_fail;
    failures += snr20_fail;
    Eval_Write_Csv_Row(csv, "snr5", &snr5);
    Eval_Write_Csv_Row(csv, "snr10", &snr10);
    Eval_Write_Csv_Row(csv, "snr20", &snr20);

    failures += SubbandEval_RunImpulseAndSilenceRobustnessTest();

    if (csv != 0)
    {
        fclose(csv);
    }

    printf("CSV report: subband_eval_report.csv\n");
    printf("Eval audio dir: %s\n", audio_dir);
    SubbandEval_PrintReport(&wola);
    SubbandEval_PrintReport(&learning);
    SubbandEval_PrintReport(&noise);
    SubbandEval_PrintReport(&snr5);
    SubbandEval_PrintReport(&snr10);
    SubbandEval_PrintReport(&snr20);
    printf("SubbandEval_OfflineTest_All failures=%d snr5=%s snr10=%s snr20=%s\n",
           failures,
           snr5_fail ? "FAIL" : "PASS",
           snr10_fail ? "FAIL" : "PASS",
           snr20_fail ? "FAIL" : "PASS");

    return failures;
}

#else

void SubbandEval_PrintReport(const SubbandEvalResult *r)
{
    (void)r;
}

int SubbandEval_RunWolaPassthroughTest(SubbandEvalResult *r)
{
    (void)r;
    return 0;
}

int SubbandEval_RunNoiseLearningTest(SubbandEvalResult *r)
{
    (void)r;
    return 0;
}

int SubbandEval_RunNoiseSuppressionTest(SubbandEvalResult *r,
                                        const char *audio_dir)
{
    (void)r;
    (void)audio_dir;
    return 0;
}

int SubbandEval_RunSNRImprovementTest(float target_snr_db,
                                      SubbandEvalResult *r,
                                      const char *audio_dir)
{
    (void)target_snr_db;
    (void)r;
    (void)audio_dir;
    return 0;
}

int SubbandEval_RunImpulseAndSilenceRobustnessTest(void)
{
    return 0;
}

int SubbandEval_OfflineTest_All(void)
{
    return 0;
}

#endif
