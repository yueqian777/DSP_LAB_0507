/**
 * user_equalizer_flow.c
 *
 * Project 3.3 board-side AD/DA equalizer example. CH1 is processed by
 * the equalizer; CH2-CH8 are passed through in the first version.
 */

#include "user_equalizer_flow.h"
#include "user_equalizer_display.h"
#include "equalizer_build_id.h"
#include "string.h"

#ifndef EQ_ALGO_ONLY

#include "dac_api.h"
#include "key_api.h"
#include "system.h"

#if defined(__TI_COMPILER_VERSION__) || defined(__TMS320C6X__)
#include "c6x.h"
#endif

#define EQ_CPU_CYCLES_PER_MS 456000.0f
static short EQ_AD_Buffer1[ADC_SAMPLE_1024];
static short EQ_AD_Buffer2[ADC_SAMPLE_1024];
static short EQ_AD_Buffer3[ADC_SAMPLE_1024];
static short EQ_AD_Buffer4[ADC_SAMPLE_1024];
static short EQ_AD_Buffer5[ADC_SAMPLE_1024];
static short EQ_AD_Buffer6[ADC_SAMPLE_1024];
static short EQ_AD_Buffer7[ADC_SAMPLE_1024];
static short EQ_AD_Buffer8[ADC_SAMPLE_1024];
static short EQ_DA_Buffer1[DAC_SAMPLE_1024];
static EQ_STATE EQ_BoardState;
static int EQ_LastServicedMode = -1;
static int EQ_AppliedDiagPath = -1;
static unsigned int EQ_FrameServiceStartCycle = 0U;
static unsigned int EQ_FrameActiveSegmentStartCycle = 0U;
static unsigned long EQ_FrameActiveServiceCycles = 0UL;
static unsigned char EQ_FrameServicePending = 0U;

#endif

volatile unsigned long EQ_DebugAdFrames = 0UL;
volatile unsigned long EQ_DebugDaFrames = 0UL;
volatile unsigned long EQ_DebugProcessFrames = 0UL;
volatile unsigned long EQ_DebugAlgoLastCycles = 0UL;
volatile unsigned long EQ_DebugAlgoMaxCycles = 0UL;
volatile float EQ_DebugAlgoLastMs = 0.0f;
volatile float EQ_DebugAlgoMaxMs = 0.0f;
volatile unsigned long EQ_DebugModeServiceLastCycles = 0UL;
volatile unsigned long EQ_DebugModeServiceMaxCycles = 0UL;
volatile unsigned long EQ_DebugFrameServiceLastCycles = 0UL;
volatile unsigned long EQ_DebugFrameServiceMaxCycles = 0UL;
volatile float EQ_DebugFrameServiceLastMs = 0.0f;
volatile float EQ_DebugFrameServiceMaxMs = 0.0f;
volatile unsigned long EQ_DebugFrameLatencyLastCycles = 0UL;
volatile unsigned long EQ_DebugFrameLatencyMaxCycles = 0UL;
volatile float EQ_DebugFrameLatencyLastMs = 0.0f;
volatile float EQ_DebugFrameLatencyMaxMs = 0.0f;
volatile unsigned long EQ_DebugDeadlineMissCount = 0UL;
volatile unsigned long EQ_DebugFrameLatencyDeadlineMissCount = 0UL;
volatile unsigned long EQ_DebugFrameServiceOverlapCount = 0UL;
volatile unsigned long EQ_DebugFrameServiceDroppedCount = 0UL;
volatile int EQ_DebugMode = EQ_PRESET_FLAT;
volatile int EQ_DebugDiagPath = EQ_DIAG_PRESET;
volatile int EQ_DebugRequestedMode = EQ_PRESET_FLAT;
volatile int EQ_DebugTransitionTargetMode = EQ_PRESET_NONE;
volatile int EQ_DebugAppliedMode = EQ_PRESET_FLAT;
volatile unsigned long EQ_DebugModeChangeCount = 0UL;
volatile float EQ_DebugBandGainDb[EQ_NUM_BANDS] =
{
    0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 0.0f, 0.0f
};
volatile const unsigned long EQ_DebugBuildMagic = 0x33030003UL;
volatile const char EQ_DebugBuildId[] = EQ_BUILD_VERSION;
volatile const char EQ_DebugBuildVersion[] = EQ_BUILD_VERSION;
volatile const char EQ_DebugBuildGitSha[] = EQ_BUILD_GIT_SHA;
volatile const char EQ_DebugBuildTimestamp[] = EQ_BUILD_TIMESTAMP;
volatile const int EQ_DebugBuildDirty = EQ_BUILD_DIRTY;

#if defined(__TI_COMPILER_VERSION__) || defined(__TMS320C6X__)
#pragma DATA_SECTION(EQ_CaptureInput, ".far")
#pragma DATA_SECTION(EQ_CaptureOutput, ".far")
#pragma DATA_SECTION(EQ_TriggerCaptureInput, ".far")
#pragma DATA_SECTION(EQ_TriggerCaptureOutput, ".far")
#endif
short EQ_CaptureInput[EQ_CAPTURE_SAMPLES];
short EQ_CaptureOutput[EQ_CAPTURE_SAMPLES];
short EQ_TriggerCaptureInput[EQ_TRIGGER_CAPTURE_SAMPLES];
short EQ_TriggerCaptureOutput[EQ_TRIGGER_CAPTURE_SAMPLES];

volatile unsigned int EQ_CaptureManualRequest = 0U;
volatile unsigned int EQ_CaptureManualActive = 0U;
volatile unsigned int EQ_CaptureManualReady = 0U;
volatile unsigned int EQ_CaptureManualIndex = 0U;
volatile unsigned int EQ_CaptureManualFrameCount = 0U;
volatile unsigned int EQ_TriggerCaptureRequest = 0U;
volatile unsigned int EQ_TriggerCaptureActive = 0U;
volatile unsigned int EQ_TriggerCaptureTriggered = 0U;
volatile unsigned int EQ_TriggerCaptureReady = 0U;
volatile unsigned int EQ_TriggerCaptureArmedSource = 0U;
volatile unsigned int EQ_TriggerCaptureSource = 0U;
volatile unsigned int EQ_TriggerCapturePrefill = 0U;
volatile unsigned int EQ_TriggerCaptureArmedReady = 0U;
volatile unsigned int EQ_TriggerCapturePreWriteIndex = 0U;
volatile unsigned int EQ_TriggerCapturePostCount = 0U;

static void EQ_CaptureCopyFrame(short *destination,
                                unsigned int frame_index,
                                const short *source)
{
    memcpy(destination + ((unsigned long)frame_index * EQ_FRAME_LEN),
           source, sizeof(short) * EQ_FRAME_LEN);
}

void EqualizerCapture_AcknowledgeManual(void)
{
    if (EQ_CaptureManualActive == 0U)
    {
        EQ_CaptureManualReady = 0U;
        EQ_CaptureManualRequest = 0U;
        EQ_CaptureManualIndex = 0U;
        EQ_CaptureManualFrameCount = 0U;
        EQ_TriggerCaptureRequest = 0U;
    }
}

void EqualizerCapture_AcknowledgeTrigger(void)
{
    if (EQ_TriggerCaptureActive == 0U)
    {
        EQ_CaptureManualRequest = 0U;
        EQ_TriggerCaptureRequest = 0U;
        EQ_TriggerCaptureTriggered = 0U;
        EQ_TriggerCaptureReady = 0U;
        EQ_TriggerCaptureArmedSource = 0U;
        EQ_TriggerCaptureSource = 0U;
        EQ_TriggerCapturePrefill = 0U;
        EQ_TriggerCaptureArmedReady = 0U;
        EQ_TriggerCapturePreWriteIndex = 0U;
        EQ_TriggerCapturePostCount = 0U;
    }
}

void EqualizerCapture_Reset(void)
{
    EQ_CaptureManualRequest = 0U;
    EQ_CaptureManualActive = 0U;
    EQ_CaptureManualReady = 0U;
    EQ_CaptureManualIndex = 0U;
    EQ_CaptureManualFrameCount = 0U;
    EQ_TriggerCaptureRequest = 0U;
    EQ_TriggerCaptureActive = 0U;
    EQ_TriggerCaptureTriggered = 0U;
    EQ_TriggerCaptureReady = 0U;
    EQ_TriggerCaptureArmedSource = 0U;
    EQ_TriggerCaptureSource = 0U;
    EQ_TriggerCapturePrefill = 0U;
    EQ_TriggerCaptureArmedReady = 0U;
    EQ_TriggerCapturePreWriteIndex = 0U;
    EQ_TriggerCapturePostCount = 0U;
}

static int EQ_CaptureSourceValid(unsigned int source)
{
    return (source == EQ_CAPTURE_TRIGGER_LCD_JOB) ||
           (source == EQ_CAPTURE_TRIGGER_MODE_SWITCH) ||
           (source == EQ_CAPTURE_TRIGGER_AUDIO_DURING_LCD);
}

static void EQ_CaptureAcceptRequest(void)
{
    unsigned int manual_request;
    unsigned int trigger_request;

    manual_request = EQ_CaptureManualRequest;
    trigger_request = EQ_TriggerCaptureRequest;
    if ((EQ_CaptureManualActive != 0U) ||
        (EQ_CaptureManualReady != 0U) ||
        (EQ_TriggerCaptureActive != 0U) ||
        (EQ_TriggerCaptureReady != 0U))
    {
        EQ_CaptureManualRequest = 0U;
        EQ_TriggerCaptureRequest = 0U;
        return;
    }
    if (manual_request != 0U)
    {
        EQ_CaptureManualActive = 1U;
        EQ_CaptureManualRequest = 0U;
        EQ_CaptureManualIndex = 0U;
        EQ_CaptureManualFrameCount = 0U;
        EQ_TriggerCaptureRequest = 0U;
        return;
    }
    if (EQ_CaptureSourceValid(trigger_request))
    {
        EQ_TriggerCaptureActive = 1U;
        EQ_TriggerCaptureTriggered = 0U;
        EQ_TriggerCaptureArmedSource = trigger_request;
        EQ_TriggerCaptureRequest = 0U;
        EQ_TriggerCaptureSource = 0U;
        EQ_TriggerCapturePrefill = 0U;
        EQ_TriggerCaptureArmedReady = 0U;
        EQ_TriggerCapturePreWriteIndex = 0U;
        EQ_TriggerCapturePostCount = 0U;
    }
}

void EqualizerCapture_OnFrame(const short *input, const short *output)
{
    unsigned int slot;

    if ((input == 0) || (output == 0))
    {
        return;
    }
    EQ_CaptureAcceptRequest();
    if (EQ_CaptureManualActive != 0U)
    {
        slot = EQ_CaptureManualFrameCount;
        EQ_CaptureCopyFrame(EQ_CaptureInput, slot, input);
        EQ_CaptureCopyFrame(EQ_CaptureOutput, slot, output);
        EQ_CaptureManualFrameCount++;
        EQ_CaptureManualIndex += EQ_FRAME_LEN;
        if (EQ_CaptureManualFrameCount >= EQ_CAPTURE_FRAMES)
        {
            EQ_CaptureManualActive = 0U;
            EQ_CaptureManualReady = 1U;
            EQ_CaptureManualRequest = 0U;
        }
        return;
    }
    if (EQ_TriggerCaptureActive == 0U)
    {
        return;
    }
    if (EQ_TriggerCaptureTriggered == 0U)
    {
        slot = EQ_TriggerCapturePreWriteIndex;
        EQ_CaptureCopyFrame(EQ_TriggerCaptureInput, slot, input);
        EQ_CaptureCopyFrame(EQ_TriggerCaptureOutput, slot, output);
        EQ_TriggerCapturePreWriteIndex =
            (slot + 1U) % EQ_TRIGGER_PRE_FRAMES;
        if (EQ_TriggerCapturePrefill < EQ_TRIGGER_PRE_FRAMES)
        {
            EQ_TriggerCapturePrefill++;
            if (EQ_TriggerCapturePrefill == EQ_TRIGGER_PRE_FRAMES)
            {
                EQ_TriggerCaptureArmedReady = 1U;
            }
        }
        return;
    }

    slot = EQ_TRIGGER_PRE_FRAMES + EQ_TriggerCapturePostCount;
    EQ_CaptureCopyFrame(EQ_TriggerCaptureInput, slot, input);
    EQ_CaptureCopyFrame(EQ_TriggerCaptureOutput, slot, output);
    EQ_TriggerCapturePostCount++;
    if (EQ_TriggerCapturePostCount >= EQ_TRIGGER_POST_FRAMES)
    {
        EQ_TriggerCaptureActive = 0U;
        EQ_TriggerCaptureReady = 1U;
        EQ_TriggerCaptureRequest = 0U;
    }
}

void EqualizerCapture_NotifyEvent(unsigned int source)
{
    if ((EQ_TriggerCaptureActive == 0U) ||
        (EQ_TriggerCaptureTriggered != 0U) ||
        (EQ_TriggerCaptureReady != 0U) ||
        (EQ_TriggerCaptureArmedReady == 0U) ||
        (source != EQ_TriggerCaptureArmedSource))
    {
        return;
    }
    EQ_TriggerCaptureTriggered = 1U;
    EQ_TriggerCaptureSource = source;
    EQ_TriggerCapturePostCount = 0U;
}

void EqualizerCapture_NotifyModeChange(unsigned long before_count,
                                       unsigned long after_count)
{
    if (after_count != before_count)
    {
        EqualizerCapture_NotifyEvent(EQ_CAPTURE_TRIGGER_MODE_SWITCH);
    }
}

void EqualizerLcdPolicy_Init(EQ_LCD_SERVICE_POLICY *policy)
{
    policy->last_service_frame = 0UL;
    policy->last_deferred_frame = ~0UL;
    policy->last_status_request_frame = 0UL;
}

int EqualizerLcdPolicy_CanService(const EQ_LCD_SERVICE_POLICY *policy,
                                  unsigned long process_frames,
                                  int flag_ad,
                                  int flag_da,
                                  int flag_ad_done,
                                  int frame_service_pending,
                                  int has_pending_job)
{
    return (flag_ad == 0) &&
           (flag_da == 0) &&
           (flag_ad_done == 0) &&
           (frame_service_pending == 0) &&
           (process_frames != policy->last_service_frame) &&
           (has_pending_job != 0);
}

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
                              int has_pending_job)
{
    if (!EqualizerLcdPolicy_CanService(
            policy, process_frames,
            outer_flag_ad, outer_flag_da, outer_flag_ad_done,
            outer_frame_service_pending, has_pending_job))
    {
        if ((has_pending_job != 0) &&
            ((outer_flag_ad != 0) || (outer_flag_da != 0) ||
             (outer_flag_ad_done != 0) ||
             (outer_frame_service_pending != 0)))
        {
            return EQ_LCD_POLICY_DEFER;
        }
        return EQ_LCD_POLICY_NONE;
    }
    if (!EqualizerLcdPolicy_CanService(
            policy, process_frames,
            predraw_flag_ad, predraw_flag_da, predraw_flag_ad_done,
            predraw_frame_service_pending, has_pending_job))
    {
        return EQ_LCD_POLICY_DEFER;
    }
    return EQ_LCD_POLICY_SERVICE;
}

void EqualizerLcdPolicy_RecordService(EQ_LCD_SERVICE_POLICY *policy,
                                      unsigned long process_frames,
                                      int completed_job)
{
    if (completed_job != 0)
    {
        policy->last_service_frame = process_frames;
    }
}

int EqualizerLcdPolicy_RecordDeferred(EQ_LCD_SERVICE_POLICY *policy,
                                      unsigned long process_frames)
{
    if (process_frames == policy->last_deferred_frame)
    {
        return 0;
    }
    policy->last_deferred_frame = process_frames;
    return 1;
}

int EqualizerLcdPolicy_ShouldRequestStatus(EQ_LCD_SERVICE_POLICY *policy,
                                           unsigned long process_frames)
{
    if ((process_frames - policy->last_status_request_frame) <
        EQ_LCD_REFRESH_FRAMES)
    {
        return 0;
    }
    policy->last_status_request_frame = process_frames;
    return 1;
}

void EqualizerLcdFaultPolicy_Init(EQ_LCD_FAULT_POLICY *policy)
{
    policy->previous_latency_misses = 0UL;
    policy->previous_overlaps = 0UL;
    policy->previous_dropped = 0UL;
}

unsigned long EqualizerLcdFaultPolicy_Monitor(
    EQ_LCD_FAULT_POLICY *policy,
    int runtime_enabled,
    unsigned long latency_misses,
    unsigned long overlaps,
    unsigned long dropped)
{
    unsigned long reason = 0UL;

    if (runtime_enabled != 0)
    {
        if (latency_misses != policy->previous_latency_misses)
        {
            reason |= EQ_LCD_FAULT_LATENCY_MISS;
        }
        if (overlaps != policy->previous_overlaps)
        {
            reason |= EQ_LCD_FAULT_OVERLAP;
        }
        if (dropped != policy->previous_dropped)
        {
            reason |= EQ_LCD_FAULT_DROPPED;
        }
    }
    policy->previous_latency_misses = latency_misses;
    policy->previous_overlaps = overlaps;
    policy->previous_dropped = dropped;
    return reason;
}

#ifndef EQ_ALGO_ONLY

static int EQ_NormalizeMode(int mode)
{
    if ((mode < EQ_PRESET_FLAT) || (mode > EQ_PRESET_V_SHAPE))
    {
        return EQ_PRESET_FLAT;
    }
    return mode;
}

static int EQ_NormalizeDiagPath(int path)
{
    if ((path < EQ_DIAG_RAW_COPY) || (path > EQ_DIAG_PRESET))
    {
        return EQ_DIAG_PRESET;
    }
    return path;
}

static void EQ_KeepBuildFingerprint(void)
{
    volatile const unsigned long *magic = &EQ_DebugBuildMagic;
    volatile const char *build_id = EQ_DebugBuildId;
    volatile const char *build_version = EQ_DebugBuildVersion;
    volatile const char *build_git_sha = EQ_DebugBuildGitSha;
    volatile const char *build_timestamp = EQ_DebugBuildTimestamp;
    volatile const int *dirty = &EQ_DebugBuildDirty;

    (void)*magic;
    (void)*build_id;
    (void)*build_version;
    (void)*build_git_sha;
    (void)*build_timestamp;
    (void)*dirty;
}

static void EQ_UpdateDebugGains(void)
{
    int band;

    for (band = 0; band < EQ_NUM_BANDS; band++)
    {
        EQ_DebugBandGainDb[band] =
            Equalizer_GetBandTargetGainDb(&EQ_BoardState, band);
    }
    EQ_DebugClipCount = EQ_BoardState.clip_count;
    EQ_DebugRequestedMode =
        ((EQ_AppliedDiagPath == EQ_DIAG_PRESET) ||
         (EQ_AppliedDiagPath == EQ_DIAG_FLAT)) ?
        Equalizer_GetRequestedPreset(&EQ_BoardState) : EQ_PRESET_NONE;
    EQ_DebugTransitionTargetMode =
        Equalizer_GetTransitionTargetPreset(&EQ_BoardState);
    EQ_DebugAppliedMode = Equalizer_GetAppliedPreset(&EQ_BoardState);
    EQ_DebugModeChangeCount = Equalizer_GetModeChangeCount(&EQ_BoardState);
}

static void EQ_ServiceMode(void)
{
    int diag_path;
    int mode;

    diag_path = EQ_NormalizeDiagPath(EQ_DebugDiagPath);
    if (EQ_DebugDiagPath != diag_path)
    {
        EQ_DebugDiagPath = diag_path;
    }
    mode = EQ_NormalizeMode(EQ_DebugMode);
    if (EQ_DebugMode != mode)
    {
        EQ_DebugMode = mode;
    }
    if (diag_path != EQ_AppliedDiagPath)
    {
        Equalizer_SetBypass(&EQ_BoardState, 0);
        if (diag_path == EQ_DIAG_RAW_COPY)
        {
            Equalizer_SetCoreMode(&EQ_BoardState, EQ_CORE_RAW_COPY);
            EQ_LastServicedMode = EQ_PRESET_NONE;
        }
        else if (diag_path == EQ_DIAG_FLOAT_COPY)
        {
            Equalizer_SetCoreMode(&EQ_BoardState, EQ_CORE_FLOAT_COPY);
            EQ_LastServicedMode = EQ_PRESET_NONE;
        }
        else if (diag_path == EQ_DIAG_FLAT)
        {
            Equalizer_SetCoreMode(&EQ_BoardState, EQ_CORE_RBJ_CASCADE);
            Equalizer_ApplyPreset(&EQ_BoardState, EQ_PRESET_FLAT);
            EQ_LastServicedMode = EQ_PRESET_FLAT;
        }
        else if (diag_path == EQ_DIAG_SINGLE_BAND)
        {
            Equalizer_SetCoreMode(&EQ_BoardState, EQ_CORE_RBJ_CASCADE);
            Equalizer_ApplySingleBand1kPlus3Db(&EQ_BoardState);
            EQ_LastServicedMode = EQ_PRESET_NONE;
        }
        else
        {
            Equalizer_SetCoreMode(&EQ_BoardState, EQ_CORE_RBJ_CASCADE);
            Equalizer_ApplyPreset(&EQ_BoardState, mode);
            EQ_LastServicedMode = mode;
        }
        EQ_AppliedDiagPath = diag_path;
    }
    else if ((diag_path == EQ_DIAG_PRESET) &&
             (mode != EQ_LastServicedMode))
    {
        Equalizer_SetCoreMode(&EQ_BoardState, EQ_CORE_RBJ_CASCADE);
        Equalizer_ApplyPreset(&EQ_BoardState, mode);
        EQ_LastServicedMode = mode;
    }
    EQ_UpdateDebugGains();
}

static void EQ_UpdateModeServiceTiming(unsigned long cycles)
{
    EQ_DebugModeServiceLastCycles = cycles;
    if (cycles > EQ_DebugModeServiceMaxCycles)
    {
        EQ_DebugModeServiceMaxCycles = cycles;
    }
}

static void EQ_ServiceModeTimed(void)
{
#if defined(__TI_COMPILER_VERSION__) || defined(__TMS320C6X__)
    unsigned int cycle_start = TSCL;

    EQ_ServiceMode();
    EQ_UpdateModeServiceTiming((unsigned long)(TSCL - cycle_start));
#else
    EQ_ServiceMode();
    EQ_UpdateModeServiceTiming(0UL);
#endif
}

static void EQ_UpdateAlgoTiming(unsigned long cycles)
{
    EQ_DebugAlgoLastCycles = cycles;
    EQ_DebugAlgoLastMs = (float)cycles / EQ_CPU_CYCLES_PER_MS;
    if (cycles > EQ_DebugAlgoMaxCycles)
    {
        EQ_DebugAlgoMaxCycles = cycles;
        EQ_DebugAlgoMaxMs = EQ_DebugAlgoLastMs;
    }
}

static void EQ_BeginFrameService(void)
{
    if (EQ_FrameServicePending != 0U)
    {
        EQ_DebugFrameServiceOverlapCount++;
        EQ_DebugFrameServiceDroppedCount++;
    }
#if defined(__TI_COMPILER_VERSION__) || defined(__TMS320C6X__)
    EQ_FrameServiceStartCycle = TSCL;
#else
    EQ_FrameServiceStartCycle = 0U;
#endif
    EQ_FrameActiveServiceCycles = 0UL;
    EQ_FrameServicePending = 1U;
}

static void EQ_BeginFrameActiveSegment(void)
{
#if defined(__TI_COMPILER_VERSION__) || defined(__TMS320C6X__)
    EQ_FrameActiveSegmentStartCycle = TSCL;
#else
    EQ_FrameActiveSegmentStartCycle = 0U;
#endif
}

static void EQ_EndFrameActiveSegment(void)
{
#if defined(__TI_COMPILER_VERSION__) || defined(__TMS320C6X__)
    EQ_FrameActiveServiceCycles +=
        (unsigned long)(TSCL - EQ_FrameActiveSegmentStartCycle);
#endif
}

static void EQ_EndFrameService(void)
{
    unsigned long latency_cycles;

    if (EQ_FrameServicePending == 0U)
    {
        return;
    }
#if defined(__TI_COMPILER_VERSION__) || defined(__TMS320C6X__)
    latency_cycles = (unsigned long)(TSCL - EQ_FrameServiceStartCycle);
#else
    latency_cycles = 0UL;
#endif
    EQ_DebugFrameServiceLastCycles = EQ_FrameActiveServiceCycles;
    EQ_DebugFrameServiceLastMs =
        (float)EQ_FrameActiveServiceCycles / EQ_CPU_CYCLES_PER_MS;
    if (EQ_FrameActiveServiceCycles > EQ_DebugFrameServiceMaxCycles)
    {
        EQ_DebugFrameServiceMaxCycles = EQ_FrameActiveServiceCycles;
        EQ_DebugFrameServiceMaxMs = EQ_DebugFrameServiceLastMs;
    }
    EQ_DebugFrameLatencyLastCycles = latency_cycles;
    EQ_DebugFrameLatencyLastMs =
        (float)latency_cycles / EQ_CPU_CYCLES_PER_MS;
    if (latency_cycles > EQ_DebugFrameLatencyMaxCycles)
    {
        EQ_DebugFrameLatencyMaxCycles = latency_cycles;
        EQ_DebugFrameLatencyMaxMs = EQ_DebugFrameLatencyLastMs;
    }
    if (EQ_FrameActiveServiceCycles > EQ_FRAME_SERVICE_BUDGET_CYCLES)
    {
        EQ_DebugDeadlineMissCount++;
    }
    if (latency_cycles > EQ_FRAME_SERVICE_BUDGET_CYCLES)
    {
        EQ_DebugFrameLatencyDeadlineMissCount++;
    }
    EQ_FrameServicePending = 0U;
}

static void EQ_CaptureAdcFrame(void)
{
    unsigned long mode_change_before;
    unsigned long mode_change_after;
#if defined(__TI_COMPILER_VERSION__) || defined(__TMS320C6X__)
    unsigned int cycle_start;
    unsigned int cycle_end;
    unsigned long cycle_delta;
#endif

    if (AD_Ping_Pong == AD_BUFFER_PONG)
    {
        memcpy(EQ_AD_Buffer1, AD_CH1_Buf0, 2 * ADC_SAMPLE_1024);
        memcpy(EQ_AD_Buffer2, AD_CH2_Buf0, 2 * ADC_SAMPLE_1024);
        memcpy(EQ_AD_Buffer3, AD_CH3_Buf0, 2 * ADC_SAMPLE_1024);
        memcpy(EQ_AD_Buffer4, AD_CH4_Buf0, 2 * ADC_SAMPLE_1024);
        memcpy(EQ_AD_Buffer5, AD_CH5_Buf0, 2 * ADC_SAMPLE_1024);
        memcpy(EQ_AD_Buffer6, AD_CH6_Buf0, 2 * ADC_SAMPLE_1024);
        memcpy(EQ_AD_Buffer7, AD_CH7_Buf0, 2 * ADC_SAMPLE_1024);
        memcpy(EQ_AD_Buffer8, AD_CH8_Buf0, 2 * ADC_SAMPLE_1024);
    }
    else
    {
        memcpy(EQ_AD_Buffer1, AD_CH1_Buf1, 2 * ADC_SAMPLE_1024);
        memcpy(EQ_AD_Buffer2, AD_CH2_Buf1, 2 * ADC_SAMPLE_1024);
        memcpy(EQ_AD_Buffer3, AD_CH3_Buf1, 2 * ADC_SAMPLE_1024);
        memcpy(EQ_AD_Buffer4, AD_CH4_Buf1, 2 * ADC_SAMPLE_1024);
        memcpy(EQ_AD_Buffer5, AD_CH5_Buf1, 2 * ADC_SAMPLE_1024);
        memcpy(EQ_AD_Buffer6, AD_CH6_Buf1, 2 * ADC_SAMPLE_1024);
        memcpy(EQ_AD_Buffer7, AD_CH7_Buf1, 2 * ADC_SAMPLE_1024);
        memcpy(EQ_AD_Buffer8, AD_CH8_Buf1, 2 * ADC_SAMPLE_1024);
    }

#if defined(__TI_COMPILER_VERSION__) || defined(__TMS320C6X__)
    cycle_start = TSCL;
#endif
    mode_change_before = Equalizer_GetModeChangeCount(&EQ_BoardState);
    Equalizer_ProcessFrame(&EQ_BoardState, EQ_AD_Buffer1, EQ_DA_Buffer1,
                           ADC_SAMPLE_1024);
    mode_change_after = Equalizer_GetModeChangeCount(&EQ_BoardState);
    EQ_DebugProcessFrames++;
#if defined(__TI_COMPILER_VERSION__) || defined(__TMS320C6X__)
    cycle_end = TSCL;
    cycle_delta = (unsigned long)(cycle_end - cycle_start);
    EQ_UpdateAlgoTiming(cycle_delta);
#else
    EQ_UpdateAlgoTiming(0UL);
#endif
    EqualizerCapture_NotifyModeChange(mode_change_before, mode_change_after);
    EqualizerCapture_OnFrame(EQ_AD_Buffer1, EQ_DA_Buffer1);
}

static void EQ_FillDacPingBuffer(void)
{
    memcpy(DA_CH1_Buf0, EQ_DA_Buffer1, 2 * DAC_SAMPLE_1024);
    memcpy(DA_CH2_Buf0, EQ_AD_Buffer2, 2 * DAC_SAMPLE_1024);
    memcpy(DA_CH3_Buf0, EQ_AD_Buffer3, 2 * DAC_SAMPLE_1024);
    memcpy(DA_CH4_Buf0, EQ_AD_Buffer4, 2 * DAC_SAMPLE_1024);
    memcpy(DA_CH5_Buf0, EQ_AD_Buffer5, 2 * DAC_SAMPLE_1024);
    memcpy(DA_CH6_Buf0, EQ_AD_Buffer6, 2 * DAC_SAMPLE_1024);
    memcpy(DA_CH7_Buf0, EQ_AD_Buffer7, 2 * DAC_SAMPLE_1024);
    memcpy(DA_CH8_Buf0, EQ_AD_Buffer8, 2 * DAC_SAMPLE_1024);
}

static void EQ_FillDacPongBuffer(void)
{
    memcpy(DA_CH1_Buf1, EQ_DA_Buffer1, 2 * DAC_SAMPLE_1024);
    memcpy(DA_CH2_Buf1, EQ_AD_Buffer2, 2 * DAC_SAMPLE_1024);
    memcpy(DA_CH3_Buf1, EQ_AD_Buffer3, 2 * DAC_SAMPLE_1024);
    memcpy(DA_CH4_Buf1, EQ_AD_Buffer4, 2 * DAC_SAMPLE_1024);
    memcpy(DA_CH5_Buf1, EQ_AD_Buffer5, 2 * DAC_SAMPLE_1024);
    memcpy(DA_CH6_Buf1, EQ_AD_Buffer6, 2 * DAC_SAMPLE_1024);
    memcpy(DA_CH7_Buf1, EQ_AD_Buffer7, 2 * DAC_SAMPLE_1024);
    memcpy(DA_CH8_Buf1, EQ_AD_Buffer8, 2 * DAC_SAMPLE_1024);
}

static void EQ_ClearDacBuffers(void)
{
    memset(EQ_DA_Buffer1, 0, sizeof(EQ_DA_Buffer1));
    memset(DA_CH1_Buf0, 0, 2 * DAC_SAMPLE_1024);
    memset(DA_CH2_Buf0, 0, 2 * DAC_SAMPLE_1024);
    memset(DA_CH3_Buf0, 0, 2 * DAC_SAMPLE_1024);
    memset(DA_CH4_Buf0, 0, 2 * DAC_SAMPLE_1024);
    memset(DA_CH5_Buf0, 0, 2 * DAC_SAMPLE_1024);
    memset(DA_CH6_Buf0, 0, 2 * DAC_SAMPLE_1024);
    memset(DA_CH7_Buf0, 0, 2 * DAC_SAMPLE_1024);
    memset(DA_CH8_Buf0, 0, 2 * DAC_SAMPLE_1024);
    memset(DA_CH1_Buf1, 0, 2 * DAC_SAMPLE_1024);
    memset(DA_CH2_Buf1, 0, 2 * DAC_SAMPLE_1024);
    memset(DA_CH3_Buf1, 0, 2 * DAC_SAMPLE_1024);
    memset(DA_CH4_Buf1, 0, 2 * DAC_SAMPLE_1024);
    memset(DA_CH5_Buf1, 0, 2 * DAC_SAMPLE_1024);
    memset(DA_CH6_Buf1, 0, 2 * DAC_SAMPLE_1024);
    memset(DA_CH7_Buf1, 0, 2 * DAC_SAMPLE_1024);
    memset(DA_CH8_Buf1, 0, 2 * DAC_SAMPLE_1024);
}

static void EQ_FillDacInactiveBuffer(void)
{
    if (DA_Ping_Pong == DA_BUFFER_PONG)
    {
        EQ_FillDacPingBuffer();
    }
    else
    {
        EQ_FillDacPongBuffer();
    }
}

void Equalizer_Flow_Example(void)
{
    unsigned char flag_ad_done;
#if EQ_ENABLE_LCD_DISPLAY != 0
    EQ_LCD_SERVICE_POLICY lcd_policy;
    EQ_LCD_FAULT_POLICY lcd_fault_policy;
    unsigned long lcd_disable_reason;
    unsigned int lcd_audio_before;
    unsigned int lcd_audio_after;
    int lcd_requested_mode;
    int lcd_transition_target_mode;
    int lcd_applied_mode;
    int lcd_job;
    int lcd_policy_decision;
#endif

    flag_ad_done = 0;
#if EQ_ENABLE_LCD_DISPLAY != 0
    EqualizerLcdPolicy_Init(&lcd_policy);
    EqualizerLcdFaultPolicy_Init(&lcd_fault_policy);
    lcd_requested_mode = EQ_PRESET_NONE;
    lcd_transition_target_mode = EQ_PRESET_NONE;
    lcd_applied_mode = EQ_PRESET_NONE;
#endif
    Sys_Init();
    Key_Init();
    EQ_KeepBuildFingerprint();

    Adc_Init(ADC_50KHZ, ADC_SAMPLE_1024);
    Dac_Init(DAC_50KHZ, DAC_SAMPLE_1024, DAC_CHANNEL_ALL);
    Equalizer_Init(&EQ_BoardState);
    EQ_ServiceMode();
    EQ_ClearDacBuffers();
#if EQ_ENABLE_LCD_DISPLAY != 0
    EqualizerDisplay_Init();
    EqualizerDisplay_DrawStaticLayout();
    EqualizerDisplay_BeginRuntime();
    EqualizerDisplay_RequestGains(&EQ_BoardState);
    EqualizerDisplay_RequestStatus(
        EQ_DebugProcessFrames,
        EQ_DebugAlgoLastMs,
        EQ_DebugAlgoMaxMs,
        EQ_DebugClipCount,
        EQ_DebugRequestedMode,
        EQ_DebugTransitionTargetMode,
        EQ_DebugAppliedMode);
    lcd_requested_mode = EQ_DebugRequestedMode;
    lcd_transition_target_mode = EQ_DebugTransitionTargetMode;
    lcd_applied_mode = EQ_DebugAppliedMode;
#endif

#if defined(__TI_COMPILER_VERSION__) || defined(__TMS320C6X__)
    TSCL = 0;
#endif

    Adc_Start();
    Dac_Start();

    while (1)
    {
        if (FLAG_AD == 1)
        {
            EQ_BeginFrameService();
            EQ_BeginFrameActiveSegment();
            FLAG_AD = 0;
            flag_ad_done = 1;
            EQ_DebugAdFrames++;
            EQ_CaptureAdcFrame();
            EQ_EndFrameActiveSegment();
        }

        if ((FLAG_DA == 1) && (flag_ad_done == 1))
        {
            EQ_BeginFrameActiveSegment();
            FLAG_DA = 0;
            flag_ad_done = 0;
            EQ_DebugDaFrames++;
            EQ_FillDacInactiveBuffer();
            EQ_EndFrameActiveSegment();
            EQ_EndFrameService();
        }

        if ((FLAG_AD == 0) && (FLAG_DA == 0) && (flag_ad_done == 0))
        {
            EQ_ServiceModeTimed();

#if EQ_ENABLE_LCD_DISPLAY != 0
            if ((EQ_DebugRequestedMode != lcd_requested_mode) ||
                (EQ_DebugTransitionTargetMode != lcd_transition_target_mode) ||
                (EQ_DebugAppliedMode != lcd_applied_mode))
            {
                lcd_requested_mode = EQ_DebugRequestedMode;
                lcd_transition_target_mode = EQ_DebugTransitionTargetMode;
                lcd_applied_mode = EQ_DebugAppliedMode;
                EqualizerDisplay_RequestGains(&EQ_BoardState);
                EqualizerDisplay_RequestStatus(
                    EQ_DebugProcessFrames,
                    EQ_DebugAlgoLastMs,
                    EQ_DebugAlgoMaxMs,
                    EQ_DebugClipCount,
                    EQ_DebugRequestedMode,
                    EQ_DebugTransitionTargetMode,
                    EQ_DebugAppliedMode);
            }
            if (EqualizerLcdPolicy_ShouldRequestStatus(
                    &lcd_policy, EQ_DebugProcessFrames))
            {
                EqualizerDisplay_RequestStatus(
                    EQ_DebugProcessFrames,
                    EQ_DebugAlgoLastMs,
                    EQ_DebugAlgoMaxMs,
                    EQ_DebugClipCount,
                    EQ_DebugRequestedMode,
                    EQ_DebugTransitionTargetMode,
                    EQ_DebugAppliedMode);
            }

            lcd_disable_reason = EqualizerLcdFaultPolicy_Monitor(
                &lcd_fault_policy,
                EQ_DebugLcdRuntimeMask != 0U,
                EQ_DebugFrameLatencyDeadlineMissCount,
                EQ_DebugFrameServiceOverlapCount,
                EQ_DebugFrameServiceDroppedCount);
            if (lcd_disable_reason != 0UL)
            {
                EqualizerDisplay_AutoDisable(lcd_disable_reason);
            }
#endif

            if ((FLAG_AD == 0) && (FLAG_DA == 0) && (flag_ad_done == 0) &&
                (FLAG_KEY1 == 1))
            {
                FLAG_KEY1 = 0;
                Adc_Start();
            }
            if ((FLAG_AD == 0) && (FLAG_DA == 0) && (flag_ad_done == 0) &&
                (FLAG_KEY2 == 1))
            {
                FLAG_KEY2 = 0;
                Adc_Stop();
            }
            if ((FLAG_AD == 0) && (FLAG_DA == 0) && (flag_ad_done == 0) &&
                (FLAG_KEY3 == 1))
            {
                FLAG_KEY3 = 0;
                Dac_Start();
            }
            if ((FLAG_AD == 0) && (FLAG_DA == 0) && (flag_ad_done == 0) &&
                (FLAG_KEY4 == 1))
            {
                FLAG_KEY4 = 0;
                Dac_Stop();
            }

        }

#if EQ_ENABLE_LCD_DISPLAY != 0
        if (EqualizerLcdPolicy_CanService(
                &lcd_policy, EQ_DebugProcessFrames,
                FLAG_AD, FLAG_DA, flag_ad_done,
                EQ_FrameServicePending,
                EqualizerDisplay_HasPendingJob()))
        {
            lcd_audio_before = ((FLAG_AD != 0) ? 0x01U : 0U) |
                               ((FLAG_DA != 0) ? 0x02U : 0U) |
                               ((flag_ad_done != 0) ? 0x04U : 0U) |
                               ((EQ_FrameServicePending != 0U) ? 0x08U : 0U);
            lcd_policy_decision = EqualizerLcdPolicy_Decide(
                &lcd_policy, EQ_DebugProcessFrames,
                0, 0, 0, 0,
                (lcd_audio_before & 0x01U) != 0U,
                (lcd_audio_before & 0x02U) != 0U,
                (lcd_audio_before & 0x04U) != 0U,
                (lcd_audio_before & 0x08U) != 0U,
                1);
            if (lcd_policy_decision == EQ_LCD_POLICY_DEFER)
            {
                if (EqualizerLcdPolicy_RecordDeferred(
                        &lcd_policy, EQ_DebugProcessFrames))
                {
                    EQ_DebugLcdDeferredAudioCount++;
                }
            }
            else if (lcd_policy_decision == EQ_LCD_POLICY_SERVICE)
            {
                lcd_job = EqualizerDisplay_ServiceOneJob();
                lcd_audio_after = ((FLAG_AD != 0) ? 0x01U : 0U) |
                                  ((FLAG_DA != 0) ? 0x02U : 0U) |
                                  ((flag_ad_done != 0) ? 0x04U : 0U) |
                                  ((EQ_FrameServicePending != 0U) ?
                                   0x08U : 0U);
                if ((lcd_audio_before == 0U) && (lcd_audio_after != 0U))
                {
                    EQ_DebugLcdAudioArrivedDuringDrawCount++;
                    EqualizerCapture_NotifyEvent(
                        EQ_CAPTURE_TRIGGER_AUDIO_DURING_LCD);
                }
                if (lcd_job != EQ_LCD_JOB_NONE)
                {
                    EqualizerCapture_NotifyEvent(
                        EQ_CAPTURE_TRIGGER_LCD_JOB);
                    EqualizerLcdPolicy_RecordService(
                        &lcd_policy, EQ_DebugProcessFrames, 1);
                    continue;
                }
            }
        }
        else if ((EqualizerDisplay_HasPendingJob() != 0) &&
                 ((FLAG_AD != 0) || (FLAG_DA != 0) ||
                  (flag_ad_done != 0) ||
                  (EQ_FrameServicePending != 0U)) &&
                 EqualizerLcdPolicy_RecordDeferred(
                     &lcd_policy, EQ_DebugProcessFrames))
        {
            EQ_DebugLcdDeferredAudioCount++;
        }
#endif
    }
}

#else

void Equalizer_Flow_Example(void)
{
}

#endif
