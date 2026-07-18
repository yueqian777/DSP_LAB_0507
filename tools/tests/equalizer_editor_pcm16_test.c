#include "user_equalizer.h"
#include "user_equalizer_control.h"

#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HOST_SAMPLE_RATE 50000
#define HOST_FRAME_SAMPLES 127
#define STATIC_SAMPLES 50000
#define MEASURE_START 10000
#define TRANSITION_SAMPLES 6000
#define CENTER_ERROR_LIMIT_DB 0.15
#define COMPLEX_ERROR_LIMIT 0.025
#define PI_D 3.14159265358979323846

typedef struct
{
    const char *name;
    float gains_db[EQ_NUM_BANDS];
} CUSTOM_PATTERN;

typedef enum
{
    SIGNAL_CENTER_SINE = 0,
    SIGNAL_MULTITONE,
    SIGNAL_IMPULSE,
    SIGNAL_WHITE_NOISE,
    SIGNAL_MUSIC_LIKE,
    SIGNAL_SILENCE,
    SIGNAL_NEAR_FULL_MULTITONE
} SIGNAL_KIND;

typedef struct
{
    char name[32];
    SIGNAL_KIND kind;
    int band;
    double target_peak_fs;
    int repeated_frames_expected;
} SIGNAL_SPEC;

typedef struct
{
    EQ_STATE equalizer;
    EQ_CONTROL_STATE control;
    EQ_CONTROL_MAILBOX mailbox;
} HOST_EQ;

typedef struct
{
    double peak_pcm;
    double peak_fs;
    double rms_pcm;
    unsigned long nonfinite_count;
} PCM_STATS;

typedef struct
{
    double max_center_error_db;
    double max_complex_error;
    double max_output_peak_fs;
    unsigned long total_clip_count;
    unsigned long total_nonfinite_count;
    unsigned long identity_mismatch_count;
    unsigned long unexpected_repeated_frames;
    unsigned long transition_repeated_frames;
    unsigned long transition_progress_checks;
    unsigned long transition_sequence_early_updates;
    int static_case_count;
    int response_point_count;
    int transition_case_count;
    int transition_min_samples;
    int transition_max_samples;
} RUN_SUMMARY;

static const CUSTOM_PATTERN k_patterns[] =
{
    { "A_125_plus3",
      { 0.0f, 0.0f, 3.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 0.0f, 0.0f } },
    { "B_1k_minus3",
      { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
       -3.0f, 0.0f, 0.0f, 0.0f, 0.0f } },
    { "C_8k_plus3",
      { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 3.0f, 0.0f } },
    { "D_v_like",
      { 6.0f, 4.0f, 2.0f, 0.0f, -2.0f,
       -2.0f, 0.0f, 2.0f, 4.0f, 6.0f } },
    { "E_all_zero",
      { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 0.0f, 0.0f } },
    { "F_limits",
      { -18.0f, 12.0f, -18.0f, 12.0f, -18.0f,
         12.0f, -18.0f, 12.0f, -18.0f, 12.0f } }
};

static const char *k_center_labels[EQ_NUM_BANDS] =
{
    "31", "63", "125", "250", "500",
    "1k", "2k", "4k", "8k", "16k"
};

static int failures = 0;

static void failf(const char *format, ...)
{
    va_list args;

    fprintf(stderr, "FAIL: ");
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fprintf(stderr, "\n");
    failures++;
}

static int finite_number(double value)
{
    return isfinite(value) ? 1 : 0;
}

static void join_path(char *out, size_t out_size,
                      const char *directory, const char *name)
{
    size_t length = strlen(directory);
    const char *separator =
        ((length > 0U) &&
         ((directory[length - 1U] == '/') ||
          (directory[length - 1U] == '\\'))) ? "" : "/";

    if (snprintf(out, out_size, "%s%s%s", directory,
                 separator, name) >= (int)out_size)
    {
        failf("output path is too long: %s", name);
        out[0] = '\0';
    }
}

static void write_u16_le(FILE *file, unsigned int value)
{
    unsigned char bytes[2];

    bytes[0] = (unsigned char)(value & 0xffU);
    bytes[1] = (unsigned char)((value >> 8) & 0xffU);
    (void)fwrite(bytes, 1U, sizeof(bytes), file);
}

static void write_u32_le(FILE *file, uint32_t value)
{
    unsigned char bytes[4];

    bytes[0] = (unsigned char)(value & 0xffU);
    bytes[1] = (unsigned char)((value >> 8) & 0xffU);
    bytes[2] = (unsigned char)((value >> 16) & 0xffU);
    bytes[3] = (unsigned char)((value >> 24) & 0xffU);
    (void)fwrite(bytes, 1U, sizeof(bytes), file);
}

static int write_wav(const char *path, const short *samples, int count)
{
    FILE *file;
    uint32_t data_bytes;
    int index;

    file = fopen(path, "wb");
    if (file == 0)
    {
        failf("cannot create WAV: %s", path);
        return 0;
    }
    data_bytes = (uint32_t)count * 2U;
    (void)fwrite("RIFF", 1U, 4U, file);
    write_u32_le(file, 36U + data_bytes);
    (void)fwrite("WAVE", 1U, 4U, file);
    (void)fwrite("fmt ", 1U, 4U, file);
    write_u32_le(file, 16U);
    write_u16_le(file, 1U);
    write_u16_le(file, 1U);
    write_u32_le(file, HOST_SAMPLE_RATE);
    write_u32_le(file, HOST_SAMPLE_RATE * 2U);
    write_u16_le(file, 2U);
    write_u16_le(file, 16U);
    (void)fwrite("data", 1U, 4U, file);
    write_u32_le(file, data_bytes);
    for (index = 0; index < count; index++)
    {
        write_u16_le(file, (unsigned int)(unsigned short)samples[index]);
    }
    if (fclose(file) != 0)
    {
        failf("cannot finish WAV: %s", path);
        return 0;
    }
    return 1;
}

static short round_pcm16(double value)
{
    double rounded;

    if (value > 32767.0)
    {
        value = 32767.0;
    }
    if (value < -32768.0)
    {
        value = -32768.0;
    }
    rounded = (value >= 0.0) ? floor(value + 0.5) : ceil(value - 0.5);
    return (short)rounded;
}

static double signal_raw_value(const SIGNAL_SPEC *spec, int sample,
                               uint32_t *noise_state)
{
    double time = (double)sample / (double)HOST_SAMPLE_RATE;
    double value = 0.0;
    int band;

    switch (spec->kind)
    {
        case SIGNAL_CENTER_SINE:
            value = sin(2.0 * PI_D *
                        (double)Equalizer_GetBandCenterHz(spec->band) *
                        time + 0.31);
            break;
        case SIGNAL_MULTITONE:
            for (band = 0; band < EQ_NUM_BANDS; band++)
            {
                value += sin(2.0 * PI_D *
                    (double)Equalizer_GetBandCenterHz(band) * time +
                    0.19 * (double)(band + 1));
            }
            value /= (double)EQ_NUM_BANDS;
            break;
        case SIGNAL_IMPULSE:
            value = (sample == 0) ? 1.0 : 0.0;
            break;
        case SIGNAL_WHITE_NOISE:
            *noise_state = *noise_state * 1664525U + 1013904223U;
            value = ((double)((*noise_state >> 8) & 0x00ffffffU) /
                     8388607.5) - 1.0;
            break;
        case SIGNAL_MUSIC_LIKE:
        {
            double envelope = 0.68 +
                0.22 * sin(2.0 * PI_D * 1.7 * time) +
                0.10 * sin(2.0 * PI_D * 0.43 * time + 0.8);
            value = envelope *
                (0.38 * sin(2.0 * PI_D * 110.0 * time + 0.2) +
                 0.25 * sin(2.0 * PI_D * 220.0 * time + 0.7) +
                 0.18 * sin(2.0 * PI_D * 440.0 * time + 1.1) +
                 0.12 * sin(2.0 * PI_D * 1760.0 * time + 0.4) +
                 0.07 * sin(2.0 * PI_D * 7040.0 * time + 1.7));
            break;
        }
        case SIGNAL_SILENCE:
            value = 0.0;
            break;
        case SIGNAL_NEAR_FULL_MULTITONE:
            value =
                0.29 * sin(2.0 * PI_D * 62.5 * time + 0.13) +
                0.24 * sin(2.0 * PI_D * 250.0 * time + 0.71) +
                0.19 * sin(2.0 * PI_D * 1000.0 * time + 1.19) +
                0.16 * sin(2.0 * PI_D * 4000.0 * time + 0.47) +
                0.12 * sin(2.0 * PI_D * 16000.0 * time + 1.83);
            break;
        default:
            break;
    }
    return value;
}

static void generate_signal(const SIGNAL_SPEC *spec, short *output,
                            double *scratch, int count)
{
    uint32_t noise_state = 0x13579bdfU;
    double maximum = 0.0;
    double scale;
    int index;

    for (index = 0; index < count; index++)
    {
        double value = signal_raw_value(spec, index, &noise_state);
        double magnitude = fabs(value);

        scratch[index] = value;
        if (magnitude > maximum)
        {
            maximum = magnitude;
        }
    }
    if ((maximum == 0.0) || (spec->target_peak_fs == 0.0))
    {
        memset(output, 0, (size_t)count * sizeof(*output));
        return;
    }
    scale = spec->target_peak_fs * 32767.0 / maximum;
    for (index = 0; index < count; index++)
    {
        output[index] = round_pcm16(scratch[index] * scale);
    }
}

static void get_pcm_stats(const short *samples, int count, int start,
                          PCM_STATS *stats)
{
    double sum_squares = 0.0;
    double peak = 0.0;
    int index;

    memset(stats, 0, sizeof(*stats));
    if ((start < 0) || (start >= count))
    {
        start = 0;
    }
    for (index = start; index < count; index++)
    {
        double value = (double)samples[index];
        double magnitude = fabs(value);

        if (!finite_number(value))
        {
            stats->nonfinite_count++;
            continue;
        }
        if (magnitude > peak)
        {
            peak = magnitude;
        }
        sum_squares += value * value;
    }
    stats->peak_pcm = peak;
    stats->peak_fs = peak / 32768.0;
    stats->rms_pcm = sqrt(sum_squares / (double)(count - start));
}

static unsigned long count_repeated_frames(const short *samples, int count)
{
    unsigned long repeated = 0UL;
    int offset;

    for (offset = HOST_FRAME_SAMPLES;
         offset + HOST_FRAME_SAMPLES <= count;
         offset += HOST_FRAME_SAMPLES)
    {
        if (memcmp(samples + offset - HOST_FRAME_SAMPLES,
                   samples + offset,
                   (size_t)HOST_FRAME_SAMPLES * sizeof(*samples)) == 0)
        {
            repeated++;
        }
    }
    return repeated;
}

static void fill_request(EQ_CONTROL_REQUEST *request, int command)
{
    memset(request, 0, sizeof(*request));
    request->command = command;
    request->preset = EQ_PRESET_NONE;
}

static void host_eq_init(HOST_EQ *host)
{
    memset(host, 0, sizeof(*host));
    Equalizer_Init(&host->equalizer);
    EqualizerControl_Init(&host->control, &host->equalizer);
}

static void service_safe_boundary(HOST_EQ *host)
{
    EqualizerControl_ObserveFrameBoundary(&host->control,
                                          &host->equalizer);
    (void)EqualizerControl_ServiceMailbox(&host->control,
                                          &host->mailbox,
                                          &host->equalizer);
    EqualizerControl_InvalidateStaleWork(&host->control);
    (void)EqualizerControl_TryInstallReady(&host->control,
                                           &host->equalizer);
    if (EqualizerControl_BuilderEligible(&host->control,
                                         &host->equalizer) != 0)
    {
        (void)EqualizerControl_ServiceOneBuilderSlice(&host->control);
    }
}

static EQ_CONTROL_SEQUENCE submit_custom(HOST_EQ *host,
                                         const float *gains_db)
{
    EQ_CONTROL_REQUEST request;
    int band;

    fill_request(&request, EQ_CONTROL_SET_ALL);
    request.preset = EQ_PRESET_CUSTOM;
    for (band = 0; band < EQ_NUM_BANDS; band++)
    {
        request.shadow_gain_db[band] = gains_db[band];
    }
    return EqualizerControl_SubmitRequest(&host->mailbox, &request);
}

static EQ_CONTROL_SEQUENCE submit_preset(HOST_EQ *host, int preset)
{
    EQ_CONTROL_REQUEST request;

    fill_request(&request, EQ_CONTROL_APPLY_PRESET);
    request.preset = preset;
    return EqualizerControl_SubmitRequest(&host->mailbox, &request);
}

static EQ_CONTROL_SEQUENCE submit_reset_flat(HOST_EQ *host)
{
    EQ_CONTROL_REQUEST request;

    fill_request(&request, EQ_CONTROL_RESET_FLAT);
    request.preset = EQ_PRESET_FLAT;
    return EqualizerControl_SubmitRequest(&host->mailbox, &request);
}

static int settle_token_with_silence(HOST_EQ *host,
                                     EQ_CONTROL_SEQUENCE token)
{
    short input[HOST_FRAME_SAMPLES];
    short output[HOST_FRAME_SAMPLES];
    int guard;

    memset(input, 0, sizeof(input));
    for (guard = 0; guard < 512; guard++)
    {
        int count = HOST_FRAME_SAMPLES;

        service_safe_boundary(host);
        if (Equalizer_HasPendingTransition(&host->equalizer) != 0)
        {
            int remaining = Equalizer_GetTransitionRemaining(
                &host->equalizer);
            if (count > remaining)
            {
                count = remaining;
            }
            Equalizer_ProcessFrame(&host->equalizer, input, output, count);
        }
        if ((host->control.applied_sequence == token) &&
            (Equalizer_HasPendingTransition(&host->equalizer) == 0))
        {
            return 1;
        }
    }
    failf("request token %u did not settle", (unsigned int)token);
    return 0;
}

static int activate_custom(HOST_EQ *host, const CUSTOM_PATTERN *pattern)
{
    EQ_CONTROL_SEQUENCE token;
    int band;

    host_eq_init(host);
    token = submit_custom(host, pattern->gains_db);
    if ((token == 0U) || !settle_token_with_silence(host, token))
    {
        return 0;
    }
    if (Equalizer_GetAppliedPreset(&host->equalizer) != EQ_PRESET_CUSTOM)
    {
        failf("pattern %s did not apply as CUSTOM", pattern->name);
        return 0;
    }
    for (band = 0; band < EQ_NUM_BANDS; band++)
    {
        if (fabs((double)host->equalizer.active_gain_db[band] -
                 (double)pattern->gains_db[band]) > 1.0e-6)
        {
            failf("pattern %s gain mismatch at band %d",
                  pattern->name, band);
            return 0;
        }
    }
    return 1;
}

static void process_pcm(HOST_EQ *host, const short *input,
                        short *output, int count)
{
    int offset = 0;

    while (offset < count)
    {
        int frame = count - offset;
        if (frame > HOST_FRAME_SAMPLES)
        {
            frame = HOST_FRAME_SAMPLES;
        }
        Equalizer_ProcessFrame(&host->equalizer,
                               input + offset, output + offset, frame);
        offset += frame;
    }
}

static void measure_complex_response(const short *input,
                                     const short *output,
                                     int count, int start,
                                     double frequency_hz,
                                     double *real_out,
                                     double *imag_out)
{
    double xr = 0.0;
    double xi = 0.0;
    double yr = 0.0;
    double yi = 0.0;
    double denominator;
    int index;

    for (index = start; index < count; index++)
    {
        double angle = 2.0 * PI_D * frequency_hz *
                       (double)index / (double)HOST_SAMPLE_RATE;
        double cosine = cos(angle);
        double minus_sine = -sin(angle);

        xr += (double)input[index] * cosine;
        xi += (double)input[index] * minus_sine;
        yr += (double)output[index] * cosine;
        yi += (double)output[index] * minus_sine;
    }
    denominator = xr * xr + xi * xi;
    if (denominator <= 1.0e-20)
    {
        *real_out = 0.0;
        *imag_out = 0.0;
        failf("complex response denominator is zero at %.3f Hz",
              frequency_hz);
        return;
    }
    *real_out = (yr * xr + yi * xi) / denominator;
    *imag_out = (yi * xr - yr * xi) / denominator;
}

static int build_signal_specs(SIGNAL_SPEC *specs)
{
    int count = 0;
    int band;

    for (band = 0; band < EQ_NUM_BANDS; band++)
    {
        (void)snprintf(specs[count].name, sizeof(specs[count].name),
                       "sine_%s", k_center_labels[band]);
        specs[count].kind = SIGNAL_CENTER_SINE;
        specs[count].band = band;
        specs[count].target_peak_fs = 0.20;
        specs[count].repeated_frames_expected = 0;
        count++;
    }
    (void)snprintf(specs[count].name, sizeof(specs[count].name),
                   "multitone");
    specs[count].kind = SIGNAL_MULTITONE;
    specs[count].band = -1;
    specs[count].target_peak_fs = 0.45;
    specs[count].repeated_frames_expected = 0;
    count++;
    (void)snprintf(specs[count].name, sizeof(specs[count].name),
                   "impulse_0p9fs");
    specs[count].kind = SIGNAL_IMPULSE;
    specs[count].band = -1;
    specs[count].target_peak_fs = 0.90;
    specs[count].repeated_frames_expected = 1;
    count++;
    (void)snprintf(specs[count].name, sizeof(specs[count].name),
                   "white_noise");
    specs[count].kind = SIGNAL_WHITE_NOISE;
    specs[count].band = -1;
    specs[count].target_peak_fs = 0.35;
    specs[count].repeated_frames_expected = 0;
    count++;
    (void)snprintf(specs[count].name, sizeof(specs[count].name),
                   "music_like");
    specs[count].kind = SIGNAL_MUSIC_LIKE;
    specs[count].band = -1;
    specs[count].target_peak_fs = 0.55;
    specs[count].repeated_frames_expected = 0;
    count++;
    (void)snprintf(specs[count].name, sizeof(specs[count].name),
                   "silence");
    specs[count].kind = SIGNAL_SILENCE;
    specs[count].band = -1;
    specs[count].target_peak_fs = 0.0;
    specs[count].repeated_frames_expected = 1;
    count++;
    (void)snprintf(specs[count].name, sizeof(specs[count].name),
                   "near_full_multitone");
    specs[count].kind = SIGNAL_NEAR_FULL_MULTITONE;
    specs[count].band = -1;
    specs[count].target_peak_fs = 0.891250938;
    specs[count].repeated_frames_expected = 0;
    count++;
    return count;
}

static void run_static_cases(const char *output_directory,
                             FILE *case_csv, FILE *response_csv,
                             RUN_SUMMARY *summary)
{
    SIGNAL_SPEC specs[EQ_NUM_BANDS + 6];
    short *input = (short *)malloc(STATIC_SAMPLES * sizeof(*input));
    short *output = (short *)malloc(STATIC_SAMPLES * sizeof(*output));
    double *scratch = (double *)malloc(STATIC_SAMPLES * sizeof(*scratch));
    int signal_count = build_signal_specs(specs);
    int pattern_index;
    int signal_index;

    if ((input == 0) || (output == 0) || (scratch == 0))
    {
        failf("cannot allocate static PCM buffers");
        free(input);
        free(output);
        free(scratch);
        return;
    }
    for (signal_index = 0; signal_index < signal_count; signal_index++)
    {
        char filename[96];
        char path[1024];

        generate_signal(&specs[signal_index], input, scratch, STATIC_SAMPLES);
        (void)snprintf(filename, sizeof(filename), "input_%s.wav",
                       specs[signal_index].name);
        join_path(path, sizeof(path), output_directory, filename);
        if (path[0] != '\0')
        {
            (void)write_wav(path, input, STATIC_SAMPLES);
        }
    }
    for (pattern_index = 0;
         pattern_index < (int)(sizeof(k_patterns) / sizeof(k_patterns[0]));
         pattern_index++)
    {
        for (signal_index = 0; signal_index < signal_count; signal_index++)
        {
            HOST_EQ host;
            PCM_STATS input_stats;
            PCM_STATS output_stats;
            unsigned long repeated;
            unsigned long clip_count;
            unsigned long identity_mismatch = 0UL;
            char filename[128];
            char path[1024];
            int identity_checked = (pattern_index == 4) ? 1 : 0;
            int sample;

            generate_signal(&specs[signal_index], input, scratch,
                            STATIC_SAMPLES);
            memset(output, 0, STATIC_SAMPLES * sizeof(*output));
            if (!activate_custom(&host, &k_patterns[pattern_index]))
            {
                continue;
            }
            process_pcm(&host, input, output, STATIC_SAMPLES);
            get_pcm_stats(input, STATIC_SAMPLES, 0, &input_stats);
            get_pcm_stats(output, STATIC_SAMPLES, 0, &output_stats);
            repeated = count_repeated_frames(output, STATIC_SAMPLES);
            clip_count = host.equalizer.clip_count;
            if (identity_checked)
            {
                for (sample = 0; sample < STATIC_SAMPLES; sample++)
                {
                    if (input[sample] != output[sample])
                    {
                        identity_mismatch++;
                    }
                }
            }
            if (clip_count != 0UL)
            {
                failf("clip in %s/%s: %lu", k_patterns[pattern_index].name,
                      specs[signal_index].name, clip_count);
            }
            if (output_stats.nonfinite_count != 0UL)
            {
                failf("nonfinite output in %s/%s",
                      k_patterns[pattern_index].name,
                      specs[signal_index].name);
            }
            if (identity_mismatch != 0UL)
            {
                failf("identity mismatch in %s: %lu",
                      specs[signal_index].name, identity_mismatch);
            }
            if ((repeated != 0UL) &&
                (specs[signal_index].repeated_frames_expected == 0))
            {
                failf("unexpected repeated frame in %s/%s: %lu",
                      k_patterns[pattern_index].name,
                      specs[signal_index].name, repeated);
                summary->unexpected_repeated_frames += repeated;
            }
            if (output_stats.peak_fs > summary->max_output_peak_fs)
            {
                summary->max_output_peak_fs = output_stats.peak_fs;
            }
            summary->total_clip_count += clip_count;
            summary->total_nonfinite_count += output_stats.nonfinite_count;
            summary->identity_mismatch_count += identity_mismatch;
            summary->static_case_count++;
            fprintf(case_csv,
                    "%s,%s,%d,%.9f,%.9f,%.9f,%.9f,%lu,%lu,%d,%lu,%lu,%d,%.9f,%.9f\n",
                    k_patterns[pattern_index].name,
                    specs[signal_index].name,
                    STATIC_SAMPLES,
                    input_stats.peak_fs, input_stats.rms_pcm,
                    output_stats.peak_fs, output_stats.rms_pcm,
                    clip_count, output_stats.nonfinite_count,
                    identity_checked, identity_mismatch, repeated,
                    specs[signal_index].repeated_frames_expected,
                    (double)Equalizer_GetPreampDb(&host.equalizer),
                    (double)Equalizer_GetPredictedPeakDb(&host.equalizer));

            (void)snprintf(filename, sizeof(filename), "output_%s_%s.wav",
                           k_patterns[pattern_index].name,
                           specs[signal_index].name);
            join_path(path, sizeof(path), output_directory, filename);
            if (path[0] != '\0')
            {
                (void)write_wav(path, output, STATIC_SAMPLES);
            }

            if (specs[signal_index].kind == SIGNAL_CENTER_SINE)
            {
                PCM_STATS measured_input;
                PCM_STATS measured_output;
                float theory_real_f;
                float theory_imag_f;
                double measured_real;
                double measured_imag;
                double theory_real;
                double theory_imag;
                double theory_magnitude_db;
                double measured_magnitude_db;
                double magnitude_error_db;
                double complex_error;
                double frequency = (double)Equalizer_GetBandCenterHz(
                    specs[signal_index].band);

                get_pcm_stats(input, STATIC_SAMPLES, MEASURE_START,
                              &measured_input);
                get_pcm_stats(output, STATIC_SAMPLES, MEASURE_START,
                              &measured_output);
                Equalizer_GetSystemResponse(&host.equalizer,
                    (float)frequency, &theory_real_f, &theory_imag_f);
                theory_real = (double)theory_real_f;
                theory_imag = (double)theory_imag_f;
                measure_complex_response(input, output, STATIC_SAMPLES,
                    MEASURE_START, frequency,
                    &measured_real, &measured_imag);
                theory_magnitude_db = 20.0 * log10(sqrt(
                    theory_real * theory_real +
                    theory_imag * theory_imag));
                measured_magnitude_db = 20.0 * log10(
                    measured_output.rms_pcm / measured_input.rms_pcm);
                magnitude_error_db =
                    measured_magnitude_db - theory_magnitude_db;
                complex_error = sqrt(
                    (measured_real - theory_real) *
                    (measured_real - theory_real) +
                    (measured_imag - theory_imag) *
                    (measured_imag - theory_imag));
                if ((!finite_number(theory_real)) ||
                    (!finite_number(theory_imag)) ||
                    (!finite_number(measured_real)) ||
                    (!finite_number(measured_imag)) ||
                    (!finite_number(magnitude_error_db)) ||
                    (!finite_number(complex_error)))
                {
                    summary->total_nonfinite_count++;
                    failf("nonfinite response in %s/%s",
                          k_patterns[pattern_index].name,
                          specs[signal_index].name);
                }
                if (fabs(magnitude_error_db) >
                    summary->max_center_error_db)
                {
                    summary->max_center_error_db =
                        fabs(magnitude_error_db);
                }
                if (complex_error > summary->max_complex_error)
                {
                    summary->max_complex_error = complex_error;
                }
                if (fabs(magnitude_error_db) > CENTER_ERROR_LIMIT_DB)
                {
                    failf("center error %.6f dB exceeds %.3f dB in %s/%s",
                          magnitude_error_db, CENTER_ERROR_LIMIT_DB,
                          k_patterns[pattern_index].name,
                          specs[signal_index].name);
                }
                if (complex_error > COMPLEX_ERROR_LIMIT)
                {
                    failf("complex error %.8f exceeds %.3f in %s/%s",
                          complex_error, COMPLEX_ERROR_LIMIT,
                          k_patterns[pattern_index].name,
                          specs[signal_index].name);
                }
                fprintf(response_csv,
                        "%s,%d,%.6f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%lu,%lu\n",
                        k_patterns[pattern_index].name,
                        specs[signal_index].band,
                        frequency, theory_real, theory_imag,
                        measured_real, measured_imag,
                        theory_magnitude_db, measured_magnitude_db,
                        magnitude_error_db, complex_error,
                        measured_output.rms_pcm,
                        clip_count, output_stats.nonfinite_count);
                summary->response_point_count++;
            }
        }
    }
    free(input);
    free(output);
    free(scratch);
}

static void generate_transition_signal(short *output, double *scratch,
                                       int count, int case_index)
{
    double maximum = 0.0;
    double scale;
    int sample;

    for (sample = 0; sample < count; sample++)
    {
        double time = (double)(sample + case_index * 7013) /
                      (double)HOST_SAMPLE_RATE;
        double envelope = 0.72 + 0.18 *
            sin(2.0 * PI_D * 1.3 * time + 0.2 * case_index);
        double value = envelope *
            (0.40 * sin(2.0 * PI_D * 125.0 * time + 0.1) +
             0.30 * sin(2.0 * PI_D * 1000.0 * time + 0.7) +
             0.20 * sin(2.0 * PI_D * 4000.0 * time + 1.2) +
             0.10 * sin(2.0 * PI_D * 8000.0 * time + 0.4));
        double magnitude = fabs(value);

        scratch[sample] = value;
        if (magnitude > maximum)
        {
            maximum = magnitude;
        }
    }
    scale = 0.55 * 32767.0 / maximum;
    for (sample = 0; sample < count; sample++)
    {
        output[sample] = round_pcm16(scratch[sample] * scale);
    }
}

static int wait_for_transition_start(HOST_EQ *host,
                                     EQ_CONTROL_SEQUENCE token,
                                     EQ_CONTROL_SEQUENCE previous_applied)
{
    int guard;

    for (guard = 0; guard < 128; guard++)
    {
        service_safe_boundary(host);
        if (host->control.applied_sequence != previous_applied)
        {
            failf("token %u updated before transition started",
                  (unsigned int)token);
            return 0;
        }
        if (Equalizer_HasPendingTransition(&host->equalizer) != 0)
        {
            return 1;
        }
    }
    failf("token %u did not start a transition", (unsigned int)token);
    return 0;
}

static void run_one_transition(const char *output_directory,
                               FILE *transition_csv,
                               RUN_SUMMARY *summary,
                               HOST_EQ *host,
                               const char *name,
                               const char *from_name,
                               const char *to_name,
                               EQ_CONTROL_SEQUENCE token,
                               int case_index,
                               short *input,
                               short *output,
                               double *scratch)
{
    EQ_CONTROL_SEQUENCE previous_applied = host->control.applied_sequence;
    unsigned long clip_before = host->equalizer.clip_count;
    unsigned long repeated;
    PCM_STATS input_stats;
    PCM_STATS output_stats;
    int transition_total;
    int processed = 0;
    int progress_checks = 0;
    int progress_monotonic = 1;
    int sequence_held = 1;
    int applied_after_boundary = 0;
    int previous_progress = -1;
    char input_filename[128];
    char output_filename[128];
    char path[1024];

    generate_transition_signal(input, scratch, TRANSITION_SAMPLES,
                               case_index);
    memset(output, 0, TRANSITION_SAMPLES * sizeof(*output));
    if (!wait_for_transition_start(host, token, previous_applied))
    {
        return;
    }
    transition_total = host->equalizer.transition_total;
    if (transition_total != TRANSITION_SAMPLES)
    {
        failf("%s transition length %d, expected %d",
              name, transition_total, TRANSITION_SAMPLES);
    }
    if (host->equalizer.transition_remaining != transition_total)
    {
        failf("%s transition did not start at zero progress", name);
    }
    while (Equalizer_HasPendingTransition(&host->equalizer) != 0)
    {
        int remaining_before = host->equalizer.transition_remaining;
        int frame = HOST_FRAME_SAMPLES;
        int progress;

        if (frame > remaining_before)
        {
            frame = remaining_before;
        }
        if (processed + frame > TRANSITION_SAMPLES)
        {
            failf("%s transition exceeded output buffer", name);
            break;
        }
        Equalizer_ProcessFrame(&host->equalizer,
                               input + processed,
                               output + processed, frame);
        processed += frame;
        progress = transition_total -
                   host->equalizer.transition_remaining;
        if ((progress <= previous_progress) ||
            (progress != processed) ||
            (host->equalizer.transition_remaining !=
             remaining_before - frame))
        {
            progress_monotonic = 0;
        }
        previous_progress = progress;
        progress_checks++;
        if (host->control.applied_sequence != previous_applied)
        {
            sequence_held = 0;
        }
        if (Equalizer_HasPendingTransition(&host->equalizer) != 0)
        {
            service_safe_boundary(host);
            if (host->control.applied_sequence != previous_applied)
            {
                sequence_held = 0;
            }
        }
    }
    if (processed != transition_total)
    {
        failf("%s processed %d transition samples, expected %d",
              name, processed, transition_total);
    }
    if (host->control.applied_sequence != previous_applied)
    {
        sequence_held = 0;
    }
    service_safe_boundary(host);
    applied_after_boundary =
        (host->control.applied_sequence == token) ? 1 : 0;
    if (!progress_monotonic)
    {
        failf("%s transition progress was not monotonic", name);
    }
    if (!sequence_held)
    {
        failf("%s applied sequence changed before completion boundary", name);
        summary->transition_sequence_early_updates++;
    }
    if (!applied_after_boundary)
    {
        failf("%s token was not applied at the next safe boundary", name);
    }
    repeated = count_repeated_frames(output, processed);
    if (repeated != 0UL)
    {
        failf("%s repeated %lu adjacent transition frames", name, repeated);
    }
    get_pcm_stats(input, processed, 0, &input_stats);
    get_pcm_stats(output, processed, 0, &output_stats);
    if ((host->equalizer.clip_count - clip_before) != 0UL)
    {
        failf("%s clipped %lu samples", name,
              host->equalizer.clip_count - clip_before);
    }
    if (output_stats.nonfinite_count != 0UL)
    {
        failf("%s produced nonfinite samples", name);
    }
    (void)snprintf(input_filename, sizeof(input_filename),
                   "transition_%s_input.wav", name);
    (void)snprintf(output_filename, sizeof(output_filename),
                   "transition_%s_output.wav", name);
    join_path(path, sizeof(path), output_directory, input_filename);
    if (path[0] != '\0')
    {
        (void)write_wav(path, input, processed);
    }
    join_path(path, sizeof(path), output_directory, output_filename);
    if (path[0] != '\0')
    {
        (void)write_wav(path, output, processed);
    }
    fprintf(transition_csv,
            "%s,%s,%s,%u,%u,%d,%d,%.6f,%d,%d,%d,%d,%lu,%lu,%.9f,%.9f,%lu,%s,%s\n",
            name, from_name, to_name,
            (unsigned int)token, (unsigned int)previous_applied,
            transition_total, processed,
            1000.0 * (double)processed / (double)HOST_SAMPLE_RATE,
            progress_checks, progress_monotonic, sequence_held,
            applied_after_boundary,
            host->equalizer.clip_count - clip_before,
            output_stats.nonfinite_count,
            input_stats.peak_fs, output_stats.peak_fs, repeated,
            input_filename, output_filename);
    summary->transition_case_count++;
    summary->transition_progress_checks += (unsigned long)progress_checks;
    summary->transition_repeated_frames += repeated;
    summary->total_clip_count += host->equalizer.clip_count - clip_before;
    summary->total_nonfinite_count += output_stats.nonfinite_count;
    if ((summary->transition_min_samples == 0) ||
        (processed < summary->transition_min_samples))
    {
        summary->transition_min_samples = processed;
    }
    if (processed > summary->transition_max_samples)
    {
        summary->transition_max_samples = processed;
    }
    if (output_stats.peak_fs > summary->max_output_peak_fs)
    {
        summary->max_output_peak_fs = output_stats.peak_fs;
    }
}

static void run_transition_cases(const char *output_directory,
                                 FILE *transition_csv,
                                 RUN_SUMMARY *summary)
{
    HOST_EQ host;
    short *input = (short *)malloc(TRANSITION_SAMPLES * sizeof(*input));
    short *output = (short *)malloc(TRANSITION_SAMPLES * sizeof(*output));
    double *scratch = (double *)malloc(TRANSITION_SAMPLES * sizeof(*scratch));
    EQ_CONTROL_SEQUENCE token;

    if ((input == 0) || (output == 0) || (scratch == 0))
    {
        failf("cannot allocate transition PCM buffers");
        free(input);
        free(output);
        free(scratch);
        return;
    }
    host_eq_init(&host);

    token = submit_custom(&host, k_patterns[0].gains_db);
    run_one_transition(output_directory, transition_csv, summary,
                       &host, "flat_to_custom_a", "FLAT", "CUSTOM_A",
                       token, 0, input, output, scratch);

    token = submit_custom(&host, k_patterns[1].gains_db);
    run_one_transition(output_directory, transition_csv, summary,
                       &host, "custom_a_to_b", "CUSTOM_A", "CUSTOM_B",
                       token, 1, input, output, scratch);

    token = submit_preset(&host, EQ_PRESET_VOCAL);
    run_one_transition(output_directory, transition_csv, summary,
                       &host, "custom_to_vocal", "CUSTOM_B", "VOCAL",
                       token, 2, input, output, scratch);

    token = submit_reset_flat(&host);
    run_one_transition(output_directory, transition_csv, summary,
                       &host, "vocal_to_reset_flat", "VOCAL", "FLAT",
                       token, 3, input, output, scratch);

    if (Equalizer_GetAppliedPreset(&host.equalizer) != EQ_PRESET_FLAT)
    {
        failf("transition chain did not end in FLAT");
    }
    free(input);
    free(output);
    free(scratch);
}

static int write_summary_json(const char *output_directory,
                              const RUN_SUMMARY *summary)
{
    char path[1024];
    FILE *file;

    join_path(path, sizeof(path), output_directory,
              "equalizer_editor_pcm16_summary.json");
    file = fopen(path, "wb");
    if (file == 0)
    {
        failf("cannot create summary JSON");
        return 0;
    }
    fprintf(file,
        "{\n"
        "  \"schema\": \"project33_equalizer_editor_pcm16_v1\",\n"
        "  \"sample_rate_hz\": %d,\n"
        "  \"channels\": 1,\n"
        "  \"sample_format\": \"PCM_S16LE\",\n"
        "  \"deterministic\": true,\n"
        "  \"pattern_count\": %d,\n"
        "  \"signal_count\": %d,\n"
        "  \"static_case_count\": %d,\n"
        "  \"response_point_count\": %d,\n"
        "  \"transition_case_count\": %d,\n"
        "  \"center_error_limit_db\": %.6f,\n"
        "  \"complex_error_limit\": %.6f,\n"
        "  \"max_center_error_db\": %.9f,\n"
        "  \"max_complex_error\": %.9f,\n"
        "  \"max_output_peak_fs\": %.9f,\n"
        "  \"total_clip_count\": %lu,\n"
        "  \"total_nonfinite_count\": %lu,\n"
        "  \"identity_mismatch_count\": %lu,\n"
        "  \"unexpected_repeated_frames\": %lu,\n"
        "  \"transition_repeated_frames\": %lu,\n"
        "  \"transition_progress_checks\": %lu,\n"
        "  \"transition_sequence_early_updates\": %lu,\n"
        "  \"transition_expected_samples\": %d,\n"
        "  \"transition_min_samples\": %d,\n"
        "  \"transition_max_samples\": %d,\n"
        "  \"failures\": %d,\n"
        "  \"methodology\": {\n"
        "    \"center_measurement\": \"40000 steady-state PCM16 samples after a 10000-sample settling interval; the window contains integer cycles at every EQ center\",\n"
        "    \"complex_response\": \"single-bin DFT ratio Y/X compared with Equalizer_GetSystemResponse real and imaginary values\",\n"
        "    \"center_error_threshold\": \"0.15 dB is a tight 1.7 percent amplitude bound while allowing PCM16 rounding and remaining below the 0.5 dB editor step\",\n"
        "    \"transition_measurement\": \"sample-counted pending-bank crossfade with safe-boundary sequence observation\"\n"
        "  }\n"
        "}\n",
        HOST_SAMPLE_RATE,
        (int)(sizeof(k_patterns) / sizeof(k_patterns[0])),
        EQ_NUM_BANDS + 6,
        summary->static_case_count,
        summary->response_point_count,
        summary->transition_case_count,
        CENTER_ERROR_LIMIT_DB,
        COMPLEX_ERROR_LIMIT,
        summary->max_center_error_db,
        summary->max_complex_error,
        summary->max_output_peak_fs,
        summary->total_clip_count,
        summary->total_nonfinite_count,
        summary->identity_mismatch_count,
        summary->unexpected_repeated_frames,
        summary->transition_repeated_frames,
        summary->transition_progress_checks,
        summary->transition_sequence_early_updates,
        TRANSITION_SAMPLES,
        summary->transition_min_samples,
        summary->transition_max_samples,
        failures);
    if (fclose(file) != 0)
    {
        failf("cannot finish summary JSON");
        return 0;
    }
    return 1;
}

int main(int argc, char **argv)
{
    char path[1024];
    FILE *case_csv;
    FILE *response_csv;
    FILE *transition_csv;
    RUN_SUMMARY summary;

    if (argc != 2)
    {
        fprintf(stderr, "usage: %s OUTPUT_DIRECTORY\n", argv[0]);
        return 2;
    }
    memset(&summary, 0, sizeof(summary));

    join_path(path, sizeof(path), argv[1],
              "equalizer_editor_pcm16_cases.csv");
    case_csv = fopen(path, "wb");
    join_path(path, sizeof(path), argv[1],
              "equalizer_editor_pcm16_response.csv");
    response_csv = fopen(path, "wb");
    join_path(path, sizeof(path), argv[1],
              "equalizer_editor_pcm16_transitions.csv");
    transition_csv = fopen(path, "wb");
    if ((case_csv == 0) || (response_csv == 0) ||
        (transition_csv == 0))
    {
        failf("cannot create CSV outputs in %s", argv[1]);
        if (case_csv != 0) fclose(case_csv);
        if (response_csv != 0) fclose(response_csv);
        if (transition_csv != 0) fclose(transition_csv);
        return 2;
    }
    fprintf(case_csv,
        "pattern,signal,samples,input_peak_fs,input_rms_pcm,output_peak_fs,output_rms_pcm,clip_count,nonfinite_count,identity_checked,identity_mismatch_count,repeated_adjacent_frames,repeated_frames_expected,preamp_db,predicted_peak_db\n");
    fprintf(response_csv,
        "pattern,band,frequency_hz,theory_real,theory_imag,measured_real,measured_imag,theory_magnitude_db,measured_magnitude_db,magnitude_error_db,complex_abs_error,output_rms_pcm,clip_count,nonfinite_count\n");
    fprintf(transition_csv,
        "name,from_mode,to_mode,token,previous_applied_sequence,transition_total_samples,processed_samples,duration_ms,progress_checks,progress_monotonic,sequence_held_until_complete,applied_after_boundary,clip_count,nonfinite_count,input_peak_fs,output_peak_fs,repeated_adjacent_frames,input_wav,output_wav\n");

    run_static_cases(argv[1], case_csv, response_csv, &summary);
    run_transition_cases(argv[1], transition_csv, &summary);

    (void)fclose(case_csv);
    (void)fclose(response_csv);
    (void)fclose(transition_csv);
    (void)write_summary_json(argv[1], &summary);

    printf("equalizer_editor_pcm16 static=%d response=%d transitions=%d "
           "max_center_error_db=%.6f max_complex_error=%.8f "
           "clip=%lu nonfinite=%lu identity_mismatch=%lu "
           "repeated_transition_frames=%lu failures=%d\n",
           summary.static_case_count,
           summary.response_point_count,
           summary.transition_case_count,
           summary.max_center_error_db,
           summary.max_complex_error,
           summary.total_clip_count,
           summary.total_nonfinite_count,
           summary.identity_mismatch_count,
           summary.transition_repeated_frames,
           failures);
    return (failures == 0) ? 0 : 1;
}
