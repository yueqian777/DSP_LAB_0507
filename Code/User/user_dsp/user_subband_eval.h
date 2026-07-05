/**
 * user_subband_eval.h
 *
 * PC algo-only evaluation helpers and board-side timing watch variables.
 */

#ifndef _USER_SUBBAND_EVAL_H_
#define _USER_SUBBAND_EVAL_H_

#define SUBBAND_EVAL_BOARD_NOT_MEASURED "NOT_MEASURED_ON_PC"
#define SUBBAND_EVAL_BOARD_PASS "PASS"
#define SUBBAND_EVAL_BOARD_FAIL "FAIL"

typedef struct
{
    float wola_passthrough_snr_db;
    float wola_energy_ratio;
    int wola_delay_samples;
    int expected_delay_samples;
    int measured_delay_samples;
    unsigned long learn_target_hops;
    unsigned long learn_actual_hops;
    float learn_progress_final;
    float noise_psd_avg;
    float input_snr_db;
    float output_snr_db;
    float snr_improvement_db;
    float noise_reduction_db;
    float input_energy;
    float output_energy;
    float output_input_energy_ratio;
    float speech_preservation_ratio;
    float clean_output_corr;
    float gain_avg;
    float gain_min;
    float gain_max;
    float frame_budget_ms;
    float wola_only_last_ms;
    float wola_only_max_ms;
    float pc_runtime_ms;
    float board_last_ms;
    float board_max_ms;
    float denoise_last_ms;
    float denoise_max_ms;
    float cpu_usage_percent;
    const char *board_realtime_status;
    int pass_wola;
    int pass_learning;
    int pass_snr_improvement;
    int pass_noise_reduction;
    int pass_speech_preservation;
    int pass_realtime;
    int pass_overall;
} SubbandEvalResult;

extern volatile unsigned long SUBBAND_EVAL_DebugAdFrames;
extern volatile unsigned long SUBBAND_EVAL_DebugDaFrames;
extern volatile unsigned long SUBBAND_EVAL_DebugWolaFrames;
extern volatile unsigned long SUBBAND_EVAL_DebugDenoiseFrames;
extern volatile float SUBBAND_EVAL_DebugFrameBudgetMs;
extern volatile float SUBBAND_EVAL_DebugDenoiseLastMs;
extern volatile float SUBBAND_EVAL_DebugDenoiseMaxMs;
extern volatile float SUBBAND_EVAL_DebugCpuUsagePercent;

void SubbandEval_PrintReport(const SubbandEvalResult *r);
int SubbandEval_RunWolaPassthroughTest(SubbandEvalResult *r);
int SubbandEval_RunNoiseLearningTest(SubbandEvalResult *r);
int SubbandEval_RunNoiseSuppressionTest(SubbandEvalResult *r,
                                        const char *audio_dir);
int SubbandEval_RunSNRImprovementTest(float target_snr_db,
                                      SubbandEvalResult *r,
                                      const char *audio_dir);
int SubbandEval_RunImpulseAndSilenceRobustnessTest(void);
int SubbandEval_OfflineTest_All(void);

#endif /* _USER_SUBBAND_EVAL_H_ */
