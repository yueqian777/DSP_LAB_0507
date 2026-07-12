/**
 * user_equalizer_flow.h
 *
 * Project 3.3 board-side AD/DA flow for the graphic equalizer.
 */

#ifndef _USER_EQUALIZER_FLOW_H_
#define _USER_EQUALIZER_FLOW_H_

#include "user_equalizer.h"

#define EQ_DIAG_RAW_COPY       0
#define EQ_DIAG_FLOAT_COPY     1
#define EQ_DIAG_FLAT           2
#define EQ_DIAG_SINGLE_BAND    3
#define EQ_DIAG_PRESET         4

#define EQ_FRAME_SERVICE_BUDGET_MS \
    (1000.0f * (float)EQ_FRAME_LEN / EQ_SAMPLE_RATE)
#define EQ_FRAME_SERVICE_BUDGET_CYCLES \
    ((unsigned long)(456000.0f * EQ_FRAME_SERVICE_BUDGET_MS + 0.5f))

extern volatile unsigned long EQ_DebugAdFrames;
extern volatile unsigned long EQ_DebugDaFrames;
extern volatile unsigned long EQ_DebugProcessFrames;
extern volatile unsigned long EQ_DebugAlgoLastCycles;
extern volatile unsigned long EQ_DebugAlgoMaxCycles;
extern volatile float EQ_DebugAlgoLastMs;
extern volatile float EQ_DebugAlgoMaxMs;
extern volatile unsigned long EQ_DebugModeServiceLastCycles;
extern volatile unsigned long EQ_DebugModeServiceMaxCycles;
extern volatile unsigned long EQ_DebugFrameServiceLastCycles;
extern volatile unsigned long EQ_DebugFrameServiceMaxCycles;
extern volatile float EQ_DebugFrameServiceLastMs;
extern volatile float EQ_DebugFrameServiceMaxMs;
extern volatile unsigned long EQ_DebugFrameLatencyLastCycles;
extern volatile unsigned long EQ_DebugFrameLatencyMaxCycles;
extern volatile float EQ_DebugFrameLatencyLastMs;
extern volatile float EQ_DebugFrameLatencyMaxMs;
extern volatile unsigned long EQ_DebugDeadlineMissCount;
extern volatile unsigned long EQ_DebugFrameServiceOverlapCount;
extern volatile unsigned long EQ_DebugFrameServiceDroppedCount;
extern volatile int EQ_DebugMode;
extern volatile int EQ_DebugDiagPath;
extern volatile int EQ_DebugRequestedMode;
extern volatile int EQ_DebugTransitionTargetMode;
extern volatile int EQ_DebugAppliedMode;
extern volatile unsigned long EQ_DebugModeChangeCount;
extern volatile float EQ_DebugBandGainDb[EQ_NUM_BANDS];
extern volatile const unsigned long EQ_DebugBuildMagic;
extern volatile const char EQ_DebugBuildId[];
extern volatile const char EQ_DebugBuildVersion[];
extern volatile const char EQ_DebugBuildGitSha[];
extern volatile const char EQ_DebugBuildTimestamp[];
extern volatile const int EQ_DebugBuildDirty;

void Equalizer_Flow_Example(void);

#endif /* _USER_EQUALIZER_FLOW_H_ */
