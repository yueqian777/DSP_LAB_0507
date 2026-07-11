#ifndef _USER_SUBBAND_UI_LOGIC_H_
#define _USER_SUBBAND_UI_LOGIC_H_

typedef struct
{
    unsigned char pressed;
} SubbandUITouchLatch;

typedef enum
{
    SUBBAND_UI_LEARNING_DRAW_NONE = 0,
    SUBBAND_UI_LEARNING_DRAW_STATE,
    SUBBAND_UI_LEARNING_DRAW_REMAINING_DIGIT
} SubbandUILearningDisplayJob;

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
SubbandUILearningDisplayJob SubbandUI_SelectLearningDisplayJob(
    int mode_uses_learning,
    int last_mode_uses_learning,
    int learning,
    int last_learning,
    int ready,
    int last_ready,
    int remaining_seconds,
    int last_remaining_seconds);
void SubbandUI_LatchInit(SubbandUITouchLatch *latch);
int SubbandUI_LatchUpdate(SubbandUITouchLatch *latch, int is_pressed);

#endif
