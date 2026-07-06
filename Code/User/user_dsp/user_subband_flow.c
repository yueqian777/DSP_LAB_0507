/**
 * user_subband_flow.c
 *
 * 描述: 子频带分析-合成实时测试例程
 * 功能: CH1 经过 1024 点子带分析-合成接口，CH2~CH8 暂保持直通
 */

#include "user_subband_flow.h"
#include "user_subband_wola.h"
#include "user_subband_denoise.h"
#include "user_subband_eval.h"
#include "math.h"
#include "string.h"

#ifndef SUBBAND_FLOW_ALGO_ONLY
#include "dac_api.h"
#include "system.h"
#include "key_api.h"
#endif

#if defined(SUBBAND_FLOW_ALGO_ONLY) && defined(SUBBAND_OFFLINE_TEST_MAIN)
#include "stdio.h"
#endif

#define SUBBAND_PI 3.1415926535897932384626433832795
#define SUBBAND_LINE_LEN (SUBBAND_ORD - 1 + SUBBAND_FRM_LEN)

typedef float SUBBAND_REAL;

#if SUBBAND_USE_LEGACY_FIR && !ADDA_SUBBAND_BYPASS

typedef struct
{
    SUBBAND_REAL ana_line[SUBBAND_LINE_LEN];
} ANA_STATE;

typedef struct
{
    SUBBAND_REAL syn_line[SUBBAND_D][SUBBAND_LINE_LEN][2];
} SYN_STATE;

#if defined(__TI_COMPILER_VERSION__) || defined(__TMS320C6X__)
#pragma DATA_SECTION(Ana_State, ".subband_l2")
#pragma DATA_SECTION(Syn_State, ".subband_l2")
#pragma DATA_SECTION(Prototype_H, ".subband_l2")
#pragma DATA_SECTION(Hk, ".subband_l2")
#pragma DATA_SECTION(Ana_Sub_Out, ".subband_l2")
#pragma DATA_SECTION(Syn_Sub_In, ".subband_l2")
#endif

static ANA_STATE Ana_State;
static SYN_STATE Syn_State;
static SUBBAND_REAL Prototype_H[SUBBAND_ORD];
static SUBBAND_REAL Hk[SUBBAND_D][SUBBAND_ORD][2];
static SUBBAND_REAL Ana_Sub_Out[SUBBAND_D][SUBBAND_FRM_LEN][2];
static SUBBAND_REAL Syn_Sub_In[SUBBAND_D][SUBBAND_FRM_LEN][2];

#endif

static unsigned char Subband_FilterBank_Ready = 0;

#ifndef SUBBAND_FLOW_ALGO_ONLY

static short AD_Buffer1[ADC_SAMPLE_1024];
static short AD_Buffer2[ADC_SAMPLE_1024];
static short AD_Buffer3[ADC_SAMPLE_1024];
static short AD_Buffer4[ADC_SAMPLE_1024];
static short AD_Buffer5[ADC_SAMPLE_1024];
static short AD_Buffer6[ADC_SAMPLE_1024];
static short AD_Buffer7[ADC_SAMPLE_1024];
static short AD_Buffer8[ADC_SAMPLE_1024];

static short DA_Buffer1[DAC_SAMPLE_1024];

volatile unsigned long SUBBAND_DebugAdFrames = 0;
volatile unsigned long SUBBAND_DebugDaFrames = 0;
volatile unsigned char SUBBAND_DebugFrameReady = 0;
volatile unsigned char SUBBAND_DebugLastAdPingPong = 0;
volatile unsigned char SUBBAND_DebugLastDaPingPong = 0;
volatile unsigned long SUBBAND_FilterbankFrames = 0;
volatile int SUBBAND_DebugDemoMode = SUBBAND_DEMO_DEFAULT_MODE;
volatile int SUBBAND_DebugAppliedDemoMode = -1;
volatile unsigned long SUBBAND_DebugDemoModeChanges = 0;

#endif

#if SUBBAND_USE_LEGACY_FIR && !ADDA_SUBBAND_BYPASS

static void Clear_FilterBank_State(void)
{
    memset(&Ana_State, 0, sizeof(Ana_State));
    memset(&Syn_State, 0, sizeof(Syn_State));
    memset(Ana_Sub_Out, 0, sizeof(Ana_Sub_Out));
    memset(Syn_Sub_In, 0, sizeof(Syn_Sub_In));
}

#endif

#ifndef SUBBAND_FLOW_ALGO_ONLY

static int Subband_Normalize_Demo_Mode(int mode)
{
    switch (mode)
    {
    case SUBBAND_DEMO_MODE_RAW_BYPASS:
    case SUBBAND_DEMO_MODE_WOLA_ONLY:
    case SUBBAND_DEMO_MODE_WOLA_DENOISE:
    case SUBBAND_DEMO_MODE_MILD_DENOISE:
    case SUBBAND_DEMO_MODE_WOLA_MS_DENOISE:
    case SUBBAND_DEMO_MODE_MILD_MS_DENOISE:
        return mode;
    default:
        return SUBBAND_DEMO_DEFAULT_MODE;
    }
}

static void Subband_Apply_Demo_Mode(int mode)
{
    switch (mode)
    {
    case SUBBAND_DEMO_MODE_RAW_BYPASS:
        SubbandWOLA_SetBypass(1);
        SubbandDenoise_StopLearning();
        SubbandDenoise_SetEnabled(0);
        break;

    case SUBBAND_DEMO_MODE_WOLA_ONLY:
        SubbandWOLA_SetBypass(0);
        SubbandWOLA_ResetStream();
        SubbandWOLA_ResetAllGains();
        SubbandDenoise_StopLearning();
        SubbandDenoise_SetEnabled(0);
        break;

    case SUBBAND_DEMO_MODE_MILD_DENOISE:
        SubbandWOLA_SetBypass(0);
        SubbandWOLA_ResetStream();
        SubbandWOLA_ResetAllGains();
        SubbandDenoise_Reset();
        SubbandDenoise_SetParams(0.96f, 0.35f, 0.85f, 0.60f);
        SubbandDenoise_StartNoiseLearning();
        break;

    case SUBBAND_DEMO_MODE_WOLA_MS_DENOISE:
        SubbandWOLA_SetBypass(0);
        SubbandWOLA_ResetStream();
        SubbandWOLA_ResetAllGains();
        SubbandDenoise_Reset();
        SubbandDenoise_SetNoiseTrackMode(SUBBAND_DENOISE_TRACK_HYBRID);
        SubbandDenoise_StartNoiseLearning();
        break;

    case SUBBAND_DEMO_MODE_MILD_MS_DENOISE:
        SubbandWOLA_SetBypass(0);
        SubbandWOLA_ResetStream();
        SubbandWOLA_ResetAllGains();
        SubbandDenoise_Reset();
        SubbandDenoise_SetParams(0.96f, 0.35f, 0.85f, 0.60f);
        SubbandDenoise_SetNoiseTrackMode(SUBBAND_DENOISE_TRACK_HYBRID);
        SubbandDenoise_StartNoiseLearning();
        break;

    case SUBBAND_DEMO_MODE_WOLA_DENOISE:
    default:
        SubbandWOLA_SetBypass(0);
        SubbandWOLA_ResetStream();
        SubbandWOLA_ResetAllGains();
        SubbandDenoise_Reset();
        SubbandDenoise_StartNoiseLearning();
        break;
    }
}

static void Subband_Service_Demo_Mode(void)
{
    int mode;

    mode = Subband_Normalize_Demo_Mode(SUBBAND_DebugDemoMode);
    if (SUBBAND_DebugDemoMode != mode)
    {
        SUBBAND_DebugDemoMode = mode;
    }
    if (mode != SUBBAND_DebugAppliedDemoMode)
    {
        Subband_Apply_Demo_Mode(mode);
        SUBBAND_DebugAppliedDemoMode = mode;
        SUBBAND_DebugDemoModeChanges++;
    }
}

#endif

void Subband_FilterBank_Init(void)
{
    if (Subband_FilterBank_Ready != 0)
    {
        return;
    }

#if SUBBAND_USE_LEGACY_FIR && !ADDA_SUBBAND_BYPASS
    int k;
    int n;
    double m;
    double wc;
    double sum;
    double phase;

    wc = SUBBAND_PI / (double)SUBBAND_D;
    sum = 0.0;

    for (n = 0; n < SUBBAND_ORD; n++)
    {
        m = (double)n - ((double)SUBBAND_ORD - 1.0) / 2.0;

        if ((m > -1.0e-12) && (m < 1.0e-12))
        {
            Prototype_H[n] = wc / SUBBAND_PI;
        }
        else
        {
            Prototype_H[n] = sin(wc * m) / (SUBBAND_PI * m);
        }

        Prototype_H[n] *= 0.54 - 0.46 * cos((2.0 * SUBBAND_PI * (double)n) / ((double)SUBBAND_ORD - 1.0));
        sum += Prototype_H[n];
    }

    if ((sum > 1.0e-12) || (sum < -1.0e-12))
    {
        for (n = 0; n < SUBBAND_ORD; n++)
        {
            Prototype_H[n] /= sum;
        }
    }

    for (k = 0; k < SUBBAND_D; k++)
    {
        for (n = 0; n < SUBBAND_ORD; n++)
        {
            phase = (2.0 * SUBBAND_PI * (double)n * (double)k) / (double)SUBBAND_D;
            Hk[k][n][0] = (SUBBAND_REAL)(Prototype_H[n] * cos(phase));
            Hk[k][n][1] = (SUBBAND_REAL)(Prototype_H[n] * sin(phase));
        }
    }

    Clear_FilterBank_State();
#else
    SubbandWOLA_Init();
#endif

    Subband_FilterBank_Ready = 1;
}

#if SUBBAND_USE_LEGACY_FIR && !ADDA_SUBBAND_BYPASS

static short Saturate_Real_To_Short(SUBBAND_REAL value)
{
    if (value > (SUBBAND_REAL)32767.0)
    {
        return 32767;
    }
    if (value < (SUBBAND_REAL)-32768.0)
    {
        return -32768;
    }

    return (short)value;
}

static void Analysis_Filter_1024(short *in)
{
    int k;
    int i;
    int n;
    int i_step;
    SUBBAND_REAL vr;
    SUBBAND_REAL vi;
    SUBBAND_REAL x;

    for (n = 0; n < SUBBAND_ORD - 1; n++)
    {
        Ana_State.ana_line[n] = Ana_State.ana_line[SUBBAND_FRM_LEN + n];
    }

    for (i = 0; i < SUBBAND_FRM_LEN; i++)
    {
        Ana_State.ana_line[SUBBAND_ORD - 1 + i] = (SUBBAND_REAL)in[i];
    }

#if SUBBAND_USE_CRITICAL_SAMPLING
    i_step = SUBBAND_D;
#else
    i_step = 1;
#endif

    for (k = 0; k < SUBBAND_D; k++)
    {
        for (i = 0; i < SUBBAND_FRM_LEN; i += i_step)
        {
            vr = (SUBBAND_REAL)0.0;
            vi = (SUBBAND_REAL)0.0;

            for (n = 0; n < SUBBAND_ORD; n++)
            {
                x = Ana_State.ana_line[i + n];
                vr += Hk[k][n][0] * x;
                vi += Hk[k][n][1] * x;
            }

            Ana_Sub_Out[k][i][0] = vr;
            Ana_Sub_Out[k][i][1] = vi;
        }
    }
}

static void Subband_Direct_Through_1024(void)
{
    int k;
    int i;

    for (k = 0; k < SUBBAND_D; k++)
    {
        for (i = 0; i < SUBBAND_FRM_LEN; i++)
        {
#if SUBBAND_USE_CRITICAL_SAMPLING
            /*
             * Guidebook critical-sampling path: keep one sample every SUBBAND_D
             * points and insert zeros before synthesis. Reconstruction quality
             * depends strongly on the prototype order, so keep SNR tests enabled.
             */
            if ((i % SUBBAND_D) == 0)
            {
                Syn_Sub_In[k][i][0] = Ana_Sub_Out[k][i][0];
                Syn_Sub_In[k][i][1] = Ana_Sub_Out[k][i][1];
            }
            else
            {
                Syn_Sub_In[k][i][0] = 0.0;
                Syn_Sub_In[k][i][1] = 0.0;
            }
#else
            Syn_Sub_In[k][i][0] = Ana_Sub_Out[k][i][0];
            Syn_Sub_In[k][i][1] = Ana_Sub_Out[k][i][1];
#endif
        }
    }
}

static void Synthesis_Filter_1024(short *out)
{
    int k;
    int i;
    int n;
    SUBBAND_REAL xr;
    SUBBAND_REAL xi;
    SUBBAND_REAL hr;
    SUBBAND_REAL hi;
    SUBBAND_REAL vr;
    SUBBAND_REAL vi;
    SUBBAND_REAL acc;

    for (k = 0; k < SUBBAND_D; k++)
    {
        for (n = 0; n < SUBBAND_ORD - 1; n++)
        {
            Syn_State.syn_line[k][n][0] = Syn_State.syn_line[k][SUBBAND_FRM_LEN + n][0];
            Syn_State.syn_line[k][n][1] = Syn_State.syn_line[k][SUBBAND_FRM_LEN + n][1];
        }

        for (i = 0; i < SUBBAND_FRM_LEN; i++)
        {
            Syn_State.syn_line[k][SUBBAND_ORD - 1 + i][0] = Syn_Sub_In[k][i][0];
            Syn_State.syn_line[k][SUBBAND_ORD - 1 + i][1] = Syn_Sub_In[k][i][1];
        }
    }

    for (i = 0; i < SUBBAND_FRM_LEN; i++)
    {
        acc = (SUBBAND_REAL)0.0;

        for (k = 0; k < SUBBAND_D; k++)
        {
            vr = (SUBBAND_REAL)0.0;
            vi = (SUBBAND_REAL)0.0;

#if SUBBAND_USE_CRITICAL_SAMPLING
            for (n = (SUBBAND_D - (i % SUBBAND_D)) % SUBBAND_D; n < SUBBAND_ORD; n += SUBBAND_D)
#else
            for (n = 0; n < SUBBAND_ORD; n++)
#endif
            {
                xr = Syn_State.syn_line[k][i + n][0];
                xi = Syn_State.syn_line[k][i + n][1];
                hr = Hk[k][n][0];
                hi = Hk[k][n][1];

                vr += hr * xr - hi * xi;
                vi += hr * xi + hi * xr;
            }

            (void)vi;
            acc += vr;
        }

        /* Reconstruction gain compensation. Tune this if the filterbank output is too quiet or too loud. */
        acc *= (SUBBAND_REAL)SUBBAND_RECON_GAIN;
        out[i] = Saturate_Real_To_Short(acc);
    }
}

#endif

void Subband_Process_1024(short *in, short *out)
{
#if SUBBAND_USE_LEGACY_FIR && ADDA_SUBBAND_BYPASS
    int i;
#endif

    if ((in == 0) || (out == 0))
    {
        return;
    }

    Subband_FilterBank_Init();

#if SUBBAND_USE_LEGACY_FIR
#if ADDA_SUBBAND_BYPASS
    for (i = 0; i < SUBBAND_FRM_LEN; i++)
    {
        out[i] = in[i];
    }
#else
    Analysis_Filter_1024(in);
    Subband_Direct_Through_1024();
    Synthesis_Filter_1024(out);
#endif
#else
    SubbandWOLA_ProcessFrame(in, out);
#endif

#ifndef SUBBAND_FLOW_ALGO_ONLY
    SUBBAND_FilterbankFrames++;
#endif
}

#if SUBBAND_USE_LEGACY_FIR && defined(SUBBAND_FLOW_ALGO_ONLY)

void Subband_Offline_Sine_Test(double freq_hz, double sample_rate_hz, double amplitude,
                               SUBBAND_OFFLINE_METRICS *metrics)
{
    short input[SUBBAND_FRM_LEN];
    short output[SUBBAND_FRM_LEN];
    int i;
    double phase;
    double diff;
    double abs_diff;
    double err_sum;
    double input_energy;
    double output_energy;
    double sample_rate;
    int lag;
    int best_lag;
    double best_snr;
    double best_gain;

    if (metrics == 0)
    {
        return;
    }

    if ((sample_rate_hz > 1.0e-12) && (sample_rate_hz < 1.0e12))
    {
        sample_rate = sample_rate_hz;
    }
    else
    {
        sample_rate = 50000.0;
    }

    for (i = 0; i < SUBBAND_FRM_LEN; i++)
    {
        phase = (2.0 * SUBBAND_PI * freq_hz * (double)i) / sample_rate;
        input[i] = (short)(amplitude * sin(phase));
        output[i] = 0;
    }

    Subband_FilterBank_Init();
#if SUBBAND_USE_LEGACY_FIR && !ADDA_SUBBAND_BYPASS
    Clear_FilterBank_State();
#endif
    Subband_Process_1024(input, output);

    metrics->max_error = 0.0;
    err_sum = 0.0;
    input_energy = 0.0;
    output_energy = 0.0;

    for (i = 0; i < SUBBAND_FRM_LEN; i++)
    {
        diff = (double)output[i] - (double)input[i];
        abs_diff = fabs(diff);
        if (abs_diff > metrics->max_error)
        {
            metrics->max_error = abs_diff;
        }

        err_sum += diff * diff;
        input_energy += (double)input[i] * (double)input[i];
        output_energy += (double)output[i] * (double)output[i];
    }

    metrics->mse = err_sum / (double)SUBBAND_FRM_LEN;
    metrics->input_energy = input_energy;
    if (input_energy > 1.0e-12)
    {
        metrics->output_energy_ratio = output_energy / input_energy;
    }
    else
    {
        metrics->output_energy_ratio = 0.0;
    }

    best_snr = -1.0e9;
    best_lag = 0;
    best_gain = 0.0;
    for (lag = 0; lag <= SUBBAND_FILTER_ORDER; lag++)
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

        for (i = 0; i < SUBBAND_FRM_LEN - lag; i++)
        {
            xy += (double)input[i] * (double)output[i + lag];
            xx += (double)input[i] * (double)input[i];
            yy += (double)output[i + lag] * (double)output[i + lag];
        }

        if (xx > 1.0e-12)
        {
            gain = xy / xx;
        }
        else
        {
            gain = 0.0;
        }

        for (i = 0; i < SUBBAND_FRM_LEN - lag; i++)
        {
            double d;

            d = (double)output[i + lag] - gain * (double)input[i];
            ee += d * d;
        }

        snr = 10.0 * log10((yy + 1.0e-12) / (ee + 1.0e-12));
        if (snr > best_snr)
        {
            best_snr = snr;
            best_lag = lag;
            best_gain = gain;
        }
    }

    metrics->aligned_snr_db = best_snr;
    metrics->aligned_lag = best_lag;
    metrics->aligned_gain = best_gain;
}

#ifdef SUBBAND_OFFLINE_TEST_MAIN
int main(void)
{
    SUBBAND_OFFLINE_METRICS metrics;

    Subband_Offline_Sine_Test(1000.0, 50000.0, 12000.0, &metrics);

    printf("ADDA_SUBBAND_BYPASS=%d SUBBAND_USE_CRITICAL_SAMPLING=%d "
           "max_error=%.3f mse=%.3f input_energy=%.3f output_energy_ratio=%.9f "
           "aligned_lag=%d aligned_gain=%.9f aligned_snr_db=%.3f\n",
           ADDA_SUBBAND_BYPASS,
           SUBBAND_USE_CRITICAL_SAMPLING,
           metrics.max_error,
           metrics.mse,
           metrics.input_energy,
           metrics.output_energy_ratio,
           metrics.aligned_lag,
           metrics.aligned_gain,
           metrics.aligned_snr_db);

    return 0;
}
#endif

#endif

#ifndef SUBBAND_FLOW_ALGO_ONLY

static void Capture_Adc_Frame(void)
{
    SUBBAND_DebugLastAdPingPong = AD_Ping_Pong;

    if (AD_Ping_Pong == AD_BUFFER_PONG)
    {
        memcpy(AD_Buffer1, AD_CH1_Buf0, 2 * ADC_SAMPLE_1024);
        memcpy(AD_Buffer2, AD_CH2_Buf0, 2 * ADC_SAMPLE_1024);
        memcpy(AD_Buffer3, AD_CH3_Buf0, 2 * ADC_SAMPLE_1024);
        memcpy(AD_Buffer4, AD_CH4_Buf0, 2 * ADC_SAMPLE_1024);
        memcpy(AD_Buffer5, AD_CH5_Buf0, 2 * ADC_SAMPLE_1024);
        memcpy(AD_Buffer6, AD_CH6_Buf0, 2 * ADC_SAMPLE_1024);
        memcpy(AD_Buffer7, AD_CH7_Buf0, 2 * ADC_SAMPLE_1024);
        memcpy(AD_Buffer8, AD_CH8_Buf0, 2 * ADC_SAMPLE_1024);
    }
    else
    {
        memcpy(AD_Buffer1, AD_CH1_Buf1, 2 * ADC_SAMPLE_1024);
        memcpy(AD_Buffer2, AD_CH2_Buf1, 2 * ADC_SAMPLE_1024);
        memcpy(AD_Buffer3, AD_CH3_Buf1, 2 * ADC_SAMPLE_1024);
        memcpy(AD_Buffer4, AD_CH4_Buf1, 2 * ADC_SAMPLE_1024);
        memcpy(AD_Buffer5, AD_CH5_Buf1, 2 * ADC_SAMPLE_1024);
        memcpy(AD_Buffer6, AD_CH6_Buf1, 2 * ADC_SAMPLE_1024);
        memcpy(AD_Buffer7, AD_CH7_Buf1, 2 * ADC_SAMPLE_1024);
        memcpy(AD_Buffer8, AD_CH8_Buf1, 2 * ADC_SAMPLE_1024);
    }

    Subband_Process_1024(AD_Buffer1, DA_Buffer1);
}

static void Fill_Dac_Ping_Buffer(void)
{
    memcpy(DA_CH1_Buf0, DA_Buffer1, 2 * DAC_SAMPLE_1024);
    memcpy(DA_CH2_Buf0, AD_Buffer2, 2 * DAC_SAMPLE_1024);
    memcpy(DA_CH3_Buf0, AD_Buffer3, 2 * DAC_SAMPLE_1024);
    memcpy(DA_CH4_Buf0, AD_Buffer4, 2 * DAC_SAMPLE_1024);
    memcpy(DA_CH5_Buf0, AD_Buffer5, 2 * DAC_SAMPLE_1024);
    memcpy(DA_CH6_Buf0, AD_Buffer6, 2 * DAC_SAMPLE_1024);
    memcpy(DA_CH7_Buf0, AD_Buffer7, 2 * DAC_SAMPLE_1024);
    memcpy(DA_CH8_Buf0, AD_Buffer8, 2 * DAC_SAMPLE_1024);
}

static void Fill_Dac_Pong_Buffer(void)
{
    memcpy(DA_CH1_Buf1, DA_Buffer1, 2 * DAC_SAMPLE_1024);
    memcpy(DA_CH2_Buf1, AD_Buffer2, 2 * DAC_SAMPLE_1024);
    memcpy(DA_CH3_Buf1, AD_Buffer3, 2 * DAC_SAMPLE_1024);
    memcpy(DA_CH4_Buf1, AD_Buffer4, 2 * DAC_SAMPLE_1024);
    memcpy(DA_CH5_Buf1, AD_Buffer5, 2 * DAC_SAMPLE_1024);
    memcpy(DA_CH6_Buf1, AD_Buffer6, 2 * DAC_SAMPLE_1024);
    memcpy(DA_CH7_Buf1, AD_Buffer7, 2 * DAC_SAMPLE_1024);
    memcpy(DA_CH8_Buf1, AD_Buffer8, 2 * DAC_SAMPLE_1024);
}

static void Fill_Dac_Inactive_Buffer(void)
{
    SUBBAND_DebugLastDaPingPong = DA_Ping_Pong;

    if (DA_Ping_Pong == DA_BUFFER_PONG)
    {
        Fill_Dac_Ping_Buffer();
    }
    else
    {
        Fill_Dac_Pong_Buffer();
    }
}

void Subband_Flow_Example(void)
{
    unsigned char FLAG_AD_DONE = 0;

    Sys_Init();
    Key_Init();

    Adc_Init(ADC_50KHZ, ADC_SAMPLE_1024);
    Dac_Init(DAC_50KHZ, DAC_SAMPLE_1024, DAC_CHANNEL_ALL);
    Subband_FilterBank_Init();
    Subband_Service_Demo_Mode();

    Adc_Start();
    Dac_Start();

    while (1)
    {
        Subband_Service_Demo_Mode();

        if (FLAG_AD == 1)
        {
            FLAG_AD = 0;
            FLAG_AD_DONE = 1;
            SUBBAND_DebugAdFrames++;
            SUBBAND_EVAL_DebugAdFrames = SUBBAND_DebugAdFrames;

            Capture_Adc_Frame();
        }

        if (FLAG_DA == 1 && FLAG_AD_DONE == 1)
        {
            FLAG_DA = 0;
            FLAG_AD_DONE = 0;
            SUBBAND_DebugDaFrames++;
            SUBBAND_EVAL_DebugDaFrames = SUBBAND_DebugDaFrames;

            Fill_Dac_Inactive_Buffer();
        }

        SUBBAND_DebugFrameReady = FLAG_AD_DONE;

        if (FLAG_KEY1 == 1)
        {
            FLAG_KEY1 = 0;
            Adc_Start();
        }

        if (FLAG_KEY2 == 1)
        {
            FLAG_KEY2 = 0;
            Adc_Stop();
        }

        if (FLAG_KEY3 == 1)
        {
            FLAG_KEY3 = 0;
            Dac_Start();
        }

        if (FLAG_KEY4 == 1)
        {
            FLAG_KEY4 = 0;
            Dac_Stop();
        }
    }
}

#endif /* SUBBAND_FLOW_ALGO_ONLY */
