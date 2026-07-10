#include "user_subband_ui_logic.h"

int SubbandUI_ModeForButton(unsigned int button_index)
{
    static const int modes[8] = {0, 1, 2, 4, 6, 7, 8, 11};

    if (button_index >= 8U)
    {
        return -1;
    }
    return modes[button_index];
}

int SubbandUI_NormalizeCodecKbps(int kbps)
{
    if ((kbps == 160) || (kbps == 240) || (kbps == 320))
    {
        return kbps;
    }
    return 240;
}

int SubbandUI_IsCodecMode(int mode)
{
    return ((mode == 8) || (mode == 11)) ? 1 : 0;
}

int SubbandUI_ModeUsesLearning(int mode)
{
    switch (mode)
    {
    case 2:
    case 4:
    case 6:
    case 7:
    case 8:
        return 1;
    default:
        return 0;
    }
}

unsigned long SubbandUI_RemainingMs(unsigned long learn_hops,
                                    unsigned long target_hops,
                                    unsigned long hop_samples,
                                    unsigned long sample_rate)
{
    unsigned long remaining_hops;
    unsigned long long numerator;

    if ((sample_rate == 0UL) || (learn_hops >= target_hops))
    {
        return 0UL;
    }
    remaining_hops = target_hops - learn_hops;
    numerator = (unsigned long long)remaining_hops *
                (unsigned long long)hop_samples * 1000ULL;
    return (unsigned long)((numerator + sample_rate - 1UL) / sample_rate);
}

void SubbandUI_LatchInit(SubbandUITouchLatch *latch)
{
    if (latch != 0)
    {
        latch->pressed = 0U;
    }
}

int SubbandUI_LatchUpdate(SubbandUITouchLatch *latch, int is_pressed)
{
    if (latch == 0)
    {
        return 0;
    }
    if (is_pressed == 0)
    {
        latch->pressed = 0U;
        return 0;
    }
    if (latch->pressed != 0U)
    {
        return 0;
    }
    latch->pressed = 1U;
    return 1;
}
