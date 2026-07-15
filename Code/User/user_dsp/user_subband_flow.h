/**
 * user_subband_flow.h
 *
 * 描述: 子频带分析-合成实时测试例程头文件
 * 功能: 提供 1024 点 short 帧处理接口和板端 AD/DA 测试入口
 */

#ifndef _USER_SUBBAND_FLOW_H_
#define _USER_SUBBAND_FLOW_H_

#ifndef ADDA_SUBBAND_BYPASS
#define ADDA_SUBBAND_BYPASS 0
#endif

#ifndef SUBBAND_BYPASS
#define SUBBAND_BYPASS ADDA_SUBBAND_BYPASS
#endif

#ifndef SUBBAND_USE_LEGACY_FIR
#define SUBBAND_USE_LEGACY_FIR 0
#endif

/* JTAG-only board THD harness. The production Project 3.2 path keeps this off. */
#ifndef SUBBAND_THD_BOARD_TEST
#define SUBBAND_THD_BOARD_TEST 0
#endif

/* Keep the original FIR bank compiled as a test backend without making it
 * the default project path. */
#ifndef SUBBAND_ENABLE_LEGACY_FIR_BACKEND
#define SUBBAND_ENABLE_LEGACY_FIR_BACKEND 1
#endif

#include "user_subband_wola.h"

#define SUBBAND_D        8
/* Guidebook N is the FIR order. Coefficient count is N + 1. */
#define SUBBAND_FILTER_ORDER 128
#define SUBBAND_ORD      (SUBBAND_FILTER_ORDER + 1)

#ifndef SUBBAND_USE_CRITICAL_SAMPLING
#define SUBBAND_USE_CRITICAL_SAMPLING 1
#endif

/*
 * Full-rate smoke-test reconstruction uses the FIR bank without down/up sampling.
 * Critical-sampling mode follows the guidebook example and needs D gain compensation.
 */
#if SUBBAND_USE_CRITICAL_SAMPLING
#define SUBBAND_RECON_GAIN ((double)SUBBAND_D)
#else
#define SUBBAND_RECON_GAIN 1.0
#endif

#ifdef SUBBAND_FLOW_ALGO_ONLY
#define SUBBAND_FRM_LEN  1024
#else
#include "adc_api.h"
#define SUBBAND_FRM_LEN  ADC_SAMPLE_1024
#endif

#define SUBBAND_LEN      (SUBBAND_FRM_LEN / SUBBAND_D)

#define SUBBAND_DEMO_MODE_RAW_BYPASS        0
#define SUBBAND_DEMO_MODE_WOLA_ONLY         1
#define SUBBAND_DEMO_MODE_WOLA_DENOISE      2
#define SUBBAND_DEMO_MODE_MILD_DENOISE      3
#define SUBBAND_DEMO_MODE_WOLA_MS_DENOISE   4
#define SUBBAND_DEMO_MODE_MILD_MS_DENOISE   5
#define SUBBAND_DEMO_MODE_MCRA_DENOISE      6
#define SUBBAND_DEMO_MODE_STRONG_MCRA_DENOISE 7
#define SUBBAND_DEMO_MODE_MCRA_CODEC_LOOPBACK 8
#define SUBBAND_DEMO_MODE_STRONG_MCRA_CODEC_LOOPBACK 9
#define SUBBAND_DEMO_MODE_FIXED_CODEC_LOOPBACK 10
#define SUBBAND_DEMO_MODE_CODEC_LOOPBACK_ONLY 11

#define SUBBAND_BACKEND_WOLA       0
#define SUBBAND_BACKEND_LEGACY_FIR 1
#define SUBBAND_BACKEND_POLYPHASE  2

#ifndef SUBBAND_DEMO_DEFAULT_MODE
#define SUBBAND_DEMO_DEFAULT_MODE SUBBAND_DEMO_MODE_WOLA_DENOISE
#endif

#if SUBBAND_USE_LEGACY_FIR && defined(SUBBAND_FLOW_ALGO_ONLY)
typedef struct
{
    double max_error;
    double mse;
    double input_energy;
    double output_energy_ratio;
    double aligned_snr_db;
    double aligned_gain;
    int aligned_lag;
} SUBBAND_OFFLINE_METRICS;

void Subband_Offline_Sine_Test(double freq_hz, double sample_rate_hz, double amplitude,
                               SUBBAND_OFFLINE_METRICS *metrics);
#endif

void Subband_FilterBank_Init(void);
void Subband_Process_1024(short *in, short *out);

#ifndef SUBBAND_FLOW_ALGO_ONLY
extern volatile int SUBBAND_DebugDemoMode;
extern volatile int SUBBAND_DebugAppliedDemoMode;
extern volatile unsigned long SUBBAND_DebugDemoModeChanges;
extern volatile int SUBBAND_DebugPersistentCodecKbps;
extern volatile int SUBBAND_DebugRequestedBackend;
extern volatile int SUBBAND_DebugAppliedBackend;
extern volatile unsigned long SUBBAND_DebugBackendChanges;
extern volatile unsigned long SUBBAND_DebugAlgoFrames;
extern volatile unsigned long SUBBAND_DebugLastCycles;
extern volatile unsigned long SUBBAND_DebugMaxCycles;
extern volatile float SUBBAND_DebugLastMs;
extern volatile float SUBBAND_DebugMaxMs;
extern volatile float SUBBAND_DebugCpuUsagePercent;
extern volatile unsigned long SUBBAND_DebugInputNonzeroFrames;
extern volatile unsigned long SUBBAND_DebugOutputNonzeroFrames;
extern volatile unsigned long SUBBAND_DebugInputClipFrames;
extern volatile unsigned long SUBBAND_DebugOutputClipFrames;
extern volatile int SUBBAND_DebugInputPeakMax;
extern volatile int SUBBAND_DebugOutputPeakMax;
/* Integer running mean-square values across the current benchmark window. */
extern volatile unsigned long SUBBAND_DebugInputMeanSquareAvg;
extern volatile unsigned long SUBBAND_DebugOutputMeanSquareAvg;
extern volatile unsigned long SUBBAND_DebugBenchmarkResetRequest;
extern volatile short SUBBAND_DebugAdFirstSample;
extern volatile short SUBBAND_DebugDaFirstSample;
extern volatile int SUBBAND_DebugAdPeak;
extern volatile int SUBBAND_DebugDaPeak;

void Subband_Request_Codec_Target(int kbps);
void Subband_Flow_Example(void);

#if SUBBAND_THD_BOARD_TEST
#define SUBBAND_THD_INPUT_SAMPLES       500000
#define SUBBAND_THD_PROCESSED_SAMPLES   500736
#define SUBBAND_THD_FRAME_COUNT         489
#define SUBBAND_THD_STATUS_BOOT         0
#define SUBBAND_THD_STATUS_WAITING      1
#define SUBBAND_THD_STATUS_RUNNING      2
#define SUBBAND_THD_STATUS_READY        3

extern unsigned int SUBBAND_THD_InputPacked[SUBBAND_THD_PROCESSED_SAMPLES / 2];
extern unsigned int SUBBAND_THD_OutputPacked[SUBBAND_THD_PROCESSED_SAMPLES / 2];
extern volatile unsigned int SUBBAND_THD_DebugRequest;
extern volatile unsigned int SUBBAND_THD_DebugFrequencyHz;
extern volatile unsigned int SUBBAND_THD_DebugStatus;
extern volatile unsigned long SUBBAND_THD_DebugFrames;
extern volatile unsigned long SUBBAND_THD_DebugCycleCount;
extern volatile unsigned long SUBBAND_THD_DebugMaxFrameCycles;
extern volatile const char SUBBAND_THD_DebugBuildGitSha[];
extern volatile const int SUBBAND_THD_DebugBuildDirty;

void Subband_THD_Board_Test_Example(void);
#endif
#endif

#endif /* _USER_SUBBAND_FLOW_H_ */
