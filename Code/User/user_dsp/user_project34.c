#include "user_project34_flow.h"

#if defined(DSP_LAB_PROJECT_SELECT) && (DSP_LAB_PROJECT_SELECT == 34)

#include "user_project34_internal.h"

static const short *Project3_GetCompletedMusicInputLeft(unsigned char completed_buffer)
{
    return (completed_buffer == AD_BUFFER_PING) ? AD_CH3_Buf0 : AD_CH3_Buf1;
}

static const short *Project3_GetCompletedMusicInputRight(unsigned char completed_buffer)
{
    return (completed_buffer == AD_BUFFER_PING) ? AD_CH4_Buf0 : AD_CH4_Buf1;
}

static short *Project3_GetInactiveMusicOutput(unsigned char active_output,
                                              unsigned char *output_buffer)
{
    if (active_output == DA_BUFFER_PONG) {
        *output_buffer = DA_BUFFER_PING;
        return DA_CH2_Buf0;
    }

    *output_buffer = DA_BUFFER_PONG;
    return DA_CH2_Buf1;
}

static void Project3_RealtimeMusicCallback(unsigned char completed_buffer,
                                           unsigned int sample_len)
{
#if PROJECT3_ENABLE_REALTIME_MUSIC
    const short *src_left;
    const short *src_right;
    const short *mic_src;
    const short *mic2_src;
    short *dst;
    unsigned int count;
    unsigned int i;
    unsigned int peak = 0u;
    unsigned int mic_peak = 0u;
    unsigned int mic2_peak = 0u;
    unsigned int music_left_peak = 0u;
    unsigned int music_right_peak = 0u;
    unsigned int cycle_start;
    unsigned int elapsed_cycles;
    unsigned char active_output;
    unsigned char output_buffer;
    int start_gain;
    int target_gain;
    int gain_accumulator;
    int gain_step;
    unsigned long adc_source_sequence;
#if PROJECT3_ENABLE_CAPTURE_QUEUE
    volatile short *capture_mic_dst;
    volatile short *capture_ref_dst;
    unsigned long capture_write_sequence;
    unsigned long capture_read_sequence;
    volatile unsigned long capture_readback;
    unsigned int capture_slot;
    unsigned int capture_depth;
    unsigned char capture_slot_available;
#endif

    /* Keep a real ADC-time sequence even when the foreground is blocked by
     * inference.  It also drives display hold time independently of backlog
     * consumption. */
    adc_source_sequence = g_project3_adc_completed_sequence + 1u;
    g_project3_adc_completed_buffer = completed_buffer;
    g_project3_adc_completed_sequence = adc_source_sequence;

    cycle_start = TSCL;
    count = sample_len;
    if (count > PROJECT3_BLOCK_SAMPLES) {
        count = PROJECT3_BLOCK_SAMPLES;
    }
    if (count == 0u) {
        return;
    }

    /* DAC starts 1 ms before ADC to keep this write far from the next DAC
     * boundary.  Select the inactive DAC buffer dynamically as well, so an
     * unusual startup phase cannot leave the music path permanently muted. */
    active_output = DA_Ping_Pong;
    /* LINE1 shares ADC CH1/CH2 with MIC1/MIC2 and mechanically disconnects
     * those microphones when a cable is inserted.  Keep LINE1 empty for KWS
     * and take stereo music from the independent LINE2 pair, ADC CH3/CH4. */
    src_left = Project3_GetCompletedMusicInputLeft(completed_buffer);
    src_right = Project3_GetCompletedMusicInputRight(completed_buffer);
    mic_src = (completed_buffer == AD_BUFFER_PING) ? AD_CH1_Buf0 : AD_CH1_Buf1;
    mic2_src = (completed_buffer == AD_BUFFER_PING) ? AD_CH2_Buf0 : AD_CH2_Buf1;
    dst = Project3_GetInactiveMusicOutput(active_output, &output_buffer);

#if PROJECT3_ENABLE_CAPTURE_QUEUE
    /* SPSC reserve: the interrupt only advances write_sequence; the
     * foreground only advances read_sequence.  When full, keep the queued
     * chronological data and drop this newest block. */
    capture_write_sequence = g_project3_capture_write_sequence;
    capture_read_sequence = g_project3_capture_read_sequence;
    capture_slot_available =
        ((capture_write_sequence - capture_read_sequence) <
         PROJECT3_CAPTURE_QUEUE_SLOTS) ? 1u : 0u;
    capture_slot = (unsigned int)(capture_write_sequence &
                                  PROJECT3_CAPTURE_QUEUE_MASK);
    capture_mic_dst = g_project3_capture_mic[capture_slot];
    capture_ref_dst = g_project3_capture_ref[capture_slot];
    if (!capture_slot_available) {
        g_project3_capture_overflow_count++;
        g_project3_adc_drop_count++;
        g_project3_capture_queue_depth = PROJECT3_CAPTURE_QUEUE_SLOTS;
    }
#endif

    /* C7000000-C707FFFF is configured non-cacheable in main(), so this ISR
     * sees fresh EDMA input and PRU0 sees every output write without cache
     * maintenance inside interrupt context.  Gain ramps over one block to
     * avoid clicks on on/off and up/down transitions. */
    start_gain = s_project3_music_applied_gain_q15;
    target_gain = g_project3_music_playing ?
        (int)g_project3_music_user_gain_q15 : 0;
    gain_accumulator = start_gain * 65536;
    gain_step = ((target_gain - start_gain) * 65536) / (int)count;

    for (i = 0u; i < count; i++) {
        int left_sample = (int)src_left[i];
        int right_sample = (int)src_right[i];
        int input_sample = (left_sample + right_sample) / 2;
        int mic_sample = (int)mic_src[i];
        int mic2_sample = (int)mic2_src[i];
        int magnitude = (input_sample < 0) ? -input_sample : input_sample;
        int mic_magnitude = (mic_sample < 0) ? -mic_sample : mic_sample;
        int mic2_magnitude = (mic2_sample < 0) ? -mic2_sample : mic2_sample;
        int left_magnitude = (left_sample < 0) ? -left_sample : left_sample;
        int right_magnitude = (right_sample < 0) ? -right_sample : right_sample;
        int gain = gain_accumulator / 65536;
        int output_sample = (input_sample * gain) / 32768;

#if PROJECT3_ENABLE_CAPTURE_QUEUE
        if (capture_slot_available) {
            capture_mic_dst[i] = (short)mic_sample;
            capture_ref_dst[i] = (short)input_sample;
        }
#endif

        if ((unsigned int)magnitude > peak) {
            peak = (unsigned int)magnitude;
        }
        if ((unsigned int)mic_magnitude > mic_peak) {
            mic_peak = (unsigned int)mic_magnitude;
        }
        if ((unsigned int)mic2_magnitude > mic2_peak) {
            mic2_peak = (unsigned int)mic2_magnitude;
        }
        if ((unsigned int)left_magnitude > music_left_peak) {
            music_left_peak = (unsigned int)left_magnitude;
        }
        if ((unsigned int)right_magnitude > music_right_peak) {
            music_right_peak = (unsigned int)right_magnitude;
        }
        if (output_sample > 32767) {
            output_sample = 32767;
        } else if (output_sample < -32768) {
            output_sample = -32768;
        }
        dst[i] = (short)output_sample;
        gain_accumulator += gain_step;
    }
    for (; i < PROJECT3_BLOCK_SAMPLES; i++) {
        dst[i] = 0;
#if PROJECT3_ENABLE_CAPTURE_QUEUE
        if (capture_slot_available) {
            capture_mic_dst[i] = 0;
            capture_ref_dst[i] = 0;
        }
#endif
    }

#if PROJECT3_ENABLE_CAPTURE_QUEUE
    if (capture_slot_available) {
        g_project3_capture_sample_count[capture_slot] = count;
        g_project3_capture_source_sequence[capture_slot] = adc_source_sequence;
        /* All slot fields are volatile and uncached.  Publish write_sequence
         * only after a same-region readback drains posted DDR writes. */
        capture_readback =
            g_project3_capture_source_sequence[capture_slot];
        (void)capture_readback;
        g_project3_capture_write_sequence = capture_write_sequence + 1u;
        capture_depth = (unsigned int)
            ((capture_write_sequence + 1u) - capture_read_sequence);
        if (capture_depth > PROJECT3_CAPTURE_QUEUE_SLOTS) {
            capture_depth = PROJECT3_CAPTURE_QUEUE_SLOTS;
        }
        g_project3_capture_queue_depth = capture_depth;
        if (capture_depth > g_project3_capture_queue_high_water) {
            g_project3_capture_queue_high_water = capture_depth;
        }
    }
#endif

    s_project3_music_applied_gain_q15 = target_gain;
    g_project3_music_input_peak = (peak > 32767u) ? 32767u : peak;
    g_project3_adc_ch1_peak = (mic_peak > 32767u) ? 32767u : mic_peak;
    g_project3_adc_ch2_peak = (mic2_peak > 32767u) ? 32767u : mic2_peak;
    g_project3_adc_ch3_peak =
        (music_left_peak > 32767u) ? 32767u : music_left_peak;
    g_project3_adc_ch4_peak =
        (music_right_peak > 32767u) ? 32767u : music_right_peak;
    /* A change here means PRU0 crossed a block boundary while the ISR was
     * writing.  The next callback automatically selects the new inactive
     * buffer; expose the event for board-level timing validation. */
    if (DA_Ping_Pong != active_output) {
        g_project3_music_sync_miss_count++;
    }
    g_project3_music_last_output_buffer = output_buffer;
    g_project3_music_block_count++;
    elapsed_cycles = TSCL - cycle_start;
    g_project3_music_last_isr_cycles = elapsed_cycles;
    if (elapsed_cycles > g_project3_music_max_isr_cycles) {
        g_project3_music_max_isr_cycles = elapsed_cycles;
    }
#else
    (void)completed_buffer;
    (void)sample_len;
#endif
}

#if PROJECT3_ENABLE_REALTIME_MUSIC && PROJECT3_ENABLE_CAPTURE_QUEUE
static unsigned char Project3_PopCapturedBlock(short *mic,
                                                short *music_ref,
                                                unsigned int *sample_count,
                                                unsigned long *source_sequence)
{
    unsigned long read_sequence;
    unsigned long write_sequence;
    unsigned long next_read_sequence;
    unsigned int slot;
    unsigned int count;
    unsigned int depth;
    unsigned int i;

    read_sequence = g_project3_capture_read_sequence;
    write_sequence = g_project3_capture_write_sequence;
    if (read_sequence == write_sequence) {
        g_project3_capture_queue_depth = 0u;
        return 0u;
    }

    /* write_sequence is the producer's volatile publication marker. */
    slot = (unsigned int)(read_sequence & PROJECT3_CAPTURE_QUEUE_MASK);
    count = g_project3_capture_sample_count[slot];
    if (count > PROJECT3_BLOCK_SAMPLES) {
        count = PROJECT3_BLOCK_SAMPLES;
    }
    for (i = 0u; i < count; i++) {
        mic[i] = g_project3_capture_mic[slot][i];
        music_ref[i] = g_project3_capture_ref[slot][i];
    }
    *sample_count = count;
    *source_sequence = g_project3_capture_source_sequence[slot];

    /* Release this slot only after both private foreground copies finish. */
    next_read_sequence = read_sequence + 1u;
    g_project3_capture_read_sequence = next_read_sequence;
    write_sequence = g_project3_capture_write_sequence;
    depth = (unsigned int)(write_sequence - next_read_sequence);
    if (depth > PROJECT3_CAPTURE_QUEUE_SLOTS) {
        depth = PROJECT3_CAPTURE_QUEUE_SLOTS;
    }
    g_project3_capture_queue_depth = depth;
    return 1u;
}

static void Project3_HandleCaptureSequence(PROJECT3_CONTEXT *ctx,
                                           unsigned long source_sequence)
{
    unsigned long expected_sequence;
    unsigned int b;

    if (g_project3_capture_last_source_sequence != 0u) {
        expected_sequence = g_project3_capture_last_source_sequence + 1u;
        if (source_sequence != expected_sequence) {
            /* Never join speech from opposite sides of a dropped interval. */
            g_project3_capture_gap_reset_count++;
            Project3_ResetUtterance(ctx);
            Project3_ResetPreroll();
            ctx->input_gate_blocks = 0u;
            ctx->vad.smooth_energy = ctx->vad.noise_floor;
            ctx->vad.speech_score = 0.0f;
            ctx->vad.low_fraction = 0.0f;
            ctx->vad.active_bands = 0u;
            for (b = 0u; b < PROJECT3_VAD_BAND_COUNT; b++) {
                ctx->vad.band_smooth[b] = ctx->vad.band_noise[b];
                g_project3_vad_band_ratio_permille[b] = 0u;
            }
            g_project3_vad_smooth_energy = ctx->vad.noise_floor;
            g_project3_vad_speech_score_permille = 0u;
            g_project3_vad_low_fraction_permille = 0u;
            g_project3_vad_active_band_count = 0u;
            g_project3_last_voiced_frame_count = 0u;
            g_project3_last_periodicity_permille = 0u;
            if (ctx->app_state == PROJECT3_APP_SPEECH &&
                ctx->message_hold_blocks == 0u &&
                ctx->error_hold_blocks == 0u) {
                Project3_SetAppState(ctx, PROJECT3_APP_LISTENING);
                Project3_SetUiText(ctx, P3_LISTENING_TEXT, "", "");
            }
        }
    }

    g_project3_capture_last_source_sequence = source_sequence;
    g_project3_adc_consumed_sequence = source_sequence;
}
#endif

static void Project3_ApplyAcceptedMusicCommand(PROJECT3_CONTEXT *ctx,
                                               unsigned char class_id)
{
#if PROJECT3_ENABLE_REALTIME_MUSIC
    unsigned int level = g_project3_music_volume_level;
    unsigned char handled = 1u;

    switch (class_id) {
    case PROJECT3_CLASS_ON:
        g_project3_music_playing = 1u;
        break;
    case PROJECT3_CLASS_OFF:
        g_project3_music_playing = 0u;
        break;
    case PROJECT3_CLASS_UP:
        if (level < PROJECT3_MUSIC_VOLUME_LEVELS) {
            level++;
        }
        break;
    case PROJECT3_CLASS_DOWN:
        if (level > 0u) {
            level--;
        }
        break;
    default:
        handled = 0u;
        break;
    }

    if (handled) {
        g_project3_music_volume_level = (unsigned char)level;
        g_project3_music_volume_percent =
            (level * 100u) / PROJECT3_MUSIC_VOLUME_LEVELS;
        g_project3_music_user_gain_q15 =
            (level * PROJECT3_MUSIC_Q15_FULL_SCALE) /
            PROJECT3_MUSIC_VOLUME_LEVELS;
        g_project3_music_last_command = class_id;
        g_project3_music_command_count++;
        ctx->redraw_needed = 1u;
        ctx->lcd_retry_frame = 0u;
    }
#else
    (void)ctx;
    (void)class_id;
#endif
}

void Project34_Flow_Example(void)
{
    static PROJECT3_CONTEXT ctx;
    unsigned int i;
#if PROJECT3_ENABLE_REALTIME_MUSIC
#if PROJECT3_ENABLE_CAPTURE_QUEUE
    unsigned int captured_samples;
    unsigned long captured_source_sequence;
#else
    unsigned long ready_sequence;
    unsigned long confirm_sequence;
    unsigned long sequence_delta;
    unsigned char completed_buffer;
#endif
#endif

    Project3_InitContext(&ctx);

    Sys_Init();
#if PROJECT3_ENABLE_REALTIME_MUSIC
#if PROJECT3_ENABLE_CAPTURE_QUEUE
    /* C7 is one MAR window.  Flush every startup zero-fill before disabling
     * caching for both the DMA buffers and the adjacent capture queue. */
    CacheWBInv(PROJECT3_AUDIO_UNCACHED_BASE,
               PROJECT3_AUDIO_UNCACHED_BYTES);
    CacheDisableMAR(PROJECT3_AUDIO_UNCACHED_BASE,
                    PROJECT3_AUDIO_UNCACHED_BYTES);
#else
    CacheWBInv(PROJECT3_AUDIO_DMA_BASE, PROJECT3_AUDIO_DMA_BYTES);
    CacheDisableMAR(PROJECT3_AUDIO_DMA_BASE, PROJECT3_AUDIO_DMA_BYTES);
#endif
#endif
#if PROJECT3_ENABLE_KEY_CONTROLS
    Key_Init();
#endif
    Touch_Init();
    Project3_InitUi(&ctx);

    Adc_Init(PROJECT3_ADC_RATE, PROJECT3_BLOCK_SAMPLES);
    Dac_Init(PROJECT3_DAC_RATE, PROJECT3_BLOCK_SAMPLES, PROJECT3_DAC_CHANNEL_MASK);

    for (i = 0; i < PROJECT3_BLOCK_SAMPLES; i++) {
        DA_CH1_Buf0[i] = 0;
        DA_CH1_Buf1[i] = 0;
        DA_CH2_Buf0[i] = 0;
        DA_CH2_Buf1[i] = 0;
    }

#if PROJECT3_ENABLE_REALTIME_MUSIC
    Adc_RegisterBlockCallback(Project3_RealtimeMusicCallback);
#endif

    Project3_ModelInit(&ctx);
    Project3_UpdateUi(&ctx, 1);

    Dac_Start();
#if PROJECT3_ENABLE_REALTIME_MUSIC
    /* Keep DAC one millisecond ahead of ADC.  Their equal-rate timer halves
     * then preserve a safe ping/pong phase for the interrupt-time bridge. */
    delay(1u);
#endif
    Adc_Start();

    while (1) {
        Project3_HandleKeys(&ctx);
        Project3_HandleTouch(&ctx);

#if PROJECT3_ENABLE_REALTIME_MUSIC
#if PROJECT3_ENABLE_CAPTURE_QUEUE
        if (Project3_PopCapturedBlock(g_project3_input_block,
                                      g_project3_output_block,
                                      &captured_samples,
                                      &captured_source_sequence)) {
            Project3_HandleCaptureSequence(&ctx, captured_source_sequence);
            Project3_SuppressMusicReference(g_project3_input_block,
                                            g_project3_output_block,
                                            captured_samples);
            FLAG_AD = 0;
            Project3_ProcessAudioBlock(&ctx, g_project3_input_block,
                                       captured_samples);
            FLAG_DA = 0;
        }
#else
        do {
            ready_sequence = g_project3_adc_completed_sequence;
            completed_buffer = g_project3_adc_completed_buffer;
            confirm_sequence = g_project3_adc_completed_sequence;
        } while (ready_sequence != confirm_sequence);

        if (ready_sequence != g_project3_adc_consumed_sequence) {
            sequence_delta = ready_sequence - g_project3_adc_consumed_sequence;
            if (sequence_delta > 1u) {
                g_project3_adc_drop_count += sequence_delta - 1u;
            }
            Project3_CopyInputBlock(g_project3_input_block, completed_buffer);
            g_project3_adc_consumed_sequence = ready_sequence;
            FLAG_AD = 0;
            Project3_ProcessAudioBlock(&ctx, g_project3_input_block, PROJECT3_BLOCK_SAMPLES);
            FLAG_DA = 0;
        }
#endif
#else
        if (FLAG_AD == 1) {
            unsigned char completed_buffer =
                (AD_Ping_Pong == AD_BUFFER_PONG) ? AD_BUFFER_PING : AD_BUFFER_PONG;
            FLAG_AD = 0;
            Project3_CopyInputBlock(g_project3_input_block, completed_buffer);
            Project3_ProcessAudioBlock(&ctx, g_project3_input_block, PROJECT3_BLOCK_SAMPLES);
            Project3_FillDacOutput(&ctx, g_project3_input_block);
            Project3_WriteOutputBlockToDac(g_project3_output_block);
            FLAG_DA = 0;
        }
#endif

        if (ctx.redraw_needed && !s_lcd_busy) {
            Project3_UpdateUi(&ctx, 0);
        }

        Lcd_ServiceRasterHealth();
    }
}

#endif
