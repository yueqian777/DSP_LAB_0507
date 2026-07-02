/**
 * user_subband_flow.c
 *
 * 描述: 子频带分析-合成实时测试例程
 * 功能: 8 个通道全部经过 1024 点子带分析-合成接口，暂不加入额外处理
 */

#include "user_subband_flow.h"
#include "string.h"

#ifndef SUBBAND_FLOW_ALGO_ONLY
#include "dac_api.h"
#include "system.h"
#include "key_api.h"
#endif

typedef struct
{
    long ana_sub_out[SUBBAND_D][SUBBAND_LEN];
} ANA_STATE;

typedef struct
{
    long syn_sub_in[SUBBAND_D][SUBBAND_LEN];
} SYN_STATE;

static ANA_STATE Ana_State;
static SYN_STATE Syn_State;

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
static short DA_Buffer2[DAC_SAMPLE_1024];
static short DA_Buffer3[DAC_SAMPLE_1024];
static short DA_Buffer4[DAC_SAMPLE_1024];
static short DA_Buffer5[DAC_SAMPLE_1024];
static short DA_Buffer6[DAC_SAMPLE_1024];
static short DA_Buffer7[DAC_SAMPLE_1024];
static short DA_Buffer8[DAC_SAMPLE_1024];

volatile unsigned long SUBBAND_DebugAdFrames = 0;
volatile unsigned long SUBBAND_DebugDaFrames = 0;
volatile unsigned char SUBBAND_DebugFrameReady = 0;
volatile unsigned char SUBBAND_DebugLastAdPingPong = 0;
volatile unsigned char SUBBAND_DebugLastDaPingPong = 0;

#endif

#if !ADDA_SUBBAND_BYPASS

static short Saturate_To_Short(long value)
{
    if (value > 32767L)
    {
        return 32767;
    }
    if (value < -32768L)
    {
        return -32768;
    }

    return (short)value;
}

static void Analyze_Subband_1024(short *in)
{
    int k;
    int i;
    int n;

    for (i = 0; i < SUBBAND_LEN; i++)
    {
        for (k = 0; k < SUBBAND_D; k++)
        {
            n = i * SUBBAND_D + k;
            Ana_State.ana_sub_out[k][i] = (long)in[n];
        }
    }
}

static void Process_Subband_1024(void)
{
    int k;
    int i;

    for (k = 0; k < SUBBAND_D; k++)
    {
        for (i = 0; i < SUBBAND_LEN; i++)
        {
            Syn_State.syn_sub_in[k][i] = Ana_State.ana_sub_out[k][i];
        }
    }
}

static void Synthesize_Subband_1024(short *out)
{
    int k;
    int i;
    int n;

    for (i = 0; i < SUBBAND_LEN; i++)
    {
        for (k = 0; k < SUBBAND_D; k++)
        {
            n = i * SUBBAND_D + k;
            out[n] = Saturate_To_Short(Syn_State.syn_sub_in[k][i]);
        }
    }
}

#endif

void Subband_Process_1024(short *in, short *out)
{
    if ((in == 0) || (out == 0))
    {
        return;
    }

#if ADDA_SUBBAND_BYPASS
    {
        int i;

        (void)Ana_State;
        (void)Syn_State;

        for (i = 0; i < SUBBAND_FRM_LEN; i++)
        {
            out[i] = in[i];
        }
    }
#else
    Analyze_Subband_1024(in);
    Process_Subband_1024();
    Synthesize_Subband_1024(out);
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
    Subband_Process_1024(AD_Buffer2, DA_Buffer2);
    Subband_Process_1024(AD_Buffer3, DA_Buffer3);
    Subband_Process_1024(AD_Buffer4, DA_Buffer4);
    Subband_Process_1024(AD_Buffer5, DA_Buffer5);
    Subband_Process_1024(AD_Buffer6, DA_Buffer6);
    Subband_Process_1024(AD_Buffer7, DA_Buffer7);
    Subband_Process_1024(AD_Buffer8, DA_Buffer8);
}

static void Fill_Dac_Ping_Buffer(void)
{
    memcpy(DA_CH1_Buf0, DA_Buffer1, 2 * DAC_SAMPLE_1024);
    memcpy(DA_CH2_Buf0, DA_Buffer2, 2 * DAC_SAMPLE_1024);
    memcpy(DA_CH3_Buf0, DA_Buffer3, 2 * DAC_SAMPLE_1024);
    memcpy(DA_CH4_Buf0, DA_Buffer4, 2 * DAC_SAMPLE_1024);
    memcpy(DA_CH5_Buf0, DA_Buffer5, 2 * DAC_SAMPLE_1024);
    memcpy(DA_CH6_Buf0, DA_Buffer6, 2 * DAC_SAMPLE_1024);
    memcpy(DA_CH7_Buf0, DA_Buffer7, 2 * DAC_SAMPLE_1024);
    memcpy(DA_CH8_Buf0, DA_Buffer8, 2 * DAC_SAMPLE_1024);
}

static void Fill_Dac_Pong_Buffer(void)
{
    memcpy(DA_CH1_Buf1, DA_Buffer1, 2 * DAC_SAMPLE_1024);
    memcpy(DA_CH2_Buf1, DA_Buffer2, 2 * DAC_SAMPLE_1024);
    memcpy(DA_CH3_Buf1, DA_Buffer3, 2 * DAC_SAMPLE_1024);
    memcpy(DA_CH4_Buf1, DA_Buffer4, 2 * DAC_SAMPLE_1024);
    memcpy(DA_CH5_Buf1, DA_Buffer5, 2 * DAC_SAMPLE_1024);
    memcpy(DA_CH6_Buf1, DA_Buffer6, 2 * DAC_SAMPLE_1024);
    memcpy(DA_CH7_Buf1, DA_Buffer7, 2 * DAC_SAMPLE_1024);
    memcpy(DA_CH8_Buf1, DA_Buffer8, 2 * DAC_SAMPLE_1024);
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
