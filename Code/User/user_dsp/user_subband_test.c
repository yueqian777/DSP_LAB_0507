/**
 * user_subband_test.c
 *
 * PC algo-only tests for the WOLA subband processor.
 */

#include "user_subband_wola.h"
#include "user_subband_test.h"
#include "user_subband_denoise.h"
#include "user_subband_eval.h"

#ifdef SUBBAND_ALGO_ONLY
#include <math.h>
#include <stdio.h>
#include <string.h>

#define TEST_FRAMES 24
#define TEST_SAMPLES (TEST_FRAMES * SUBBAND_FRAME_LEN)
#define TEST_SKIP_SAMPLES (4 * SUBBAND_FRAME_LEN)
#define TEST_TAIL_SKIP_SAMPLES (2 * SUBBAND_NFFT)
#define TEST_MAX_LAG SUBBAND_NFFT

static short Test_Input[SUBBAND_FRAME_LEN];
static short Test_Output[SUBBAND_FRAME_LEN];
static short Test_Stream_In[TEST_SAMPLES];
static short Test_Stream_Out[TEST_SAMPLES];
static float Test_Float_In[TEST_SAMPLES];
static float Test_Float_Out[TEST_SAMPLES];
static int SubbandWOLA_TestFailures = 0;

static void Set_All_Gains(float gain)
{
    int band;

    for (band = 0; band < SUBBAND_NUM_BANDS; band++)
    {
        SubbandWOLA_SetBandGain(band, gain);
    }
}

static void Reset_WOLA_Test_State(void)
{
    SubbandWOLA_Init();
    SubbandWOLA_ResetStream();
    SubbandWOLA_ResetAllGains();
    SubbandDenoise_Reset();
    SubbandDenoise_SetEnabled(0);
}

static void Fill_Sine(short *dst, float freq_hz, float amplitude)
{
    int i;

    for (i = 0; i < SUBBAND_FRAME_LEN; i++)
    {
        float phase = (2.0f * 3.14159265358979323846f * freq_hz * (float)i) /
                      SUBBAND_SAMPLE_RATE;
        dst[i] = (short)(amplitude * sinf(phase));
    }
}

static void Fill_Sine_Stream(short *dst, int count, float freq_hz, float amplitude)
{
    int i;

    for (i = 0; i < count; i++)
    {
        float phase = (2.0f * 3.14159265358979323846f * freq_hz * (float)i) /
                      SUBBAND_SAMPLE_RATE;
        dst[i] = (short)(amplitude * sinf(phase));
    }
}

static void Fill_Noise_Stream(short *dst, int count, unsigned long seed, float amplitude)
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

static void Fill_Float_Noise(float *dst, int count, unsigned long seed, float amplitude)
{
    int i;
    unsigned long state;

    state = seed;
    for (i = 0; i < count; i++)
    {
        float centered;

        state = (1103515245UL * state + 12345UL) & 0x7fffffffUL;
        centered = ((float)state / 1073741824.0f) - 1.0f;
        dst[i] = amplitude * centered;
    }
}

static void Process_Stream(short *src, short *dst, int frames)
{
    int frame;

    for (frame = 0; frame < frames; frame++)
    {
        SubbandWOLA_ProcessFrame(&src[frame * SUBBAND_FRAME_LEN],
                                 &dst[frame * SUBBAND_FRAME_LEN]);
    }
}

static void Compute_Aligned_Metrics(short *src, short *dst, int count,
                                    SubbandWOLATestMetrics *metrics)
{
    int lag;
    int best_lag;
    int i;
    int start;
    double best_snr;
    double best_gain;
    double input_energy;
    double output_energy;
    double raw_error;

    start = TEST_SKIP_SAMPLES;
    best_snr = -1.0e30;
    best_gain = 0.0;
    best_lag = 0;

    for (lag = 0; lag <= TEST_MAX_LAG; lag++)
    {
        double xy;
        double xx;
        double yy;
        double ee;
        double gain;
        double snr;

        xy = 0.0;
        xx = 0.0;
        yy = 0.0;
        ee = 0.0;

        for (i = start; i < count - lag - TEST_TAIL_SKIP_SAMPLES; i++)
        {
            double x;
            double y;

            x = (double)src[i];
            y = (double)dst[i + lag];
            xy += x * y;
            xx += x * x;
            yy += y * y;
        }

        if (xx > 1.0e-20)
        {
            gain = xy / xx;
        }
        else
        {
            gain = 0.0;
        }

        for (i = start; i < count - lag - TEST_TAIL_SKIP_SAMPLES; i++)
        {
            double x;
            double y;
            double d;

            x = (double)src[i];
            y = (double)dst[i + lag];
            d = y - gain * x;
            ee += d * d;
        }

        snr = 10.0 * log10((yy + 1.0e-20) / (ee + 1.0e-20));
        if (snr > best_snr)
        {
            best_snr = snr;
            best_gain = gain;
            best_lag = lag;
        }
    }

    input_energy = 0.0;
    output_energy = 0.0;
    raw_error = 0.0;
    metrics->max_error = 0.0;

    for (i = start; i < count - best_lag - TEST_TAIL_SKIP_SAMPLES; i++)
    {
        double x;
        double y;
        double d;
        double ad;

        x = (double)src[i];
        y = (double)dst[i + best_lag];
        d = y - best_gain * x;
        ad = fabs(d);
        if (ad > metrics->max_error)
        {
            metrics->max_error = ad;
        }
        raw_error += d * d;
        input_energy += x * x;
        output_energy += y * y;
    }

    metrics->mse = raw_error /
                   (double)(count - best_lag - start - TEST_TAIL_SKIP_SAMPLES);
    metrics->input_energy = input_energy;
    metrics->output_energy_ratio = output_energy / (input_energy + 1.0e-20);
    metrics->aligned_lag = best_lag;
    metrics->aligned_gain = best_gain;
    metrics->aligned_snr_db = best_snr;
}

static void Compute_Aligned_Float_Metrics(float *src, float *dst, int count,
                                          SubbandWOLATestMetrics *metrics)
{
    int lag;
    int best_lag;
    int i;
    int start;
    double best_snr;
    double best_gain;
    double input_energy;
    double output_energy;
    double raw_error;

    start = TEST_SKIP_SAMPLES;
    best_snr = -1.0e30;
    best_gain = 0.0;
    best_lag = 0;

    for (lag = 0; lag <= TEST_MAX_LAG; lag++)
    {
        double xy;
        double xx;
        double yy;
        double ee;
        double gain;
        double snr;

        xy = 0.0;
        xx = 0.0;
        yy = 0.0;
        ee = 0.0;

        for (i = start; i < count - lag - TEST_TAIL_SKIP_SAMPLES; i++)
        {
            double x;
            double y;

            x = (double)src[i];
            y = (double)dst[i + lag];
            xy += x * y;
            xx += x * x;
            yy += y * y;
        }

        if (xx > 1.0e-20)
        {
            gain = xy / xx;
        }
        else
        {
            gain = 0.0;
        }

        for (i = start; i < count - lag - TEST_TAIL_SKIP_SAMPLES; i++)
        {
            double x;
            double y;
            double d;

            x = (double)src[i];
            y = (double)dst[i + lag];
            d = y - gain * x;
            ee += d * d;
        }

        snr = 10.0 * log10((yy + 1.0e-20) / (ee + 1.0e-20));
        if (snr > best_snr)
        {
            best_snr = snr;
            best_gain = gain;
            best_lag = lag;
        }
    }

    input_energy = 0.0;
    output_energy = 0.0;
    raw_error = 0.0;
    metrics->max_error = 0.0;

    for (i = start; i < count - best_lag - TEST_TAIL_SKIP_SAMPLES; i++)
    {
        double x;
        double y;
        double d;
        double ad;

        x = (double)src[i];
        y = (double)dst[i + best_lag];
        d = y - best_gain * x;
        ad = fabs(d);
        if (ad > metrics->max_error)
        {
            metrics->max_error = ad;
        }
        raw_error += d * d;
        input_energy += x * x;
        output_energy += y * y;
    }

    metrics->mse = raw_error /
                   (double)(count - best_lag - start - TEST_TAIL_SKIP_SAMPLES);
    metrics->input_energy = input_energy;
    metrics->output_energy_ratio = output_energy / (input_energy + 1.0e-20);
    metrics->aligned_lag = best_lag;
    metrics->aligned_gain = best_gain;
    metrics->aligned_snr_db = best_snr;
}

static int Run_Bypass_Sine_Test(void)
{
    int i;

    Fill_Sine(Test_Input, 1000.0f, 12000.0f);
    memset(Test_Output, 0, sizeof(Test_Output));

    Reset_WOLA_Test_State();
    SubbandWOLA_SetBypass(1);
    SubbandWOLA_ProcessFrame(Test_Input, Test_Output);

    for (i = 0; i < SUBBAND_FRAME_LEN; i++)
    {
        if (Test_Output[i] != Test_Input[i])
        {
            printf("bypass_sine FAIL index=%d in=%d out=%d\n",
                   i, Test_Input[i], Test_Output[i]);
            return 1;
        }
    }

    printf("bypass_sine PASS\n");
    return 0;
}

static int Run_Bypass_Noise_Test(void)
{
    int i;

    Fill_Noise_Stream(Test_Input, SUBBAND_FRAME_LEN, 7UL, 12000.0f);
    memset(Test_Output, 0, sizeof(Test_Output));

    Reset_WOLA_Test_State();
    SubbandWOLA_SetBypass(1);
    SubbandWOLA_ProcessFrame(Test_Input, Test_Output);

    for (i = 0; i < SUBBAND_FRAME_LEN; i++)
    {
        if (Test_Output[i] != Test_Input[i])
        {
            printf("bypass_noise FAIL index=%d in=%d out=%d\n",
                   i, Test_Input[i], Test_Output[i]);
            return 1;
        }
    }

    printf("bypass_noise PASS\n");
    return 0;
}

static int Run_COLA_Test(void)
{
    float max_error;

    Reset_WOLA_Test_State();
    max_error = SubbandWOLA_GetCOLAMaxError();
    printf("cola_check max_cola_error=%.9g\n", max_error);

    if (max_error >= 1.0e-5f)
    {
        printf("cola_check FAIL\n");
        return 1;
    }

    return 0;
}

static int Run_Impulse_Delay_Test(void)
{
    int i;
    int impulse_index;
    int peak_index;
    int aligned_lag;
    short peak_value;
    int peak_abs;

    memset(Test_Stream_In, 0, sizeof(Test_Stream_In));
    memset(Test_Stream_Out, 0, sizeof(Test_Stream_Out));

    impulse_index = TEST_SKIP_SAMPLES + 64;
    Test_Stream_In[impulse_index] = 12000;

    Reset_WOLA_Test_State();
    SubbandWOLA_SetBypass(0);
    Set_All_Gains(1.0f);
    Process_Stream(Test_Stream_In, Test_Stream_Out, TEST_FRAMES);

    peak_index = 0;
    peak_value = 0;
    peak_abs = 0;
    for (i = TEST_SKIP_SAMPLES; i < TEST_SAMPLES - TEST_TAIL_SKIP_SAMPLES; i++)
    {
        int value_abs;

        value_abs = (int)Test_Stream_Out[i];
        if (value_abs < 0)
        {
            value_abs = -value_abs;
        }

        if (value_abs > peak_abs)
        {
            peak_abs = value_abs;
            peak_index = i;
            peak_value = Test_Stream_Out[i];
        }
    }

    aligned_lag = peak_index - impulse_index;
    printf("impulse_test impulse_peak_index=%d impulse_peak_value=%d aligned_lag=%d "
           "expected_delay=%d\n",
           peak_index, peak_value, aligned_lag, SUBBAND_EXPECTED_DELAY);

    if ((aligned_lag != SUBBAND_EXPECTED_DELAY) ||
        (peak_value < 11990) ||
        (peak_value > 12010))
    {
        printf("impulse_test FAIL\n");
        return 1;
    }

    return 0;
}

static float Frame_Energy(short *x)
{
    int i;
    float energy;

    energy = 0.0f;
    for (i = 0; i < SUBBAND_FRAME_LEN; i++)
    {
        float v;

        v = (float)x[i];
        energy += v * v;
    }

    return energy;
}

static double Aligned_Energy_Ratio(short *src, short *dst, int count, int delay)
{
    int i;
    double input_energy;
    double output_energy;

    input_energy = 0.0;
    output_energy = 0.0;

    for (i = TEST_SKIP_SAMPLES; i < count - delay - TEST_TAIL_SKIP_SAMPLES; i++)
    {
        double x;
        double y;

        x = (double)src[i];
        y = (double)dst[i + delay];
        input_energy += x * x;
        output_energy += y * y;
    }

    return output_energy / (input_energy + 1.0e-20);
}

static int Run_Band0_Rejects_10k_Test(void)
{
    float input_energy;
    float output_energy;
    float ratio;
    int frame;

    Reset_WOLA_Test_State();
    SubbandWOLA_SetBypass(0);
    Set_All_Gains(0.0f);
    SubbandWOLA_SetBandGain(0, 1.0f);

    output_energy = 0.0f;
    input_energy = 0.0f;

    for (frame = 0; frame < 16; frame++)
    {
        Fill_Sine(Test_Input, 10000.0f, 12000.0f);
        memset(Test_Output, 0, sizeof(Test_Output));
        SubbandWOLA_ProcessFrame(Test_Input, Test_Output);

        if (frame >= 4)
        {
            input_energy += Frame_Energy(Test_Input);
            output_energy += Frame_Energy(Test_Output);
        }
    }

    ratio = output_energy / (input_energy + 1.0e-20f);
    if (ratio > 0.05f)
    {
        printf("band0_rejects_10k FAIL ratio=%f\n", ratio);
        return 1;
    }

    printf("band0_rejects_10k PASS ratio=%f\n", ratio);
    return 0;
}

static int Run_WOLA_Passthrough_Test(const char *name, float freq_hz, int is_noise)
{
    SubbandWOLATestMetrics metrics;

    memset(Test_Stream_Out, 0, sizeof(Test_Stream_Out));
    if (is_noise != 0)
    {
        Fill_Noise_Stream(Test_Stream_In, TEST_SAMPLES, 123UL, 10000.0f);
    }
    else
    {
        Fill_Sine_Stream(Test_Stream_In, TEST_SAMPLES, freq_hz, 12000.0f);
    }

    Reset_WOLA_Test_State();
    SubbandWOLA_SetBypass(0);
    Set_All_Gains(1.0f);
    Process_Stream(Test_Stream_In, Test_Stream_Out, TEST_FRAMES);
    Compute_Aligned_Metrics(Test_Stream_In, Test_Stream_Out, TEST_SAMPLES, &metrics);

    printf("wola_passthrough %-10s max_error=%.3f mse=%.6f energy_ratio=%.9f "
           "aligned_lag=%d aligned_gain=%.9f aligned_snr_db=%.3f\n",
           name,
           metrics.max_error,
           metrics.mse,
           metrics.output_energy_ratio,
           metrics.aligned_lag,
           metrics.aligned_gain,
           metrics.aligned_snr_db);

    if ((metrics.output_energy_ratio < 0.98) ||
        (metrics.output_energy_ratio > 1.02) ||
        (metrics.aligned_snr_db < 60.0))
    {
        printf("wola_passthrough %s FAIL\n", name);
        return 1;
    }

    return 0;
}

static int Run_WOLA_Float_Reconstruction_Test(void)
{
    int frame;
    SubbandWOLATestMetrics metrics;

    Fill_Float_Noise(Test_Float_In, TEST_SAMPLES, 999UL, 10000.0f);
    memset(Test_Float_Out, 0, sizeof(Test_Float_Out));

    Reset_WOLA_Test_State();
    SubbandWOLA_SetBypass(0);
    Set_All_Gains(1.0f);

    for (frame = 0; frame < TEST_FRAMES; frame++)
    {
        SubbandWOLA_ProcessFrameFloat(&Test_Float_In[frame * SUBBAND_FRAME_LEN],
                                      &Test_Float_Out[frame * SUBBAND_FRAME_LEN]);
    }

    Compute_Aligned_Float_Metrics(Test_Float_In, Test_Float_Out, TEST_SAMPLES, &metrics);
    printf("wola_float_noise max_error=%.6f mse=%.12f energy_ratio=%.9f "
           "aligned_lag=%d aligned_gain=%.9f aligned_snr_db=%.3f\n",
           metrics.max_error,
           metrics.mse,
           metrics.output_energy_ratio,
           metrics.aligned_lag,
           metrics.aligned_gain,
           metrics.aligned_snr_db);

    if ((metrics.output_energy_ratio < 0.999) ||
        (metrics.output_energy_ratio > 1.001) ||
        (metrics.aligned_snr_db < 90.0))
    {
        printf("wola_float_noise FAIL\n");
        return 1;
    }

    return 0;
}

static int Run_Single_Band_Retention_Test(void)
{
    int target_band;
    int failures;
    int width;

    failures = 0;
    width = (SUBBAND_NFFT / 2) / SUBBAND_NUM_BANDS;

    for (target_band = 0; target_band < SUBBAND_NUM_BANDS; target_band++)
    {
        int source_band;
        double main_band_energy_ratio;
        double max_adjacent_band_leakage;
        double max_outband_leakage;

        main_band_energy_ratio = 0.0;
        max_adjacent_band_leakage = 0.0;
        max_outband_leakage = 0.0;

        for (source_band = 0; source_band < SUBBAND_NUM_BANDS; source_band++)
        {
            int center_bin;
            int band_distance;
            float freq_hz;
            double ratio;

            center_bin = source_band * width + width / 2;
            if (source_band == SUBBAND_NUM_BANDS - 1)
            {
                center_bin = SUBBAND_NFFT / 2 - width / 2;
            }
            freq_hz = ((float)center_bin * SUBBAND_SAMPLE_RATE) / (float)SUBBAND_NFFT;

            Fill_Sine_Stream(Test_Stream_In, TEST_SAMPLES, freq_hz, 12000.0f);
            memset(Test_Stream_Out, 0, sizeof(Test_Stream_Out));

            Reset_WOLA_Test_State();
            SubbandWOLA_SetBypass(0);
            Set_All_Gains(0.0f);
            SubbandWOLA_SetBandGain(target_band, 1.0f);
            Process_Stream(Test_Stream_In, Test_Stream_Out, TEST_FRAMES);

            ratio = Aligned_Energy_Ratio(Test_Stream_In, Test_Stream_Out,
                                         TEST_SAMPLES, SUBBAND_EXPECTED_DELAY);

            band_distance = source_band - target_band;
            if (band_distance < 0)
            {
                band_distance = -band_distance;
            }

            if (source_band == target_band)
            {
                main_band_energy_ratio = ratio;
            }
            else if (band_distance == 1)
            {
                if (ratio > max_adjacent_band_leakage)
                {
                    max_adjacent_band_leakage = ratio;
                }
            }
            else
            {
                if (ratio > max_outband_leakage)
                {
                    max_outband_leakage = ratio;
                }
            }
        }

        printf("single_band target=%d main_band_energy_ratio=%.9f "
               "max_adjacent_band_leakage=%.9f max_outband_leakage=%.9f\n",
               target_band,
               main_band_energy_ratio,
               max_adjacent_band_leakage,
               max_outband_leakage);

        if ((main_band_energy_ratio < 0.95) ||
            (max_adjacent_band_leakage > 0.01) ||
            (max_outband_leakage > 0.001))
        {
            failures++;
        }
    }

    if (failures != 0)
    {
        printf("single_band_retention FAIL failures=%d\n", failures);
        return 1;
    }

    printf("single_band_retention PASS\n");
    return 0;
}

static int Run_Gain_Perturbation_Test(void)
{
    static const float gain_sets[4][SUBBAND_NUM_BANDS] = {
        {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f},
        {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f},
        {1.0f, 0.5f, 0.2f, 1.0f, 1.0f, 0.2f, 0.5f, 1.0f}
    };
    int set_idx;

    Fill_Noise_Stream(Test_Stream_In, TEST_SAMPLES, 777UL, 10000.0f);

    for (set_idx = 0; set_idx < 4; set_idx++)
    {
        int band;
        SubbandWOLATestMetrics metrics;

        memset(Test_Stream_Out, 0, sizeof(Test_Stream_Out));
        Reset_WOLA_Test_State();
        SubbandWOLA_SetBypass(0);
        for (band = 0; band < SUBBAND_NUM_BANDS; band++)
        {
            SubbandWOLA_SetBandGain(band, gain_sets[set_idx][band]);
        }

        Process_Stream(Test_Stream_In, Test_Stream_Out, TEST_FRAMES);
        Compute_Aligned_Metrics(Test_Stream_In, Test_Stream_Out, TEST_SAMPLES, &metrics);

        printf("gain_set%d energy_ratio=%.9f aligned_lag=%d aligned_gain=%.9f "
               "aligned_snr_db=%.3f gains=[%.1f %.1f %.1f %.1f %.1f %.1f %.1f %.1f]\n",
               set_idx,
               metrics.output_energy_ratio,
               metrics.aligned_lag,
               metrics.aligned_gain,
               metrics.aligned_snr_db,
               gain_sets[set_idx][0],
               gain_sets[set_idx][1],
               gain_sets[set_idx][2],
               gain_sets[set_idx][3],
               gain_sets[set_idx][4],
               gain_sets[set_idx][5],
               gain_sets[set_idx][6],
               gain_sets[set_idx][7]);
    }

    return 0;
}

void SubbandWOLA_OfflineTest_All(void)
{
    int failures;

    failures = 0;
    failures += Run_Bypass_Sine_Test();
    failures += Run_Bypass_Noise_Test();
    failures += Run_COLA_Test();
    failures += Run_Impulse_Delay_Test();
    failures += Run_Band0_Rejects_10k_Test();
    failures += Run_WOLA_Passthrough_Test("100Hz", 100.0f, 0);
    failures += Run_WOLA_Passthrough_Test("500Hz", 500.0f, 0);
    failures += Run_WOLA_Passthrough_Test("1kHz", 1000.0f, 0);
    failures += Run_WOLA_Passthrough_Test("2kHz", 2000.0f, 0);
    failures += Run_WOLA_Passthrough_Test("5kHz", 5000.0f, 0);
    failures += Run_WOLA_Passthrough_Test("10kHz", 10000.0f, 0);
    failures += Run_WOLA_Passthrough_Test("15kHz", 15000.0f, 0);
    failures += Run_WOLA_Passthrough_Test("white", 0.0f, 1);
    failures += Run_WOLA_Float_Reconstruction_Test();
    failures += Run_Single_Band_Retention_Test();
    failures += Run_Gain_Perturbation_Test();
    failures += SubbandEval_OfflineTest_All();

    SubbandWOLA_TestFailures = failures;
    printf("SubbandWOLA_OfflineTest_All failures=%d\n", failures);
}

#ifdef SUBBAND_TEST_MAIN
int main(void)
{
    SubbandWOLA_OfflineTest_All();
    if (SubbandWOLA_TestFailures != 0)
    {
        return 1;
    }
    return 0;
}
#endif

#endif /* SUBBAND_ALGO_ONLY */
