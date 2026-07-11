#ifndef _USER_SUBBAND_UI_LOGIC_H_
#define _USER_SUBBAND_UI_LOGIC_H_

typedef struct
{
    unsigned char pressed;
} SubbandUITouchLatch;

int SubbandUI_ModeForButton(unsigned int button_index);
int SubbandUI_NormalizeCodecKbps(int kbps);
int SubbandUI_IsCodecMode(int mode);
int SubbandUI_ModeUsesLearning(int mode);
int SubbandUI_BuildProcessingChain(char *buffer,
                                   unsigned int buffer_size,
                                   int applied_mode,
                                   int target_kbps);
unsigned long SubbandUI_RemainingMs(unsigned long learn_hops,
                                    unsigned long target_hops,
                                    unsigned long hop_samples,
                                    unsigned long sample_rate);
int SubbandUI_RemainingSeconds(unsigned long learn_hops,
                               unsigned long target_hops,
                               unsigned long hop_samples,
                               unsigned long sample_rate);
void SubbandUI_LatchInit(SubbandUITouchLatch *latch);
int SubbandUI_LatchUpdate(SubbandUITouchLatch *latch, int is_pressed);

#endif
