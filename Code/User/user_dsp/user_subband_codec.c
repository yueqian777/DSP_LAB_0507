/**
 * user_subband_codec.c
 *
 * Minimal WOLA-DFT subband scalar quantization codec used for offline
 * comparison of direct compression and denoise-then-compress.
 */

#include "user_subband_codec.h"
#include "math.h"
#include "string.h"

#define SUBBAND_CODEC_PI 3.14159265358979323846f
#define SUBBAND_CODEC_SIDE_BITS_PER_HOP \
    (SUBBAND_NUM_BANDS * \
     (SUBBAND_CODEC_BAND_BITS_SIDE_BITS + SUBBAND_CODEC_BAND_SCALE_SIDE_BITS))

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
    float band_scale[SUBBAND_NUM_BANDS];
    float band_energy[SUBBAND_NUM_BANDS];
    float band_energy_after[SUBBAND_NUM_BANDS];
    int band_bits[SUBBAND_NUM_BANDS];
    int scalar_count[SUBBAND_NUM_BANDS];
    int initialized;
} SubbandCodecState;

#if defined(__TI_COMPILER_VERSION__) || defined(__TMS320C6X__)
#pragma DATA_SECTION(SubbandCodec_State, ".subband_l2")
#endif
static SubbandCodecState SubbandCodec_State;

static float Codec_Abs(float x)
{
    if (x < 0.0f)
    {
        return -x;
    }
    return x;
}

static short Codec_Saturate_To_Short(float x, int *clip_count)
{
    if (x > 32767.0f)
    {
        if (clip_count != 0)
        {
            (*clip_count)++;
        }
        return 32767;
    }
    if (x < -32768.0f)
    {
        if (clip_count != 0)
        {
            (*clip_count)++;
        }
        return -32768;
    }
    return (short)x;
}

static int Codec_Is_Invalid(float x)
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

static int Codec_Positive_Bin_To_Band(int bin)
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

static int Codec_Bin_Scalar_Count(int bin)
{
    if ((bin == 0) || (bin == (SUBBAND_NFFT / 2)))
    {
        return 1;
    }
    return 2;
}

static void Codec_Bit_Reverse(float *re, float *im)
{
    unsigned int i;
    unsigned int j;

    j = 0;
    for (i = 1; i < (unsigned int)SUBBAND_NFFT; i++)
    {
        unsigned int bit;

        bit = (unsigned int)SUBBAND_NFFT >> 1;
        while ((j & bit) != 0U)
        {
            j ^= bit;
            bit >>= 1;
        }
        j ^= bit;

        if (i < j)
        {
            float tr;
            float ti;

            tr = re[i];
            ti = im[i];
            re[i] = re[j];
            im[i] = im[j];
            re[j] = tr;
            im[j] = ti;
        }
    }
}

static void Codec_FFT_InPlace(float *re, float *im)
{
    int len;

    Codec_Bit_Reverse(re, im);

    for (len = 2; len <= SUBBAND_NFFT; len <<= 1)
    {
        int half;
        int tw_step;
        int start;

        half = len >> 1;
        tw_step = SUBBAND_NFFT / len;

        for (start = 0; start < SUBBAND_NFFT; start += len)
        {
            int j;

            for (j = 0; j < half; j++)
            {
                int even;
                int odd;
                int tw_idx;
                float wr;
                float wi;
                float ur;
                float ui;
                float vr;
                float vi;

                even = start + j;
                odd = even + half;
                tw_idx = j * tw_step;
                wr = SubbandCodec_State.tw_re[tw_idx];
                wi = SubbandCodec_State.tw_im[tw_idx];
                ur = re[even];
                ui = im[even];
                vr = re[odd] * wr - im[odd] * wi;
                vi = re[odd] * wi + im[odd] * wr;
                re[even] = ur + vr;
                im[even] = ui + vi;
                re[odd] = ur - vr;
                im[odd] = ui - vi;
            }
        }
    }
}

static void Codec_IFFT_InPlace(float *re, float *im)
{
    int i;
    float inv_n;

    for (i = 0; i < SUBBAND_NFFT; i++)
    {
        im[i] = -im[i];
    }
    Codec_FFT_InPlace(re, im);
    inv_n = 1.0f / (float)SUBBAND_NFFT;
    for (i = 0; i < SUBBAND_NFFT; i++)
    {
        re[i] *= inv_n;
        im[i] = -im[i] * inv_n;
    }
}

static void Codec_Init(void)
{
    int i;

    if (SubbandCodec_State.initialized != 0)
    {
        return;
    }

    memset(&SubbandCodec_State, 0, sizeof(SubbandCodec_State));
    for (i = 0; i < SUBBAND_NFFT; i++)
    {
        float phase;
        float hann;

        phase = (2.0f * SUBBAND_CODEC_PI * (float)i) /
                (float)SUBBAND_NFFT;
        hann = 0.5f - 0.5f * cosf(phase);
        if (hann < 0.0f)
        {
            hann = 0.0f;
        }
        SubbandCodec_State.window[i] = sqrtf(hann);
    }
    for (i = 0; i < SUBBAND_NFFT / 2; i++)
    {
        float phase;

        phase = (-2.0f * SUBBAND_CODEC_PI * (float)i) /
                (float)SUBBAND_NFFT;
        SubbandCodec_State.tw_re[i] = cosf(phase);
        SubbandCodec_State.tw_im[i] = sinf(phase);
    }
    SubbandCodec_State.initialized = 1;
}

static void Codec_Reset_Stream(void)
{
    int initialized;

    initialized = SubbandCodec_State.initialized;
    memset(SubbandCodec_State.analysis_buf, 0,
           sizeof(SubbandCodec_State.analysis_buf));
    memset(SubbandCodec_State.ola_buf, 0,
           sizeof(SubbandCodec_State.ola_buf));
    memset(SubbandCodec_State.fft_re, 0,
           sizeof(SubbandCodec_State.fft_re));
    memset(SubbandCodec_State.fft_im, 0,
           sizeof(SubbandCodec_State.fft_im));
    memset(SubbandCodec_State.time_buf, 0,
           sizeof(SubbandCodec_State.time_buf));
    SubbandCodec_State.initialized = initialized;
}

void SubbandCodec_ResetStats(SubbandCodecStats *stats)
{
    int band;

    if (stats == 0)
    {
        return;
    }
    memset(stats, 0, sizeof(*stats));
    for (band = 0; band < SUBBAND_NUM_BANDS; band++)
    {
        stats->band_energy_before[band] = 0.0f;
        stats->band_energy_after[band] = 0.0f;
        stats->band_bits[band] = 0.0f;
    }
}

static void Codec_Clear_Band_Work(void)
{
    int band;

    for (band = 0; band < SUBBAND_NUM_BANDS; band++)
    {
        SubbandCodec_State.band_scale[band] = 0.0f;
        SubbandCodec_State.band_energy[band] = 0.0f;
        SubbandCodec_State.band_energy_after[band] = 0.0f;
        SubbandCodec_State.band_bits[band] =
            SUBBAND_CODEC_MIN_BITS_PER_SCALAR;
        SubbandCodec_State.scalar_count[band] = 0;
    }
}

static int Codec_Effective_Scalar_Bits(int bits)
{
    /* Values below 2 are drop modes and carry no scalar payload bits. */
    if (bits < 2)
    {
        return 0;
    }
    return bits;
}

static int Codec_Next_Band_Bits(int bits)
{
    if (bits < 2)
    {
        return 2;
    }
    return bits + 1;
}

static unsigned long Codec_Current_Payload_Bits(void)
{
    int band;
    unsigned long bits;

    bits = (unsigned long)SUBBAND_CODEC_SIDE_BITS_PER_HOP;
    for (band = 0; band < SUBBAND_NUM_BANDS; band++)
    {
        bits += (unsigned long)SubbandCodec_State.scalar_count[band] *
                (unsigned long)Codec_Effective_Scalar_Bits(
                    SubbandCodec_State.band_bits[band]);
    }
    return bits;
}

static void Codec_Analyze_Bands(float *re, float *im)
{
    int bin;

    Codec_Clear_Band_Work();
    for (bin = 0; bin <= SUBBAND_NFFT / 2; bin++)
    {
        int band;
        float ar;
        float ai;
        float energy;

        band = Codec_Positive_Bin_To_Band(bin);
        ar = Codec_Abs(re[bin]);
        ai = Codec_Abs(im[bin]);
        if ((bin == 0) || (bin == (SUBBAND_NFFT / 2)))
        {
            ai = 0.0f;
        }
        energy = re[bin] * re[bin] + im[bin] * im[bin];
        SubbandCodec_State.band_energy[band] += energy;
        if (ar > SubbandCodec_State.band_scale[band])
        {
            SubbandCodec_State.band_scale[band] = ar;
        }
        if (ai > SubbandCodec_State.band_scale[band])
        {
            SubbandCodec_State.band_scale[band] = ai;
        }
        SubbandCodec_State.scalar_count[band] +=
            Codec_Bin_Scalar_Count(bin);
    }
}

static void Codec_Allocate_Bits(int target_bitrate_kbps)
{
    float budget;
    unsigned long budget_bits;
    unsigned long payload_bits;

    budget = ((float)target_bitrate_kbps * 1000.0f *
              (float)SUBBAND_HOP) / (float)SUBBAND_SAMPLE_RATE;
    budget_bits = (unsigned long)(budget + 0.5f);
    payload_bits = Codec_Current_Payload_Bits();

    while (payload_bits < budget_bits)
    {
        int band;
        int best_band;
        float best_score;

        best_band = -1;
        best_score = -1.0f;
        for (band = 0; band < SUBBAND_NUM_BANDS; band++)
        {
            unsigned long extra_bits;
            float score;
            int next_bits;
            int cur_payload_bits;
            int next_payload_bits;

            next_bits = Codec_Next_Band_Bits(
                SubbandCodec_State.band_bits[band]);
            if (next_bits > SUBBAND_CODEC_MAX_BITS_PER_SCALAR)
            {
                continue;
            }
            cur_payload_bits = Codec_Effective_Scalar_Bits(
                SubbandCodec_State.band_bits[band]);
            next_payload_bits = Codec_Effective_Scalar_Bits(next_bits);
            extra_bits =
                (unsigned long)SubbandCodec_State.scalar_count[band] *
                (unsigned long)(next_payload_bits - cur_payload_bits);
            if ((payload_bits + extra_bits) > budget_bits)
            {
                continue;
            }
            score = logf(SubbandCodec_State.band_energy[band] + 1.0f) /
                    (float)(next_bits + 1);
            if (score > best_score)
            {
                best_score = score;
                best_band = band;
            }
        }
        if (best_band < 0)
        {
            break;
        }
        {
            int cur_bits;
            int next_bits;

            cur_bits = Codec_Effective_Scalar_Bits(
                SubbandCodec_State.band_bits[best_band]);
            next_bits = Codec_Next_Band_Bits(
                SubbandCodec_State.band_bits[best_band]);
            payload_bits +=
                (unsigned long)SubbandCodec_State.scalar_count[best_band] *
                (unsigned long)(Codec_Effective_Scalar_Bits(next_bits) -
                                cur_bits);
            SubbandCodec_State.band_bits[best_band] = next_bits;
        }
    }
}

static float Codec_Quantize_Value(float x, float scale, int bits)
{
    int qmax;
    int q;
    float qf;

    if (bits < 2)
    {
        return 0.0f;
    }
    qmax = (1 << (bits - 1)) - 1;
    if ((qmax <= 0) || (scale <= 1.0e-20f))
    {
        return 0.0f;
    }
    qf = x / scale;
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
    }
    if (q < -qmax)
    {
        q = -qmax;
    }
    return (float)q * scale;
}

static unsigned long Codec_Quantize_Dequantize(float *re, float *im)
{
    int bin;
    int band;
    unsigned long payload_bits;

    payload_bits = (unsigned long)SUBBAND_CODEC_SIDE_BITS_PER_HOP;
    for (bin = 0; bin <= SUBBAND_NFFT / 2; bin++)
    {
        int bits;
        int qmax;
        float scale;

        band = Codec_Positive_Bin_To_Band(bin);
        bits = SubbandCodec_State.band_bits[band];
        payload_bits +=
            (unsigned long)Codec_Effective_Scalar_Bits(bits) *
            (unsigned long)Codec_Bin_Scalar_Count(bin);
        if (bits >= 2)
        {
            qmax = (1 << (bits - 1)) - 1;
        }
        else
        {
            qmax = 0;
        }
        if (qmax > 0)
        {
            scale = SubbandCodec_State.band_scale[band] / (float)qmax;
        }
        else
        {
            scale = 0.0f;
        }

        re[bin] = Codec_Quantize_Value(re[bin], scale, bits);
        if ((bin == 0) || (bin == (SUBBAND_NFFT / 2)))
        {
            im[bin] = 0.0f;
        }
        else
        {
            im[bin] = Codec_Quantize_Value(im[bin], scale, bits);
        }
        SubbandCodec_State.band_energy_after[band] +=
            re[bin] * re[bin] + im[bin] * im[bin];
    }
    return payload_bits;
}

static void Codec_Restore_Conjugate(float *re, float *im)
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

static void Codec_Shift_Analysis(const short *input, int offset,
                                 int sample_count)
{
    int i;

    for (i = 0; i < SUBBAND_NFFT - SUBBAND_HOP; i++)
    {
        SubbandCodec_State.analysis_buf[i] =
            SubbandCodec_State.analysis_buf[i + SUBBAND_HOP];
    }
    for (i = 0; i < SUBBAND_HOP; i++)
    {
        int idx;

        idx = offset + i;
        if (idx < sample_count)
        {
            SubbandCodec_State.analysis_buf[SUBBAND_NFFT - SUBBAND_HOP + i] =
                (float)input[idx];
        }
        else
        {
            SubbandCodec_State.analysis_buf[SUBBAND_NFFT - SUBBAND_HOP + i] =
                0.0f;
        }
    }
}

static void Codec_Shift_OLA(void)
{
    int i;

    for (i = 0; i < SUBBAND_NFFT - SUBBAND_HOP; i++)
    {
        SubbandCodec_State.ola_buf[i] =
            SubbandCodec_State.ola_buf[i + SUBBAND_HOP];
    }
    for (i = SUBBAND_NFFT - SUBBAND_HOP; i < SUBBAND_NFFT; i++)
    {
        SubbandCodec_State.ola_buf[i] = 0.0f;
    }
}

static unsigned long Codec_Process_Hop(const short *input,
                                       short *output,
                                       int offset,
                                       int sample_count,
                                       int target_bitrate_kbps,
                                       SubbandCodecStats *stats)
{
    int i;
    int band;
    unsigned long payload_bits;

    Codec_Shift_Analysis(input, offset, sample_count);
    for (i = 0; i < SUBBAND_NFFT; i++)
    {
        SubbandCodec_State.fft_re[i] =
            SubbandCodec_State.analysis_buf[i] *
            SubbandCodec_State.window[i];
        SubbandCodec_State.fft_im[i] = 0.0f;
    }

    Codec_FFT_InPlace(SubbandCodec_State.fft_re, SubbandCodec_State.fft_im);
    Codec_Analyze_Bands(SubbandCodec_State.fft_re,
                        SubbandCodec_State.fft_im);
    Codec_Allocate_Bits(target_bitrate_kbps);
    payload_bits = Codec_Quantize_Dequantize(SubbandCodec_State.fft_re,
                                             SubbandCodec_State.fft_im);
    Codec_Restore_Conjugate(SubbandCodec_State.fft_re,
                            SubbandCodec_State.fft_im);
    Codec_IFFT_InPlace(SubbandCodec_State.fft_re, SubbandCodec_State.fft_im);

    for (i = 0; i < SUBBAND_NFFT; i++)
    {
        SubbandCodec_State.time_buf[i] =
            SubbandCodec_State.fft_re[i] * SubbandCodec_State.window[i];
        SubbandCodec_State.ola_buf[i] += SubbandCodec_State.time_buf[i];
        if (Codec_Is_Invalid(SubbandCodec_State.ola_buf[i]) != 0)
        {
            stats->invalid_count++;
            SubbandCodec_State.ola_buf[i] = 0.0f;
        }
    }

    for (i = 0; i < SUBBAND_HOP; i++)
    {
        int idx;
        short y;

        idx = offset + i;
        if (idx < sample_count)
        {
            y = Codec_Saturate_To_Short(SubbandCodec_State.ola_buf[i],
                                        &stats->output_clipping_count);
            output[idx] = y;
            if (y != 0)
            {
                stats->nonzero_output_count++;
            }
        }
    }

    for (band = 0; band < SUBBAND_NUM_BANDS; band++)
    {
        stats->band_energy_before[band] +=
            SubbandCodec_State.band_energy[band];
        stats->band_energy_after[band] +=
            SubbandCodec_State.band_energy_after[band];
        stats->band_bits[band] +=
            (float)SubbandCodec_State.band_bits[band];
        stats->scalar_count +=
            (unsigned long)SubbandCodec_State.scalar_count[band];
        stats->scalar_bits +=
            (unsigned long)SubbandCodec_State.scalar_count[band] *
            (unsigned long)Codec_Effective_Scalar_Bits(
                SubbandCodec_State.band_bits[band]);
    }

    Codec_Shift_OLA();
    return payload_bits;
}

int SubbandCodec_ProcessPcm(const short *input,
                            short *output,
                            int sample_count,
                            int target_bitrate_kbps,
                            SubbandCodecStats *stats)
{
    int hop;
    int hops;
    int i;
    float duration_sec;

    if ((input == 0) || (output == 0) || (stats == 0) ||
        (sample_count <= 0) || (target_bitrate_kbps <= 0))
    {
        return 1;
    }

    Codec_Init();
    Codec_Reset_Stream();
    SubbandCodec_ResetStats(stats);
    stats->target_bitrate_kbps = target_bitrate_kbps;

    for (i = 0; i < sample_count; i++)
    {
        output[i] = 0;
    }

    hops = sample_count / SUBBAND_HOP;
    stats->hops = hops;
    for (hop = 0; hop < hops; hop++)
    {
        int offset;

        offset = hop * SUBBAND_HOP;
        stats->payload_bits +=
            Codec_Process_Hop(input, output, offset, sample_count,
                              target_bitrate_kbps, stats);
    }

    if (hops > 0)
    {
        int band;

        for (band = 0; band < SUBBAND_NUM_BANDS; band++)
        {
            stats->band_energy_before[band] /= (float)hops;
            stats->band_energy_after[band] /= (float)hops;
            stats->band_bits[band] /= (float)hops;
        }
    }

    duration_sec = ((float)hops * (float)SUBBAND_HOP) /
                   (float)SUBBAND_SAMPLE_RATE;
    if (duration_sec > 1.0e-20f)
    {
        stats->bitrate_kbps =
            ((float)stats->payload_bits / duration_sec) / 1000.0f;
    }
    if (stats->bitrate_kbps > 1.0e-20f)
    {
        stats->compression_ratio =
            SUBBAND_CODEC_PCM_BITRATE_KBPS / stats->bitrate_kbps;
    }
    if (stats->scalar_count > 0UL)
    {
        stats->avg_bits_per_scalar =
            (float)stats->scalar_bits / (float)stats->scalar_count;
    }

    return 0;
}
