/**
 * user_subband_flow.c
 *
 * 描述: 子频带分析-合成实时测试例程
 * 功能: CH1 经过 1024 点子带分析-合成接口，CH2~CH8 暂保持直通
 */

#include "user_subband_flow.h"
#include "math.h"
#include "string.h"

#ifndef SUBBAND_FLOW_ALGO_ONLY
#include "dac_api.h"
#include "system.h"
#include "key_api.h"
#endif

#define SUBBAND_PI 3.1415926535897932384626433832795
#define SUBBAND_LINE_LEN (SUBBAND_ORD - 1 + SUBBAND_FRM_LEN)

#if !ADDA_SUBBAND_BYPASS

typedef struct
{
    double ana_line[SUBBAND_LINE_LEN];
} ANA_STATE;

typedef struct
{
    double syn_line[SUBBAND_D][SUBBAND_LINE_LEN][2];
} SYN_STATE;

static ANA_STATE Ana_State;
static SYN_STATE Syn_State;
static double Prototype_H[SUBBAND_ORD];
static double Hk[SUBBAND_D][SUBBAND_ORD][2];
static double Ana_Sub_Out[SUBBAND_D][SUBBAND_FRM_LEN][2];
static double Syn_Sub_In[SUBBAND_D][SUBBAND_FRM_LEN][2];

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

#endif

#if !ADDA_SUBBAND_BYPASS

static void Clear_FilterBank_State(void)
{
    memset(&Ana_State, 0, sizeof(Ana_State));
    memset(&Syn_State, 0, sizeof(Syn_State));
    memset(Ana_Sub_Out, 0, sizeof(Ana_Sub_Out));
    memset(Syn_Sub_In, 0, sizeof(Syn_Sub_In));
}

#endif

void Subband_FilterBank_Init(void)
{
#if !ADDA_SUBBAND_BYPASS
    int k;
    int n;
    double m;
    double wc;
    double sum;
    double phase;

    if (Subband_FilterBank_Ready != 0)
    {
        return;
    }

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
            Hk[k][n][0] = Prototype_H[n] * cos(phase);
            Hk[k][n][1] = Prototype_H[n] * sin(phase);
        }
    }

    Clear_FilterBank_State();
#endif

    Subband_FilterBank_Ready = 1;
}

#if !ADDA_SUBBAND_BYPASS

static short Saturate_To_Short(double value)
{
    if (value > 32767.0)
    {
        return 32767;
    }
    if (value < -32768.0)
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
    double vr;
    double vi;
    double x;

    for (n = 0; n < SUBBAND_ORD - 1; n++)
    {
        Ana_State.ana_line[n] = Ana_State.ana_line[SUBBAND_FRM_LEN + n];
    }

    for (i = 0; i < SUBBAND_FRM_LEN; i++)
    {
        Ana_State.ana_line[SUBBAND_ORD - 1 + i] = (double)in[i];
    }

    for (k = 0; k < SUBBAND_D; k++)
    {
        for (i = 0; i < SUBBAND_FRM_LEN; i++)
        {
            vr = 0.0;
            vi = 0.0;

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
        }
    }
}

static void Synthesis_Filter_1024(short *out)
{
    int k;
    int i;
    int n;
    double xr;
    double xi;
    double hr;
    double hi;
    double vr;
    double vi;
    double acc;

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
        acc = 0.0;

        for (k = 0; k < SUBBAND_D; k++)
        {
            vr = 0.0;
            vi = 0.0;

            for (n = 0; n < SUBBAND_ORD; n++)
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
        acc *= SUBBAND_RECON_GAIN;
        out[i] = Saturate_To_Short(acc);
    }
}

#endif

void Subband_Process_1024(short *in, short *out)
{
#if ADDA_SUBBAND_BYPASS
    int i;
#endif

    if ((in == 0) || (out == 0))
    {
        return;
    }

    Subband_FilterBank_Init();

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

#ifndef SUBBAND_FLOW_ALGO_ONLY
    SUBBAND_FilterbankFrames++;
#endif
}

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

    Adc_Start();
    Dac_Start();

    while (1)
    {
        if (FLAG_AD == 1)
        {
            FLAG_AD = 0;
            FLAG_AD_DONE = 1;
            SUBBAND_DebugAdFrames++;

            Capture_Adc_Frame();
        }

        if (FLAG_DA == 1 && FLAG_AD_DONE == 1)
        {
            FLAG_DA = 0;
            FLAG_AD_DONE = 0;
            SUBBAND_DebugDaFrames++;

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
