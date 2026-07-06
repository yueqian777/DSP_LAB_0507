/**
 * user_subband_audio_compare.c
 *
 * Offline WAV export for comparing input, WOLA, denoise, and codec paths.
 * This tool is for PC listening tests only; it is not part of the realtime
 * C6748 speaker path.
 */

#include "user_subband_audio_compare.h"
#include "user_subband_wola.h"
#include "user_subband_denoise.h"
#include "user_subband_codec.h"

#ifdef SUBBAND_ALGO_ONLY

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define AUDIO_COMPARE_RATE_COUNT 3

static const int AudioCompare_Rates[AUDIO_COMPARE_RATE_COUNT] =
{
    160, 240, 320
};

static unsigned int AudioCompare_Read_U32(const unsigned char *p)
{
    return (unsigned int)p[0] |
           ((unsigned int)p[1] << 8) |
           ((unsigned int)p[2] << 16) |
           ((unsigned int)p[3] << 24);
}

static unsigned int AudioCompare_Read_U16(const unsigned char *p)
{
    return (unsigned int)p[0] | ((unsigned int)p[1] << 8);
}

static void AudioCompare_Write_U16(FILE *fp, unsigned int value)
{
    unsigned char b[2];

    b[0] = (unsigned char)(value & 0xffU);
    b[1] = (unsigned char)((value >> 8) & 0xffU);
    fwrite(b, 1, 2, fp);
}

static void AudioCompare_Write_U32(FILE *fp, unsigned int value)
{
    unsigned char b[4];

    b[0] = (unsigned char)(value & 0xffU);
    b[1] = (unsigned char)((value >> 8) & 0xffU);
    b[2] = (unsigned char)((value >> 16) & 0xffU);
    b[3] = (unsigned char)((value >> 24) & 0xffU);
    fwrite(b, 1, 4, fp);
}

static int AudioCompare_Read_Wav(const char *path, short **samples_out,
                                 int *sample_count_out)
{
    FILE *fp;
    unsigned char riff[12];
    int fmt_seen;
    int data_seen;
    int fmt_format;
    int fmt_channels;
    int fmt_rate;
    int fmt_bits;
    short *samples;
    int sample_count;

    fp = fopen(path, "rb");
    if (fp == 0)
    {
        printf("audio_compare read FAIL open=%s\n", path);
        return 1;
    }
    if (fread(riff, 1, sizeof(riff), fp) != sizeof(riff))
    {
        fclose(fp);
        printf("audio_compare read FAIL short_header=%s\n", path);
        return 1;
    }
    if ((memcmp(riff, "RIFF", 4) != 0) || (memcmp(&riff[8], "WAVE", 4) != 0))
    {
        fclose(fp);
        printf("audio_compare read FAIL not_wav=%s\n", path);
        return 1;
    }

    fmt_seen = 0;
    data_seen = 0;
    fmt_format = 0;
    fmt_channels = 0;
    fmt_rate = 0;
    fmt_bits = 0;
    samples = 0;
    sample_count = 0;

    while (!data_seen)
    {
        unsigned char chunk[8];
        unsigned int chunk_size;

        if (fread(chunk, 1, sizeof(chunk), fp) != sizeof(chunk))
        {
            break;
        }
        chunk_size = AudioCompare_Read_U32(&chunk[4]);
        if (memcmp(chunk, "fmt ", 4) == 0)
        {
            unsigned char fmt[40];
            unsigned int read_size;

            read_size = chunk_size;
            if (read_size > sizeof(fmt))
            {
                read_size = sizeof(fmt);
            }
            memset(fmt, 0, sizeof(fmt));
            if (fread(fmt, 1, read_size, fp) != read_size)
            {
                fclose(fp);
                return 1;
            }
            if (chunk_size > read_size)
            {
                fseek(fp, (long)(chunk_size - read_size), SEEK_CUR);
            }
            fmt_format = (int)AudioCompare_Read_U16(&fmt[0]);
            fmt_channels = (int)AudioCompare_Read_U16(&fmt[2]);
            fmt_rate = (int)AudioCompare_Read_U32(&fmt[4]);
            fmt_bits = (int)AudioCompare_Read_U16(&fmt[14]);
            fmt_seen = 1;
        }
        else if (memcmp(chunk, "data", 4) == 0)
        {
            size_t got;

            if ((fmt_seen == 0) || (fmt_format != 1) ||
                (fmt_channels != 1) ||
                (fmt_rate != (int)SUBBAND_SAMPLE_RATE) ||
                (fmt_bits != 16))
            {
                fclose(fp);
                printf("audio_compare read FAIL format path=%s "
                       "format=%d rate=%d ch=%d bits=%d\n",
                       path, fmt_format, fmt_rate, fmt_channels, fmt_bits);
                return 1;
            }
            sample_count = (int)(chunk_size / 2U);
            samples = (short *)malloc((size_t)sample_count * sizeof(short));
            if (samples == 0)
            {
                fclose(fp);
                return 1;
            }
            got = fread(samples, sizeof(short), (size_t)sample_count, fp);
            if (got != (size_t)sample_count)
            {
                free(samples);
                fclose(fp);
                return 1;
            }
            data_seen = 1;
        }
        else
        {
            fseek(fp, (long)chunk_size, SEEK_CUR);
        }
        if ((chunk_size & 1U) != 0U)
        {
            fseek(fp, 1L, SEEK_CUR);
        }
    }

    fclose(fp);
    if ((data_seen == 0) || (samples == 0) || (sample_count <= 0))
    {
        if (samples != 0)
        {
            free(samples);
        }
        printf("audio_compare read FAIL no_data=%s\n", path);
        return 1;
    }

    *samples_out = samples;
    *sample_count_out = sample_count;
    return 0;
}

static int AudioCompare_Write_Wav(const char *path, const short *samples,
                                  int sample_count)
{
    FILE *fp;
    unsigned int data_bytes;

    fp = fopen(path, "wb");
    if (fp == 0)
    {
        printf("audio_compare write FAIL open=%s\n", path);
        return 1;
    }
    data_bytes = (unsigned int)sample_count * 2U;
    fwrite("RIFF", 1, 4, fp);
    AudioCompare_Write_U32(fp, 36U + data_bytes);
    fwrite("WAVE", 1, 4, fp);
    fwrite("fmt ", 1, 4, fp);
    AudioCompare_Write_U32(fp, 16U);
    AudioCompare_Write_U16(fp, 1U);
    AudioCompare_Write_U16(fp, 1U);
    AudioCompare_Write_U32(fp, (unsigned int)SUBBAND_SAMPLE_RATE);
    AudioCompare_Write_U32(fp, (unsigned int)SUBBAND_SAMPLE_RATE * 2U);
    AudioCompare_Write_U16(fp, 2U);
    AudioCompare_Write_U16(fp, 16U);
    fwrite("data", 1, 4, fp);
    AudioCompare_Write_U32(fp, data_bytes);
    fwrite(samples, sizeof(short), (size_t)sample_count, fp);
    fclose(fp);
    return 0;
}

static int AudioCompare_Pad_Count(int count)
{
    int rem;

    rem = count % SUBBAND_FRAME_LEN;
    if (rem == 0)
    {
        return count;
    }
    return count + (SUBBAND_FRAME_LEN - rem);
}

static double AudioCompare_Energy(const short *x, int count)
{
    int i;
    double sum;

    sum = 0.0;
    for (i = 0; i < count; i++)
    {
        double v;

        v = (double)x[i];
        sum += v * v;
    }
    return sum;
}

static int AudioCompare_Nonzero_Count(const short *x, int count)
{
    int i;
    int nonzero;

    nonzero = 0;
    for (i = 0; i < count; i++)
    {
        if (x[i] != 0)
        {
            nonzero++;
        }
    }
    return nonzero;
}

static int AudioCompare_Clip_Count(const short *x, int count)
{
    int i;
    int clips;

    clips = 0;
    for (i = 0; i < count; i++)
    {
        if ((x[i] == 32767) || (x[i] == -32768))
        {
            clips++;
        }
    }
    return clips;
}

static void AudioCompare_Write_Metrics_Header(FILE *csv)
{
    fprintf(csv, "mode,target_bitrate_kbps,actual_bitrate_kbps,"
                 "compression_ratio,payload_bits,avg_bits_per_scalar,"
                 "output_energy_ratio,clipping_count,invalid_count,"
                 "nonzero_output_count\n");
}

static void AudioCompare_Write_Metrics(FILE *csv, const char *mode,
                                       int target_bitrate_kbps,
                                       const SubbandCodecStats *stats,
                                       const short *output,
                                       int sample_count,
                                       double input_energy)
{
    double output_energy;
    double energy_ratio;
    float actual_bitrate;
    float compression_ratio;
    float avg_bits;
    unsigned long payload_bits;
    int clipping_count;
    int invalid_count;
    int nonzero_count;

    output_energy = AudioCompare_Energy(output, sample_count);
    energy_ratio = output_energy / (input_energy + 1.0e-20);
    if (stats != 0)
    {
        actual_bitrate = stats->bitrate_kbps;
        compression_ratio = stats->compression_ratio;
        avg_bits = stats->avg_bits_per_scalar;
        payload_bits = stats->payload_bits;
        clipping_count = stats->clipping_count;
        invalid_count = stats->invalid_count;
        nonzero_count = stats->nonzero_output_count;
    }
    else
    {
        actual_bitrate = 0.0f;
        compression_ratio = 0.0f;
        avg_bits = 0.0f;
        payload_bits = 0UL;
        clipping_count = AudioCompare_Clip_Count(output, sample_count);
        invalid_count = 0;
        nonzero_count = AudioCompare_Nonzero_Count(output, sample_count);
    }
    fprintf(csv, "%s,%d,%.3f,%.3f,%lu,%.3f,%.9f,%d,%d,%d\n",
            mode, target_bitrate_kbps, actual_bitrate, compression_ratio,
            payload_bits, avg_bits, energy_ratio, clipping_count,
            invalid_count, nonzero_count);
}

static void AudioCompare_Reset_Wola_Only(void)
{
    SubbandWOLA_Init();
    SubbandWOLA_SetBypass(0);
    SubbandWOLA_ResetStream();
    SubbandWOLA_ResetAllGains();
    SubbandDenoise_StopLearning();
    SubbandDenoise_SetEnabled(0);
}

static void AudioCompare_Reset_Denoise(void)
{
    SubbandWOLA_Init();
    SubbandWOLA_SetBypass(0);
    SubbandWOLA_ResetStream();
    SubbandWOLA_ResetAllGains();
    SubbandDenoise_Reset();
    SubbandDenoise_StartNoiseLearning();
}

static void AudioCompare_Process_Wola(const short *input, short *output,
                                      int padded_count, int denoise)
{
    int frame;
    int frames;

    if (denoise != 0)
    {
        AudioCompare_Reset_Denoise();
    }
    else
    {
        AudioCompare_Reset_Wola_Only();
    }
    frames = padded_count / SUBBAND_FRAME_LEN;
    for (frame = 0; frame < frames; frame++)
    {
        SubbandWOLA_ProcessFrame((short *)&input[frame * SUBBAND_FRAME_LEN],
                                 &output[frame * SUBBAND_FRAME_LEN]);
    }
}

static int AudioCompare_Export_Codec(FILE *csv, const char *path,
                                     const char *mode,
                                     const short *input,
                                     short *output,
                                     int sample_count,
                                     int padded_count,
                                     int target_bitrate_kbps,
                                     double input_energy)
{
    SubbandCodecStats stats;

    memset(output, 0, (size_t)padded_count * sizeof(short));
    if (SubbandCodec_ProcessPcm(input, output, padded_count,
                                target_bitrate_kbps, &stats) != 0)
    {
        return 1;
    }
    if (AudioCompare_Write_Wav(path, output, sample_count) != 0)
    {
        return 1;
    }
    AudioCompare_Write_Metrics(csv, mode, target_bitrate_kbps, &stats,
                               output, sample_count, input_energy);
    return 0;
}

int SubbandAudioCompare_ExportAll(const char *input_wav_path)
{
    short *input;
    short *input_padded;
    short *wola_only;
    short *denoised;
    short *codec_out;
    FILE *csv;
    int sample_count;
    int padded_count;
    int i;
    int failures;
    double input_energy;

    input = 0;
    input_padded = 0;
    wola_only = 0;
    denoised = 0;
    codec_out = 0;
    csv = 0;
    sample_count = 0;
    failures = 0;

    if (AudioCompare_Read_Wav(input_wav_path, &input, &sample_count) != 0)
    {
        return 1;
    }
    padded_count = AudioCompare_Pad_Count(sample_count);
    input_padded = (short *)calloc((size_t)padded_count, sizeof(short));
    wola_only = (short *)calloc((size_t)padded_count, sizeof(short));
    denoised = (short *)calloc((size_t)padded_count, sizeof(short));
    codec_out = (short *)calloc((size_t)padded_count, sizeof(short));
    if ((input_padded == 0) || (wola_only == 0) ||
        (denoised == 0) || (codec_out == 0))
    {
        failures = 1;
        goto cleanup;
    }
    memcpy(input_padded, input, (size_t)sample_count * sizeof(short));
    input_energy = AudioCompare_Energy(input_padded, sample_count);

    csv = fopen("compare_audio_metrics.csv", "w");
    if (csv == 0)
    {
        failures = 1;
        goto cleanup;
    }
    AudioCompare_Write_Metrics_Header(csv);

    failures += AudioCompare_Write_Wav("compare_00_input.wav",
                                       input_padded, sample_count);
    AudioCompare_Write_Metrics(csv, "input", 0, 0, input_padded,
                               sample_count, input_energy);

    AudioCompare_Process_Wola(input_padded, wola_only, padded_count, 0);
    failures += AudioCompare_Write_Wav("compare_01_wola_only.wav",
                                       wola_only, sample_count);
    AudioCompare_Write_Metrics(csv, "wola_only", 0, 0, wola_only,
                               sample_count, input_energy);

    AudioCompare_Process_Wola(input_padded, denoised, padded_count, 1);
    failures += AudioCompare_Write_Wav("compare_02_wola_denoise.wav",
                                       denoised, sample_count);
    AudioCompare_Write_Metrics(csv, "wola_denoise", 0, 0, denoised,
                               sample_count, input_energy);

    for (i = 0; i < AUDIO_COMPARE_RATE_COUNT; i++)
    {
        char wav_name[64];
        char mode_name[64];

        sprintf(wav_name, "compare_%02d_codec_direct_%dk.wav",
                3 + i, AudioCompare_Rates[i]);
        sprintf(mode_name, "codec_direct_%dk", AudioCompare_Rates[i]);
        failures += AudioCompare_Export_Codec(csv, wav_name, mode_name,
                                              input_padded, codec_out,
                                              sample_count, padded_count,
                                              AudioCompare_Rates[i],
                                              input_energy);
    }

    for (i = 0; i < AUDIO_COMPARE_RATE_COUNT; i++)
    {
        char wav_name[64];
        char mode_name[64];

        sprintf(wav_name, "compare_%02d_denoise_codec_%dk.wav",
                6 + i, AudioCompare_Rates[i]);
        sprintf(mode_name, "denoise_codec_%dk", AudioCompare_Rates[i]);
        failures += AudioCompare_Export_Codec(csv, wav_name, mode_name,
                                              denoised, codec_out,
                                              sample_count, padded_count,
                                              AudioCompare_Rates[i],
                                              input_energy);
    }

cleanup:
    if (csv != 0)
    {
        fclose(csv);
    }
    if (input != 0)
    {
        free(input);
    }
    if (input_padded != 0)
    {
        free(input_padded);
    }
    if (wola_only != 0)
    {
        free(wola_only);
    }
    if (denoised != 0)
    {
        free(denoised);
    }
    if (codec_out != 0)
    {
        free(codec_out);
    }
    if (failures == 0)
    {
        printf("Audio compare exported compare_*.wav and "
               "compare_audio_metrics.csv\n");
    }
    return (failures == 0) ? 0 : 1;
}

#else

int SubbandAudioCompare_ExportAll(const char *input_wav_path)
{
    (void)input_wav_path;
    return 1;
}

#endif /* SUBBAND_ALGO_ONLY */
