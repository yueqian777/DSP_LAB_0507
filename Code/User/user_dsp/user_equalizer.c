/**
 * user_equalizer.c
 *
 * Project 3.3 graphic equalizer with a retained legacy parallel-bank path
 * and an RBJ cascade path used for offline quality validation.
 */

#include "user_equalizer.h"
#include "math.h"
#include "string.h"

#define EQ_PI 3.14159265358979323846f
#define EQ_LN2 0.69314718055994530942f
#define EQ_EPS 1.0e-20f
#define EQ_Q_FACTOR 3.0f
#define EQ_RBJ_BANDWIDTH_OCTAVES 1.0f
#define EQ_RBJ_SHELF_SLOPE 1.0f
#define EQ_HEADROOM_POINTS 512
#define EQ_RBJ_BANK_SINGLE_1K EQ_PRESET_COUNT
#define EQ_RBJ_BANK_CACHE_COUNT (EQ_PRESET_COUNT + 1)
#define EQ_RBJ_BANK_CUSTOM (-1)

typedef struct
{
    double real;
    double imag;
} EQ_COMPLEX;

static const float EQ_BandCenterHz[EQ_NUM_BANDS] =
{
    31.25f, 62.5f, 125.0f, 250.0f, 500.0f,
    1000.0f, 2000.0f, 4000.0f, 8000.0f, 16000.0f
};

static const float EQ_LegacyPresetGainsDb[EQ_PRESET_COUNT][EQ_NUM_BANDS] =
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

static const float EQ_RbjPresetGainsDb[EQ_PRESET_COUNT][EQ_NUM_BANDS] =
{
    { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
      0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
    { 3.0f, 3.0f, 2.0f, 1.0f, 0.0f,
      0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
    { -2.0f, -1.0f, 0.0f, 1.0f, 2.0f,
      3.0f, 2.0f, 1.0f, 0.0f, -1.0f },
    { -1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
      0.0f, 1.0f, 2.0f, 3.0f, 3.0f },
    { 2.0f, 2.0f, 1.0f, 0.0f, -1.0f,
      -1.0f, 0.0f, 1.0f, 2.0f, 2.0f }
};

static EQ_BIQUAD EQ_LegacyCoeff[EQ_NUM_BANDS];
static int EQ_LegacyCoeffReady = 0;
static EQ_FILTER_BANK EQ_RbjBankCache[EQ_RBJ_BANK_CACHE_COUNT];
static int EQ_RbjBankCacheReady = 0;

volatile unsigned long EQ_DebugClipCount = 0UL;
volatile float EQ_DebugPredictedPeakDb = 0.0f;
volatile float EQ_DebugPreampDb = 0.0f;
volatile float EQ_DebugFloatCopyMaxError = 0.0f;
volatile unsigned long EQ_DebugHeadroomScanCount = 0UL;

static float EQ_Abs(float x)
{
    return (x < 0.0f) ? -x : x;
}

static int EQ_IsFinite(float x)
{
    return ((x == x) && (x < 3.0e38f) && (x > -3.0e38f));
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

static EQ_COMPLEX EQ_ComplexAdd(EQ_COMPLEX a, EQ_COMPLEX b)
{
    EQ_COMPLEX r;

    r.real = a.real + b.real;
    r.imag = a.imag + b.imag;
    return r;
}

static EQ_COMPLEX EQ_ComplexScale(EQ_COMPLEX a, double gain)
{
    EQ_COMPLEX r;

    r.real = a.real * gain;
    r.imag = a.imag * gain;
    return r;
}

static EQ_COMPLEX EQ_ComplexMultiply(EQ_COMPLEX a, EQ_COMPLEX b)
{
    EQ_COMPLEX r;

    r.real = a.real * b.real - a.imag * b.imag;
    r.imag = a.real * b.imag + a.imag * b.real;
    return r;
}

static float EQ_ComplexMagnitude(EQ_COMPLEX a)
{
    return (float)sqrt(a.real * a.real + a.imag * a.imag);
}

static float EQ_ComplexPhase(EQ_COMPLEX a)
{
    return (float)atan2(a.imag, a.real);
}

static EQ_COMPLEX EQ_BiquadResponse(const EQ_BIQUAD *c, float freq_hz)
{
    EQ_COMPLEX h;
    double w;
    double z1r;
    double z1i;
    double z2r;
    double z2i;
    double nr;
    double ni;
    double dr;
    double di;
    double den;

    h.real = 0.0f;
    h.imag = 0.0f;
    if ((c == 0) || (freq_hz <= 0.0f) ||
        (freq_hz >= (EQ_SAMPLE_RATE * 0.5f)))
    {
        return h;
    }

    w = (2.0 * (double)EQ_PI * (double)freq_hz) /
        (double)EQ_SAMPLE_RATE;
    z1r = cos(w);
    z1i = -sin(w);
    z2r = cos(2.0 * w);
    z2i = -sin(2.0 * w);
    nr = c->b0 + c->b1 * z1r + c->b2 * z2r;
    ni = c->b1 * z1i + c->b2 * z2i;
    dr = 1.0f + c->a1 * z1r + c->a2 * z2r;
    di = c->a1 * z1i + c->a2 * z2i;
    den = dr * dr + di * di;
    if (den < (double)EQ_EPS)
    {
        return h;
    }

    h.real = (nr * dr + ni * di) / den;
    h.imag = (ni * dr - nr * di) / den;
    return h;
}

static void EQ_DesignLegacyCoeffs(void)
{
    int band;

    if (EQ_LegacyCoeffReady != 0)
    {
        return;
    }

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
        EQ_LegacyCoeff[band].b0 = v2 / a0;
        EQ_LegacyCoeff[band].b1 = 0.0f;
        EQ_LegacyCoeff[band].b2 = -v2 / a0;
        EQ_LegacyCoeff[band].a1 = (2.0f * v3 - 8.0f) / a0;
        EQ_LegacyCoeff[band].a2 = (4.0f - v2 + v3) / a0;
    }

    EQ_LegacyCoeffReady = 1;
}

static void EQ_SetIdentity(EQ_BIQUAD *c)
{
    c->b0 = 1.0f;
    c->b1 = 0.0f;
    c->b2 = 0.0f;
    c->a1 = 0.0f;
    c->a2 = 0.0f;
}

static int EQ_BiquadIsFinite(const EQ_BIQUAD *c)
{
    return ((EQ_IsFinite(c->b0) != 0) && (EQ_IsFinite(c->b1) != 0) &&
            (EQ_IsFinite(c->b2) != 0) && (EQ_IsFinite(c->a1) != 0) &&
            (EQ_IsFinite(c->a2) != 0));
}

static void EQ_BiquadPoleRadii(const EQ_BIQUAD *c, float *r1, float *r2)
{
    float discriminant;

    if ((c == 0) || (r1 == 0) || (r2 == 0))
    {
        return;
    }

    discriminant = c->a1 * c->a1 - 4.0f * c->a2;
    if (discriminant >= 0.0f)
    {
        float root = sqrtf(discriminant);

        *r1 = EQ_Abs((-c->a1 + root) * 0.5f);
        *r2 = EQ_Abs((-c->a1 - root) * 0.5f);
    }
    else if (c->a2 >= 0.0f)
    {
        *r1 = sqrtf(c->a2);
        *r2 = *r1;
    }
    else
    {
        *r1 = 2.0f;
        *r2 = 2.0f;
    }
}

static int EQ_BiquadIsStable(const EQ_BIQUAD *c)
{
    float r1;
    float r2;

    if (EQ_BiquadIsFinite(c) == 0)
    {
        return 0;
    }
    EQ_BiquadPoleRadii(c, &r1, &r2);
    return ((r1 < 1.0f) && (r2 < 1.0f));
}

static void EQ_NormalizeBiquad(EQ_BIQUAD *c, float a0)
{
    if ((EQ_IsFinite(a0) == 0) || (EQ_Abs(a0) < EQ_EPS))
    {
        EQ_SetIdentity(c);
        return;
    }

    c->b0 /= a0;
    c->b1 /= a0;
    c->b2 /= a0;
    c->a1 /= a0;
    c->a2 /= a0;
    if ((EQ_BiquadIsFinite(c) == 0) || (EQ_BiquadIsStable(c) == 0))
    {
        EQ_SetIdentity(c);
    }
}

static void EQ_DesignRbjPeaking(EQ_BIQUAD *c, float f0_hz, float gain_db)
{
    float a;
    float w0;
    float sin_w0;
    float cos_w0;
    float alpha;
    float a0;

    if (EQ_Abs(gain_db) < 1.0e-7f)
    {
        EQ_SetIdentity(c);
        return;
    }

    a = powf(10.0f, gain_db / 40.0f);
    w0 = (2.0f * EQ_PI * f0_hz) / EQ_SAMPLE_RATE;
    sin_w0 = sinf(w0);
    cos_w0 = cosf(w0);
    if (EQ_Abs(sin_w0) < EQ_EPS)
    {
        EQ_SetIdentity(c);
        return;
    }
    alpha = sin_w0 * sinhf((EQ_LN2 * 0.5f) *
                           EQ_RBJ_BANDWIDTH_OCTAVES * w0 / sin_w0);
    c->b0 = 1.0f + alpha * a;
    c->b1 = -2.0f * cos_w0;
    c->b2 = 1.0f - alpha * a;
    a0 = 1.0f + alpha / a;
    c->a1 = -2.0f * cos_w0;
    c->a2 = 1.0f - alpha / a;
    EQ_NormalizeBiquad(c, a0);
}

static void EQ_DesignRbjShelf(EQ_BIQUAD *c, float f0_hz, float gain_db,
                              int high_shelf)
{
    float a;
    float w0;
    float sin_w0;
    float cos_w0;
    float alpha;
    float two_sqrt_a_alpha;
    float a0;

    if (EQ_Abs(gain_db) < 1.0e-7f)
    {
        EQ_SetIdentity(c);
        return;
    }

    a = powf(10.0f, gain_db / 40.0f);
    w0 = (2.0f * EQ_PI * f0_hz) / EQ_SAMPLE_RATE;
    sin_w0 = sinf(w0);
    cos_w0 = cosf(w0);
    alpha = (sin_w0 * 0.5f) *
            sqrtf((a + 1.0f / a) * (1.0f / EQ_RBJ_SHELF_SLOPE - 1.0f) +
                  2.0f);
    two_sqrt_a_alpha = 2.0f * sqrtf(a) * alpha;

    if (high_shelf == 0)
    {
        c->b0 = a * ((a + 1.0f) - (a - 1.0f) * cos_w0 + two_sqrt_a_alpha);
        c->b1 = 2.0f * a * ((a - 1.0f) - (a + 1.0f) * cos_w0);
        c->b2 = a * ((a + 1.0f) - (a - 1.0f) * cos_w0 - two_sqrt_a_alpha);
        a0 = (a + 1.0f) + (a - 1.0f) * cos_w0 + two_sqrt_a_alpha;
        c->a1 = -2.0f * ((a - 1.0f) + (a + 1.0f) * cos_w0);
        c->a2 = (a + 1.0f) + (a - 1.0f) * cos_w0 - two_sqrt_a_alpha;
    }
    else
    {
        c->b0 = a * ((a + 1.0f) + (a - 1.0f) * cos_w0 + two_sqrt_a_alpha);
        c->b1 = -2.0f * a * ((a - 1.0f) + (a + 1.0f) * cos_w0);
        c->b2 = a * ((a + 1.0f) + (a - 1.0f) * cos_w0 - two_sqrt_a_alpha);
        a0 = (a + 1.0f) - (a - 1.0f) * cos_w0 + two_sqrt_a_alpha;
        c->a1 = 2.0f * ((a - 1.0f) - (a + 1.0f) * cos_w0);
        c->a2 = (a + 1.0f) - (a - 1.0f) * cos_w0 - two_sqrt_a_alpha;
    }
    EQ_NormalizeBiquad(c, a0);
}

static int EQ_AllGainsFlat(const float gains_db[EQ_NUM_BANDS])
{
    int band;

    for (band = 0; band < EQ_NUM_BANDS; band++)
    {
        if (EQ_Abs(gains_db[band]) > 1.0e-7f)
        {
            return 0;
        }
    }
    return 1;
}

static EQ_COMPLEX EQ_BankResponse(const EQ_FILTER_BANK *bank, float freq_hz,
                                  int include_preamp)
{
    EQ_COMPLEX h;
    int section;

    h.real = 1.0f;
    h.imag = 0.0f;
    if (bank == 0)
    {
        return h;
    }

    for (section = 0; section < EQ_NUM_BANDS; section++)
    {
        h = EQ_ComplexMultiply(h,
                               EQ_BiquadResponse(&bank->section[section],
                                                 freq_hz));
    }
    if (include_preamp != 0)
    {
        h = EQ_ComplexScale(h, (double)bank->preamp_gain);
    }
    return h;
}

static float EQ_BankPeakDb(const EQ_FILTER_BANK *bank)
{
    int index;
    float log_min;
    float log_max;
    float step;
    float peak_db;

    EQ_DebugHeadroomScanCount++;
    log_min = logf(20.0f);
    log_max = logf(20000.0f);
    step = (log_max - log_min) / (float)(EQ_HEADROOM_POINTS - 1);
    peak_db = -120.0f;
    for (index = 0; index < EQ_HEADROOM_POINTS; index++)
    {
        float freq_hz;
        float mag;
        float db;

        freq_hz = expf(log_min + step * (float)index);
        mag = EQ_ComplexMagnitude(EQ_BankResponse(bank, freq_hz, 0));
        if (mag < EQ_EPS)
        {
            mag = EQ_EPS;
        }
        db = 20.0f * log10f(mag);
        if (db > peak_db)
        {
            peak_db = db;
        }
    }
    return peak_db;
}

static void EQ_ClearBankState(EQ_FILTER_BANK *bank)
{
    if (bank == 0)
    {
        return;
    }
    memset(bank->s1, 0, sizeof(bank->s1));
    memset(bank->s2, 0, sizeof(bank->s2));
}

static void EQ_BuildRbjBank(EQ_FILTER_BANK *bank,
                             const float gains_db[EQ_NUM_BANDS]);

static int EQ_GainsMatch(const float a[EQ_NUM_BANDS],
                         const float b[EQ_NUM_BANDS])
{
    int band;

    for (band = 0; band < EQ_NUM_BANDS; band++)
    {
        if (EQ_Abs(a[band] - b[band]) > 1.0e-7f)
        {
            return 0;
        }
    }
    return 1;
}

static void EQ_EnsureRbjBankCache(void)
{
    float single_1k_gains[EQ_NUM_BANDS];
    int preset;
    int band;

    if (EQ_RbjBankCacheReady != 0)
    {
        return;
    }
    for (preset = EQ_PRESET_FLAT; preset < EQ_PRESET_COUNT; preset++)
    {
        EQ_BuildRbjBank(&EQ_RbjBankCache[preset],
                        EQ_RbjPresetGainsDb[preset]);
    }
    for (band = 0; band < EQ_NUM_BANDS; band++)
    {
        single_1k_gains[band] = 0.0f;
    }
    single_1k_gains[5] = 3.0f;
    EQ_BuildRbjBank(&EQ_RbjBankCache[EQ_RBJ_BANK_SINGLE_1K],
                    single_1k_gains);
    EQ_RbjBankCacheReady = 1;
}

static int EQ_FindRbjCachedBank(const float gains_db[EQ_NUM_BANDS])
{
    float single_1k_gains[EQ_NUM_BANDS];
    int preset;
    int band;

    for (preset = EQ_PRESET_FLAT; preset < EQ_PRESET_COUNT; preset++)
    {
        if (EQ_GainsMatch(gains_db, EQ_RbjPresetGainsDb[preset]) != 0)
        {
            return preset;
        }
    }
    for (band = 0; band < EQ_NUM_BANDS; band++)
    {
        single_1k_gains[band] = 0.0f;
    }
    single_1k_gains[5] = 3.0f;
    return (EQ_GainsMatch(gains_db, single_1k_gains) != 0) ?
           EQ_RBJ_BANK_SINGLE_1K : EQ_RBJ_BANK_CUSTOM;
}

static void EQ_BuildRbjBank(EQ_FILTER_BANK *bank,
                            const float gains_db[EQ_NUM_BANDS])
{
    int section;

    memset(bank, 0, sizeof(*bank));
    EQ_DesignRbjShelf(&bank->section[0], EQ_BandCenterHz[0], gains_db[0], 0);
    for (section = 1; section < (EQ_NUM_BANDS - 1); section++)
    {
        EQ_DesignRbjPeaking(&bank->section[section], EQ_BandCenterHz[section],
                            gains_db[section]);
    }
    EQ_DesignRbjShelf(&bank->section[EQ_NUM_BANDS - 1],
                      EQ_BandCenterHz[EQ_NUM_BANDS - 1],
                      gains_db[EQ_NUM_BANDS - 1], 1);

    if (EQ_AllGainsFlat(gains_db) != 0)
    {
        bank->predicted_peak_db = 0.0f;
        bank->preamp_db = 0.0f;
    }
    else
    {
        bank->predicted_peak_db = EQ_BankPeakDb(bank);
        if (bank->predicted_peak_db > 0.0f)
        {
            bank->preamp_db = -bank->predicted_peak_db - 0.5f;
        }
        else
        {
            bank->preamp_db = -0.5f;
        }
    }
    bank->preamp_gain = EQ_DbToGain(bank->preamp_db);
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

static int EQ_TransitionSamples(void)
{
    int samples;

    samples = (int)((EQ_SAMPLE_RATE * EQ_TRANSITION_MS) / 1000.0f + 0.5f);
    if (samples < 1)
    {
        samples = 1;
    }
    return samples;
}

static void EQ_ResetLegacyState(EQ_STATE *st)
{
    memset(st->legacy_x1, 0, sizeof(st->legacy_x1));
    memset(st->legacy_x2, 0, sizeof(st->legacy_x2));
    memset(st->legacy_y1, 0, sizeof(st->legacy_y1));
    memset(st->legacy_y2, 0, sizeof(st->legacy_y2));
}

static void EQ_SetLegacyImmediate(EQ_STATE *st)
{
    int band;

    for (band = 0; band < EQ_NUM_BANDS; band++)
    {
        float gain = EQ_DbToGain(st->band_gain_db[band]);

        st->current_gain[band] = gain;
        st->target_gain[band] = gain;
        st->gain_step[band] = 0.0f;
    }
}

static void EQ_SetLegacyTarget(EQ_STATE *st, int band, int immediate)
{
    float target;
    int fade_samples;

    target = EQ_DbToGain(st->band_gain_db[band]);
    st->target_gain[band] = target;
    if (immediate != 0)
    {
        st->current_gain[band] = target;
        st->gain_step[band] = 0.0f;
        return;
    }

    fade_samples = EQ_FadeSamples();
    st->gain_step[band] = (target - st->current_gain[band]) /
                          (float)fade_samples;
    if (EQ_Abs(st->gain_step[band]) < 1.0e-12f)
    {
        st->current_gain[band] = target;
        st->gain_step[band] = 0.0f;
    }
}

static float EQ_NextLegacyGain(EQ_STATE *st, int band)
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

static void EQ_UpdateDebugHeadroom(const EQ_STATE *st)
{
    const EQ_FILTER_BANK *bank;

    if ((st == 0) || (st->core_mode != EQ_CORE_RBJ_CASCADE))
    {
        EQ_DebugPredictedPeakDb = 0.0f;
        EQ_DebugPreampDb = 0.0f;
        return;
    }
    bank = (st->pending_bank_valid != 0) ? &st->pending_bank :
                                            &st->active_bank;
    EQ_DebugPredictedPeakDb = bank->predicted_peak_db;
    EQ_DebugPreampDb = bank->preamp_db;
}

static void EQ_InstallRbjCandidate(EQ_STATE *st, const EQ_FILTER_BANK *candidate,
                                   int immediate)
{
    if ((immediate != 0) || (EQ_ENABLE_BANK_CROSSFADE == 0))
    {
        st->active_bank = *candidate;
        EQ_ClearBankState(&st->active_bank);
        memset(&st->pending_bank, 0, sizeof(st->pending_bank));
        st->pending_bank_valid = 0;
        st->transition_remaining = 0;
        st->transition_total = 0;
    }
    else
    {
        st->pending_bank = *candidate;
        EQ_ClearBankState(&st->pending_bank);
        st->pending_bank_valid = 1;
        st->transition_total = EQ_TransitionSamples();
        st->transition_remaining = st->transition_total;
    }
    EQ_UpdateDebugHeadroom(st);
}

static void EQ_InstallRbjCachedTarget(EQ_STATE *st, int bank_id,
                                      int immediate)
{
    EQ_EnsureRbjBankCache();
    if ((bank_id < EQ_PRESET_FLAT) || (bank_id >= EQ_RBJ_BANK_CACHE_COUNT))
    {
        bank_id = EQ_PRESET_FLAT;
    }
    EQ_InstallRbjCandidate(st, &EQ_RbjBankCache[bank_id], immediate);
    st->rbj_bank_id = bank_id;
}

static void EQ_InstallRbjTarget(EQ_STATE *st, int immediate)
{
    EQ_FILTER_BANK candidate;
    int bank_id;

    bank_id = EQ_FindRbjCachedBank(st->band_gain_db);
    if (bank_id != EQ_RBJ_BANK_CUSTOM)
    {
        EQ_InstallRbjCachedTarget(st, bank_id, immediate);
        return;
    }
    EQ_BuildRbjBank(&candidate, st->band_gain_db);
    EQ_InstallRbjCandidate(st, &candidate, immediate);
    st->rbj_bank_id = EQ_RBJ_BANK_CUSTOM;
}

static float EQ_ProcessLegacySample(EQ_STATE *st, float x)
{
    float acc;
    int band;

    acc = x;
    for (band = 0; band < EQ_NUM_BANDS; band++)
    {
        const EQ_BIQUAD *c;
        float y;
        float gain;

        c = &EQ_LegacyCoeff[band];
        y = c->b0 * x + c->b1 * st->legacy_x1[band] +
            c->b2 * st->legacy_x2[band] - c->a1 * st->legacy_y1[band] -
            c->a2 * st->legacy_y2[band];
        st->legacy_x2[band] = st->legacy_x1[band];
        st->legacy_x1[band] = x;
        st->legacy_y2[band] = st->legacy_y1[band];
        st->legacy_y1[band] = y;
        gain = EQ_NextLegacyGain(st, band);
        acc += (gain - 1.0f) * y;
    }
    return acc;
}

static float EQ_ProcessRbjBank(EQ_FILTER_BANK *bank, float x)
{
    int section;
    float y;

    y = x * bank->preamp_gain;
    for (section = 0; section < EQ_NUM_BANDS; section++)
    {
        const EQ_BIQUAD *c = &bank->section[section];
        float out = c->b0 * y + bank->s1[section];

        bank->s1[section] = c->b1 * y - c->a1 * out +
                            bank->s2[section];
        bank->s2[section] = c->b2 * y - c->a2 * out;
        y = out;
    }
    return y;
}

static float EQ_ProcessRbjSample(EQ_STATE *st, float x)
{
    float active;

    active = EQ_ProcessRbjBank(&st->active_bank, x);
    if (st->pending_bank_valid != 0)
    {
        float pending;
        float mix;

        pending = EQ_ProcessRbjBank(&st->pending_bank, x);
        mix = 1.0f - (float)st->transition_remaining /
                      (float)st->transition_total;
        if (mix < 0.0f)
        {
            mix = 0.0f;
        }
        if (mix > 1.0f)
        {
            mix = 1.0f;
        }
        active += (pending - active) * mix;
        st->transition_remaining--;
        if (st->transition_remaining <= 0)
        {
            st->active_bank = st->pending_bank;
            memset(&st->pending_bank, 0, sizeof(st->pending_bank));
            st->pending_bank_valid = 0;
            st->transition_remaining = 0;
            st->transition_total = 0;
        }
    }
    return active;
}

static int EQ_UsesTransparentPath(const EQ_STATE *st)
{
    if ((st == 0) || (st->pending_bank_valid != 0))
    {
        return 0;
    }
    if (st->bypass != 0)
    {
        return 1;
    }
    if (st->core_mode == EQ_CORE_RAW_COPY)
    {
        return 1;
    }
    return ((st->core_mode == EQ_CORE_RBJ_CASCADE) &&
            (EQ_AllGainsFlat(st->band_gain_db) != 0)) ? 1 : 0;
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
    rounded = (y >= 0.0f) ? (y + 0.5f) : (y - 0.5f);
    return (short)rounded;
}

static short EQ_ProcessFloatCopyShort(EQ_STATE *st, short input)
{
    float normalized = (float)input / 32768.0f;
    float restored = normalized * 32768.0f;
    short output = EQ_SaturateToShort(st, restored);
    float error = EQ_Abs((float)output - (float)input);

    if (error > EQ_DebugFloatCopyMaxError)
    {
        EQ_DebugFloatCopyMaxError = error;
    }
    return output;
}

void Equalizer_Init(EQ_STATE *st)
{
    if (st == 0)
    {
        return;
    }
    memset(st, 0, sizeof(*st));
    EQ_DesignLegacyCoeffs();
    EQ_EnsureRbjBankCache();
    st->core_mode = EQ_CORE_RBJ_CASCADE;
    st->preset = EQ_PRESET_FLAT;
    st->rbj_bank_id = EQ_PRESET_FLAT;
    EQ_SetLegacyImmediate(st);
    EQ_InstallRbjCachedTarget(st, EQ_PRESET_FLAT, 1);
    EQ_DebugClipCount = 0UL;
    EQ_DebugFloatCopyMaxError = 0.0f;
}

void Equalizer_Reset(EQ_STATE *st)
{
    float gains_db[EQ_NUM_BANDS];
    int core_mode;
    int bypass;
    int preset;
    int rbj_bank_id;
    int band;

    if (st == 0)
    {
        return;
    }
    for (band = 0; band < EQ_NUM_BANDS; band++)
    {
        gains_db[band] = st->band_gain_db[band];
    }
    core_mode = st->core_mode;
    bypass = st->bypass;
    preset = st->preset;
    rbj_bank_id = st->rbj_bank_id;
    memset(st, 0, sizeof(*st));
    st->core_mode = core_mode;
    st->bypass = bypass;
    st->preset = preset;
    st->rbj_bank_id = rbj_bank_id;
    for (band = 0; band < EQ_NUM_BANDS; band++)
    {
        st->band_gain_db[band] = gains_db[band];
    }
    EQ_DesignLegacyCoeffs();
    EQ_SetLegacyImmediate(st);
    if ((st->core_mode == EQ_CORE_RBJ_CASCADE) &&
        (st->rbj_bank_id != EQ_RBJ_BANK_CUSTOM))
    {
        EQ_InstallRbjCachedTarget(st, st->rbj_bank_id, 1);
    }
    else
    {
        EQ_InstallRbjTarget(st, 1);
    }
    EQ_DebugClipCount = 0UL;
    EQ_DebugFloatCopyMaxError = 0.0f;
    EQ_UpdateDebugHeadroom(st);
}

void Equalizer_SetBypass(EQ_STATE *st, int enable)
{
    if (st != 0)
    {
        st->bypass = (enable != 0) ? 1 : 0;
    }
}

int Equalizer_GetBypass(const EQ_STATE *st)
{
    return ((st != 0) && (st->bypass != 0)) ? 1 : 0;
}

void Equalizer_SetCoreMode(EQ_STATE *st, int mode)
{
    if (st == 0)
    {
        return;
    }
    if ((mode < EQ_CORE_RAW_COPY) || (mode > EQ_CORE_RBJ_CASCADE))
    {
        mode = EQ_CORE_RBJ_CASCADE;
    }
    if (st->core_mode == mode)
    {
        return;
    }
    st->core_mode = mode;
    EQ_ResetLegacyState(st);
    EQ_SetLegacyImmediate(st);
    if (mode == EQ_CORE_RBJ_CASCADE)
    {
        if (st->rbj_bank_id != EQ_RBJ_BANK_CUSTOM)
        {
            EQ_InstallRbjCachedTarget(st, st->rbj_bank_id, 1);
        }
        else
        {
            EQ_InstallRbjTarget(st, 1);
        }
    }
    EQ_UpdateDebugHeadroom(st);
}

int Equalizer_GetCoreMode(const EQ_STATE *st)
{
    return (st == 0) ? EQ_CORE_RBJ_CASCADE : st->core_mode;
}

void Equalizer_SetBandGainDb(EQ_STATE *st, int band, float gain_db)
{
    float clamped_gain;

    if ((st == 0) || (band < 0) || (band >= EQ_NUM_BANDS))
    {
        return;
    }
    clamped_gain = EQ_ClampDb(gain_db);
    if (EQ_Abs(st->band_gain_db[band] - clamped_gain) < 1.0e-7f)
    {
        return;
    }
    st->band_gain_db[band] = clamped_gain;
    st->preset = EQ_PRESET_CUSTOM;
    st->rbj_bank_id = EQ_RBJ_BANK_CUSTOM;
    if (st->core_mode == EQ_CORE_LEGACY)
    {
        EQ_SetLegacyTarget(st, band, 0);
    }
    else if (st->core_mode == EQ_CORE_RBJ_CASCADE)
    {
        EQ_InstallRbjTarget(st, 0);
    }
}

void Equalizer_SetAllGainsDb(EQ_STATE *st,
                             const float gains_db[EQ_NUM_BANDS])
{
    int band;
    int changed = 0;

    if ((st == 0) || (gains_db == 0))
    {
        return;
    }
    for (band = 0; band < EQ_NUM_BANDS; band++)
    {
        float clamped_gain = EQ_ClampDb(gains_db[band]);

        if (EQ_Abs(st->band_gain_db[band] - clamped_gain) >= 1.0e-7f)
        {
            changed = 1;
        }
        st->band_gain_db[band] = clamped_gain;
    }
    if (changed == 0)
    {
        return;
    }
    st->preset = EQ_PRESET_CUSTOM;
    st->rbj_bank_id = EQ_FindRbjCachedBank(st->band_gain_db);
    if (st->core_mode == EQ_CORE_LEGACY)
    {
        for (band = 0; band < EQ_NUM_BANDS; band++)
        {
            EQ_SetLegacyTarget(st, band, 0);
        }
    }
    else if (st->core_mode == EQ_CORE_RBJ_CASCADE)
    {
        EQ_InstallRbjTarget(st, 0);
    }
}

void Equalizer_ApplyPreset(EQ_STATE *st, int preset)
{
    const float (*preset_bank)[EQ_NUM_BANDS];
    int band;
    int changed = 0;

    if (st == 0)
    {
        return;
    }
    if ((preset < EQ_PRESET_FLAT) || (preset >= EQ_PRESET_COUNT))
    {
        preset = EQ_PRESET_FLAT;
    }
    st->preset = preset;
    preset_bank = (st->core_mode == EQ_CORE_LEGACY) ?
                   EQ_LegacyPresetGainsDb : EQ_RbjPresetGainsDb;
    for (band = 0; band < EQ_NUM_BANDS; band++)
    {
        if (EQ_Abs(st->band_gain_db[band] - preset_bank[preset][band]) >=
            1.0e-7f)
        {
            changed = 1;
        }
        st->band_gain_db[band] = preset_bank[preset][band];
    }
    if (st->core_mode == EQ_CORE_LEGACY)
    {
        for (band = 0; band < EQ_NUM_BANDS; band++)
        {
            EQ_SetLegacyTarget(st, band, 0);
        }
        return;
    }
    st->rbj_bank_id = preset;
    if ((st->core_mode == EQ_CORE_RBJ_CASCADE) && (changed != 0))
    {
        EQ_InstallRbjCachedTarget(st, preset, 0);
    }
}

void Equalizer_ApplySingleBand1kPlus3Db(EQ_STATE *st)
{
    int band;
    int changed = 0;

    if (st == 0)
    {
        return;
    }
    for (band = 0; band < EQ_NUM_BANDS; band++)
    {
        float gain_db = (band == 5) ? 3.0f : 0.0f;

        if (EQ_Abs(st->band_gain_db[band] - gain_db) >= 1.0e-7f)
        {
            changed = 1;
        }
        st->band_gain_db[band] = gain_db;
    }
    st->preset = EQ_PRESET_CUSTOM;
    st->rbj_bank_id = EQ_RBJ_BANK_SINGLE_1K;
    if (st->core_mode == EQ_CORE_LEGACY)
    {
        for (band = 0; band < EQ_NUM_BANDS; band++)
        {
            EQ_SetLegacyTarget(st, band, 0);
        }
    }
    else if ((st->core_mode == EQ_CORE_RBJ_CASCADE) && (changed != 0))
    {
        EQ_InstallRbjCachedTarget(st, EQ_RBJ_BANK_SINGLE_1K, 0);
    }
}

void Equalizer_ProcessFrame(EQ_STATE *st, const short *in, short *out, int n)
{
    int index;

    if ((st == 0) || (in == 0) || (out == 0) || (n <= 0))
    {
        return;
    }
    if (EQ_UsesTransparentPath(st) != 0)
    {
        if (in != out)
        {
            memcpy(out, in, (unsigned int)n * sizeof(*out));
        }
        return;
    }

    if (st->core_mode == EQ_CORE_FLOAT_COPY)
    {
        for (index = 0; index < n; index++)
        {
            out[index] = EQ_ProcessFloatCopyShort(st, in[index]);
        }
        return;
    }

    EQ_DesignLegacyCoeffs();
    for (index = 0; index < n; index++)
    {
        float y;

        if (EQ_UsesTransparentPath(st) != 0)
        {
            out[index] = in[index];
            continue;
        }
        if (st->core_mode == EQ_CORE_LEGACY)
        {
            y = EQ_ProcessLegacySample(st, (float)in[index]);
        }
        else
        {
            y = EQ_ProcessRbjSample(st, (float)in[index]);
        }
        out[index] = EQ_SaturateToShort(st, y);
    }
}

void Equalizer_ProcessFrameFloat(EQ_STATE *st, const float *in,
                                 float *out, int n)
{
    int index;

    if ((st == 0) || (in == 0) || (out == 0) || (n <= 0))
    {
        return;
    }
    if ((EQ_UsesTransparentPath(st) != 0) ||
        (st->core_mode == EQ_CORE_FLOAT_COPY))
    {
        if (in != out)
        {
            memcpy(out, in, (unsigned int)n * sizeof(*out));
        }
        return;
    }

    EQ_DesignLegacyCoeffs();
    for (index = 0; index < n; index++)
    {
        if (EQ_UsesTransparentPath(st) != 0)
        {
            out[index] = in[index];
            continue;
        }
        if (st->core_mode == EQ_CORE_LEGACY)
        {
            out[index] = EQ_ProcessLegacySample(st, in[index]);
        }
        else
        {
            out[index] = EQ_ProcessRbjSample(st, in[index]);
        }
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
    return st->band_gain_db[band];
}

float Equalizer_GetBandMagnitudeDb(int band, float freq_hz)
{
    float magnitude;

    if ((band < 0) || (band >= EQ_NUM_BANDS))
    {
        return -120.0f;
    }
    EQ_DesignLegacyCoeffs();
    magnitude = EQ_ComplexMagnitude(EQ_BiquadResponse(&EQ_LegacyCoeff[band],
                                                       freq_hz));
    if (magnitude < EQ_EPS)
    {
        magnitude = EQ_EPS;
    }
    return 20.0f * log10f(magnitude);
}

float Equalizer_GetPredictedPeakDb(const EQ_STATE *st)
{
    const EQ_FILTER_BANK *bank;

    if ((st == 0) || (st->core_mode != EQ_CORE_RBJ_CASCADE) ||
        (Equalizer_IsFlat(st) != 0))
    {
        return 0.0f;
    }
    bank = (st->pending_bank_valid != 0) ? &st->pending_bank :
                                            &st->active_bank;
    return bank->predicted_peak_db;
}

float Equalizer_GetPreampDb(const EQ_STATE *st)
{
    const EQ_FILTER_BANK *bank;

    if ((st == 0) || (st->core_mode != EQ_CORE_RBJ_CASCADE) ||
        (Equalizer_IsFlat(st) != 0))
    {
        return 0.0f;
    }
    bank = (st->pending_bank_valid != 0) ? &st->pending_bank :
                                            &st->active_bank;
    return bank->preamp_db;
}

int Equalizer_IsFlat(const EQ_STATE *st)
{
    return ((st == 0) || (EQ_AllGainsFlat(st->band_gain_db) != 0)) ? 1 : 0;
}

int Equalizer_HasPendingTransition(const EQ_STATE *st)
{
    return ((st != 0) && (st->pending_bank_valid != 0)) ? 1 : 0;
}

int Equalizer_GetTransitionRemaining(const EQ_STATE *st)
{
    return (st == 0) ? 0 : st->transition_remaining;
}

int Equalizer_GetCoefficientInfo(const EQ_STATE *st, int section,
                                 EQ_SECTION_INFO *info)
{
    const EQ_BIQUAD *c;
    const EQ_FILTER_BANK *bank;

    if ((st == 0) || (info == 0) || (section < 0) ||
        (section >= EQ_NUM_BANDS))
    {
        return 0;
    }
    memset(info, 0, sizeof(*info));
    info->core = st->core_mode;
    info->section = section;
    info->f0_hz = EQ_BandCenterHz[section];
    info->gain_db = st->band_gain_db[section];
    if (st->core_mode == EQ_CORE_LEGACY)
    {
        EQ_DesignLegacyCoeffs();
        c = &EQ_LegacyCoeff[section];
        info->type = EQ_SECTION_LEGACY_BANDPASS;
        info->q_or_bw = EQ_Q_FACTOR;
    }
    else if (st->core_mode == EQ_CORE_RBJ_CASCADE)
    {
        bank = (st->pending_bank_valid != 0) ? &st->pending_bank :
                                                &st->active_bank;
        c = &bank->section[section];
        info->type = (section == 0) ? EQ_SECTION_LOW_SHELF :
                     ((section == (EQ_NUM_BANDS - 1)) ?
                      EQ_SECTION_HIGH_SHELF : EQ_SECTION_PEAKING);
        info->q_or_bw = (info->type == EQ_SECTION_PEAKING) ?
                          EQ_RBJ_BANDWIDTH_OCTAVES : EQ_RBJ_SHELF_SLOPE;
    }
    else
    {
        return 0;
    }
    info->b0 = c->b0;
    info->b1 = c->b1;
    info->b2 = c->b2;
    info->a1 = c->a1;
    info->a2 = c->a2;
    EQ_BiquadPoleRadii(c, &info->pole_radius_1, &info->pole_radius_2);
    return (EQ_BiquadIsFinite(c) != 0) ? 1 : 0;
}

void Equalizer_GetSystemResponse(const EQ_STATE *st, float freq_hz,
                                 float *real_out, float *imag_out)
{
    EQ_COMPLEX h;

    h.real = 1.0f;
    h.imag = 0.0f;
    if ((st != 0) && (EQ_UsesTransparentPath(st) == 0))
    {
        if (st->core_mode == EQ_CORE_LEGACY)
        {
            int band;

            EQ_DesignLegacyCoeffs();
            for (band = 0; band < EQ_NUM_BANDS; band++)
            {
                EQ_COMPLEX b = EQ_BiquadResponse(&EQ_LegacyCoeff[band],
                                                  freq_hz);

                h = EQ_ComplexAdd(h, EQ_ComplexScale(b,
                    pow(10.0, (double)st->band_gain_db[band] / 20.0) - 1.0));
            }
        }
        else if (st->core_mode == EQ_CORE_RBJ_CASCADE)
        {
            const EQ_FILTER_BANK *bank = (st->pending_bank_valid != 0) ?
                                           &st->pending_bank :
                                           &st->active_bank;

            h = EQ_BankResponse(bank, freq_hz, 1);
        }
    }
    if (real_out != 0)
    {
        *real_out = h.real;
    }
    if (imag_out != 0)
    {
        *imag_out = h.imag;
    }
}

float Equalizer_GetSystemMagnitudeDb(const EQ_STATE *st, float freq_hz)
{
    float real;
    float imag;
    float magnitude;

    Equalizer_GetSystemResponse(st, freq_hz, &real, &imag);
    magnitude = sqrtf(real * real + imag * imag);
    if (magnitude < EQ_EPS)
    {
        magnitude = EQ_EPS;
    }
    return 20.0f * log10f(magnitude);
}

float Equalizer_GetSystemPhaseDeg(const EQ_STATE *st, float freq_hz)
{
    float real;
    float imag;
    EQ_COMPLEX h;

    Equalizer_GetSystemResponse(st, freq_hz, &real, &imag);
    h.real = real;
    h.imag = imag;
    return EQ_ComplexPhase(h) * (180.0f / EQ_PI);
}

float Equalizer_GetSystemGroupDelaySamples(const EQ_STATE *st,
                                           float freq_hz)
{
    float delta_hz;
    float low_hz;
    float high_hz;
    float phase_low;
    float phase_high;
    float phase_delta;
    float omega_delta;

    if ((freq_hz <= 0.0f) || (freq_hz >= (EQ_SAMPLE_RATE * 0.5f)))
    {
        return 0.0f;
    }
    delta_hz = (freq_hz * 0.001f > 0.1f) ? freq_hz * 0.001f : 0.1f;
    low_hz = freq_hz - delta_hz;
    high_hz = freq_hz + delta_hz;
    if (low_hz < 1.0f)
    {
        low_hz = 1.0f;
    }
    if (high_hz > (EQ_SAMPLE_RATE * 0.5f - 1.0f))
    {
        high_hz = EQ_SAMPLE_RATE * 0.5f - 1.0f;
    }
    phase_low = Equalizer_GetSystemPhaseDeg(st, low_hz) * (EQ_PI / 180.0f);
    phase_high = Equalizer_GetSystemPhaseDeg(st, high_hz) * (EQ_PI / 180.0f);
    phase_delta = phase_high - phase_low;
    while (phase_delta > EQ_PI)
    {
        phase_delta -= 2.0f * EQ_PI;
    }
    while (phase_delta < -EQ_PI)
    {
        phase_delta += 2.0f * EQ_PI;
    }
    omega_delta = (2.0f * EQ_PI * (high_hz - low_hz)) / EQ_SAMPLE_RATE;
    if (EQ_Abs(omega_delta) < EQ_EPS)
    {
        return 0.0f;
    }
    return -phase_delta / omega_delta;
}
