/**
 * user_subband_wola.h
 *
 * Streaming WOLA-DFT subband analysis/synthesis interface.
 */

#ifndef _USER_SUBBAND_WOLA_H_
#define _USER_SUBBAND_WOLA_H_

#ifndef SUBBAND_FRAME_LEN
#define SUBBAND_FRAME_LEN      1024
#endif

#ifndef SUBBAND_SAMPLE_RATE
#define SUBBAND_SAMPLE_RATE    50000.0f
#endif

#ifndef SUBBAND_NFFT
#define SUBBAND_NFFT           512
#endif

#ifndef SUBBAND_HOP
#define SUBBAND_HOP            256
#endif

#ifndef SUBBAND_NUM_BANDS
#define SUBBAND_NUM_BANDS      8
#endif

#define SUBBAND_POS_BINS       (SUBBAND_NFFT / 2 + 1)
#define SUBBAND_EXPECTED_DELAY (SUBBAND_NFFT - SUBBAND_HOP)

#ifndef SUBBAND_BYPASS
#define SUBBAND_BYPASS         0
#endif

#if (SUBBAND_FRAME_LEN % SUBBAND_HOP) != 0
#error "SUBBAND_FRAME_LEN must be a multiple of SUBBAND_HOP"
#endif

#if (SUBBAND_NFFT != 512) && (SUBBAND_NFFT != 256)
#error "SUBBAND_NFFT currently supports 512 or 256"
#endif

#if (SUBBAND_HOP * 2) != SUBBAND_NFFT
#error "This WOLA implementation expects 50 percent overlap"
#endif

extern volatile unsigned long SUBBAND_WOLA_DebugFrames;
extern volatile float SUBBAND_WOLA_DebugInputEnergy;
extern volatile float SUBBAND_WOLA_DebugOutputEnergy;
extern volatile float SUBBAND_WOLA_DebugEnergyRatio;
extern volatile int SUBBAND_WOLA_DebugBypass;
extern volatile unsigned long SUBBAND_WOLA_DebugLastCycles;
extern volatile unsigned long SUBBAND_WOLA_DebugMaxCycles;
extern volatile float SUBBAND_WOLA_DebugLastMs;
extern volatile float SUBBAND_WOLA_DebugMaxMs;

void SubbandWOLA_Init(void);
void SubbandWOLA_ProcessFrame(short *in, short *out);
void SubbandWOLA_SetBandGain(int band, float gain);
void SubbandWOLA_SetBypass(int enable);
float SubbandWOLA_GetCOLAMaxError(void);

#ifdef SUBBAND_ALGO_ONLY
void SubbandWOLA_ProcessFrameFloat(float *in, float *out);
#endif

#endif /* _USER_SUBBAND_WOLA_H_ */
