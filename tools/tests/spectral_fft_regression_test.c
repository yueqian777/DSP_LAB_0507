#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "user_spectral_fft.h"

#define LEGACY_NFFT 512
#define SANITY_NFFT 1024
#define EXPECTED_BIT_EXACT_CASES 8
#define EXPECTED_INVALID_CHECKS 18
#define EXPECTED_WOLA_BIT_EXACT_HOPS 12
#define LEGACY_PI 3.14159265358979323846f

static float LegacyTwRe[LEGACY_NFFT / 2];
static float LegacyTwIm[LEGACY_NFFT / 2];
static float SharedTwRe[LEGACY_NFFT / 2];
static float SharedTwIm[LEGACY_NFFT / 2];
static float InputRe[LEGACY_NFFT];
static float InputIm[LEGACY_NFFT];
static float LegacyRe[LEGACY_NFFT];
static float LegacyIm[LEGACY_NFFT];
static float SharedRe[LEGACY_NFFT];
static float SharedIm[LEGACY_NFFT];
static float SanityRe[SANITY_NFFT];
static float SanityIm[SANITY_NFFT];
static float SanityTwRe[SANITY_NFFT / 2];
static float SanityTwIm[SANITY_NFFT / 2];

static int failures = 0;
static int bit_exact_cases = 0;
static int invalid_checks = 0;
static int sanity_1024 = 0;
static int wola_bit_exact_hops = 0;

static int (*const GenerateTwiddles)(float *, float *, int) =
    SpectralFFT_GenerateTwiddles;
static int (*const ForwardFFT)(float *, float *, const float *,
                               const float *, int) = SpectralFFT_Forward;

static void Check(int condition, const char *message)
{
    if (!condition)
    {
        printf("FAIL: %s\n", message);
        failures++;
    }
}

static void Check_Invalid(int result, const char *message)
{
    invalid_checks++;
    Check(result == 0, message);
}

static void Legacy_Generate_Twiddles(void)
{
    int i;
    float phase;

    for (i = 0; i < LEGACY_NFFT / 2; i++)
    {
        phase = (-2.0f * LEGACY_PI * (float)i) / (float)LEGACY_NFFT;
        LegacyTwRe[i] = cosf(phase);
        LegacyTwIm[i] = sinf(phase);
    }
}

/*
 * Frozen from the Project 3.2 512-point user_subband_wola.c kernel.
 * Keep the operation and statement order unchanged so memcmp remains a
 * regression check for the shared implementation.
 */
static void Legacy_Bit_Reverse(float *re, float *im)
{
    unsigned int i;
    unsigned int j;

    j = 0;
    for (i = 1; i < (unsigned int)LEGACY_NFFT; i++)
    {
        unsigned int bit;

        bit = (unsigned int)LEGACY_NFFT >> 1;
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

static void Legacy_FFT_InPlace(float *re, float *im)
{
    int len;

    Legacy_Bit_Reverse(re, im);

    for (len = 2; len <= LEGACY_NFFT; len <<= 1)
    {
        int half;
        int tw_step;
        int start;

        half = len >> 1;
        tw_step = LEGACY_NFFT / len;

        for (start = 0; start < LEGACY_NFFT; start += len)
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
                wr = LegacyTwRe[tw_idx];
                wi = LegacyTwIm[tw_idx];

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

static void Prepare_Impulse(void)
{
    memset(InputRe, 0, sizeof(InputRe));
    memset(InputIm, 0, sizeof(InputIm));
    InputRe[173] = 1.0f;
}

static void Prepare_Bin_Tone(int bin)
{
    int i;

    for (i = 0; i < LEGACY_NFFT; i++)
    {
        float phase;

        phase = (2.0f * LEGACY_PI * (float)bin * (float)i) /
                (float)LEGACY_NFFT;
        InputRe[i] = cosf(phase);
        InputIm[i] = 0.0f;
    }
}

static uint32_t Next_Random(uint32_t *state)
{
    uint32_t value;

    value = *state;
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    *state = value;
    return value;
}

static float Random_Float(uint32_t *state)
{
    int value;

    value = (int)(Next_Random(state) & 0xffffU) - 32768;
    return (float)value / 32768.0f;
}

static void Prepare_Random_Complex(uint32_t seed)
{
    int i;
    uint32_t state;

    state = seed;
    for (i = 0; i < LEGACY_NFFT; i++)
    {
        InputRe[i] = Random_Float(&state);
        InputIm[i] = Random_Float(&state);
    }
}

static void Run_Bit_Exact_Case(const char *name)
{
    int forward_ok;
    int re_match;
    int im_match;

    memcpy(LegacyRe, InputRe, sizeof(LegacyRe));
    memcpy(LegacyIm, InputIm, sizeof(LegacyIm));
    memcpy(SharedRe, InputRe, sizeof(SharedRe));
    memcpy(SharedIm, InputIm, sizeof(SharedIm));

    Legacy_FFT_InPlace(LegacyRe, LegacyIm);
    forward_ok = ForwardFFT(SharedRe, SharedIm, SharedTwRe, SharedTwIm,
                            LEGACY_NFFT);
    if (forward_ok != 1)
    {
        printf("FAIL: %s: SpectralFFT_Forward rejected 512 points\n", name);
        failures++;
        return;
    }

    re_match = memcmp(LegacyRe, SharedRe, sizeof(LegacyRe)) == 0;
    im_match = memcmp(LegacyIm, SharedIm, sizeof(LegacyIm)) == 0;
    if (!re_match)
    {
        printf("FAIL: %s: real output is not bit exact\n", name);
        failures++;
    }
    if (!im_match)
    {
        printf("FAIL: %s: imaginary output is not bit exact\n", name);
        failures++;
    }
    if (re_match && im_match)
    {
        bit_exact_cases++;
    }
}

static void Test_512_Bit_Exact_Regression(void)
{
    static const int tone_bins[] = {1, 7, 73, 255};
    static const char *const tone_names[] = {
        "bin-tone-1", "bin-tone-7", "bin-tone-73", "bin-tone-255"};
    static const uint32_t random_seeds[] = {
        UINT32_C(0x13579bdf), UINT32_C(0x2468ace1),
        UINT32_C(0xc001d00d)};
    static const char *const random_names[] = {
        "random-complex-a", "random-complex-b", "random-complex-c"};
    int i;
    int generated;

    Legacy_Generate_Twiddles();
    generated = GenerateTwiddles(SharedTwRe, SharedTwIm, LEGACY_NFFT);
    Check(generated == 1,
          "SpectralFFT_GenerateTwiddles rejected 512 points");
    if (generated != 1)
    {
        return;
    }

    Check(memcmp(LegacyTwRe, SharedTwRe, sizeof(LegacyTwRe)) == 0,
          "512-point real twiddles are not bit exact");
    Check(memcmp(LegacyTwIm, SharedTwIm, sizeof(LegacyTwIm)) == 0,
          "512-point imaginary twiddles are not bit exact");

    Prepare_Impulse();
    Run_Bit_Exact_Case("impulse-173");

    for (i = 0; i < (int)(sizeof(tone_bins) / sizeof(tone_bins[0])); i++)
    {
        Prepare_Bin_Tone(tone_bins[i]);
        Run_Bit_Exact_Case(tone_names[i]);
    }

    for (i = 0;
         i < (int)(sizeof(random_seeds) / sizeof(random_seeds[0])); i++)
    {
        Prepare_Random_Complex(random_seeds[i]);
        Run_Bit_Exact_Case(random_names[i]);
    }
}

typedef struct
{
    float window[LEGACY_NFFT];
    float analysis[LEGACY_NFFT];
    float overlap[LEGACY_NFFT];
    float re[LEGACY_NFFT];
    float im[LEGACY_NFFT];
} WOLA_REGRESSION_STATE;

static WOLA_REGRESSION_STATE LegacyWola;
static WOLA_REGRESSION_STATE SharedWola;
static float LegacyWolaOut[LEGACY_NFFT / 2];
static float SharedWolaOut[LEGACY_NFFT / 2];

static void Wola_Init(WOLA_REGRESSION_STATE *state)
{
    int i;

    memset(state, 0, sizeof(*state));
    for (i = 0; i < LEGACY_NFFT; i++)
    {
        float phase;
        float hann;

        phase = (2.0f * LEGACY_PI * (float)i) / (float)LEGACY_NFFT;
        hann = 0.5f - 0.5f * cosf(phase);
        if (hann < 0.0f)
        {
            hann = 0.0f;
        }
        state->window[i] = sqrtf(hann);
    }
}

static void Wola_Fft(WOLA_REGRESSION_STATE *state, int use_shared)
{
    if (use_shared != 0)
    {
        (void)ForwardFFT(state->re, state->im, SharedTwRe, SharedTwIm,
                         LEGACY_NFFT);
    }
    else
    {
        Legacy_FFT_InPlace(state->re, state->im);
    }
}

static void Wola_Process_Hop(WOLA_REGRESSION_STATE *state,
                             const float *input, float *output,
                             int use_shared)
{
    int i;
    const int hop = LEGACY_NFFT / 2;
    const float inv_n = 1.0f / (float)LEGACY_NFFT;

    for (i = 0; i < LEGACY_NFFT - hop; i++)
    {
        state->analysis[i] = state->analysis[i + hop];
    }
    for (i = 0; i < hop; i++)
    {
        state->analysis[LEGACY_NFFT - hop + i] = input[i];
    }

    for (i = 0; i < LEGACY_NFFT; i++)
    {
        state->re[i] = state->analysis[i] * state->window[i];
        state->im[i] = 0.0f;
    }
    Wola_Fft(state, use_shared);

    for (i = 0; i < LEGACY_NFFT; i++)
    {
        state->im[i] = -state->im[i];
    }
    Wola_Fft(state, use_shared);
    for (i = 0; i < LEGACY_NFFT; i++)
    {
        state->re[i] *= inv_n;
        state->im[i] = -state->im[i] * inv_n;
        state->overlap[i] += state->re[i] * state->window[i];
    }

    for (i = 0; i < hop; i++)
    {
        output[i] = state->overlap[i];
    }
    for (i = 0; i < LEGACY_NFFT - hop; i++)
    {
        state->overlap[i] = state->overlap[i + hop];
    }
    for (i = LEGACY_NFFT - hop; i < LEGACY_NFFT; i++)
    {
        state->overlap[i] = 0.0f;
    }
}

static void Test_Wola_Output_Bit_Exact(void)
{
    float input[LEGACY_NFFT / 2];
    uint32_t random_state;
    int hop_index;
    int i;

    Wola_Init(&LegacyWola);
    Wola_Init(&SharedWola);
    random_state = UINT32_C(0x32c6748a);

    for (hop_index = 0;
         hop_index < EXPECTED_WOLA_BIT_EXACT_HOPS;
         hop_index++)
    {
        for (i = 0; i < LEGACY_NFFT / 2; i++)
        {
            input[i] = Random_Float(&random_state) * 30000.0f;
        }

        Wola_Process_Hop(&LegacyWola, input, LegacyWolaOut, 0);
        Wola_Process_Hop(&SharedWola, input, SharedWolaOut, 1);
        if (memcmp(LegacyWolaOut, SharedWolaOut,
                   sizeof(LegacyWolaOut)) != 0)
        {
            printf("FAIL: WOLA hop %d output is not bit exact\n",
                   hop_index);
            failures++;
        }
        else
        {
            wola_bit_exact_hops++;
        }
    }
}

static void Test_Invalid_Inputs(void)
{
    static const int invalid_sizes[] = {-512, 0, 1, 3, 511, 1000};
    int i;

    Check_Invalid(GenerateTwiddles(NULL, SanityTwIm, LEGACY_NFFT),
                  "GenerateTwiddles accepts a null real table");
    Check_Invalid(GenerateTwiddles(SanityTwRe, NULL, LEGACY_NFFT),
                  "GenerateTwiddles accepts a null imaginary table");

    for (i = 0;
         i < (int)(sizeof(invalid_sizes) / sizeof(invalid_sizes[0])); i++)
    {
        Check_Invalid(GenerateTwiddles(SanityTwRe, SanityTwIm,
                                       invalid_sizes[i]),
                      "GenerateTwiddles accepts an invalid size");
    }

    Check_Invalid(ForwardFFT(NULL, SanityIm, SanityTwRe, SanityTwIm,
                             LEGACY_NFFT),
                  "Forward accepts a null real buffer");
    Check_Invalid(ForwardFFT(SanityRe, NULL, SanityTwRe, SanityTwIm,
                             LEGACY_NFFT),
                  "Forward accepts a null imaginary buffer");
    Check_Invalid(ForwardFFT(SanityRe, SanityIm, NULL, SanityTwIm,
                             LEGACY_NFFT),
                  "Forward accepts a null real twiddle table");
    Check_Invalid(ForwardFFT(SanityRe, SanityIm, SanityTwRe, NULL,
                             LEGACY_NFFT),
                  "Forward accepts a null imaginary twiddle table");

    for (i = 0;
         i < (int)(sizeof(invalid_sizes) / sizeof(invalid_sizes[0])); i++)
    {
        Check_Invalid(ForwardFFT(SanityRe, SanityIm, SanityTwRe,
                                 SanityTwIm, invalid_sizes[i]),
                      "Forward accepts an invalid size");
    }
}

static void Test_1024_Sanity(void)
{
    int i;
    int generated;
    int forwarded;
    int magnitudes_ok;

    generated = GenerateTwiddles(SanityTwRe, SanityTwIm, SANITY_NFFT);
    Check(generated == 1,
          "SpectralFFT_GenerateTwiddles rejected 1024 points");
    if (generated != 1)
    {
        return;
    }

    memset(SanityRe, 0, sizeof(SanityRe));
    memset(SanityIm, 0, sizeof(SanityIm));
    SanityRe[SANITY_NFFT - 1] = 1.0f;

    forwarded = ForwardFFT(SanityRe, SanityIm, SanityTwRe, SanityTwIm,
                           SANITY_NFFT);
    Check(forwarded == 1, "SpectralFFT_Forward rejected 1024 points");
    if (forwarded != 1)
    {
        return;
    }

    magnitudes_ok = 1;
    for (i = 0; i < SANITY_NFFT; i++)
    {
        float magnitude_squared;

        magnitude_squared =
            SanityRe[i] * SanityRe[i] + SanityIm[i] * SanityIm[i];
        if (!(fabsf(magnitude_squared - 1.0f) <= 1.0e-4f))
        {
            magnitudes_ok = 0;
            break;
        }
    }
    Check(magnitudes_ok,
          "1024-point impulse spectrum does not have unit magnitude");
    if (magnitudes_ok)
    {
        sanity_1024 = 1;
    }
}

int main(void)
{
    Test_512_Bit_Exact_Regression();
    Test_Wola_Output_Bit_Exact();
    Test_Invalid_Inputs();
    Test_1024_Sanity();

    Check(bit_exact_cases == EXPECTED_BIT_EXACT_CASES,
          "not all 512-point bit-exact cases passed");
    Check(invalid_checks == EXPECTED_INVALID_CHECKS,
          "invalid-input check count changed");
    Check(wola_bit_exact_hops == EXPECTED_WOLA_BIT_EXACT_HOPS,
          "not all Project 3.2 WOLA output hops were bit exact");
    Check(sanity_1024 == 1, "1024-point sanity case did not pass");

    printf("spectral fft regression: %s failures=%d bit_exact_cases=%d "
           "wola_bit_exact_hops=%d invalid_checks=%d sanity_1024=%d\n",
           failures == 0 ? "PASS" : "FAIL", failures, bit_exact_cases,
           wola_bit_exact_hops, invalid_checks, sanity_1024);
    return failures == 0 ? 0 : 1;
}
