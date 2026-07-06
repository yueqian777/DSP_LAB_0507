/**
 * user_subband_codec_eval.c
 *
 * PC algo-only comparison of direct subband compression and
 * denoise-then-compress. Codec ideas are inspired by subband and transform
 * coding families, but this file does not implement G.722, SBC, MPEG, or Opus.
 */

#include "user_subband_codec_eval.h"
#include "user_subband_codec.h"
#include "user_subband_wola.h"
#include "user_subband_denoise.h"

#ifdef SUBBAND_ALGO_ONLY

#include <math.h>
#include <stdio.h>
#include <string.h>

#ifndef SUBBAND_EVAL_AUDIO_DIR
#define SUBBAND_EVAL_AUDIO_DIR "test_vectors/subband_eval_audio"
#endif

#define CODEC_EVAL_MAX_SAMPLES 500000
#define CODEC_EVAL_SKIP_TAIL (2 * SUBBAND_NFFT)
#define CODEC_EVAL_MAX_LAG (2 * SUBBAND_NFFT)
#define CODEC_EVAL_WAV_HEADER_MAX 4096
#define CODEC_EVAL_CASES 3
#define CODEC_EVAL_RATES 3

static short CodecEval_Noisy[CODEC_EVAL_MAX_SAMPLES];
static short CodecEval_Clean[CODEC_EVAL_MAX_SAMPLES];
static short CodecEval_Denoised[CODEC_EVAL_MAX_SAMPLES];
static short CodecEval_DirectOut[CODEC_EVAL_MAX_SAMPLES];
static short CodecEval_DenoiseCodecOut[CODEC_EVAL_MAX_SAMPLES];

typedef struct
{
    float target_snr_db;
    int target_bitrate_kbps;
    float input_snr_db;
    float denoise_output_snr_db;
    float direct_compress_snr_db;
    float denoise_then_compress_snr_db;
    float direct_reconstruction_snr_db;
    float denoise_then_reconstruction_snr_db;
    SubbandCodecStats direct_stats;
    SubbandCodecStats denoise_stats;
    int pass;
} CodecEvalRow;

static int CodecEval_Min_Int(int a, int b)
{
    if (a < b)
    {
        return a;
    }
    return b;
}

static int CodecEval_Read_Wav(const char *path, short *dst, int max_samples,
                              int *sample_rate, int *sample_count)
{
    FILE *fp;
    unsigned char header[CODEC_EVAL_WAV_HEADER_MAX];
    int header_len;
    int pos;
    int fmt_rate;
    int fmt_channels;
    int fmt_bits;
    int data_offset;
    int data_bytes;
    int samples;
    size_t got;

    fp = fopen(path, "rb");
    if (fp == 0)
    {
        printf("codec_wav_read FAIL open=%s\n", path);
        return 0;
    }

    header_len = (int)fread(header, 1, sizeof(header), fp);
    if (header_len < 44)
    {
        fclose(fp);
        printf("codec_wav_read FAIL short_header=%s\n", path);
        return 0;
    }

    fmt_rate = 0;
    fmt_channels = 0;
    fmt_bits = 0;
    data_offset = -1;
    data_bytes = 0;

    pos = 12;
    while (pos + 8 <= header_len)
    {
        unsigned int chunk_size;

        chunk_size =
            (unsigned int)header[pos + 4] |
            ((unsigned int)header[pos + 5] << 8) |
            ((unsigned int)header[pos + 6] << 16) |
            ((unsigned int)header[pos + 7] << 24);
        if ((header[pos] == 'f') && (header[pos + 1] == 'm') &&
            (header[pos + 2] == 't') && (header[pos + 3] == ' '))
        {
            if (pos + 24 <= header_len)
            {
                fmt_channels =
                    (int)(header[pos + 10] |
                          ((unsigned int)header[pos + 11] << 8));
                fmt_rate =
                    (int)(header[pos + 12] |
                          ((unsigned int)header[pos + 13] << 8) |
                          ((unsigned int)header[pos + 14] << 16) |
                          ((unsigned int)header[pos + 15] << 24));
                fmt_bits =
                    (int)(header[pos + 22] |
                          ((unsigned int)header[pos + 23] << 8));
            }
        }
        if ((header[pos] == 'd') && (header[pos + 1] == 'a') &&
            (header[pos + 2] == 't') && (header[pos + 3] == 'a'))
        {
            data_offset = pos + 8;
            data_bytes = (int)chunk_size;
            break;
        }
        pos += 8 + (int)chunk_size + ((chunk_size & 1U) ? 1 : 0);
    }

    if ((data_offset < 0) || (fmt_rate != SUBBAND_SAMPLE_RATE) ||
        (fmt_channels != 1) || (fmt_bits != 16))
    {
        fclose(fp);
        printf("codec_wav_read FAIL format path=%s rate=%d ch=%d bits=%d\n",
               path, fmt_rate, fmt_channels, fmt_bits);
        return 0;
    }

    samples = data_bytes / 2;
    if (samples > max_samples)
    {
        samples = max_samples;
    }
    fseek(fp, data_offset, SEEK_SET);
    got = fread(dst, 2, (size_t)samples, fp);
    fclose(fp);
    if ((int)got != samples)
    {
        printf("codec_wav_read FAIL data path=%s got=%d expected=%d\n",
               path, (int)got, samples);
        return 0;
    }

    *sample_rate = fmt_rate;
    *sample_count = samples;
    return 1;
}

static void CodecEval_Join_Path(char *dst, int dst_len,
                                const char *dir, const char *name)
{
    int n;

    n = snprintf(dst, (size_t)dst_len, "%s/%s", dir, name);
    if ((n < 0) || (n >= dst_len))
    {
        dst[0] = '\0';
    }
}

static const char *CodecEval_Noisy_Name(float target_snr_db)
{
    if (target_snr_db < 7.5f)
    {
        return "noisy_speechlike_snr5dB_50k_10s_first2s_noise.wav";
    }
    if (target_snr_db < 15.0f)
    {
        return "noisy_speechlike_snr10dB_50k_10s_first2s_noise.wav";
    }
    return "noisy_speechlike_snr20dB_50k_10s_first2s_noise.wav";
}

static const char *CodecEval_Clean_Name(float target_snr_db)
{
    if (target_snr_db < 7.5f)
    {
        return "clean_reference_for_snr5dB_50k_10s_first2s_silence.wav";
    }
    if (target_snr_db < 15.0f)
    {
        return "clean_reference_for_snr10dB_50k_10s_first2s_silence.wav";
    }
    return "clean_reference_for_snr20dB_50k_10s_first2s_silence.wav";
}

static float CodecEval_Input_Snr(const short *noisy, const short *clean,
                                 int count, int start)
{
    int i;
    double sig;
    double noise;

    sig = 0.0;
    noise = 0.0;
    for (i = start; i < count - CODEC_EVAL_SKIP_TAIL; i++)
    {
        double c;
        double n;

        c = (double)clean[i];
        n = (double)noisy[i] - c;
        sig += c * c;
        noise += n * n;
    }
    return (float)(10.0 * log10((sig + 1.0e-20) /
                                (noise + 1.0e-20)));
}

static int CodecEval_Find_Lag(const short *ref, const short *out,
                              int count, int start)
{
    int lag;
    int best_lag;
    double best_err;

    best_lag = SUBBAND_EXPECTED_DELAY;
    best_err = 1.0e300;
    for (lag = 0; lag <= CODEC_EVAL_MAX_LAG; lag += 4)
    {
        int i;
        double err;

        err = 0.0;
        for (i = start; i < count - lag - CODEC_EVAL_SKIP_TAIL; i += 2)
        {
            double d;

            d = (double)out[i + lag] - (double)ref[i];
            err += d * d;
        }
        if (err < best_err)
        {
            best_err = err;
            best_lag = lag;
        }
    }
    return best_lag;
}

static float CodecEval_Aligned_Snr(const short *ref, const short *out,
                                   int count, int start)
{
    int i;
    int lag;
    double sig;
    double err;

    lag = CodecEval_Find_Lag(ref, out, count, start);
    sig = 0.0;
    err = 0.0;
    for (i = start; i < count - lag - CODEC_EVAL_SKIP_TAIL; i++)
    {
        double x;
        double y;
        double d;

        x = (double)ref[i];
        y = (double)out[i + lag];
        d = y - x;
        sig += x * x;
        err += d * d;
    }
    return (float)(10.0 * log10((sig + 1.0e-20) /
                                (err + 1.0e-20)));
}

static int CodecEval_Output_Is_Valid(const short *out, int count,
                                     const SubbandCodecStats *stats)
{
    int i;
    int nonzero;

    if ((stats == 0) || (stats->invalid_count != 0) ||
        (stats->payload_bits == 0UL))
    {
        return 0;
    }
    nonzero = 0;
    for (i = 0; i < count; i++)
    {
        if (out[i] != 0)
        {
            nonzero = 1;
            break;
        }
    }
    return nonzero;
}

static void CodecEval_Reset_Denoise_Path(void)
{
    SubbandWOLA_Init();
    SubbandWOLA_ResetStream();
    SubbandWOLA_ResetAllGains();
    SubbandWOLA_SetBypass(0);
    SubbandDenoise_Reset();
    SubbandDenoise_StartNoiseLearning();
}

static void CodecEval_Process_Denoise(const short *input, short *output,
                                      int frames)
{
    int frame;

    CodecEval_Reset_Denoise_Path();
    for (frame = 0; frame < frames; frame++)
    {
        SubbandWOLA_ProcessFrame((short *)&input[frame * SUBBAND_FRAME_LEN],
                                 &output[frame * SUBBAND_FRAME_LEN]);
    }
}

static void CodecEval_Write_Header(FILE *fp)
{
    int band;

    if (fp == 0)
    {
        return;
    }
    fprintf(fp, "codec_case,target_bitrate_kbps,target_snr_db,"
                "input_snr_db,denoise_output_snr_db,"
                "direct_compress_snr_db,denoise_then_compress_snr_db,"
                "direct_reconstruction_snr_db,"
                "denoise_then_reconstruction_snr_db,"
                "direct_bitrate_kbps,denoise_then_bitrate_kbps,"
                "direct_compression_ratio,denoise_then_compression_ratio,"
                "direct_payload_bits,denoise_payload_bits,"
                "avg_bits_per_scalar");
    for (band = 0; band < SUBBAND_NUM_BANDS; band++)
    {
        fprintf(fp, ",direct_band_energy_%d", band);
    }
    for (band = 0; band < SUBBAND_NUM_BANDS; band++)
    {
        fprintf(fp, ",denoise_band_energy_%d", band);
    }
    for (band = 0; band < SUBBAND_NUM_BANDS; band++)
    {
        fprintf(fp, ",direct_band_bits_%d", band);
    }
    for (band = 0; band < SUBBAND_NUM_BANDS; band++)
    {
        fprintf(fp, ",denoise_band_bits_%d", band);
    }
    fprintf(fp, ",pass\n");
}

static void CodecEval_Write_Row(FILE *fp, const char *case_name,
                                const CodecEvalRow *row)
{
    int band;

    if ((fp == 0) || (row == 0))
    {
        return;
    }
    fprintf(fp, "%s,%d,%.0f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,"
                "%.3f,%.3f,%.3f,%.3f,%lu,%lu,%.3f",
            case_name,
            row->target_bitrate_kbps,
            row->target_snr_db,
            row->input_snr_db,
            row->denoise_output_snr_db,
            row->direct_compress_snr_db,
            row->denoise_then_compress_snr_db,
            row->direct_reconstruction_snr_db,
            row->denoise_then_reconstruction_snr_db,
            row->direct_stats.bitrate_kbps,
            row->denoise_stats.bitrate_kbps,
            row->direct_stats.compression_ratio,
            row->denoise_stats.compression_ratio,
            row->direct_stats.payload_bits,
            row->denoise_stats.payload_bits,
            row->denoise_stats.avg_bits_per_scalar);
    for (band = 0; band < SUBBAND_NUM_BANDS; band++)
    {
        fprintf(fp, ",%.3f", row->direct_stats.band_energy_before[band]);
    }
    for (band = 0; band < SUBBAND_NUM_BANDS; band++)
    {
        fprintf(fp, ",%.3f", row->denoise_stats.band_energy_before[band]);
    }
    for (band = 0; band < SUBBAND_NUM_BANDS; band++)
    {
        fprintf(fp, ",%.3f", row->direct_stats.band_bits[band]);
    }
    for (band = 0; band < SUBBAND_NUM_BANDS; band++)
    {
        fprintf(fp, ",%.3f", row->denoise_stats.band_bits[band]);
    }
    fprintf(fp, ",%d\n", row->pass);
}

static int CodecEval_Run_Row(float target_snr_db,
                             int target_bitrate_kbps,
                             float input_snr_db,
                             float denoise_output_snr_db,
                             int samples,
                             CodecEvalRow *row)
{
    int start;
    int fail;
    int direct_ok;
    int denoise_ok;

    memset(row, 0, sizeof(*row));
    row->target_snr_db = target_snr_db;
    row->target_bitrate_kbps = target_bitrate_kbps;
    row->input_snr_db = input_snr_db;
    row->denoise_output_snr_db = denoise_output_snr_db;

    fail = 0;
    if (SubbandCodec_ProcessPcm(CodecEval_Noisy, CodecEval_DirectOut,
                                samples, target_bitrate_kbps,
                                &row->direct_stats) != 0)
    {
        fail = 1;
    }
    if (SubbandCodec_ProcessPcm(CodecEval_Denoised,
                                CodecEval_DenoiseCodecOut,
                                samples, target_bitrate_kbps,
                                &row->denoise_stats) != 0)
    {
        fail = 1;
    }

    start = (int)(2.5f * SUBBAND_SAMPLE_RATE);
    row->direct_compress_snr_db =
        CodecEval_Aligned_Snr(CodecEval_Clean, CodecEval_DirectOut,
                              samples, start);
    row->denoise_then_compress_snr_db =
        CodecEval_Aligned_Snr(CodecEval_Clean, CodecEval_DenoiseCodecOut,
                              samples, start);
    row->direct_reconstruction_snr_db =
        CodecEval_Aligned_Snr(CodecEval_Noisy, CodecEval_DirectOut,
                              samples, start);
    row->denoise_then_reconstruction_snr_db =
        CodecEval_Aligned_Snr(CodecEval_Denoised,
                              CodecEval_DenoiseCodecOut, samples, start);

    direct_ok = CodecEval_Output_Is_Valid(CodecEval_DirectOut, samples,
                                          &row->direct_stats);
    denoise_ok = CodecEval_Output_Is_Valid(CodecEval_DenoiseCodecOut,
                                           samples, &row->denoise_stats);
    if ((direct_ok == 0) || (denoise_ok == 0))
    {
        fail = 1;
    }
    if ((row->direct_stats.bitrate_kbps >
         ((float)target_bitrate_kbps * 1.05f)) ||
        (row->denoise_stats.bitrate_kbps >
         ((float)target_bitrate_kbps * 1.05f)))
    {
        fail = 1;
    }
    if (target_bitrate_kbps == 240)
    {
        if ((row->direct_stats.compression_ratio < 3.0f) ||
            (row->direct_stats.compression_ratio > 3.8f) ||
            (row->denoise_stats.compression_ratio < 3.0f) ||
            (row->denoise_stats.compression_ratio > 3.8f))
        {
            fail = 1;
        }
    }
    if (target_snr_db < 15.0f)
    {
        if (row->denoise_then_compress_snr_db + 0.10f <
            row->direct_compress_snr_db)
        {
            fail = 1;
        }
    }

    row->pass = (fail == 0);
    printf("codec_eval target_snr=%.0f bitrate=%d input_snr=%.3f "
           "denoise_snr=%.3f direct_snr=%.3f denoise_codec_snr=%.3f "
           "direct_recon=%.3f denoise_recon=%.3f direct_kbps=%.3f "
           "denoise_kbps=%.3f pass=%s\n",
           target_snr_db,
           target_bitrate_kbps,
           row->input_snr_db,
           row->denoise_output_snr_db,
           row->direct_compress_snr_db,
           row->denoise_then_compress_snr_db,
           row->direct_reconstruction_snr_db,
           row->denoise_then_reconstruction_snr_db,
           row->direct_stats.bitrate_kbps,
           row->denoise_stats.bitrate_kbps,
           row->pass ? "PASS" : "FAIL");
    return fail;
}

static int CodecEval_Run_Case(float target_snr_db,
                              FILE *csv,
                              float *direct_recon_by_rate,
                              float *denoise_recon_by_rate)
{
    const int rates[CODEC_EVAL_RATES] = {160, 240, 320};
    char noisy_path[512];
    char clean_path[512];
    int rate;
    int noisy_count;
    int clean_count;
    int frames;
    int samples;
    int start;
    int failures;
    int rate_idx;
    float input_snr_db;
    float denoise_output_snr_db;
    CodecEvalRow row;

    failures = 0;
    CodecEval_Join_Path(noisy_path, sizeof(noisy_path),
                        SUBBAND_EVAL_AUDIO_DIR,
                        CodecEval_Noisy_Name(target_snr_db));
    CodecEval_Join_Path(clean_path, sizeof(clean_path),
                        SUBBAND_EVAL_AUDIO_DIR,
                        CodecEval_Clean_Name(target_snr_db));
    if (!CodecEval_Read_Wav(noisy_path, CodecEval_Noisy,
                            CODEC_EVAL_MAX_SAMPLES, &rate, &noisy_count))
    {
        return 1;
    }
    if (!CodecEval_Read_Wav(clean_path, CodecEval_Clean,
                            CODEC_EVAL_MAX_SAMPLES, &rate, &clean_count))
    {
        return 1;
    }

    frames = noisy_count / SUBBAND_FRAME_LEN;
    frames = CodecEval_Min_Int(frames, clean_count / SUBBAND_FRAME_LEN);
    samples = frames * SUBBAND_FRAME_LEN;
    start = (int)(2.5f * SUBBAND_SAMPLE_RATE);
    input_snr_db = CodecEval_Input_Snr(CodecEval_Noisy, CodecEval_Clean,
                                       samples, start);
    memset(CodecEval_Denoised, 0, sizeof(CodecEval_Denoised));
    CodecEval_Process_Denoise(CodecEval_Noisy, CodecEval_Denoised, frames);
    denoise_output_snr_db =
        CodecEval_Aligned_Snr(CodecEval_Clean, CodecEval_Denoised,
                              samples, start);

    for (rate_idx = 0; rate_idx < CODEC_EVAL_RATES; rate_idx++)
    {
        failures += CodecEval_Run_Row(target_snr_db, rates[rate_idx],
                                      input_snr_db, denoise_output_snr_db,
                                      samples, &row);
        direct_recon_by_rate[rate_idx] =
            row.direct_reconstruction_snr_db;
        denoise_recon_by_rate[rate_idx] =
            row.denoise_then_reconstruction_snr_db;
        if (target_snr_db < 7.5f)
        {
            CodecEval_Write_Row(csv, "snr5", &row);
        }
        else if (target_snr_db < 15.0f)
        {
            CodecEval_Write_Row(csv, "snr10", &row);
        }
        else
        {
            CodecEval_Write_Row(csv, "snr20", &row);
        }
    }

    if ((direct_recon_by_rate[2] + 0.10f) < direct_recon_by_rate[0])
    {
        printf("codec_eval bitrate_order direct FAIL snr=%.0f "
               "160=%.3f 320=%.3f\n",
               target_snr_db, direct_recon_by_rate[0],
               direct_recon_by_rate[2]);
        failures++;
    }
    if ((denoise_recon_by_rate[2] + 0.10f) < denoise_recon_by_rate[0])
    {
        printf("codec_eval bitrate_order denoise FAIL snr=%.0f "
               "160=%.3f 320=%.3f\n",
               target_snr_db, denoise_recon_by_rate[0],
               denoise_recon_by_rate[2]);
        failures++;
    }

    return failures;
}

int SubbandCodecEval_OfflineTest_All(void)
{
    FILE *csv;
    int failures;
    float direct_recon[CODEC_EVAL_RATES];
    float denoise_recon[CODEC_EVAL_RATES];

    failures = 0;
    csv = fopen("subband_codec_eval_report.csv", "w");
    CodecEval_Write_Header(csv);
    failures += CodecEval_Run_Case(5.0f, csv, direct_recon, denoise_recon);
    failures += CodecEval_Run_Case(10.0f, csv, direct_recon, denoise_recon);
    failures += CodecEval_Run_Case(20.0f, csv, direct_recon, denoise_recon);
    if (csv != 0)
    {
        fclose(csv);
    }

    printf("Codec CSV report: subband_codec_eval_report.csv\n");
    printf("SubbandCodecEval_OfflineTest_All failures=%d\n", failures);
    return failures;
}

#else

int SubbandCodecEval_OfflineTest_All(void)
{
    return 0;
}

#endif /* SUBBAND_ALGO_ONLY */
