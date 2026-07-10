/**
 * user_subband_polyphase.c
 *
 * C6748 test wrapper derived from project.zip/project1.c. The analysis and
 * synthesis arithmetic is kept as the supplied 8-channel polyphase path;
 * only the original standalone main and foreign driver headers are removed.
 */

#include "user_subband_polyphase.h"
#include <math.h>
#include <string.h>

#define SUBBAND_POLYPHASE_PI 3.14159265358979323846f

#if defined(__TI_COMPILER_VERSION__) || defined(__TMS320C6X__)
#pragma DATA_SECTION(SubbandPolyphase_Prototype, ".subband_l2")
#pragma DATA_ALIGN(SubbandPolyphase_Prototype, 8)
#pragma DATA_SECTION(SubbandPolyphase_AnaLine, ".subband_l2")
#pragma DATA_ALIGN(SubbandPolyphase_AnaLine, 8)
#pragma DATA_SECTION(SubbandPolyphase_SynLine, ".subband_l2")
#pragma DATA_ALIGN(SubbandPolyphase_SynLine, 8)
#pragma DATA_SECTION(SubbandPolyphase_H, ".subband_l2")
#pragma DATA_ALIGN(SubbandPolyphase_H, 8)
#pragma DATA_SECTION(SubbandPolyphase_Cos, ".subband_l2")
#pragma DATA_ALIGN(SubbandPolyphase_Cos, 8)
#pragma DATA_SECTION(SubbandPolyphase_Sin, ".subband_l2")
#pragma DATA_ALIGN(SubbandPolyphase_Sin, 8)
#endif

static const float SubbandPolyphase_Prototype[SUBBAND_POLYPHASE_PROTOTYPE_TAPS] =
{
    -6.315349e-04f, -5.510414e-04f, -3.259621e-04f,  2.340440e-06f,
     3.642858e-04f,  6.766050e-04f,  8.611797e-04f,  8.640147e-04f,
     6.698439e-04f,  3.085988e-04f, -1.483775e-04f, -6.022076e-04f,
    -9.478917e-04f, -1.098731e-03f, -1.008196e-03f, -6.838552e-04f,
    -1.894605e-04f,  3.662389e-04f,  8.523396e-04f,  1.145797e-03f,
     1.162045e-03f,  8.789646e-04f,  3.478767e-04f, -3.122778e-04f,
    -9.377814e-04f, -1.356247e-03f, -1.428731e-03f, -1.088462e-03f,
    -3.666099e-04f,  6.024938e-04f,  1.600312e-03f,  2.365728e-03f,
     2.652446e-03f,  2.290888e-03f,  1.240943e-03f, -3.776170e-04f,
    -2.286936e-03f, -4.089015e-03f, -5.336370e-03f, -5.624458e-03f,
    -4.688907e-03f, -2.487475e-03f,  7.526787e-04f,  4.537047e-03f,
     8.161600e-03f,  1.082239e-02f,  1.176229e-02f,  1.043122e-02f,
     6.631116e-03f,  6.163657e-04f, -6.874063e-03f, -1.466670e-02f,
    -2.128611e-02f, -2.515561e-02f, -2.483840e-02f, -1.928255e-02f,
    -8.029233e-03f,  8.652438e-03f,  2.973120e-02f,  5.349194e-02f,
     7.772069e-02f,  9.997327e-02f,  1.178896e-01f,  1.295080e-01f,
     1.335319e-01f,  1.295080e-01f,  1.178896e-01f,  9.997327e-02f,
     7.772069e-02f,  5.349194e-02f,  2.973120e-02f,  8.652438e-03f,
    -8.029233e-03f, -1.928255e-02f, -2.483840e-02f, -2.515561e-02f,
    -2.128611e-02f, -1.466670e-02f, -6.874063e-03f,  6.163657e-04f,
     6.631116e-03f,  1.043122e-02f,  1.176229e-02f,  1.082239e-02f,
     8.161600e-03f,  4.537047e-03f,  7.526787e-04f, -2.487475e-03f,
    -4.688907e-03f, -5.624458e-03f, -5.336370e-03f, -4.089015e-03f,
    -2.286936e-03f, -3.776170e-04f,  1.240943e-03f,  2.290888e-03f,
     2.652446e-03f,  2.365728e-03f,  1.600312e-03f,  6.024938e-04f,
    -3.666099e-04f, -1.088462e-03f, -1.428731e-03f, -1.356247e-03f,
    -9.377814e-04f, -3.122778e-04f,  3.478767e-04f,  8.789646e-04f,
     1.162045e-03f,  1.145797e-03f,  8.523396e-04f,  3.662389e-04f,
    -1.894605e-04f, -6.838552e-04f, -1.008196e-03f, -1.098731e-03f,
    -9.478917e-04f, -6.022076e-04f, -1.483775e-04f,  3.085988e-04f,
     6.698439e-04f,  8.640147e-04f,  8.611797e-04f,  6.766050e-04f,
     3.642858e-04f,  2.340440e-06f, -3.259621e-04f, -5.510414e-04f,
    -6.315349e-04f
};

static float SubbandPolyphase_AnaLine[SUBBAND_POLYPHASE_PADDED_TAPS];
static float SubbandPolyphase_SynLine[SUBBAND_POLYPHASE_CHANNELS]
                                      [SUBBAND_POLYPHASE_PHASE_TAPS];
static float SubbandPolyphase_H[SUBBAND_POLYPHASE_CHANNELS]
                               [SUBBAND_POLYPHASE_PHASE_TAPS];
static float SubbandPolyphase_Cos[SUBBAND_POLYPHASE_CHANNELS]
                                 [SUBBAND_POLYPHASE_CHANNELS];
static float SubbandPolyphase_Sin[SUBBAND_POLYPHASE_CHANNELS]
                                 [SUBBAND_POLYPHASE_CHANNELS];
static int SubbandPolyphase_Initialized = 0;

static short SubbandPolyphase_Saturate(float value)
{
    if (value > 32767.0f)
    {
        return 32767;
    }
    if (value < -32768.0f)
    {
        return -32768;
    }
    return (short)value;
}

void SubbandPolyphase_Init(void)
{
    int k;
    int l;
    int m;

    if (SubbandPolyphase_Initialized != 0)
    {
        return;
    }

    for (l = 0; l < SUBBAND_POLYPHASE_CHANNELS; l++)
    {
#if defined(__TI_COMPILER_VERSION__) || defined(__TMS320C6X__)
#pragma MUST_ITERATE(17, 17, 1)
#endif
        for (m = 0; m < SUBBAND_POLYPHASE_PHASE_TAPS; m++)
        {
            int original_idx;

            original_idx = m * SUBBAND_POLYPHASE_CHANNELS + l;
            if (original_idx < SUBBAND_POLYPHASE_PROTOTYPE_TAPS)
            {
                SubbandPolyphase_H[l][m] =
                    SubbandPolyphase_Prototype[original_idx];
            }
            else
            {
                SubbandPolyphase_H[l][m] = 0.0f;
            }
        }
    }

    for (k = 0; k < SUBBAND_POLYPHASE_CHANNELS; k++)
    {
#if defined(__TI_COMPILER_VERSION__) || defined(__TMS320C6X__)
#pragma MUST_ITERATE(8, 8, 8)
#endif
        for (l = 0; l < SUBBAND_POLYPHASE_CHANNELS; l++)
        {
            float phase;

            phase = (2.0f * SUBBAND_POLYPHASE_PI * (float)l * (float)k) /
                    (float)SUBBAND_POLYPHASE_CHANNELS;
            SubbandPolyphase_Cos[k][l] = cosf(phase);
            SubbandPolyphase_Sin[k][l] = sinf(phase);
        }
    }

    SubbandPolyphase_Reset();
    SubbandPolyphase_Initialized = 1;
}

void SubbandPolyphase_Reset(void)
{
    memset(SubbandPolyphase_AnaLine, 0, sizeof(SubbandPolyphase_AnaLine));
    memset(SubbandPolyphase_SynLine, 0, sizeof(SubbandPolyphase_SynLine));
}

static void SubbandPolyphase_ProcessBlock(
    const short * SUBBAND_POLYPHASE_RESTRICT input,
    short * SUBBAND_POLYPHASE_RESTRICT output)
{
    float v[SUBBAND_POLYPHASE_CHANNELS];
    float subband[SUBBAND_POLYPHASE_CHANNELS][2];
    float phase_in[SUBBAND_POLYPHASE_CHANNELS];
    float phase_out[SUBBAND_POLYPHASE_CHANNELS];
    int k;
    int l;
    int m;

#if defined(__TI_COMPILER_VERSION__) || defined(__TMS320C6X__)
#pragma MUST_ITERATE(128, 128, 8)
#endif
    for (m = SUBBAND_POLYPHASE_PADDED_TAPS - 1;
         m >= SUBBAND_POLYPHASE_CHANNELS;
         m--)
    {
        SubbandPolyphase_AnaLine[m] =
            SubbandPolyphase_AnaLine[m - SUBBAND_POLYPHASE_CHANNELS];
    }

#if defined(__TI_COMPILER_VERSION__) || defined(__TMS320C6X__)
#pragma MUST_ITERATE(8, 8, 8)
#endif
    for (l = 0; l < SUBBAND_POLYPHASE_CHANNELS; l++)
    {
        SubbandPolyphase_AnaLine[SUBBAND_POLYPHASE_CHANNELS - 1 - l] =
            (float)input[l];
    }

#if defined(__TI_COMPILER_VERSION__) || defined(__TMS320C6X__)
#pragma MUST_ITERATE(8, 8, 8)
#endif
    for (l = 0; l < SUBBAND_POLYPHASE_CHANNELS; l++)
    {
        float sum;

        sum = 0.0f;
#if defined(__TI_COMPILER_VERSION__) || defined(__TMS320C6X__)
#pragma MUST_ITERATE(17, 17, 1)
#endif
        for (m = 0; m < SUBBAND_POLYPHASE_PHASE_TAPS; m++)
        {
            sum += SubbandPolyphase_H[l][m] *
                   SubbandPolyphase_AnaLine[m * SUBBAND_POLYPHASE_CHANNELS + l];
        }
        v[l] = sum;
    }

#if defined(__TI_COMPILER_VERSION__) || defined(__TMS320C6X__)
#pragma MUST_ITERATE(8, 8, 8)
#endif
    for (k = 0; k < SUBBAND_POLYPHASE_CHANNELS; k++)
    {
        float re;
        float im;

        re = 0.0f;
        im = 0.0f;
#if defined(__TI_COMPILER_VERSION__) || defined(__TMS320C6X__)
#pragma MUST_ITERATE(8, 8, 8)
#endif
        for (l = 0; l < SUBBAND_POLYPHASE_CHANNELS; l++)
        {
            re += v[l] * SubbandPolyphase_Cos[k][l];
            im += v[l] * SubbandPolyphase_Sin[k][l];
        }
        subband[k][0] = re;
        subband[k][1] = im;
    }

#if defined(__TI_COMPILER_VERSION__) || defined(__TMS320C6X__)
#pragma MUST_ITERATE(8, 8, 8)
#endif
    for (l = 0; l < SUBBAND_POLYPHASE_CHANNELS; l++)
    {
        float re;

        re = 0.0f;
#if defined(__TI_COMPILER_VERSION__) || defined(__TMS320C6X__)
#pragma MUST_ITERATE(8, 8, 8)
#endif
        for (k = 0; k < SUBBAND_POLYPHASE_CHANNELS; k++)
        {
            re += subband[k][0] * SubbandPolyphase_Cos[k][l] +
                  subband[k][1] * SubbandPolyphase_Sin[k][l];
        }
        phase_in[l] = re / (float)SUBBAND_POLYPHASE_CHANNELS;
    }

#if defined(__TI_COMPILER_VERSION__) || defined(__TMS320C6X__)
#pragma MUST_ITERATE(8, 8, 8)
#endif
    for (l = 0; l < SUBBAND_POLYPHASE_CHANNELS; l++)
    {
        phase_out[l] = 0.0f;
#if defined(__TI_COMPILER_VERSION__) || defined(__TMS320C6X__)
#pragma MUST_ITERATE(16, 16, 1)
#endif
        for (m = SUBBAND_POLYPHASE_PHASE_TAPS - 1; m >= 1; m--)
        {
            SubbandPolyphase_SynLine[l][m] = SubbandPolyphase_SynLine[l][m - 1];
        }
        SubbandPolyphase_SynLine[l][0] = phase_in[l];

#if defined(__TI_COMPILER_VERSION__) || defined(__TMS320C6X__)
#pragma MUST_ITERATE(17, 17, 1)
#endif
        for (m = 0; m < SUBBAND_POLYPHASE_PHASE_TAPS; m++)
        {
            phase_out[l] += SubbandPolyphase_H[l][m] *
                            SubbandPolyphase_SynLine[l][m];
        }
    }

#if defined(__TI_COMPILER_VERSION__) || defined(__TMS320C6X__)
#pragma MUST_ITERATE(8, 8, 8)
#endif
    for (l = 0; l < SUBBAND_POLYPHASE_CHANNELS; l++)
    {
        float value;

        value = phase_out[SUBBAND_POLYPHASE_CHANNELS - 1 - l] *
                (float)SUBBAND_POLYPHASE_CHANNELS;
        if (value > 0.0f)
        {
            value += 0.5f;
        }
        else
        {
            value -= 0.5f;
        }
        output[l] = SubbandPolyphase_Saturate(value);
    }
}

void SubbandPolyphase_ProcessFrame(
    const short * SUBBAND_POLYPHASE_RESTRICT input,
    short * SUBBAND_POLYPHASE_RESTRICT output,
    int sample_count)
{
    int offset;

    if ((input == 0) || (output == 0) || (sample_count <= 0))
    {
        return;
    }

    SubbandPolyphase_Init();

    for (offset = 0;
         offset + SUBBAND_POLYPHASE_BLOCK <= sample_count;
         offset += SUBBAND_POLYPHASE_BLOCK)
    {
        SubbandPolyphase_ProcessBlock(&input[offset], &output[offset]);
    }
}
