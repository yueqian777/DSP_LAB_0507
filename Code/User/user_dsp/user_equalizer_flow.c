/**
 * user_equalizer_flow.c
 *
 * Project 3.3 board-side AD/DA equalizer example. CH1 is processed by
 * the equalizer; CH2-CH8 are passed through in the first version.
 */

#include "user_equalizer_flow.h"
#include "user_equalizer_display.h"
#include "string.h"

#ifndef EQ_ALGO_ONLY

#include "dac_api.h"
#include "key_api.h"
#include "system.h"

#if defined(__TI_COMPILER_VERSION__) || defined(__TMS320C6X__)
#include "c6x.h"
#endif

#define EQ_CPU_CYCLES_PER_MS 456000.0f

static short EQ_AD_Buffer1[ADC_SAMPLE_1024];
static short EQ_AD_Buffer2[ADC_SAMPLE_1024];
static short EQ_AD_Buffer3[ADC_SAMPLE_1024];
static short EQ_AD_Buffer4[ADC_SAMPLE_1024];
static short EQ_AD_Buffer5[ADC_SAMPLE_1024];
static short EQ_AD_Buffer6[ADC_SAMPLE_1024];
static short EQ_AD_Buffer7[ADC_SAMPLE_1024];
static short EQ_AD_Buffer8[ADC_SAMPLE_1024];
static short EQ_DA_Buffer1[DAC_SAMPLE_1024];
static EQ_STATE EQ_BoardState;
static int EQ_AppliedMode = -1;
static int EQ_AppliedDiagPath = -1;

#endif

volatile unsigned long EQ_DebugAdFrames = 0UL;
volatile unsigned long EQ_DebugDaFrames = 0UL;
volatile unsigned long EQ_DebugProcessFrames = 0UL;
volatile unsigned long EQ_DebugLastCycles = 0UL;
volatile unsigned long EQ_DebugMaxCycles = 0UL;
volatile float EQ_DebugLastMs = 0.0f;
volatile float EQ_DebugMaxMs = 0.0f;
volatile int EQ_DebugMode = EQ_PRESET_FLAT;
volatile int EQ_DebugDiagPath = EQ_DIAG_PRESET;
volatile float EQ_DebugBandGainDb[EQ_NUM_BANDS] =
{
    0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 0.0f, 0.0f
};
volatile const unsigned long EQ_DebugBuildMagic = 0x33030003UL;
volatile const char EQ_DebugBuildId[] = "P33_FIX_c0eb163";
volatile const int EQ_DebugBuildDirty = 0;

#ifndef EQ_ALGO_ONLY

static int EQ_NormalizeMode(int mode)
{
    if ((mode < EQ_PRESET_FLAT) || (mode > EQ_PRESET_V_SHAPE))
    {
        return EQ_PRESET_FLAT;
    }
    return mode;
}

static int EQ_NormalizeDiagPath(int path)
{
    if ((path < EQ_DIAG_RAW_COPY) || (path > EQ_DIAG_PRESET))
    {
        return EQ_DIAG_PRESET;
    }
    return path;
}

static void EQ_KeepBuildFingerprint(void)
{
    volatile const unsigned long *magic = &EQ_DebugBuildMagic;
    volatile const char *build_id = EQ_DebugBuildId;
    volatile const int *dirty = &EQ_DebugBuildDirty;

    (void)*magic;
    (void)*build_id;
    (void)*dirty;
}

static void EQ_UpdateDebugGains(void)
{
    int band;

    for (band = 0; band < EQ_NUM_BANDS; band++)
    {
        EQ_DebugBandGainDb[band] =
            Equalizer_GetBandTargetGainDb(&EQ_BoardState, band);
    }
    EQ_DebugClipCount = EQ_BoardState.clip_count;
}

static void EQ_ServiceMode(void)
{
    int diag_path;
    int mode;

    diag_path = EQ_NormalizeDiagPath(EQ_DebugDiagPath);
    if (EQ_DebugDiagPath != diag_path)
    {
        EQ_DebugDiagPath = diag_path;
    }
    mode = EQ_NormalizeMode(EQ_DebugMode);
    if (EQ_DebugMode != mode)
    {
        EQ_DebugMode = mode;
    }
    if (diag_path != EQ_AppliedDiagPath)
    {
        Equalizer_SetBypass(&EQ_BoardState, 0);
        if (diag_path == EQ_DIAG_RAW_COPY)
        {
            Equalizer_SetCoreMode(&EQ_BoardState, EQ_CORE_RAW_COPY);
            EQ_AppliedMode = -1;
        }
        else if (diag_path == EQ_DIAG_FLOAT_COPY)
        {
            Equalizer_SetCoreMode(&EQ_BoardState, EQ_CORE_FLOAT_COPY);
            EQ_AppliedMode = -1;
        }
        else if (diag_path == EQ_DIAG_FLAT)
        {
            Equalizer_SetCoreMode(&EQ_BoardState, EQ_CORE_RBJ_CASCADE);
            Equalizer_ApplyPreset(&EQ_BoardState, EQ_PRESET_FLAT);
            EQ_AppliedMode = EQ_PRESET_FLAT;
        }
        else if (diag_path == EQ_DIAG_SINGLE_BAND)
        {
            Equalizer_SetCoreMode(&EQ_BoardState, EQ_CORE_RBJ_CASCADE);
            Equalizer_ApplySingleBand1kPlus3Db(&EQ_BoardState);
            EQ_AppliedMode = -1;
        }
        else
        {
            Equalizer_SetCoreMode(&EQ_BoardState, EQ_CORE_RBJ_CASCADE);
            Equalizer_ApplyPreset(&EQ_BoardState, mode);
            EQ_AppliedMode = mode;
        }
        EQ_AppliedDiagPath = diag_path;
    }
    else if ((diag_path == EQ_DIAG_PRESET) && (mode != EQ_AppliedMode))
    {
        Equalizer_SetCoreMode(&EQ_BoardState, EQ_CORE_RBJ_CASCADE);
        Equalizer_ApplyPreset(&EQ_BoardState, mode);
        EQ_AppliedMode = mode;
    }
    EQ_UpdateDebugGains();
}

static void EQ_UpdateTiming(unsigned long cycles)
{
    EQ_DebugLastCycles = cycles;
    EQ_DebugLastMs = (float)cycles / EQ_CPU_CYCLES_PER_MS;
    if (cycles > EQ_DebugMaxCycles)
    {
        EQ_DebugMaxCycles = cycles;
        EQ_DebugMaxMs = EQ_DebugLastMs;
    }
}

static void EQ_CaptureAdcFrame(void)
{
#if defined(__TI_COMPILER_VERSION__) || defined(__TMS320C6X__)
    unsigned int cycle_start;
    unsigned int cycle_end;
    unsigned long cycle_delta;
#endif

    if (AD_Ping_Pong == AD_BUFFER_PONG)
    {
        memcpy(EQ_AD_Buffer1, AD_CH1_Buf0, 2 * ADC_SAMPLE_1024);
        memcpy(EQ_AD_Buffer2, AD_CH2_Buf0, 2 * ADC_SAMPLE_1024);
        memcpy(EQ_AD_Buffer3, AD_CH3_Buf0, 2 * ADC_SAMPLE_1024);
        memcpy(EQ_AD_Buffer4, AD_CH4_Buf0, 2 * ADC_SAMPLE_1024);
        memcpy(EQ_AD_Buffer5, AD_CH5_Buf0, 2 * ADC_SAMPLE_1024);
        memcpy(EQ_AD_Buffer6, AD_CH6_Buf0, 2 * ADC_SAMPLE_1024);
        memcpy(EQ_AD_Buffer7, AD_CH7_Buf0, 2 * ADC_SAMPLE_1024);
        memcpy(EQ_AD_Buffer8, AD_CH8_Buf0, 2 * ADC_SAMPLE_1024);
    }
    else
    {
        memcpy(EQ_AD_Buffer1, AD_CH1_Buf1, 2 * ADC_SAMPLE_1024);
        memcpy(EQ_AD_Buffer2, AD_CH2_Buf1, 2 * ADC_SAMPLE_1024);
        memcpy(EQ_AD_Buffer3, AD_CH3_Buf1, 2 * ADC_SAMPLE_1024);
        memcpy(EQ_AD_Buffer4, AD_CH4_Buf1, 2 * ADC_SAMPLE_1024);
        memcpy(EQ_AD_Buffer5, AD_CH5_Buf1, 2 * ADC_SAMPLE_1024);
        memcpy(EQ_AD_Buffer6, AD_CH6_Buf1, 2 * ADC_SAMPLE_1024);
        memcpy(EQ_AD_Buffer7, AD_CH7_Buf1, 2 * ADC_SAMPLE_1024);
        memcpy(EQ_AD_Buffer8, AD_CH8_Buf1, 2 * ADC_SAMPLE_1024);
    }

#if defined(__TI_COMPILER_VERSION__) || defined(__TMS320C6X__)
    cycle_start = TSCL;
#endif
    Equalizer_ProcessFrame(&EQ_BoardState, EQ_AD_Buffer1, EQ_DA_Buffer1,
                           ADC_SAMPLE_1024);
    EQ_DebugProcessFrames++;
#if defined(__TI_COMPILER_VERSION__) || defined(__TMS320C6X__)
    cycle_end = TSCL;
    cycle_delta = (unsigned long)(cycle_end - cycle_start);
    EQ_UpdateTiming(cycle_delta);
#else
    EQ_UpdateTiming(0UL);
#endif
}

static void EQ_FillDacPingBuffer(void)
{
    memcpy(DA_CH1_Buf0, EQ_DA_Buffer1, 2 * DAC_SAMPLE_1024);
    memcpy(DA_CH2_Buf0, EQ_AD_Buffer2, 2 * DAC_SAMPLE_1024);
    memcpy(DA_CH3_Buf0, EQ_AD_Buffer3, 2 * DAC_SAMPLE_1024);
    memcpy(DA_CH4_Buf0, EQ_AD_Buffer4, 2 * DAC_SAMPLE_1024);
    memcpy(DA_CH5_Buf0, EQ_AD_Buffer5, 2 * DAC_SAMPLE_1024);
    memcpy(DA_CH6_Buf0, EQ_AD_Buffer6, 2 * DAC_SAMPLE_1024);
    memcpy(DA_CH7_Buf0, EQ_AD_Buffer7, 2 * DAC_SAMPLE_1024);
    memcpy(DA_CH8_Buf0, EQ_AD_Buffer8, 2 * DAC_SAMPLE_1024);
}

static void EQ_FillDacPongBuffer(void)
{
    memcpy(DA_CH1_Buf1, EQ_DA_Buffer1, 2 * DAC_SAMPLE_1024);
    memcpy(DA_CH2_Buf1, EQ_AD_Buffer2, 2 * DAC_SAMPLE_1024);
    memcpy(DA_CH3_Buf1, EQ_AD_Buffer3, 2 * DAC_SAMPLE_1024);
    memcpy(DA_CH4_Buf1, EQ_AD_Buffer4, 2 * DAC_SAMPLE_1024);
    memcpy(DA_CH5_Buf1, EQ_AD_Buffer5, 2 * DAC_SAMPLE_1024);
    memcpy(DA_CH6_Buf1, EQ_AD_Buffer6, 2 * DAC_SAMPLE_1024);
    memcpy(DA_CH7_Buf1, EQ_AD_Buffer7, 2 * DAC_SAMPLE_1024);
    memcpy(DA_CH8_Buf1, EQ_AD_Buffer8, 2 * DAC_SAMPLE_1024);
}

static void EQ_ClearDacBuffers(void)
{
    memset(EQ_DA_Buffer1, 0, sizeof(EQ_DA_Buffer1));
    memset(DA_CH1_Buf0, 0, 2 * DAC_SAMPLE_1024);
    memset(DA_CH2_Buf0, 0, 2 * DAC_SAMPLE_1024);
    memset(DA_CH3_Buf0, 0, 2 * DAC_SAMPLE_1024);
    memset(DA_CH4_Buf0, 0, 2 * DAC_SAMPLE_1024);
    memset(DA_CH5_Buf0, 0, 2 * DAC_SAMPLE_1024);
    memset(DA_CH6_Buf0, 0, 2 * DAC_SAMPLE_1024);
    memset(DA_CH7_Buf0, 0, 2 * DAC_SAMPLE_1024);
    memset(DA_CH8_Buf0, 0, 2 * DAC_SAMPLE_1024);
    memset(DA_CH1_Buf1, 0, 2 * DAC_SAMPLE_1024);
    memset(DA_CH2_Buf1, 0, 2 * DAC_SAMPLE_1024);
    memset(DA_CH3_Buf1, 0, 2 * DAC_SAMPLE_1024);
    memset(DA_CH4_Buf1, 0, 2 * DAC_SAMPLE_1024);
    memset(DA_CH5_Buf1, 0, 2 * DAC_SAMPLE_1024);
    memset(DA_CH6_Buf1, 0, 2 * DAC_SAMPLE_1024);
    memset(DA_CH7_Buf1, 0, 2 * DAC_SAMPLE_1024);
    memset(DA_CH8_Buf1, 0, 2 * DAC_SAMPLE_1024);
}

static void EQ_FillDacInactiveBuffer(void)
{
    if (DA_Ping_Pong == DA_BUFFER_PONG)
    {
        EQ_FillDacPingBuffer();
    }
    else
    {
        EQ_FillDacPongBuffer();
    }
}

void Equalizer_Flow_Example(void)
{
    unsigned char flag_ad_done;
#if EQ_ENABLE_LCD_DISPLAY != 0
    unsigned long lcd_status_frame;
    int lcd_mode;
#endif

    flag_ad_done = 0;
#if EQ_ENABLE_LCD_DISPLAY != 0
    lcd_status_frame = 0UL;
    lcd_mode = -1;
#endif
    Sys_Init();
    Key_Init();
    EQ_KeepBuildFingerprint();

    Adc_Init(ADC_50KHZ, ADC_SAMPLE_1024);
    Dac_Init(DAC_50KHZ, DAC_SAMPLE_1024, DAC_CHANNEL_ALL);
    Equalizer_Init(&EQ_BoardState);
    EQ_ServiceMode();
    EQ_ClearDacBuffers();
#if EQ_ENABLE_LCD_DISPLAY != 0
    EqualizerDisplay_Init();
    EqualizerDisplay_UpdateAll(&EQ_BoardState);
    EqualizerDisplay_UpdateStatus(EQ_DebugProcessFrames,
                                  EQ_DebugLastMs,
                                  EQ_DebugMaxMs,
                                  EQ_DebugClipCount,
                                  EQ_DebugMode);
    lcd_mode = EQ_AppliedMode;
#endif

#if defined(__TI_COMPILER_VERSION__) || defined(__TMS320C6X__)
    TSCL = 0;
    TSCH = 0;
#endif

    Adc_Start();
    Dac_Start();

    while (1)
    {
        if (FLAG_AD == 1)
        {
            FLAG_AD = 0;
            flag_ad_done = 1;
            EQ_DebugAdFrames++;
            EQ_CaptureAdcFrame();
        }

        if ((FLAG_DA == 1) && (flag_ad_done == 1))
        {
            FLAG_DA = 0;
            flag_ad_done = 0;
            EQ_DebugDaFrames++;
            EQ_FillDacInactiveBuffer();
        }

        /* Defer bank selection until the current AD/DA work is complete. */
        EQ_ServiceMode();

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

#if EQ_ENABLE_LCD_DISPLAY != 0
        if (EQ_AppliedMode != lcd_mode)
        {
            lcd_mode = EQ_AppliedMode;
            EqualizerDisplay_UpdateGains(&EQ_BoardState);
            EqualizerDisplay_UpdateStatus(EQ_DebugProcessFrames,
                                          EQ_DebugLastMs,
                                          EQ_DebugMaxMs,
                                          EQ_DebugClipCount,
                                          EQ_DebugMode);
        }
        if ((EQ_DebugProcessFrames != lcd_status_frame) &&
            ((EQ_DebugProcessFrames % EQ_LCD_REFRESH_FRAMES) == 0UL))
        {
            lcd_status_frame = EQ_DebugProcessFrames;
            EqualizerDisplay_UpdateStatus(EQ_DebugProcessFrames,
                                          EQ_DebugLastMs,
                                          EQ_DebugMaxMs,
                                          EQ_DebugClipCount,
                                          EQ_DebugMode);
        }
#endif
    }
}

#else

void Equalizer_Flow_Example(void)
{
}

#endif
