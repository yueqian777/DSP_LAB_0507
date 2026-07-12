/**
 * user_equalizer_flow.h
 *
 * Project 3.3 board-side AD/DA flow for the graphic equalizer.
 */

#ifndef _USER_EQUALIZER_FLOW_H_
#define _USER_EQUALIZER_FLOW_H_

#include "user_equalizer.h"

#define EQ_DIAG_RAW_COPY       0
#define EQ_DIAG_FLOAT_COPY     1
#define EQ_DIAG_FLAT           2
#define EQ_DIAG_SINGLE_BAND    3
#define EQ_DIAG_PRESET         4

#define EQ_CAPTURE_FRAMES              8
#define EQ_CAPTURE_SAMPLES             (EQ_CAPTURE_FRAMES * EQ_FRAME_LEN)
#define EQ_TRIGGER_PRE_FRAMES          4
#define EQ_TRIGGER_POST_FRAMES         8
#define EQ_TRIGGER_TOTAL_FRAMES        12
#define EQ_TRIGGER_CAPTURE_SAMPLES     \
    (EQ_TRIGGER_TOTAL_FRAMES * EQ_FRAME_LEN)

#define EQ_CAPTURE_TRIGGER_LCD_JOB          0x01U
#define EQ_CAPTURE_TRIGGER_MODE_SWITCH      0x02U
#define EQ_CAPTURE_TRIGGER_AUDIO_DURING_LCD 0x04U

#define EQ_FRAME_SERVICE_BUDGET_MS \
    (1000.0f * (float)EQ_FRAME_LEN / EQ_SAMPLE_RATE)
#define EQ_FRAME_SERVICE_BUDGET_CYCLES \
    ((unsigned long)(456000.0f * EQ_FRAME_SERVICE_BUDGET_MS + 0.5f))

#define EQ_LCD_FAULT_LATENCY_MISS   0x02UL
#define EQ_LCD_FAULT_OVERLAP        0x04UL
#define EQ_LCD_FAULT_DROPPED        0x08UL
#define EQ_LCD_POLICY_NONE          0
#define EQ_LCD_POLICY_DEFER         1
#define EQ_LCD_POLICY_SERVICE       2

typedef struct
{
    unsigned long last_service_frame;
    unsigned long last_deferred_frame;
    unsigned long last_status_request_frame;
} EQ_LCD_SERVICE_POLICY;

typedef struct
{
    unsigned long previous_latency_misses;
    unsigned long previous_overlaps;
    unsigned long previous_dropped;
} EQ_LCD_FAULT_POLICY;

extern volatile unsigned long EQ_DebugAdFrames;
extern volatile unsigned long EQ_DebugDaFrames;
extern volatile unsigned long EQ_DebugProcessFrames;
extern volatile unsigned long EQ_DebugAlgoLastCycles;
extern volatile unsigned long EQ_DebugAlgoMaxCycles;
extern volatile float EQ_DebugAlgoLastMs;
extern volatile float EQ_DebugAlgoMaxMs;
extern volatile unsigned long EQ_DebugModeServiceLastCycles;
extern volatile unsigned long EQ_DebugModeServiceMaxCycles;
extern volatile unsigned long EQ_DebugFrameServiceLastCycles;
extern volatile unsigned long EQ_DebugFrameServiceMaxCycles;
extern volatile float EQ_DebugFrameServiceLastMs;
extern volatile float EQ_DebugFrameServiceMaxMs;
extern volatile unsigned long EQ_DebugFrameLatencyLastCycles;
extern volatile unsigned long EQ_DebugFrameLatencyMaxCycles;
extern volatile float EQ_DebugFrameLatencyLastMs;
extern volatile float EQ_DebugFrameLatencyMaxMs;
extern volatile unsigned long EQ_DebugDeadlineMissCount;
extern volatile unsigned long EQ_DebugFrameLatencyDeadlineMissCount;
extern volatile unsigned long EQ_DebugFrameServiceOverlapCount;
extern volatile unsigned long EQ_DebugFrameServiceDroppedCount;
extern volatile int EQ_DebugMode;
extern volatile int EQ_DebugDiagPath;
extern volatile int EQ_DebugRequestedMode;
extern volatile int EQ_DebugTransitionTargetMode;
extern volatile int EQ_DebugAppliedMode;
extern volatile unsigned long EQ_DebugModeChangeCount;
extern volatile float EQ_DebugBandGainDb[EQ_NUM_BANDS];
extern volatile const unsigned long EQ_DebugBuildMagic;
extern volatile const char EQ_DebugBuildId[];
extern volatile const char EQ_DebugBuildVersion[];
extern volatile const char EQ_DebugBuildGitSha[];
extern volatile const char EQ_DebugBuildTimestamp[];
extern volatile const int EQ_DebugBuildDirty;

extern short EQ_CaptureInput[EQ_CAPTURE_SAMPLES];
extern short EQ_CaptureOutput[EQ_CAPTURE_SAMPLES];
extern short EQ_TriggerCaptureInput[EQ_TRIGGER_CAPTURE_SAMPLES];
extern short EQ_TriggerCaptureOutput[EQ_TRIGGER_CAPTURE_SAMPLES];

extern volatile unsigned int EQ_CaptureManualRequest;
extern volatile unsigned int EQ_CaptureManualActive;
extern volatile unsigned int EQ_CaptureManualReady;
extern volatile unsigned int EQ_CaptureManualIndex;
extern volatile unsigned int EQ_CaptureManualFrameCount;
extern volatile unsigned int EQ_TriggerCaptureRequest;
extern volatile unsigned int EQ_TriggerCaptureActive;
extern volatile unsigned int EQ_TriggerCaptureTriggered;
extern volatile unsigned int EQ_TriggerCaptureReady;
extern volatile unsigned int EQ_TriggerCaptureArmedSource;
extern volatile unsigned int EQ_TriggerCaptureSource;
extern volatile unsigned int EQ_TriggerCapturePrefill;
extern volatile unsigned int EQ_TriggerCaptureArmedReady;
extern volatile unsigned int EQ_TriggerCapturePreWriteIndex;
extern volatile unsigned int EQ_TriggerCapturePostCount;

void EqualizerCapture_Reset(void);
void EqualizerCapture_AcknowledgeManual(void);
void EqualizerCapture_AcknowledgeTrigger(void);
void EqualizerCapture_OnFrame(const short *input, const short *output);
void EqualizerCapture_NotifyEvent(unsigned int source);
void EqualizerCapture_NotifyModeChange(unsigned long before_count,
                                       unsigned long after_count);

void EqualizerLcdPolicy_Init(EQ_LCD_SERVICE_POLICY *policy);
int EqualizerLcdPolicy_CanService(const EQ_LCD_SERVICE_POLICY *policy,
                                  unsigned long process_frames,
                                  int flag_ad,
                                  int flag_da,
                                  int flag_ad_done,
                                  int frame_service_pending,
                                  int has_pending_job);
int EqualizerLcdPolicy_Decide(const EQ_LCD_SERVICE_POLICY *policy,
                              unsigned long process_frames,
                              int outer_flag_ad,
                              int outer_flag_da,
                              int outer_flag_ad_done,
                              int outer_frame_service_pending,
                              int predraw_flag_ad,
                              int predraw_flag_da,
                              int predraw_flag_ad_done,
                              int predraw_frame_service_pending,
                              int has_pending_job);
void EqualizerLcdPolicy_RecordService(EQ_LCD_SERVICE_POLICY *policy,
                                      unsigned long process_frames,
                                      int completed_job);
int EqualizerLcdPolicy_RecordDeferred(EQ_LCD_SERVICE_POLICY *policy,
                                      unsigned long process_frames);
int EqualizerLcdPolicy_ShouldRequestStatus(EQ_LCD_SERVICE_POLICY *policy,
                                           unsigned long process_frames);
void EqualizerLcdFaultPolicy_Init(EQ_LCD_FAULT_POLICY *policy);
unsigned long EqualizerLcdFaultPolicy_Monitor(
    EQ_LCD_FAULT_POLICY *policy,
    int runtime_enabled,
    unsigned long latency_misses,
    unsigned long overlaps,
    unsigned long dropped);

void Equalizer_Flow_Example(void);

#endif /* _USER_EQUALIZER_FLOW_H_ */
