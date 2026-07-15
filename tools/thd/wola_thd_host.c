/* Host-only raw PCM wrapper around the production WOLA implementation. */

#include "user_subband_wola.h"
#include "user_subband_denoise.h"
#include "user_subband_codec_loopback.h"
#include "user_subband_eval.h"

#include <stdio.h>
#include <stdlib.h>


volatile unsigned long SUBBAND_EVAL_DebugAdFrames = 0UL;
volatile unsigned long SUBBAND_EVAL_DebugDaFrames = 0UL;
volatile unsigned long SUBBAND_EVAL_DebugWolaFrames = 0UL;
volatile unsigned long SUBBAND_EVAL_DebugDenoiseFrames = 0UL;
volatile float SUBBAND_EVAL_DebugFrameBudgetMs = 0.0f;
volatile float SUBBAND_EVAL_DebugDenoiseLastMs = 0.0f;
volatile float SUBBAND_EVAL_DebugDenoiseMaxMs = 0.0f;
volatile float SUBBAND_EVAL_DebugCpuUsagePercent = 0.0f;


static int file_sample_count(FILE *file, long *sample_count)
{
    long byte_count;

    if (fseek(file, 0L, SEEK_END) != 0)
    {
        return 1;
    }
    byte_count = ftell(file);
    if ((byte_count < 0L) || ((byte_count & 1L) != 0L))
    {
        return 1;
    }
    if (fseek(file, 0L, SEEK_SET) != 0)
    {
        return 1;
    }
    *sample_count = byte_count / (long)sizeof(short);
    return (*sample_count > 0L) ? 0 : 1;
}


int main(int argc, char **argv)
{
    FILE *input_file;
    FILE *output_file;
    short *input;
    short *output;
    long input_samples;
    long process_samples;
    long frame;
    long frame_count;
    long zero_fill;
    int status;

    if (argc != 3)
    {
        fprintf(stderr, "usage: wola_thd_host input.pcm16le output.pcm16le\n");
        return 2;
    }

    input_file = fopen(argv[1], "rb");
    if (input_file == 0)
    {
        fprintf(stderr, "cannot open input: %s\n", argv[1]);
        return 3;
    }
    if (file_sample_count(input_file, &input_samples) != 0)
    {
        fclose(input_file);
        fprintf(stderr, "invalid PCM16 input length\n");
        return 4;
    }

    process_samples = input_samples + (long)SUBBAND_EXPECTED_DELAY;
    process_samples =
        ((process_samples + (long)SUBBAND_FRAME_LEN - 1L) /
         (long)SUBBAND_FRAME_LEN) * (long)SUBBAND_FRAME_LEN;
    input = (short *)calloc((size_t)process_samples, sizeof(short));
    output = (short *)calloc((size_t)process_samples, sizeof(short));
    if ((input == 0) || (output == 0))
    {
        fclose(input_file);
        free(input);
        free(output);
        fprintf(stderr, "allocation failed\n");
        return 5;
    }
    if (fread(input, sizeof(short), (size_t)input_samples, input_file) !=
        (size_t)input_samples)
    {
        fclose(input_file);
        free(input);
        free(output);
        fprintf(stderr, "PCM16 input read failed\n");
        return 6;
    }
    fclose(input_file);

    SubbandWOLA_Init();
    SubbandWOLA_SetBypass(0);
    SubbandWOLA_ResetStream();
    SubbandWOLA_ResetAllGains();
    SubbandCodecLoopback_SetEnabled(0);
    SubbandDenoise_StopLearning();
    SubbandDenoise_SetEnabled(0);

    frame_count = process_samples / (long)SUBBAND_FRAME_LEN;
    for (frame = 0L; frame < frame_count; frame++)
    {
        SubbandWOLA_ProcessFrame(
            &input[frame * (long)SUBBAND_FRAME_LEN],
            &output[frame * (long)SUBBAND_FRAME_LEN]);
    }

    output_file = fopen(argv[2], "wb");
    if (output_file == 0)
    {
        free(input);
        free(output);
        fprintf(stderr, "cannot open output: %s\n", argv[2]);
        return 7;
    }
    status = 0;
    if (fwrite(output, sizeof(short), (size_t)process_samples, output_file) !=
        (size_t)process_samples)
    {
        status = 8;
    }
    if (fclose(output_file) != 0)
    {
        status = 8;
    }

    zero_fill = process_samples - input_samples;
    printf(
        "{\"input_samples\":%ld,\"processed_samples\":%ld,"
        "\"frame_count\":%ld,\"frame_length\":%d,"
        "\"startup_delay_samples\":%d,\"tail_flush_samples\":%d,"
        "\"zero_fill_samples\":%ld,\"band_gains\":\"all_1.0\","
        "\"denoise\":\"disabled\",\"codec_loopback\":\"disabled\"}\n",
        input_samples,
        process_samples,
        frame_count,
        SUBBAND_FRAME_LEN,
        SUBBAND_EXPECTED_DELAY,
        SUBBAND_EXPECTED_DELAY,
        zero_fill);

    free(input);
    free(output);
    return status;
}
