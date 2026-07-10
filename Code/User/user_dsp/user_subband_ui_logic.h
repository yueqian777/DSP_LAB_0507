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
unsigned long SubbandUI_RemainingMs(unsigned long learn_hops,
                                    unsigned long target_hops,
                                    unsigned long hop_samples,
                                    unsigned long sample_rate);
void SubbandUI_LatchInit(SubbandUITouchLatch *latch);
int SubbandUI_LatchUpdate(SubbandUITouchLatch *latch, int is_pressed);

#endif
