#ifndef _USER_PROJECT34_INTERNAL_H_
#define _USER_PROJECT34_INTERNAL_H_

#include "driver_include.h"
#include "user_include.h"
#include "user_project34_weights.h"
#include "raster.h"
#include "soc_C6748.h"
#include "lcd_dma.h"
#include "lcd_grlib.h"
#include "DSPF_sp_fftSPxSP.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

// ============================================================
// 固定布局宏定义 - 防偏移专用
// ============================================================
#define P3_LCD_W                800
#define P3_LCD_H                480
#define P3_TEXT_AREA_X          80
#define P3_TEXT_AREA_Y          135
#define P3_TEXT_AREA_W          640
#define P3_TEXT_AREA_H          210

// 坐标裁剪宏
#define P3_CLAMP_X(x)           (((x) < 0) ? 0 : (((x) >= P3_LCD_W) ? (P3_LCD_W - 1) : (x)))
#define P3_CLAMP_Y(y)           (((y) < 0) ? 0 : (((y) >= P3_LCD_H) ? (P3_LCD_H - 1) : (y)))

// LCD刷新控制
#define P3_LCD_BUFFER_BYTES     (4 + 32 + P3_LCD_W * P3_LCD_H * 2)
#define P3_LCD_PIXEL_OFFSET     (4 + 32)
#define P3_CACHE_LINE_BYTES     128u
#define P3_TEXT_BAND_X          P3_TEXT_AREA_X
#define P3_TEXT_BAND_Y          P3_TEXT_AREA_Y
#define P3_TEXT_BAND_W          P3_TEXT_AREA_W
#define P3_TEXT_BAND_H          P3_TEXT_AREA_H
#define P3_DYNAMIC_X            24
#define P3_DYNAMIC_Y            64
#define P3_DYNAMIC_W            (P3_TEXT_BAND_W - 48)
#define P3_DYNAMIC_H            122
#define P3_TEXT_BUFFER_BYTES    (4 + 32 + P3_TEXT_BAND_W * P3_TEXT_BAND_H * 2)
#define P3_LISTENING_TEXT       "Listening..."
#define P3_FONT_W               5
#define P3_FONT_H               7
#define P3_FONT_GAP             1
#define P3_RGB565_BLACK         0x0000u
#define P3_RGB565_WHITE         0xFFFFu
#define P3_RGB565_PANEL         0x0862u
#define P3_RGB565_PANEL_INNER   0x10C4u
#define P3_RGB565_BORDER        0x2E7Fu
#define P3_RGB565_MUTED         0x8410u
#define P3_RGB565_IDLE          0x9E7Fu
#define P3_RGB565_RESULT        0xFFFFu
#define PROJECT3_LCD_PAUSE_DURING_INFERENCE 0

#define PROJECT3_PI                         3.14159265358979323846f

/* Hardware stream: use 20 kHz ADC, then decimate 5 -> 4 to match the PC training rate of 16 kHz. */
#define PROJECT3_ENABLE_REALTIME_MUSIC       1
#define PROJECT3_ADC_RATE                   ADC_20KHZ
#define PROJECT3_DAC_RATE                   DAC_20KHZ
#define PROJECT3_BLOCK_SAMPLES              ADC_SAMPLE_1024
#if PROJECT3_ENABLE_REALTIME_MUSIC
#define PROJECT3_DAC_CHANNEL_MASK           DAC_CHANNEL_2
#else
#define PROJECT3_DAC_CHANNEL_MASK           DAC_CHANNEL_1
#endif
#define PROJECT3_HW_SAMPLE_RATE             20000
#define PROJECT3_MODEL_SAMPLE_RATE          16000
#define PROJECT3_DSP_CLOCK_HZ               456000000u

/* ADC CH1 remains the microphone/KWS input.  The LINE1 stereo jack shares
 * CH1/CH2 with MIC1/MIC2 and disconnects those microphones when inserted, so
 * live music must use the independent LINE2 pair on ADC CH3/CH4.  Music is
 * downmixed to mono and routed to DAC CH2.  The fixed AD/DA DMA window is
 * uncached so the interrupt bridge remains coherent during inference. */
#define PROJECT3_AUDIO_DMA_BASE             0xC7000000u
#define PROJECT3_AUDIO_DMA_BYTES            0x00080000u
#define PROJECT3_ENABLE_CAPTURE_QUEUE        1u
#define PROJECT3_CAPTURE_QUEUE_SLOTS         32u
#define PROJECT3_CAPTURE_QUEUE_MASK          (PROJECT3_CAPTURE_QUEUE_SLOTS - 1u)
#define PROJECT3_CAPTURE_QUEUE_BASE          0xC7080000u
#define PROJECT3_CAPTURE_QUEUE_REGION_BYTES  0x00040000u
#define PROJECT3_AUDIO_UNCACHED_BASE         PROJECT3_AUDIO_DMA_BASE
#define PROJECT3_AUDIO_UNCACHED_BYTES        0x000C0000u
#if (PROJECT3_CAPTURE_QUEUE_SLOTS == 0u) || \
    ((PROJECT3_CAPTURE_QUEUE_SLOTS & PROJECT3_CAPTURE_QUEUE_MASK) != 0u)
#error PROJECT3_CAPTURE_QUEUE_SLOTS_must_be_a_power_of_two
#endif
#define PROJECT3_MUSIC_VOLUME_LEVELS        10u
#define PROJECT3_MUSIC_DEFAULT_LEVEL        5u
#define PROJECT3_MUSIC_Q15_FULL_SCALE       32767u

/* The LINE2 CH3/CH4 downmix is also used as a synchronous music reference for
 * the CH1 microphone stream.  A least-squares projection removes only the
 * component of CH1 correlated with the electrical music input; uncorrelated
 * speech is retained. */
#define PROJECT3_ENABLE_MUSIC_REFERENCE      1u
#define PROJECT3_MUSIC_REF_ON_PEAK           256u
#define PROJECT3_MUSIC_REF_OFF_PEAK          128u
#define PROJECT3_MUSIC_REF_MIN_CORR_SQ        0.0625f
#define PROJECT3_MUSIC_REF_MAX_ABS_GAIN       2.0f

/* One command is normalized to the same 1-second waveform used by train_bcresnet.py. */
#define PROJECT3_RAW_MAX_SAMPLES            (PROJECT3_HW_SAMPLE_RATE * 13 / 10)
#define PROJECT3_MODEL_SAMPLES              16000
#define PROJECT3_VAD_FRAME_LEN              400
#define PROJECT3_VAD_HOP                    200
#define PROJECT3_PREROLL_SAMPLES            (PROJECT3_HW_SAMPLE_RATE / 10)
#define PROJECT3_WIN_SIZE                   480
#define PROJECT3_HOP_SIZE                   160
#define PROJECT3_FFT_LEN                    512
#define PROJECT3_FREQ_NUM                   (PROJECT3_FFT_LEN / 2 + 1)
#define PROJECT3_DSPLIB_TWIDDLE_FLOATS      (2 * PROJECT3_FFT_LEN)
#define PROJECT3_FFT_VALIDATION_MAX_PPM     5000u
#define PROJECT3_MELS_NUM                   40
#define PROJECT3_MODEL_FRAMES               101
#define PROJECT3_CMD_COUNT                  12
#define PROJECT3_EMBEDDING_SIZE             64
#define PROJECT3_ENABLE_MODEL_SPEECH_GATE    1u
/* Validation-calibrated two-zone gate: very speech-like events pass directly;
 * borderline events must also have a confident command prediction. */
#define PROJECT3_MODEL_SPEECH_LOW_THRESHOLD  0.95f
#define PROJECT3_MODEL_SPEECH_HIGH_THRESHOLD 0.99f
#define PROJECT3_MODEL_SPEECH_MIN_CONFIDENCE 0.60f
#define PROJECT3_FEATURE_COUNT              (PROJECT3_MELS_NUM * PROJECT3_MODEL_FRAMES)

#define PROJECT3_UI_TEXT_LEN                64
#define PROJECT3_RESULT_TEXT_LEN            32
#define PROJECT3_PASS_THROUGH_ENABLE        0
#define PROJECT3_ENABLE_KEY_CONTROLS        0
#define PROJECT3_INFERENCE_CONF_THRESHOLD     0.55f
#define PROJECT3_INFERENCE_MARGIN_THRESHOLD   0.12f
#define PROJECT3_UNKNOWN_COMPETITOR_THRESHOLD 0.25f
#define PROJECT3_UNKNOWN_COMMAND_OVERRIDE     0.65f
#define PROJECT3_FEATURE_NORM_EPS             1.0e-6f
#define PROJECT3_WAVE_TARGET_RMS            0.10f
#define PROJECT3_WAVE_MAX_GAIN              16.0f
#define PROJECT3_WAVE_PEAK_LIMIT            0.98f
#define PROJECT3_WAVE_MIN_RMS               1.0e-5f
#define PROJECT3_MIN_UTTERANCE_SAMPLES      (PROJECT3_HW_SAMPLE_RATE / 5)
#define PROJECT3_MIN_ACTIVE_SPEECH_SAMPLES  (PROJECT3_HW_SAMPLE_RATE * 11 / 50)
#define PROJECT3_VAD_ABS_START_ENERGY       2.5e-6f
#define PROJECT3_POST_INFER_IGNORE_BLOCKS   4
#define PROJECT3_MESSAGE_HOLD_BLOCKS        20
#define PROJECT3_RESULT_HOLD_MS             750u
#define PROJECT3_RESULT_HOLD_BLOCKS         \
    ((PROJECT3_HW_SAMPLE_RATE * PROJECT3_RESULT_HOLD_MS + \
      PROJECT3_BLOCK_SAMPLES * 1000u - 1u) / \
     (PROJECT3_BLOCK_SAMPLES * 1000u))
#define PROJECT3_ERROR_HOLD_BLOCKS          50
#define PROJECT3_VAD_CALIB_FRAMES           40
#define PROJECT3_VAD_MIN_FLOOR              1.0e-8f
#define PROJECT3_VAD_BAND_COUNT              5u
#define PROJECT3_VAD_BAND_FLOOR              1.0e-10f
#define PROJECT3_VAD_BAND_SMOOTH_ALPHA       0.80f
#define PROJECT3_VAD_ACTIVE_BAND_RATIO       2.00f
#define PROJECT3_VAD_LOW_DOMINANCE_LIMIT     0.76f
#define PROJECT3_SPEECH_CHECK_FRAME_LEN      400u
#define PROJECT3_SPEECH_CHECK_HOP            200u
#define PROJECT3_SPEECH_PITCH_LAG_MIN        63u
#define PROJECT3_SPEECH_PITCH_LAG_MAX        250u
#define PROJECT3_SPEECH_PITCH_LAG_STEP       5u
#define PROJECT3_SPEECH_PERIODICITY_SQ       0.0784f
#define PROJECT3_SPEECH_REQUIRED_FRAMES      2u
#define PROJECT3_NONSPEECH_IGNORE_BLOCKS     4u
#define PROJECT3_ENABLE_SPEECHLIKE_VETO       1u

#define PROJECT3_CENTER_X                   400
#define PROJECT3_CENTER_Y                   215
#define PROJECT3_CENTER_W                   520
#define PROJECT3_CENTER_H                   150

#define PROJECT3_BN_EPS                     1.0e-5f
#define PROJECT3_ACT_A_MAX                  (16 * 20 * PROJECT3_MODEL_FRAMES)
#define PROJECT3_ACT_B_MAX                  (8 * 20 * PROJECT3_MODEL_FRAMES)
#define PROJECT3_ACT_C_MAX                  (8 * 20 * PROJECT3_MODEL_FRAMES)
#define PROJECT3_GUARD_WORDS                64
#define PROJECT3_GUARD_BASE                 0x5A17C0DEu
#define PROJECT3_GUARD_STEP                 0x00100193u

typedef enum {
    PROJECT3_APP_BOOT = 0,
    PROJECT3_APP_LISTENING,
    PROJECT3_APP_SPEECH,
    PROJECT3_APP_INFERENCING,
    PROJECT3_APP_RESULT,
    PROJECT3_APP_ERROR
} PROJECT3_APP_STATE;

typedef enum {
    PROJECT3_MODEL_READY = 0
} PROJECT3_MODEL_STATE;

typedef enum {
    PROJECT3_CLASS_SILENCE = 0,
    PROJECT3_CLASS_UNKNOWN = 1,
    PROJECT3_CLASS_DOWN = 2,
    PROJECT3_CLASS_GO = 3,
    PROJECT3_CLASS_LEFT = 4,
    PROJECT3_CLASS_NO = 5,
    PROJECT3_CLASS_OFF = 6,
    PROJECT3_CLASS_ON = 7,
    PROJECT3_CLASS_RIGHT = 8,
    PROJECT3_CLASS_STOP = 9,
    PROJECT3_CLASS_UP = 10,
    PROJECT3_CLASS_YES = 11
} PROJECT3_CLASS_ID;

typedef struct {
    float total_energy;
    float band_energy[PROJECT3_VAD_BAND_COUNT];
    float low_fraction;
} PROJECT3_VAD_FEATURES;

typedef struct {
    float noise_floor;
    float smooth_energy;
    float band_noise[PROJECT3_VAD_BAND_COUNT];
    float band_smooth[PROJECT3_VAD_BAND_COUNT];
    float band_deviation[PROJECT3_VAD_BAND_COUNT];
    float speech_score;
    float low_fraction;
    unsigned short speech_hold_frames;
    unsigned short silence_hold_frames;
    unsigned short calibrate_frames;
    unsigned char active_bands;
    unsigned char active;
} PROJECT3_VAD_STATE;

typedef struct {
    short raw[PROJECT3_RAW_MAX_SAMPLES];
    unsigned int count;
    unsigned char ready;
} PROJECT3_UTTERANCE_BUFFER;

typedef struct {
    unsigned char class_id;
    unsigned char second_class_id;
    float confidence;
    float second_confidence;
    float margin;
    unsigned char valid;
} PROJECT3_INFER_RESULT;

typedef struct {
    float logits[PROJECT3_CMD_COUNT];
    volatile unsigned int guard[PROJECT3_GUARD_WORDS];
} PROJECT3_LOGITS_STORAGE;

typedef struct {
    PROJECT3_APP_STATE app_state;
    PROJECT3_MODEL_STATE model_state;
    PROJECT3_VAD_STATE vad;
    PROJECT3_UTTERANCE_BUFFER utter;
    PROJECT3_INFER_RESULT last_result;
    char main_text[PROJECT3_RESULT_TEXT_LEN];
    char line1[PROJECT3_UI_TEXT_LEN];
    char line2[PROJECT3_UI_TEXT_LEN];
    char last_main_text[PROJECT3_RESULT_TEXT_LEN];
    unsigned int last_music_volume_percent;
    unsigned char last_music_playing;
    char last_line1[PROJECT3_UI_TEXT_LEN];
    char last_line2[PROJECT3_UI_TEXT_LEN];
    char stable_text[PROJECT3_RESULT_TEXT_LEN];
    unsigned long recognized_count;
    unsigned long frame_counter;
    unsigned long lcd_retry_frame;
    unsigned long lcd_fault_count;
    unsigned long lcd_last_redraw_frame;
    unsigned int active_speech_samples;
    unsigned char pass_through_enable;
    unsigned char input_gate_blocks;
    unsigned char message_hold_blocks;
    unsigned long message_hold_start_sequence;
    unsigned char message_return_to_listening;
    unsigned char error_hold_blocks;
    unsigned char redraw_needed;
    unsigned char stable_text_valid;
    unsigned char music_ref_last_active;
} PROJECT3_CONTEXT;

// LCD防重入标志
static volatile int s_lcd_busy = 0;
static volatile int s_lcd_infer_paused = 0;
static unsigned char s_lcd_layout_ready = 0u;

static void Project3_InitContext(PROJECT3_CONTEXT *ctx);
static void Project3_InitUi(PROJECT3_CONTEXT *ctx);
static void Project3_RealtimeMusicCallback(unsigned char completed_buffer,
                                           unsigned int sample_len);
static void Project3_ApplyAcceptedMusicCommand(PROJECT3_CONTEXT *ctx,
                                               unsigned char class_id);
static void Project3_HandleKeys(PROJECT3_CONTEXT *ctx);
static void Project3_HandleTouch(PROJECT3_CONTEXT *ctx);
static void Project3_ProcessAudioBlock(PROJECT3_CONTEXT *ctx, short *block, unsigned int block_samples);
static void Project3_UpdateUi(PROJECT3_CONTEXT *ctx, unsigned char force_redraw);
static void Project3_ModelInit(PROJECT3_CONTEXT *ctx);
static PROJECT3_INFER_RESULT Project3_RunInference(PROJECT3_CONTEXT *ctx, const PROJECT3_UTTERANCE_BUFFER *utter);
static const char *Project3_GetLabel(unsigned char class_id);
static void Project3_ResetUtterance(PROJECT3_CONTEXT *ctx);
static void Project3_ResetPreroll(void);

#define P3_IDX3(c, h, w, H, W)       ((((c) * (H)) + (h)) * (W) + (w))
#define P3_W4(o, i, kh, kw, IC, KH, KW) (((((o) * (IC)) + (i)) * (KH) + (kh)) * (KW) + (kw))
#define P3_DW(c, kh, kw, KH, KW)     (((c) * (KH) + (kh)) * (KW) + (kw))

static const char *g_project3_labels[PROJECT3_CMD_COUNT] = {
    "silence",
    "unknown",
    "down",
    "go",
    "left",
    "no",
    "off",
    "on",
    "right",
    "stop",
    "up",
    "yes"
};

#pragma DATA_ALIGN(g_project3_input_block, 8)
static short g_project3_input_block[PROJECT3_BLOCK_SAMPLES];

#pragma DATA_ALIGN(g_project3_preroll, 8)
static short g_project3_preroll[PROJECT3_PREROLL_SAMPLES];
static unsigned int g_project3_preroll_pos = 0;
static unsigned int g_project3_preroll_count = 0;

#pragma DATA_ALIGN(g_project3_output_block, 8)
static short g_project3_output_block[PROJECT3_BLOCK_SAMPLES];

#if PROJECT3_ENABLE_CAPTURE_QUEUE
/* The ISR and foreground share this SPSC queue.  It lives in a dedicated
 * uncached DDR window so capture traffic cannot evict model data from cache.
 * Each slot keeps the microphone and its synchronous electrical music
 * reference together; delayed processing must not reread ADC ping/pong. */
#pragma DATA_SECTION(g_project3_capture_mic, "project3_audio_queue")
#pragma DATA_ALIGN(g_project3_capture_mic, 128)
static volatile short g_project3_capture_mic[PROJECT3_CAPTURE_QUEUE_SLOTS]
                                             [PROJECT3_BLOCK_SAMPLES];

#pragma DATA_SECTION(g_project3_capture_ref, "project3_audio_queue")
#pragma DATA_ALIGN(g_project3_capture_ref, 128)
static volatile short g_project3_capture_ref[PROJECT3_CAPTURE_QUEUE_SLOTS]
                                             [PROJECT3_BLOCK_SAMPLES];

#pragma DATA_SECTION(g_project3_capture_source_sequence, "project3_audio_queue")
#pragma DATA_ALIGN(g_project3_capture_source_sequence, 128)
static volatile unsigned long
    g_project3_capture_source_sequence[PROJECT3_CAPTURE_QUEUE_SLOTS];

#pragma DATA_SECTION(g_project3_capture_sample_count, "project3_audio_queue")
#pragma DATA_ALIGN(g_project3_capture_sample_count, 128)
static volatile unsigned int
    g_project3_capture_sample_count[PROJECT3_CAPTURE_QUEUE_SLOTS];
#endif

#pragma DATA_ALIGN(g_project3_model_wave, 8)
static float g_project3_model_wave[PROJECT3_MODEL_SAMPLES];

#pragma DATA_ALIGN(g_project3_window, 8)
static float g_project3_window[PROJECT3_WIN_SIZE];

#pragma DATA_ALIGN(g_project3_mel_filter, 8)
static float g_project3_mel_filter[PROJECT3_FREQ_NUM * PROJECT3_MELS_NUM];

#pragma DATA_ALIGN(g_project3_logmel, 8)
static float g_project3_logmel[PROJECT3_MELS_NUM * PROJECT3_MODEL_FRAMES];

/* Feature extraction finishes before the CNN starts, so their temporary
 * workspaces have disjoint lifetimes.  Overlaying them puts the FFT's heavily
 * reused data in fast on-chip L2 without increasing the already tight 256 kB
 * L2 footprint or changing any arithmetic. */
typedef struct {
    float act_a[PROJECT3_ACT_A_MAX];
    float act_b[PROJECT3_ACT_B_MAX];
    float act_c[PROJECT3_ACT_C_MAX];
} PROJECT3_CNN_WORKSPACE;

typedef struct {
    float fft_re[PROJECT3_FFT_LEN];
    float fft_im[PROJECT3_FFT_LEN];
    float dsplib_input[2 * PROJECT3_FFT_LEN];
    float dsplib_output[2 * PROJECT3_FFT_LEN];
    float dsplib_twiddle[PROJECT3_DSPLIB_TWIDDLE_FLOATS];
    float spec[PROJECT3_FREQ_NUM];
} PROJECT3_FEATURE_WORKSPACE;

typedef union {
    PROJECT3_CNN_WORKSPACE cnn;
    PROJECT3_FEATURE_WORKSPACE feature;
} PROJECT3_L2_WORKSPACE;

#pragma DATA_SECTION(g_project3_l2_workspace, "project3_l2_data")
#pragma DATA_ALIGN(g_project3_l2_workspace, 8)
static PROJECT3_L2_WORKSPACE g_project3_l2_workspace;

#define g_project3_act_a  (g_project3_l2_workspace.cnn.act_a)
#define g_project3_act_b  (g_project3_l2_workspace.cnn.act_b)
#define g_project3_act_c  (g_project3_l2_workspace.cnn.act_c)
#define g_project3_fft_re (g_project3_l2_workspace.feature.fft_re)
#define g_project3_fft_im (g_project3_l2_workspace.feature.fft_im)
#define g_project3_dsplib_input \
    (g_project3_l2_workspace.feature.dsplib_input)
#define g_project3_dsplib_output \
    (g_project3_l2_workspace.feature.dsplib_output)
#define g_project3_dsplib_twiddle \
    (g_project3_l2_workspace.feature.dsplib_twiddle)
#define g_project3_spec   (g_project3_l2_workspace.feature.spec)

/* DSPLIB twiddles must survive the CNN overwrite of the shared L2 workspace.
 * Keep one master copy in DDR and copy it into the L2 feature view once per
 * utterance.  The 64-entry bit-reversal table is the layout specified by the
 * TI C674 DSPF_sp_fftSPxSP test driver. */
#pragma DATA_ALIGN(g_project3_dsplib_twiddle_store, 8)
static float
    g_project3_dsplib_twiddle_store[PROJECT3_DSPLIB_TWIDDLE_FLOATS];

#pragma DATA_ALIGN(g_project3_fft_brev, 8)
static unsigned char g_project3_fft_brev[64] = {
    0x0, 0x20, 0x10, 0x30, 0x8, 0x28, 0x18, 0x38,
    0x4, 0x24, 0x14, 0x34, 0xc, 0x2c, 0x1c, 0x3c,
    0x2, 0x22, 0x12, 0x32, 0xa, 0x2a, 0x1a, 0x3a,
    0x6, 0x26, 0x16, 0x36, 0xe, 0x2e, 0x1e, 0x3e,
    0x1, 0x21, 0x11, 0x31, 0x9, 0x29, 0x19, 0x39,
    0x5, 0x25, 0x15, 0x35, 0xd, 0x2d, 0x1d, 0x3d,
    0x3, 0x23, 0x13, 0x33, 0xb, 0x2b, 0x1b, 0x3b,
    0x7, 0x27, 0x17, 0x37, 0xf, 0x2f, 0x1f, 0x3f
};

#pragma DATA_ALIGN(g_project3_fc_in, 8)
static float g_project3_fc_in[PROJECT3_EMBEDDING_SIZE];

#pragma DATA_ALIGN(g_project3_logits_store, 8)
static PROJECT3_LOGITS_STORAGE g_project3_logits_store;
#define g_project3_logits (g_project3_logits_store.logits)

#pragma DATA_SECTION(g_project3_tw_re, "project3_l2_data")
#pragma DATA_ALIGN(g_project3_tw_re, 8)
static float g_project3_tw_re[PROJECT3_FFT_LEN / 2];

#pragma DATA_SECTION(g_project3_tw_im, "project3_l2_data")
#pragma DATA_ALIGN(g_project3_tw_im, 8)
static float g_project3_tw_im[PROJECT3_FFT_LEN / 2];

static unsigned short g_project3_mel_first_bin[PROJECT3_MELS_NUM];
static unsigned short g_project3_mel_last_bin[PROJECT3_MELS_NUM];
static unsigned char g_project3_tables_ready = 0;
static volatile unsigned char g_project3_memory_fault = 0;
volatile unsigned long g_project3_last_inference_ms = 0u;
volatile unsigned char g_project3_fft_dsplib_active = 1u;
volatile unsigned char g_project3_fft_validation_done = 0u;
volatile unsigned char g_project3_fft_validation_pass = 0u;
volatile unsigned int g_project3_fft_validation_error_ppm = 0u;
volatile float g_project3_last_speech_logit = 0.0f;
volatile float g_project3_last_speech_probability = 0.0f;
volatile unsigned char g_project3_last_speech_gate_pass = 0u;
volatile unsigned char g_project3_last_raw_class_id = PROJECT3_CLASS_SILENCE;

/* Low-intrusion inference profiler.  Cycle counts keep sub-microsecond
 * information; the matching microsecond values are convenient in CCS Watch.
 * Values are overwritten after every completed utterance and no printing is
 * performed in the real-time path. */
volatile unsigned long long g_project3_profile_total_cycles = 0u;
volatile unsigned long g_project3_profile_total_us = 0u;
volatile unsigned long long g_project3_profile_overhead_cycles = 0u;
volatile unsigned long g_project3_profile_overhead_us = 0u;

volatile unsigned long long g_project3_profile_feature_cycles = 0u;
volatile unsigned long g_project3_profile_feature_us = 0u;
volatile unsigned long long g_project3_profile_wave_cycles = 0u;
volatile unsigned long g_project3_profile_wave_us = 0u;
volatile unsigned long long g_project3_profile_frame_prep_cycles = 0u;
volatile unsigned long g_project3_profile_frame_prep_us = 0u;
volatile unsigned long long g_project3_profile_fft_cycles = 0u;
volatile unsigned long g_project3_profile_fft_us = 0u;
volatile unsigned long long g_project3_profile_power_cycles = 0u;
volatile unsigned long g_project3_profile_power_us = 0u;
volatile unsigned long long g_project3_profile_mel_cycles = 0u;
volatile unsigned long g_project3_profile_mel_us = 0u;
volatile unsigned long long g_project3_profile_norm_cycles = 0u;
volatile unsigned long g_project3_profile_norm_us = 0u;

volatile unsigned long long g_project3_profile_cnn_cycles = 0u;
volatile unsigned long g_project3_profile_cnn_us = 0u;
volatile unsigned long long g_project3_profile_stem_cycles = 0u;
volatile unsigned long g_project3_profile_stem_us = 0u;
volatile unsigned long long g_project3_profile_block1_cycles = 0u;
volatile unsigned long g_project3_profile_block1_us = 0u;
volatile unsigned long long g_project3_profile_block2_cycles = 0u;
volatile unsigned long g_project3_profile_block2_us = 0u;
volatile unsigned long long g_project3_profile_block3_cycles = 0u;
volatile unsigned long g_project3_profile_block3_us = 0u;
volatile unsigned long long g_project3_profile_head_cycles = 0u;
volatile unsigned long g_project3_profile_head_us = 0u;
volatile unsigned long long g_project3_profile_pool_fc_cycles = 0u;
volatile unsigned long g_project3_profile_pool_fc_us = 0u;

volatile unsigned long long g_project3_profile_conv_cycles = 0u;
volatile unsigned long g_project3_profile_conv_us = 0u;
volatile unsigned long long g_project3_profile_pointwise_cycles = 0u;
volatile unsigned long g_project3_profile_pointwise_us = 0u;
volatile unsigned long long g_project3_profile_depthwise_cycles = 0u;
volatile unsigned long g_project3_profile_depthwise_us = 0u;
volatile unsigned long long g_project3_profile_bn_cycles = 0u;
volatile unsigned long g_project3_profile_bn_us = 0u;
volatile unsigned long long g_project3_profile_addrelu_cycles = 0u;
volatile unsigned long g_project3_profile_addrelu_us = 0u;
volatile unsigned long long g_project3_profile_post_cycles = 0u;
volatile unsigned long g_project3_profile_post_us = 0u;

volatile unsigned long g_project3_last_utterance_samples = 0u;
volatile unsigned long g_project3_audio_block_count = 0u;
volatile unsigned char g_project3_adc_completed_buffer = AD_BUFFER_PING;
volatile unsigned long g_project3_adc_completed_sequence = 0u;
volatile unsigned long g_project3_adc_consumed_sequence = 0u;
volatile unsigned long g_project3_adc_drop_count = 0u;
volatile unsigned long g_project3_capture_write_sequence = 0u;
volatile unsigned long g_project3_capture_read_sequence = 0u;
volatile unsigned int g_project3_capture_queue_depth = 0u;
volatile unsigned int g_project3_capture_queue_high_water = 0u;
volatile unsigned long g_project3_capture_overflow_count = 0u;
volatile unsigned long g_project3_capture_gap_reset_count = 0u;
volatile unsigned long g_project3_capture_last_source_sequence = 0u;
volatile unsigned int g_project3_adc_ch1_peak = 0u;
volatile unsigned int g_project3_adc_ch2_peak = 0u;
volatile unsigned int g_project3_adc_ch3_peak = 0u;
volatile unsigned int g_project3_adc_ch4_peak = 0u;
volatile unsigned long g_project3_vad_start_count = 0u;
volatile unsigned long g_project3_vad_end_count = 0u;
volatile unsigned long g_project3_current_utterance_samples = 0u;
volatile float g_project3_last_frame_energy = 0.0f;
volatile float g_project3_vad_noise_floor = 0.0f;
volatile float g_project3_vad_smooth_energy = 0.0f;
volatile float g_project3_vad_start_threshold = 0.0f;
volatile unsigned int g_project3_vad_calibrate_frames = 0u;
volatile short g_project3_audio_peak = 0;
volatile unsigned char g_project3_raw_top_class = PROJECT3_CLASS_SILENCE;
volatile unsigned char g_project3_raw_second_class = PROJECT3_CLASS_SILENCE;
volatile unsigned char g_project3_result_accepted = 0u;
volatile unsigned int g_project3_raw_top_permille = 0u;
volatile unsigned int g_project3_raw_second_permille = 0u;
volatile unsigned int g_project3_raw_margin_permille = 0u;
volatile unsigned long g_project3_nonspeech_reject_count = 0u;
volatile unsigned int g_project3_last_voiced_frame_count = 0u;
volatile unsigned int g_project3_last_periodicity_permille = 0u;
volatile unsigned int g_project3_vad_speech_score_permille = 0u;
volatile unsigned int g_project3_vad_low_fraction_permille = 0u;
volatile unsigned int g_project3_vad_active_band_count = 0u;
volatile unsigned int g_project3_vad_band_ratio_permille[PROJECT3_VAD_BAND_COUNT] = {0u, 0u, 0u, 0u, 0u};

/* Music observables are independent of PROJECT3_CONTEXT because the audio
 * bridge runs from the ADC EDMA completion interrupt. */
volatile unsigned char g_project3_music_playing = 1u;
volatile unsigned char g_project3_music_volume_level = PROJECT3_MUSIC_DEFAULT_LEVEL;
volatile unsigned int g_project3_music_volume_percent =
    (PROJECT3_MUSIC_DEFAULT_LEVEL * 100u) / PROJECT3_MUSIC_VOLUME_LEVELS;
volatile unsigned int g_project3_music_user_gain_q15 =
    (PROJECT3_MUSIC_DEFAULT_LEVEL * PROJECT3_MUSIC_Q15_FULL_SCALE) /
    PROJECT3_MUSIC_VOLUME_LEVELS;
volatile unsigned long g_project3_music_block_count = 0u;
volatile unsigned long g_project3_music_sync_miss_count = 0u;
volatile unsigned long g_project3_music_command_count = 0u;
volatile unsigned long g_project3_music_last_isr_cycles = 0u;
volatile unsigned long g_project3_music_max_isr_cycles = 0u;
volatile unsigned int g_project3_music_input_peak = 0u;
volatile unsigned char g_project3_music_last_command = PROJECT3_CLASS_SILENCE;
volatile unsigned char g_project3_music_last_output_buffer = DA_BUFFER_PING;
volatile unsigned char g_project3_music_ref_enabled = PROJECT3_ENABLE_MUSIC_REFERENCE;
volatile unsigned char g_project3_music_ref_active = 0u;
volatile unsigned char g_project3_music_ref_correlated = 0u;
volatile unsigned int g_project3_music_ref_corr_permille = 0u;
volatile int g_project3_music_ref_gain_q15 = 0;
volatile unsigned int g_project3_music_ref_residual_permille = 1000u;
volatile unsigned int g_project3_music_ref_residual_peak = 0u;
volatile unsigned long g_project3_music_ref_transition_count = 0u;
static int s_project3_music_applied_gain_q15 =
    (PROJECT3_MUSIC_DEFAULT_LEVEL * PROJECT3_MUSIC_Q15_FULL_SCALE) /
    PROJECT3_MUSIC_VOLUME_LEVELS;

extern unsigned char Lcd_Buffer[];

#pragma DATA_ALIGN(g_project3_text_buffer, 128)
static unsigned char g_project3_text_buffer[P3_TEXT_BUFFER_BYTES];

static void Project3_SetAppState(PROJECT3_CONTEXT *ctx, PROJECT3_APP_STATE state);
static void Project3_SetUiText(PROJECT3_CONTEXT *ctx, const char *main_text, const char *line1, const char *line2);
static const char *Project3_GetStableDisplayText(const PROJECT3_CONTEXT *ctx);
static void Project3_ClearStableDisplay(PROJECT3_CONTEXT *ctx);
static void Project3_CommitStableWord(PROJECT3_CONTEXT *ctx, const char *label);
static void Project3_RestoreStableDisplay(PROJECT3_CONTEXT *ctx);
static void Project3_InitTables(void);
static void Project3_GenerateDsplibTwiddle(float *twiddle);
static unsigned char Project3_ValidateDsplibFft(void);
static void Project3_Fft512(float *re, float *im);
static float Project3_PaddedModelSample(int idx);
static void Project3_BuildModelWave(const PROJECT3_UTTERANCE_BUFFER *utter);
static void Project3_ExtractLogMel(const PROJECT3_UTTERANCE_BUFFER *utter, float *out_logmel);
static void Project3_NormalizeLogMel(float *logmel);
static void Project3_FormatRawResult(const PROJECT3_INFER_RESULT *result, char *text, unsigned int text_len);
static float Project3_ComputeFrameEnergy(const short *frame, unsigned int len);
static void Project3_ComputeVadFeatures(const short *frame, unsigned int len,
                                        PROJECT3_VAD_FEATURES *features);
static void Project3_ResetVadAdaptiveState(PROJECT3_VAD_STATE *vad);
static unsigned char Project3_IsSpeechLikeUtterance(const PROJECT3_CONTEXT *ctx,
                                                     const PROJECT3_UTTERANCE_BUFFER *utter);
static unsigned char Project3_UpdateVad(PROJECT3_CONTEXT *ctx,
                                        const PROJECT3_VAD_FEATURES *features);
static unsigned char Project3_ShouldEndUtterance(PROJECT3_CONTEXT *ctx);
static void Project3_AppendRawBlock(PROJECT3_CONTEXT *ctx, const short *block, unsigned int block_samples);
static void Project3_UpdatePreroll(const short *block, unsigned int block_samples);
static void Project3_AppendPreroll(PROJECT3_CONTEXT *ctx);
static void Project3_CopyInputBlock(short *dst, unsigned char completed_buffer);
static void Project3_SuppressMusicReference(short *mic, const short *music,
                                            unsigned int sample_count);
static void Project3_FillDacOutput(const PROJECT3_CONTEXT *ctx, const short *src);
static void Project3_WriteOutputBlockToDac(const short *src);
static void Project3_HandleSpeechEnd(PROJECT3_CONTEXT *ctx);
static unsigned char Project3_RenderScreen(PROJECT3_CONTEXT *ctx, unsigned char force_redraw);
static void Project3_RequestTextRedraw(PROJECT3_CONTEXT *ctx);
static void Project3_LcdPauseForInference(void);
static void Project3_LcdResumeAfterInference(void);
static void Project3_StartMessageHold(PROJECT3_CONTEXT *ctx,
                                      unsigned char hold_blocks,
                                      unsigned char return_to_listening);
static void Project3_ServiceMessageHold(PROJECT3_CONTEXT *ctx);
static void Project3_ServiceErrorHold(PROJECT3_CONTEXT *ctx);
static void Project3_ResetMemoryGuard(void);
static unsigned char Project3_CheckMemoryGuard(void);
static PROJECT3_INFER_RESULT Project3_MemoryFaultResult(void);
static unsigned long Project3_ProfileCyclesToUs(unsigned long long cycles);
static void Project3_ResetInferenceProfile(void);

static void Project3_PointwiseConv2d(const float *in, float *out,
                                     int in_c, int in_h, int in_w,
                                     int out_c, int out_h, int out_w,
                                     int s_h, int s_w,
                                     const float *weight);
static void Project3_Conv2d(const float *in, float *out,
                            int in_c, int in_h, int in_w,
                            int out_c, int out_h, int out_w,
                            int k_h, int k_w, int s_h, int s_w,
                            int p_h, int p_w, const float *weight);
static void Project3_DepthwiseConv2d(const float *in, float *out,
                                     int channels, int in_h, int in_w,
                                     int out_h, int out_w,
                                     int k_h, int k_w, int s_h, int s_w,
                                     int p_h, int p_w, int d_h, int d_w,
                                     const float *weight);
static void Project3_BatchNorm(float *x, int channels, int h, int w,
                               const float *gamma, const float *beta,
                               const float *mean, const float *var,
                               unsigned char do_relu);
static void Project3_AddRelu(float *x, const float *identity, int count);
static void Project3_BCResBlock(const float *in, float *out,
                                int in_c, int in_h, int in_w,
                                int out_c, int out_h, int out_w,
                                int stride_h, int stride_w,
                                int temporal_dilation,
                                const float *conv1_w,
                                const float *bn1_w, const float *bn1_b, const float *bn1_m, const float *bn1_v,
                                const float *dw_w,
                                const float *bn2_w, const float *bn2_b, const float *bn2_m, const float *bn2_v,
                                const float *conv2_w,
                                const float *bn3_w, const float *bn3_b, const float *bn3_m, const float *bn3_v,
                                const float *shortcut_w,
                                const float *shortcut_bn_w, const float *shortcut_bn_b,
                                const float *shortcut_bn_m, const float *shortcut_bn_v);
static void Project3_BCResNetForward(const float *logmel, float *logits,
                                     float *speech_logit);
static PROJECT3_INFER_RESULT Project3_LogitsToResult(const float *logits);
static unsigned char Project3_AcceptResult(const PROJECT3_INFER_RESULT *result);
static unsigned char Project3_IsCommandClass(unsigned char class_id);

static unsigned long Project3_ProfileCyclesToUs(unsigned long long cycles)
{
    return (unsigned long)
        (cycles / (unsigned long long)(PROJECT3_DSP_CLOCK_HZ / 1000000u));
}

static void Project3_ResetInferenceProfile(void)
{
    g_project3_profile_total_cycles = 0u;
    g_project3_profile_total_us = 0u;
    g_project3_profile_overhead_cycles = 0u;
    g_project3_profile_overhead_us = 0u;

    g_project3_profile_feature_cycles = 0u;
    g_project3_profile_feature_us = 0u;
    g_project3_profile_wave_cycles = 0u;
    g_project3_profile_wave_us = 0u;
    g_project3_profile_frame_prep_cycles = 0u;
    g_project3_profile_frame_prep_us = 0u;
    g_project3_profile_fft_cycles = 0u;
    g_project3_profile_fft_us = 0u;
    g_project3_profile_power_cycles = 0u;
    g_project3_profile_power_us = 0u;
    g_project3_profile_mel_cycles = 0u;
    g_project3_profile_mel_us = 0u;
    g_project3_profile_norm_cycles = 0u;
    g_project3_profile_norm_us = 0u;

    g_project3_profile_cnn_cycles = 0u;
    g_project3_profile_cnn_us = 0u;
    g_project3_profile_stem_cycles = 0u;
    g_project3_profile_stem_us = 0u;
    g_project3_profile_block1_cycles = 0u;
    g_project3_profile_block1_us = 0u;
    g_project3_profile_block2_cycles = 0u;
    g_project3_profile_block2_us = 0u;
    g_project3_profile_block3_cycles = 0u;
    g_project3_profile_block3_us = 0u;
    g_project3_profile_head_cycles = 0u;
    g_project3_profile_head_us = 0u;
    g_project3_profile_pool_fc_cycles = 0u;
    g_project3_profile_pool_fc_us = 0u;

    g_project3_profile_conv_cycles = 0u;
    g_project3_profile_conv_us = 0u;
    g_project3_profile_pointwise_cycles = 0u;
    g_project3_profile_pointwise_us = 0u;
    g_project3_profile_depthwise_cycles = 0u;
    g_project3_profile_depthwise_us = 0u;
    g_project3_profile_bn_cycles = 0u;
    g_project3_profile_bn_us = 0u;
    g_project3_profile_addrelu_cycles = 0u;
    g_project3_profile_addrelu_us = 0u;
    g_project3_profile_post_cycles = 0u;
    g_project3_profile_post_us = 0u;
}

static void Project3_SetAppState(PROJECT3_CONTEXT *ctx, PROJECT3_APP_STATE state)
{
    ctx->app_state = state;
}

static void Project3_SetUiText(PROJECT3_CONTEXT *ctx, const char *main_text, const char *line1, const char *line2)
{
    unsigned char changed = 0;

    if (main_text != NULL && strncmp(ctx->main_text, main_text, PROJECT3_RESULT_TEXT_LEN) != 0) {
        strncpy(ctx->main_text, main_text, PROJECT3_RESULT_TEXT_LEN - 1);
        ctx->main_text[PROJECT3_RESULT_TEXT_LEN - 1] = '\0';
        changed = 1;
    }
    if (line1 != NULL && strncmp(ctx->line1, line1, PROJECT3_UI_TEXT_LEN) != 0) {
        strncpy(ctx->line1, line1, PROJECT3_UI_TEXT_LEN - 1);
        ctx->line1[PROJECT3_UI_TEXT_LEN - 1] = '\0';
        changed = 1;
    }
    if (line2 != NULL && strncmp(ctx->line2, line2, PROJECT3_UI_TEXT_LEN) != 0) {
        strncpy(ctx->line2, line2, PROJECT3_UI_TEXT_LEN - 1);
        ctx->line2[PROJECT3_UI_TEXT_LEN - 1] = '\0';
        changed = 1;
    }
    if (changed) {
        ctx->redraw_needed = 1;
    }
}

static void Project3_RequestTextRedraw(PROJECT3_CONTEXT *ctx)
{
    ctx->last_main_text[0] = '\0';
    ctx->redraw_needed = 1;
    ctx->lcd_retry_frame = 0u;
}

static const char *Project3_GetStableDisplayText(const PROJECT3_CONTEXT *ctx)
{
    if (ctx->stable_text_valid && ctx->stable_text[0] != '\0') {
        return ctx->stable_text;
    }
    return P3_LISTENING_TEXT;
}

static void Project3_ClearStableDisplay(PROJECT3_CONTEXT *ctx)
{
    strncpy(ctx->stable_text, P3_LISTENING_TEXT, PROJECT3_RESULT_TEXT_LEN - 1);
    ctx->stable_text[PROJECT3_RESULT_TEXT_LEN - 1] = '\0';
    ctx->stable_text_valid = 0u;
    Project3_SetAppState(ctx, PROJECT3_APP_LISTENING);
    Project3_SetUiText(ctx, P3_LISTENING_TEXT, "", "");
    Project3_RequestTextRedraw(ctx);
}

static void Project3_CommitStableWord(PROJECT3_CONTEXT *ctx, const char *label)
{
    unsigned char changed;

    if (label == NULL || label[0] == '\0') {
        return;
    }

    changed = (!ctx->stable_text_valid ||
               strncmp(ctx->stable_text, label, PROJECT3_RESULT_TEXT_LEN) != 0);
    strncpy(ctx->stable_text, label, PROJECT3_RESULT_TEXT_LEN - 1);
    ctx->stable_text[PROJECT3_RESULT_TEXT_LEN - 1] = '\0';
    ctx->stable_text_valid = 1u;
    Project3_SetAppState(ctx, PROJECT3_APP_RESULT);
    Project3_SetUiText(ctx, ctx->stable_text, "", "");

    /* Repainting an unchanged word needlessly touches both DMA buffers and is
     * visible as a flash.  A changed label is already marked dirty by
     * Project3_SetUiText(); only repair the dirty flag if the saved state was
     * invalid but the character data happened to compare equal. */
    if (changed) {
        ctx->redraw_needed = 1u;
    }
}

static void Project3_RestoreStableDisplay(PROJECT3_CONTEXT *ctx)
{
    if (ctx->stable_text_valid) {
        Project3_SetAppState(ctx, PROJECT3_APP_RESULT);
    } else {
        Project3_SetAppState(ctx, PROJECT3_APP_LISTENING);
    }
    Project3_SetUiText(ctx, Project3_GetStableDisplayText(ctx), "", "");
}

static void Project3_ResetMemoryGuard(void)
{
    unsigned int i;

    for (i = 0; i < PROJECT3_GUARD_WORDS; i++) {
        g_project3_logits_store.guard[i] = PROJECT3_GUARD_BASE ^ (PROJECT3_GUARD_STEP * (i + 1u));
    }
    g_project3_memory_fault = 0;
}

static unsigned char Project3_CheckMemoryGuard(void)
{
    unsigned int i;

    for (i = 0; i < PROJECT3_GUARD_WORDS; i++) {
        if (g_project3_logits_store.guard[i] != (PROJECT3_GUARD_BASE ^ (PROJECT3_GUARD_STEP * (i + 1u)))) {
            g_project3_memory_fault = 1;
            return 0;
        }
    }

    return 1;
}

static PROJECT3_INFER_RESULT Project3_MemoryFaultResult(void)
{
    PROJECT3_INFER_RESULT result;

    result.class_id = PROJECT3_CLASS_UNKNOWN;
    result.second_class_id = PROJECT3_CLASS_UNKNOWN;
    result.confidence = 0.0f;
    result.second_confidence = 0.0f;
    result.margin = 0.0f;
    result.valid = 0;
    return result;
}

static void Project3_CacheWriteBackAligned(unsigned int base_addr, unsigned int byte_size)
{
    unsigned int aligned_base;
    unsigned int aligned_end;

    if (byte_size == 0u) {
        return;
    }

    aligned_base = base_addr & ~(P3_CACHE_LINE_BYTES - 1u);
    aligned_end = (base_addr + byte_size + P3_CACHE_LINE_BYTES - 1u) &
                  ~(P3_CACHE_LINE_BYTES - 1u);
    CacheWB(aligned_base, aligned_end - aligned_base);
}

static void Project3_LcdPauseForInference(void)
{
#if PROJECT3_LCD_PAUSE_DURING_INFERENCE
    if (!s_lcd_infer_paused) {
        Project3_CacheWriteBackAligned((unsigned int)Lcd_Buffer, P3_LCD_BUFFER_BYTES);
        (void)Lcd_WaitForEndOfFrame();
        RasterDisable(SOC_LCDC_0_REGS);
        s_lcd_infer_paused = 1;
    }
#endif
}

static void Project3_LcdResumeAfterInference(void)
{
#if PROJECT3_LCD_PAUSE_DURING_INFERENCE
    if (s_lcd_infer_paused) {
        Project3_CacheWriteBackAligned((unsigned int)Lcd_Buffer, P3_LCD_BUFFER_BYTES);
        RasterEnable(SOC_LCDC_0_REGS);
        s_lcd_infer_paused = 0;
        (void)Lcd_GetRasterFaultStatus();
    }
#endif
}

static void Project3_DrawSafeTextToBand(const char *text, unsigned long color)
{
    tDisplay display;
    tContext context;
    tRectangle rect;
    const tFont *main_font;
    unsigned long main_color;
    char music_status[32];
    unsigned int volume_percent;
    unsigned char music_playing;

    GrOffScreen16BPPInit(&display, g_project3_text_buffer,
                         P3_TEXT_BAND_W, P3_TEXT_BAND_H);
    GrContextInit(&context, &display);

    rect.sXMin = 0;
    rect.sYMin = 0;
    rect.sXMax = P3_TEXT_BAND_W - 1;
    rect.sYMax = P3_TEXT_BAND_H - 1;
    GrContextForegroundSet(&context, ClrBlack);
    GrRectFill(&context, &rect);

    rect.sXMin = 8;
    rect.sYMin = 8;
    rect.sXMax = P3_TEXT_BAND_W - 9;
    rect.sYMax = P3_TEXT_BAND_H - 9;
    GrContextForegroundSet(&context, 0x081A2Au);
    GrRectFill(&context, &rect);
    GrContextForegroundSet(&context, 0x32C9D6u);
    GrRectDraw(&context, &rect);

    rect.sXMin = 14;
    rect.sYMin = 14;
    rect.sXMax = P3_TEXT_BAND_W - 15;
    rect.sYMax = P3_TEXT_BAND_H - 15;
    GrContextForegroundSet(&context, 0x17405Cu);
    GrRectDraw(&context, &rect);

    GrContextFontSet(&context, &g_sFontCm20);
    GrContextForegroundSet(&context, 0x86A9C0u);
    GrContextBackgroundSet(&context, 0x081A2Au);
    GrStringDrawCentered(&context, "VOICE COMMAND", -1,
                         P3_TEXT_BAND_W / 2, 36, 0);

    GrContextForegroundSet(&context, 0x17405Cu);
    GrLineDraw(&context, 28, 60, P3_TEXT_BAND_W - 29, 60);

    if (text == NULL || text[0] == '\0') {
        return;
    }

    main_font = &g_sFontCm48b;
    GrContextFontSet(&context, main_font);
    if (GrStringWidthGet(&context, text, -1) > P3_DYNAMIC_W - 40) {
        main_font = &g_sFontCm36b;
        GrContextFontSet(&context, main_font);
    }
    if (GrStringWidthGet(&context, text, -1) > P3_DYNAMIC_W - 40) {
        main_font = &g_sFontCm30b;
        GrContextFontSet(&context, main_font);
    }
    if (GrStringWidthGet(&context, text, -1) > P3_DYNAMIC_W - 40) {
        main_font = &g_sFontCm24b;
        GrContextFontSet(&context, main_font);
    }

    main_color = (strcmp(text, P3_LISTENING_TEXT) == 0) ?
                 0x8FD8E8u : color;
    GrContextForegroundSet(&context, main_color);
    GrContextBackgroundSet(&context, 0x081A2Au);
    GrStringDrawCentered(&context, text, -1,
                         P3_TEXT_BAND_W / 2, 128, 0);

    /* Snapshot the user-facing music state once per UI redraw.  The ADC ISR
     * only consumes these values; LCD drawing remains in the foreground. */
    music_playing = g_project3_music_playing;
    volume_percent = g_project3_music_volume_percent;
    if (volume_percent > 100u) {
        volume_percent = 100u;
    }
    snprintf(music_status, sizeof(music_status),
             "MUSIC %s   VOL %u%%",
             music_playing ? "ON" : "OFF",
             volume_percent);
    GrContextFontSet(&context, &g_sFontCm20);
    GrContextForegroundSet(&context, 0x86A9C0u);
    GrContextBackgroundSet(&context, 0x081A2Au);
    GrStringDrawCentered(&context, music_status, -1,
                         P3_TEXT_BAND_W / 2, 170, 0);

    GrContextForegroundSet(&context,
                           (strcmp(text, P3_LISTENING_TEXT) == 0) ?
                           0x8FD8E8u : 0x32C9D6u);
    GrLineDraw(&context,
               P3_TEXT_BAND_W / 2 - 28, P3_TEXT_BAND_H - 25,
               P3_TEXT_BAND_W / 2 + 28, P3_TEXT_BAND_H - 25);
}

static unsigned char Project3_ClearAndDrawText(const char *text, unsigned long color)
{
    unsigned char *src;
    const unsigned int row_bytes = P3_TEXT_BAND_W * 2u;
    unsigned char updated;

    Project3_DrawSafeTextToBand(text, color);

    src = g_project3_text_buffer + P3_LCD_PIXEL_OFFSET;
    if (!s_lcd_layout_ready) {
        updated = Lcd_UpdateRegionToBothBuffers(src,
                                                P3_TEXT_BAND_X,
                                                P3_TEXT_BAND_Y,
                                                P3_TEXT_BAND_W,
                                                P3_TEXT_BAND_H,
                                                row_bytes);
        if (updated) {
            s_lcd_layout_ready = 1u;
        }
        return updated;
    }

    /* Keep the frame, title and separators untouched after initialization.
     * Only the center result window is copied for each recognition event. */
    src += ((P3_DYNAMIC_Y * P3_TEXT_BAND_W + P3_DYNAMIC_X) * 2u);
    return Lcd_UpdateRegionToBothBuffers(src,
                                         P3_TEXT_BAND_X + P3_DYNAMIC_X,
                                         P3_TEXT_BAND_Y + P3_DYNAMIC_Y,
                                         P3_DYNAMIC_W,
                                         P3_DYNAMIC_H,
                                         row_bytes);
}

static void Project3_GenerateDsplibTwiddle(float *twiddle)
{
    int i;
    int j;
    int k;
    double theta;
    const double pi = 3.14159265358979323846;

    for (i = 0; i < PROJECT3_DSPLIB_TWIDDLE_FLOATS; i++) {
        twiddle[i] = 0.0f;
    }

    /* Mixed-radix layout required by TI DSPF_sp_fftSPxSP. */
    k = 0;
    for (j = 1; j <= (PROJECT3_FFT_LEN >> 2); j <<= 2) {
        for (i = 0; i < (PROJECT3_FFT_LEN >> 2); i += j) {
            theta = 2.0 * pi * (double)i / (double)PROJECT3_FFT_LEN;
            twiddle[k] = (float)cos(theta);
            twiddle[k + 1] = (float)sin(theta);

            theta = 4.0 * pi * (double)i / (double)PROJECT3_FFT_LEN;
            twiddle[k + 2] = (float)cos(theta);
            twiddle[k + 3] = (float)sin(theta);

            theta = 6.0 * pi * (double)i / (double)PROJECT3_FFT_LEN;
            twiddle[k + 4] = (float)cos(theta);
            twiddle[k + 5] = (float)sin(theta);
            k += 6;
        }
    }
}

static void Project3_InitTables(void)
{
    int i, j;
    float all_freq;
    float m_min;
    float m_max;
    float m_pts[PROJECT3_MELS_NUM + 2];
    float f_pts[PROJECT3_MELS_NUM + 2];
    float f_diff[PROJECT3_MELS_NUM + 1];

    if (g_project3_tables_ready) {
        return;
    }

    for (i = 0; i < PROJECT3_WIN_SIZE; i++) {
        g_project3_window[i] = 0.5f * (1.0f - cosf(2.0f * PROJECT3_PI * (float)i / (float)PROJECT3_WIN_SIZE));
    }

    for (i = 0; i < PROJECT3_FFT_LEN / 2; i++) {
        float angle = 2.0f * PROJECT3_PI * (float)i / (float)PROJECT3_FFT_LEN;
        g_project3_tw_re[i] = cosf(angle);
        g_project3_tw_im[i] = -sinf(angle);
    }
    Project3_GenerateDsplibTwiddle(g_project3_dsplib_twiddle_store);

    m_min = 2595.0f * log10f(1.0f);
    m_max = 2595.0f * log10f(1.0f + ((float)PROJECT3_MODEL_SAMPLE_RATE / 2.0f) / 700.0f);

    for (i = 0; i < PROJECT3_MELS_NUM + 2; i++) {
        m_pts[i] = m_min + (m_max - m_min) * (float)i / (float)(PROJECT3_MELS_NUM + 1);
        f_pts[i] = 700.0f * (powf(10.0f, m_pts[i] / 2595.0f) - 1.0f);
    }

    for (i = 0; i < PROJECT3_MELS_NUM + 1; i++) {
        f_diff[i] = f_pts[i + 1] - f_pts[i];
    }

    for (j = 0; j < PROJECT3_MELS_NUM; j++) {
        g_project3_mel_first_bin[j] = PROJECT3_FREQ_NUM;
        g_project3_mel_last_bin[j] = 0;
    }

    for (i = 0; i < PROJECT3_FREQ_NUM; i++) {
        all_freq = ((float)i) * ((float)PROJECT3_MODEL_SAMPLE_RATE / 2.0f) / (float)(PROJECT3_FREQ_NUM - 1);
        for (j = 0; j < PROJECT3_MELS_NUM; j++) {
            float slope_j = f_pts[j] - all_freq;
            float slope_j2 = f_pts[j + 2] - all_freq;
            float left = -slope_j / f_diff[j];
            float right = slope_j2 / f_diff[j + 1];
            float val = (left < right) ? left : right;
            if (val < 0.0f) val = 0.0f;
            g_project3_mel_filter[i * PROJECT3_MELS_NUM + j] = val;
            if (val > 0.0f) {
                if (i < g_project3_mel_first_bin[j]) {
                    g_project3_mel_first_bin[j] = (unsigned short)i;
                }
                g_project3_mel_last_bin[j] = (unsigned short)i;
            }
        }
    }

    for (j = 0; j < PROJECT3_MELS_NUM; j++) {
        if (g_project3_mel_first_bin[j] >= PROJECT3_FREQ_NUM) {
            g_project3_mel_first_bin[j] = 0;
            g_project3_mel_last_bin[j] = 0;
        }
    }

    g_project3_tables_ready = 1;
}

static void Project3_Fft512(float *re, float *im)
{
    int i, j, bit, len, half, step, base;

    j = 0;
    for (i = 1; i < PROJECT3_FFT_LEN; i++) {
        bit = PROJECT3_FFT_LEN >> 1;
        while (j & bit) {
            j ^= bit;
            bit >>= 1;
        }
        j ^= bit;
        if (i < j) {
            float tr = re[i];
            float ti = im[i];
            re[i] = re[j];
            im[i] = im[j];
            re[j] = tr;
            im[j] = ti;
        }
    }

    for (len = 2; len <= PROJECT3_FFT_LEN; len <<= 1) {
        half = len >> 1;
        step = PROJECT3_FFT_LEN / len;
        for (base = 0; base < PROJECT3_FFT_LEN; base += len) {
            for (j = 0; j < half; j++) {
                int even = base + j;
                int odd = even + half;
                int tw = j * step;
                float wr = g_project3_tw_re[tw];
                float wi = g_project3_tw_im[tw];
                float tr = wr * re[odd] - wi * im[odd];
                float ti = wr * im[odd] + wi * re[odd];
                float ur = re[even];
                float ui = im[even];
                re[even] = ur + tr;
                im[even] = ui + ti;
                re[odd] = ur - tr;
                im[odd] = ui - ti;
            }
        }
    }

}

static unsigned char Project3_ValidateDsplibFft(void)
{
    int f;
    float ref_power;
    float opt_power;
    float difference;
    float max_reference = 0.0f;
    float max_difference = 0.0f;
    float error_ppm;

    for (f = 0; f < PROJECT3_FREQ_NUM; f++) {
        ref_power = g_project3_fft_re[f] * g_project3_fft_re[f] +
                    g_project3_fft_im[f] * g_project3_fft_im[f];
        opt_power = g_project3_dsplib_output[2 * f] *
                    g_project3_dsplib_output[2 * f] +
                    g_project3_dsplib_output[2 * f + 1] *
                    g_project3_dsplib_output[2 * f + 1];
        difference = fabsf(opt_power - ref_power);
        if (ref_power > max_reference) {
            max_reference = ref_power;
        }
        if (difference > max_difference) {
            max_difference = difference;
        }
    }

    if (max_reference > 1.0e-20f) {
        error_ppm = (max_difference / max_reference) * 1000000.0f;
    } else {
        error_ppm = (max_difference <= 1.0e-20f) ? 0.0f : 1000000.0f;
    }
    if (error_ppm > 4294967000.0f) {
        error_ppm = 4294967000.0f;
    }

    g_project3_fft_validation_error_ppm =
        (unsigned int)(error_ppm + 0.5f);
    g_project3_fft_validation_done = 1u;
    if (g_project3_fft_validation_error_ppm <=
        PROJECT3_FFT_VALIDATION_MAX_PPM) {
        g_project3_fft_validation_pass = 1u;
        g_project3_fft_dsplib_active = 1u;
        return 1u;
    }

    /* Never trade correctness for speed: retain the verified reference path
     * automatically if the library/table convention does not match. */
    g_project3_fft_validation_pass = 0u;
    g_project3_fft_dsplib_active = 0u;
    return 0u;
}

static float Project3_PaddedModelSample(int idx)
{
    if (idx < PROJECT3_WIN_SIZE / 2) {
        int src = PROJECT3_WIN_SIZE / 2 - idx;
        if (src >= PROJECT3_MODEL_SAMPLES) src = PROJECT3_MODEL_SAMPLES - 1;
        return g_project3_model_wave[src];
    }
    if (idx < PROJECT3_WIN_SIZE / 2 + PROJECT3_MODEL_SAMPLES) {
        return g_project3_model_wave[idx - PROJECT3_WIN_SIZE / 2];
    }
    {
        int tail = idx - (PROJECT3_WIN_SIZE / 2 + PROJECT3_MODEL_SAMPLES);
        int src = PROJECT3_MODEL_SAMPLES - 2 - tail;
        if (src < 0) src = 0;
        return g_project3_model_wave[src];
    }
}

static void Project3_BuildModelWave(const PROJECT3_UTTERANCE_BUFFER *utter)
{
    int n;
    double mean = 0.0;
    double power = 0.0;
    float rms;
    float gain;
    float peak = 0.0f;
    unsigned char compress_long_utterance;
    float source_position = 0.0f;
    float source_step = 0.0f;

    if (utter->count > 0u) {
        unsigned int i;
        for (i = 0; i < utter->count; i++) {
            mean += (double)utter->raw[i];
        }
        mean /= (double)utter->count;
    }

    compress_long_utterance =
        (utter->count > PROJECT3_HW_SAMPLE_RATE) ? 1u : 0u;
    if (compress_long_utterance) {
        source_step = (float)(utter->count - 1u) /
                      (float)(PROJECT3_MODEL_SAMPLES - 1);
    }

    for (n = 0; n < PROJECT3_MODEL_SAMPLES; n++) {
        int base;
        float frac;
        float sample = (float)mean;

        if (compress_long_utterance) {
            base = (int)source_position;
            frac = source_position - (float)base;
            source_position += source_step;
        } else {
            int q = n * 5;
            int rem = q & 3;
            base = q >> 2;
            frac = (float)rem * 0.25f;
        }

        if ((unsigned int)(base + 1) < utter->count) {
            sample = (1.0f - frac) * (float)utter->raw[base] + frac * (float)utter->raw[base + 1];
        } else if ((unsigned int)base < utter->count) {
            sample = (float)utter->raw[base];
        }
        sample = (sample - (float)mean) / 32768.0f;
        g_project3_model_wave[n] = sample;
        power += (double)sample * (double)sample;
        if (fabsf(sample) > peak) {
            peak = fabsf(sample);
        }
    }

    rms = sqrtf((float)(power / (double)PROJECT3_MODEL_SAMPLES));
    if (rms > PROJECT3_WAVE_MIN_RMS && rms < PROJECT3_WAVE_TARGET_RMS) {
        gain = PROJECT3_WAVE_TARGET_RMS / rms;
        if (gain > PROJECT3_WAVE_MAX_GAIN) {
            gain = PROJECT3_WAVE_MAX_GAIN;
        }
        if (peak > 0.0f && peak * gain > PROJECT3_WAVE_PEAK_LIMIT) {
            gain = PROJECT3_WAVE_PEAK_LIMIT / peak;
        }
        if (gain > 1.0f) {
            for (n = 0; n < PROJECT3_MODEL_SAMPLES; n++) {
                g_project3_model_wave[n] *= gain;
            }
        }
    }
}

static void Project3_ExtractLogMel(const PROJECT3_UTTERANCE_BUFFER *utter, float *out_logmel)
{
    int frame, i, m, f;
    unsigned long long total_start;
    unsigned long long total_end;
    unsigned long long stage_start;
    unsigned long long stage_end;
    unsigned long long wave_cycles;
    unsigned long long frame_prep_cycles = 0u;
    unsigned long long fft_cycles = 0u;
    unsigned long long power_cycles = 0u;
    unsigned long long mel_cycles = 0u;
    unsigned long long norm_cycles;
    unsigned char need_reference;
    unsigned char use_dsplib;
    float frame_sample;

    total_start = _itoll(TSCH, TSCL);
    Project3_InitTables();
    if (!g_project3_fft_validation_done ||
        g_project3_fft_dsplib_active) {
        memcpy(g_project3_dsplib_twiddle,
               g_project3_dsplib_twiddle_store,
               sizeof(g_project3_dsplib_twiddle_store));
    }

    stage_start = _itoll(TSCH, TSCL);
    Project3_BuildModelWave(utter);
    stage_end = _itoll(TSCH, TSCL);
    wave_cycles = stage_end - stage_start;

    for (frame = 0; frame < PROJECT3_MODEL_FRAMES; frame++) {
        int start = frame * PROJECT3_HOP_SIZE;

        need_reference = (!g_project3_fft_validation_done ||
                          !g_project3_fft_dsplib_active) ? 1u : 0u;
        use_dsplib = (!g_project3_fft_validation_done ||
                      g_project3_fft_dsplib_active) ? 1u : 0u;

        stage_start = _itoll(TSCH, TSCL);
        for (i = 0; i < PROJECT3_FFT_LEN; i++) {
            frame_sample = 0.0f;
            if (i < PROJECT3_WIN_SIZE) {
                frame_sample = Project3_PaddedModelSample(start + i) *
                               g_project3_window[i];
            }
            if (use_dsplib) {
                g_project3_dsplib_input[2 * i] = frame_sample;
                g_project3_dsplib_input[2 * i + 1] = 0.0f;
            }
            if (need_reference) {
                g_project3_fft_re[i] = frame_sample;
                g_project3_fft_im[i] = 0.0f;
            }
        }
        stage_end = _itoll(TSCH, TSCL);
        frame_prep_cycles += stage_end - stage_start;

        stage_start = _itoll(TSCH, TSCL);
        if (!g_project3_fft_validation_done) {
            Project3_Fft512(g_project3_fft_re, g_project3_fft_im);
            DSPF_sp_fftSPxSP(PROJECT3_FFT_LEN,
                             g_project3_dsplib_input,
                             g_project3_dsplib_twiddle,
                             g_project3_dsplib_output,
                             g_project3_fft_brev,
                             2, 0, PROJECT3_FFT_LEN);
            Project3_ValidateDsplibFft();
        } else if (g_project3_fft_dsplib_active) {
            DSPF_sp_fftSPxSP(PROJECT3_FFT_LEN,
                             g_project3_dsplib_input,
                             g_project3_dsplib_twiddle,
                             g_project3_dsplib_output,
                             g_project3_fft_brev,
                             2, 0, PROJECT3_FFT_LEN);
        } else {
            Project3_Fft512(g_project3_fft_re, g_project3_fft_im);
        }
        stage_end = _itoll(TSCH, TSCL);
        fft_cycles += stage_end - stage_start;

        stage_start = _itoll(TSCH, TSCL);
        if (g_project3_fft_dsplib_active) {
            for (f = 0; f < PROJECT3_FREQ_NUM; f++) {
                g_project3_spec[f] =
                    g_project3_dsplib_output[2 * f] *
                    g_project3_dsplib_output[2 * f] +
                    g_project3_dsplib_output[2 * f + 1] *
                    g_project3_dsplib_output[2 * f + 1];
            }
        } else {
            for (f = 0; f < PROJECT3_FREQ_NUM; f++) {
                g_project3_spec[f] =
                    g_project3_fft_re[f] * g_project3_fft_re[f] +
                    g_project3_fft_im[f] * g_project3_fft_im[f];
            }
        }
        stage_end = _itoll(TSCH, TSCL);
        power_cycles += stage_end - stage_start;

        stage_start = _itoll(TSCH, TSCL);
        for (m = 0; m < PROJECT3_MELS_NUM; m++) {
            float mel_energy = 1.0e-6f;
            int first_bin = (int)g_project3_mel_first_bin[m];
            int last_bin = (int)g_project3_mel_last_bin[m];
            for (f = first_bin; f <= last_bin; f++) {
                mel_energy += g_project3_spec[f] * g_project3_mel_filter[f * PROJECT3_MELS_NUM + m];
            }
            out_logmel[m * PROJECT3_MODEL_FRAMES + frame] = logf(mel_energy);
        }
        stage_end = _itoll(TSCH, TSCL);
        mel_cycles += stage_end - stage_start;
    }

    stage_start = _itoll(TSCH, TSCL);
    Project3_NormalizeLogMel(out_logmel);
    stage_end = _itoll(TSCH, TSCL);
    norm_cycles = stage_end - stage_start;
    total_end = _itoll(TSCH, TSCL);

    g_project3_profile_wave_cycles = wave_cycles;
    g_project3_profile_wave_us = Project3_ProfileCyclesToUs(wave_cycles);
    g_project3_profile_frame_prep_cycles = frame_prep_cycles;
    g_project3_profile_frame_prep_us =
        Project3_ProfileCyclesToUs(frame_prep_cycles);
    g_project3_profile_fft_cycles = fft_cycles;
    g_project3_profile_fft_us = Project3_ProfileCyclesToUs(fft_cycles);
    g_project3_profile_power_cycles = power_cycles;
    g_project3_profile_power_us = Project3_ProfileCyclesToUs(power_cycles);
    g_project3_profile_mel_cycles = mel_cycles;
    g_project3_profile_mel_us = Project3_ProfileCyclesToUs(mel_cycles);
    g_project3_profile_norm_cycles = norm_cycles;
    g_project3_profile_norm_us = Project3_ProfileCyclesToUs(norm_cycles);
    g_project3_profile_feature_cycles = total_end - total_start;
    g_project3_profile_feature_us =
        Project3_ProfileCyclesToUs(total_end - total_start);
}

static void Project3_NormalizeLogMel(float *logmel)
{
    int i;
    double sum = 0.0;
    double variance_sum = 0.0;
    float mean;
    float inv_std;

    for (i = 0; i < PROJECT3_FEATURE_COUNT; i++) {
        sum += (double)logmel[i];
    }
    mean = (float)(sum / (double)PROJECT3_FEATURE_COUNT);

    for (i = 0; i < PROJECT3_FEATURE_COUNT; i++) {
        double centered = (double)logmel[i] - (double)mean;
        variance_sum += centered * centered;
    }

    inv_std = 1.0f /
              (sqrtf((float)(variance_sum / (double)PROJECT3_FEATURE_COUNT)) +
               PROJECT3_FEATURE_NORM_EPS);

    for (i = 0; i < PROJECT3_FEATURE_COUNT; i++) {
        logmel[i] = (logmel[i] - mean) * inv_std;
    }
}

static float Project3_ComputeFrameEnergy(const short *frame, unsigned int len)
{
    unsigned int i;
    float mean = 0.0f;
    float energy = 0.0f;

    for (i = 0; i < len; i++) {
        mean += (float)frame[i];
    }
    mean /= (float)len;

    for (i = 0; i < len; i++) {
        float sample = ((float)frame[i] - mean) / 32768.0f;
        energy += sample * sample;
    }

    return energy / (float)len;
}

static void Project3_ComputeVadFeatures(const short *frame, unsigned int len,
                                        PROJECT3_VAD_FEATURES *features)
{
    /* Four one-pole low-pass sections form five coarse, overlapping bands.
     * The coefficients are fixed by the 20 kHz sampling rate, while every
     * decision threshold is relative to the independently learned noise
     * energy in each band.  Local filter state makes this routine safe for
     * the overlapping 400-sample VAD frames. */
    const float alpha0 = 0.0755356f;  /* approximately 0..250 Hz */
    const float alpha1 = 0.1974812f;  /* approximately 250..700 Hz */
    const float alpha2 = 0.3950774f;  /* approximately 700..1600 Hz */
    const float alpha3 = 0.6340697f;  /* approximately 1600..3200 Hz */
    float lp0 = 0.0f;
    float lp1 = 0.0f;
    float lp2 = 0.0f;
    float lp3 = 0.0f;
    float mean = 0.0f;
    float band_sum = 0.0f;
    unsigned int i;
    unsigned int b;

    features->total_energy = 0.0f;
    features->low_fraction = 0.0f;
    for (b = 0u; b < PROJECT3_VAD_BAND_COUNT; b++) {
        features->band_energy[b] = 0.0f;
    }

    if (len == 0u) {
        return;
    }

    for (i = 0u; i < len; i++) {
        mean += (float)frame[i];
    }
    mean /= (float)len;

    for (i = 0u; i < len; i++) {
        float x = ((float)frame[i] - mean) / 32768.0f;
        float band0;
        float band1;
        float band2;
        float band3;
        float band4;

        lp0 += alpha0 * (x - lp0);
        lp1 += alpha1 * (x - lp1);
        lp2 += alpha2 * (x - lp2);
        lp3 += alpha3 * (x - lp3);

        band0 = lp0;
        band1 = lp1 - lp0;
        band2 = lp2 - lp1;
        band3 = lp3 - lp2;
        band4 = x - lp3;

        features->total_energy += x * x;
        features->band_energy[0] += band0 * band0;
        features->band_energy[1] += band1 * band1;
        features->band_energy[2] += band2 * band2;
        features->band_energy[3] += band3 * band3;
        features->band_energy[4] += band4 * band4;
    }

    features->total_energy /= (float)len;
    for (b = 0u; b < PROJECT3_VAD_BAND_COUNT; b++) {
        features->band_energy[b] /= (float)len;
        if (features->band_energy[b] < PROJECT3_VAD_BAND_FLOOR) {
            features->band_energy[b] = PROJECT3_VAD_BAND_FLOOR;
        }
        band_sum += features->band_energy[b];
    }
    if (band_sum > PROJECT3_VAD_BAND_FLOOR) {
        features->low_fraction = features->band_energy[0] / band_sum;
    }
}

static void Project3_ResetVadAdaptiveState(PROJECT3_VAD_STATE *vad)
{
    unsigned int b;

    vad->noise_floor = PROJECT3_VAD_MIN_FLOOR;
    vad->smooth_energy = PROJECT3_VAD_MIN_FLOOR;
    vad->speech_score = 0.0f;
    vad->low_fraction = 0.0f;
    vad->speech_hold_frames = 0u;
    vad->silence_hold_frames = 0u;
    vad->calibrate_frames = 0u;
    vad->active_bands = 0u;
    vad->active = 0u;
    for (b = 0u; b < PROJECT3_VAD_BAND_COUNT; b++) {
        vad->band_noise[b] = PROJECT3_VAD_BAND_FLOOR;
        vad->band_smooth[b] = PROJECT3_VAD_BAND_FLOOR;
        vad->band_deviation[b] = PROJECT3_VAD_BAND_FLOOR;
        g_project3_vad_band_ratio_permille[b] = 0u;
    }
    g_project3_vad_speech_score_permille = 0u;
    g_project3_vad_low_fraction_permille = 0u;
    g_project3_vad_active_band_count = 0u;
    g_project3_vad_start_threshold = PROJECT3_VAD_ABS_START_ENERGY;
    g_project3_vad_calibrate_frames = 0u;
}

static unsigned char Project3_IsSpeechLikeUtterance(const PROJECT3_CONTEXT *ctx,
                                                     const PROJECT3_UTTERANCE_BUFFER *utter)
{
    unsigned int offset;
    unsigned int voiced_frames = 0u;
    unsigned int spectral_frames = 0u;
    unsigned int spectral_voiced_frames = 0u;
    float max_periodicity_sq = 0.0f;
    float min_frame_energy = ctx->vad.noise_floor * 2.0f;

    if (min_frame_energy < PROJECT3_VAD_ABS_START_ENERGY * 0.60f) {
        min_frame_energy = PROJECT3_VAD_ABS_START_ENERGY * 0.60f;
    }

    for (offset = 0u;
         offset + PROJECT3_SPEECH_CHECK_FRAME_LEN <= utter->count;
         offset += PROJECT3_SPEECH_CHECK_HOP) {
        const short *frame = &utter->raw[offset];
        PROJECT3_VAD_FEATURES features;
        float frame_energy;
        float mean = 0.0f;
        float best_score_sq = 0.0f;
        unsigned int active_bands = 0u;
        unsigned char spectral_like;
        unsigned int i;
        unsigned int lag;
        unsigned int b;

        Project3_ComputeVadFeatures(frame, PROJECT3_SPEECH_CHECK_FRAME_LEN,
                                    &features);
        frame_energy = features.total_energy;

        if (frame_energy < min_frame_energy) {
            continue;
        }

        for (b = 1u; b < PROJECT3_VAD_BAND_COUNT; b++) {
            float baseline = ctx->vad.band_noise[b];
            if (baseline < PROJECT3_VAD_BAND_FLOOR) {
                baseline = PROJECT3_VAD_BAND_FLOOR;
            }
            if (features.band_energy[b] >= baseline * 1.60f) {
                active_bands++;
            }
        }
        spectral_like =
            (active_bands >= 2u &&
             features.low_fraction < PROJECT3_VAD_LOW_DOMINANCE_LIMIT + 0.08f)
                ? 1u : 0u;
        if (spectral_like) {
            spectral_frames++;
        }

        for (i = 0u; i < PROJECT3_SPEECH_CHECK_FRAME_LEN; i++) {
            mean += (float)frame[i];
        }
        mean /= (float)PROJECT3_SPEECH_CHECK_FRAME_LEN;

        for (lag = PROJECT3_SPEECH_PITCH_LAG_MIN;
             lag <= PROJECT3_SPEECH_PITCH_LAG_MAX;
             lag += PROJECT3_SPEECH_PITCH_LAG_STEP) {
            float correlation = 0.0f;
            float energy_a = 0.0f;
            float energy_b = 0.0f;
            float score_sq;
            unsigned int limit = PROJECT3_SPEECH_CHECK_FRAME_LEN - lag;

            /* Every other sample is sufficient for the gate and halves the
             * cost.  Compare squared normalized correlation to avoid sqrtf
             * in the inner loop. */
            for (i = 0u; i < limit; i += 2u) {
                float a = (float)frame[i] - mean;
                float b = (float)frame[i + lag] - mean;
                correlation += a * b;
                energy_a += a * a;
                energy_b += b * b;
            }

            if (correlation <= 0.0f || energy_a <= 1.0f || energy_b <= 1.0f) {
                continue;
            }
            score_sq = (correlation * correlation) / (energy_a * energy_b);
            if (score_sq > best_score_sq) {
                best_score_sq = score_sq;
            }
        }

        if (best_score_sq > max_periodicity_sq) {
            max_periodicity_sq = best_score_sq;
        }
        if (best_score_sq >= PROJECT3_SPEECH_PERIODICITY_SQ) {
            voiced_frames++;
            if (spectral_like) {
                spectral_voiced_frames++;
            }
            if (spectral_voiced_frames >= PROJECT3_SPEECH_REQUIRED_FRAMES &&
                spectral_frames >= PROJECT3_SPEECH_REQUIRED_FRAMES) {
                break;
            }
        }
    }

    g_project3_last_voiced_frame_count = voiced_frames;
    g_project3_last_periodicity_permille =
        (unsigned int)(sqrtf(max_periodicity_sq) * 1000.0f + 0.5f);
    if (g_project3_last_periodicity_permille > 1000u) {
        g_project3_last_periodicity_permille = 1000u;
    }
    return (spectral_voiced_frames >= PROJECT3_SPEECH_REQUIRED_FRAMES &&
            spectral_frames >= PROJECT3_SPEECH_REQUIRED_FRAMES) ? 1u : 0u;
}

static unsigned char Project3_UpdateVad(PROJECT3_CONTEXT *ctx,
                                        const PROJECT3_VAD_FEATURES *features)
{
    PROJECT3_VAD_STATE *vad = &ctx->vad;
    const float smooth_alpha = 0.85f;
    const float floor_rise_alpha = 0.95f;
    const float floor_fall_alpha = 0.92f;
    const float start_ratio = 4.5f;
    const float stop_ratio = 1.8f;
    const unsigned short start_frames = 5u;
    const float score_weights[PROJECT3_VAD_BAND_COUNT] = {
        0.0f, 1.0f, 1.0f, 0.85f, 0.65f
    };
    float frame_energy = features->total_energy;
    float start_threshold;
    float stop_threshold;
    float speech_score = 0.0f;
    float band_sum = 0.0f;
    float low_fraction;
    unsigned int active_bands = 0u;
    unsigned int b;

    if (frame_energy < PROJECT3_VAD_MIN_FLOOR) {
        frame_energy = PROJECT3_VAD_MIN_FLOOR;
    }

    vad->smooth_energy = smooth_alpha * vad->smooth_energy + (1.0f - smooth_alpha) * frame_energy;
    for (b = 0u; b < PROJECT3_VAD_BAND_COUNT; b++) {
        vad->band_smooth[b] =
            PROJECT3_VAD_BAND_SMOOTH_ALPHA * vad->band_smooth[b] +
            (1.0f - PROJECT3_VAD_BAND_SMOOTH_ALPHA) * features->band_energy[b];
    }

    if (vad->calibrate_frames < PROJECT3_VAD_CALIB_FRAMES) {
        float cal_alpha = (vad->calibrate_frames == 0u) ? 0.0f : 0.70f;
        vad->noise_floor = cal_alpha * vad->noise_floor + (1.0f - cal_alpha) * frame_energy;
        vad->smooth_energy = vad->noise_floor;
        for (b = 0u; b < PROJECT3_VAD_BAND_COUNT; b++) {
            float previous = vad->band_noise[b];
            float sample = features->band_energy[b];
            float delta;
            vad->band_noise[b] = cal_alpha * previous + (1.0f - cal_alpha) * sample;
            delta = fabsf(sample - vad->band_noise[b]);
            vad->band_deviation[b] =
                cal_alpha * vad->band_deviation[b] + (1.0f - cal_alpha) * delta;
            if (vad->band_deviation[b] < PROJECT3_VAD_BAND_FLOOR) {
                vad->band_deviation[b] = PROJECT3_VAD_BAND_FLOOR;
            }
            vad->band_smooth[b] = vad->band_noise[b];
        }
        vad->calibrate_frames++;
        g_project3_vad_calibrate_frames = vad->calibrate_frames;
        vad->speech_hold_frames = 0;
        vad->silence_hold_frames = 0;
        return 0;
    }

    /* Restore the proven scalar adaptive trigger.  The band model is updated
     * and exposed for the post-capture breath veto, but it has no authority
     * to start or prolong an utterance. */
    if (!vad->active) {
        if (vad->smooth_energy > vad->noise_floor) {
            vad->noise_floor = floor_rise_alpha * vad->noise_floor +
                (1.0f - floor_rise_alpha) * vad->smooth_energy;
        } else {
            vad->noise_floor = floor_fall_alpha * vad->noise_floor +
                (1.0f - floor_fall_alpha) * vad->smooth_energy;
        }
        if (vad->noise_floor < PROJECT3_VAD_MIN_FLOOR) {
            vad->noise_floor = PROJECT3_VAD_MIN_FLOOR;
        }

        for (b = 0u; b < PROJECT3_VAD_BAND_COUNT; b++) {
            float alpha = (vad->band_smooth[b] > vad->band_noise[b])
                ? floor_rise_alpha : floor_fall_alpha;
            float new_noise = alpha * vad->band_noise[b] +
                (1.0f - alpha) * vad->band_smooth[b];
            float deviation = fabsf(vad->band_smooth[b] - new_noise);
            vad->band_noise[b] = new_noise;
            vad->band_deviation[b] = alpha * vad->band_deviation[b] +
                (1.0f - alpha) * deviation;
            if (vad->band_noise[b] < PROJECT3_VAD_BAND_FLOOR) {
                vad->band_noise[b] = PROJECT3_VAD_BAND_FLOOR;
            }
            if (vad->band_deviation[b] < PROJECT3_VAD_BAND_FLOOR) {
                vad->band_deviation[b] = PROJECT3_VAD_BAND_FLOOR;
            }
        }
    }

    start_threshold = vad->noise_floor * start_ratio;
    if (start_threshold < PROJECT3_VAD_ABS_START_ENERGY) {
        start_threshold = PROJECT3_VAD_ABS_START_ENERGY;
    }
    g_project3_vad_start_threshold = start_threshold;
    g_project3_vad_calibrate_frames = vad->calibrate_frames;
    stop_threshold = vad->noise_floor * stop_ratio;
    if (stop_threshold < PROJECT3_VAD_ABS_START_ENERGY * 0.45f) {
        stop_threshold = PROJECT3_VAD_ABS_START_ENERGY * 0.45f;
    }

    for (b = 0u; b < PROJECT3_VAD_BAND_COUNT; b++) {
        float baseline = vad->band_noise[b];
        float ratio;
        unsigned int ratio_permille;
        if (baseline < PROJECT3_VAD_BAND_FLOOR) {
            baseline = PROJECT3_VAD_BAND_FLOOR;
        }
        ratio = vad->band_smooth[b] / baseline;
        band_sum += vad->band_smooth[b];
        ratio_permille = (unsigned int)(ratio * 1000.0f + 0.5f);
        if (ratio_permille > 9999u) ratio_permille = 9999u;
        g_project3_vad_band_ratio_permille[b] = ratio_permille;

        if (b > 0u) {
            float excess = ratio - 1.0f;
            if (ratio >= PROJECT3_VAD_ACTIVE_BAND_RATIO) {
                active_bands++;
            }
            if (excess > 0.0f) {
                if (excess > 4.0f) excess = 4.0f;
                speech_score += excess * score_weights[b];
            }
        }
    }

    low_fraction = (band_sum > PROJECT3_VAD_BAND_FLOOR)
        ? (vad->band_smooth[0] / band_sum) : 0.0f;
    vad->speech_score = speech_score;
    vad->low_fraction = low_fraction;
    vad->active_bands = (unsigned char)active_bands;
    g_project3_vad_speech_score_permille =
        (unsigned int)(speech_score * 1000.0f + 0.5f);
    if (g_project3_vad_speech_score_permille > 9999u) {
        g_project3_vad_speech_score_permille = 9999u;
    }
    g_project3_vad_low_fraction_permille =
        (unsigned int)(low_fraction * 1000.0f + 0.5f);
    if (g_project3_vad_low_fraction_permille > 1000u) {
        g_project3_vad_low_fraction_permille = 1000u;
    }
    g_project3_vad_active_band_count = active_bands;

    if (vad->smooth_energy > start_threshold) {
        if (vad->speech_hold_frames < 255) vad->speech_hold_frames++;
    } else {
        vad->speech_hold_frames = 0;
    }

    if (vad->active) {
        if (vad->smooth_energy < stop_threshold) {
            if (vad->silence_hold_frames < 255) vad->silence_hold_frames++;
        } else {
            vad->silence_hold_frames = 0;
        }
    }

    if (!vad->active && vad->speech_hold_frames >= start_frames) {
        vad->active = 1;
        vad->speech_hold_frames = 0;
        vad->silence_hold_frames = 0;
        Project3_SetAppState(ctx, PROJECT3_APP_SPEECH);
        g_project3_vad_start_count++;
        return 1;
    }

    return 0;
}

static unsigned char Project3_ShouldEndUtterance(PROJECT3_CONTEXT *ctx)
{
    /* With four VAD updates per 1024-sample ADC block, 21 quiet updates are
     * about 270 ms in practice and protect slow command endings. */
    const unsigned short stop_frames = 21;
    const unsigned int min_samples = PROJECT3_MIN_UTTERANCE_SAMPLES;

    if (ctx->vad.active && ctx->vad.silence_hold_frames >= stop_frames) {
        ctx->vad.active = 0;
        ctx->vad.silence_hold_frames = 0;
        if (ctx->utter.count >= min_samples) {
            ctx->utter.ready = 1;
            return 1;
        }
        Project3_ResetUtterance(ctx);
    }

    if (ctx->utter.count >= PROJECT3_RAW_MAX_SAMPLES) {
        ctx->vad.active = 0;
        ctx->utter.ready = 1;
        return 1;
    }

    return 0;
}

static void Project3_AppendRawBlock(PROJECT3_CONTEXT *ctx, const short *block, unsigned int block_samples)
{
    unsigned int i;
    for (i = 0; i < block_samples && ctx->utter.count < PROJECT3_RAW_MAX_SAMPLES; i++) {
        ctx->utter.raw[ctx->utter.count++] = block[i];
    }
}

static void Project3_ResetPreroll(void)
{
    g_project3_preroll_pos = 0;
    g_project3_preroll_count = 0;
}

static void Project3_UpdatePreroll(const short *block, unsigned int block_samples)
{
    unsigned int i;

    for (i = 0; i < block_samples; i++) {
        g_project3_preroll[g_project3_preroll_pos] = block[i];
        g_project3_preroll_pos++;
        if (g_project3_preroll_pos >= PROJECT3_PREROLL_SAMPLES) {
            g_project3_preroll_pos = 0;
        }
        if (g_project3_preroll_count < PROJECT3_PREROLL_SAMPLES) {
            g_project3_preroll_count++;
        }
    }
}

static void Project3_AppendPreroll(PROJECT3_CONTEXT *ctx)
{
    unsigned int i;
    unsigned int start;

    if (g_project3_preroll_count == 0u) {
        return;
    }

    start = (g_project3_preroll_pos + PROJECT3_PREROLL_SAMPLES - g_project3_preroll_count) %
            PROJECT3_PREROLL_SAMPLES;
    for (i = 0; i < g_project3_preroll_count && ctx->utter.count < PROJECT3_RAW_MAX_SAMPLES; i++) {
        unsigned int idx = start + i;
        if (idx >= PROJECT3_PREROLL_SAMPLES) {
            idx -= PROJECT3_PREROLL_SAMPLES;
        }
        ctx->utter.raw[ctx->utter.count++] = g_project3_preroll[idx];
    }
}

static void Project3_SuppressMusicReference(short *mic, const short *music,
                                            unsigned int sample_count)
{
    static unsigned char reference_active = 0u;
    unsigned int i;
    unsigned int mic_peak = 0u;
    unsigned int music_peak = 0u;
    unsigned int residual_peak = 0u;
    float mic_sum = 0.0f;
    float music_sum = 0.0f;
    float mic_mean;
    float music_mean;
    float mic_energy = 0.0f;
    float music_energy = 0.0f;
    float covariance = 0.0f;
    float correlation_sq;
    float cancellation_gain;
    float residual_energy = 0.0f;
    float residual_ratio;

    if (!g_project3_music_ref_enabled || sample_count == 0u) {
        reference_active = 0u;
        g_project3_music_ref_active = 0u;
        g_project3_music_ref_correlated = 0u;
        g_project3_music_ref_corr_permille = 0u;
        g_project3_music_ref_gain_q15 = 0;
        g_project3_music_ref_residual_permille = 1000u;
        g_project3_music_ref_residual_peak = 0u;
        return;
    }

    for (i = 0u; i < sample_count; i++) {
        int mic_sample = (int)mic[i];
        int music_sample = (int)music[i];
        unsigned int mic_magnitude = (unsigned int)
            ((mic_sample < 0) ? -mic_sample : mic_sample);
        unsigned int music_magnitude = (unsigned int)
            ((music_sample < 0) ? -music_sample : music_sample);

        if (mic_magnitude > mic_peak) mic_peak = mic_magnitude;
        if (music_magnitude > music_peak) music_peak = music_magnitude;
        mic_sum += (float)mic_sample;
        music_sum += (float)music_sample;
    }

    if (reference_active) {
        if (music_peak < PROJECT3_MUSIC_REF_OFF_PEAK) {
            reference_active = 0u;
        }
    } else if (music_peak >= PROJECT3_MUSIC_REF_ON_PEAK) {
        reference_active = 1u;
    }
    g_project3_music_ref_active = reference_active;

    if (!reference_active) {
        g_project3_music_ref_correlated = 0u;
        g_project3_music_ref_corr_permille = 0u;
        g_project3_music_ref_gain_q15 = 0;
        g_project3_music_ref_residual_permille = 1000u;
        g_project3_music_ref_residual_peak = mic_peak;
        return;
    }

    mic_mean = mic_sum / (float)sample_count;
    music_mean = music_sum / (float)sample_count;
    for (i = 0u; i < sample_count; i++) {
        float x = (float)mic[i] - mic_mean;
        float r = (float)music[i] - music_mean;
        mic_energy += x * x;
        music_energy += r * r;
        covariance += x * r;
    }

    if (mic_energy <= 1.0f || music_energy <= 1.0f) {
        g_project3_music_ref_correlated = 0u;
        g_project3_music_ref_corr_permille = 0u;
        g_project3_music_ref_gain_q15 = 0;
        g_project3_music_ref_residual_permille = 1000u;
        g_project3_music_ref_residual_peak = mic_peak;
        return;
    }

    correlation_sq = (covariance * covariance) /
        (mic_energy * music_energy);
    if (correlation_sq > 1.0f) correlation_sq = 1.0f;
    g_project3_music_ref_corr_permille =
        (unsigned int)(sqrtf(correlation_sq) * 1000.0f + 0.5f);

    if (correlation_sq < PROJECT3_MUSIC_REF_MIN_CORR_SQ) {
        g_project3_music_ref_correlated = 0u;
        g_project3_music_ref_gain_q15 = 0;
        g_project3_music_ref_residual_permille = 1000u;
        g_project3_music_ref_residual_peak = mic_peak;
        return;
    }

    cancellation_gain = covariance / music_energy;
    if (cancellation_gain > PROJECT3_MUSIC_REF_MAX_ABS_GAIN) {
        cancellation_gain = PROJECT3_MUSIC_REF_MAX_ABS_GAIN;
    } else if (cancellation_gain < -PROJECT3_MUSIC_REF_MAX_ABS_GAIN) {
        cancellation_gain = -PROJECT3_MUSIC_REF_MAX_ABS_GAIN;
    }

    g_project3_music_ref_correlated = 1u;
    g_project3_music_ref_gain_q15 =
        (int)(cancellation_gain * 32768.0f);

    for (i = 0u; i < sample_count; i++) {
        float reference = (float)music[i] - music_mean;
        float residual = ((float)mic[i] - mic_mean) -
            cancellation_gain * reference;
        int output_sample;
        unsigned int magnitude;

        residual_energy += residual * residual;
        output_sample = (residual >= 0.0f) ?
            (int)(residual + 0.5f) : (int)(residual - 0.5f);
        if (output_sample > 32767) {
            output_sample = 32767;
        } else if (output_sample < -32768) {
            output_sample = -32768;
        }
        mic[i] = (short)output_sample;
        magnitude = (unsigned int)
            ((output_sample < 0) ? -output_sample : output_sample);
        if (magnitude > residual_peak) residual_peak = magnitude;
    }

    residual_ratio = residual_energy / mic_energy;
    if (residual_ratio > 9.999f) residual_ratio = 9.999f;
    g_project3_music_ref_residual_permille =
        (unsigned int)(residual_ratio * 1000.0f + 0.5f);
    g_project3_music_ref_residual_peak = residual_peak;
}

static void Project3_CopyInputBlock(short *dst, unsigned char completed_buffer)
{
    short *src = (completed_buffer == AD_BUFFER_PING) ? AD_CH1_Buf0 : AD_CH1_Buf1;
#if PROJECT3_ENABLE_REALTIME_MUSIC
    short *music_left =
        (completed_buffer == AD_BUFFER_PING) ? AD_CH3_Buf0 : AD_CH3_Buf1;
    short *music_right =
        (completed_buffer == AD_BUFFER_PING) ? AD_CH4_Buf0 : AD_CH4_Buf1;
    unsigned int i;
#endif

    /* EDMA writes the ADC ping-pong buffers without maintaining the DSP
     * cache.  Invalidate before the CPU reads them or VAD can repeatedly see
     * an old/zero cache line even though fresh audio has arrived. */
    CacheInv((unsigned int)src,
             sizeof(short) * PROJECT3_BLOCK_SAMPLES);
    CacheInv((unsigned int)music_left,
             sizeof(short) * PROJECT3_BLOCK_SAMPLES);
    CacheInv((unsigned int)music_right,
             sizeof(short) * PROJECT3_BLOCK_SAMPLES);
    memcpy(dst, src, sizeof(short) * PROJECT3_BLOCK_SAMPLES);
#if PROJECT3_ENABLE_REALTIME_MUSIC
    /* g_project3_output_block is unused by the real-time music branch, so
     * reuse it as a temporary stereo-to-mono reference without consuming the
     * remaining L2 memory. */
    for (i = 0u; i < PROJECT3_BLOCK_SAMPLES; i++) {
        g_project3_output_block[i] =
            (short)(((int)music_left[i] + (int)music_right[i]) / 2);
    }
    Project3_SuppressMusicReference(dst, g_project3_output_block,
                                    PROJECT3_BLOCK_SAMPLES);
#endif
}

static void Project3_FillDacOutput(const PROJECT3_CONTEXT *ctx, const short *src)
{
    unsigned int i;
    if (ctx->pass_through_enable) {
        memcpy(g_project3_output_block, src, sizeof(short) * PROJECT3_BLOCK_SAMPLES);
    } else {
        for (i = 0; i < PROJECT3_BLOCK_SAMPLES; i++) {
            g_project3_output_block[i] = 0;
        }
    }
}

static void Project3_WriteOutputBlockToDac(const short *src)
{
    short *dst;

    if (DA_Ping_Pong == DA_BUFFER_PONG) {
        dst = DA_CH1_Buf0;
    } else {
        dst = DA_CH1_Buf1;
    }
    memcpy(dst, src, sizeof(short) * PROJECT3_BLOCK_SAMPLES);
    CacheWB((unsigned int)dst,
            sizeof(short) * PROJECT3_BLOCK_SAMPLES);
}

/* The BC-ResNet pointwise layers are dense matrix products with no spatial
 * padding.  Keeping them in the six-loop generic convolution pays kernel-loop,
 * boundary-check and index-multiply overhead for every MAC.  This path keeps
 * the original input-channel accumulation order while walking channel planes
 * with pointers, so it is numerically equivalent but easier for the C674x
 * compiler to software-pipeline. */
static void Project3_PointwiseConv2d(const float *in, float *out,
                                     int in_c, int in_h, int in_w,
                                     int out_c, int out_h, int out_w,
                                     int s_h, int s_w,
                                     const float *weight)
{
    int oc;
    int oh;
    int ow;
    int ic;
    int pos;
    int in_index;
    int in_plane = in_h * in_w;
    int out_plane = out_h * out_w;
    int in_stride2 = in_plane * 2;
    int in_stride3 = in_plane * 3;
    int in_stride4 = in_plane * 4;
    float sum;
    const float *weight_ptr;
    const float *in_ptr;
    float *out_ptr;

    if (s_h == 1 && s_w == 1 &&
        in_h == out_h && in_w == out_w) {
        for (oc = 0; oc < out_c; oc++) {
            weight_ptr = &weight[oc * in_c];
            out_ptr = &out[oc * out_plane];
            for (pos = 0; pos < out_plane; pos++) {
                sum = 0.0f;
                in_ptr = &in[pos];
                for (ic = 0; ic + 3 < in_c; ic += 4) {
                    sum += in_ptr[0] * weight_ptr[ic];
                    sum += in_ptr[in_plane] * weight_ptr[ic + 1];
                    sum += in_ptr[in_stride2] * weight_ptr[ic + 2];
                    sum += in_ptr[in_stride3] * weight_ptr[ic + 3];
                    in_ptr += in_stride4;
                }
                for (; ic < in_c; ic++) {
                    sum += in_ptr[0] * weight_ptr[ic];
                    in_ptr += in_plane;
                }
                out_ptr[pos] = sum;
            }
        }
        return;
    }

    for (oc = 0; oc < out_c; oc++) {
        weight_ptr = &weight[oc * in_c];
        out_ptr = &out[oc * out_plane];
        for (oh = 0; oh < out_h; oh++) {
            for (ow = 0; ow < out_w; ow++) {
                in_index = oh * s_h * in_w + ow * s_w;
                sum = 0.0f;
                in_ptr = &in[in_index];
                for (ic = 0; ic + 3 < in_c; ic += 4) {
                    sum += in_ptr[0] * weight_ptr[ic];
                    sum += in_ptr[in_plane] * weight_ptr[ic + 1];
                    sum += in_ptr[in_stride2] * weight_ptr[ic + 2];
                    sum += in_ptr[in_stride3] * weight_ptr[ic + 3];
                    in_ptr += in_stride4;
                }
                for (; ic < in_c; ic++) {
                    sum += in_ptr[0] * weight_ptr[ic];
                    in_ptr += in_plane;
                }
                *out_ptr++ = sum;
            }
        }
    }
}

static void Project3_Conv2d(const float *in, float *out,
                            int in_c, int in_h, int in_w,
                            int out_c, int out_h, int out_w,
                            int k_h, int k_w, int s_h, int s_w,
                            int p_h, int p_w, const float *weight)
{
    int oc, oh, ow, ic, kh, kw;
    unsigned long long profile_start;
    unsigned long long profile_end;

    profile_start = _itoll(TSCH, TSCL);
    if (k_h == 1 && k_w == 1 && p_h == 0 && p_w == 0) {
        Project3_PointwiseConv2d(in, out,
                                in_c, in_h, in_w,
                                out_c, out_h, out_w,
                                s_h, s_w, weight);
        profile_end = _itoll(TSCH, TSCL);
        g_project3_profile_conv_cycles += profile_end - profile_start;
        g_project3_profile_pointwise_cycles += profile_end - profile_start;
        return;
    }

    for (oc = 0; oc < out_c; oc++) {
        for (oh = 0; oh < out_h; oh++) {
            for (ow = 0; ow < out_w; ow++) {
                float sum = 0.0f;
                for (ic = 0; ic < in_c; ic++) {
                    for (kh = 0; kh < k_h; kh++) {
                        int ih = oh * s_h + kh - p_h;
                        if (ih < 0 || ih >= in_h) continue;
                        for (kw = 0; kw < k_w; kw++) {
                            int iw = ow * s_w + kw - p_w;
                            if (iw < 0 || iw >= in_w) continue;
                            sum += in[P3_IDX3(ic, ih, iw, in_h, in_w)] * weight[P3_W4(oc, ic, kh, kw, in_c, k_h, k_w)];
                        }
                    }
                }
                out[P3_IDX3(oc, oh, ow, out_h, out_w)] = sum;
            }
        }
    }
    profile_end = _itoll(TSCH, TSCL);
    g_project3_profile_conv_cycles += profile_end - profile_start;
}

static void Project3_DepthwiseConv2d(const float *in, float *out,
                                     int channels, int in_h, int in_w,
                                     int out_h, int out_w,
                                     int k_h, int k_w, int s_h, int s_w,
                                     int p_h, int p_w, int d_h, int d_w,
                                     const float *weight)
{
    int c, oh, ow, kh, kw;
    unsigned long long profile_start;
    unsigned long long profile_end;

    profile_start = _itoll(TSCH, TSCL);
    for (c = 0; c < channels; c++) {
        for (oh = 0; oh < out_h; oh++) {
            for (ow = 0; ow < out_w; ow++) {
                float sum = 0.0f;
                for (kh = 0; kh < k_h; kh++) {
                    int ih = oh * s_h + kh * d_h - p_h;
                    if (ih < 0 || ih >= in_h) continue;
                    for (kw = 0; kw < k_w; kw++) {
                        int iw = ow * s_w + kw * d_w - p_w;
                        if (iw < 0 || iw >= in_w) continue;
                        sum += in[P3_IDX3(c, ih, iw, in_h, in_w)] * weight[P3_DW(c, kh, kw, k_h, k_w)];
                    }
                }
                out[P3_IDX3(c, oh, ow, out_h, out_w)] = sum;
            }
        }
    }
    profile_end = _itoll(TSCH, TSCL);
    g_project3_profile_depthwise_cycles += profile_end - profile_start;
}

static void Project3_BatchNorm(float *x, int channels, int h, int w,
                               const float *gamma, const float *beta,
                               const float *mean, const float *var,
                               unsigned char do_relu)
{
    int c, i;
    int hw = h * w;
    unsigned long long profile_start;
    unsigned long long profile_end;

    profile_start = _itoll(TSCH, TSCL);
    for (c = 0; c < channels; c++) {
        float scale = gamma[c] / sqrtf(var[c] + PROJECT3_BN_EPS);
        float bias = beta[c] - mean[c] * scale;
        float *ptr = &x[c * hw];
        for (i = 0; i < hw; i++) {
            float v = ptr[i] * scale + bias;
            if (do_relu && v < 0.0f) v = 0.0f;
            ptr[i] = v;
        }
    }
    profile_end = _itoll(TSCH, TSCL);
    g_project3_profile_bn_cycles += profile_end - profile_start;
}

static void Project3_AddRelu(float *x, const float *identity, int count)
{
    int i;
    unsigned long long profile_start;
    unsigned long long profile_end;

    profile_start = _itoll(TSCH, TSCL);
    for (i = 0; i < count; i++) {
        float v = x[i] + identity[i];
        x[i] = (v < 0.0f) ? 0.0f : v;
    }
    profile_end = _itoll(TSCH, TSCL);
    g_project3_profile_addrelu_cycles += profile_end - profile_start;
}

static void Project3_BCResBlock(const float *in, float *out,
                                int in_c, int in_h, int in_w,
                                int out_c, int out_h, int out_w,
                                int stride_h, int stride_w,
                                int temporal_dilation,
                                const float *conv1_w,
                                const float *bn1_w, const float *bn1_b, const float *bn1_m, const float *bn1_v,
                                const float *dw_w,
                                const float *bn2_w, const float *bn2_b, const float *bn2_m, const float *bn2_v,
                                const float *conv2_w,
                                const float *bn3_w, const float *bn3_b, const float *bn3_m, const float *bn3_v,
                                const float *shortcut_w,
                                const float *shortcut_bn_w, const float *shortcut_bn_b,
                                const float *shortcut_bn_m, const float *shortcut_bn_v)
{
    Project3_Conv2d(in, out, in_c, in_h, in_w, out_c, in_h, in_w, 1, 1, 1, 1, 0, 0, conv1_w);
    Project3_BatchNorm(out, out_c, in_h, in_w, bn1_w, bn1_b, bn1_m, bn1_v, 1);

    Project3_DepthwiseConv2d(out, g_project3_act_c,
                             out_c, in_h, in_w, out_h, out_w,
                             3, 3, stride_h, stride_w,
                             1, temporal_dilation,
                             1, temporal_dilation, dw_w);
    Project3_BatchNorm(g_project3_act_c, out_c, out_h, out_w, bn2_w, bn2_b, bn2_m, bn2_v, 1);

    Project3_Conv2d(g_project3_act_c, out, out_c, out_h, out_w, out_c, out_h, out_w, 1, 1, 1, 1, 0, 0, conv2_w);
    Project3_BatchNorm(out, out_c, out_h, out_w, bn3_w, bn3_b, bn3_m, bn3_v, 0);

    Project3_Conv2d(in, g_project3_act_c, in_c, in_h, in_w, out_c, out_h, out_w, 1, 1, stride_h, stride_w, 0, 0, shortcut_w);
    Project3_BatchNorm(g_project3_act_c, out_c, out_h, out_w, shortcut_bn_w, shortcut_bn_b, shortcut_bn_m, shortcut_bn_v, 0);

    Project3_AddRelu(out, g_project3_act_c, out_c * out_h * out_w);
}

static void Project3_BCResNetForward(const float *logmel, float *logits,
                                     float *speech_logit)
{
    int c, w, cls, i;
    unsigned long long total_start;
    unsigned long long total_end;
    unsigned long long stage_start;
    unsigned long long stage_end;
    unsigned long long stem_cycles;
    unsigned long long block1_cycles;
    unsigned long long block2_cycles;
    unsigned long long block3_cycles;
    unsigned long long head_cycles;
    unsigned long long pool_fc_cycles;

    total_start = _itoll(TSCH, TSCL);
    stage_start = _itoll(TSCH, TSCL);
    Project3_Conv2d(logmel, g_project3_act_a, 1, 40, PROJECT3_MODEL_FRAMES,
                    16, 20, PROJECT3_MODEL_FRAMES, 3, 3, 2, 1, 1, 1, conv1_weight);
    Project3_BatchNorm(g_project3_act_a, 16, 20, PROJECT3_MODEL_FRAMES,
                       bn1_weight, bn1_bias, bn1_running_mean, bn1_running_var, 1);
    stage_end = _itoll(TSCH, TSCL);
    stem_cycles = stage_end - stage_start;

    stage_start = _itoll(TSCH, TSCL);
    Project3_BCResBlock(g_project3_act_a, g_project3_act_b,
                        16, 20, PROJECT3_MODEL_FRAMES,
                        8, 20, PROJECT3_MODEL_FRAMES, 1, 1, 1,
                        layer1_conv1_weight, layer1_bn1_weight, layer1_bn1_bias, layer1_bn1_running_mean, layer1_bn1_running_var,
                        layer1_dwconv_weight, layer1_bn2_weight, layer1_bn2_bias, layer1_bn2_running_mean, layer1_bn2_running_var,
                        layer1_conv2_weight, layer1_bn3_weight, layer1_bn3_bias, layer1_bn3_running_mean, layer1_bn3_running_var,
                        layer1_shortcut_0_weight, layer1_shortcut_1_weight, layer1_shortcut_1_bias,
                        layer1_shortcut_1_running_mean, layer1_shortcut_1_running_var);
    stage_end = _itoll(TSCH, TSCL);
    block1_cycles = stage_end - stage_start;

    stage_start = _itoll(TSCH, TSCL);
    Project3_BCResBlock(g_project3_act_b, g_project3_act_a,
                        8, 20, PROJECT3_MODEL_FRAMES,
                        12, 10, PROJECT3_MODEL_FRAMES, 2, 1, 2,
                        layer2_conv1_weight, layer2_bn1_weight, layer2_bn1_bias, layer2_bn1_running_mean, layer2_bn1_running_var,
                        layer2_dwconv_weight, layer2_bn2_weight, layer2_bn2_bias, layer2_bn2_running_mean, layer2_bn2_running_var,
                        layer2_conv2_weight, layer2_bn3_weight, layer2_bn3_bias, layer2_bn3_running_mean, layer2_bn3_running_var,
                        layer2_shortcut_0_weight, layer2_shortcut_1_weight, layer2_shortcut_1_bias,
                        layer2_shortcut_1_running_mean, layer2_shortcut_1_running_var);
    stage_end = _itoll(TSCH, TSCL);
    block2_cycles = stage_end - stage_start;

    stage_start = _itoll(TSCH, TSCL);
    Project3_BCResBlock(g_project3_act_a, g_project3_act_b,
                        12, 10, PROJECT3_MODEL_FRAMES,
                        16, 5, PROJECT3_MODEL_FRAMES, 2, 1, 4,
                        layer3_conv1_weight, layer3_bn1_weight, layer3_bn1_bias, layer3_bn1_running_mean, layer3_bn1_running_var,
                        layer3_dwconv_weight, layer3_bn2_weight, layer3_bn2_bias, layer3_bn2_running_mean, layer3_bn2_running_var,
                        layer3_conv2_weight, layer3_bn3_weight, layer3_bn3_bias, layer3_bn3_running_mean, layer3_bn3_running_var,
                        layer3_shortcut_0_weight, layer3_shortcut_1_weight, layer3_shortcut_1_bias,
                        layer3_shortcut_1_running_mean, layer3_shortcut_1_running_var);
    stage_end = _itoll(TSCH, TSCL);
    block3_cycles = stage_end - stage_start;

    stage_start = _itoll(TSCH, TSCL);
    Project3_DepthwiseConv2d(g_project3_act_b, g_project3_act_a,
                             16, 5, PROJECT3_MODEL_FRAMES, 5, PROJECT3_MODEL_FRAMES,
                             3, 3, 1, 1, 1, 8, 1, 8, dwconv_weight);
    Project3_BatchNorm(g_project3_act_a, 16, 5, PROJECT3_MODEL_FRAMES,
                       bn_dw_weight, bn_dw_bias, bn_dw_running_mean, bn_dw_running_var, 1);

    Project3_Conv2d(g_project3_act_a, g_project3_act_b,
                    16, 5, PROJECT3_MODEL_FRAMES, 20, 5, PROJECT3_MODEL_FRAMES,
                    1, 1, 1, 1, 0, 0, pwconv_weight);
    Project3_BatchNorm(g_project3_act_b, 20, 5, PROJECT3_MODEL_FRAMES,
                       bn_pw_weight, bn_pw_bias, bn_pw_running_mean, bn_pw_running_var, 1);

    Project3_Conv2d(g_project3_act_b, g_project3_act_a,
                    20, 5, PROJECT3_MODEL_FRAMES, 20, 1, PROJECT3_MODEL_FRAMES,
                    5, 1, 1, 1, 0, 0, conv2_weight);
    Project3_BatchNorm(g_project3_act_a, 20, 1, PROJECT3_MODEL_FRAMES,
                       bn2_weight, bn2_bias, bn2_running_mean, bn2_running_var, 1);

    Project3_Conv2d(g_project3_act_a, g_project3_act_b,
                    20, 1, PROJECT3_MODEL_FRAMES, 32, 1, PROJECT3_MODEL_FRAMES,
                    1, 1, 1, 1, 0, 0, conv_expand_weight);
    Project3_BatchNorm(g_project3_act_b, 32, 1, PROJECT3_MODEL_FRAMES,
                       bn_expand_weight, bn_expand_bias, bn_expand_running_mean, bn_expand_running_var, 1);
    stage_end = _itoll(TSCH, TSCL);
    head_cycles = stage_end - stage_start;

    stage_start = _itoll(TSCH, TSCL);
    for (c = 0; c < 32; c++) {
        float sum = 0.0f;
        float maximum =
            g_project3_act_b[P3_IDX3(c, 0, 0, 1, PROJECT3_MODEL_FRAMES)];
        for (w = 0; w < PROJECT3_MODEL_FRAMES; w++) {
            float value =
                g_project3_act_b[P3_IDX3(c, 0, w,
                                         1, PROJECT3_MODEL_FRAMES)];
            sum += value;
            if (value > maximum) {
                maximum = value;
            }
        }
        g_project3_fc_in[c] = sum / (float)PROJECT3_MODEL_FRAMES;
        g_project3_fc_in[32 + c] = maximum;
    }

    for (cls = 0; cls < PROJECT3_CMD_COUNT; cls++) {
        float sum = fc_bias[cls];
        for (i = 0; i < PROJECT3_EMBEDDING_SIZE; i++) {
            sum += g_project3_fc_in[i] *
                   fc_weight[cls * PROJECT3_EMBEDDING_SIZE + i];
        }
        logits[cls] = sum;
    }
    if (speech_logit != NULL) {
        float sum = speech_head_bias[0];
        for (i = 0; i < PROJECT3_EMBEDDING_SIZE; i++) {
            sum += g_project3_fc_in[i] * speech_head_weight[i];
        }
        *speech_logit = sum;
    }
    stage_end = _itoll(TSCH, TSCL);
    pool_fc_cycles = stage_end - stage_start;
    total_end = _itoll(TSCH, TSCL);

    g_project3_profile_stem_cycles = stem_cycles;
    g_project3_profile_stem_us = Project3_ProfileCyclesToUs(stem_cycles);
    g_project3_profile_block1_cycles = block1_cycles;
    g_project3_profile_block1_us = Project3_ProfileCyclesToUs(block1_cycles);
    g_project3_profile_block2_cycles = block2_cycles;
    g_project3_profile_block2_us = Project3_ProfileCyclesToUs(block2_cycles);
    g_project3_profile_block3_cycles = block3_cycles;
    g_project3_profile_block3_us = Project3_ProfileCyclesToUs(block3_cycles);
    g_project3_profile_head_cycles = head_cycles;
    g_project3_profile_head_us = Project3_ProfileCyclesToUs(head_cycles);
    g_project3_profile_pool_fc_cycles = pool_fc_cycles;
    g_project3_profile_pool_fc_us = Project3_ProfileCyclesToUs(pool_fc_cycles);
    g_project3_profile_cnn_cycles = total_end - total_start;
    g_project3_profile_cnn_us =
        Project3_ProfileCyclesToUs(total_end - total_start);

    g_project3_profile_conv_us =
        Project3_ProfileCyclesToUs(g_project3_profile_conv_cycles);
    g_project3_profile_pointwise_us =
        Project3_ProfileCyclesToUs(g_project3_profile_pointwise_cycles);
    g_project3_profile_depthwise_us =
        Project3_ProfileCyclesToUs(g_project3_profile_depthwise_cycles);
    g_project3_profile_bn_us =
        Project3_ProfileCyclesToUs(g_project3_profile_bn_cycles);
    g_project3_profile_addrelu_us =
        Project3_ProfileCyclesToUs(g_project3_profile_addrelu_cycles);
}

static PROJECT3_INFER_RESULT Project3_LogitsToResult(const float *logits)
{
    PROJECT3_INFER_RESULT result;
    int i;
    int best = 0;
    int second = 1;
    float max_logit = logits[0];
    float second_logit;
    float sum_exp = 0.0f;
    float best_prob;
    float second_prob;

    for (i = 1; i < PROJECT3_CMD_COUNT; i++) {
        if (logits[i] > max_logit) {
            max_logit = logits[i];
            best = i;
        }
    }

    second = (best == 0) ? 1 : 0;
    second_logit = logits[second];
    for (i = 0; i < PROJECT3_CMD_COUNT; i++) {
        if (i != best && logits[i] > second_logit) {
            second_logit = logits[i];
            second = i;
        }
    }

    for (i = 0; i < PROJECT3_CMD_COUNT; i++) {
        sum_exp += expf(logits[i] - max_logit);
    }

    best_prob = expf(logits[best] - max_logit) / sum_exp;
    second_prob = expf(logits[second] - max_logit) / sum_exp;
    result.class_id = (unsigned char)best;
    result.second_class_id = (unsigned char)second;
    result.confidence = best_prob;
    result.second_confidence = second_prob;
    result.margin = best_prob - second_prob;
    result.valid = 1;

    if (best == PROJECT3_CLASS_SILENCE) {
        result.valid = 0;
    } else if (best == PROJECT3_CLASS_UNKNOWN) {
        /* Unknown is a real model output and is useful feedback to the user. */
        result.valid = 1;
    } else if (second == PROJECT3_CLASS_UNKNOWN &&
               second_prob >= PROJECT3_UNKNOWN_COMPETITOR_THRESHOLD &&
               best_prob < PROJECT3_UNKNOWN_COMMAND_OVERRIDE) {
        /* A strong unknown runner-up means the model is unsure whether this
         * audio belongs to the command vocabulary at all. */
        result.valid = 0;
    } else if (best_prob < PROJECT3_INFERENCE_CONF_THRESHOLD) {
        result.valid = 0;
    } else if (result.margin < PROJECT3_INFERENCE_MARGIN_THRESHOLD) {
        result.valid = 0;
    }

    return result;
}

static unsigned char Project3_AcceptResult(const PROJECT3_INFER_RESULT *result)
{
    return result->valid ? 1u : 0u;
}

static unsigned char Project3_IsCommandClass(unsigned char class_id)
{
    return (class_id >= PROJECT3_CLASS_DOWN && class_id < PROJECT3_CMD_COUNT) ? 1u : 0u;
}

static const char *Project3_GetShortLabel(unsigned char class_id)
{
    static const char *short_labels[PROJECT3_CMD_COUNT] = {
        "sil",
        "unk",
        "down",
        "go",
        "left",
        "no",
        "off",
        "on",
        "right",
        "stop",
        "up",
        "yes"
    };

    if (class_id >= PROJECT3_CMD_COUNT) {
        return "unk";
    }
    return short_labels[class_id];
}

static void Project3_FormatRawResult(const PROJECT3_INFER_RESULT *result, char *text, unsigned int text_len)
{
    unsigned int p1;
    unsigned int p2;

    if (text == NULL || text_len == 0u) {
        return;
    }

    p1 = (unsigned int)(result->confidence * 100.0f + 0.5f);
    p2 = (unsigned int)(result->second_confidence * 100.0f + 0.5f);
    if (p1 > 99u) p1 = 99u;
    if (p2 > 99u) p2 = 99u;

    snprintf(text, text_len, "raw %s%u/%s%u",
             Project3_GetShortLabel(result->class_id), p1,
             Project3_GetShortLabel(result->second_class_id), p2);
    text[text_len - 1u] = '\0';
}

static void Project3_HandleSpeechEnd(PROJECT3_CONTEXT *ctx)
{
    PROJECT3_INFER_RESULT result;
    const char *label;
    unsigned char accepted;
    unsigned long long infer_start;
    unsigned long long infer_end;
    unsigned long long infer_cycles;
    unsigned long long accounted_cycles;

    Project3_SetAppState(ctx, PROJECT3_APP_INFERENCING);
    Project3_SetUiText(ctx, "Processing...", "", "");
    Project3_UpdateUi(ctx, 1u);

    g_project3_last_utterance_samples = ctx->utter.count;
    g_project3_result_accepted = 0u;
    Project3_ResetInferenceProfile();
    infer_start = _itoll(TSCH, TSCL);
    result = Project3_RunInference(ctx, &ctx->utter);
    infer_end = _itoll(TSCH, TSCL);
    infer_cycles = infer_end - infer_start;
    g_project3_profile_total_cycles = infer_cycles;
    g_project3_profile_total_us = Project3_ProfileCyclesToUs(infer_cycles);
    accounted_cycles = g_project3_profile_feature_cycles +
                       g_project3_profile_cnn_cycles +
                       g_project3_profile_post_cycles;
    if (infer_cycles > accounted_cycles) {
        g_project3_profile_overhead_cycles = infer_cycles - accounted_cycles;
    } else {
        g_project3_profile_overhead_cycles = 0u;
    }
    g_project3_profile_overhead_us =
        Project3_ProfileCyclesToUs(g_project3_profile_overhead_cycles);
    g_project3_last_inference_ms =
        (unsigned long)(infer_cycles / (PROJECT3_DSP_CLOCK_HZ / 1000u));
    g_project3_raw_top_class = result.class_id;
    g_project3_raw_second_class = result.second_class_id;
    g_project3_raw_top_permille =
        (unsigned int)(result.confidence * 1000.0f + 0.5f);
    g_project3_raw_second_permille =
        (unsigned int)(result.second_confidence * 1000.0f + 0.5f);
    g_project3_raw_margin_permille =
        (unsigned int)(result.margin * 1000.0f + 0.5f);

    if (g_project3_memory_fault) {
        ctx->last_result = result;
        Project3_SetAppState(ctx, PROJECT3_APP_ERROR);
        Project3_SetUiText(ctx, "MEMORY ERROR", "", "");
        ctx->error_hold_blocks = PROJECT3_ERROR_HOLD_BLOCKS;
        ctx->input_gate_blocks = PROJECT3_POST_INFER_IGNORE_BLOCKS;
        Project3_ResetUtterance(ctx);
        return;
    }
    accepted = Project3_AcceptResult(&result);
    g_project3_result_accepted = accepted;
    ctx->input_gate_blocks = PROJECT3_POST_INFER_IGNORE_BLOCKS;
    Project3_ResetUtterance(ctx);
    Project3_ResetPreroll();

    if (!accepted) {
        ctx->last_result = result;
        if (result.class_id != PROJECT3_CLASS_SILENCE) {
            /* A low-confidence command is presented as unknown instead of
             * leaving the user with an apparently frozen Listening screen. */
            Project3_CommitStableWord(ctx, "unknown");
            Project3_StartMessageHold(ctx, PROJECT3_RESULT_HOLD_BLOCKS, 1u);
        } else {
            Project3_SetAppState(ctx, PROJECT3_APP_LISTENING);
            Project3_SetUiText(ctx, P3_LISTENING_TEXT, "", "");
        }
        return;
    }

    Project3_ApplyAcceptedMusicCommand(ctx, result.class_id);
    ctx->last_result = result;
    label = Project3_GetLabel(result.class_id);
    Project3_CommitStableWord(ctx, label);
    ctx->recognized_count++;
    Project3_StartMessageHold(ctx, PROJECT3_RESULT_HOLD_BLOCKS, 1u);
}

#if 0
static unsigned char Project3_RenderScreen(PROJECT3_CONTEXT *ctx, unsigned char force_redraw)
{
    unsigned long color = ClrWhite;

    // 防重入检查
    if (s_lcd_busy) {
        return 0;
    }

    // 只在文字真正改变时重绘
    if (!force_redraw && strncmp(ctx->main_text, ctx->last_main_text, PROJECT3_RESULT_TEXT_LEN) == 0) {
        return 1;
    }

    // 设置忙标志
    s_lcd_busy = 1;

    // 设置文字颜色
    if (ctx->app_state == PROJECT3_APP_RESULT) {
        color = ClrWhite;
    } else {
        color = ClrLightSteelBlue;
    }

    // 原子操作：清空并绘制（不可分割）
    Project3_ClearAndDrawText(ctx->main_text, color);

    // 记录当前文字，避免重复绘制
    strncpy(ctx->last_main_text, ctx->main_text, PROJECT3_RESULT_TEXT_LEN);
    ctx->last_main_text[PROJECT3_RESULT_TEXT_LEN - 1] = '\0';

    // 清除忙标志
    s_lcd_busy = 0;
    return 1;
}

#endif

static unsigned char Project3_RenderScreen(PROJECT3_CONTEXT *ctx, unsigned char force_redraw)
{
    unsigned long color;
    unsigned int volume_percent;
    unsigned char music_playing;

    if (s_lcd_busy) {
        return 0;
    }

    music_playing = g_project3_music_playing;
    volume_percent = g_project3_music_volume_percent;
    if (volume_percent > 100u) {
        volume_percent = 100u;
    }

    if (!force_redraw &&
        strncmp(ctx->main_text, ctx->last_main_text, PROJECT3_RESULT_TEXT_LEN) == 0 &&
        music_playing == ctx->last_music_playing &&
        volume_percent == ctx->last_music_volume_percent) {
        return 1;
    }

    s_lcd_busy = 1;

    color = (strcmp(ctx->main_text, P3_LISTENING_TEXT) == 0) ? ClrLightSteelBlue : ClrWhite;
    if (!Project3_ClearAndDrawText(ctx->main_text, color)) {
        s_lcd_busy = 0;
        return 0;
    }

    strncpy(ctx->last_main_text, ctx->main_text, PROJECT3_RESULT_TEXT_LEN);
    ctx->last_main_text[PROJECT3_RESULT_TEXT_LEN - 1] = '\0';
    ctx->last_music_playing = music_playing;
    ctx->last_music_volume_percent = volume_percent;

    s_lcd_busy = 0;
    return 1;
}

static void Project3_InitContext(PROJECT3_CONTEXT *ctx)
{
    /* Writing TSCL starts the free-running C674x cycle counter used for
     * non-intrusive inference timing. */
    TSCL = 0;
    TSCH = 0;
    memset(ctx, 0, sizeof(PROJECT3_CONTEXT));
    ctx->pass_through_enable = PROJECT3_PASS_THROUGH_ENABLE;
    ctx->model_state = PROJECT3_MODEL_READY;
    ctx->app_state = PROJECT3_APP_BOOT;
    Project3_ResetVadAdaptiveState(&ctx->vad);
    g_project3_music_playing = 1u;
    g_project3_music_volume_level = PROJECT3_MUSIC_DEFAULT_LEVEL;
    g_project3_music_volume_percent =
        (PROJECT3_MUSIC_DEFAULT_LEVEL * 100u) /
        PROJECT3_MUSIC_VOLUME_LEVELS;
    g_project3_music_user_gain_q15 =
        (PROJECT3_MUSIC_DEFAULT_LEVEL * PROJECT3_MUSIC_Q15_FULL_SCALE) /
        PROJECT3_MUSIC_VOLUME_LEVELS;
    g_project3_music_block_count = 0u;
    g_project3_music_sync_miss_count = 0u;
    g_project3_music_command_count = 0u;
    g_project3_music_last_isr_cycles = 0u;
    g_project3_music_max_isr_cycles = 0u;
    g_project3_music_input_peak = 0u;
    g_project3_music_last_command = PROJECT3_CLASS_SILENCE;
    g_project3_music_last_output_buffer = DA_BUFFER_PING;
    g_project3_music_ref_enabled = PROJECT3_ENABLE_MUSIC_REFERENCE;
    g_project3_music_ref_active = 0u;
    g_project3_music_ref_correlated = 0u;
    g_project3_music_ref_corr_permille = 0u;
    g_project3_music_ref_gain_q15 = 0;
    g_project3_music_ref_residual_permille = 1000u;
    g_project3_music_ref_residual_peak = 0u;
    g_project3_music_ref_transition_count = 0u;
    g_project3_adc_completed_buffer = AD_BUFFER_PING;
    g_project3_adc_completed_sequence = 0u;
    g_project3_adc_consumed_sequence = 0u;
    g_project3_adc_drop_count = 0u;
    g_project3_capture_write_sequence = 0u;
    g_project3_capture_read_sequence = 0u;
    g_project3_capture_queue_depth = 0u;
    g_project3_capture_queue_high_water = 0u;
    g_project3_capture_overflow_count = 0u;
    g_project3_capture_gap_reset_count = 0u;
    g_project3_capture_last_source_sequence = 0u;
    g_project3_adc_ch1_peak = 0u;
    g_project3_adc_ch2_peak = 0u;
    g_project3_adc_ch3_peak = 0u;
    g_project3_adc_ch4_peak = 0u;
    s_project3_music_applied_gain_q15 =
        (PROJECT3_MUSIC_DEFAULT_LEVEL * PROJECT3_MUSIC_Q15_FULL_SCALE) /
        PROJECT3_MUSIC_VOLUME_LEVELS;
    strcpy(ctx->main_text, "Booting...");
    strcpy(ctx->line1, "Initializing system");
    strcpy(ctx->line2, "Please wait");
    strcpy(ctx->stable_text, P3_LISTENING_TEXT);
    ctx->stable_text_valid = 0u;
    ctx->last_music_volume_percent = 101u;
    ctx->last_music_playing = 0xFFu;
    ctx->redraw_needed = 1;
    g_project3_fft_dsplib_active = 1u;
    g_project3_fft_validation_done = 0u;
    g_project3_fft_validation_pass = 0u;
    g_project3_fft_validation_error_ppm = 0u;
    Project3_ResetPreroll();
    Project3_ResetMemoryGuard();
    Project3_InitTables();
}

static void Project3_InitUi(PROJECT3_CONTEXT *ctx)
{
    Lcd_Init();

    // 防重入标志初始化
    s_lcd_busy = 0;
    s_lcd_layout_ready = 0u;

    // 全屏清屏一次（只在初始化时）- 使用固定布局宏

    // 初始化 last_main_text 为空，确保第一次一定绘制
    ctx->last_main_text[0] = '\0';

    // 等待LCD稳定
    volatile int i;
    for (i = 0; i < 100000; i++);

    // 绘制初始界面 - 强制刷新
    Project3_RenderScreen(ctx, 1);
}

static void Project3_HandleKeys(PROJECT3_CONTEXT *ctx)
{
#if PROJECT3_ENABLE_KEY_CONTROLS
    if (FLAG_KEY1) {
        FLAG_KEY1 = 0;
        // KEY1: 清除当前结果，回到 listening
        Project3_ResetUtterance(ctx);
        Project3_ResetPreroll();
        ctx->last_result.valid = 0;
        ctx->input_gate_blocks = 0;
        ctx->message_hold_blocks = 0;
        ctx->message_hold_start_sequence = 0u;
        ctx->message_return_to_listening = 0;
        ctx->error_hold_blocks = 0;
        Project3_ClearStableDisplay(ctx);
    }
    if (FLAG_KEY2) {
        unsigned int b;
        FLAG_KEY2 = 0;
        // KEY2: 降低VAD灵敏度（减少误触发）
        ctx->vad.noise_floor *= 1.15f;
        for (b = 0u; b < PROJECT3_VAD_BAND_COUNT; b++) {
            ctx->vad.band_noise[b] *= 1.15f;
        }
        Project3_SetUiText(ctx, "VAD less sensitive", "", "");
        Project3_StartMessageHold(ctx, PROJECT3_MESSAGE_HOLD_BLOCKS, 0u);
    }
    if (FLAG_KEY3) {
        unsigned int b;
        FLAG_KEY3 = 0;
        // KEY3: 提高VAD灵敏度（更容易触发）
        ctx->vad.noise_floor *= 0.85f;
        for (b = 0u; b < PROJECT3_VAD_BAND_COUNT; b++) {
            ctx->vad.band_noise[b] *= 0.85f;
        }
        Project3_SetUiText(ctx, "VAD more sensitive", "", "");
        Project3_StartMessageHold(ctx, PROJECT3_MESSAGE_HOLD_BLOCKS, 0u);
    }
    if (FLAG_KEY4) {
        FLAG_KEY4 = 0;
        // KEY4: 重置VAD基线
        Project3_ResetUtterance(ctx);
        Project3_ResetPreroll();
        Project3_ResetVadAdaptiveState(&ctx->vad);
        Project3_SetUiText(ctx, "Calibrating...", "", "");
        Project3_StartMessageHold(ctx, PROJECT3_MESSAGE_HOLD_BLOCKS, 0u);
    }
    if (FLAG_KEY5) {
        FLAG_KEY5 = 0;
        // KEY5: 系统重启
        Project3_InitContext(ctx);
        Project3_ModelInit(ctx);
    }
#else
    /* Keep the hardware buttons inert in the normal demo build. */
    (void)ctx;
    FLAG_KEY1 = 0;
    FLAG_KEY2 = 0;
    FLAG_KEY3 = 0;
    FLAG_KEY4 = 0;
    FLAG_KEY5 = 0;
#endif
}

static void Project3_HandleTouch(PROJECT3_CONTEXT *ctx)
{
    (void)ctx;
    FLAG_TOUCH = 0;
    FLAG_BUTTON_1 = 0;
    FLAG_BUTTON_2 = 0;
    FLAG_BUTTON_3 = 0;
    FLAG_BUTTON_4 = 0;
    FLAG_BUTTON_5 = 0;
    FLAG_BUTTON_6 = 0;
    FLAG_BUTTON_7 = 0;
    FLAG_BUTTON_8 = 0;
}

static void Project3_ProcessAudioBlock(PROJECT3_CONTEXT *ctx, short *block, unsigned int block_samples)
{
    unsigned int offset;
    unsigned int i;
    unsigned char block_has_speech = 0;
    unsigned char end_pending = 0;
    int peak = 0;

    g_project3_audio_block_count++;
    for (i = 0u; i < block_samples; i++) {
        int sample = (int)block[i];
        if (sample < 0) sample = -sample;
        if (sample > peak) peak = sample;
    }
    if (peak > 32767) peak = 32767;
    g_project3_audio_peak = (short)peak;

#if PROJECT3_ENABLE_REALTIME_MUSIC
    /* A music source appearing or disappearing changes the CH1 background
     * statistics abruptly.  Recalibrate the VAD on the already-cleaned mic
     * stream so the transition itself cannot become a fake command. */
    if (ctx->music_ref_last_active != g_project3_music_ref_active) {
        ctx->music_ref_last_active = g_project3_music_ref_active;
        g_project3_music_ref_transition_count++;
        Project3_ResetVadAdaptiveState(&ctx->vad);
        Project3_ResetUtterance(ctx);
        Project3_ResetPreroll();
        ctx->input_gate_blocks = 0u;
        Project3_SetAppState(ctx, PROJECT3_APP_LISTENING);
        Project3_SetUiText(ctx, P3_LISTENING_TEXT, "", "");
    }
#endif

    Project3_ServiceErrorHold(ctx);
    Project3_ServiceMessageHold(ctx);

    if (ctx->error_hold_blocks > 0) {
        return;
    }

    /* Message hold is a display policy only.  Do not stop VAD processing here,
     * otherwise every displayed result makes the recognizer deaf for a second. */

    if (ctx->input_gate_blocks > 0) {
        ctx->input_gate_blocks--;
        /* Keep look-back audio while suppressing VAD retrigger.  A command
         * that starts near the end of the 205 ms refractory window can then
         * recover its first transition when VAD becomes active again. */
        Project3_UpdatePreroll(block, block_samples);
        return;
    }

    for (offset = 0; offset + PROJECT3_VAD_FRAME_LEN <= block_samples; offset += PROJECT3_VAD_HOP) {
        PROJECT3_VAD_FEATURES vad_features;
        unsigned char started;

        Project3_ComputeVadFeatures(&block[offset], PROJECT3_VAD_FRAME_LEN,
                                    &vad_features);
        started = Project3_UpdateVad(ctx, &vad_features);

        g_project3_last_frame_energy = vad_features.total_energy;
        g_project3_vad_noise_floor = ctx->vad.noise_floor;
        g_project3_vad_smooth_energy = ctx->vad.smooth_energy;

        ctx->frame_counter++;

        if (started) {
            ctx->utter.ready = 0;
            Project3_AppendPreroll(ctx);
        }

        if (ctx->vad.active || started) {
            block_has_speech = 1;
        }

        if (Project3_ShouldEndUtterance(ctx)) {
            end_pending = 1;
            block_has_speech = 1;
            break;
        }
    }

    if (block_has_speech) {
        Project3_AppendRawBlock(ctx, block, block_samples);
        ctx->active_speech_samples += block_samples;
        g_project3_current_utterance_samples = ctx->utter.count;
    } else {
        Project3_UpdatePreroll(block, block_samples);
    }

    if (end_pending || ctx->utter.count >= PROJECT3_RAW_MAX_SAMPLES) {
        g_project3_vad_end_count++;
        if (ctx->utter.count >= PROJECT3_MIN_UTTERANCE_SAMPLES &&
            ctx->active_speech_samples >= PROJECT3_MIN_ACTIVE_SPEECH_SAMPLES) {
            if (!PROJECT3_ENABLE_SPEECHLIKE_VETO ||
                Project3_IsSpeechLikeUtterance(ctx, &ctx->utter)) {
                Project3_HandleSpeechEnd(ctx);
            } else {
                g_project3_nonspeech_reject_count++;
                Project3_ResetUtterance(ctx);
                Project3_ResetPreroll();
                ctx->input_gate_blocks = PROJECT3_NONSPEECH_IGNORE_BLOCKS;
                Project3_SetAppState(ctx, PROJECT3_APP_LISTENING);
                Project3_SetUiText(ctx, P3_LISTENING_TEXT, "", "");
            }
        } else {
            g_project3_last_voiced_frame_count = 0u;
            g_project3_last_periodicity_permille = 0u;
            Project3_ResetUtterance(ctx);
            ctx->input_gate_blocks = 0;
        }
    }
}

static void Project3_ModelInit(PROJECT3_CONTEXT *ctx)
{
    ctx->model_state = PROJECT3_MODEL_READY;
    Project3_ClearStableDisplay(ctx);
}

static PROJECT3_INFER_RESULT Project3_RunInference(PROJECT3_CONTEXT *ctx, const PROJECT3_UTTERANCE_BUFFER *utter)
{
    PROJECT3_INFER_RESULT result;
    float exp_value;
    float speech_logit;
    unsigned long long stage_start;
    unsigned long long stage_end;

    ctx->model_state = PROJECT3_MODEL_READY;
    g_project3_last_speech_logit = 0.0f;
    g_project3_last_speech_probability = 0.0f;
    g_project3_last_speech_gate_pass = 0u;
    g_project3_last_raw_class_id = PROJECT3_CLASS_SILENCE;
    Project3_ResetMemoryGuard();
    Project3_LcdPauseForInference();

    Project3_ExtractLogMel(utter, g_project3_logmel);
    if (!Project3_CheckMemoryGuard()) {
        result = Project3_MemoryFaultResult();
        Project3_LcdResumeAfterInference();
        return result;
    }
    Project3_BCResNetForward(g_project3_logmel, g_project3_logits,
                             &speech_logit);
    g_project3_last_speech_logit = speech_logit;
    if (!Project3_CheckMemoryGuard()) {
        result = Project3_MemoryFaultResult();
        Project3_LcdResumeAfterInference();
        return result;
    }
    stage_start = _itoll(TSCH, TSCL);
    result = Project3_LogitsToResult(g_project3_logits);
    g_project3_last_raw_class_id = result.class_id;
    if (g_project3_last_speech_logit >= 0.0f) {
        g_project3_last_speech_probability =
            1.0f / (1.0f + expf(-g_project3_last_speech_logit));
    } else {
        exp_value = expf(g_project3_last_speech_logit);
        g_project3_last_speech_probability =
            exp_value / (1.0f + exp_value);
    }
    g_project3_last_speech_gate_pass =
        (g_project3_last_speech_probability >=
             PROJECT3_MODEL_SPEECH_LOW_THRESHOLD &&
         (g_project3_last_speech_probability >=
              PROJECT3_MODEL_SPEECH_HIGH_THRESHOLD ||
          result.confidence >=
              PROJECT3_MODEL_SPEECH_MIN_CONFIDENCE)) ? 1u : 0u;
#if PROJECT3_ENABLE_MODEL_SPEECH_GATE
    if (!g_project3_last_speech_gate_pass) {
        result.second_class_id = result.class_id;
        result.second_confidence = result.confidence;
        result.class_id = PROJECT3_CLASS_SILENCE;
        result.confidence = 1.0f - g_project3_last_speech_probability;
        result.margin = result.confidence - result.second_confidence;
        result.valid = 1u;
    }
#endif
    stage_end = _itoll(TSCH, TSCL);
    g_project3_profile_post_cycles = stage_end - stage_start;
    g_project3_profile_post_us =
        Project3_ProfileCyclesToUs(stage_end - stage_start);
    Project3_LcdResumeAfterInference();
    return result;
}

static const char *Project3_GetLabel(unsigned char class_id)
{
    if (class_id >= PROJECT3_CMD_COUNT) {
        return "_unknown_";
    }
    return g_project3_labels[class_id];
}

static void Project3_ResetUtterance(PROJECT3_CONTEXT *ctx)
{
    ctx->utter.count = 0;
    ctx->utter.ready = 0;
    ctx->vad.active = 0;
    ctx->vad.speech_hold_frames = 0;
    ctx->vad.silence_hold_frames = 0;
    ctx->active_speech_samples = 0u;
    g_project3_current_utterance_samples = 0u;
}

static void Project3_StartMessageHold(PROJECT3_CONTEXT *ctx,
                                      unsigned char hold_blocks,
                                      unsigned char return_to_listening)
{
    ctx->message_hold_blocks = hold_blocks;
    ctx->message_hold_start_sequence = g_project3_adc_completed_sequence;
    ctx->message_return_to_listening = return_to_listening;
}

static void Project3_ServiceMessageHold(PROJECT3_CONTEXT *ctx)
{
    unsigned long elapsed_blocks;

    if (ctx->message_hold_blocks == 0u) {
        return;
    }

    /* Use real ADC time, not foreground-consumed blocks.  After inference the
     * foreground may rapidly drain a one-second backlog; decrementing once per
     * drained block would make the result disappear almost immediately. */
    elapsed_blocks = g_project3_adc_completed_sequence -
                     ctx->message_hold_start_sequence;
    if (elapsed_blocks < (unsigned long)ctx->message_hold_blocks) {
        return;
    }

    ctx->message_hold_blocks = 0u;
    if (ctx->message_return_to_listening) {
        ctx->message_return_to_listening = 0u;
        Project3_SetAppState(ctx,
            (ctx->vad.active || ctx->utter.count > 0u) ?
            PROJECT3_APP_SPEECH : PROJECT3_APP_LISTENING);
        Project3_SetUiText(ctx, P3_LISTENING_TEXT, "", "");
    } else {
        Project3_RestoreStableDisplay(ctx);
    }
}

static void Project3_ServiceErrorHold(PROJECT3_CONTEXT *ctx)
{
    if (ctx->error_hold_blocks > 0) {
        ctx->error_hold_blocks--;
        if (ctx->error_hold_blocks == 0) {
            Project3_RestoreStableDisplay(ctx);
        }
    }
}

static void Project3_UpdateUi(PROJECT3_CONTEXT *ctx, unsigned char force_redraw)
{
    if (force_redraw || ctx->redraw_needed) {
        if (!force_redraw && ctx->lcd_retry_frame != 0u && ctx->frame_counter < ctx->lcd_retry_frame) {
            return;
        }
        if (Project3_RenderScreen(ctx, force_redraw)) {
            ctx->redraw_needed = 0;
            ctx->lcd_retry_frame = 0u;
            ctx->lcd_last_redraw_frame = ctx->frame_counter;
        } else {
            ctx->lcd_retry_frame = ctx->frame_counter + 4u;
        }
    }
}

#endif
