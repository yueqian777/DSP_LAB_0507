/**
 * user_adda.c
 *
 * ADDA example for audio passthrough.
 */

#include "adc_api.h"
#include "dac_api.h"
#include "system.h"
#include "math.h"
#include "key_api.h"
#include "string.h"

#define ADDA_SAMPLE_BYTES            (2u * ADC_SAMPLE_1024)
#define ADDA_STARTUP_TONE_CYCLES     (9u)
#define ADDA_STARTUP_TONE_AMPLITUDE  (8000.0f)
#define ADDA_STARTUP_TONE_FRAMES     (12u)
#define ADDA_SILENCE_FLUSH_FRAMES    (2u)
#define ADDA_PI                      (3.14159265358979323846f)
#define ADDA_ENABLE_DAC_OUTPUT       (1u)

static short User_Buffer1[ADC_SAMPLE_1024];
static short User_Buffer2[ADC_SAMPLE_1024];
static short User_OutputBuffer[DAC_SAMPLE_1024];
static short User_StartupTone[DAC_SAMPLE_1024];
static short User_Silence[DAC_SAMPLE_1024];

volatile unsigned long ADDA_DebugAdFrames = 0;
volatile unsigned long ADDA_DebugDaFrames = 0;
volatile unsigned short ADDA_DebugCh1Peak = 0;
volatile unsigned short ADDA_DebugCh2Peak = 0;
volatile unsigned short ADDA_DebugOutputPeak = 0;
volatile unsigned char ADDA_DebugMode = 0;
volatile unsigned char ADDA_DebugLastAdFlag = 0;
volatile unsigned char ADDA_DebugLastAdPingPong = 0;
volatile unsigned char ADDA_DebugLastDaPingPong = 0;

static void Build_Startup_Tone(void)
{
    unsigned int i;
    float phaseStep;

    phaseStep = (2.0f * ADDA_PI * (float)ADDA_STARTUP_TONE_CYCLES) / (float)DAC_SAMPLE_1024;

    for (i = 0; i < DAC_SAMPLE_1024; i++)
    {
        User_StartupTone[i] = (short)(ADDA_STARTUP_TONE_AMPLITUDE * sin(phaseStep * (float)i));
    }
}

static short Clip_To_Short(long value)
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

static unsigned short Abs_Short(short value)
{
    long sample;

    sample = (long)value;
    if (sample < 0L)
    {
        sample = -sample;
    }
    if (sample > 32768L)
    {
        sample = 32768L;
    }

    return (unsigned short)sample;
}

static void Update_Debug_Peaks(unsigned short ch1Peak, unsigned short ch2Peak, unsigned short outputPeak)
{
    ADDA_DebugCh1Peak = ch1Peak;
    ADDA_DebugCh2Peak = ch2Peak;
    ADDA_DebugOutputPeak = outputPeak;
}

static void Build_Passthrough_Mono(void)
{
    unsigned int i;
    unsigned short ch1Peak;
    unsigned short ch2Peak;
    unsigned short outputPeak;
    unsigned short samplePeak;
    long mixed;

    ch1Peak = 0;
    ch2Peak = 0;
    outputPeak = 0;

    for (i = 0; i < ADC_SAMPLE_1024; i++)
    {
        mixed = ((long)User_Buffer1[i] + (long)User_Buffer2[i]) / 2L;
        User_OutputBuffer[i] = Clip_To_Short(mixed);

        samplePeak = Abs_Short(User_Buffer1[i]);
        if (samplePeak > ch1Peak)
        {
            ch1Peak = samplePeak;
        }

        samplePeak = Abs_Short(User_Buffer2[i]);
        if (samplePeak > ch2Peak)
        {
            ch2Peak = samplePeak;
        }

        samplePeak = Abs_Short(User_OutputBuffer[i]);
        if (samplePeak > outputPeak)
        {
            outputPeak = samplePeak;
        }
    }

    Update_Debug_Peaks(ch1Peak, ch2Peak, outputPeak);
}

static void Fill_Dac_Ping_Buffer(const short *buffer)
{
    memcpy(DA_CH1_Buf0, buffer, ADDA_SAMPLE_BYTES);
    memcpy(DA_CH2_Buf0, buffer, ADDA_SAMPLE_BYTES);
}

static void Fill_Dac_Pong_Buffer(const short *buffer)
{
    memcpy(DA_CH1_Buf1, buffer, ADDA_SAMPLE_BYTES);
    memcpy(DA_CH2_Buf1, buffer, ADDA_SAMPLE_BYTES);
}

static void Fill_Dac_Inactive_Buffer(const short *buffer)
{
    if (DA_Ping_Pong == DA_BUFFER_PONG)
    {
        Fill_Dac_Ping_Buffer(buffer);
    }
    else
    {
        Fill_Dac_Pong_Buffer(buffer);
    }
}

static void Fill_Dac_Buffers(const short *buffer)
{
    Fill_Dac_Ping_Buffer(buffer);
    Fill_Dac_Pong_Buffer(buffer);
}

static void Capture_Adc_Input(void)
{
    ADDA_DebugLastAdPingPong = AD_Ping_Pong;

    if (AD_Ping_Pong == AD_BUFFER_PONG)
    {
        memcpy(User_Buffer1, AD_CH1_Buf0, ADDA_SAMPLE_BYTES);
        memcpy(User_Buffer2, AD_CH2_Buf0, ADDA_SAMPLE_BYTES);
    }
    else
    {
        memcpy(User_Buffer1, AD_CH1_Buf1, ADDA_SAMPLE_BYTES);
        memcpy(User_Buffer2, AD_CH2_Buf1, ADDA_SAMPLE_BYTES);
    }
}

static void Update_Debug_Local_View(volatile unsigned long *debugAdFramesLocal,
                                    volatile unsigned long *debugDaFramesLocal,
                                    volatile unsigned short *debugCh1PeakLocal,
                                    volatile unsigned short *debugCh2PeakLocal,
                                    volatile unsigned short *debugOutputPeakLocal,
                                    volatile unsigned char *debugModeLocal,
                                    volatile unsigned char *debugAdEnLocal,
                                    volatile unsigned char *debugDaEnLocal,
                                    volatile unsigned char *debugFlagAdLocal,
                                    volatile unsigned char *debugFlagDaLocal,
                                    volatile unsigned char *debugAdPingPongLocal,
                                    volatile unsigned char *debugDaPingPongLocal)
{
    *debugAdFramesLocal = ADDA_DebugAdFrames;
    *debugDaFramesLocal = ADDA_DebugDaFrames;
    *debugCh1PeakLocal = ADDA_DebugCh1Peak;
    *debugCh2PeakLocal = ADDA_DebugCh2Peak;
    *debugOutputPeakLocal = ADDA_DebugOutputPeak;
    *debugModeLocal = ADDA_DebugMode;
    *debugAdEnLocal = AD_En;
    *debugDaEnLocal = DA_En;
    *debugFlagAdLocal = FLAG_AD;
    *debugFlagDaLocal = FLAG_DA;
    *debugAdPingPongLocal = AD_Ping_Pong;
    *debugDaPingPongLocal = DA_Ping_Pong;
}

void Adda_Example(void)
{
    volatile unsigned long debugAdFramesLocal;
    volatile unsigned long debugDaFramesLocal;
    volatile unsigned short debugCh1PeakLocal;
    volatile unsigned short debugCh2PeakLocal;
    volatile unsigned short debugOutputPeakLocal;
    volatile unsigned char debugModeLocal;
    volatile unsigned char debugAdEnLocal;
    volatile unsigned char debugDaEnLocal;
    volatile unsigned char debugFlagAdLocal;
    volatile unsigned char debugFlagDaLocal;
    volatile unsigned char debugAdPingPongLocal;
    volatile unsigned char debugDaPingPongLocal;
    unsigned char flagAdDone;
    unsigned int startupFramesLeft;
    unsigned int silenceFramesLeft;

    flagAdDone = 0;
    startupFramesLeft = ADDA_STARTUP_TONE_FRAMES;
    silenceFramesLeft = ADDA_SILENCE_FLUSH_FRAMES;
    Update_Debug_Local_View(&debugAdFramesLocal,
                            &debugDaFramesLocal,
                            &debugCh1PeakLocal,
                            &debugCh2PeakLocal,
                            &debugOutputPeakLocal,
                            &debugModeLocal,
                            &debugAdEnLocal,
                            &debugDaEnLocal,
                            &debugFlagAdLocal,
                            &debugFlagDaLocal,
                            &debugAdPingPongLocal,
                            &debugDaPingPongLocal);

    Build_Startup_Tone();

    Sys_Init();
    Key_Init();

    Adc_Init(ADC_50KHZ, ADC_SAMPLE_1024);
#if ADDA_ENABLE_DAC_OUTPUT
    Dac_Init(DAC_50KHZ, DAC_SAMPLE_1024, DAC_CHANNEL_12);
#else
    DA_En = 0;
    FLAG_DA = 0;
    ADDA_DebugMode = 4;
#endif

    FLAG_AD = 0;
#if ADDA_ENABLE_DAC_OUTPUT
    FLAG_DA = 0;
    Fill_Dac_Buffers(User_StartupTone);
#endif

    Adc_Start();
#if ADDA_ENABLE_DAC_OUTPUT
    Dac_Start();
#endif

    while (1)
    {
        if (FLAG_AD == 1)
        {
            ADDA_DebugAdFrames++;
            ADDA_DebugLastAdFlag = FLAG_AD;
            FLAG_AD = 0;
            Capture_Adc_Input();
            Build_Passthrough_Mono();
            flagAdDone = 1;
        }
        else if (FLAG_AD != 0)
        {
            ADDA_DebugLastAdFlag = FLAG_AD;
        }

        Update_Debug_Local_View(&debugAdFramesLocal,
                                &debugDaFramesLocal,
                                &debugCh1PeakLocal,
                                &debugCh2PeakLocal,
                                &debugOutputPeakLocal,
                                &debugModeLocal,
                                &debugAdEnLocal,
                                &debugDaEnLocal,
                                &debugFlagAdLocal,
                                &debugFlagDaLocal,
                                &debugAdPingPongLocal,
                                &debugDaPingPongLocal);

#if ADDA_ENABLE_DAC_OUTPUT
        if (FLAG_DA == 1)
        {
            ADDA_DebugDaFrames++;
            ADDA_DebugLastDaPingPong = DA_Ping_Pong;
            FLAG_DA = 0;

            if (startupFramesLeft > 0u)
            {
                ADDA_DebugMode = 1;
                Fill_Dac_Inactive_Buffer(User_StartupTone);
                startupFramesLeft--;
            }
            else if (flagAdDone == 1)
            {
                ADDA_DebugMode = 2;
                Fill_Dac_Inactive_Buffer(User_OutputBuffer);
                flagAdDone = 0;
                silenceFramesLeft = 0;
            }
            else if (silenceFramesLeft > 0u)
            {
                ADDA_DebugMode = 3;
                Fill_Dac_Inactive_Buffer(User_Silence);
                silenceFramesLeft--;
            }
        }
#endif

        Update_Debug_Local_View(&debugAdFramesLocal,
                                &debugDaFramesLocal,
                                &debugCh1PeakLocal,
                                &debugCh2PeakLocal,
                                &debugOutputPeakLocal,
                                &debugModeLocal,
                                &debugAdEnLocal,
                                &debugDaEnLocal,
                                &debugFlagAdLocal,
                                &debugFlagDaLocal,
                                &debugAdPingPongLocal,
                                &debugDaPingPongLocal);

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
#if ADDA_ENABLE_DAC_OUTPUT
            Dac_Start();
#endif
        }

        if (FLAG_KEY4 == 1)
        {
            FLAG_KEY4 = 0;
#if ADDA_ENABLE_DAC_OUTPUT
            Dac_Stop();
#endif
        }
    }
}
