#include <stdio.h>
#include <string.h>

#include "user_subband_denoise.h"

static int failures = 0;

static void Check(int condition, const char *message)
{
    if (!condition)
    {
        printf("FAIL: %s\n", message);
        failures++;
    }
}

static void FillTonalSpectrum(float *re, float *im)
{
    int k;

    memset(re, 0, sizeof(float) * SUBBAND_POS_BINS);
    memset(im, 0, sizeof(float) * SUBBAND_POS_BINS);
    for (k = 0; k < SUBBAND_POS_BINS; k++)
    {
        re[k] = 1.0f;
    }

    re[9] = 16.0f;
    re[10] = 20.0f;
    re[11] = 16.0f;
}

static void PrepareReadyMcra(float *re, float *im)
{
    int k;

    SubbandDenoise_StartNoiseLearning();
    memset(re, 0, sizeof(float) * SUBBAND_POS_BINS);
    memset(im, 0, sizeof(float) * SUBBAND_POS_BINS);
    for (k = 0; k < SUBBAND_POS_BINS; k++)
    {
        re[k] = 10.0f;
    }
    while (SubbandDenoise_IsLearning() != 0)
    {
        SubbandDenoise_ProcessSpectrum(re, im);
    }
    SubbandDenoise_SetNoiseTrackMode(SUBBAND_DENOISE_TRACK_MCRA);
    SubbandDenoise_SetEnabled(1);
}

static void TestDefaultsAndResetIsolation(void)
{
    float re[SUBBAND_POS_BINS];
    float im[SUBBAND_POS_BINS];

    SubbandDenoise_Reset();
    Check(SUBBAND_DENOISE_DebugMcraTonalGuardHits == 0UL,
          "Reset clears cumulative tonal guard hits");
    Check(SUBBAND_DENOISE_DebugMcraTonalGuardBinsLastFrame == 0UL,
          "Reset clears last-frame tonal guard bins");

    PrepareReadyMcra(re, im);
    FillTonalSpectrum(re, im);
    SubbandDenoise_ProcessSpectrum(re, im);
    Check(SUBBAND_DENOISE_DebugMcraTonalGuardBinsLastFrame > 0UL,
          "legacy defaults trigger the test tone");
    Check(SUBBAND_DENOISE_DebugMcraTonalGuardHits ==
              SUBBAND_DENOISE_DebugMcraTonalGuardBinsLastFrame,
          "first processed frame accumulates exactly its guarded bins");

    SubbandDenoise_SetMcraTonalGuardParams(5.0f, 1.80f, 0.28f);
    FillTonalSpectrum(re, im);
    SubbandDenoise_ProcessSpectrum(re, im);
    Check(SUBBAND_DENOISE_DebugMcraTonalGuardBinsLastFrame == 0UL,
          "Mode 6 thresholds reject the legacy-only test tone");
    Check(SUBBAND_DENOISE_DebugMcraTonalGuardHits > 0UL,
          "last-frame clearing does not clear cumulative hits");

    SubbandDenoise_Reset();
    Check(SUBBAND_DENOISE_DebugMcraTonalGuardHits == 0UL,
          "Mode 8 Reset clears cumulative hits after Mode 6");
    Check(SUBBAND_DENOISE_DebugMcraTonalGuardBinsLastFrame == 0UL,
          "Mode 8 Reset clears last-frame hits after Mode 6");
    PrepareReadyMcra(re, im);
    FillTonalSpectrum(re, im);
    SubbandDenoise_ProcessSpectrum(re, im);
    Check(SUBBAND_DENOISE_DebugMcraTonalGuardBinsLastFrame > 0UL,
          "Reset restores legacy tonal guard thresholds");
}

int main(void)
{
    TestDefaultsAndResetIsolation();
    if (failures != 0)
    {
        printf("subband mcra tonal guard: %d failure(s)\n", failures);
        return 1;
    }
    printf("subband mcra tonal guard: PASS\n");
    return 0;
}
