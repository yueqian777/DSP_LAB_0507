/**
 * user_subband_codec_loopback.c
 *
 * Frequency-domain scalar quantization loopback inserted inside WOLA.
 */

#include "user_subband_codec_loopback.h"
#include "math.h"
#include "string.h"

#define SUBBAND_CODEC_LOOP_EPS 1.0e-12f

typedef struct
{
    float band_energy[SUBBAND_NUM_BANDS];
    float band_max_abs[SUBBAND_NUM_BANDS];
    float band_rms[SUBBAND_NUM_BANDS];
    float band_scale[SUBBAND_NUM_BANDS];
    int band_bits[SUBBAND_NUM_BANDS];
    int scalar_count[SUBBAND_NUM_BANDS];
    int enabled;
    int target_kbps;
    int initialized;
} SubbandCodecLoopbackState;

#if defined(__TI_COMPILER_VERSION__) || defined(__TMS320C6X__)
#pragma DATA_SECTION(SubbandCodecLoopback_State, ".subband_l2")
#endif
static SubbandCodecLoopbackState SubbandCodecLoopback_State;

volatile int SUBBAND_CODEC_LOOP_DebugEnabled = 0;
volatile int SUBBAND_CODEC_LOOP_DebugTargetKbps =
    SUBBAND_CODEC_LOOP_DEFAULT_KBPS;
volatile int SUBBAND_CODEC_LOOP_DebugRequestedTargetKbps = 0;
volatile float SUBBAND_CODEC_LOOP_DebugEstimatedBitrateKbps = 0.0f;
volatile float SUBBAND_CODEC_LOOP_DebugCompressionRatio = 0.0f;
volatile float SUBBAND_CODEC_LOOP_DebugAvgBitsPerScalar = 0.0f;
volatile float SUBBAND_CODEC_LOOP_DebugBandBits0 = 0.0f;
volatile float SUBBAND_CODEC_LOOP_DebugBandBits1 = 0.0f;
volatile float SUBBAND_CODEC_LOOP_DebugBandBits2 = 0.0f;
volatile float SUBBAND_CODEC_LOOP_DebugBandBits3 = 0.0f;
volatile float SUBBAND_CODEC_LOOP_DebugBandBits4 = 0.0f;
volatile float SUBBAND_CODEC_LOOP_DebugBandBits5 = 0.0f;
volatile float SUBBAND_CODEC_LOOP_DebugBandBits6 = 0.0f;
volatile float SUBBAND_CODEC_LOOP_DebugBandBits7 = 0.0f;
volatile int SUBBAND_CODEC_LOOP_DebugInvalidCount = 0;
volatile int SUBBAND_CODEC_LOOP_DebugClippingCount = 0;
volatile unsigned long SUBBAND_CODEC_LOOP_DebugFrames = 0UL;

static const float SubbandCodecLoopback_PerceptualWeight[SUBBAND_NUM_BANDS] =
{
    1.30f, 1.80f, 2.10f, 1.60f,
    0.90f, 0.60f, 0.35f, 0.15f
};

static int Loop_Is_Invalid(float x)
{
    if (x != x)
    {
        return 1;
    }
    if ((x > 1.0e30f) || (x < -1.0e30f))
    {
        return 1;
    }
    return 0;
}

static float Loop_Abs(float x)
{
    return (x < 0.0f) ? -x : x;
}

static int Loop_Positive_Bin_To_Band(int bin)
{
    int band;

    band = (bin * SUBBAND_NUM_BANDS) / SUBBAND_POS_BINS;
    if (band < 0)
    {
        band = 0;
    }
    if (band >= SUBBAND_NUM_BANDS)
    {
        band = SUBBAND_NUM_BANDS - 1;
    }
    return band;
}

static int Loop_Bin_Scalar_Count(int bin)
{
    if ((bin == 0) || (bin == (SUBBAND_NFFT / 2)))
    {
        return 1;
    }
    return 2;
}

static int Loop_Effective_Bits(int bits)
{
    if (bits < 2)
    {
        return 0;
    }
    return bits;
}

static int Loop_Next_Bits(int bits)
{
    if (bits < 2)
    {
        return 2;
    }
    return bits + 1;
}

static int Loop_Max_Bits_For_Band(int band)
{
    if (band == 7)
    {
        return 2;
    }
    if (band == 6)
    {
        return 3;
    }
    return SUBBAND_CODEC_LOOP_MAX_BITS;
}

static int Loop_Normalize_Target_Kbps(int kbps)
{
    if (kbps <= 200)
    {
        return 160;
    }
    if (kbps <= 280)
    {
        return 240;
    }
    return 320;
}

static void Loop_Clear_Band_Work(void)
{
    int band;

    for (band = 0; band < SUBBAND_NUM_BANDS; band++)
    {
        SubbandCodecLoopback_State.band_energy[band] = 0.0f;
        SubbandCodecLoopback_State.band_max_abs[band] = 0.0f;
        SubbandCodecLoopback_State.band_rms[band] = 0.0f;
        SubbandCodecLoopback_State.band_scale[band] = 0.0f;
        SubbandCodecLoopback_State.band_bits[band] =
            SUBBAND_CODEC_LOOP_MIN_BITS;
        SubbandCodecLoopback_State.scalar_count[band] = 0;
    }
}

static unsigned long Loop_Current_Payload_Bits(void)
{
    int band;
    unsigned long bits;

    bits = (unsigned long)(SUBBAND_NUM_BANDS *
                           SUBBAND_CODEC_LOOP_BAND_SIDE_BITS);
    for (band = 0; band < SUBBAND_NUM_BANDS; band++)
    {
        bits += (unsigned long)SubbandCodecLoopback_State.scalar_count[band] *
                (unsigned long)Loop_Effective_Bits(
                    SubbandCodecLoopback_State.band_bits[band]);
    }
    return bits;
}

static unsigned long Loop_Current_Scalar_Bits(void)
{
    int band;
    unsigned long bits;

    bits = 0UL;
    for (band = 0; band < SUBBAND_NUM_BANDS; band++)
    {
        bits += (unsigned long)SubbandCodecLoopback_State.scalar_count[band] *
                (unsigned long)Loop_Effective_Bits(
                    SubbandCodecLoopback_State.band_bits[band]);
    }
    return bits;
}

static unsigned long Loop_Current_Scalar_Count(void)
{
    int band;
    unsigned long count;

    count = 0UL;
    for (band = 0; band < SUBBAND_NUM_BANDS; band++)
    {
        count += (unsigned long)SubbandCodecLoopback_State.scalar_count[band];
    }
    return count;
}

static void Loop_Analyze_Bands(float *re, float *im)
{
    int bin;

    Loop_Clear_Band_Work();
    for (bin = 0; bin <= SUBBAND_NFFT / 2; bin++)
    {
        int band;
        float real_part;
        float imag_part;
        float ar;
        float ai;
        float energy;

        real_part = re[bin];
        imag_part = im[bin];
        if (Loop_Is_Invalid(real_part) != 0)
        {
            real_part = 0.0f;
            re[bin] = 0.0f;
            SUBBAND_CODEC_LOOP_DebugInvalidCount++;
        }
        if (Loop_Is_Invalid(imag_part) != 0)
        {
            imag_part = 0.0f;
            im[bin] = 0.0f;
            SUBBAND_CODEC_LOOP_DebugInvalidCount++;
        }
        if ((bin == 0) || (bin == (SUBBAND_NFFT / 2)))
        {
            imag_part = 0.0f;
            im[bin] = 0.0f;
        }

        band = Loop_Positive_Bin_To_Band(bin);
        ar = Loop_Abs(real_part);
        ai = Loop_Abs(imag_part);
        energy = real_part * real_part + imag_part * imag_part;
        if (Loop_Is_Invalid(energy) != 0)
        {
            energy = 0.0f;
            SUBBAND_CODEC_LOOP_DebugInvalidCount++;
        }
        SubbandCodecLoopback_State.band_energy[band] += energy;
        if (ar > SubbandCodecLoopback_State.band_max_abs[band])
        {
            SubbandCodecLoopback_State.band_max_abs[band] = ar;
        }
        if (ai > SubbandCodecLoopback_State.band_max_abs[band])
        {
            SubbandCodecLoopback_State.band_max_abs[band] = ai;
        }
        SubbandCodecLoopback_State.scalar_count[band] +=
            Loop_Bin_Scalar_Count(bin);
    }
}

static void Loop_Compute_Scales(void)
{
    int band;

    for (band = 0; band < SUBBAND_NUM_BANDS; band++)
    {
        float rms;
        float robust;
        float max_abs;
        float scale;

        max_abs = SubbandCodecLoopback_State.band_max_abs[band];
        if (SubbandCodecLoopback_State.scalar_count[band] > 0)
        {
            rms = sqrtf(SubbandCodecLoopback_State.band_energy[band] /
                        (float)SubbandCodecLoopback_State.scalar_count[band]);
        }
        else
        {
            rms = 0.0f;
        }
        robust = 3.0f * rms;
        if (robust > max_abs)
        {
            robust = max_abs;
        }
        scale = 0.7f * max_abs + 0.3f * robust;
        if (Loop_Is_Invalid(scale) != 0)
        {
            scale = 0.0f;
            SUBBAND_CODEC_LOOP_DebugInvalidCount++;
        }
        SubbandCodecLoopback_State.band_rms[band] = rms;
        SubbandCodecLoopback_State.band_scale[band] = scale;
    }
}

static void Loop_Allocate_Bits(int target_kbps)
{
    unsigned long budget_bits;
    unsigned long payload_bits;
    float budget;

    budget = ((float)target_kbps * 1000.0f * (float)SUBBAND_HOP) /
             (float)SUBBAND_SAMPLE_RATE;
    budget_bits = (unsigned long)(budget + 0.5f);
    payload_bits = Loop_Current_Payload_Bits();

    while (payload_bits < budget_bits)
    {
        int band;
        int best_band;
        int best_next_bits;
        unsigned long best_extra_bits;
        float best_score;
        unsigned long current_error;

        best_band = -1;
        best_next_bits = 0;
        best_extra_bits = 0UL;
        best_score = -1.0f;

        for (band = 0; band < SUBBAND_NUM_BANDS; band++)
        {
            int next_bits;
            int cur_payload_bits;
            int next_payload_bits;
            unsigned long extra_bits;
            unsigned long new_payload_bits;
            float score;

            next_bits = Loop_Next_Bits(
                SubbandCodecLoopback_State.band_bits[band]);
            if (next_bits > Loop_Max_Bits_For_Band(band))
            {
                continue;
            }
            cur_payload_bits = Loop_Effective_Bits(
                SubbandCodecLoopback_State.band_bits[band]);
            next_payload_bits = Loop_Effective_Bits(next_bits);
            extra_bits =
                (unsigned long)SubbandCodecLoopback_State.scalar_count[band] *
                (unsigned long)(next_payload_bits - cur_payload_bits);
            if (extra_bits == 0UL)
            {
                continue;
            }
            new_payload_bits = payload_bits + extra_bits;
            if (new_payload_bits > budget_bits)
            {
                continue;
            }
            score = logf(SubbandCodecLoopback_State.band_energy[band] + 1.0f) *
                    SubbandCodecLoopback_PerceptualWeight[band] /
                    (float)(next_bits + 1);
            if (score > best_score)
            {
                best_score = score;
                best_band = band;
                best_next_bits = next_bits;
                best_extra_bits = extra_bits;
            }
        }

        if (best_band < 0)
        {
            unsigned long best_error;

            current_error = budget_bits - payload_bits;
            best_error = current_error;
            for (band = 0; band < SUBBAND_NUM_BANDS; band++)
            {
                int next_bits;
                int cur_payload_bits;
                int next_payload_bits;
                unsigned long extra_bits;
                unsigned long new_payload_bits;
                unsigned long new_error;

                next_bits = Loop_Next_Bits(
                    SubbandCodecLoopback_State.band_bits[band]);
                if (next_bits > Loop_Max_Bits_For_Band(band))
                {
                    continue;
                }
                cur_payload_bits = Loop_Effective_Bits(
                    SubbandCodecLoopback_State.band_bits[band]);
                next_payload_bits = Loop_Effective_Bits(next_bits);
                extra_bits =
                    (unsigned long)SubbandCodecLoopback_State.scalar_count[band] *
                    (unsigned long)(next_payload_bits - cur_payload_bits);
                if (extra_bits == 0UL)
                {
                    continue;
                }
                new_payload_bits = payload_bits + extra_bits;
                new_error = new_payload_bits - budget_bits;
                if (new_error < best_error)
                {
                    best_error = new_error;
                    best_band = band;
                    best_next_bits = next_bits;
                    best_extra_bits = extra_bits;
                }
            }
            if (best_band < 0)
            {
                break;
            }
        }

        SubbandCodecLoopback_State.band_bits[best_band] = best_next_bits;
        payload_bits += best_extra_bits;
    }
}

static float Loop_Quantize_Value(float x, float scale, int bits)
{
    int qmax;
    int q;
    float step;
    float qf;
    float y;

    if ((bits < 2) || (scale < SUBBAND_CODEC_LOOP_EPS))
    {
        return 0.0f;
    }
    qmax = (1 << (bits - 1)) - 1;
    if (qmax <= 0)
    {
        return 0.0f;
    }

    step = scale / (float)qmax;
    if (step < SUBBAND_CODEC_LOOP_EPS)
    {
        return 0.0f;
    }
    qf = x / step;
    if (qf >= 0.0f)
    {
        q = (int)(qf + 0.5f);
    }
    else
    {
        q = (int)(qf - 0.5f);
    }
    if (q > qmax)
    {
        q = qmax;
        SUBBAND_CODEC_LOOP_DebugClippingCount++;
    }
    if (q < -qmax)
    {
        q = -qmax;
        SUBBAND_CODEC_LOOP_DebugClippingCount++;
    }
    y = (float)q * step;
    if (Loop_Is_Invalid(y) != 0)
    {
        SUBBAND_CODEC_LOOP_DebugInvalidCount++;
        return 0.0f;
    }
    return y;
}

static void Loop_Quantize_Dequantize(float *re, float *im)
{
    int bin;

    for (bin = 0; bin <= SUBBAND_NFFT / 2; bin++)
    {
        int band;
        int bits;
        float scale;

        band = Loop_Positive_Bin_To_Band(bin);
        bits = SubbandCodecLoopback_State.band_bits[band];
        scale = SubbandCodecLoopback_State.band_scale[band];

        re[bin] = Loop_Quantize_Value(re[bin], scale, bits);
        if ((bin == 0) || (bin == (SUBBAND_NFFT / 2)))
        {
            im[bin] = 0.0f;
        }
        else
        {
            im[bin] = Loop_Quantize_Value(im[bin], scale, bits);
        }
    }
}

static void Loop_Restore_Conjugate(float *re, float *im)
{
    int bin;

    im[0] = 0.0f;
    im[SUBBAND_NFFT / 2] = 0.0f;
    for (bin = 1; bin < SUBBAND_NFFT / 2; bin++)
    {
        int mirror;

        mirror = SUBBAND_NFFT - bin;
        re[mirror] = re[bin];
        im[mirror] = -im[bin];
    }
}

static void Loop_Update_Band_Debug(void)
{
    SUBBAND_CODEC_LOOP_DebugBandBits0 =
        (float)SubbandCodecLoopback_State.band_bits[0];
    SUBBAND_CODEC_LOOP_DebugBandBits1 =
        (float)SubbandCodecLoopback_State.band_bits[1];
    SUBBAND_CODEC_LOOP_DebugBandBits2 =
        (float)SubbandCodecLoopback_State.band_bits[2];
    SUBBAND_CODEC_LOOP_DebugBandBits3 =
        (float)SubbandCodecLoopback_State.band_bits[3];
    SUBBAND_CODEC_LOOP_DebugBandBits4 =
        (float)SubbandCodecLoopback_State.band_bits[4];
    SUBBAND_CODEC_LOOP_DebugBandBits5 =
        (float)SubbandCodecLoopback_State.band_bits[5];
    SUBBAND_CODEC_LOOP_DebugBandBits6 =
        (float)SubbandCodecLoopback_State.band_bits[6];
    SUBBAND_CODEC_LOOP_DebugBandBits7 =
        (float)SubbandCodecLoopback_State.band_bits[7];
}

static void Loop_Update_Rate_Debug(void)
{
    unsigned long payload_bits;
    unsigned long scalar_bits;
    unsigned long scalar_count;
    float hop_duration;

    payload_bits = Loop_Current_Payload_Bits();
    scalar_bits = Loop_Current_Scalar_Bits();
    scalar_count = Loop_Current_Scalar_Count();
    hop_duration = (float)SUBBAND_HOP / (float)SUBBAND_SAMPLE_RATE;

    if (hop_duration > 1.0e-20f)
    {
        SUBBAND_CODEC_LOOP_DebugEstimatedBitrateKbps =
            ((float)payload_bits / hop_duration) / 1000.0f;
    }
    else
    {
        SUBBAND_CODEC_LOOP_DebugEstimatedBitrateKbps = 0.0f;
    }
    if (SUBBAND_CODEC_LOOP_DebugEstimatedBitrateKbps > 1.0e-20f)
    {
        SUBBAND_CODEC_LOOP_DebugCompressionRatio =
            SUBBAND_CODEC_LOOP_PCM_KBPS /
            SUBBAND_CODEC_LOOP_DebugEstimatedBitrateKbps;
    }
    else
    {
        SUBBAND_CODEC_LOOP_DebugCompressionRatio = 0.0f;
    }
    if (scalar_count > 0UL)
    {
        SUBBAND_CODEC_LOOP_DebugAvgBitsPerScalar =
            (float)scalar_bits / (float)scalar_count;
    }
    else
    {
        SUBBAND_CODEC_LOOP_DebugAvgBitsPerScalar = 0.0f;
    }
    Loop_Update_Band_Debug();
}

static void Loop_Service_Requested_Target(void)
{
    int requested;

    requested = SUBBAND_CODEC_LOOP_DebugRequestedTargetKbps;
    if ((requested == 160) || (requested == 240) || (requested == 320))
    {
        SubbandCodecLoopback_SetTargetKbps(requested);
    }
}

void SubbandCodecLoopback_Init(void)
{
    if (SubbandCodecLoopback_State.initialized != 0)
    {
        return;
    }
    memset(&SubbandCodecLoopback_State, 0,
           sizeof(SubbandCodecLoopback_State));
    SubbandCodecLoopback_State.target_kbps =
        SUBBAND_CODEC_LOOP_DEFAULT_KBPS;
    SubbandCodecLoopback_State.initialized = 1;
    SUBBAND_CODEC_LOOP_DebugEnabled = 0;
    SUBBAND_CODEC_LOOP_DebugTargetKbps =
        SUBBAND_CODEC_LOOP_DEFAULT_KBPS;
}

void SubbandCodecLoopback_Reset(void)
{
    int enabled;
    int target_kbps;
    int initialized;

    SubbandCodecLoopback_Init();
    enabled = SubbandCodecLoopback_State.enabled;
    target_kbps = SubbandCodecLoopback_State.target_kbps;
    initialized = SubbandCodecLoopback_State.initialized;
    memset(&SubbandCodecLoopback_State, 0,
           sizeof(SubbandCodecLoopback_State));
    SubbandCodecLoopback_State.enabled = enabled;
    SubbandCodecLoopback_State.target_kbps = target_kbps;
    SubbandCodecLoopback_State.initialized = initialized;
    SUBBAND_CODEC_LOOP_DebugEnabled = enabled;
    SUBBAND_CODEC_LOOP_DebugTargetKbps = target_kbps;
    SUBBAND_CODEC_LOOP_DebugEstimatedBitrateKbps = 0.0f;
    SUBBAND_CODEC_LOOP_DebugCompressionRatio = 0.0f;
    SUBBAND_CODEC_LOOP_DebugAvgBitsPerScalar = 0.0f;
    SUBBAND_CODEC_LOOP_DebugInvalidCount = 0;
    SUBBAND_CODEC_LOOP_DebugClippingCount = 0;
    SUBBAND_CODEC_LOOP_DebugFrames = 0UL;
    Loop_Update_Band_Debug();
}

void SubbandCodecLoopback_SetEnabled(int enable)
{
    SubbandCodecLoopback_Init();
    SubbandCodecLoopback_State.enabled = (enable != 0) ? 1 : 0;
    SUBBAND_CODEC_LOOP_DebugEnabled = SubbandCodecLoopback_State.enabled;
}

int SubbandCodecLoopback_IsEnabled(void)
{
    SubbandCodecLoopback_Init();
    return SubbandCodecLoopback_State.enabled;
}

void SubbandCodecLoopback_SetTargetKbps(int kbps)
{
    SubbandCodecLoopback_Init();
    SubbandCodecLoopback_State.target_kbps =
        Loop_Normalize_Target_Kbps(kbps);
    SUBBAND_CODEC_LOOP_DebugTargetKbps =
        SubbandCodecLoopback_State.target_kbps;
}

int SubbandCodecLoopback_GetTargetKbps(void)
{
    SubbandCodecLoopback_Init();
    return SubbandCodecLoopback_State.target_kbps;
}

void SubbandCodecLoopback_ProcessSpectrum(float *re, float *im)
{
    SubbandCodecLoopback_Init();
    Loop_Service_Requested_Target();

    if ((re == 0) || (im == 0))
    {
        SUBBAND_CODEC_LOOP_DebugInvalidCount++;
        return;
    }
    if (SubbandCodecLoopback_State.enabled == 0)
    {
        SUBBAND_CODEC_LOOP_DebugEnabled = 0;
        return;
    }

    SUBBAND_CODEC_LOOP_DebugEnabled = 1;
    SUBBAND_CODEC_LOOP_DebugTargetKbps =
        SubbandCodecLoopback_State.target_kbps;
    Loop_Analyze_Bands(re, im);
    Loop_Compute_Scales();
    Loop_Allocate_Bits(SubbandCodecLoopback_State.target_kbps);
    Loop_Quantize_Dequantize(re, im);
    Loop_Restore_Conjugate(re, im);
    Loop_Update_Rate_Debug();
    SUBBAND_CODEC_LOOP_DebugFrames++;
}
