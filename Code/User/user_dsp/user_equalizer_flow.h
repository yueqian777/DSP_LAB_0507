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

extern volatile unsigned long EQ_DebugAdFrames;
extern volatile unsigned long EQ_DebugDaFrames;
extern volatile unsigned long EQ_DebugProcessFrames;
extern volatile unsigned long EQ_DebugLastCycles;
extern volatile unsigned long EQ_DebugMaxCycles;
extern volatile float EQ_DebugLastMs;
extern volatile float EQ_DebugMaxMs;
extern volatile int EQ_DebugMode;
extern volatile int EQ_DebugDiagPath;
extern volatile float EQ_DebugBandGainDb[EQ_NUM_BANDS];
extern volatile const unsigned long EQ_DebugBuildMagic;
extern volatile const char EQ_DebugBuildId[];
extern volatile const int EQ_DebugBuildDirty;

void Equalizer_Flow_Example(void);

#endif /* _USER_EQUALIZER_FLOW_H_ */
