/**
 * user_subband_flow.c
 *
 * Flow-only subband analysis/synthesis test. The current analysis step only
 * demultiplexes samples into D phase bands, and synthesis interleaves them
 * back. Real filter coefficients can be added inside the marked stage later.
 */

#include "user_subband_flow.h"
#include "string.h"

void Subband_Flow_Init(SUBBAND_FLOW_STATE *state)
{
    if (state == 0)
    {
        return;
    }

    memset(state->subband, 0, sizeof(state->subband));
}

static void Subband_Flow_Analyze(SUBBAND_FLOW_STATE *state,
                                 const short input[SUBBAND_FLOW_FRAME_LEN])
{
    unsigned int band;
    unsigned int group;
    unsigned int index;

    for (group = 0u; group < SUBBAND_FLOW_SUBBAND_LEN; group++)
    {
        for (band = 0u; band < SUBBAND_FLOW_BAND_COUNT; band++)
        {
            index = group * SUBBAND_FLOW_BAND_COUNT + band;
            state->subband[band][group] = input[index];
        }
    }
}

static void Subband_Flow_ProcessSubbands(SUBBAND_FLOW_STATE *state)
{
    (void)state;
}

static void Subband_Flow_Synthesize(const SUBBAND_FLOW_STATE *state,
                                    short output[SUBBAND_FLOW_FRAME_LEN])
{
    unsigned int band;
    unsigned int group;
    unsigned int index;

    for (group = 0u; group < SUBBAND_FLOW_SUBBAND_LEN; group++)
    {
        for (band = 0u; band < SUBBAND_FLOW_BAND_COUNT; band++)
        {
            index = group * SUBBAND_FLOW_BAND_COUNT + band;
            output[index] = state->subband[band][group];
        }
    }
}

void Subband_Flow_ProcessFrame(SUBBAND_FLOW_STATE *state,
                               const short input[SUBBAND_FLOW_FRAME_LEN],
                               short output[SUBBAND_FLOW_FRAME_LEN])
{
    if ((state == 0) || (input == 0) || (output == 0))
    {
        return;
    }

    Subband_Flow_Analyze(state, input);
    Subband_Flow_ProcessSubbands(state);
    Subband_Flow_Synthesize(state, output);
}

#ifndef SUBBAND_FLOW_ALGO_ONLY

#include "adc_api.h"
#include "dac_api.h"
#include "key_api.h"
#include "system.h"

#define SUBBAND_FLOW_SAMPLE_BYTES  (2u * SUBBAND_FLOW_FRAME_LEN)

static SUBBAND_FLOW_STATE SubbandFlow_State;
static short SubbandFlow_Input[SUBBAND_FLOW_FRAME_LEN];
static short SubbandFlow_Output[SUBBAND_FLOW_FRAME_LEN];
static short SubbandFlow_Silence[SUBBAND_FLOW_FRAME_LEN];

volatile unsigned long SUBBAND_DebugAdFrames = 0;
volatile unsigned long SUBBAND_DebugDaFrames = 0;
volatile unsigned short SUBBAND_DebugInputPeak = 0;
volatile unsigned short SUBBAND_DebugOutputPeak = 0;
volatile unsigned short SUBBAND_DebugBand0Peak = 0;
volatile unsigned char SUBBAND_DebugFrameReady = 0;
volatile unsigned char SUBBAND_DebugLastAdPingPong = 0;
volatile unsigned char SUBBAND_DebugLastDaPingPong = 0;

static short Subband_Flow_ClipToShort(long value)
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

static unsigned short Subband_Flow_AbsShort(short value)
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

static void Subband_Flow_UpdatePeaks(void)
{
    unsigned int index;
    unsigned short samplePeak;
    unsigned short inputPeak;
    unsigned short outputPeak;
    unsigned short band0Peak;

    inputPeak = 0u;
    outputPeak = 0u;
    band0Peak = 0u;

    for (index = 0u; index < SUBBAND_FLOW_FRAME_LEN; index++)
    {
        samplePeak = Subband_Flow_AbsShort(SubbandFlow_Input[index]);
        if (samplePeak > inputPeak)
        {
            inputPeak = samplePeak;
        }

        samplePeak = Subband_Flow_AbsShort(SubbandFlow_Output[index]);
        if (samplePeak > outputPeak)
        {
            outputPeak = samplePeak;
        }
    }

    for (index = 0u; index < SUBBAND_FLOW_SUBBAND_LEN; index++)
    {
        samplePeak = Subband_Flow_AbsShort(SubbandFlow_State.subband[0][index]);
        if (samplePeak > band0Peak)
        {
            band0Peak = samplePeak;
        }
    }

    SUBBAND_DebugInputPeak = inputPeak;
    SUBBAND_DebugOutputPeak = outputPeak;
    SUBBAND_DebugBand0Peak = band0Peak;
}

static void Subband_Flow_CaptureMonoInput(void)
{
    const short *ch1;
    const short *ch2;
    unsigned int index;
    long mixed;

    SUBBAND_DebugLastAdPingPong = AD_Ping_Pong;

    if (AD_Ping_Pong == AD_BUFFER_PONG)
    {
        ch1 = AD_CH1_Buf0;
        ch2 = AD_CH2_Buf0;
    }
    else
    {
        ch1 = AD_CH1_Buf1;
        ch2 = AD_CH2_Buf1;
    }

    for (index = 0u; index < SUBBAND_FLOW_FRAME_LEN; index++)
    {
        mixed = ((long)ch1[index] + (long)ch2[index]) / 2L;
        SubbandFlow_Input[index] = Subband_Flow_ClipToShort(mixed);
    }
}

static void Subband_Flow_FillDacPingBuffer(const short *buffer)
{
    memcpy(DA_CH1_Buf0, buffer, SUBBAND_FLOW_SAMPLE_BYTES);
    memcpy(DA_CH2_Buf0, buffer, SUBBAND_FLOW_SAMPLE_BYTES);
}

static void Subband_Flow_FillDacPongBuffer(const short *buffer)
{
    memcpy(DA_CH1_Buf1, buffer, SUBBAND_FLOW_SAMPLE_BYTES);
    memcpy(DA_CH2_Buf1, buffer, SUBBAND_FLOW_SAMPLE_BYTES);
}

static void Subband_Flow_FillDacBuffers(const short *buffer)
{
    Subband_Flow_FillDacPingBuffer(buffer);
    Subband_Flow_FillDacPongBuffer(buffer);
}

static void Subband_Flow_FillDacInactiveBuffer(const short *buffer)
{
    SUBBAND_DebugLastDaPingPong = DA_Ping_Pong;

    if (DA_Ping_Pong == DA_BUFFER_PONG)
    {
        Subband_Flow_FillDacPingBuffer(buffer);
    }
    else
    {
        Subband_Flow_FillDacPongBuffer(buffer);
    }
}

void Subband_Flow_Example(void)
{
    unsigned char frameReady;

    frameReady = 0u;
    Subband_Flow_Init(&SubbandFlow_State);

    Sys_Init();
    Key_Init();

    Adc_Init(ADC_50KHZ, ADC_SAMPLE_1024);
    Dac_Init(DAC_50KHZ, DAC_SAMPLE_1024, DAC_CHANNEL_12);

    FLAG_AD = 0;
    FLAG_DA = 0;
    Subband_Flow_FillDacBuffers(SubbandFlow_Silence);

    Adc_Start();
    Dac_Start();

    while (1)
    {
        if (FLAG_AD == 1)
        {
            FLAG_AD = 0;
            SUBBAND_DebugAdFrames++;

            Subband_Flow_CaptureMonoInput();
            Subband_Flow_ProcessFrame(&SubbandFlow_State,
                                      SubbandFlow_Input,
                                      SubbandFlow_Output);
            Subband_Flow_UpdatePeaks();

            frameReady = 1u;
            SUBBAND_DebugFrameReady = frameReady;
        }

        if (FLAG_DA == 1)
        {
            FLAG_DA = 0;
            SUBBAND_DebugDaFrames++;

            if (frameReady == 1u)
            {
                Subband_Flow_FillDacInactiveBuffer(SubbandFlow_Output);
                frameReady = 0u;
                SUBBAND_DebugFrameReady = frameReady;
            }
            else
            {
                Subband_Flow_FillDacInactiveBuffer(SubbandFlow_Silence);
            }
        }

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
