/**
 * user_subband_flow.h
 *
 * Minimal subband analysis/synthesis flow test.
 */

#ifndef _USER_SUBBAND_FLOW_H_
#define _USER_SUBBAND_FLOW_H_

#define SUBBAND_FLOW_BAND_COUNT   8u
#define SUBBAND_FLOW_FRAME_LEN    1024u
#define SUBBAND_FLOW_SUBBAND_LEN  (SUBBAND_FLOW_FRAME_LEN / SUBBAND_FLOW_BAND_COUNT)

typedef struct
{
    short subband[SUBBAND_FLOW_BAND_COUNT][SUBBAND_FLOW_SUBBAND_LEN];
} SUBBAND_FLOW_STATE;

void Subband_Flow_Init(SUBBAND_FLOW_STATE *state);
void Subband_Flow_ProcessFrame(SUBBAND_FLOW_STATE *state,
                               const short input[SUBBAND_FLOW_FRAME_LEN],
                               short output[SUBBAND_FLOW_FRAME_LEN]);

#ifndef SUBBAND_FLOW_ALGO_ONLY
void Subband_Flow_Example(void);
#endif

#endif /* _USER_SUBBAND_FLOW_H_ */
