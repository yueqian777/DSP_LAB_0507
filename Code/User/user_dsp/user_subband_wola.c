/**
 * user_subband_wola.c
 *
 * Streaming WOLA-DFT subband analysis/synthesis implementation.
 */

#include "user_subband_wola.h"
#include "user_subband_denoise.h"
#include "user_subband_diagnostics.h"
#include "user_subband_codec_loopback.h"
#include "user_spectral_fft.h"
#include "math.h"
#include "string.h"

#if defined(__TI_COMPILER_VERSION__) || defined(__TMS320C6X__)
#include "c6x.h"
#define SUBBAND_WOLA_CPU_CYCLES_PER_MS 456000.0f
#endif

#define SUBBAND_WOLA_PI 3.14159265358979323846f
#define SUBBAND_HOPS_PER_FRAME (SUBBAND_FRAME_LEN / SUBBAND_HOP)

typedef struct
{
    float analysis_buf[SUBBAND_NFFT];
    float ola_buf[SUBBAND_NFFT];
    float window[SUBBAND_NFFT];
    float tw_re[SUBBAND_NFFT / 2];
    float tw_im[SUBBAND_NFFT / 2];
    float fft_re[SUBBAND_NFFT];
    float fft_im[SUBBAND_NFFT];
    float time_buf[SUBBAND_NFFT];
    float hop_in[SUBBAND_HOP];
    float hop_out[SUBBAND_HOP];
    float band_gain[SUBBAND_NUM_BANDS];
    int initialized;
    int bypass;
    unsigned long frame_count;
} SubbandWOLAState;

#if defined(__TI_COMPILER_VERSION__) || defined(__TMS320C6X__)
#pragma DATA_SECTION(SubbandWOLA_State, ".subband_l2")
#endif
static SubbandWOLAState SubbandWOLA_State;

volatile unsigned long SUBBAND_WOLA_DebugFrames = 0;
volatile float SUBBAND_WOLA_DebugInputEnergy = 0.0f;
volatile float SUBBAND_WOLA_DebugOutputEnergy = 0.0f;
volatile float SUBBAND_WOLA_DebugEnergyRatio = 0.0f;
volatile int SUBBAND_WOLA_DebugBypass = SUBBAND_BYPASS;
volatile unsigned long SUBBAND_WOLA_DebugLastCycles = 0;
volatile unsigned long SUBBAND_WOLA_DebugMaxCycles = 0;
volatile float SUBBAND_WOLA_DebugLastMs = 0.0f;
volatile float SUBBAND_WOLA_DebugMaxMs = 0.0f;

static void Update_Eval_Frame_Debug(void)
{
    SUBBAND_EVAL_DebugWolaFrames = SUBBAND_WOLA_DebugFrames;
    SUBBAND_EVAL_DebugDenoiseFrames = SUBBAND_WOLA_DebugFrames;
    SUBBAND_EVAL_DebugFrameBudgetMs =
        ((float)SUBBAND_FRAME_LEN / SUBBAND_SAMPLE_RATE) * 1000.0f;
    SUBBAND_EVAL_DebugDenoiseLastMs = SUBBAND_WOLA_DebugLastMs;
    if (SUBBAND_WOLA_DebugMaxMs > SUBBAND_EVAL_DebugDenoiseMaxMs)
    {
        SUBBAND_EVAL_DebugDenoiseMaxMs = SUBBAND_WOLA_DebugMaxMs;
    }
    if (SUBBAND_EVAL_DebugFrameBudgetMs > 1.0e-20f)
    {
        SUBBAND_EVAL_DebugCpuUsagePercent =
            (SUBBAND_EVAL_DebugDenoiseMaxMs /
             SUBBAND_EVAL_DebugFrameBudgetMs) * 100.0f;
    }
}

static short Saturate_To_Short(float x)
{
    if (x > 32767.0f)
    {
        return 32767;
    }
    if (x < -32768.0f)
    {
        return -32768;
    }
    return (short)x;
}

static void FFT_InPlace(float *re, float *im)
{
    (void)SpectralFFT_Forward(re, im,
                              SubbandWOLA_State.tw_re,
                              SubbandWOLA_State.tw_im,
                              SUBBAND_NFFT);
}

static void IFFT_InPlace(float *re, float *im)
{
    int i;
    float inv_n;

    for (i = 0; i < SUBBAND_NFFT; i++)
    {
        im[i] = -im[i];
    }

    FFT_InPlace(re, im);

    inv_n = 1.0f / (float)SUBBAND_NFFT;
    for (i = 0; i < SUBBAND_NFFT; i++)
    {
        re[i] *= inv_n;
        im[i] = -im[i] * inv_n;
    }
}

static int Positive_Bin_To_Band(int bin)
{
    int width;
    int band;

    width = (SUBBAND_NFFT / 2) / SUBBAND_NUM_BANDS;
    if (width < 1)
    {
        width = 1;
    }

    band = bin / width;
    if (band >= SUBBAND_NUM_BANDS)
    {
        band = SUBBAND_NUM_BANDS - 1;
    }

    return band;
}

static void Apply_Band_Gain(void)
{
    int bin;

    for (bin = 0; bin <= SUBBAND_NFFT / 2; bin++)
    {
        int band;
        float gain;

        band = Positive_Bin_To_Band(bin);
        gain = SubbandWOLA_State.band_gain[band];
        SubbandWOLA_State.fft_re[bin] *= gain;
        SubbandWOLA_State.fft_im[bin] *= gain;
    }

    SubbandWOLA_State.fft_im[0] = 0.0f;
    SubbandWOLA_State.fft_im[SUBBAND_NFFT / 2] = 0.0f;

    for (bin = 1; bin < SUBBAND_NFFT / 2; bin++)
    {
        int mirror;

        mirror = SUBBAND_NFFT - bin;
        SubbandWOLA_State.fft_re[mirror] = SubbandWOLA_State.fft_re[bin];
        SubbandWOLA_State.fft_im[mirror] = -SubbandWOLA_State.fft_im[bin];
    }
}

static void Shift_Analysis_Buffer_Float(float *input_hop)
{
    int i;

    for (i = 0; i < SUBBAND_NFFT - SUBBAND_HOP; i++)
    {
        SubbandWOLA_State.analysis_buf[i] =
            SubbandWOLA_State.analysis_buf[i + SUBBAND_HOP];
    }

    for (i = 0; i < SUBBAND_HOP; i++)
    {
        SubbandWOLA_State.analysis_buf[SUBBAND_NFFT - SUBBAND_HOP + i] =
            input_hop[i];
    }
}

static void Shift_OLA_Buffer(void)
{
    int i;

    for (i = 0; i < SUBBAND_NFFT - SUBBAND_HOP; i++)
    {
        SubbandWOLA_State.ola_buf[i] =
            SubbandWOLA_State.ola_buf[i + SUBBAND_HOP];
    }

    for (i = SUBBAND_NFFT - SUBBAND_HOP; i < SUBBAND_NFFT; i++)
    {
        SubbandWOLA_State.ola_buf[i] = 0.0f;
    }
}

static void Process_Hop_Float(float *input_hop, float *output_hop)
{
    int i;

    Shift_Analysis_Buffer_Float(input_hop);

    for (i = 0; i < SUBBAND_NFFT; i++)
    {
        SubbandWOLA_State.fft_re[i] =
            SubbandWOLA_State.analysis_buf[i] * SubbandWOLA_State.window[i];
        SubbandWOLA_State.fft_im[i] = 0.0f;
    }

    FFT_InPlace(SubbandWOLA_State.fft_re, SubbandWOLA_State.fft_im);
    SubbandDenoise_ProcessSpectrum(SubbandWOLA_State.fft_re,
                                   SubbandWOLA_State.fft_im);
    SubbandCodecLoopback_ProcessSpectrum(SubbandWOLA_State.fft_re,
                                         SubbandWOLA_State.fft_im);
    Apply_Band_Gain();
    IFFT_InPlace(SubbandWOLA_State.fft_re, SubbandWOLA_State.fft_im);

    for (i = 0; i < SUBBAND_NFFT; i++)
    {
        SubbandWOLA_State.time_buf[i] =
            SubbandWOLA_State.fft_re[i] * SubbandWOLA_State.window[i];
        SubbandWOLA_State.ola_buf[i] += SubbandWOLA_State.time_buf[i];
    }

    for (i = 0; i < SUBBAND_HOP; i++)
    {
        output_hop[i] = SubbandWOLA_State.ola_buf[i];
    }

    Shift_OLA_Buffer();
}

static void Process_Hop_Short(short *input_hop, short *output_hop)
{
    int i;

    for (i = 0; i < SUBBAND_HOP; i++)
    {
        SubbandWOLA_State.hop_in[i] = (float)input_hop[i];
    }

    Process_Hop_Float(SubbandWOLA_State.hop_in, SubbandWOLA_State.hop_out);

    for (i = 0; i < SUBBAND_HOP; i++)
    {
        output_hop[i] = Saturate_To_Short(SubbandWOLA_State.hop_out[i]);
    }
}

static void Update_Debug_Energy(short *in, short *out)
{
    int i;
    float input_energy;
    float output_energy;

    input_energy = 0.0f;
    output_energy = 0.0f;

    for (i = 0; i < SUBBAND_FRAME_LEN; i++)
    {
        float x;
        float y;

        x = (float)in[i];
        y = (float)out[i];
        input_energy += x * x;
        output_energy += y * y;
    }

    SUBBAND_WOLA_DebugFrames++;
    SUBBAND_WOLA_DebugInputEnergy = input_energy;
    SUBBAND_WOLA_DebugOutputEnergy = output_energy;
    if (input_energy > 1.0e-20f)
    {
        SUBBAND_WOLA_DebugEnergyRatio = output_energy / input_energy;
    }
    else
    {
        SUBBAND_WOLA_DebugEnergyRatio = 0.0f;
    }
    SUBBAND_WOLA_DebugBypass = SubbandWOLA_State.bypass;
}

void SubbandWOLA_Init(void)
{
    int i;
    float phase;

    if (SubbandWOLA_State.initialized != 0)
    {
        return;
    }

    memset(&SubbandWOLA_State, 0, sizeof(SubbandWOLA_State));

    for (i = 0; i < SUBBAND_NUM_BANDS; i++)
    {
        SubbandWOLA_State.band_gain[i] = 1.0f;
    }

    for (i = 0; i < SUBBAND_NFFT; i++)
    {
        float hann;

        phase = (2.0f * SUBBAND_WOLA_PI * (float)i) / (float)SUBBAND_NFFT;
        hann = 0.5f - 0.5f * cosf(phase);
        if (hann < 0.0f)
        {
            hann = 0.0f;
        }
        SubbandWOLA_State.window[i] = sqrtf(hann);
    }

    (void)SpectralFFT_GenerateTwiddles(SubbandWOLA_State.tw_re,
                                       SubbandWOLA_State.tw_im,
                                       SUBBAND_NFFT);

    SubbandWOLA_State.bypass = SUBBAND_BYPASS;
    SubbandWOLA_State.initialized = 1;
    SUBBAND_WOLA_DebugBypass = SubbandWOLA_State.bypass;
    SubbandDenoise_Init();
    SubbandCodecLoopback_Init();
#if defined(__TI_COMPILER_VERSION__) || defined(__TMS320C6X__)
    TSCL = 0;
    TSCH = 0;
#endif
}

void SubbandWOLA_ResetStream(void)
{
    SubbandWOLA_Init();
    memset(SubbandWOLA_State.analysis_buf, 0,
           sizeof(SubbandWOLA_State.analysis_buf));
    memset(SubbandWOLA_State.ola_buf, 0, sizeof(SubbandWOLA_State.ola_buf));
    memset(SubbandWOLA_State.fft_re, 0, sizeof(SubbandWOLA_State.fft_re));
    memset(SubbandWOLA_State.fft_im, 0, sizeof(SubbandWOLA_State.fft_im));
    memset(SubbandWOLA_State.time_buf, 0, sizeof(SubbandWOLA_State.time_buf));
    memset(SubbandWOLA_State.hop_in, 0, sizeof(SubbandWOLA_State.hop_in));
    memset(SubbandWOLA_State.hop_out, 0, sizeof(SubbandWOLA_State.hop_out));
    SubbandWOLA_State.frame_count = 0UL;
    SUBBAND_WOLA_DebugFrames = 0UL;
    SUBBAND_WOLA_DebugInputEnergy = 0.0f;
    SUBBAND_WOLA_DebugOutputEnergy = 0.0f;
    SUBBAND_WOLA_DebugEnergyRatio = 0.0f;
    SUBBAND_WOLA_DebugLastCycles = 0UL;
    SUBBAND_WOLA_DebugMaxCycles = 0UL;
    SUBBAND_WOLA_DebugLastMs = 0.0f;
    SUBBAND_WOLA_DebugMaxMs = 0.0f;
    SUBBAND_EVAL_DebugWolaFrames = 0UL;
    SUBBAND_EVAL_DebugDenoiseFrames = 0UL;
    SUBBAND_EVAL_DebugDenoiseLastMs = 0.0f;
    SUBBAND_EVAL_DebugDenoiseMaxMs = 0.0f;
    SUBBAND_EVAL_DebugCpuUsagePercent = 0.0f;
    SubbandCodecLoopback_Reset();
}

void SubbandWOLA_ResetAllGains(void)
{
    int band;

    SubbandWOLA_Init();
    for (band = 0; band < SUBBAND_NUM_BANDS; band++)
    {
        SubbandWOLA_State.band_gain[band] = 1.0f;
    }
}

void SubbandWOLA_SetBandGain(int band, float gain)
{
    SubbandWOLA_Init();

    if ((band < 0) || (band >= SUBBAND_NUM_BANDS))
    {
        return;
    }

    SubbandWOLA_State.band_gain[band] = gain;
}

void SubbandWOLA_SetBypass(int enable)
{
    SubbandWOLA_Init();

    if (enable != 0)
    {
        SubbandWOLA_State.bypass = 1;
    }
    else
    {
        SubbandWOLA_State.bypass = 0;
    }
    SUBBAND_WOLA_DebugBypass = SubbandWOLA_State.bypass;
}

float SubbandWOLA_GetCOLAMaxError(void)
{
    int i;
    float max_error;

    SubbandWOLA_Init();

    max_error = 0.0f;
    for (i = 0; i < SUBBAND_HOP; i++)
    {
        float a;
        float b;
        float sum;
        float err;

        a = SubbandWOLA_State.window[i];
        b = SubbandWOLA_State.window[i + SUBBAND_HOP];
        sum = a * a + b * b;
        err = sum - 1.0f;
        if (err < 0.0f)
        {
            err = -err;
        }
        if (err > max_error)
        {
            max_error = err;
        }
    }

    return max_error;
}

void SubbandWOLA_ProcessFrame(short *in, short *out)
{
    int i;
    int hop;
#if defined(__TI_COMPILER_VERSION__) || defined(__TMS320C6X__)
    unsigned int cycle_start;
    unsigned int cycle_end;
    unsigned long cycle_delta;
#endif

    if ((in == 0) || (out == 0))
    {
        return;
    }

    SubbandWOLA_Init();

#if defined(__TI_COMPILER_VERSION__) || defined(__TMS320C6X__)
    cycle_start = TSCL;
#endif

    if (SubbandWOLA_State.bypass != 0)
    {
        for (i = 0; i < SUBBAND_FRAME_LEN; i++)
        {
            out[i] = in[i];
        }
        Update_Debug_Energy(in, out);
        SubbandWOLA_State.frame_count++;
#if defined(__TI_COMPILER_VERSION__) || defined(__TMS320C6X__)
        cycle_end = TSCL;
        cycle_delta = (unsigned long)(cycle_end - cycle_start);
        SUBBAND_WOLA_DebugLastCycles = cycle_delta;
        SUBBAND_WOLA_DebugLastMs =
            (float)cycle_delta / SUBBAND_WOLA_CPU_CYCLES_PER_MS;
        if (cycle_delta > SUBBAND_WOLA_DebugMaxCycles)
        {
            SUBBAND_WOLA_DebugMaxCycles = cycle_delta;
            SUBBAND_WOLA_DebugMaxMs = SUBBAND_WOLA_DebugLastMs;
        }
#endif
        Update_Eval_Frame_Debug();
        return;
    }

    (void)i;
    for (hop = 0; hop < SUBBAND_HOPS_PER_FRAME; hop++)
    {
        Process_Hop_Short(&in[hop * SUBBAND_HOP], &out[hop * SUBBAND_HOP]);
    }

    Update_Debug_Energy(in, out);
    SubbandWOLA_State.frame_count++;

#if defined(__TI_COMPILER_VERSION__) || defined(__TMS320C6X__)
    cycle_end = TSCL;
    cycle_delta = (unsigned long)(cycle_end - cycle_start);
    SUBBAND_WOLA_DebugLastCycles = cycle_delta;
    SUBBAND_WOLA_DebugLastMs =
        (float)cycle_delta / SUBBAND_WOLA_CPU_CYCLES_PER_MS;
    if (cycle_delta > SUBBAND_WOLA_DebugMaxCycles)
    {
        SUBBAND_WOLA_DebugMaxCycles = cycle_delta;
        SUBBAND_WOLA_DebugMaxMs = SUBBAND_WOLA_DebugLastMs;
    }
#endif
    Update_Eval_Frame_Debug();
}

#ifdef SUBBAND_ALGO_ONLY
void SubbandWOLA_ProcessFrameFloat(float *in, float *out)
{
    int i;
    int hop;

    if ((in == 0) || (out == 0))
    {
        return;
    }

    SubbandWOLA_Init();

    if (SubbandWOLA_State.bypass != 0)
    {
        for (i = 0; i < SUBBAND_FRAME_LEN; i++)
        {
            out[i] = in[i];
        }
        SubbandWOLA_State.frame_count++;
        return;
    }

    for (hop = 0; hop < SUBBAND_HOPS_PER_FRAME; hop++)
    {
        Process_Hop_Float(&in[hop * SUBBAND_HOP], &out[hop * SUBBAND_HOP]);
    }

    SubbandWOLA_State.frame_count++;
}
#endif
