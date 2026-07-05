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
    unsigned long learn_count;
    unsigned long target_hops;
    unsigned long fade_count;
    unsigned long fade_total_hops;
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

static void Update_Debug_State(void)
{
    float progress;

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
    SubbandDenoise_State.learning = 1;
    SubbandDenoise_State.ready = 0;
    SubbandDenoise_State.enabled = 0;
    SUBBAND_DENOISE_DebugInputPowerAvg = 0.0f;
    SUBBAND_DENOISE_DebugOutputPowerAvg = 0.0f;
    SUBBAND_DENOISE_DebugGainAvg = 1.0f;
    SUBBAND_DENOISE_DebugMinGain = 1.0f;
    SUBBAND_DENOISE_DebugMaxGain = 1.0f;
    SUBBAND_DENOISE_DebugNoisePsdAvg = 0.0f;
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
#if SUBBAND_DENOISE_AUTO_ENABLE_AFTER_LEARN
    SubbandDenoise_State.enabled = 1;
#endif
    SubbandDenoise_State.fade_count = 0UL;
    SUBBAND_DENOISE_DebugNoisePsdAvg =
        psd_sum / (float)SUBBAND_POS_BINS;
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
    SubbandDenoise_State.initialized = 0;
    SubbandDenoise_Init();
}

void SubbandDenoise_SetEnabled(int enable)
{
    SubbandDenoise_Init();

    if (enable != 0)
    {
        SubbandDenoise_State.enabled = 1;
    }
    else
    {
        SubbandDenoise_State.enabled = 0;
        SubbandDenoise_State.learning = 0;
    }
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
        im[0] = 0.0f;
        im[SUBBAND_NFFT / 2] = 0.0f;
        return;
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
        noise = SubbandDenoise_State.noise_psd[k] + SUBBAND_DENOISE_EPS;
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
        gain = Clamp_Float(gain, SubbandDenoise_State.gain_floor, 1.0f);
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
        applied_gain = 1.0f - fade + fade * final_gain;
        applied_gain = Clamp_Float(applied_gain,
                                   SubbandDenoise_State.gain_floor, 1.0f);
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
