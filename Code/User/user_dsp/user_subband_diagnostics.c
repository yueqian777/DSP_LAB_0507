#include "user_subband_diagnostics.h"
#include "user_subband_wola.h"

volatile unsigned long SUBBAND_EVAL_DebugAdFrames = 0;
volatile unsigned long SUBBAND_EVAL_DebugDaFrames = 0;
volatile unsigned long SUBBAND_EVAL_DebugWolaFrames = 0;
volatile unsigned long SUBBAND_EVAL_DebugDenoiseFrames = 0;
volatile float SUBBAND_EVAL_DebugFrameBudgetMs =
    ((float)SUBBAND_FRAME_LEN / SUBBAND_SAMPLE_RATE) * 1000.0f;
volatile float SUBBAND_EVAL_DebugDenoiseLastMs = 0.0f;
volatile float SUBBAND_EVAL_DebugDenoiseMaxMs = 0.0f;
volatile float SUBBAND_EVAL_DebugCpuUsagePercent = 0.0f;
