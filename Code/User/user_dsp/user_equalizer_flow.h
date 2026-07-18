/**
 * user_equalizer_flow.h
 *
 * Project 3.3 board-side AD/DA flow for the graphic equalizer.
 */

#ifndef _USER_EQUALIZER_FLOW_H_
#define _USER_EQUALIZER_FLOW_H_

#include "user_equalizer_control.h"

#ifndef EQ_ENABLE_AUDIO_FEATURE_ANALYZER
#define EQ_ENABLE_AUDIO_FEATURE_ANALYZER 0
#endif

#ifndef EQ_ENABLE_SMART_BASS
#define EQ_ENABLE_SMART_BASS 0
#endif

#ifndef EQ_ENABLE_DYNAMIC_CLARITY
#define EQ_ENABLE_DYNAMIC_CLARITY 0
#endif

#ifndef EQ_ENABLE_HARSHNESS_GUARD
#define EQ_ENABLE_HARSHNESS_GUARD 0
#endif

#ifndef EQ_ENABLE_DYNAMIC_CLARITY_TIMING_DIAGNOSTICS
#define EQ_ENABLE_DYNAMIC_CLARITY_TIMING_DIAGNOSTICS 0
#endif

#ifndef EQ_ENABLE_DYNAMIC_CLARITY_TRANSITION_CAPTURE
#define EQ_ENABLE_DYNAMIC_CLARITY_TRANSITION_CAPTURE 0
#endif

#if (EQ_ENABLE_SMART_BASS != 0) && \
    (EQ_ENABLE_AUDIO_FEATURE_ANALYZER == 0)
#error Smart Bass requires the audio feature analyzer.
#endif

#if (EQ_ENABLE_DYNAMIC_CLARITY != 0) && \
    (EQ_ENABLE_AUDIO_FEATURE_ANALYZER == 0)
#error Dynamic Clarity requires the audio feature analyzer.
#endif

#if (EQ_ENABLE_HARSHNESS_GUARD != 0) && \
    (EQ_ENABLE_AUDIO_FEATURE_ANALYZER == 0)
#error Harshness Guard requires the audio feature analyzer.
#endif

#if (EQ_ENABLE_DYNAMIC_CLARITY_TIMING_DIAGNOSTICS != 0) && \
    (EQ_ENABLE_DYNAMIC_CLARITY == 0)
#error Dynamic Clarity timing diagnostics require Dynamic Clarity.
#endif

#if (EQ_ENABLE_DYNAMIC_CLARITY_TRANSITION_CAPTURE != 0) && \
    (EQ_ENABLE_DYNAMIC_CLARITY == 0)
#error Dynamic Clarity transition capture requires Dynamic Clarity.
#endif

#define DYNAMIC_CLARITY_PATH_IDENTITY                    0
#define DYNAMIC_CLARITY_PATH_STABLE_FILTER               1
#define DYNAMIC_CLARITY_PATH_TRANSITION_0_TO_FILTER      2
#define DYNAMIC_CLARITY_PATH_TRANSITION_FILTER_TO_FILTER 3
#define DYNAMIC_CLARITY_PATH_TRANSITION_FILTER_TO_0      4
#define DYNAMIC_CLARITY_PATH_COUNT                       5

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
#define EQ_CAPTURE_TRIGGER_DYNAMIC_CLARITY  0x08U

#define EQ_DYNAMIC_CLARITY_CAPTURE_PREROLL_FRAMES 100U

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

#define EQ_BACKGROUND_NONE          0
#define EQ_BACKGROUND_BUILDER       1
#define EQ_BACKGROUND_LCD           2
#define EQ_BACKGROUND_ANALYZER      3
#define EQ_BACKGROUND_UART          4

#define EQ_ANALYZER_ACTION_NONE         0
#define EQ_ANALYZER_ACTION_DISABLE      1
#define EQ_ANALYZER_ACTION_ENABLE_RESET 2
#define EQ_ANALYZER_ACTION_MANUAL_RESET 3

typedef struct
{
    unsigned long consumed_frame;
    int consumed_frame_valid;
    int consumed_kind;
    int next_preference;
} EQ_BACKGROUND_SERVICE_STATE;

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

typedef struct
{
    unsigned long baseline_process_frames;
    unsigned long baseline_deadline;
    unsigned long baseline_latency;
    unsigned long baseline_overlap;
    unsigned long baseline_dropped;
    unsigned long deadline_delta;
    unsigned long latency_delta;
    unsigned long overlap_delta;
    unsigned long dropped_delta;
    unsigned int flags_before;
    unsigned int pending;
    unsigned int complete;
    unsigned int audio_arrived;
} EQ_UART_FEATURE_AUDIT;

extern volatile unsigned long EQ_DebugAdFrames;
extern volatile unsigned long EQ_DebugDaFrames;
extern volatile unsigned long EQ_DebugProcessFrames;
/* 1 flow; 2 ADC init; 3 DAC init; 4 EQ/cache; 5 runtime ready;
 * 6 ADC start; 7 DAC start; 8 loop; 9 first AD; 10 process; 11 DA. */
extern volatile unsigned long EQ_DebugInitStage;
extern volatile unsigned int EQ_DebugFlagAdDone;
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
extern volatile const unsigned int EQ_DebugAnalyzerCompiled;
extern volatile unsigned int EQ_DebugAnalyzerEnabled;
extern volatile unsigned int EQ_DebugAnalyzerResetRequest;
extern volatile unsigned int EQ_DebugAnalyzerPending;
extern volatile unsigned int EQ_DebugAnalyzerValid;
extern volatile unsigned int EQ_DebugAnalyzerWarmup;
extern volatile unsigned long EQ_DebugAnalyzerRunCount;
extern volatile unsigned long EQ_DebugAnalyzerAnalysisCount;
extern volatile unsigned long EQ_DebugAnalyzerDeferredCount;
extern volatile unsigned long EQ_DebugAnalyzerPendingOverwriteCount;
extern volatile unsigned long EQ_DebugAnalyzerAudioArrivedCount;
extern volatile unsigned long EQ_DebugAnalyzerLastCycles;
extern volatile unsigned long EQ_DebugAnalyzerMaxCycles;
extern volatile float EQ_DebugAnalyzerPeakDbfs;
extern volatile float EQ_DebugAnalyzerRmsDbfs;
extern volatile float EQ_DebugAnalyzerBassDb;
extern volatile float EQ_DebugAnalyzerMudDb;
extern volatile float EQ_DebugAnalyzerPresenceDb;
extern volatile float EQ_DebugAnalyzerBrightnessDb;
extern volatile const unsigned int EQ_DebugSmartBassCompiled;
extern volatile unsigned int EQ_DebugSmartBassEnabled;
extern volatile int EQ_DebugSmartBassStrength;
extern volatile unsigned int EQ_DebugSmartBassProcessingActive;
extern volatile float EQ_DebugSmartBassInputBassDb;
extern volatile float EQ_DebugSmartBassInputRmsDbfs;
extern volatile int EQ_DebugSmartBassRequestedLevel;
extern volatile int EQ_DebugSmartBassAppliedLevel;
extern volatile int EQ_DebugSmartBassPendingLevel;
extern volatile float EQ_DebugSmartBassRequestedGainDb;
extern volatile float EQ_DebugSmartBassAppliedGainDb;
extern volatile unsigned int EQ_DebugSmartBassTransitionActive;
extern volatile float EQ_DebugSmartBassTransitionProgress;
extern volatile unsigned long EQ_DebugSmartBassDecisionCount;
extern volatile unsigned long EQ_DebugSmartBassLevelChangeCount;
extern volatile unsigned long EQ_DebugSmartBassTransitionCount;
extern volatile unsigned long EQ_DebugSmartBassInvalidReleaseCount;
extern volatile unsigned long EQ_DebugSmartBassLastCycles;
extern volatile unsigned long EQ_DebugSmartBassMaxCycles;
extern volatile unsigned long EQ_DebugSmartBassSaturationCount;
extern volatile unsigned long EQ_DebugSmartBassNonFiniteCount;
extern volatile int EQ_DebugSmartBassReason;
extern volatile const unsigned int EQ_DebugDynamicClarityCompiled;
extern volatile unsigned int EQ_DebugDynamicClarityEnabled;
extern volatile int EQ_DebugDynamicClarityStrength;
extern volatile unsigned int EQ_DebugDynamicClarityProcessingActive;
extern volatile float EQ_DebugDynamicClarityMudDb;
extern volatile float EQ_DebugDynamicClarityPresenceDb;
extern volatile float EQ_DebugDynamicClarityMaskingDb;
extern volatile float EQ_DebugDynamicClarityRmsDbfs;
extern volatile int EQ_DebugDynamicClarityRequestedLevel;
extern volatile int EQ_DebugDynamicClarityAppliedLevel;
extern volatile int EQ_DebugDynamicClarityPendingLevel;
extern volatile float EQ_DebugDynamicClarityRequestedGainDb;
extern volatile float EQ_DebugDynamicClarityAppliedGainDb;
extern volatile unsigned int EQ_DebugDynamicClarityTransitionActive;
extern volatile float EQ_DebugDynamicClarityTransitionProgress;
extern volatile int EQ_DebugDynamicClarityReason;
extern volatile unsigned long EQ_DebugDynamicClarityDecisionCount;
extern volatile unsigned long EQ_DebugDynamicClarityLevelChangeCount;
extern volatile unsigned long EQ_DebugDynamicClarityTransitionCount;
extern volatile unsigned long EQ_DebugDynamicClarityInvalidReleaseCount;
extern volatile unsigned long EQ_DebugDynamicClarityLastCycles;
extern volatile unsigned long EQ_DebugDynamicClarityMaxCycles;
extern volatile unsigned long EQ_DebugDynamicClaritySaturationCount;
extern volatile unsigned long EQ_DebugDynamicClarityNonFiniteCount;
extern volatile const unsigned int EQ_DebugHarshnessGuardCompiled;
extern volatile unsigned int EQ_DebugHarshnessGuardEnabled;
extern volatile int EQ_DebugHarshnessGuardStrength;
extern volatile unsigned int EQ_DebugHarshnessGuardProcessingActive;
extern volatile float EQ_DebugHarshnessGuardBrightnessDb;
extern volatile float EQ_DebugHarshnessGuardPresenceDb;
extern volatile float EQ_DebugHarshnessGuardExcessDb;
extern volatile float EQ_DebugHarshnessGuardRmsDbfs;
extern volatile int EQ_DebugHarshnessGuardRequestedLevel;
extern volatile int EQ_DebugHarshnessGuardAppliedLevel;
extern volatile int EQ_DebugHarshnessGuardPendingLevel;
extern volatile float EQ_DebugHarshnessGuardRequestedGainDb;
extern volatile float EQ_DebugHarshnessGuardAppliedGainDb;
extern volatile unsigned int EQ_DebugHarshnessGuardTransitionActive;
extern volatile float EQ_DebugHarshnessGuardTransitionProgress;
extern volatile int EQ_DebugHarshnessGuardReason;
extern volatile unsigned long EQ_DebugHarshnessGuardDecisionCount;
extern volatile unsigned long EQ_DebugHarshnessGuardLevelChangeCount;
extern volatile unsigned long EQ_DebugHarshnessGuardTransitionCount;
extern volatile unsigned long EQ_DebugHarshnessGuardInvalidReleaseCount;
extern volatile unsigned long EQ_DebugHarshnessGuardLastCycles;
extern volatile unsigned long EQ_DebugHarshnessGuardMaxCycles;
extern volatile unsigned long EQ_DebugHarshnessGuardSaturationCount;
extern volatile unsigned long EQ_DebugHarshnessGuardNonFiniteCount;
#if EQ_ENABLE_DYNAMIC_CLARITY_TRANSITION_CAPTURE != 0
extern volatile unsigned int EQ_DebugDynamicClarityTransitionCaptureRequest;
extern volatile unsigned int EQ_DebugDynamicClarityTransitionCaptureActive;
extern volatile unsigned int EQ_DebugDynamicClarityTransitionCaptureDone;
extern volatile unsigned int EQ_DebugDynamicClarityTransitionCaptureOverride;
extern volatile int EQ_DebugDynamicClarityTransitionCaptureBaseLevel;
extern volatile int EQ_DebugDynamicClarityTransitionCaptureTargetLevel;
extern volatile int EQ_DebugDynamicClarityTransitionCaptureResult;
extern volatile unsigned long
    EQ_DebugDynamicClarityTransitionCaptureRequestFrame;
extern volatile unsigned long
    EQ_DebugDynamicClarityTransitionCaptureTriggerFrame;
extern volatile unsigned long
    EQ_DebugDynamicClarityTransitionCaptureDoneFrame;
#endif
#if EQ_ENABLE_DYNAMIC_CLARITY_TIMING_DIAGNOSTICS != 0
extern volatile unsigned long
    EQ_DebugDynamicClarityTimingFrameCount[DYNAMIC_CLARITY_PATH_COUNT];
extern volatile unsigned long
    EQ_DebugDynamicClarityTimingLastCycles[DYNAMIC_CLARITY_PATH_COUNT];
extern volatile unsigned long
    EQ_DebugDynamicClarityTimingMaxCycles[DYNAMIC_CLARITY_PATH_COUNT];
extern volatile unsigned long
    EQ_DebugDynamicClarityTimingMaxProcessFrame[DYNAMIC_CLARITY_PATH_COUNT];
extern volatile int
    EQ_DebugDynamicClarityTimingMaxActiveLevel[DYNAMIC_CLARITY_PATH_COUNT];
extern volatile int
    EQ_DebugDynamicClarityTimingMaxPendingLevel[DYNAMIC_CLARITY_PATH_COUNT];
extern volatile int
    EQ_DebugDynamicClarityTimingMaxTargetLevel[DYNAMIC_CLARITY_PATH_COUNT];
extern volatile int
    EQ_DebugDynamicClarityTimingMaxTransitionRemaining[DYNAMIC_CLARITY_PATH_COUNT];
extern volatile unsigned long
    EQ_DebugDynamicClarityTimingMaxAnalyzerCount[DYNAMIC_CLARITY_PATH_COUNT];
extern volatile unsigned long
    EQ_DebugDynamicClarityTimingMaxAdFrames[DYNAMIC_CLARITY_PATH_COUNT];
extern volatile unsigned long
    EQ_DebugDynamicClarityTimingMaxDaFrames[DYNAMIC_CLARITY_PATH_COUNT];
extern volatile unsigned int
    EQ_DebugDynamicClarityTimingMaxFlagAd[DYNAMIC_CLARITY_PATH_COUNT];
extern volatile unsigned int
    EQ_DebugDynamicClarityTimingMaxFlagDa[DYNAMIC_CLARITY_PATH_COUNT];
extern volatile unsigned long
    EQ_DebugDynamicClarityTimingMaxDeadlineCount[DYNAMIC_CLARITY_PATH_COUNT];
extern volatile unsigned long
    EQ_DebugDynamicClarityTimingMaxLatencyMissCount[DYNAMIC_CLARITY_PATH_COUNT];
extern volatile unsigned long
    EQ_DebugDynamicClarityTimingMaxOverlapCount[DYNAMIC_CLARITY_PATH_COUNT];
extern volatile unsigned long
    EQ_DebugDynamicClarityTimingMaxDroppedCount[DYNAMIC_CLARITY_PATH_COUNT];
extern volatile unsigned long
    EQ_DebugDynamicClarityTimingMaxUpdateCount[DYNAMIC_CLARITY_PATH_COUNT];
#endif
extern volatile unsigned int EQ_DebugUartFeatureRequest;
extern volatile unsigned long EQ_DebugUartFeatureDeadlineDelta;
extern volatile unsigned long EQ_DebugUartFeatureLatencyMissDelta;
extern volatile unsigned long EQ_DebugUartFeatureOverlapDelta;
extern volatile unsigned long EQ_DebugUartFeatureDroppedDelta;
extern volatile unsigned int EQ_DebugUartFeatureAuditPending;
extern volatile unsigned int EQ_DebugUartFeatureAuditComplete;
extern volatile unsigned int EQ_DebugUartFeatureAudioArrived;
extern volatile unsigned long EQ_DebugUartFeatureBaselineFrame;
extern volatile int EQ_DebugMode;
extern volatile int EQ_DebugDiagPath;
extern volatile int EQ_DebugRequestedMode;
extern volatile int EQ_DebugTransitionTargetMode;
extern volatile int EQ_DebugAppliedMode;
extern volatile unsigned long EQ_DebugModeChangeCount;
extern volatile float EQ_DebugBandGainDb[EQ_NUM_BANDS];
extern EQ_CONTROL_MAILBOX EQ_ControlMailbox;
extern volatile int EQ_DebugControlCommand;
extern volatile int EQ_DebugControlBand;
extern volatile int EQ_DebugControlPreset;
extern volatile float EQ_DebugControlValueDb;
extern volatile float EQ_DebugControlStepDb;
extern volatile float EQ_DebugControlShadowGainDb[EQ_NUM_BANDS];
extern volatile EQ_CONTROL_SEQUENCE EQ_DebugControlRequestToken;
extern volatile EQ_CONTROL_SEQUENCE EQ_DebugControlObservedToken;
extern volatile EQ_CONTROL_SEQUENCE EQ_DebugControlAcceptedToken;
extern volatile EQ_CONTROL_SEQUENCE EQ_DebugControlTargetToken;
extern volatile EQ_CONTROL_SEQUENCE EQ_DebugControlPreparedToken;
extern volatile int EQ_DebugControlReadyValid;
extern volatile EQ_CONTROL_SEQUENCE EQ_DebugControlInstalledToken;
extern volatile EQ_CONTROL_SEQUENCE EQ_DebugControlAppliedToken;
extern volatile int EQ_DebugControlInstalledPairValid;
extern volatile unsigned long EQ_DebugControlRejectedCount;
extern volatile unsigned long EQ_DebugControlCoalescedCount;
extern volatile int EQ_DebugControlLastError;
extern volatile int EQ_DebugBuilderState;
extern volatile int EQ_DebugBuilderSectionIndex;
extern volatile int EQ_DebugBuilderScanIndex;
extern volatile unsigned long EQ_DebugBuilderGeneration;
extern volatile EQ_CONTROL_SEQUENCE EQ_DebugBuilderRequestToken;
extern volatile unsigned long EQ_DebugBuilderSliceCount;
extern volatile unsigned long EQ_DebugBuilderCancelCount;
extern volatile unsigned long EQ_DebugBuilderRestartCount;
extern volatile int EQ_DebugBuilderLastError;
extern volatile unsigned long EQ_DebugBuilderLastCycles;
extern volatile unsigned long EQ_DebugBuilderMaxCycles;
extern volatile float EQ_DebugBuilderLastMs;
extern volatile float EQ_DebugBuilderMaxMs;
extern volatile unsigned long EQ_DebugBuilderDeferredAudioCount;
extern volatile unsigned long EQ_DebugBuilderAudioArrivedDuringSliceCount;
extern volatile unsigned long EQ_DebugResponseActiveGeneration;
extern volatile unsigned long EQ_DebugResponseTargetGeneration;
extern volatile int EQ_DebugResponseTransitionActive;
extern volatile float EQ_DebugResponseTransitionProgress;
extern volatile int EQ_DebugResponseActivePathType;
extern volatile int EQ_DebugResponseTargetValid;
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

void EqualizerBackgroundService_Init(EQ_BACKGROUND_SERVICE_STATE *state);
int EqualizerBackgroundService_IsAudioSafeFinalCheck(
    int final_flag_ad,
    int final_flag_da,
    int final_flag_ad_done,
    int final_frame_service_pending);
int EqualizerAnalyzer_CanService(
    int final_flag_ad,
    int final_flag_da,
    int final_flag_ad_done,
    int final_frame_service_pending,
    int builder_eligible,
    int analyzer_enabled,
    int analyzer_pending);
void EqualizerAnalyzerRuntime_Init(unsigned int *last_enabled);
int EqualizerAnalyzerRuntime_Decide(
    unsigned int *last_enabled,
    unsigned int requested_enabled,
    unsigned int reset_requested,
    int audio_safe,
    int builder_eligible);
void EqualizerUartFeatureAudit_Init(EQ_UART_FEATURE_AUDIT *audit);
int EqualizerUartFeatureAudit_Begin(
    EQ_UART_FEATURE_AUDIT *audit,
    unsigned long process_frames,
    unsigned long deadline,
    unsigned long latency,
    unsigned long overlap,
    unsigned long dropped,
    unsigned int flags_before);
void EqualizerUartFeatureAudit_EndWrite(
    EQ_UART_FEATURE_AUDIT *audit,
    unsigned int flags_after);
int EqualizerUartFeatureAudit_CompleteAfterFrame(
    EQ_UART_FEATURE_AUDIT *audit,
    unsigned long process_frames,
    unsigned long deadline,
    unsigned long latency,
    unsigned long overlap,
    unsigned long dropped);
int EqualizerBackgroundService_Decide(
    const EQ_BACKGROUND_SERVICE_STATE *state,
    unsigned long processed_frame,
    int final_flag_ad,
    int final_flag_da,
    int final_flag_ad_done,
    int final_frame_service_pending,
    int builder_eligible,
    int analyzer_eligible,
    int uart_eligible,
    int lcd_eligible);
void EqualizerBackgroundService_Record(
    EQ_BACKGROUND_SERVICE_STATE *state,
    unsigned long processed_frame,
    int completed_kind);

void Equalizer_Flow_Example(void);

#endif /* _USER_EQUALIZER_FLOW_H_ */
