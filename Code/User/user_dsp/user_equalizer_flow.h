/**
 * user_equalizer_flow.h
 *
 * Project 3.3 board-side AD/DA flow for the graphic equalizer.
 */

#ifndef _USER_EQUALIZER_FLOW_H_
#define _USER_EQUALIZER_FLOW_H_

#include "user_equalizer.h"

extern volatile unsigned long EQ_DebugAdFrames;
extern volatile unsigned long EQ_DebugDaFrames;
extern volatile unsigned long EQ_DebugProcessFrames;
extern volatile unsigned long EQ_DebugLastCycles;
extern volatile unsigned long EQ_DebugMaxCycles;
extern volatile float EQ_DebugLastMs;
extern volatile float EQ_DebugMaxMs;
extern volatile int EQ_DebugMode;
extern volatile float EQ_DebugBandGainDb[EQ_NUM_BANDS];

void Equalizer_Flow_Example(void);

#endif /* _USER_EQUALIZER_FLOW_H_ */
