#include "user_equalizer_eval.h"

#include <stdio.h>

static int failures = 0;

static void check(int condition, const char *message)
{
    if (!condition)
    {
        printf("FAIL: %s\n", message);
        failures++;
    }
}

int main(void)
{
    int index;
    int nonflat_differs;
    unsigned long expected_cases;

    EqualizerEval_BoardFinalMetrics();
    expected_cases = (unsigned long)EQ_FINAL_METRICS_CASE_COUNT *
                     (1UL +
                      (unsigned long)EQ_FINAL_METRICS_THD_FREQUENCY_COUNT +
                      (unsigned long)EQ_FINAL_METRICS_SNR_SIGNAL_COUNT);

    check(EQ_DebugFinalMetricsCompiled == 1U,
          "dedicated metrics build marker");
    check(EQ_DebugFinalMetricsStatus == EQ_FINAL_METRICS_STATUS_READY,
          "metrics run reaches READY");
    check(EQ_DebugFinalMetricsCompletedCases == expected_cases,
          "all response, THD, and SNR cases complete");
    check(EQ_DebugFinalMetricsErrorCount == 0UL,
          "metrics run reports no internal errors");
    check(EQ_DebugFinalMetricsMaxFrameCycles == 0UL,
          "Host run does not claim C6748 cycle evidence");

    for (index = 0; index < EQ_FINAL_METRICS_CASE_COUNT; index++)
    {
        check(EQ_FinalMetricsResponseClipCount[index] == 0UL,
              "response case has no clipping");
    }
    for (index = 0;
         index < EQ_FINAL_METRICS_CASE_COUNT *
                 EQ_FINAL_METRICS_THD_FREQUENCY_COUNT;
         index++)
    {
        check(EQ_FinalMetricsThdClipCount[index] == 0UL,
              "THD case has no clipping");
    }
    for (index = 0;
         index < EQ_FINAL_METRICS_CASE_COUNT *
                 EQ_FINAL_METRICS_SNR_SIGNAL_COUNT;
         index++)
    {
        check(EQ_FinalMetricsSnrClipCount[index] == 0UL,
              "SNR case has no clipping");
    }

    check(EQ_FinalMetricsResponsePacked[0] ==
              (unsigned int)EQ_FINAL_METRICS_IMPULSE_PEAK,
          "FLAT impulse first packed word is identity");
    for (index = 1; index < EQ_FINAL_METRICS_PACKED_WORDS; index++)
    {
        check(EQ_FinalMetricsResponsePacked[index] == 0U,
              "FLAT impulse tail is byte exact zero");
    }
    nonflat_differs = 0;
    for (index = 0; index < EQ_FINAL_METRICS_PACKED_WORDS; index++)
    {
        if (EQ_FinalMetricsResponsePacked[
                EQ_FINAL_METRICS_PACKED_WORDS + index] !=
            EQ_FinalMetricsResponsePacked[index])
        {
            nonflat_differs = 1;
            break;
        }
    }
    check(nonflat_differs != 0, "BASS response differs from FLAT");
    check(EQ_FinalMetricsSnrInputPacked[0] != 0U,
          "SNR reference includes captured preroll input");

    printf("equalizer_final_metrics_test failures=%d cases=%lu\n",
           failures, EQ_DebugFinalMetricsCompletedCases);
    return failures == 0 ? 0 : 1;
}
