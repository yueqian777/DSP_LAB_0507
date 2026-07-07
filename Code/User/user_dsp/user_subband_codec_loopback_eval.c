/**
 * user_subband_codec_loopback_eval.c
 *
 * PC algo-only evaluation for WOLA frequency-domain codec loopback.
 */

#include "user_subband_codec_loopback_eval.h"

#ifdef SUBBAND_ALGO_ONLY

#include "user_subband_codec_loopback.h"
#include "user_subband_wola.h"
#include "user_subband_denoise.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define LOOPBACK_EVAL_FRAMES 160
#define LOOPBACK_EVAL_SAMPLES (LOOPBACK_EVAL_FRAMES * SUBBAND_FRAME_LEN)
#define LOOPBACK_EVAL_SKIP (2 * SUBBAND_NFFT)
#define LOOPBACK_EVAL_DELAY SUBBAND_EXPECTED_DELAY
#define LOOPBACK_EVAL_REPORT "subband_codec_loopback_eval_report.csv"

typedef struct
{
    const char *mode_name;
    int target_bitrate_kbps;
    float estimated_bitrate_kbps;
    float compression_ratio;
    float output_snr_db;
    float speechband_snr_0_8k_db;
    float clean_output_corr;
    float speech_preservation_ratio;
    float avg_bits_per_scalar;
    float band_bits[SUBBAND_NUM_BANDS];
    int invalid_count;
    int clipping_count;
    int nonzero_count;
    int pass;
} LoopbackEvalResult;

static short LoopbackEval_Clean[LOOPBACK_EVAL_SAMPLES];
static short LoopbackEval_Out[LOOPBACK_EVAL_SAMPLES];

static void LoopbackEval_Fill_Speechlike(short *dst, int count)
{
    int i;

    for (i = 0; i < count; i++)
    {
        float t;
        float env;
        float sample;

        t = (float)i / SUBBAND_SAMPLE_RATE;
        env = 0.70f + 0.30f *
              sinf(2.0f * 3.14159265358979323846f * 2.7f * t);
        sample =
            5200.0f * sinf(2.0f * 3.14159265358979323846f * 220.0f * t) +
            3800.0f * sinf(2.0f * 3.14159265358979323846f * 720.0f * t) +
            2600.0f * sinf(2.0f * 3.14159265358979323846f * 1450.0f * t) +
            1500.0f * sinf(2.0f * 3.14159265358979323846f * 2850.0f * t) +
            900.0f * sinf(2.0f * 3.14159265358979323846f * 5200.0f * t) +
            450.0f * sinf(2.0f * 3.14159265358979323846f * 6900.0f * t);
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

static int LoopbackEval_Is_Invalid_Float(float x)
{
    if (x != x)
    {
        return 1;
    }
    if ((x > 1.0e30f) || (x < -1.0e30f))
    {
        return 1;
    }
    return 0;
}

static int LoopbackEval_Nonzero_Count(const short *x, int count)
{
    int i;
    int nonzero;

    nonzero = 0;
    for (i = 0; i < count; i++)
    {
        if (x[i] != 0)
        {
            nonzero++;
        }
    }
    return nonzero;
}

static void LoopbackEval_Compute_Metrics(const short *clean,
                                         const short *out,
                                         int count,
                                         LoopbackEvalResult *result)
{
    int i;
    int start;
    int end;
    double ref_energy;
    double err_energy;
    double out_energy;
    double dot;

    start = LOOPBACK_EVAL_SKIP;
    end = count - LOOPBACK_EVAL_SKIP - LOOPBACK_EVAL_DELAY;
    ref_energy = 0.0;
    err_energy = 0.0;
    out_energy = 0.0;
    dot = 0.0;

    for (i = start; i < end; i++)
    {
        double x;
        double y;
        double e;

        x = (double)clean[i];
        y = (double)out[i + LOOPBACK_EVAL_DELAY];
        e = y - x;
        ref_energy += x * x;
        out_energy += y * y;
        err_energy += e * e;
        dot += x * y;
    }

    result->output_snr_db =
        (float)(10.0 * log10((ref_energy + 1.0e-20) /
                             (err_energy + 1.0e-20)));
    result->speechband_snr_0_8k_db = result->output_snr_db;
    result->speech_preservation_ratio =
        (float)(dot / (ref_energy + 1.0e-20));
    result->clean_output_corr =
        (float)(dot / (sqrt(ref_energy * out_energy) + 1.0e-20));
    result->nonzero_count = LoopbackEval_Nonzero_Count(out, count);
}

static void LoopbackEval_Capture_Debug(LoopbackEvalResult *result)
{
    result->estimated_bitrate_kbps =
        SUBBAND_CODEC_LOOP_DebugEstimatedBitrateKbps;
    result->compression_ratio =
        SUBBAND_CODEC_LOOP_DebugCompressionRatio;
    result->avg_bits_per_scalar =
        SUBBAND_CODEC_LOOP_DebugAvgBitsPerScalar;
    result->band_bits[0] = SUBBAND_CODEC_LOOP_DebugBandBits0;
    result->band_bits[1] = SUBBAND_CODEC_LOOP_DebugBandBits1;
    result->band_bits[2] = SUBBAND_CODEC_LOOP_DebugBandBits2;
    result->band_bits[3] = SUBBAND_CODEC_LOOP_DebugBandBits3;
    result->band_bits[4] = SUBBAND_CODEC_LOOP_DebugBandBits4;
    result->band_bits[5] = SUBBAND_CODEC_LOOP_DebugBandBits5;
    result->band_bits[6] = SUBBAND_CODEC_LOOP_DebugBandBits6;
    result->band_bits[7] = SUBBAND_CODEC_LOOP_DebugBandBits7;
    result->invalid_count = SUBBAND_CODEC_LOOP_DebugInvalidCount;
    result->clipping_count = SUBBAND_CODEC_LOOP_DebugClippingCount;
}

static void LoopbackEval_Reset_For_Codec(int target_bitrate_kbps)
{
    SubbandWOLA_Init();
    SubbandWOLA_SetBypass(0);
    SubbandWOLA_ResetStream();
    SubbandWOLA_ResetAllGains();
    SubbandDenoise_Reset();
    SubbandDenoise_StopLearning();
    SubbandDenoise_SetEnabled(0);
    SubbandCodecLoopback_Reset();
    SubbandCodecLoopback_SetTargetKbps(target_bitrate_kbps);
    SubbandCodecLoopback_SetEnabled(1);
}

static void LoopbackEval_Process_Frames(const short *input, short *output,
                                        int frames)
{
    int frame;

    for (frame = 0; frame < frames; frame++)
    {
        SubbandWOLA_ProcessFrame((short *)&input[frame * SUBBAND_FRAME_LEN],
                                 &output[frame * SUBBAND_FRAME_LEN]);
    }
}

static int LoopbackEval_Rate_Close(float actual, int target)
{
    float diff;
    float limit;

    diff = actual - (float)target;
    if (diff < 0.0f)
    {
        diff = -diff;
    }
    limit = (float)target * 0.20f;
    if (limit < 30.0f)
    {
        limit = 30.0f;
    }
    return diff <= limit;
}

static void LoopbackEval_Run_Case(const char *mode_name,
                                  int target_bitrate_kbps,
                                  LoopbackEvalResult *result)
{
    memset(result, 0, sizeof(*result));
    result->mode_name = mode_name;
    result->target_bitrate_kbps = target_bitrate_kbps;

    memset(LoopbackEval_Out, 0, sizeof(LoopbackEval_Out));
    LoopbackEval_Reset_For_Codec(target_bitrate_kbps);
    LoopbackEval_Process_Frames(LoopbackEval_Clean, LoopbackEval_Out,
                                LOOPBACK_EVAL_FRAMES);
    LoopbackEval_Compute_Metrics(LoopbackEval_Clean, LoopbackEval_Out,
                                 LOOPBACK_EVAL_SAMPLES, result);
    LoopbackEval_Capture_Debug(result);

    result->pass = 1;
    if (result->invalid_count != 0)
    {
        result->pass = 0;
    }
    if (result->nonzero_count <= 0)
    {
        result->pass = 0;
    }
    if (LoopbackEval_Rate_Close(result->estimated_bitrate_kbps,
                                target_bitrate_kbps) == 0)
    {
        result->pass = 0;
    }
    if (result->compression_ratio <= 1.0f)
    {
        result->pass = 0;
    }
    if ((LoopbackEval_Is_Invalid_Float(result->output_snr_db) != 0) ||
        (LoopbackEval_Is_Invalid_Float(result->clean_output_corr) != 0) ||
        (LoopbackEval_Is_Invalid_Float(result->speech_preservation_ratio) != 0))
    {
        result->pass = 0;
    }
}

static void LoopbackEval_Write_Header(FILE *fp)
{
    fprintf(fp, "mode_name,target_bitrate_kbps,estimated_bitrate_kbps,"
                "compression_ratio,output_snr_db,speechband_snr_0_8k_db,"
                "clean_output_corr,speech_preservation_ratio,"
                "avg_bits_per_scalar,band_bits_0,band_bits_1,band_bits_2,"
                "band_bits_3,band_bits_4,band_bits_5,band_bits_6,"
                "band_bits_7,invalid_count,clipping_count,pass\n");
}

static void LoopbackEval_Write_Row(FILE *fp,
                                   const LoopbackEvalResult *result)
{
    fprintf(fp,
            "%s,%d,%.3f,%.3f,%.3f,%.3f,%.6f,%.6f,%.3f,"
            "%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%d,%d,%d\n",
            result->mode_name,
            result->target_bitrate_kbps,
            result->estimated_bitrate_kbps,
            result->compression_ratio,
            result->output_snr_db,
            result->speechband_snr_0_8k_db,
            result->clean_output_corr,
            result->speech_preservation_ratio,
            result->avg_bits_per_scalar,
            result->band_bits[0],
            result->band_bits[1],
            result->band_bits[2],
            result->band_bits[3],
            result->band_bits[4],
            result->band_bits[5],
            result->band_bits[6],
            result->band_bits[7],
            result->invalid_count,
            result->clipping_count,
            result->pass);
}

int SubbandCodecLoopbackEval_OfflineTest_All(void)
{
    int failures;
    LoopbackEvalResult r160;
    LoopbackEvalResult r240;
    LoopbackEvalResult r320;
    FILE *fp;

    failures = 0;
    SubbandCodecLoopback_Init();
    SubbandCodecLoopback_Reset();

    SubbandCodecLoopback_SetEnabled(0);
    if (SubbandCodecLoopback_IsEnabled() != 0)
    {
        failures++;
    }

    SubbandCodecLoopback_SetTargetKbps(160);
    if (SubbandCodecLoopback_GetTargetKbps() != 160)
    {
        failures++;
    }

    printf("codec_loopback_eval api_smoke %s\n",
           failures ? "FAIL" : "PASS");

    LoopbackEval_Fill_Speechlike(LoopbackEval_Clean,
                                 LOOPBACK_EVAL_SAMPLES);
    LoopbackEval_Run_Case("codec_loopback_160k", 160, &r160);
    LoopbackEval_Run_Case("codec_loopback_240k", 240, &r240);
    LoopbackEval_Run_Case("codec_loopback_320k", 320, &r320);

    if (r240.speechband_snr_0_8k_db < r160.speechband_snr_0_8k_db + 0.50f)
    {
        r240.pass = 0;
    }
    if (r320.speechband_snr_0_8k_db + 0.10f < r240.speechband_snr_0_8k_db)
    {
        r320.pass = 0;
    }

    fp = fopen(LOOPBACK_EVAL_REPORT, "w");
    if (fp == 0)
    {
        printf("codec_loopback_eval report_open FAIL path=%s\n",
               LOOPBACK_EVAL_REPORT);
        failures++;
    }
    else
    {
        LoopbackEval_Write_Header(fp);
        LoopbackEval_Write_Row(fp, &r160);
        LoopbackEval_Write_Row(fp, &r240);
        LoopbackEval_Write_Row(fp, &r320);
        fclose(fp);
    }

    failures += (r160.pass == 0) ? 1 : 0;
    failures += (r240.pass == 0) ? 1 : 0;
    failures += (r320.pass == 0) ? 1 : 0;

    printf("codec_loopback_eval %s target=%d actual=%.3f snr=%.3f "
           "corr=%.6f preserve=%.6f invalid=%d clips=%d bits=[%.1f %.1f %.1f %.1f %.1f %.1f %.1f %.1f] %s\n",
           r160.mode_name, r160.target_bitrate_kbps,
           r160.estimated_bitrate_kbps, r160.speechband_snr_0_8k_db,
           r160.clean_output_corr, r160.speech_preservation_ratio,
           r160.invalid_count, r160.clipping_count,
           r160.band_bits[0], r160.band_bits[1], r160.band_bits[2],
           r160.band_bits[3], r160.band_bits[4], r160.band_bits[5],
           r160.band_bits[6], r160.band_bits[7],
           r160.pass ? "PASS" : "FAIL");
    printf("codec_loopback_eval %s target=%d actual=%.3f snr=%.3f "
           "corr=%.6f preserve=%.6f invalid=%d clips=%d bits=[%.1f %.1f %.1f %.1f %.1f %.1f %.1f %.1f] %s\n",
           r240.mode_name, r240.target_bitrate_kbps,
           r240.estimated_bitrate_kbps, r240.speechband_snr_0_8k_db,
           r240.clean_output_corr, r240.speech_preservation_ratio,
           r240.invalid_count, r240.clipping_count,
           r240.band_bits[0], r240.band_bits[1], r240.band_bits[2],
           r240.band_bits[3], r240.band_bits[4], r240.band_bits[5],
           r240.band_bits[6], r240.band_bits[7],
           r240.pass ? "PASS" : "FAIL");
    printf("codec_loopback_eval %s target=%d actual=%.3f snr=%.3f "
           "corr=%.6f preserve=%.6f invalid=%d clips=%d bits=[%.1f %.1f %.1f %.1f %.1f %.1f %.1f %.1f] %s\n",
           r320.mode_name, r320.target_bitrate_kbps,
           r320.estimated_bitrate_kbps, r320.speechband_snr_0_8k_db,
           r320.clean_output_corr, r320.speech_preservation_ratio,
           r320.invalid_count, r320.clipping_count,
           r320.band_bits[0], r320.band_bits[1], r320.band_bits[2],
           r320.band_bits[3], r320.band_bits[4], r320.band_bits[5],
           r320.band_bits[6], r320.band_bits[7],
           r320.pass ? "PASS" : "FAIL");
    printf("codec_loopback_eval report=%s failures=%d\n",
           LOOPBACK_EVAL_REPORT, failures);

    return failures;
}

#else

int SubbandCodecLoopbackEval_OfflineTest_All(void)
{
    return 0;
}

#endif /* SUBBAND_ALGO_ONLY */
