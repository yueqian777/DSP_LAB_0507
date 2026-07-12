/**
 * user_subband_denoise.c
 *
 * Two-second fixed-noise learning and FFT-bin Wiener denoise.
 */

#include "user_subband_denoise.h"
#include "string.h"

typedef struct
{
    float noise_accum[SUBBAND_POS_BINS];
    float noise_psd[SUBBAND_POS_BINS];
    float prev_clean_psd[SUBBAND_POS_BINS];
    float gain_smooth[SUBBAND_POS_BINS];
    float gain_tmp[SUBBAND_POS_BINS];
    float alpha;
    float gain_floor;
    float gain_smooth_up;
    float gain_smooth_down;
    float ms_smooth_power[SUBBAND_POS_BINS];
    float ms_learned_psd[SUBBAND_POS_BINS];
    float ms_block_min[SUBBAND_DENOISE_MS_NUM_BLOCKS][SUBBAND_POS_BINS];
    float ms_min[SUBBAND_POS_BINS];
    float mcra_speech_prob[SUBBAND_POS_BINS];
    float mcra_overdrive[SUBBAND_POS_BINS];
    float mcra_floor[SUBBAND_POS_BINS];
    float mcra_delta_low;
    float mcra_delta_high;
    float mcra_alpha_noise;
    float mcra_alpha_speech;
    float mcra_overdrive_speech;
    float mcra_overdrive_noise;
    float mcra_bias;
    float mcra_tonal_snr_min;
    float mcra_tonal_neighbor_ratio;
    float mcra_tonal_floor;
    unsigned long learn_count;
    unsigned long target_hops;
    unsigned long fade_count;
    unsigned long fade_total_hops;
    unsigned long ms_update_count;
    int noise_track_mode;
    int ms_block_index;
    int ms_hop_in_block;
    int mcra_strong_mode;
    int initialized;
    int enabled;
    int learning;
    int ready;
} SubbandDenoiseState;

#if defined(__TI_COMPILER_VERSION__) || defined(__TMS320C6X__)
#pragma DATA_SECTION(SubbandDenoise_State, ".subband_l2")
#endif
static SubbandDenoiseState SubbandDenoise_State;

volatile int SUBBAND_DENOISE_DebugEnabled = 0;
volatile int SUBBAND_DENOISE_DebugLearning = 0;
volatile int SUBBAND_DENOISE_DebugReady = 0;
volatile unsigned long SUBBAND_DENOISE_DebugLearnHops = 0;
volatile unsigned long SUBBAND_DENOISE_DebugTargetHops = 0;
volatile float SUBBAND_DENOISE_DebugLearnProgress = 0.0f;
volatile float SUBBAND_DENOISE_DebugInputPowerAvg = 0.0f;
volatile float SUBBAND_DENOISE_DebugOutputPowerAvg = 0.0f;
volatile float SUBBAND_DENOISE_DebugGainAvg = 1.0f;
volatile float SUBBAND_DENOISE_DebugMinGain = 1.0f;
volatile float SUBBAND_DENOISE_DebugMaxGain = 1.0f;
volatile float SUBBAND_DENOISE_DebugNoisePsdAvg = 0.0f;
volatile float SUBBAND_DENOISE_DebugFade = 0.0f;
volatile int SUBBAND_DENOISE_DebugNoiseTrackMode =
    SUBBAND_DENOISE_TRACK_DEFAULT;
volatile unsigned long SUBBAND_DENOISE_DebugMsUpdateCount = 0UL;
volatile float SUBBAND_DENOISE_DebugMsNoisePsdAvg = 0.0f;
volatile float SUBBAND_DENOISE_DebugMsMinPsdAvg = 0.0f;
volatile float SUBBAND_DENOISE_DebugMsBias = SUBBAND_DENOISE_MS_BIAS;
volatile float SUBBAND_DENOISE_DebugMsWindowSeconds = 0.0f;
volatile float SUBBAND_DENOISE_DebugMcraSpeechProbAvg = 0.0f;
volatile float SUBBAND_DENOISE_DebugMcraOverdriveAvg = 1.0f;
volatile float SUBBAND_DENOISE_DebugMcraDeltaLow = 1.5f;
volatile float SUBBAND_DENOISE_DebugMcraDeltaHigh = 4.0f;
volatile float SUBBAND_DENOISE_DebugMcraAlphaNoise = 0.85f;
volatile float SUBBAND_DENOISE_DebugMcraAlphaSpeech = 0.998f;
volatile float SUBBAND_DENOISE_DebugMcraFloorAvg = SUBBAND_DENOISE_GAIN_FLOOR;
volatile int SUBBAND_DENOISE_DebugMcraStrongMode = 0;
volatile unsigned long SUBBAND_DENOISE_DebugMcraTonalGuardHits = 0UL;
volatile unsigned long
    SUBBAND_DENOISE_DebugMcraTonalGuardBinsLastFrame = 0UL;
volatile unsigned long SUBBAND_DENOISE_DebugLearnProgressX1000000 = 0UL;
volatile unsigned long SUBBAND_DENOISE_DebugInputPowerAvgDiv16 = 0UL;
volatile unsigned long SUBBAND_DENOISE_DebugOutputPowerAvgDiv16 = 0UL;
volatile unsigned long SUBBAND_DENOISE_DebugGainAvgX1000000 = 1000000UL;
volatile unsigned long SUBBAND_DENOISE_DebugMinGainX1000000 = 1000000UL;
volatile unsigned long SUBBAND_DENOISE_DebugMaxGainX1000000 = 1000000UL;
volatile unsigned long SUBBAND_DENOISE_DebugNoisePsdAvgDiv16 = 0UL;
volatile unsigned long SUBBAND_DENOISE_DebugMcraSpeechProbAvgX1000000 = 0UL;
volatile unsigned long SUBBAND_DENOISE_DebugMcraOverdriveAvgX1000000 = 1000000UL;
volatile unsigned long SUBBAND_DENOISE_DebugMcraFloorAvgX1000000 =
    (unsigned long)(SUBBAND_DENOISE_GAIN_FLOOR * 1000000.0f + 0.5f);

#define SUBBAND_DENOISE_MS_LARGE 1.0e30f
#define SUBBAND_DENOISE_MS_HYBRID_SPEECH_GUARD 16.0f
#define SUBBAND_DENOISE_MS_HYBRID_POWER_GUARD 12.0f
#define SUBBAND_DENOISE_MS_HYBRID_MAX_FACTOR 12.0f
#define SUBBAND_DENOISE_MS_HYBRID_TRIGGER_FACTOR 1.80f
#define SUBBAND_DENOISE_MS_HYBRID_TRIGGER_BINS (SUBBAND_POS_BINS / 6)

static unsigned long Round_To_Hops(float seconds)
{
    float hops;
    unsigned long rounded;

    hops = (seconds * SUBBAND_SAMPLE_RATE) / (float)SUBBAND_HOP;
    if (hops < 1.0f)
    {
        hops = 1.0f;
    }

    rounded = (unsigned long)(hops + 0.5f);
    if (rounded < 1UL)
    {
        rounded = 1UL;
    }

    return rounded;
}

static float Clamp_Float(float value, float lo, float hi)
{
    if (value < lo)
    {
        return lo;
    }
    if (value > hi)
    {
        return hi;
    }
    return value;
}

static unsigned long Debug_Unit_To_X1000000(float value)
{
    if (value <= 0.0f)
    {
        return 0UL;
    }
    if (value >= 4000.0f)
    {
        return 0xFFFFFFFFUL;
    }
    return (unsigned long)(value * 1000000.0f + 0.5f);
}

static unsigned long Debug_Power_To_Q16(float value)
{
    if (value <= 0.0f)
    {
        return 0UL;
    }
    if (value >= 68719476720.0f)
    {
        return 0xFFFFFFFFUL;
    }
    return (unsigned long)(value / 16.0f + 0.5f);
}

static void Update_Debug_Integer_Mirrors(void)
{
    SUBBAND_DENOISE_DebugLearnProgressX1000000 =
        Debug_Unit_To_X1000000(SUBBAND_DENOISE_DebugLearnProgress);
    SUBBAND_DENOISE_DebugInputPowerAvgDiv16 =
        Debug_Power_To_Q16(SUBBAND_DENOISE_DebugInputPowerAvg);
    SUBBAND_DENOISE_DebugOutputPowerAvgDiv16 =
        Debug_Power_To_Q16(SUBBAND_DENOISE_DebugOutputPowerAvg);
    SUBBAND_DENOISE_DebugGainAvgX1000000 =
        Debug_Unit_To_X1000000(SUBBAND_DENOISE_DebugGainAvg);
    SUBBAND_DENOISE_DebugMinGainX1000000 =
        Debug_Unit_To_X1000000(SUBBAND_DENOISE_DebugMinGain);
    SUBBAND_DENOISE_DebugMaxGainX1000000 =
        Debug_Unit_To_X1000000(SUBBAND_DENOISE_DebugMaxGain);
    SUBBAND_DENOISE_DebugNoisePsdAvgDiv16 =
        Debug_Power_To_Q16(SUBBAND_DENOISE_DebugNoisePsdAvg);
    SUBBAND_DENOISE_DebugMcraSpeechProbAvgX1000000 =
        Debug_Unit_To_X1000000(SUBBAND_DENOISE_DebugMcraSpeechProbAvg);
    SUBBAND_DENOISE_DebugMcraOverdriveAvgX1000000 =
        Debug_Unit_To_X1000000(SUBBAND_DENOISE_DebugMcraOverdriveAvg);
    SUBBAND_DENOISE_DebugMcraFloorAvgX1000000 =
        Debug_Unit_To_X1000000(SUBBAND_DENOISE_DebugMcraFloorAvg);
}

static int Normalize_Noise_Track_Mode(int mode)
{
    switch (mode)
    {
    case SUBBAND_DENOISE_TRACK_FIXED:
    case SUBBAND_DENOISE_TRACK_MINSTAT:
    case SUBBAND_DENOISE_TRACK_HYBRID:
    case SUBBAND_DENOISE_TRACK_MCRA:
        return mode;
    default:
        return SUBBAND_DENOISE_TRACK_FIXED;
    }
}

static float MinStat_Window_Seconds(void)
{
    return ((float)(SUBBAND_DENOISE_MS_NUM_BLOCKS *
                    SUBBAND_DENOISE_MS_BLOCK_HOPS *
                    SUBBAND_HOP)) /
           SUBBAND_SAMPLE_RATE;
}

static void Clear_MinStat_Block(int block_index, float value)
{
    int k;

    if ((block_index < 0) ||
        (block_index >= SUBBAND_DENOISE_MS_NUM_BLOCKS))
    {
        return;
    }

    for (k = 0; k < SUBBAND_POS_BINS; k++)
    {
        SubbandDenoise_State.ms_block_min[block_index][k] = value;
    }
}

static void Set_Mcra_Default_Params_Internal(void)
{
    SubbandDenoise_State.mcra_delta_low = 1.5f;
    SubbandDenoise_State.mcra_delta_high = 4.0f;
    SubbandDenoise_State.mcra_alpha_noise = 0.85f;
    SubbandDenoise_State.mcra_alpha_speech = 0.998f;
    SubbandDenoise_State.mcra_overdrive_speech = 1.10f;
    SubbandDenoise_State.mcra_overdrive_noise = 1.70f;
    SubbandDenoise_State.mcra_bias = 1.40f;
    SubbandDenoise_State.mcra_tonal_snr_min = 3.0f;
    SubbandDenoise_State.mcra_tonal_neighbor_ratio = 1.35f;
    SubbandDenoise_State.mcra_tonal_floor = 0.45f;
    SubbandDenoise_State.mcra_strong_mode = 0;
}

static float Mcra_Floor_For_Bin(int k)
{
    float freq;

    freq = ((float)k * SUBBAND_SAMPLE_RATE) / (float)SUBBAND_NFFT;
    if (SubbandDenoise_State.mcra_strong_mode != 0)
    {
        if (freq < 150.0f)
        {
            return 0.05f;
        }
        if (freq < 4000.0f)
        {
            return 0.12f;
        }
        if (freq < 8000.0f)
        {
            return 0.08f;
        }
        return 0.05f;
    }

    if (freq < 150.0f)
    {
        return 0.08f;
    }
    if (freq < 4000.0f)
    {
        return 0.15f;
    }
    if (freq < 8000.0f)
    {
        return 0.10f;
    }
    return 0.06f;
}

static int Mcra_Is_Speech_Band(int k)
{
    float freq;

    freq = ((float)k * SUBBAND_SAMPLE_RATE) / (float)SUBBAND_NFFT;
    return ((freq >= 150.0f) && (freq < 4000.0f)) ? 1 : 0;
}

static float Mcra_Effective_Floor_For_Bin(int k)
{
    float floor_value;

    floor_value = SubbandDenoise_State.mcra_floor[k];
    if ((SubbandDenoise_State.mcra_speech_prob[k] > 0.80f) &&
        (floor_value < SubbandDenoise_State.gain_floor))
    {
        floor_value = SubbandDenoise_State.gain_floor;
    }
    return floor_value;
}

static void Reset_Mcra_State_Internal(void)
{
    int k;

    for (k = 0; k < SUBBAND_POS_BINS; k++)
    {
        SubbandDenoise_State.mcra_speech_prob[k] = 0.0f;
        SubbandDenoise_State.mcra_overdrive[k] =
            SubbandDenoise_State.mcra_overdrive_noise;
        SubbandDenoise_State.mcra_floor[k] = Mcra_Floor_For_Bin(k);
    }
}

static void Reset_Noise_Tracker_Internal(void)
{
    int b;
    int k;

    for (k = 0; k < SUBBAND_POS_BINS; k++)
    {
        float base;

        base = SubbandDenoise_State.noise_psd[k];
        if (base < SUBBAND_DENOISE_EPS)
        {
            base = SUBBAND_DENOISE_EPS;
        }

        SubbandDenoise_State.ms_smooth_power[k] = base;
        SubbandDenoise_State.ms_learned_psd[k] = base;
        SubbandDenoise_State.ms_min[k] = base;
        for (b = 0; b < SUBBAND_DENOISE_MS_NUM_BLOCKS; b++)
        {
            SubbandDenoise_State.ms_block_min[b][k] = base;
        }
    }

    SubbandDenoise_State.ms_block_index = 0;
    SubbandDenoise_State.ms_hop_in_block = 0;
    SubbandDenoise_State.ms_update_count = 0UL;
    Reset_Mcra_State_Internal();
}

static void Update_Minimum_Statistics(const float *re, const float *im)
{
    int b;
    int k;
    int block_index;
    int hybrid_allow_update;
    int hybrid_trigger_bins;

    block_index = SubbandDenoise_State.ms_block_index;
    hybrid_allow_update = 1;
    hybrid_trigger_bins = 0;

    for (k = 0; k < SUBBAND_POS_BINS; k++)
    {
        float power;
        float smooth_power;
        float min_power;
        float candidate;
        float old_noise;
        float candidate_hi;

        power = re[k] * re[k] + im[k] * im[k];
        smooth_power =
            SUBBAND_DENOISE_MS_POWER_ALPHA *
                SubbandDenoise_State.ms_smooth_power[k] +
            (1.0f - SUBBAND_DENOISE_MS_POWER_ALPHA) * power;
        if (smooth_power < SUBBAND_DENOISE_EPS)
        {
            smooth_power = SUBBAND_DENOISE_EPS;
        }
        SubbandDenoise_State.ms_smooth_power[k] = smooth_power;

        if (smooth_power <
            SubbandDenoise_State.ms_block_min[block_index][k])
        {
            SubbandDenoise_State.ms_block_min[block_index][k] =
                smooth_power;
        }

        min_power = SubbandDenoise_State.ms_block_min[0][k];
        for (b = 1; b < SUBBAND_DENOISE_MS_NUM_BLOCKS; b++)
        {
            if (SubbandDenoise_State.ms_block_min[b][k] < min_power)
            {
                min_power = SubbandDenoise_State.ms_block_min[b][k];
            }
        }
        if (min_power < SUBBAND_DENOISE_EPS)
        {
            min_power = SUBBAND_DENOISE_EPS;
        }
        SubbandDenoise_State.ms_min[k] = min_power;

        candidate = SUBBAND_DENOISE_MS_BIAS * min_power;
        if (candidate < SUBBAND_DENOISE_EPS)
        {
            candidate = SUBBAND_DENOISE_EPS;
        }
        candidate_hi = power + (4.0f * SUBBAND_DENOISE_EPS);
        if (candidate > candidate_hi)
        {
            candidate = candidate_hi;
        }

        old_noise = SubbandDenoise_State.noise_psd[k];
        if (old_noise < SUBBAND_DENOISE_EPS)
        {
            old_noise = SUBBAND_DENOISE_EPS;
        }

        if ((candidate >
             old_noise * SUBBAND_DENOISE_MS_HYBRID_TRIGGER_FACTOR) &&
            (power <=
             old_noise * SUBBAND_DENOISE_MS_HYBRID_POWER_GUARD))
        {
            hybrid_trigger_bins++;
        }
    }

    if ((SubbandDenoise_State.noise_track_mode ==
         SUBBAND_DENOISE_TRACK_HYBRID) &&
        (hybrid_trigger_bins < SUBBAND_DENOISE_MS_HYBRID_TRIGGER_BINS))
    {
        hybrid_allow_update = 0;
    }

    for (k = 0; k < SUBBAND_POS_BINS; k++)
    {
        float power;
        float min_power;
        float candidate;
        float old_noise;
        float learned_noise;
        float noise_alpha;
        float candidate_hi;
        float ratio;
        float speech_prob;
        float denom;
        float alpha_update;
        float overdrive;
        float snr_ratio;
        float snr_speech_prob;
        int mcra_speech_guard;
        int mcra_tonal_guard;
        int mcra_active_noise_update;

        power = re[k] * re[k] + im[k] * im[k];
        min_power = SubbandDenoise_State.ms_min[k];
        if (min_power < SUBBAND_DENOISE_EPS)
        {
            min_power = SUBBAND_DENOISE_EPS;
        }

        candidate = SUBBAND_DENOISE_MS_BIAS * min_power;
        if (candidate < SUBBAND_DENOISE_EPS)
        {
            candidate = SUBBAND_DENOISE_EPS;
        }
        candidate_hi = power + (4.0f * SUBBAND_DENOISE_EPS);
        if (candidate > candidate_hi)
        {
            candidate = candidate_hi;
        }

        old_noise = SubbandDenoise_State.noise_psd[k];
        if (old_noise < SUBBAND_DENOISE_EPS)
        {
            old_noise = SUBBAND_DENOISE_EPS;
        }

        if (SubbandDenoise_State.noise_track_mode ==
            SUBBAND_DENOISE_TRACK_MCRA)
        {
            int mcra_allow_up;
            float tonal_noise_cap;

            mcra_allow_up =
                (hybrid_trigger_bins >=
                 SUBBAND_DENOISE_MS_HYBRID_TRIGGER_BINS) ? 1 : 0;

            learned_noise = SubbandDenoise_State.ms_learned_psd[k];
            if (learned_noise < SUBBAND_DENOISE_EPS)
            {
                learned_noise = SUBBAND_DENOISE_EPS;
            }

            denom = SubbandDenoise_State.mcra_delta_high -
                    SubbandDenoise_State.mcra_delta_low;
            if (denom < 0.01f)
            {
                denom = 0.01f;
            }

            ratio = SubbandDenoise_State.ms_smooth_power[k] /
                    (min_power + SUBBAND_DENOISE_EPS);
            speech_prob =
                (ratio - SubbandDenoise_State.mcra_delta_low) / denom;
            speech_prob = Clamp_Float(speech_prob, 0.0f, 1.0f);

            snr_ratio = power / (old_noise + SUBBAND_DENOISE_EPS);
            snr_speech_prob = (snr_ratio - 2.0f) / 10.0f;
            snr_speech_prob =
                Clamp_Float(snr_speech_prob, 0.0f, 1.0f);
            if (snr_speech_prob > speech_prob)
            {
                speech_prob = snr_speech_prob;
            }
            mcra_tonal_guard = 0;
            if ((k > 0) && (k < (SUBBAND_NFFT / 2)) &&
                (Mcra_Is_Speech_Band(k) != 0) &&
                (snr_ratio >
                 SubbandDenoise_State.mcra_tonal_snr_min))
            {
                float left_power;
                float right_power;
                float neighbor_power;

                left_power = re[k - 1] * re[k - 1] +
                             im[k - 1] * im[k - 1];
                right_power = re[k + 1] * re[k + 1] +
                              im[k + 1] * im[k + 1];
                neighbor_power = 0.5f * (left_power + right_power) +
                                 SUBBAND_DENOISE_EPS;
                if (power >
                    (SubbandDenoise_State.mcra_tonal_neighbor_ratio *
                     neighbor_power))
                {
                    mcra_tonal_guard = 1;
                    SUBBAND_DENOISE_DebugMcraTonalGuardHits++;
                    SUBBAND_DENOISE_DebugMcraTonalGuardBinsLastFrame++;
                    if (speech_prob < 0.95f)
                    {
                        speech_prob = 0.95f;
                    }
                }
            }

            candidate = SubbandDenoise_State.mcra_bias * min_power;
            if (candidate < SUBBAND_DENOISE_EPS)
            {
                candidate = SUBBAND_DENOISE_EPS;
            }
            candidate_hi = power + (4.0f * SUBBAND_DENOISE_EPS);
            if (candidate > candidate_hi)
            {
                candidate = candidate_hi;
            }
            candidate_hi = old_noise * 8.0f;
            if (candidate > candidate_hi)
            {
                candidate = candidate_hi;
            }
            if ((mcra_allow_up == 0) && (candidate > old_noise))
            {
                candidate = old_noise;
            }

            mcra_speech_guard = 0;
            mcra_active_noise_update = 0;
            if ((power >
                 old_noise * SUBBAND_DENOISE_MS_HYBRID_POWER_GUARD) ||
                (candidate >
                 old_noise * SUBBAND_DENOISE_MS_HYBRID_SPEECH_GUARD))
            {
                mcra_speech_guard = 1;
            }
            if (((mcra_speech_guard != 0) ||
                 (mcra_tonal_guard != 0)) &&
                (candidate > old_noise))
            {
                candidate = old_noise;
            }
            else if (candidate > old_noise)
            {
                mcra_active_noise_update = 1;
            }
            if (mcra_active_noise_update == 0)
            {
                candidate = old_noise;
            }
            SubbandDenoise_State.mcra_speech_prob[k] = speech_prob;

            alpha_update =
                speech_prob * SubbandDenoise_State.mcra_alpha_speech +
                (1.0f - speech_prob) *
                    SubbandDenoise_State.mcra_alpha_noise;
            alpha_update = Clamp_Float(alpha_update, 0.0f, 0.9999f);
            if ((SubbandDenoise_State.mcra_strong_mode == 0) &&
                (alpha_update < 0.97f))
            {
                alpha_update = 0.97f;
            }

            SubbandDenoise_State.noise_psd[k] =
                alpha_update * old_noise +
                (1.0f - alpha_update) * candidate;
            if (mcra_tonal_guard != 0)
            {
                tonal_noise_cap = learned_noise * 1.75f;
                if (tonal_noise_cap < SUBBAND_DENOISE_EPS)
                {
                    tonal_noise_cap = SUBBAND_DENOISE_EPS;
                }
                if (SubbandDenoise_State.noise_psd[k] >
                    tonal_noise_cap)
                {
                    SubbandDenoise_State.noise_psd[k] =
                        tonal_noise_cap;
                }
            }
            if (SubbandDenoise_State.noise_psd[k] <
                SUBBAND_DENOISE_EPS)
            {
                SubbandDenoise_State.noise_psd[k] =
                    SUBBAND_DENOISE_EPS;
            }

            overdrive =
                SubbandDenoise_State.mcra_overdrive_speech * speech_prob +
                SubbandDenoise_State.mcra_overdrive_noise *
                    (1.0f - speech_prob);
            SubbandDenoise_State.mcra_overdrive[k] =
                Clamp_Float(overdrive, 1.0f, 4.0f);
            SubbandDenoise_State.mcra_floor[k] = Mcra_Floor_For_Bin(k);
            if ((mcra_tonal_guard != 0) &&
                (SubbandDenoise_State.mcra_strong_mode == 0) &&
                (SubbandDenoise_State.mcra_floor[k] <
                 SubbandDenoise_State.mcra_tonal_floor))
            {
                SubbandDenoise_State.mcra_floor[k] =
                    SubbandDenoise_State.mcra_tonal_floor;
            }
            continue;
        }

        if (SubbandDenoise_State.noise_track_mode ==
            SUBBAND_DENOISE_TRACK_HYBRID)
        {
            learned_noise = SubbandDenoise_State.ms_learned_psd[k];
            if (learned_noise < SUBBAND_DENOISE_EPS)
            {
                learned_noise = SUBBAND_DENOISE_EPS;
            }

            if ((hybrid_allow_update == 0) ||
                (power >
                 old_noise * SUBBAND_DENOISE_MS_HYBRID_POWER_GUARD) ||
                (candidate >
                 old_noise * SUBBAND_DENOISE_MS_HYBRID_SPEECH_GUARD))
            {
                candidate = old_noise;
            }
            candidate_hi =
                learned_noise * SUBBAND_DENOISE_MS_HYBRID_MAX_FACTOR;
            if (candidate > candidate_hi)
            {
                candidate = candidate_hi;
            }
        }

        if (candidate > old_noise)
        {
            noise_alpha = SUBBAND_DENOISE_MS_NOISE_UP_ALPHA;
        }
        else
        {
            noise_alpha = SUBBAND_DENOISE_MS_NOISE_DOWN_ALPHA;
        }

        SubbandDenoise_State.noise_psd[k] =
            noise_alpha * old_noise + (1.0f - noise_alpha) * candidate;
        if (SubbandDenoise_State.noise_psd[k] < SUBBAND_DENOISE_EPS)
        {
            SubbandDenoise_State.noise_psd[k] = SUBBAND_DENOISE_EPS;
        }
    }

    SubbandDenoise_State.ms_update_count++;
    SubbandDenoise_State.ms_hop_in_block++;
    if (SubbandDenoise_State.ms_hop_in_block >=
        SUBBAND_DENOISE_MS_BLOCK_HOPS)
    {
        SubbandDenoise_State.ms_hop_in_block = 0;
        SubbandDenoise_State.ms_block_index++;
        if (SubbandDenoise_State.ms_block_index >=
            SUBBAND_DENOISE_MS_NUM_BLOCKS)
        {
            SubbandDenoise_State.ms_block_index = 0;
        }
        Clear_MinStat_Block(SubbandDenoise_State.ms_block_index,
                            SUBBAND_DENOISE_MS_LARGE);
    }
}

static void Update_Debug_State(void)
{
    int k;
    float progress;
    float noise_sum;
    float ms_min_sum;
    float mcra_speech_sum;
    float mcra_overdrive_sum;
    float mcra_floor_sum;

    if (SubbandDenoise_State.target_hops > 0UL)
    {
        progress = (float)SubbandDenoise_State.learn_count /
                   (float)SubbandDenoise_State.target_hops;
        if (progress > 1.0f)
        {
            progress = 1.0f;
        }
    }
    else
    {
        progress = 0.0f;
    }

    SUBBAND_DENOISE_DebugEnabled = SubbandDenoise_State.enabled;
    SUBBAND_DENOISE_DebugLearning = SubbandDenoise_State.learning;
    SUBBAND_DENOISE_DebugReady = SubbandDenoise_State.ready;
    SUBBAND_DENOISE_DebugLearnHops = SubbandDenoise_State.learn_count;
    SUBBAND_DENOISE_DebugTargetHops = SubbandDenoise_State.target_hops;
    SUBBAND_DENOISE_DebugLearnProgress = progress;
    SUBBAND_DENOISE_DebugFade = SubbandDenoise_GetFade();
    SUBBAND_DENOISE_DebugNoiseTrackMode =
        SubbandDenoise_State.noise_track_mode;
    SUBBAND_DENOISE_DebugMsUpdateCount =
        SubbandDenoise_State.ms_update_count;
    SUBBAND_DENOISE_DebugMsBias = SUBBAND_DENOISE_MS_BIAS;
    SUBBAND_DENOISE_DebugMsWindowSeconds = MinStat_Window_Seconds();

    noise_sum = 0.0f;
    ms_min_sum = 0.0f;
    mcra_speech_sum = 0.0f;
    mcra_overdrive_sum = 0.0f;
    mcra_floor_sum = 0.0f;
    for (k = 0; k < SUBBAND_POS_BINS; k++)
    {
        noise_sum += SubbandDenoise_State.noise_psd[k];
        ms_min_sum += SubbandDenoise_State.ms_min[k];
        mcra_speech_sum += SubbandDenoise_State.mcra_speech_prob[k];
        mcra_overdrive_sum += SubbandDenoise_State.mcra_overdrive[k];
        mcra_floor_sum += SubbandDenoise_State.mcra_floor[k];
    }
    SUBBAND_DENOISE_DebugNoisePsdAvg =
        noise_sum / (float)SUBBAND_POS_BINS;
    SUBBAND_DENOISE_DebugMsNoisePsdAvg =
        SUBBAND_DENOISE_DebugNoisePsdAvg;
    SUBBAND_DENOISE_DebugMsMinPsdAvg =
        ms_min_sum / (float)SUBBAND_POS_BINS;
    SUBBAND_DENOISE_DebugMcraSpeechProbAvg =
        mcra_speech_sum / (float)SUBBAND_POS_BINS;
    SUBBAND_DENOISE_DebugMcraOverdriveAvg =
        mcra_overdrive_sum / (float)SUBBAND_POS_BINS;
    SUBBAND_DENOISE_DebugMcraDeltaLow =
        SubbandDenoise_State.mcra_delta_low;
    SUBBAND_DENOISE_DebugMcraDeltaHigh =
        SubbandDenoise_State.mcra_delta_high;
    SUBBAND_DENOISE_DebugMcraAlphaNoise =
        SubbandDenoise_State.mcra_alpha_noise;
    SUBBAND_DENOISE_DebugMcraAlphaSpeech =
        SubbandDenoise_State.mcra_alpha_speech;
    SUBBAND_DENOISE_DebugMcraFloorAvg =
        mcra_floor_sum / (float)SUBBAND_POS_BINS;
    SUBBAND_DENOISE_DebugMcraStrongMode =
        SubbandDenoise_State.mcra_strong_mode;
    Update_Debug_Integer_Mirrors();
}

static void Start_Learning_Internal(void)
{
    int k;

    memset(SubbandDenoise_State.noise_accum, 0,
           sizeof(SubbandDenoise_State.noise_accum));
    memset(SubbandDenoise_State.noise_psd, 0,
           sizeof(SubbandDenoise_State.noise_psd));
    memset(SubbandDenoise_State.prev_clean_psd, 0,
           sizeof(SubbandDenoise_State.prev_clean_psd));
    for (k = 0; k < SUBBAND_POS_BINS; k++)
    {
        SubbandDenoise_State.gain_smooth[k] = 1.0f;
        SubbandDenoise_State.gain_tmp[k] = 1.0f;
    }

    SubbandDenoise_State.learn_count = 0UL;
    SubbandDenoise_State.fade_count = 0UL;
    SubbandDenoise_State.ms_update_count = 0UL;
    SubbandDenoise_State.ms_block_index = 0;
    SubbandDenoise_State.ms_hop_in_block = 0;
    SubbandDenoise_State.learning = 1;
    SubbandDenoise_State.ready = 0;
    SubbandDenoise_State.enabled = 0;
    Reset_Noise_Tracker_Internal();
    SUBBAND_DENOISE_DebugInputPowerAvg = 0.0f;
    SUBBAND_DENOISE_DebugOutputPowerAvg = 0.0f;
    SUBBAND_DENOISE_DebugGainAvg = 1.0f;
    SUBBAND_DENOISE_DebugMinGain = 1.0f;
    SUBBAND_DENOISE_DebugMaxGain = 1.0f;
    Update_Debug_State();
}

static void Finalize_Learning(void)
{
    int k;
    float inv_count;
    float psd_sum;

    if (SubbandDenoise_State.learn_count == 0UL)
    {
        inv_count = 1.0f;
    }
    else
    {
        inv_count = 1.0f / (float)SubbandDenoise_State.learn_count;
    }

    psd_sum = 0.0f;
    for (k = 0; k < SUBBAND_POS_BINS; k++)
    {
        float psd;

        psd = SubbandDenoise_State.noise_accum[k] * inv_count;
        if (psd < SUBBAND_DENOISE_EPS)
        {
            psd = SUBBAND_DENOISE_EPS;
        }
        SubbandDenoise_State.noise_psd[k] = psd;
        SubbandDenoise_State.prev_clean_psd[k] = 0.0f;
        SubbandDenoise_State.gain_smooth[k] = 1.0f;
        SubbandDenoise_State.gain_tmp[k] = 1.0f;
        psd_sum += psd;
    }

    SubbandDenoise_State.learning = 0;
    SubbandDenoise_State.ready = 1;
    Reset_Noise_Tracker_Internal();
#if SUBBAND_DENOISE_AUTO_ENABLE_AFTER_LEARN
    SubbandDenoise_State.enabled = 1;
#endif
    SubbandDenoise_State.fade_count = 0UL;
    (void)psd_sum;
    Update_Debug_State();
}

void SubbandDenoise_Init(void)
{
    if (SubbandDenoise_State.initialized != 0)
    {
        return;
    }

    memset(&SubbandDenoise_State, 0, sizeof(SubbandDenoise_State));
    SubbandDenoise_State.alpha = SUBBAND_DENOISE_ALPHA;
    SubbandDenoise_State.gain_floor = SUBBAND_DENOISE_GAIN_FLOOR;
    SubbandDenoise_State.gain_smooth_up = SUBBAND_DENOISE_GAIN_SMOOTH_UP;
    SubbandDenoise_State.gain_smooth_down = SUBBAND_DENOISE_GAIN_SMOOTH_DOWN;
    Set_Mcra_Default_Params_Internal();
    Reset_Mcra_State_Internal();
    SubbandDenoise_State.noise_track_mode =
        Normalize_Noise_Track_Mode(SUBBAND_DENOISE_TRACK_DEFAULT);
    SubbandDenoise_State.target_hops =
        Round_To_Hops(SUBBAND_DENOISE_DEFAULT_LEARN_SECONDS);
    SubbandDenoise_State.fade_total_hops =
        Round_To_Hops(SUBBAND_DENOISE_FADE_SECONDS);
    SubbandDenoise_State.enabled = SUBBAND_DENOISE_ENABLE_DEFAULT;
    SubbandDenoise_State.initialized = 1;

#if SUBBAND_DENOISE_AUTO_LEARN_ON_INIT
    Start_Learning_Internal();
#else
    Update_Debug_State();
#endif
}

void SubbandDenoise_Reset(void)
{
    SUBBAND_DENOISE_DebugMcraTonalGuardHits = 0UL;
    SUBBAND_DENOISE_DebugMcraTonalGuardBinsLastFrame = 0UL;
    SubbandDenoise_State.initialized = 0;
    SubbandDenoise_Init();
}

void SubbandDenoise_SetEnabled(int enable)
{
    SubbandDenoise_Init();

    SubbandDenoise_State.enabled = (enable != 0) ? 1 : 0;
    Update_Debug_State();
}

int SubbandDenoise_IsEnabled(void)
{
    SubbandDenoise_Init();
    return SubbandDenoise_State.enabled;
}

void SubbandDenoise_StartNoiseLearning(void)
{
    SubbandDenoise_Init();
    Start_Learning_Internal();
}

void SubbandDenoise_StopLearning(void)
{
    SubbandDenoise_Init();
    SubbandDenoise_State.learning = 0;
    Update_Debug_State();
}

int SubbandDenoise_IsLearning(void)
{
    SubbandDenoise_Init();
    return SubbandDenoise_State.learning;
}

int SubbandDenoise_IsReady(void)
{
    SubbandDenoise_Init();
    return SubbandDenoise_State.ready;
}

float SubbandDenoise_GetLearnProgress(void)
{
    SubbandDenoise_Init();
    Update_Debug_State();
    return SUBBAND_DENOISE_DebugLearnProgress;
}

void SubbandDenoise_SetParams(float alpha, float gain_floor,
                              float gain_smooth_up, float gain_smooth_down)
{
    SubbandDenoise_Init();

    SubbandDenoise_State.alpha = Clamp_Float(alpha, 0.0f, 0.999f);
    SubbandDenoise_State.gain_floor = Clamp_Float(gain_floor, 0.01f, 1.0f);
    SubbandDenoise_State.gain_smooth_up =
        Clamp_Float(gain_smooth_up, 0.0f, 0.999f);
    SubbandDenoise_State.gain_smooth_down =
        Clamp_Float(gain_smooth_down, 0.0f, 0.999f);
}

void SubbandDenoise_SetNoiseTrackMode(int mode)
{
    SubbandDenoise_Init();

    SubbandDenoise_State.noise_track_mode =
        Normalize_Noise_Track_Mode(mode);
    if (SubbandDenoise_State.ready != 0)
    {
        Reset_Noise_Tracker_Internal();
    }
    Update_Debug_State();
}

int SubbandDenoise_GetNoiseTrackMode(void)
{
    SubbandDenoise_Init();
    return SubbandDenoise_State.noise_track_mode;
}

void SubbandDenoise_ResetNoiseTracker(void)
{
    SubbandDenoise_Init();
    Reset_Noise_Tracker_Internal();
    Update_Debug_State();
}

void SubbandDenoise_SetMcraParams(float delta_low,
                                  float delta_high,
                                  float alpha_noise,
                                  float alpha_speech,
                                  float overdrive_speech,
                                  float overdrive_noise,
                                  float bias,
                                  int strong_mode)
{
    SubbandDenoise_Init();

    SubbandDenoise_State.mcra_delta_low =
        Clamp_Float(delta_low, 1.0f, 8.0f);
    SubbandDenoise_State.mcra_delta_high =
        Clamp_Float(delta_high, 1.01f, 16.0f);
    if (SubbandDenoise_State.mcra_delta_high <=
        SubbandDenoise_State.mcra_delta_low + 0.01f)
    {
        SubbandDenoise_State.mcra_delta_high =
            SubbandDenoise_State.mcra_delta_low + 0.01f;
    }

    SubbandDenoise_State.mcra_alpha_noise =
        Clamp_Float(alpha_noise, 0.0f, 0.9999f);
    SubbandDenoise_State.mcra_alpha_speech =
        Clamp_Float(alpha_speech, 0.0f, 0.9999f);
    SubbandDenoise_State.mcra_overdrive_speech =
        Clamp_Float(overdrive_speech, 1.0f, 4.0f);
    SubbandDenoise_State.mcra_overdrive_noise =
        Clamp_Float(overdrive_noise, 1.0f, 4.0f);
    SubbandDenoise_State.mcra_bias = Clamp_Float(bias, 0.50f, 5.0f);
    SubbandDenoise_State.mcra_strong_mode =
        (strong_mode != 0) ? 1 : 0;

    Reset_Mcra_State_Internal();
    Update_Debug_State();
}

void SubbandDenoise_SetMcraTonalGuardParams(float snr_min,
                                            float neighbor_ratio,
                                            float tonal_floor)
{
    SubbandDenoise_Init();

    SubbandDenoise_State.mcra_tonal_snr_min =
        Clamp_Float(snr_min, 0.0f, 1000.0f);
    SubbandDenoise_State.mcra_tonal_neighbor_ratio =
        Clamp_Float(neighbor_ratio, 0.0f, 1000.0f);
    SubbandDenoise_State.mcra_tonal_floor =
        Clamp_Float(tonal_floor, 0.0f, 1.0f);
}

void SubbandDenoise_ResetMcraState(void)
{
    SubbandDenoise_Init();
    Reset_Mcra_State_Internal();
    Update_Debug_State();
}

void SubbandDenoise_ProcessSpectrum(float *re, float *im)
{
    int k;
    float input_sum;
    float output_sum;
    float gain_sum;
    float min_gain;
    float max_gain;
    float fade;

    if ((re == 0) || (im == 0))
    {
        return;
    }

    SUBBAND_DENOISE_DebugMcraTonalGuardBinsLastFrame = 0UL;
    SubbandDenoise_Init();

    input_sum = 0.0f;
    for (k = 0; k < SUBBAND_POS_BINS; k++)
    {
        float power;

        power = re[k] * re[k] + im[k] * im[k];
        input_sum += power;
        if (SubbandDenoise_State.learning != 0)
        {
            SubbandDenoise_State.noise_accum[k] += power;
        }
    }
    SUBBAND_DENOISE_DebugInputPowerAvg =
        input_sum / (float)SUBBAND_POS_BINS;

    if (SubbandDenoise_State.learning != 0)
    {
        SubbandDenoise_State.learn_count++;
        if (SubbandDenoise_State.learn_count >=
            SubbandDenoise_State.target_hops)
        {
            Finalize_Learning();
        }
        else
        {
            Update_Debug_State();
        }
        SUBBAND_DENOISE_DebugOutputPowerAvg =
            SUBBAND_DENOISE_DebugInputPowerAvg;
        SUBBAND_DENOISE_DebugGainAvg = 1.0f;
        SUBBAND_DENOISE_DebugMinGain = 1.0f;
        SUBBAND_DENOISE_DebugMaxGain = 1.0f;
        Update_Debug_Integer_Mirrors();
        im[0] = 0.0f;
        im[SUBBAND_NFFT / 2] = 0.0f;
        return;
    }

    if ((SubbandDenoise_State.enabled == 0) ||
        (SubbandDenoise_State.ready == 0))
    {
        SUBBAND_DENOISE_DebugOutputPowerAvg =
            SUBBAND_DENOISE_DebugInputPowerAvg;
        SUBBAND_DENOISE_DebugGainAvg = 1.0f;
        SUBBAND_DENOISE_DebugMinGain = 1.0f;
        SUBBAND_DENOISE_DebugMaxGain = 1.0f;
        Update_Debug_State();
        Update_Debug_Integer_Mirrors();
        im[0] = 0.0f;
        im[SUBBAND_NFFT / 2] = 0.0f;
        return;
    }

    if (SubbandDenoise_State.noise_track_mode ==
        SUBBAND_DENOISE_TRACK_MCRA)
    {
        Update_Minimum_Statistics(re, im);
    }

    for (k = 0; k < SUBBAND_POS_BINS; k++)
    {
        float power;
        float noise;
        float post_snr;
        float inst_prior;
        float prior_snr;
        float gain;
        float smooth;

        power = re[k] * re[k] + im[k] * im[k];
        noise = SubbandDenoise_State.noise_psd[k];
        if (SubbandDenoise_State.noise_track_mode ==
            SUBBAND_DENOISE_TRACK_MCRA)
        {
            float effective_overdrive;
            float learned_noise;

            effective_overdrive = 1.0f;
            learned_noise = SubbandDenoise_State.ms_learned_psd[k];
            if (learned_noise < SUBBAND_DENOISE_EPS)
            {
                learned_noise = SUBBAND_DENOISE_EPS;
            }
            if (SubbandDenoise_State.mcra_speech_prob[k] < 0.80f)
            {
                effective_overdrive =
                    SubbandDenoise_State.mcra_overdrive[k];
            }
            noise *= effective_overdrive;
        }
        noise += SUBBAND_DENOISE_EPS;
        post_snr = power / noise;
        inst_prior = post_snr - 1.0f;
        if (inst_prior < 0.0f)
        {
            inst_prior = 0.0f;
        }
        prior_snr =
            SubbandDenoise_State.alpha *
            (SubbandDenoise_State.prev_clean_psd[k] / noise) +
            (1.0f - SubbandDenoise_State.alpha) * inst_prior;
        if (prior_snr < 0.0f)
        {
            prior_snr = 0.0f;
        }

        gain = prior_snr / (1.0f + prior_snr);
        if (SubbandDenoise_State.noise_track_mode ==
            SUBBAND_DENOISE_TRACK_MCRA)
        {
            gain = Clamp_Float(gain,
                               Mcra_Effective_Floor_For_Bin(k),
                               1.0f);
        }
        else
        {
            gain = Clamp_Float(gain,
                               SubbandDenoise_State.gain_floor,
                               1.0f);
        }
        if (gain > SubbandDenoise_State.gain_smooth[k])
        {
            smooth = SubbandDenoise_State.gain_smooth_up;
        }
        else
        {
            smooth = SubbandDenoise_State.gain_smooth_down;
        }
        SubbandDenoise_State.gain_smooth[k] =
            smooth * SubbandDenoise_State.gain_smooth[k] +
            (1.0f - smooth) * gain;
    }

#if SUBBAND_DENOISE_FREQ_SMOOTH
    SubbandDenoise_State.gain_tmp[0] =
        SubbandDenoise_State.gain_smooth[0];
    SubbandDenoise_State.gain_tmp[SUBBAND_NFFT / 2] =
        SubbandDenoise_State.gain_smooth[SUBBAND_NFFT / 2];
    for (k = 1; k < SUBBAND_NFFT / 2; k++)
    {
        SubbandDenoise_State.gain_tmp[k] =
            0.25f * SubbandDenoise_State.gain_smooth[k - 1] +
            0.50f * SubbandDenoise_State.gain_smooth[k] +
            0.25f * SubbandDenoise_State.gain_smooth[k + 1];
    }
#else
    for (k = 0; k < SUBBAND_POS_BINS; k++)
    {
        SubbandDenoise_State.gain_tmp[k] =
            SubbandDenoise_State.gain_smooth[k];
    }
#endif

    if ((SubbandDenoise_State.noise_track_mode !=
         SUBBAND_DENOISE_TRACK_FIXED) &&
        (SubbandDenoise_State.noise_track_mode !=
         SUBBAND_DENOISE_TRACK_MCRA))
    {
        Update_Minimum_Statistics(re, im);
    }

    if (SubbandDenoise_State.fade_total_hops > 0UL)
    {
        fade = (float)SubbandDenoise_State.fade_count /
               (float)SubbandDenoise_State.fade_total_hops;
        if (fade > 1.0f)
        {
            fade = 1.0f;
        }
        if (SubbandDenoise_State.fade_count <
            SubbandDenoise_State.fade_total_hops)
        {
            SubbandDenoise_State.fade_count++;
        }
    }
    else
    {
        fade = 1.0f;
    }

    output_sum = 0.0f;
    gain_sum = 0.0f;
    min_gain = 1.0f;
    max_gain = 0.0f;

    for (k = 0; k < SUBBAND_POS_BINS; k++)
    {
        float power;
        float final_gain;
        float applied_gain;

        power = re[k] * re[k] + im[k] * im[k];
        final_gain = Clamp_Float(SubbandDenoise_State.gain_tmp[k],
                                 SubbandDenoise_State.gain_floor, 1.0f);
        if (SubbandDenoise_State.noise_track_mode ==
            SUBBAND_DENOISE_TRACK_MCRA)
        {
            final_gain = Clamp_Float(SubbandDenoise_State.gain_tmp[k],
                                     Mcra_Effective_Floor_For_Bin(k),
                                     1.0f);
        }
        applied_gain = 1.0f - fade + fade * final_gain;
        if (SubbandDenoise_State.noise_track_mode ==
            SUBBAND_DENOISE_TRACK_MCRA)
        {
            applied_gain = Clamp_Float(applied_gain,
                                       Mcra_Effective_Floor_For_Bin(k),
                                       1.0f);
        }
        else
        {
            applied_gain = Clamp_Float(applied_gain,
                                       SubbandDenoise_State.gain_floor,
                                       1.0f);
        }
        re[k] *= applied_gain;
        im[k] *= applied_gain;
        SubbandDenoise_State.prev_clean_psd[k] =
            applied_gain * applied_gain * power;

        output_sum += SubbandDenoise_State.prev_clean_psd[k];
        gain_sum += applied_gain;
        if (applied_gain < min_gain)
        {
            min_gain = applied_gain;
        }
        if (applied_gain > max_gain)
        {
            max_gain = applied_gain;
        }
    }

    im[0] = 0.0f;
    im[SUBBAND_NFFT / 2] = 0.0f;

    SUBBAND_DENOISE_DebugOutputPowerAvg =
        output_sum / (float)SUBBAND_POS_BINS;
    SUBBAND_DENOISE_DebugGainAvg = gain_sum / (float)SUBBAND_POS_BINS;
    SUBBAND_DENOISE_DebugMinGain = min_gain;
    SUBBAND_DENOISE_DebugMaxGain = max_gain;
    Update_Debug_State();
    Update_Debug_Integer_Mirrors();
}

unsigned long SubbandDenoise_GetLearnCount(void)
{
    SubbandDenoise_Init();
    return SubbandDenoise_State.learn_count;
}

unsigned long SubbandDenoise_GetTargetHops(void)
{
    SubbandDenoise_Init();
    return SubbandDenoise_State.target_hops;
}

float SubbandDenoise_GetNoisePsdAvg(void)
{
    SubbandDenoise_Init();
    return SUBBAND_DENOISE_DebugNoisePsdAvg;
}

float SubbandDenoise_GetGainAvg(void)
{
    SubbandDenoise_Init();
    return SUBBAND_DENOISE_DebugGainAvg;
}

float SubbandDenoise_GetGainMin(void)
{
    SubbandDenoise_Init();
    return SUBBAND_DENOISE_DebugMinGain;
}

float SubbandDenoise_GetGainMax(void)
{
    SubbandDenoise_Init();
    return SUBBAND_DENOISE_DebugMaxGain;
}

float SubbandDenoise_GetFade(void)
{
    float fade;

    if (SubbandDenoise_State.fade_total_hops > 0UL)
    {
        fade = (float)SubbandDenoise_State.fade_count /
               (float)SubbandDenoise_State.fade_total_hops;
        if (fade > 1.0f)
        {
            fade = 1.0f;
        }
        return fade;
    }

    return 1.0f;
}
