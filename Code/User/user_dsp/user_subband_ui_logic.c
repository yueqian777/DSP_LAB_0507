#include "user_subband_ui_logic.h"

static int SubbandUI_CopyBounded(char *buffer,
                                 unsigned int buffer_size,
                                 const char *text)
{
    unsigned int index;

    if ((buffer == 0) || (buffer_size == 0U) || (text == 0))
    {
        return 0;
    }
    index = 0U;
    while ((text[index] != '\0') && ((index + 1U) < buffer_size))
    {
        buffer[index] = text[index];
        index++;
    }
    buffer[index] = '\0';
    return (text[index] == '\0') ? 1 : 0;
}

static int SubbandUI_AppendBounded(char *buffer,
                                   unsigned int buffer_size,
                                   const char *text)
{
    unsigned int used;
    unsigned int source_index;

    if ((buffer == 0) || (buffer_size == 0U) || (text == 0))
    {
        return 0;
    }
    used = 0U;
    while ((used < buffer_size) && (buffer[used] != '\0'))
    {
        used++;
    }
    if (used >= buffer_size)
    {
        buffer[buffer_size - 1U] = '\0';
        return 0;
    }
    source_index = 0U;
    while ((text[source_index] != '\0') &&
           ((used + source_index + 1U) < buffer_size))
    {
        buffer[used + source_index] = text[source_index];
        source_index++;
    }
    buffer[used + source_index] = '\0';
    return (text[source_index] == '\0') ? 1 : 0;
}

static int SubbandUI_AppendKbps(char *buffer,
                                unsigned int buffer_size,
                                int target_kbps)
{
    char digits[4];

    target_kbps = SubbandUI_NormalizeCodecKbps(target_kbps);
    digits[0] = (char)('0' + ((target_kbps / 100) % 10));
    digits[1] = (char)('0' + ((target_kbps / 10) % 10));
    digits[2] = (char)('0' + (target_kbps % 10));
    digits[3] = '\0';
    return SubbandUI_AppendBounded(buffer, buffer_size, digits);
}

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

int SubbandUI_BuildProcessingChain(char *buffer,
                                   unsigned int buffer_size,
                                   int applied_mode,
                                   int target_kbps)
{
    const char *fixed_text;
    int valid_mode;
    int ok;

    fixed_text = 0;
    valid_mode = 1;
    switch (applied_mode)
    {
    case 0:
        fixed_text = "ADC -> RAW -> DAC";
        break;
    case 1:
        fixed_text = "ADC -> WOLA ANA -> WOLA SYN -> DAC";
        break;
    case 2:
        fixed_text = "ADC -> WOLA ANA -> BASIC NR -> WOLA SYN -> DAC";
        break;
    case 4:
        fixed_text = "ADC -> WOLA ANA -> MINSTAT NR -> WOLA SYN -> DAC";
        break;
    case 6:
        fixed_text = "ADC -> WOLA ANA -> MCRA NR -> WOLA SYN -> DAC";
        break;
    case 7:
        fixed_text = "ADC -> WOLA ANA -> STRONG MCRA -> WOLA SYN -> DAC";
        break;
    case 8:
        ok = SubbandUI_CopyBounded(buffer, buffer_size,
                                   "ADC -> WOLA ANA -> MCRA NR -> CODEC ");
        ok = ok && SubbandUI_AppendKbps(buffer, buffer_size, target_kbps);
        ok = ok && SubbandUI_AppendBounded(buffer, buffer_size,
                                           "k -> WOLA SYN -> DAC");
        if (ok != 0)
        {
            return 1;
        }
        valid_mode = 0;
        break;
    case 11:
        ok = SubbandUI_CopyBounded(buffer, buffer_size,
                                   "ADC -> WOLA ANA -> CODEC ");
        ok = ok && SubbandUI_AppendKbps(buffer, buffer_size, target_kbps);
        ok = ok && SubbandUI_AppendBounded(buffer, buffer_size,
                                           "k -> WOLA SYN -> DAC");
        if (ok != 0)
        {
            return 1;
        }
        valid_mode = 0;
        break;
    default:
        valid_mode = 0;
        break;
    }

    if ((valid_mode != 0) &&
        (SubbandUI_CopyBounded(buffer, buffer_size, fixed_text) != 0))
    {
        return 1;
    }
    (void)SubbandUI_CopyBounded(buffer, buffer_size,
                                "ADC -> PROCESSING -> DAC");
    return 0;
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

int SubbandUI_RemainingSeconds(unsigned long learn_hops,
                               unsigned long target_hops,
                               unsigned long hop_samples,
                               unsigned long sample_rate)
{
    unsigned long remaining_ms;
    unsigned long seconds;

    remaining_ms = SubbandUI_RemainingMs(learn_hops, target_hops,
                                         hop_samples, sample_rate);
    if (remaining_ms == 0UL)
    {
        return 0;
    }
    seconds = (remaining_ms + 999UL) / 1000UL;
    if (seconds > 2UL)
    {
        seconds = 2UL;
    }
    return (int)seconds;
}

SubbandUILearningDisplayJob SubbandUI_SelectLearningDisplayJob(
    int mode_uses_learning,
    int last_mode_uses_learning,
    int learning,
    int last_learning,
    int ready,
    int last_ready,
    int remaining_seconds,
    int last_remaining_seconds)
{
    if ((mode_uses_learning != last_mode_uses_learning) ||
        (learning != last_learning) ||
        (ready != last_ready))
    {
        return SUBBAND_UI_LEARNING_DRAW_STATE;
    }
    if ((mode_uses_learning != 0) && (learning != 0) &&
        (remaining_seconds > 0) &&
        (remaining_seconds != last_remaining_seconds))
    {
        return SUBBAND_UI_LEARNING_DRAW_REMAINING_DIGIT;
    }
    return SUBBAND_UI_LEARNING_DRAW_NONE;
}

#if defined(SUBBAND_UI_HOST_TEST)
static void SubbandUI_LearningSchedulerCancelDigit(
    SubbandUILearningSchedulerState *state)
{
    if ((state->dirty_flags & SUBBAND_UI_LEARNING_DIRTY_DIGIT) != 0UL)
    {
        state->dirty_flags &= ~SUBBAND_UI_LEARNING_DIRTY_DIGIT;
        state->cancelled_digit_jobs++;
    }
}

void SubbandUI_LearningSchedulerInit(
    SubbandUILearningSchedulerState *state)
{
    if (state == 0)
    {
        return;
    }
    state->dirty_flags = 0UL;
    state->last_mode_uses_learning = -1;
    state->last_learning = -1;
    state->last_ready = -1;
    state->last_remaining_seconds = -1;
    state->displayed_mode_uses_learning = -1;
    state->displayed_learning = -1;
    state->displayed_ready = -1;
    state->displayed_remaining_seconds = -1;
    state->cancelled_digit_jobs = 0UL;
}

void SubbandUI_LearningSchedulerObserve(
    SubbandUILearningSchedulerState *state,
    int mode_uses_learning,
    int learning,
    int ready,
    int remaining_seconds)
{
    SubbandUILearningDisplayJob job;

    if (state == 0)
    {
        return;
    }
    job = SubbandUI_SelectLearningDisplayJob(
        mode_uses_learning, state->last_mode_uses_learning,
        learning, state->last_learning,
        ready, state->last_ready,
        remaining_seconds, state->last_remaining_seconds);
    if (job == SUBBAND_UI_LEARNING_DRAW_STATE)
    {
        SubbandUI_LearningSchedulerCancelDigit(state);
        state->dirty_flags |= SUBBAND_UI_LEARNING_DIRTY_STATE;
        state->last_mode_uses_learning = mode_uses_learning;
        state->last_learning = learning;
        state->last_ready = ready;
        state->last_remaining_seconds = remaining_seconds;
    }
    else if (job == SUBBAND_UI_LEARNING_DRAW_REMAINING_DIGIT)
    {
        state->dirty_flags |= SUBBAND_UI_LEARNING_DIRTY_DIGIT;
        state->last_remaining_seconds = remaining_seconds;
    }
}

SubbandUILearningDisplayJob SubbandUI_LearningSchedulerNextJob(
    const SubbandUILearningSchedulerState *state)
{
    if (state == 0)
    {
        return SUBBAND_UI_LEARNING_DRAW_NONE;
    }
    if ((state->dirty_flags & SUBBAND_UI_LEARNING_DIRTY_STATE) != 0UL)
    {
        return SUBBAND_UI_LEARNING_DRAW_STATE;
    }
    if ((state->dirty_flags & SUBBAND_UI_LEARNING_DIRTY_DIGIT) != 0UL)
    {
        return SUBBAND_UI_LEARNING_DRAW_REMAINING_DIGIT;
    }
    return SUBBAND_UI_LEARNING_DRAW_NONE;
}

SubbandUILearningDisplayJob SubbandUI_LearningSchedulerExecuteNext(
    SubbandUILearningSchedulerState *state)
{
    SubbandUILearningDisplayJob job;

    if (state == 0)
    {
        return SUBBAND_UI_LEARNING_DRAW_NONE;
    }
    job = SubbandUI_LearningSchedulerNextJob(state);
    if (job == SUBBAND_UI_LEARNING_DRAW_STATE)
    {
        state->dirty_flags &= ~SUBBAND_UI_LEARNING_DIRTY_STATE;
        SubbandUI_LearningSchedulerCancelDigit(state);
        state->displayed_mode_uses_learning =
            state->last_mode_uses_learning;
        state->displayed_learning = state->last_learning;
        state->displayed_ready = state->last_ready;
        state->displayed_remaining_seconds =
            state->last_remaining_seconds;
        return job;
    }
    if (job == SUBBAND_UI_LEARNING_DRAW_REMAINING_DIGIT)
    {
        state->dirty_flags &= ~SUBBAND_UI_LEARNING_DIRTY_DIGIT;
        if ((state->last_mode_uses_learning == 0) ||
            (state->last_learning == 0) ||
            (state->last_ready != 0) ||
            (state->last_remaining_seconds <= 0) ||
            (state->displayed_mode_uses_learning == 0) ||
            (state->displayed_learning == 0) ||
            (state->displayed_ready != 0) ||
            (state->displayed_remaining_seconds ==
             state->last_remaining_seconds))
        {
            state->cancelled_digit_jobs++;
            return SUBBAND_UI_LEARNING_DRAW_NONE;
        }
        state->displayed_remaining_seconds =
            state->last_remaining_seconds;
        return job;
    }
    return SUBBAND_UI_LEARNING_DRAW_NONE;
}
#endif

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
