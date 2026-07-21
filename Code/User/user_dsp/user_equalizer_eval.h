/**
 * user_equalizer_eval.h
 *
 * PC algo-only evaluation entry points for Project 3.3 equalizer.
 */

#ifndef _USER_EQUALIZER_EVAL_H_
#define _USER_EQUALIZER_EVAL_H_

#ifndef EQ_ENABLE_FINAL_METRICS_BOARD_TEST
#define EQ_ENABLE_FINAL_METRICS_BOARD_TEST 0
#endif

/* Returns zero only when every required host-side quality check passes. */
int EqualizerEval_OfflineTest_All(void);

#if EQ_ENABLE_FINAL_METRICS_BOARD_TEST != 0
#include "user_equalizer.h"

#define EQ_FINAL_METRICS_CASE_COUNT             6
#define EQ_FINAL_METRICS_THD_FREQUENCY_COUNT    6
#define EQ_FINAL_METRICS_SNR_SIGNAL_COUNT       3
#define EQ_FINAL_METRICS_CAPTURE_SAMPLES     8192
#define EQ_FINAL_METRICS_PACKED_WORDS \
    (EQ_FINAL_METRICS_CAPTURE_SAMPLES / 2)
#define EQ_FINAL_METRICS_PREROLL_SAMPLES \
    (4 * EQ_FRAME_LEN)
#define EQ_FINAL_METRICS_REFERENCE_SAMPLES \
    (EQ_FINAL_METRICS_PREROLL_SAMPLES + \
     EQ_FINAL_METRICS_CAPTURE_SAMPLES)
#define EQ_FINAL_METRICS_REFERENCE_PACKED_WORDS \
    (EQ_FINAL_METRICS_REFERENCE_SAMPLES / 2)

#define EQ_FINAL_METRICS_STATUS_BOOT       0U
#define EQ_FINAL_METRICS_STATUS_RUNNING    1U
#define EQ_FINAL_METRICS_STATUS_READY      2U
#define EQ_FINAL_METRICS_STATUS_ERROR      3U

extern unsigned int EQ_FinalMetricsResponsePacked[
    EQ_FINAL_METRICS_CASE_COUNT * EQ_FINAL_METRICS_PACKED_WORDS];
extern unsigned int EQ_FinalMetricsThdInputPacked[
    EQ_FINAL_METRICS_THD_FREQUENCY_COUNT *
    EQ_FINAL_METRICS_PACKED_WORDS];
extern unsigned int EQ_FinalMetricsThdOutputPacked[
    EQ_FINAL_METRICS_CASE_COUNT *
    EQ_FINAL_METRICS_THD_FREQUENCY_COUNT *
    EQ_FINAL_METRICS_PACKED_WORDS];
extern unsigned int EQ_FinalMetricsSnrInputPacked[
    EQ_FINAL_METRICS_SNR_SIGNAL_COUNT *
    EQ_FINAL_METRICS_REFERENCE_PACKED_WORDS];
extern unsigned int EQ_FinalMetricsSnrOutputPacked[
    EQ_FINAL_METRICS_CASE_COUNT *
    EQ_FINAL_METRICS_SNR_SIGNAL_COUNT *
    EQ_FINAL_METRICS_PACKED_WORDS];

extern volatile unsigned long EQ_FinalMetricsResponseClipCount[
    EQ_FINAL_METRICS_CASE_COUNT];
extern volatile unsigned long EQ_FinalMetricsThdClipCount[
    EQ_FINAL_METRICS_CASE_COUNT *
    EQ_FINAL_METRICS_THD_FREQUENCY_COUNT];
extern volatile unsigned long EQ_FinalMetricsSnrClipCount[
    EQ_FINAL_METRICS_CASE_COUNT *
    EQ_FINAL_METRICS_SNR_SIGNAL_COUNT];
extern volatile unsigned int EQ_DebugFinalMetricsStatus;
extern volatile unsigned long EQ_DebugFinalMetricsCompletedCases;
extern volatile unsigned long EQ_DebugFinalMetricsMaxFrameCycles;
extern volatile unsigned long EQ_DebugFinalMetricsErrorCount;
extern volatile const unsigned int EQ_DebugFinalMetricsCompiled;
extern volatile const char EQ_DebugFinalMetricsBuildGitSha[];
extern volatile const int EQ_DebugFinalMetricsBuildDirty;

void EqualizerEval_BoardFinalMetrics(void);
#endif

#endif /* _USER_EQUALIZER_EVAL_H_ */
