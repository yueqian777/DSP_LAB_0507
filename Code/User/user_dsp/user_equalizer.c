/**
 * user_equalizer.c
 *
 * Project 3.3 graphic equalizer using normalized second-order IIR
 * band-pass sections.
 */

#include "user_equalizer.h"
#include "math.h"
#include "string.h"

#define EQ_PI 3.14159265358979323846f
#define EQ_OCTAVE 1.0f
#define EQ_Q_FACTOR 3.0f
#define EQ_EPS 1.0e-20f

typedef struct
{
    float b0;
    float b1;
    float b2;
    float a1;
    float a2;
} EQ_COEFF;

static const float EQ_BandCenterHz[EQ_NUM_BANDS] =
{
    31.25f, 62.5f, 125.0f, 250.0f, 500.0f,
    1000.0f, 2000.0f, 4000.0f, 8000.0f, 16000.0f
};

static const float EQ_PresetGainsDb[5][EQ_NUM_BANDS] =
{
    { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
      0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
    { 6.0f, 6.0f, 5.0f, 3.0f, 1.0f,
      0.0f, 0.0f, 1.0f, 2.0f, 2.0f },
    { -4.0f, -3.0f, -2.0f, 2.0f, 4.0f,
      4.0f, 3.0f, 2.0f, -1.0f, -3.0f },
    { -3.0f, -3.0f, -2.0f, -1.0f, 0.0f,
      0.0f, 2.0f, 4.0f, 6.0f, 6.0f },
    { 5.0f, 5.0f, 3.0f, 0.0f, -3.0f,
      -3.0f, -1.0f, 2.0f, 5.0f, 6.0f }
};

static EQ_COEFF EQ_Coeff[EQ_NUM_BANDS];
static int EQ_CoeffReady = 0;

volatile unsigned long EQ_DebugClipCount = 0UL;

static float EQ_Abs(float x)
{
    return (x < 0.0f) ? -x : x;
}

static float EQ_ClampDb(float gain_db)
{
    if (gain_db < EQ_GAIN_MIN_DB)
    {
        return EQ_GAIN_MIN_DB;
    }
    if (gain_db > EQ_GAIN_MAX_DB)
    {
        return EQ_GAIN_MAX_DB;
    }
    return gain_db;
}

static float EQ_DbToGain(float gain_db)
{
    return powf(10.0f, gain_db / 20.0f);
}

static float EQ_GainToDb(float gain)
{
    if (gain < EQ_EPS)
    {
        gain = EQ_EPS;
    }
    return 20.0f * log10f(gain);
}

static int EQ_FadeSamples(void)
{
    int samples;

    samples = (int)(EQ_SAMPLE_RATE * EQ_FADE_SECONDS + 0.5f);
    if (samples < 1)
    {
        samples = 1;
    }
    return samples;
}

static void EQ_DesignCoeffs(void)
{
    int band;

    if (EQ_CoeffReady != 0)
    {
        return;
    }

    (void)EQ_OCTAVE;
    for (band = 0; band < EQ_NUM_BANDS; band++)
    {
        float w;
        float v2;
        float v3;
        float a0;

        w = (2.0f * EQ_PI * EQ_BandCenterHz[band]) / EQ_SAMPLE_RATE;
        v2 = (2.0f * w) / EQ_Q_FACTOR;
        v3 = w * w;
        a0 = 4.0f + v2 + v3;

        EQ_Coeff[band].b0 = v2 / a0;
        EQ_Coeff[band].b1 = 0.0f;
        EQ_Coeff[band].b2 = -v2 / a0;
        EQ_Coeff[band].a1 = (2.0f * v3 - 8.0f) / a0;
        EQ_Coeff[band].a2 = (4.0f - v2 + v3) / a0;
    }

    EQ_CoeffReady = 1;
}

static void EQ_ResetGains(EQ_STATE *st)
{
    int band;

    for (band = 0; band < EQ_NUM_BANDS; band++)
    {
        st->current_gain[band] = 1.0f;
        st->target_gain[band] = 1.0f;
        st->gain_step[band] = 0.0f;
    }
}

static float EQ_NextGain(EQ_STATE *st, int band)
{
    float current;
    float target;
    float step;

    current = st->current_gain[band];
    target = st->target_gain[band];
    step = st->gain_step[band];

    if (step > 0.0f)
    {
        current += step;
        if (current >= target)
        {
            current = target;
            step = 0.0f;
        }
    }
    else if (step < 0.0f)
    {
        current += step;
        if (current <= target)
        {
            current = target;
            step = 0.0f;
        }
    }
    else
    {
        current = target;
    }

    st->current_gain[band] = current;
    st->gain_step[band] = step;
    return current;
}

static float EQ_ProcessSample(EQ_STATE *st, float x)
{
    int band;
    float acc;

    acc = x;
    for (band = 0; band < EQ_NUM_BANDS; band++)
    {
        const EQ_COEFF *c;
        float y;
        float gain;

        c = &EQ_Coeff[band];
        y = c->b0 * x +
            c->b1 * st->x1[band] +
            c->b2 * st->x2[band] -
            c->a1 * st->y1[band] -
            c->a2 * st->y2[band];

        st->x2[band] = st->x1[band];
        st->x1[band] = x;
        st->y2[band] = st->y1[band];
        st->y1[band] = y;

        gain = EQ_NextGain(st, band);
        acc += (gain - 1.0f) * y;
    }

    return acc;
}

static short EQ_SaturateToShort(EQ_STATE *st, float y)
{
    float rounded;

    if (y > 32767.0f)
    {
        st->clip_count++;
        EQ_DebugClipCount = st->clip_count;
        return 32767;
    }
    if (y < -32768.0f)
    {
        st->clip_count++;
        EQ_DebugClipCount = st->clip_count;
        return -32768;
    }

    if (y >= 0.0f)
    {
        rounded = y + 0.5f;
    }
    else
    {
        rounded = y - 0.5f;
    }
    return (short)rounded;
}

void Equalizer_Init(EQ_STATE *st)
{
    if (st == 0)
    {
        return;
    }

    EQ_DesignCoeffs();
    Equalizer_Reset(st);
}

void Equalizer_Reset(EQ_STATE *st)
{
    if (st == 0)
    {
        return;
    }

    memset(st->x1, 0, sizeof(st->x1));
    memset(st->x2, 0, sizeof(st->x2));
    memset(st->y1, 0, sizeof(st->y1));
    memset(st->y2, 0, sizeof(st->y2));
    EQ_ResetGains(st);
    st->clip_count = 0UL;
    EQ_DebugClipCount = 0UL;
}

void Equalizer_SetBandGainDb(EQ_STATE *st, int band, float gain_db)
{
    float target;
    int fade_samples;

    if ((st == 0) || (band < 0) || (band >= EQ_NUM_BANDS))
    {
        return;
    }

    gain_db = EQ_ClampDb(gain_db);
    target = EQ_DbToGain(gain_db);
    st->target_gain[band] = target;
    fade_samples = EQ_FadeSamples();

    if (fade_samples <= 1)
    {
        st->current_gain[band] = target;
        st->gain_step[band] = 0.0f;
    }
    else
    {
        st->gain_step[band] =
            (target - st->current_gain[band]) / (float)fade_samples;
        if (EQ_Abs(st->gain_step[band]) < 1.0e-12f)
        {
            st->current_gain[band] = target;
            st->gain_step[band] = 0.0f;
        }
    }
}

void Equalizer_SetAllGainsDb(EQ_STATE *st,
                             const float gains_db[EQ_NUM_BANDS])
{
    int band;

    if ((st == 0) || (gains_db == 0))
    {
        return;
    }

    for (band = 0; band < EQ_NUM_BANDS; band++)
    {
        Equalizer_SetBandGainDb(st, band, gains_db[band]);
    }
}

void Equalizer_ApplyPreset(EQ_STATE *st, int preset)
{
    if (st == 0)
    {
        return;
    }

    if ((preset < EQ_PRESET_FLAT) || (preset > EQ_PRESET_V_SHAPE))
    {
        preset = EQ_PRESET_FLAT;
    }
    Equalizer_SetAllGainsDb(st, EQ_PresetGainsDb[preset]);
}

void Equalizer_ProcessFrame(EQ_STATE *st, const short *in, short *out, int n)
{
    int i;

    if ((st == 0) || (in == 0) || (out == 0) || (n <= 0))
    {
        return;
    }

    EQ_DesignCoeffs();
    for (i = 0; i < n; i++)
    {
        float y;

        y = EQ_ProcessSample(st, (float)in[i]);
        out[i] = EQ_SaturateToShort(st, y);
    }
}

void Equalizer_ProcessFrameFloat(EQ_STATE *st, const float *in,
                                 float *out, int n)
{
    int i;

    if ((st == 0) || (in == 0) || (out == 0) || (n <= 0))
    {
        return;
    }

    EQ_DesignCoeffs();
    for (i = 0; i < n; i++)
    {
        out[i] = EQ_ProcessSample(st, in[i]);
    }
}

float Equalizer_GetBandCenterHz(int band)
{
    if ((band < 0) || (band >= EQ_NUM_BANDS))
    {
        return 0.0f;
    }
    return EQ_BandCenterHz[band];
}

float Equalizer_GetBandTargetGainDb(const EQ_STATE *st, int band)
{
    if ((st == 0) || (band < 0) || (band >= EQ_NUM_BANDS))
    {
        return 0.0f;
    }
    return EQ_GainToDb(st->target_gain[band]);
}

float Equalizer_GetBandMagnitudeDb(int band, float freq_hz)
{
    const EQ_COEFF *c;
    float w;
    float z1r;
    float z1i;
    float z2r;
    float z2i;
    float nr;
    float ni;
    float dr;
    float di;
    float num;
    float den;
    float mag;

    if ((band < 0) || (band >= EQ_NUM_BANDS) ||
        (freq_hz <= 0.0f) || (freq_hz >= (EQ_SAMPLE_RATE * 0.5f)))
    {
        return -120.0f;
    }

    EQ_DesignCoeffs();
    c = &EQ_Coeff[band];
    w = (2.0f * EQ_PI * freq_hz) / EQ_SAMPLE_RATE;
    z1r = cosf(w);
    z1i = -sinf(w);
    z2r = cosf(2.0f * w);
    z2i = -sinf(2.0f * w);

    nr = c->b0 + c->b1 * z1r + c->b2 * z2r;
    ni = c->b1 * z1i + c->b2 * z2i;
    dr = 1.0f + c->a1 * z1r + c->a2 * z2r;
    di = c->a1 * z1i + c->a2 * z2i;
    num = nr * nr + ni * ni;
    den = dr * dr + di * di;
    mag = sqrtf(num / (den + EQ_EPS));
    if (mag < EQ_EPS)
    {
        mag = EQ_EPS;
    }
    return 20.0f * log10f(mag);
}
