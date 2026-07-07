/**
 * user_subband_codec_loopback.h
 *
 * Lightweight WOLA frequency-domain quantize/dequantize loopback.
 * This simulates codec damage for listening tests; it is not a bitstream
 * writer and does not implement Opus/AAC/MP3/G.722.
 */

#ifndef _USER_SUBBAND_CODEC_LOOPBACK_H_
#define _USER_SUBBAND_CODEC_LOOPBACK_H_

#include "user_subband_wola.h"

#define SUBBAND_CODEC_LOOP_MIN_BITS 0
#define SUBBAND_CODEC_LOOP_MAX_BITS 6
#define SUBBAND_CODEC_LOOP_DEFAULT_KBPS 240
#define SUBBAND_CODEC_LOOP_PCM_KBPS 800.0f
#define SUBBAND_CODEC_LOOP_BAND_SIDE_BITS 13

extern volatile int SUBBAND_CODEC_LOOP_DebugEnabled;
extern volatile int SUBBAND_CODEC_LOOP_DebugTargetKbps;
extern volatile int SUBBAND_CODEC_LOOP_DebugRequestedTargetKbps;
extern volatile float SUBBAND_CODEC_LOOP_DebugEstimatedBitrateKbps;
extern volatile float SUBBAND_CODEC_LOOP_DebugCompressionRatio;
extern volatile float SUBBAND_CODEC_LOOP_DebugAvgBitsPerScalar;
extern volatile float SUBBAND_CODEC_LOOP_DebugBandBits0;
extern volatile float SUBBAND_CODEC_LOOP_DebugBandBits1;
extern volatile float SUBBAND_CODEC_LOOP_DebugBandBits2;
extern volatile float SUBBAND_CODEC_LOOP_DebugBandBits3;
extern volatile float SUBBAND_CODEC_LOOP_DebugBandBits4;
extern volatile float SUBBAND_CODEC_LOOP_DebugBandBits5;
extern volatile float SUBBAND_CODEC_LOOP_DebugBandBits6;
extern volatile float SUBBAND_CODEC_LOOP_DebugBandBits7;
extern volatile int SUBBAND_CODEC_LOOP_DebugInvalidCount;
extern volatile int SUBBAND_CODEC_LOOP_DebugClippingCount;
extern volatile unsigned long SUBBAND_CODEC_LOOP_DebugFrames;

void SubbandCodecLoopback_Init(void);
void SubbandCodecLoopback_Reset(void);
void SubbandCodecLoopback_SetEnabled(int enable);
int SubbandCodecLoopback_IsEnabled(void);
void SubbandCodecLoopback_SetTargetKbps(int kbps);
int SubbandCodecLoopback_GetTargetKbps(void);
void SubbandCodecLoopback_ProcessSpectrum(float *re, float *im);

#endif /* _USER_SUBBAND_CODEC_LOOPBACK_H_ */
