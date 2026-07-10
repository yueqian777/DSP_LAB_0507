/**
 * user_subband_polyphase.h
 *
 * Test-only wrapper for the project1 8-channel polyphase analysis/synthesis
 * implementation. It deliberately has no DSP main loop or driver dependency.
 */

#ifndef _USER_SUBBAND_POLYPHASE_H_
#define _USER_SUBBAND_POLYPHASE_H_

#define SUBBAND_POLYPHASE_CHANNELS 8
#define SUBBAND_POLYPHASE_BLOCK 8
#define SUBBAND_POLYPHASE_PROTOTYPE_TAPS 129
#define SUBBAND_POLYPHASE_PADDED_TAPS 136
#define SUBBAND_POLYPHASE_PHASE_TAPS 17

void SubbandPolyphase_Init(void);
void SubbandPolyphase_Reset(void);
void SubbandPolyphase_ProcessFrame(const short *input, short *output,
                                   int sample_count);

#endif /* _USER_SUBBAND_POLYPHASE_H_ */
