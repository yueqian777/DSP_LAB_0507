/**
 * user_subband_codec.h
 *
 * Minimal WOLA-DFT subband scalar quantization codec for PC algo-only
 * evaluation. This is not an implementation of G.722, SBC, MPEG, or Opus.
 */

#ifndef _USER_SUBBAND_CODEC_H_
#define _USER_SUBBAND_CODEC_H_

#include "user_subband_wola.h"

#define SUBBAND_CODEC_PCM_BITRATE_KBPS 800.0f
/* 0 means drop/zero the band; signed scalar quantization starts at 2 bits. */
#define SUBBAND_CODEC_MIN_BITS_PER_SCALAR 0
#define SUBBAND_CODEC_MAX_BITS_PER_SCALAR 6
#define SUBBAND_CODEC_BAND_BITS_SIDE_BITS 3
#define SUBBAND_CODEC_BAND_SCALE_SIDE_BITS 16

typedef struct
{
    unsigned long bit_count;
} SubbandCodecBitWriter;

typedef struct
{
    unsigned long bit_pos;
} SubbandCodecBitReader;

typedef struct
{
    int target_bitrate_kbps;
    unsigned long payload_bits;
    unsigned long scalar_bits;
    unsigned long scalar_count;
    int hops;
    int clipping_count;
    int nonzero_output_count;
    int invalid_count;
    float bitrate_kbps;
    float compression_ratio;
    float avg_bits_per_scalar;
    float band_energy_before[SUBBAND_NUM_BANDS];
    float band_energy_after[SUBBAND_NUM_BANDS];
    float band_bits[SUBBAND_NUM_BANDS];
} SubbandCodecStats;

void SubbandCodec_ResetStats(SubbandCodecStats *stats);
int SubbandCodec_ProcessPcm(const short *input,
                            short *output,
                            int sample_count,
                            int target_bitrate_kbps,
                            SubbandCodecStats *stats);

#endif /* _USER_SUBBAND_CODEC_H_ */
