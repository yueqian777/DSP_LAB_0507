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
#include "user_subband_codec_loopback.h"
#include "user_subband_polyphase.h"
#include "math.h"
#include "string.h"

#if defined(__TI_COMPILER_VERSION__) || defined(__TMS320C6X__)
#include "c6x.h"
#define SUBBAND_CPU_CYCLES_PER_MS 456000.0f
#else
#define SUBBAND_CPU_CYCLES_PER_MS 1.0f
#endif

#ifndef SUBBAND_FLOW_ALGO_ONLY
#include "dac_api.h"
#include "system.h"
#include "key_api.h"
#include "timer_api.h"
#include "touch_api.h"
#include "user_subband_ui.h"
#if SUBBAND_THD_BOARD_TEST
#include "equalizer_build_id.h"
#endif
#endif

#if defined(SUBBAND_FLOW_ALGO_ONLY) && defined(SUBBAND_OFFLINE_TEST_MAIN)
#include "stdio.h"
#endif

#define SUBBAND_PI 3.1415926535897932384626433832795
#define SUBBAND_LINE_LEN (SUBBAND_ORD - 1 + SUBBAND_FRM_LEN)

typedef float SUBBAND_REAL;

#if SUBBAND_ENABLE_LEGACY_FIR_BACKEND

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
volatile unsigned long SUBBAND_DebugDemoModeChanges = 0UL;
volatile int SUBBAND_DebugPersistentCodecKbps = 240;
volatile int SUBBAND_DebugRequestedBackend = SUBBAND_BACKEND_WOLA;
volatile int SUBBAND_DebugAppliedBackend = -1;
volatile unsigned long SUBBAND_DebugBackendChanges = 0UL;
volatile unsigned long SUBBAND_DebugAlgoFrames = 0UL;
volatile unsigned long SUBBAND_DebugLastCycles = 0UL;
volatile unsigned long SUBBAND_DebugMaxCycles = 0UL;
volatile float SUBBAND_DebugLastMs = 0.0f;
volatile float SUBBAND_DebugMaxMs = 0.0f;
volatile float SUBBAND_DebugCpuUsagePercent = 0.0f;
volatile unsigned long SUBBAND_DebugInputNonzeroFrames = 0UL;
volatile unsigned long SUBBAND_DebugOutputNonzeroFrames = 0UL;
volatile unsigned long SUBBAND_DebugInputClipFrames = 0UL;
volatile unsigned long SUBBAND_DebugOutputClipFrames = 0UL;
volatile int SUBBAND_DebugInputPeakMax = 0;
volatile int SUBBAND_DebugOutputPeakMax = 0;
volatile unsigned long SUBBAND_DebugInputMeanSquareAvg = 0UL;
volatile unsigned long SUBBAND_DebugOutputMeanSquareAvg = 0UL;
volatile unsigned long SUBBAND_DebugBenchmarkResetRequest = 0UL;
volatile short SUBBAND_DebugAdFirstSample = 0;
volatile short SUBBAND_DebugDaFirstSample = 0;
volatile int SUBBAND_DebugAdPeak = 0;
volatile int SUBBAND_DebugDaPeak = 0;

#if SUBBAND_THD_BOARD_TEST
#if defined(__TI_COMPILER_VERSION__) || defined(__TMS320C6X__)
#pragma DATA_SECTION(SUBBAND_THD_Input, ".far")
#pragma DATA_SECTION(SUBBAND_THD_OutputPacked, ".far")
#pragma DATA_SECTION(SUBBAND_THD_FrameOutput, ".far")
#pragma DATA_ALIGN(SUBBAND_THD_Input, 8)
#pragma DATA_ALIGN(SUBBAND_THD_OutputPacked, 8)
#pragma DATA_ALIGN(SUBBAND_THD_FrameOutput, 8)
#endif
short SUBBAND_THD_Input[SUBBAND_THD_PROCESSED_SAMPLES];
unsigned int SUBBAND_THD_OutputPacked[SUBBAND_THD_PROCESSED_SAMPLES / 2];
static short SUBBAND_THD_FrameOutput[SUBBAND_FRM_LEN];
volatile unsigned int SUBBAND_THD_DebugRequest = 0U;
volatile unsigned int SUBBAND_THD_DebugStatus = SUBBAND_THD_STATUS_BOOT;
volatile unsigned long SUBBAND_THD_DebugFrames = 0UL;
volatile unsigned long SUBBAND_THD_DebugCycleCount = 0UL;
volatile unsigned long SUBBAND_THD_DebugMaxFrameCycles = 0UL;
volatile const char SUBBAND_THD_DebugBuildGitSha[] = EQ_BUILD_GIT_SHA;
volatile const int SUBBAND_THD_DebugBuildDirty = EQ_BUILD_DIRTY;
#endif

#endif

#if SUBBAND_ENABLE_LEGACY_FIR_BACKEND

static void Clear_FilterBank_State(void)
{
    memset(&Ana_State, 0, sizeof(Ana_State));
    memset(&Syn_State, 0, sizeof(Syn_State));
    memset(Ana_Sub_Out, 0, sizeof(Ana_Sub_Out));
    memset(Syn_Sub_In, 0, sizeof(Syn_Sub_In));
}

#endif

#ifndef SUBBAND_FLOW_ALGO_ONLY

static int Subband_Frame_Peak(const short *x)
{
    int i;
    int peak;

    peak = 0;
    if (x == 0)
    {
        return 0;
    }

    for (i = 0; i < SUBBAND_FRM_LEN; i++)
    {
        int v;

        v = (int)x[i];
        if (v < 0)
        {
            v = -v;
        }
        if (v > peak)
        {
            peak = v;
        }
    }
    return peak;
}

static unsigned long Subband_Frame_Mean_Square(const short *x)
{
    int i;
    unsigned long long sum;

    if (x == 0)
    {
        return 0UL;
    }

    sum = 0ULL;
    for (i = 0; i < SUBBAND_FRM_LEN; i++)
    {
        long sample;

        sample = (long)x[i];
        sum += (unsigned long long)(sample * sample);
    }
    return (unsigned long)(sum / (unsigned long long)SUBBAND_FRM_LEN);
}

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
    case SUBBAND_DEMO_MODE_MCRA_DENOISE:
    case SUBBAND_DEMO_MODE_STRONG_MCRA_DENOISE:
    case SUBBAND_DEMO_MODE_MCRA_CODEC_LOOPBACK:
    case SUBBAND_DEMO_MODE_STRONG_MCRA_CODEC_LOOPBACK:
    case SUBBAND_DEMO_MODE_FIXED_CODEC_LOOPBACK:
    case SUBBAND_DEMO_MODE_CODEC_LOOPBACK_ONLY:
        return mode;
    default:
        return SUBBAND_DEMO_DEFAULT_MODE;
    }
}

static int Subband_Normalize_Codec_Target(int kbps)
{
    if ((kbps == 160) || (kbps == 240) || (kbps == 320))
    {
        return kbps;
    }
    return 240;
}

void Subband_Request_Codec_Target(int kbps)
{
    int normalized;

    normalized = Subband_Normalize_Codec_Target(kbps);
    SUBBAND_DebugPersistentCodecKbps = normalized;
    if ((SUBBAND_DebugAppliedDemoMode ==
         SUBBAND_DEMO_MODE_MCRA_CODEC_LOOPBACK) ||
        (SUBBAND_DebugAppliedDemoMode ==
         SUBBAND_DEMO_MODE_CODEC_LOOPBACK_ONLY))
    {
        SUBBAND_CODEC_LOOP_DebugRequestedTargetKbps = normalized;
    }
}

static void Subband_Apply_Demo_Mode(int mode)
{
    SubbandCodecLoopback_SetEnabled(0);

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

    case SUBBAND_DEMO_MODE_MCRA_DENOISE:
        SubbandWOLA_SetBypass(0);
        SubbandWOLA_ResetStream();
        SubbandWOLA_ResetAllGains();
        SubbandDenoise_Reset();
        SubbandDenoise_SetParams(0.96f, 0.15f, 0.85f, 0.80f);
        SubbandDenoise_SetMcraTonalGuardParams(5.0f, 1.80f, 0.28f);
        SubbandDenoise_SetNoiseTrackMode(SUBBAND_DENOISE_TRACK_MCRA);
        SubbandDenoise_SetMcraParams(1.5f, 4.0f,
                                     0.85f, 0.998f,
                                     1.10f, 1.70f,
                                     1.40f,
                                     0);
        SubbandDenoise_StartNoiseLearning();
        break;

    case SUBBAND_DEMO_MODE_STRONG_MCRA_DENOISE:
        SubbandWOLA_SetBypass(0);
        SubbandWOLA_ResetStream();
        SubbandWOLA_ResetAllGains();
        SubbandDenoise_Reset();
        SubbandDenoise_SetNoiseTrackMode(SUBBAND_DENOISE_TRACK_MCRA);
        SubbandDenoise_SetMcraParams(1.3f, 3.5f,
                                     0.80f, 0.998f,
                                     1.20f, 2.10f,
                                     1.60f,
                                     1);
        SubbandDenoise_StartNoiseLearning();
        break;

    case SUBBAND_DEMO_MODE_MCRA_CODEC_LOOPBACK:
        SubbandWOLA_SetBypass(0);
        SubbandWOLA_ResetStream();
        SubbandWOLA_ResetAllGains();
        SubbandDenoise_Reset();
        SubbandDenoise_SetNoiseTrackMode(SUBBAND_DENOISE_TRACK_MCRA);
        SubbandDenoise_SetMcraParams(1.5f, 4.0f,
                                     0.85f, 0.998f,
                                     1.10f, 1.70f,
                                     1.40f,
                                     0);
        SubbandCodecLoopback_Reset();
        SubbandCodecLoopback_SetTargetKbps(
            Subband_Normalize_Codec_Target(
                SUBBAND_DebugPersistentCodecKbps));
        SubbandCodecLoopback_SetEnabled(1);
        SubbandDenoise_StartNoiseLearning();
        break;

    case SUBBAND_DEMO_MODE_STRONG_MCRA_CODEC_LOOPBACK:
        SubbandWOLA_SetBypass(0);
        SubbandWOLA_ResetStream();
        SubbandWOLA_ResetAllGains();
        SubbandDenoise_Reset();
        SubbandDenoise_SetNoiseTrackMode(SUBBAND_DENOISE_TRACK_MCRA);
        SubbandDenoise_SetMcraParams(1.3f, 3.5f,
                                     0.80f, 0.998f,
                                     1.20f, 2.10f,
                                     1.60f,
                                     1);
        SubbandCodecLoopback_Reset();
        SubbandCodecLoopback_SetTargetKbps(240);
        SubbandCodecLoopback_SetEnabled(1);
        SubbandDenoise_StartNoiseLearning();
        break;

    case SUBBAND_DEMO_MODE_FIXED_CODEC_LOOPBACK:
        SubbandWOLA_SetBypass(0);
        SubbandWOLA_ResetStream();
        SubbandWOLA_ResetAllGains();
        SubbandDenoise_Reset();
        SubbandCodecLoopback_Reset();
        SubbandCodecLoopback_SetTargetKbps(240);
        SubbandCodecLoopback_SetEnabled(1);
        SubbandDenoise_StartNoiseLearning();
        break;

    case SUBBAND_DEMO_MODE_CODEC_LOOPBACK_ONLY:
        SubbandWOLA_SetBypass(0);
        SubbandWOLA_ResetStream();
        SubbandWOLA_ResetAllGains();
        SubbandDenoise_Reset();
        SubbandDenoise_StopLearning();
        SubbandDenoise_SetEnabled(0);
        SubbandCodecLoopback_Reset();
        SubbandCodecLoopback_SetTargetKbps(
            Subband_Normalize_Codec_Target(
                SUBBAND_DebugPersistentCodecKbps));
        SubbandCodecLoopback_SetEnabled(1);
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
    int codec_target;

    codec_target = Subband_Normalize_Codec_Target(
        SUBBAND_DebugPersistentCodecKbps);
    if (codec_target != SUBBAND_DebugPersistentCodecKbps)
    {
        SUBBAND_DebugPersistentCodecKbps = codec_target;
    }

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
        SubbandUI_ResetLoadWindow();
        SubbandUI_NotifyModeChanged();
    }
}

#endif

void Subband_FilterBank_Init(void)
{
    if (Subband_FilterBank_Ready != 0)
    {
        return;
    }

#if SUBBAND_ENABLE_LEGACY_FIR_BACKEND
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
#endif

    SubbandWOLA_Init();
    SubbandPolyphase_Init();

    Subband_FilterBank_Ready = 1;
}

#if SUBBAND_ENABLE_LEGACY_FIR_BACKEND

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

#ifndef SUBBAND_FLOW_ALGO_ONLY

static int Subband_Normalize_Backend(int backend)
{
    switch (backend)
    {
    case SUBBAND_BACKEND_WOLA:
    case SUBBAND_BACKEND_LEGACY_FIR:
    case SUBBAND_BACKEND_POLYPHASE:
        return backend;
    default:
        return SUBBAND_BACKEND_WOLA;
    }
}

static void Subband_Reset_Benchmark_Counters(void)
{
    SUBBAND_DebugAdFrames = 0UL;
    SUBBAND_DebugDaFrames = 0UL;
    SUBBAND_FilterbankFrames = 0UL;
    SUBBAND_DebugAlgoFrames = 0UL;
    SUBBAND_DebugLastCycles = 0UL;
    SUBBAND_DebugMaxCycles = 0UL;
    SUBBAND_DebugLastMs = 0.0f;
    SUBBAND_DebugMaxMs = 0.0f;
    SUBBAND_DebugCpuUsagePercent = 0.0f;
    SUBBAND_DebugInputNonzeroFrames = 0UL;
    SUBBAND_DebugOutputNonzeroFrames = 0UL;
    SUBBAND_DebugInputClipFrames = 0UL;
    SUBBAND_DebugOutputClipFrames = 0UL;
    SUBBAND_DebugInputPeakMax = 0;
    SUBBAND_DebugOutputPeakMax = 0;
    SUBBAND_DebugInputMeanSquareAvg = 0UL;
    SUBBAND_DebugOutputMeanSquareAvg = 0UL;
    SUBBAND_EVAL_DebugAdFrames = 0UL;
    SUBBAND_EVAL_DebugDaFrames = 0UL;
}

static void Subband_Service_Benchmark_Backend(void)
{
    int backend;

    backend = Subband_Normalize_Backend(SUBBAND_DebugRequestedBackend);
    if (backend != SUBBAND_DebugRequestedBackend)
    {
        SUBBAND_DebugRequestedBackend = backend;
    }

    if (backend != SUBBAND_DebugAppliedBackend)
    {
        SubbandWOLA_ResetStream();
        SubbandWOLA_ResetAllGains();
#if SUBBAND_ENABLE_LEGACY_FIR_BACKEND
        Clear_FilterBank_State();
#endif
        SubbandPolyphase_Reset();
        Subband_Reset_Benchmark_Counters();
        SUBBAND_DebugAppliedBackend = backend;
        SUBBAND_DebugBackendChanges++;
    }

    if (SUBBAND_DebugBenchmarkResetRequest != 0UL)
    {
        Subband_Reset_Benchmark_Counters();
        SUBBAND_DebugBenchmarkResetRequest = 0UL;
    }
}

static void Subband_Update_Benchmark_Debug(const short *in, const short *out,
                                           unsigned long cycles)
{
    int input_peak;
    int output_peak;
    unsigned long input_mean_square;
    unsigned long output_mean_square;
    unsigned long frames;

    input_peak = Subband_Frame_Peak(in);
    output_peak = Subband_Frame_Peak(out);
    input_mean_square = Subband_Frame_Mean_Square(in);
    output_mean_square = Subband_Frame_Mean_Square(out);
    SUBBAND_DebugAlgoFrames++;
    frames = SUBBAND_DebugAlgoFrames;
    if (frames == 1UL)
    {
        SUBBAND_DebugInputMeanSquareAvg = input_mean_square;
        SUBBAND_DebugOutputMeanSquareAvg = output_mean_square;
    }
    else
    {
        SUBBAND_DebugInputMeanSquareAvg =
            (unsigned long)(((unsigned long long)
                             SUBBAND_DebugInputMeanSquareAvg *
                             (unsigned long long)(frames - 1UL) +
                             (unsigned long long)input_mean_square) /
                            (unsigned long long)frames);
        SUBBAND_DebugOutputMeanSquareAvg =
            (unsigned long)(((unsigned long long)
                             SUBBAND_DebugOutputMeanSquareAvg *
                             (unsigned long long)(frames - 1UL) +
                             (unsigned long long)output_mean_square) /
                            (unsigned long long)frames);
    }
    SUBBAND_DebugLastCycles = cycles;
    SUBBAND_DebugLastMs = (float)cycles / SUBBAND_CPU_CYCLES_PER_MS;
    if (cycles > SUBBAND_DebugMaxCycles)
    {
        SUBBAND_DebugMaxCycles = cycles;
        SUBBAND_DebugMaxMs = SUBBAND_DebugLastMs;
    }
    SUBBAND_DebugCpuUsagePercent =
        (SUBBAND_DebugMaxMs /
         (((float)SUBBAND_FRM_LEN / SUBBAND_SAMPLE_RATE) * 1000.0f)) * 100.0f;

    if (input_peak > 64)
    {
        SUBBAND_DebugInputNonzeroFrames++;
    }
    if (output_peak > 64)
    {
        SUBBAND_DebugOutputNonzeroFrames++;
    }
    if (input_peak >= 32767)
    {
        SUBBAND_DebugInputClipFrames++;
    }
    if (output_peak >= 32767)
    {
        SUBBAND_DebugOutputClipFrames++;
    }
    if (input_peak > SUBBAND_DebugInputPeakMax)
    {
        SUBBAND_DebugInputPeakMax = input_peak;
    }
    if (output_peak > SUBBAND_DebugOutputPeakMax)
    {
        SUBBAND_DebugOutputPeakMax = output_peak;
    }
}

#endif /* SUBBAND_FLOW_ALGO_ONLY */

void Subband_Process_1024(short *in, short *out)
{
#ifndef SUBBAND_FLOW_ALGO_ONLY
    int backend;
#if defined(__TI_COMPILER_VERSION__) || defined(__TMS320C6X__)
    unsigned int cycle_start;
    unsigned int cycle_end;
    unsigned long cycle_delta;
#endif
#endif

    if ((in == 0) || (out == 0))
    {
        return;
    }

    Subband_FilterBank_Init();

#ifndef SUBBAND_FLOW_ALGO_ONLY
    backend = Subband_Normalize_Backend(SUBBAND_DebugAppliedBackend);
#if defined(__TI_COMPILER_VERSION__) || defined(__TMS320C6X__)
    cycle_start = TSCL;
#endif
#else
    {
        int backend;

        backend = SUBBAND_BACKEND_WOLA;
#endif
#if SUBBAND_ENABLE_LEGACY_FIR_BACKEND
        if (backend == SUBBAND_BACKEND_LEGACY_FIR)
        {
            Analysis_Filter_1024(in);
            Subband_Direct_Through_1024();
            Synthesis_Filter_1024(out);
        }
        else
#endif
        if (backend == SUBBAND_BACKEND_POLYPHASE)
        {
            SubbandPolyphase_ProcessFrame(in, out, SUBBAND_FRM_LEN);
        }
        else
        {
            SubbandWOLA_ProcessFrame(in, out);
        }
#ifndef SUBBAND_FLOW_ALGO_ONLY
    SUBBAND_FilterbankFrames++;
#if defined(__TI_COMPILER_VERSION__) || defined(__TMS320C6X__)
    cycle_end = TSCL;
    cycle_delta = (unsigned long)(cycle_end - cycle_start);
    Subband_Update_Benchmark_Debug(in, out, cycle_delta);
    SubbandUI_RecordAlgoCycles(cycle_delta);
#else
    Subband_Update_Benchmark_Debug(in, out, 0UL);
#endif
#else
    }
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
    SUBBAND_DebugAdFirstSample = AD_Buffer1[0];
    SUBBAND_DebugDaFirstSample = DA_Buffer1[0];
    SUBBAND_DebugAdPeak = Subband_Frame_Peak(AD_Buffer1);
    SUBBAND_DebugDaPeak = Subband_Frame_Peak(DA_Buffer1);
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

#if SUBBAND_THD_BOARD_TEST
void Subband_THD_Board_Test_Example(void)
{
    unsigned long frame;
    unsigned long sample;
    unsigned long packed_index;

    /* Volatile reads keep the JTAG-visible identity symbols in the image. */
    if ((SUBBAND_THD_DebugBuildDirty != 0) ||
        (SUBBAND_THD_DebugBuildGitSha[0] == '\0'))
    {
        SUBBAND_THD_DebugStatus = SUBBAND_THD_STATUS_BOOT;
        while (1)
        {
        }
    }
    /* GEL has initialized the core and DDR; no ADC/DAC or timer setup is needed. */
    SubbandWOLA_Init();
    SubbandWOLA_ResetStream();
    SubbandWOLA_ResetAllGains();
    SubbandWOLA_SetBypass(0);
    memset(SUBBAND_THD_OutputPacked, 0, sizeof(SUBBAND_THD_OutputPacked));

    SUBBAND_THD_DebugFrames = 0UL;
    SUBBAND_THD_DebugCycleCount = 0UL;
    SUBBAND_THD_DebugMaxFrameCycles = 0UL;
    SUBBAND_THD_DebugStatus = SUBBAND_THD_STATUS_WAITING;
    while (SUBBAND_THD_DebugRequest == 0U)
    {
    }

    SUBBAND_THD_DebugStatus = SUBBAND_THD_STATUS_RUNNING;
    for (frame = 0UL; frame < SUBBAND_THD_FRAME_COUNT; frame++)
    {
#if defined(__TI_COMPILER_VERSION__) || defined(__TMS320C6X__)
        unsigned int start_cycles;
        unsigned long frame_cycles;

        start_cycles = TSCL;
#endif
        SubbandWOLA_ProcessFrame(
            &SUBBAND_THD_Input[frame * SUBBAND_FRM_LEN],
            SUBBAND_THD_FrameOutput);
        packed_index = frame * (SUBBAND_FRM_LEN / 2);
        for (sample = 0UL; sample < SUBBAND_FRM_LEN; sample += 2UL)
        {
            SUBBAND_THD_OutputPacked[packed_index++] =
                (unsigned int)(unsigned short)SUBBAND_THD_FrameOutput[sample] |
                ((unsigned int)(unsigned short)SUBBAND_THD_FrameOutput[sample + 1UL] << 16);
        }
#if defined(__TI_COMPILER_VERSION__) || defined(__TMS320C6X__)
        frame_cycles = (unsigned long)(TSCL - start_cycles);
        SUBBAND_THD_DebugCycleCount += frame_cycles;
        if (frame_cycles > SUBBAND_THD_DebugMaxFrameCycles)
        {
            SUBBAND_THD_DebugMaxFrameCycles = frame_cycles;
        }
#endif
        SUBBAND_THD_DebugFrames = frame + 1UL;
    }
    SUBBAND_THD_DebugStatus = SUBBAND_THD_STATUS_READY;
    while (1)
    {
    }
}
#endif

void Subband_Flow_Example(void)
{
    unsigned char FLAG_AD_DONE = 0;
    unsigned char audio_serviced;
    unsigned char touch_serviced;

    Sys_Init();
    Key_Init();

    Adc_Init(ADC_50KHZ, ADC_SAMPLE_1024);
    Dac_Init(DAC_50KHZ, DAC_SAMPLE_1024, DAC_CHANNEL_ALL);
    Subband_FilterBank_Init();
    Subband_Service_Demo_Mode();
    SubbandUI_Init();
    Tim2_Init(TIMER_20HZ);

    Adc_Start();
    Dac_Start();

    while (1)
    {
        audio_serviced = 0U;
        touch_serviced = 0U;
        Subband_Service_Demo_Mode();
        Subband_Service_Benchmark_Backend();

        if (FLAG_AD == 1)
        {
            audio_serviced = 1U;
            FLAG_AD = 0;
            FLAG_AD_DONE = 1;
            SUBBAND_DebugAdFrames++;
            SUBBAND_EVAL_DebugAdFrames = SUBBAND_DebugAdFrames;

            Capture_Adc_Frame();
        }

        if (FLAG_DA == 1 && FLAG_AD_DONE == 1)
        {
            audio_serviced = 1U;
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

        if ((FLAG_TOUCH != 0U) || (FLAG_TIM2 != 0U))
        {
            unsigned char force_touch_scan;

            force_touch_scan = (FLAG_TIM2 != 0U) ? 1U : 0U;
            FLAG_TIM2 = 0U;
            SubbandUI_ServiceTouch(force_touch_scan);
            touch_serviced = 1U;
        }
        if ((FLAG_AD == 0U) &&
            (FLAG_DA == 0U) &&
            (FLAG_AD_DONE == 0U) &&
            (audio_serviced == 0U) &&
            (touch_serviced == 0U))
        {
            SubbandUI_ServiceDisplay();
        }
    }
}

#endif /* SUBBAND_FLOW_ALGO_ONLY */
