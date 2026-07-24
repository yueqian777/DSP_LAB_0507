/**
 * user_equalizer_flow.c
 *
 * Project 3.3 board-side AD/DA equalizer example. CH1 is processed by
 * the equalizer; CH2-CH8 are passed through in the first version.
 */

#include "user_equalizer_flow.h"
#if EQ_ENABLE_AUDIO_FEATURE_ANALYZER != 0
#include "user_audio_feature_analyzer.h"
#endif
#include "user_smart_bass.h"
#include "user_dynamic_clarity.h"
#include "user_harshness_guard.h"
#include "user_equalizer_display.h"
#include "user_equalizer_response.h"
#include "equalizer_build_id.h"
#include "string.h"

#if (EQ_ENABLE_AUDIO_FEATURE_ANALYZER != 0) && \
    (EQ_ENABLE_LCD_DISPLAY != 0) && \
    ((EQ_UI_RUNTIME_DEFAULT_MASK & EQ_UI_RUNTIME_ANALYZER) != 0U)
#define EQ_ANALYZER_RUNTIME_DEFAULT_ENABLED 1U
#else
#define EQ_ANALYZER_RUNTIME_DEFAULT_ENABLED 0U
#endif

#ifndef EQ_ALGO_ONLY

#include "dac_api.h"
#include "key_api.h"
#include "system.h"
#include "uart_api.h"
#include "uartStdio.h"
#if EQ_ENABLE_PROJECT33_TOUCH != 0
#include "touch_api.h"
#endif

#ifndef EQ_ENABLE_UART_DIAGNOSTICS
#define EQ_ENABLE_UART_DIAGNOSTICS 1
#endif

#if defined(__TI_COMPILER_VERSION__) || defined(__TMS320C6X__)
#include "c6x.h"
#endif

#define EQ_CPU_CYCLES_PER_MS 456000.0f
#if EQ_ENABLE_AUDIO_FEATURE_ANALYZER != 0
#define EQ_UART_FEATURE_LINE_CAPACITY 72
#endif
static short EQ_AD_Buffer1[ADC_SAMPLE_1024];
static short EQ_AD_Buffer2[ADC_SAMPLE_1024];
static short EQ_AD_Buffer3[ADC_SAMPLE_1024];
static short EQ_AD_Buffer4[ADC_SAMPLE_1024];
static short EQ_AD_Buffer5[ADC_SAMPLE_1024];
static short EQ_AD_Buffer6[ADC_SAMPLE_1024];
static short EQ_AD_Buffer7[ADC_SAMPLE_1024];
static short EQ_AD_Buffer8[ADC_SAMPLE_1024];
static short EQ_DA_Buffer1[DAC_SAMPLE_1024];
#if (defined(__TI_COMPILER_VERSION__) || defined(__TMS320C6X__)) && \
    defined(DSP_LAB_PROJECT_SELECT) && (DSP_LAB_PROJECT_SELECT == 33)
#pragma DATA_SECTION(EQ_BoardState, ".subband_l2")
#if EQ_ENABLE_AUDIO_FEATURE_ANALYZER != 0
#pragma DATA_SECTION(EQ_AudioAnalyzerState, ".subband_l2")
#pragma DATA_SECTION(EQ_AnalyzerInput, ".subband_l2")
#endif
#if EQ_ENABLE_SMART_BASS != 0
#pragma DATA_SECTION(EQ_SmartBassState, ".subband_l2")
#endif
#if EQ_ENABLE_DYNAMIC_CLARITY != 0
#pragma DATA_SECTION(EQ_DynamicClarityState, ".subband_l2")
#endif
#if EQ_ENABLE_HARSHNESS_GUARD != 0
#pragma DATA_SECTION(EQ_HarshnessGuardState, ".subband_l2")
#endif
#endif
static EQ_STATE EQ_BoardState;
#if EQ_ENABLE_AUDIO_FEATURE_ANALYZER != 0
static AUDIO_FEATURE_ANALYZER EQ_AudioAnalyzerState;
static short EQ_AnalyzerInput[AUDIO_FEATURE_FRAME_LEN];
#endif
#if EQ_ENABLE_SMART_BASS != 0
static SMART_BASS_STATE EQ_SmartBassState;
static unsigned int EQ_SmartBassAnalyzerFault = 1U;
#endif
#if EQ_ENABLE_DYNAMIC_CLARITY != 0
static DYNAMIC_CLARITY_STATE EQ_DynamicClarityState;
static unsigned int EQ_DynamicClarityAnalyzerFault = 1U;
#if EQ_ENABLE_DYNAMIC_CLARITY_TRANSITION_CAPTURE != 0
static unsigned int EQ_DynamicClarityCapturePhase = 0U;
static unsigned int EQ_DynamicClarityCapturePrerollRemaining = 0U;
#endif
#endif
#if EQ_ENABLE_HARSHNESS_GUARD != 0
static HARSHNESS_GUARD_STATE EQ_HarshnessGuardState;
static unsigned int EQ_HarshnessGuardAnalyzerFault = 1U;
#if EQ_ENABLE_HARSHNESS_GUARD_TRANSITION_CAPTURE != 0
static unsigned int EQ_HarshnessGuardCapturePhase = 0U;
static unsigned int EQ_HarshnessGuardCapturePrerollRemaining = 0U;
#endif
#endif
static EQ_CONTROL_STATE EQ_BoardControl;
static EQ_BACKGROUND_SERVICE_STATE EQ_BackgroundService;
#if (EQ_ENABLE_LCD_DISPLAY != 0) && (EQ_ENABLE_TEN_BAND_EDITOR != 0)
typedef struct
{
    unsigned long analyzer_analysis_count;
    unsigned long active_generation;
    EQ_CONTROL_SEQUENCE applied_sequence;
    EQ_CONTROL_SEQUENCE submitted_sequence;
    unsigned long draft_version;
    int applied_mode;
    int smart_enabled;
    int smart_strength;
    int smart_active;
    int clarity_enabled;
    int clarity_strength;
    int clarity_active;
    int guard_enabled;
    int guard_strength;
    int guard_active;
    int page;
    int selected_band;
    int apply_status;
    unsigned int initialized;
} EQ_UI_EVENT_VERSION;

static EQ_UI_EDITOR_STATE EQ_UiEditorState;
static EQ_UI_EVENT_VERSION EQ_UiEventVersion;
static unsigned long EQ_UiSnapshotLastRequestFrame = ~0UL;
#endif
#if EQ_ENABLE_PROJECT33_TOUCH != 0
static EQ_UI_TOUCH_STATE EQ_TouchState;
static EQ_UI_TOUCH_TRANSFORM EQ_TouchTransform;
static unsigned long EQ_TouchLastActionFrame = ~0UL;
static unsigned long EQ_TouchLastServiceFrame = ~0UL;
static unsigned int EQ_TouchAcceptedThisPress = 0U;
#endif
static int EQ_LastServicedMode = -1;
static int EQ_AppliedDiagPath = -1;
static unsigned int EQ_FrameServiceStartCycle = 0U;
static unsigned int EQ_FrameActiveSegmentStartCycle = 0U;
static unsigned long EQ_FrameActiveServiceCycles = 0UL;
static unsigned char EQ_FrameServicePending = 0U;
#if EQ_ENABLE_AUDIO_FEATURE_ANALYZER != 0
static unsigned long EQ_AnalyzerLastDeferredFrame = ~0UL;
static unsigned int EQ_AnalyzerLastEnabled = 0U;
static EQ_UART_FEATURE_AUDIT EQ_UartFeatureAudit;
#endif

static void EQ_UartInit(void)
{
#if EQ_ENABLE_UART_DIAGNOSTICS != 0
    Uart2_Init_Lite(UART_BAUD_115200);
    UARTPuts("P33 UART 115200 8N1\r\n", -1);
    UARTPuts("P33 BUILD " EQ_BUILD_GIT_SHA "\r\n", -1);
#endif
}

static void EQ_UartReportStage(unsigned long stage)
{
#if EQ_ENABLE_UART_DIAGNOSTICS != 0
    static char line[] = "P33 INIT 00\r\n";

    if (stage > 99UL)
    {
        stage = 99UL;
    }
    line[9] = (char)('0' + (stage / 10UL));
    line[10] = (char)('0' + (stage % 10UL));
    UARTPuts(line, -1);
#else
    (void)stage;
#endif
}

#if EQ_ENABLE_AUDIO_FEATURE_ANALYZER != 0
#if EQ_ENABLE_UART_DIAGNOSTICS != 0
static int EQ_UartAppendChar(char *line, int index, char value)
{
    if ((index < 0) || (index >= (EQ_UART_FEATURE_LINE_CAPACITY - 1)))
    {
        return -1;
    }
    line[index] = value;
    return index + 1;
}

static int EQ_UartAppendLiteral(char *line, int index, const char *text)
{
    while (*text != '\0')
    {
        index = EQ_UartAppendChar(line, index, *text);
        if (index < 0)
        {
            return -1;
        }
        text++;
    }
    return index;
}

static int EQ_UartAppendUnsigned(char *line, int index,
                                 unsigned long value)
{
    char digits[10];
    int count;

    count = 0;
    do
    {
        digits[count] = (char)('0' + (value % 10UL));
        value /= 10UL;
        count++;
    } while ((value != 0UL) && (count < (int)sizeof(digits)));

    while (count > 0)
    {
        count--;
        index = EQ_UartAppendChar(line, index, digits[count]);
        if (index < 0)
        {
            return -1;
        }
    }
    return index;
}

static int EQ_UartAppendSigned(char *line, int index, long value)
{
    unsigned long magnitude;

    if (value < 0L)
    {
        index = EQ_UartAppendChar(line, index, '-');
        if (index < 0)
        {
            return -1;
        }
        magnitude = (unsigned long)(-value);
    }
    else
    {
        magnitude = (unsigned long)value;
    }
    return EQ_UartAppendUnsigned(line, index, magnitude);
}

static long EQ_DbToTenths(float value)
{
    float scaled;

    if (value > 9999.9f)
    {
        value = 9999.9f;
    }
    else if (value < -9999.9f)
    {
        value = -9999.9f;
    }
    scaled = value * 10.0f;
    return (scaled >= 0.0f) ?
        (long)(scaled + 0.5f) : (long)(scaled - 0.5f);
}

static unsigned int EQ_AudioFlagMask(unsigned char flag_ad_done)
{
    return ((FLAG_AD != 0) ? 0x01U : 0U) |
           ((FLAG_DA != 0) ? 0x02U : 0U) |
           ((flag_ad_done != 0) ? 0x04U : 0U) |
           ((EQ_FrameServicePending != 0U) ? 0x08U : 0U);
}
#endif

static void EQ_SyncUartFeatureAuditDebug(void)
{
    EQ_DebugUartFeatureDeadlineDelta =
        EQ_UartFeatureAudit.deadline_delta;
    EQ_DebugUartFeatureLatencyMissDelta =
        EQ_UartFeatureAudit.latency_delta;
    EQ_DebugUartFeatureOverlapDelta =
        EQ_UartFeatureAudit.overlap_delta;
    EQ_DebugUartFeatureDroppedDelta =
        EQ_UartFeatureAudit.dropped_delta;
    EQ_DebugUartFeatureAuditPending = EQ_UartFeatureAudit.pending;
    EQ_DebugUartFeatureAuditComplete = EQ_UartFeatureAudit.complete;
    EQ_DebugUartFeatureAudioArrived =
        EQ_UartFeatureAudit.audio_arrived;
    EQ_DebugUartFeatureBaselineFrame =
        EQ_UartFeatureAudit.baseline_process_frames;
}

static void EQ_UartReportFeatureOnce(unsigned char flag_ad_done)
{
#if EQ_ENABLE_UART_DIAGNOSTICS != 0
    static char line[EQ_UART_FEATURE_LINE_CAPACITY];
    unsigned int audio_flags_before;
    unsigned int audio_flags_after;
    int index;

    index = EQ_UartAppendLiteral(line, 0, "P33 FEAT,");
    index = EQ_UartAppendUnsigned(
        line, index, EQ_DebugAnalyzerAnalysisCount);
    index = EQ_UartAppendChar(line, index, ',');
    index = EQ_UartAppendSigned(
        line, index, EQ_DbToTenths(EQ_DebugAnalyzerRmsDbfs));
    index = EQ_UartAppendChar(line, index, ',');
    index = EQ_UartAppendSigned(
        line, index, EQ_DbToTenths(EQ_DebugAnalyzerBassDb));
    index = EQ_UartAppendChar(line, index, ',');
    index = EQ_UartAppendSigned(
        line, index, EQ_DbToTenths(EQ_DebugAnalyzerMudDb));
    index = EQ_UartAppendChar(line, index, ',');
    index = EQ_UartAppendSigned(
        line, index, EQ_DbToTenths(EQ_DebugAnalyzerPresenceDb));
    index = EQ_UartAppendChar(line, index, ',');
    index = EQ_UartAppendSigned(
        line, index, EQ_DbToTenths(EQ_DebugAnalyzerBrightnessDb));
    index = EQ_UartAppendLiteral(line, index, "\r\n");
    if (index < 0)
    {
        EQ_DebugUartFeatureRequest = 0U;
        return;
    }
    line[index] = '\0';

    audio_flags_before = EQ_AudioFlagMask(flag_ad_done);
    if (!EqualizerUartFeatureAudit_Begin(
            &EQ_UartFeatureAudit,
            EQ_DebugProcessFrames,
            EQ_DebugDeadlineMissCount,
            EQ_DebugFrameLatencyDeadlineMissCount,
            EQ_DebugFrameServiceOverlapCount,
            EQ_DebugFrameServiceDroppedCount,
            audio_flags_before))
    {
        return;
    }
    UARTPuts(line, -1);
    audio_flags_after = EQ_AudioFlagMask(flag_ad_done);
    EqualizerUartFeatureAudit_EndWrite(
        &EQ_UartFeatureAudit, audio_flags_after);
    EQ_SyncUartFeatureAuditDebug();
#endif
    EQ_DebugUartFeatureRequest = 0U;
}

static void EQ_CompleteUartFeatureAudit(void)
{
    if (EqualizerUartFeatureAudit_CompleteAfterFrame(
            &EQ_UartFeatureAudit,
            EQ_DebugProcessFrames,
            EQ_DebugDeadlineMissCount,
            EQ_DebugFrameLatencyDeadlineMissCount,
            EQ_DebugFrameServiceOverlapCount,
            EQ_DebugFrameServiceDroppedCount))
    {
        EQ_SyncUartFeatureAuditDebug();
    }
}
#endif /* EQ_ENABLE_AUDIO_FEATURE_ANALYZER */

#endif

#if (defined(__TI_COMPILER_VERSION__) || defined(__TMS320C6X__)) && \
    defined(DSP_LAB_PROJECT_SELECT) && (DSP_LAB_PROJECT_SELECT == 33)
#pragma DATA_SECTION(EQ_DebugInitStage, ".subband_l2")
#pragma DATA_SECTION(EQ_DebugFlagAdDone, ".subband_l2")
#endif
volatile unsigned long EQ_DebugAdFrames = 0UL;
volatile unsigned long EQ_DebugDaFrames = 0UL;
volatile unsigned long EQ_DebugProcessFrames = 0UL;
volatile unsigned long EQ_DebugInitStage = 0UL;
volatile unsigned int EQ_DebugFlagAdDone = 0U;
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
volatile unsigned long EQ_DebugStaticDynamicTransitionOverlapFrameCount = 0UL;
#if EQ_ENABLE_PROJECT33_TOUCH != 0
volatile unsigned int EQ_DebugTouchRawX = 0U;
volatile unsigned int EQ_DebugTouchRawY = 0U;
volatile int EQ_DebugTouchScreenX = 0;
volatile int EQ_DebugTouchScreenY = 0;
volatile unsigned int EQ_DebugTouchPressed = 0U;
volatile int EQ_DebugTouchLastAction = EQ_UI_ACTION_NONE;
volatile unsigned long EQ_DebugTouchActionCount = 0UL;
volatile unsigned long EQ_DebugTouchRejectedCount = 0UL;
volatile unsigned long EQ_DebugTouchInvalidCoordinateCount = 0UL;
volatile unsigned long EQ_DebugTouchPresetRequestCount = 0UL;
volatile unsigned long EQ_DebugTouchDynamicEnableRequestCount = 0UL;
volatile unsigned long EQ_DebugTouchDynamicStrengthRequestCount = 0UL;
volatile unsigned long EQ_DebugTouchEditorActionCount = 0UL;
volatile unsigned long EQ_DebugTouchDuplicateActionCount = 0UL;
volatile unsigned long
    EQ_DebugTouchActionHistogram[EQ_TOUCH_ACTION_DIAGNOSTIC_COUNT];
volatile unsigned long EQ_DebugTouchLastCycles = 0UL;
volatile unsigned long EQ_DebugTouchMaxCycles = 0UL;
#if defined(__TI_COMPILER_VERSION__) || defined(__TMS320C6X__)
#pragma RETAIN(EQ_DebugTouchStateBytes)
#pragma RETAIN(EQ_DebugTouchActionHistogramSize)
#endif
volatile const unsigned long EQ_DebugTouchStateBytes =
    (unsigned long)(sizeof(EQ_UI_TOUCH_STATE) +
                    sizeof(EQ_UI_TOUCH_TRANSFORM));
volatile const unsigned int EQ_DebugTouchActionHistogramSize =
    EQ_TOUCH_ACTION_DIAGNOSTIC_COUNT;
#endif
#if (EQ_ENABLE_LCD_DISPLAY != 0) && (EQ_ENABLE_TEN_BAND_EDITOR != 0)
volatile int EQ_DebugUiRequestedPage = EQ_UI_PAGE_DYNAMIC_STATUS;
volatile int EQ_DebugUiDisplayedPage = EQ_UI_PAGE_DYNAMIC_STATUS;
volatile unsigned int EQ_DebugUiPageBuilding = 0U;
volatile unsigned long EQ_DebugUiSnapshotBuildCount = 0UL;
volatile unsigned long EQ_DebugUiSnapshotSkippedCount = 0UL;
volatile unsigned long EQ_DebugUiAppliedGainRefreshCount = 0UL;
volatile unsigned long EQ_DebugUiDraftVersion = 0UL;
volatile unsigned long EQ_DebugUiPageSwitchCount = 0UL;
volatile int EQ_DebugUiEditorSelectedBand = 0;
volatile unsigned int EQ_DebugUiEditorDraftDirty = 0U;
volatile unsigned int EQ_DebugUiEditorSubmittedValid = 0U;
volatile int EQ_DebugUiEditorApplyStatus = EQ_UI_APPLY_APPLIED;
volatile EQ_CONTROL_SEQUENCE EQ_DebugUiEditorSubmittedSequence = 0U;
volatile EQ_CONTROL_SEQUENCE EQ_DebugUiEditorAppliedSequence = 0U;
volatile signed char EQ_DebugUiEditorDraftGainHalfDb[EQ_NUM_BANDS];
volatile signed char EQ_DebugUiEditorSubmittedGainHalfDb[EQ_NUM_BANDS];
volatile signed char EQ_DebugUiEditorAppliedGainHalfDb[EQ_NUM_BANDS];
volatile const unsigned long EQ_DebugUiEditorStateBytes =
    (unsigned long)sizeof(EQ_UI_EDITOR_STATE);
volatile const unsigned long EQ_DebugUiTotalStateBytes =
    (unsigned long)(sizeof(EQ_UI_STATE) + sizeof(EQ_UI_EDITOR_STATE));
#endif
volatile const unsigned int EQ_DebugAnalyzerCompiled =
    EQ_ENABLE_AUDIO_FEATURE_ANALYZER;
volatile unsigned int EQ_DebugAnalyzerEnabled =
    EQ_ANALYZER_RUNTIME_DEFAULT_ENABLED;
volatile unsigned int EQ_DebugAnalyzerResetRequest = 0U;
volatile unsigned int EQ_DebugAnalyzerPending = 0U;
volatile unsigned int EQ_DebugAnalyzerValid = 0U;
volatile unsigned int EQ_DebugAnalyzerWarmup = 0U;
volatile unsigned long EQ_DebugAnalyzerRunCount = 0UL;
volatile unsigned long EQ_DebugAnalyzerAnalysisCount = 0UL;
volatile unsigned long EQ_DebugAnalyzerEpoch = 0UL;
volatile unsigned long EQ_DebugAnalyzerPublicationCount = 0UL;
volatile unsigned long EQ_DebugAnalyzerDeferredCount = 0UL;
volatile unsigned long EQ_DebugAnalyzerPendingOverwriteCount = 0UL;
volatile unsigned long EQ_DebugAnalyzerAudioArrivedCount = 0UL;
volatile unsigned long EQ_DebugAnalyzerLastCycles = 0UL;
volatile unsigned long EQ_DebugAnalyzerMaxCycles = 0UL;
volatile float EQ_DebugAnalyzerPeakDbfs = -240.0f;
volatile float EQ_DebugAnalyzerRmsDbfs = -240.0f;
volatile float EQ_DebugAnalyzerBassDb = 0.0f;
volatile float EQ_DebugAnalyzerMudDb = 0.0f;
volatile float EQ_DebugAnalyzerPresenceDb = 0.0f;
volatile float EQ_DebugAnalyzerBrightnessDb = 0.0f;
volatile const unsigned int EQ_DebugSmartBassCompiled =
    EQ_ENABLE_SMART_BASS;
volatile unsigned int EQ_DebugSmartBassEnabled = 0U;
volatile int EQ_DebugSmartBassStrength = SMART_BASS_STRENGTH_MEDIUM;
volatile unsigned int EQ_DebugSmartBassProcessingActive = 0U;
volatile float EQ_DebugSmartBassInputBassDb = 0.0f;
volatile float EQ_DebugSmartBassInputRmsDbfs = -240.0f;
volatile int EQ_DebugSmartBassRequestedLevel = 0;
volatile int EQ_DebugSmartBassAppliedLevel = 0;
volatile int EQ_DebugSmartBassPendingLevel = 0;
volatile float EQ_DebugSmartBassRequestedGainDb = 0.0f;
volatile float EQ_DebugSmartBassAppliedGainDb = 0.0f;
volatile unsigned int EQ_DebugSmartBassTransitionActive = 0U;
volatile float EQ_DebugSmartBassTransitionProgress = 0.0f;
volatile unsigned long EQ_DebugSmartBassDecisionCount = 0UL;
volatile unsigned long EQ_DebugSmartBassLevelChangeCount = 0UL;
volatile unsigned long EQ_DebugSmartBassTransitionCount = 0UL;
volatile unsigned long EQ_DebugSmartBassInvalidReleaseCount = 0UL;
volatile unsigned long EQ_DebugSmartBassLastCycles = 0UL;
volatile unsigned long EQ_DebugSmartBassMaxCycles = 0UL;
volatile unsigned long EQ_DebugSmartBassSaturationCount = 0UL;
volatile unsigned long EQ_DebugSmartBassNonFiniteCount = 0UL;
volatile int EQ_DebugSmartBassReason = SMART_BASS_REASON_DISABLED;
volatile const unsigned int EQ_DebugDynamicClarityCompiled =
    EQ_ENABLE_DYNAMIC_CLARITY;
volatile unsigned int EQ_DebugDynamicClarityEnabled = 0U;
volatile int EQ_DebugDynamicClarityStrength =
    DYNAMIC_CLARITY_STRENGTH_MEDIUM;
volatile unsigned int EQ_DebugDynamicClarityProcessingActive = 0U;
volatile float EQ_DebugDynamicClarityMudDb = 0.0f;
volatile float EQ_DebugDynamicClarityPresenceDb = 0.0f;
volatile float EQ_DebugDynamicClarityMaskingDb = 0.0f;
volatile float EQ_DebugDynamicClarityRmsDbfs = -240.0f;
volatile int EQ_DebugDynamicClarityRequestedLevel = 0;
volatile int EQ_DebugDynamicClarityAppliedLevel = 0;
volatile int EQ_DebugDynamicClarityPendingLevel = 0;
volatile float EQ_DebugDynamicClarityRequestedGainDb = 0.0f;
volatile float EQ_DebugDynamicClarityAppliedGainDb = 0.0f;
volatile unsigned int EQ_DebugDynamicClarityTransitionActive = 0U;
volatile float EQ_DebugDynamicClarityTransitionProgress = 0.0f;
volatile int EQ_DebugDynamicClarityReason =
    DYNAMIC_CLARITY_REASON_DISABLED;
volatile unsigned long EQ_DebugDynamicClarityDecisionCount = 0UL;
volatile unsigned long EQ_DebugDynamicClarityLevelChangeCount = 0UL;
volatile unsigned long EQ_DebugDynamicClarityTransitionCount = 0UL;
volatile unsigned long EQ_DebugDynamicClarityInvalidReleaseCount = 0UL;
volatile unsigned long EQ_DebugDynamicClarityLastCycles = 0UL;
volatile unsigned long EQ_DebugDynamicClarityMaxCycles = 0UL;
volatile unsigned long EQ_DebugDynamicClaritySaturationCount = 0UL;
volatile unsigned long EQ_DebugDynamicClarityNonFiniteCount = 0UL;
volatile const unsigned int EQ_DebugHarshnessGuardCompiled =
    EQ_ENABLE_HARSHNESS_GUARD;
volatile unsigned int EQ_DebugHarshnessGuardEnabled = 0U;
volatile int EQ_DebugHarshnessGuardStrength =
    HARSHNESS_GUARD_STRENGTH_MEDIUM;
volatile unsigned int EQ_DebugHarshnessGuardProcessingActive = 0U;
volatile float EQ_DebugHarshnessGuardBrightnessDb = 0.0f;
volatile float EQ_DebugHarshnessGuardPresenceDb = 0.0f;
volatile float EQ_DebugHarshnessGuardExcessDb = 0.0f;
volatile float EQ_DebugHarshnessGuardRmsDbfs = -240.0f;
volatile int EQ_DebugHarshnessGuardRequestedLevel = 0;
volatile int EQ_DebugHarshnessGuardAppliedLevel = 0;
volatile int EQ_DebugHarshnessGuardPendingLevel = 0;
volatile float EQ_DebugHarshnessGuardRequestedGainDb = 0.0f;
volatile float EQ_DebugHarshnessGuardAppliedGainDb = 0.0f;
volatile unsigned int EQ_DebugHarshnessGuardTransitionActive = 0U;
volatile float EQ_DebugHarshnessGuardTransitionProgress = 0.0f;
volatile int EQ_DebugHarshnessGuardReason =
    HARSHNESS_GUARD_REASON_DISABLED;
volatile unsigned long EQ_DebugHarshnessGuardDecisionCount = 0UL;
volatile unsigned long EQ_DebugHarshnessGuardLevelChangeCount = 0UL;
volatile unsigned long EQ_DebugHarshnessGuardTransitionCount = 0UL;
volatile unsigned long EQ_DebugHarshnessGuardInvalidReleaseCount = 0UL;
volatile unsigned long EQ_DebugHarshnessGuardLastCycles = 0UL;
volatile unsigned long EQ_DebugHarshnessGuardMaxCycles = 0UL;
volatile unsigned long EQ_DebugHarshnessGuardSaturationCount = 0UL;
volatile unsigned long EQ_DebugHarshnessGuardNonFiniteCount = 0UL;
#if EQ_ENABLE_FOUR_WAY_TRANSITION_DIAGNOSTICS != 0
volatile unsigned int EQ_DebugFourWayTransitionMask = 0U;
volatile unsigned long EQ_DebugFourWayTransitionOverlapCount = 0UL;
volatile unsigned long EQ_DebugFourWayTransitionFirstFrame = 0UL;
volatile int EQ_DebugFourWayTransitionArmMode = EQ_PRESET_NONE;
volatile unsigned long EQ_DebugFourWayTransitionArmCount = 0UL;
#endif
#if EQ_ENABLE_DYNAMIC_CLARITY_TRANSITION_CAPTURE != 0
volatile unsigned int EQ_DebugDynamicClarityTransitionCaptureRequest = 0U;
volatile unsigned int EQ_DebugDynamicClarityTransitionCaptureActive = 0U;
volatile unsigned int EQ_DebugDynamicClarityTransitionCaptureDone = 0U;
volatile unsigned int EQ_DebugDynamicClarityTransitionCaptureOverride = 0U;
volatile int EQ_DebugDynamicClarityTransitionCaptureBaseLevel = 0;
volatile int EQ_DebugDynamicClarityTransitionCaptureTargetLevel = 0;
volatile int EQ_DebugDynamicClarityTransitionCaptureResult = 0;
volatile unsigned long
    EQ_DebugDynamicClarityTransitionCaptureRequestFrame = 0UL;
volatile unsigned long
    EQ_DebugDynamicClarityTransitionCaptureTriggerFrame = 0UL;
volatile unsigned long
    EQ_DebugDynamicClarityTransitionCaptureDoneFrame = 0UL;
#endif
#if EQ_ENABLE_HARSHNESS_GUARD_TRANSITION_CAPTURE != 0
volatile unsigned int EQ_DebugHarshnessGuardTransitionCaptureRequest = 0U;
volatile unsigned int EQ_DebugHarshnessGuardTransitionCaptureActive = 0U;
volatile unsigned int EQ_DebugHarshnessGuardTransitionCaptureDone = 0U;
volatile unsigned int EQ_DebugHarshnessGuardTransitionCaptureOverride = 0U;
volatile int EQ_DebugHarshnessGuardTransitionCaptureBaseLevel = 0;
volatile int EQ_DebugHarshnessGuardTransitionCaptureTargetLevel = 0;
volatile int EQ_DebugHarshnessGuardTransitionCaptureResult = 0;
volatile unsigned long
    EQ_DebugHarshnessGuardTransitionCaptureRequestFrame = 0UL;
volatile unsigned long
    EQ_DebugHarshnessGuardTransitionCaptureTriggerFrame = 0UL;
volatile unsigned long
    EQ_DebugHarshnessGuardTransitionCaptureDoneFrame = 0UL;
#endif
#if EQ_ENABLE_DYNAMIC_CLARITY_TIMING_DIAGNOSTICS != 0
volatile unsigned long
    EQ_DebugDynamicClarityTimingFrameCount[DYNAMIC_CLARITY_PATH_COUNT];
volatile unsigned long
    EQ_DebugDynamicClarityTimingLastCycles[DYNAMIC_CLARITY_PATH_COUNT];
volatile unsigned long
    EQ_DebugDynamicClarityTimingMaxCycles[DYNAMIC_CLARITY_PATH_COUNT];
volatile unsigned long
    EQ_DebugDynamicClarityTimingMaxProcessFrame[DYNAMIC_CLARITY_PATH_COUNT];
volatile int
    EQ_DebugDynamicClarityTimingMaxActiveLevel[DYNAMIC_CLARITY_PATH_COUNT];
volatile int
    EQ_DebugDynamicClarityTimingMaxPendingLevel[DYNAMIC_CLARITY_PATH_COUNT];
volatile int
    EQ_DebugDynamicClarityTimingMaxTargetLevel[DYNAMIC_CLARITY_PATH_COUNT];
volatile int
    EQ_DebugDynamicClarityTimingMaxTransitionRemaining[DYNAMIC_CLARITY_PATH_COUNT];
volatile unsigned long
    EQ_DebugDynamicClarityTimingMaxAnalyzerCount[DYNAMIC_CLARITY_PATH_COUNT];
volatile unsigned long
    EQ_DebugDynamicClarityTimingMaxAdFrames[DYNAMIC_CLARITY_PATH_COUNT];
volatile unsigned long
    EQ_DebugDynamicClarityTimingMaxDaFrames[DYNAMIC_CLARITY_PATH_COUNT];
volatile unsigned int
    EQ_DebugDynamicClarityTimingMaxFlagAd[DYNAMIC_CLARITY_PATH_COUNT];
volatile unsigned int
    EQ_DebugDynamicClarityTimingMaxFlagDa[DYNAMIC_CLARITY_PATH_COUNT];
volatile unsigned long
    EQ_DebugDynamicClarityTimingMaxDeadlineCount[DYNAMIC_CLARITY_PATH_COUNT];
volatile unsigned long
    EQ_DebugDynamicClarityTimingMaxLatencyMissCount[DYNAMIC_CLARITY_PATH_COUNT];
volatile unsigned long
    EQ_DebugDynamicClarityTimingMaxOverlapCount[DYNAMIC_CLARITY_PATH_COUNT];
volatile unsigned long
    EQ_DebugDynamicClarityTimingMaxDroppedCount[DYNAMIC_CLARITY_PATH_COUNT];
volatile unsigned long
    EQ_DebugDynamicClarityTimingMaxUpdateCount[DYNAMIC_CLARITY_PATH_COUNT];
#endif
volatile unsigned int EQ_DebugUartFeatureRequest = 0U;
volatile unsigned long EQ_DebugUartFeatureDeadlineDelta = 0UL;
volatile unsigned long EQ_DebugUartFeatureLatencyMissDelta = 0UL;
volatile unsigned long EQ_DebugUartFeatureOverlapDelta = 0UL;
volatile unsigned long EQ_DebugUartFeatureDroppedDelta = 0UL;
volatile unsigned int EQ_DebugUartFeatureAuditPending = 0U;
volatile unsigned int EQ_DebugUartFeatureAuditComplete = 0U;
volatile unsigned int EQ_DebugUartFeatureAudioArrived = 0U;
volatile unsigned long EQ_DebugUartFeatureBaselineFrame = 0UL;
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
EQ_CONTROL_MAILBOX EQ_ControlMailbox;
volatile int EQ_DebugControlCommand = EQ_CONTROL_NONE;
volatile int EQ_DebugControlBand = 0;
volatile int EQ_DebugControlPreset = EQ_PRESET_FLAT;
volatile float EQ_DebugControlValueDb = 0.0f;
volatile float EQ_DebugControlStepDb = 0.0f;
volatile float EQ_DebugControlShadowGainDb[EQ_NUM_BANDS] =
{
    0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 0.0f, 0.0f
};
volatile EQ_CONTROL_SEQUENCE EQ_DebugControlRequestToken = 0U;
volatile EQ_CONTROL_SEQUENCE EQ_DebugControlObservedToken = 0U;
volatile EQ_CONTROL_SEQUENCE EQ_DebugControlAcceptedToken = 0U;
volatile EQ_CONTROL_SEQUENCE EQ_DebugControlTargetToken = 0U;
volatile EQ_CONTROL_SEQUENCE EQ_DebugControlPreparedToken = 0U;
volatile int EQ_DebugControlReadyValid = 0;
volatile EQ_CONTROL_SEQUENCE EQ_DebugControlInstalledToken = 0U;
volatile EQ_CONTROL_SEQUENCE EQ_DebugControlAppliedToken = 0U;
volatile int EQ_DebugControlInstalledPairValid = 0;
volatile unsigned long EQ_DebugControlRejectedCount = 0UL;
volatile unsigned long EQ_DebugControlCoalescedCount = 0UL;
volatile int EQ_DebugControlLastError = EQ_CONTROL_ERROR_NONE;
volatile int EQ_DebugBuilderState = EQ_BUILDER_IDLE;
volatile int EQ_DebugBuilderSectionIndex = 0;
volatile int EQ_DebugBuilderScanIndex = 0;
volatile unsigned long EQ_DebugBuilderGeneration = 0UL;
volatile EQ_CONTROL_SEQUENCE EQ_DebugBuilderRequestToken = 0U;
volatile unsigned long EQ_DebugBuilderSliceCount = 0UL;
volatile unsigned long EQ_DebugBuilderCancelCount = 0UL;
volatile unsigned long EQ_DebugBuilderRestartCount = 0UL;
volatile int EQ_DebugBuilderLastError = EQ_CONTROL_ERROR_NONE;
volatile unsigned long EQ_DebugBuilderLastCycles = 0UL;
volatile unsigned long EQ_DebugBuilderMaxCycles = 0UL;
volatile float EQ_DebugBuilderLastMs = 0.0f;
volatile float EQ_DebugBuilderMaxMs = 0.0f;
volatile unsigned long EQ_DebugBuilderDeferredAudioCount = 0UL;
volatile unsigned long EQ_DebugBuilderAudioArrivedDuringSliceCount = 0UL;
volatile unsigned long EQ_DebugResponseActiveGeneration = 0UL;
volatile unsigned long EQ_DebugResponseTargetGeneration = 0UL;
volatile int EQ_DebugResponseTransitionActive = 0;
volatile float EQ_DebugResponseTransitionProgress = 0.0f;
volatile int EQ_DebugResponseActivePathType =
    EQ_RESPONSE_PATH_IDENTITY_RBJ_FLAT_COPY;
volatile int EQ_DebugResponseTargetValid = 0;
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
           (source == EQ_CAPTURE_TRIGGER_AUDIO_DURING_LCD)
#if EQ_ENABLE_DYNAMIC_CLARITY_TRANSITION_CAPTURE != 0
           || (source == EQ_CAPTURE_TRIGGER_DYNAMIC_CLARITY)
#endif
#if EQ_ENABLE_HARSHNESS_GUARD_TRANSITION_CAPTURE != 0
           || (source == EQ_CAPTURE_TRIGGER_HARSHNESS_GUARD)
#endif
           ;
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
    policy->control_quiet_until_frame = 0UL;
    policy->last_deferred_frame = ~0UL;
    policy->last_service_frame_valid = 0U;
}

int EqualizerLcdPolicy_CanService(const EQ_LCD_SERVICE_POLICY *policy,
                                  unsigned long process_frames,
                                  int flag_ad,
                                  int flag_da,
                                  int flag_ad_done,
                                  int frame_service_pending,
                                  int audio_serviced,
                                  int touch_serviced,
                                  int builder_serviced,
                                  int analyzer_serviced,
                                  int has_pending_job)
{
    return (flag_ad == 0) &&
           (flag_da == 0) &&
           (flag_ad_done == 0) &&
           (frame_service_pending == 0) &&
           (audio_serviced == 0) &&
           (touch_serviced == 0) &&
           (builder_serviced == 0) &&
           (analyzer_serviced == 0) &&
           (process_frames >= policy->control_quiet_until_frame) &&
           ((policy->last_service_frame_valid == 0U) ||
            ((process_frames - policy->last_service_frame) >=
             EQ_LCD_MIN_JOB_GAP_FRAMES)) &&
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
                               int audio_serviced,
                               int touch_serviced,
                               int builder_serviced,
                               int analyzer_serviced,
                               int has_pending_job)
{
    if (!EqualizerLcdPolicy_CanService(
             policy, process_frames,
             outer_flag_ad, outer_flag_da, outer_flag_ad_done,
             outer_frame_service_pending,
             audio_serviced, touch_serviced,
             builder_serviced, analyzer_serviced,
             has_pending_job))
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
             predraw_frame_service_pending,
             audio_serviced, touch_serviced,
             builder_serviced, analyzer_serviced,
             has_pending_job))
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
        policy->last_service_frame_valid = 1U;
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

void EqualizerLcdPolicy_RecordControlChange(
    EQ_LCD_SERVICE_POLICY *policy, unsigned long process_frames)
{
    policy->control_quiet_until_frame =
        process_frames + EQ_LCD_CONTROL_QUIET_FRAMES;
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

void EqualizerBackgroundService_Init(EQ_BACKGROUND_SERVICE_STATE *state)
{
    if (state == 0)
    {
        return;
    }
    state->consumed_frame = 0UL;
    state->consumed_frame_valid = 0;
    state->consumed_kind = EQ_BACKGROUND_NONE;
    state->next_preference = EQ_BACKGROUND_BUILDER;
}

int EqualizerBackgroundService_IsAudioSafeFinalCheck(
    int final_flag_ad,
    int final_flag_da,
    int final_flag_ad_done,
    int final_frame_service_pending)
{
    return ((final_flag_ad == 0) && (final_flag_da == 0) &&
            (final_flag_ad_done == 0) &&
            (final_frame_service_pending == 0)) ? 1 : 0;
}

int EqualizerAnalyzer_CanService(
    int final_flag_ad,
    int final_flag_da,
    int final_flag_ad_done,
    int final_frame_service_pending,
    int builder_eligible,
    int analyzer_enabled,
    int analyzer_pending)
{
    return (EqualizerBackgroundService_IsAudioSafeFinalCheck(
                final_flag_ad, final_flag_da, final_flag_ad_done,
                final_frame_service_pending) != 0) &&
           (builder_eligible == 0) &&
           (analyzer_enabled != 0) &&
           (analyzer_pending != 0);
}

#if defined(EQ_ALGO_ONLY) || (EQ_ENABLE_AUDIO_FEATURE_ANALYZER != 0)
void EqualizerAnalyzerRuntime_Init(unsigned int *last_enabled)
{
    if (last_enabled != 0)
    {
        *last_enabled = 0U;
    }
}

int EqualizerAnalyzerRuntime_Decide(
    unsigned int *last_enabled,
    unsigned int requested_enabled,
    unsigned int reset_requested,
    int audio_safe,
    int builder_eligible)
{
    unsigned int requested;

    if ((last_enabled == 0) || (audio_safe == 0))
    {
        return EQ_ANALYZER_ACTION_NONE;
    }
    requested = (requested_enabled != 0U) ? 1U : 0U;
    if ((*last_enabled != 0U) && (requested == 0U))
    {
        *last_enabled = 0U;
        return EQ_ANALYZER_ACTION_DISABLE;
    }
    if ((*last_enabled == 0U) && (requested != 0U))
    {
        if (builder_eligible != 0)
        {
            return EQ_ANALYZER_ACTION_NONE;
        }
        *last_enabled = 1U;
        return EQ_ANALYZER_ACTION_ENABLE_RESET;
    }
    if ((reset_requested != 0U) && (builder_eligible == 0))
    {
        return EQ_ANALYZER_ACTION_MANUAL_RESET;
    }
    return EQ_ANALYZER_ACTION_NONE;
}

void EqualizerUartFeatureAudit_Init(EQ_UART_FEATURE_AUDIT *audit)
{
    if (audit != 0)
    {
        memset(audit, 0, sizeof(*audit));
    }
}

int EqualizerUartFeatureAudit_Begin(
    EQ_UART_FEATURE_AUDIT *audit,
    unsigned long process_frames,
    unsigned long deadline,
    unsigned long latency,
    unsigned long overlap,
    unsigned long dropped,
    unsigned int flags_before)
{
    if ((audit == 0) || (audit->pending != 0U))
    {
        return 0;
    }
    audit->baseline_process_frames = process_frames;
    audit->baseline_deadline = deadline;
    audit->baseline_latency = latency;
    audit->baseline_overlap = overlap;
    audit->baseline_dropped = dropped;
    audit->deadline_delta = 0UL;
    audit->latency_delta = 0UL;
    audit->overlap_delta = 0UL;
    audit->dropped_delta = 0UL;
    audit->flags_before = flags_before;
    audit->pending = 0U;
    audit->complete = 0U;
    audit->audio_arrived = 0U;
    return 1;
}

void EqualizerUartFeatureAudit_EndWrite(
    EQ_UART_FEATURE_AUDIT *audit,
    unsigned int flags_after)
{
    if (audit == 0)
    {
        return;
    }
    if ((audit->flags_before == 0U) && (flags_after != 0U))
    {
        audit->audio_arrived = 1U;
    }
    audit->pending = 1U;
    audit->complete = 0U;
}

int EqualizerUartFeatureAudit_CompleteAfterFrame(
    EQ_UART_FEATURE_AUDIT *audit,
    unsigned long process_frames,
    unsigned long deadline,
    unsigned long latency,
    unsigned long overlap,
    unsigned long dropped)
{
    if ((audit == 0) || (audit->pending == 0U) ||
        (process_frames <= audit->baseline_process_frames))
    {
        return 0;
    }
    audit->deadline_delta = deadline - audit->baseline_deadline;
    audit->latency_delta = latency - audit->baseline_latency;
    audit->overlap_delta = overlap - audit->baseline_overlap;
    audit->dropped_delta = dropped - audit->baseline_dropped;
    audit->pending = 0U;
    audit->complete = 1U;
    return 1;
}
#endif

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
    int lcd_eligible)
{
    if (state == 0)
    {
        return EQ_BACKGROUND_NONE;
    }
    if (EqualizerBackgroundService_IsAudioSafeFinalCheck(
            final_flag_ad, final_flag_da, final_flag_ad_done,
            final_frame_service_pending) == 0)
    {
        return EQ_BACKGROUND_NONE;
    }
    if ((state->consumed_frame_valid != 0) &&
        (state->consumed_frame == processed_frame))
    {
        return EQ_BACKGROUND_NONE;
    }
    if (builder_eligible != 0)
    {
        return EQ_BACKGROUND_BUILDER;
    }
    if (analyzer_eligible != 0)
    {
        return EQ_BACKGROUND_ANALYZER;
    }
    if (uart_eligible != 0)
    {
        return EQ_BACKGROUND_UART;
    }
    if (lcd_eligible != 0)
    {
        return EQ_BACKGROUND_LCD;
    }
    return EQ_BACKGROUND_NONE;
}

void EqualizerBackgroundService_Record(
    EQ_BACKGROUND_SERVICE_STATE *state,
    unsigned long processed_frame,
    int completed_kind)
{
    if ((state == 0) ||
        ((completed_kind != EQ_BACKGROUND_BUILDER) &&
         (completed_kind != EQ_BACKGROUND_ANALYZER) &&
         (completed_kind != EQ_BACKGROUND_UART) &&
         (completed_kind != EQ_BACKGROUND_LCD)))
    {
        return;
    }
    state->consumed_frame = processed_frame;
    state->consumed_frame_valid = 1;
    state->consumed_kind = completed_kind;
    state->next_preference = EQ_BACKGROUND_BUILDER;
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
    EQ_RESPONSE_SNAPSHOT active_response;
    EQ_COMMAND_SNAPSHOT command_response;
    int band;

    for (band = 0; band < EQ_NUM_BANDS; band++)
    {
        EQ_DebugBandGainDb[band] = (EQ_BoardControl.target_valid != 0) ?
            EQ_BoardControl.target_gain_db[band] :
            Equalizer_GetBandTargetGainDb(&EQ_BoardState, band);
        EQ_DebugControlShadowGainDb[band] =
            EQ_BoardControl.shadow_gain_db[band];
    }
    EQ_DebugClipCount = EQ_BoardState.clip_count;
    EQ_DebugRequestedMode =
        (EQ_BoardControl.target_valid != 0) ?
        EQ_BoardControl.target_preset : EQ_PRESET_NONE;
    EQ_DebugTransitionTargetMode =
        Equalizer_GetTransitionTargetPreset(&EQ_BoardState);
    EQ_DebugAppliedMode = Equalizer_GetAppliedPreset(&EQ_BoardState);
    EQ_DebugModeChangeCount = Equalizer_GetModeChangeCount(&EQ_BoardState);

    EQ_DebugControlCommand = EQ_ControlMailbox.command;
    EQ_DebugControlBand = EQ_ControlMailbox.band;
    EQ_DebugControlPreset = EQ_ControlMailbox.preset;
    EQ_DebugControlValueDb = EQ_ControlMailbox.value_db;
    EQ_DebugControlStepDb = EQ_ControlMailbox.step_db;
    EQ_DebugControlRequestToken = EQ_ControlMailbox.sequence;
    EQ_DebugControlObservedToken = EQ_BoardControl.observed_sequence;
    EQ_DebugControlAcceptedToken = EQ_BoardControl.accepted_sequence;
    EQ_DebugControlTargetToken = EQ_BoardControl.target_sequence;
    EQ_DebugControlPreparedToken = EQ_BoardControl.prepared_sequence;
    EQ_DebugControlReadyValid = EQ_BoardControl.ready_candidate.valid;
    EQ_DebugControlInstalledToken = EQ_BoardControl.installed_sequence;
    EQ_DebugControlAppliedToken = EQ_BoardControl.applied_sequence;
    EQ_DebugControlInstalledPairValid =
        EQ_BoardControl.installed_pair_valid;
    EQ_DebugControlRejectedCount = EQ_BoardControl.rejected_count;
    EQ_DebugControlCoalescedCount = EQ_BoardControl.coalesced_count;
    EQ_DebugControlLastError = EQ_BoardControl.last_error;
    EQ_DebugBuilderState = EQ_BoardControl.builder.state;
    EQ_DebugBuilderSectionIndex = EQ_BoardControl.builder.section_index;
    EQ_DebugBuilderScanIndex = EQ_BoardControl.builder.scan_index;
    EQ_DebugBuilderGeneration = EQ_BoardControl.builder.generation;
    EQ_DebugBuilderRequestToken =
        EQ_BoardControl.builder.request_sequence;
    EQ_DebugBuilderSliceCount =
        EQ_BoardControl.builder.payload_slice_count;
    EQ_DebugBuilderCancelCount = EQ_BoardControl.builder.cancel_count;
    EQ_DebugBuilderRestartCount = EQ_BoardControl.builder.restart_count;
    EQ_DebugBuilderLastError = EQ_BoardControl.builder.last_error;
    EQ_DebugResponseActiveGeneration =
        Equalizer_GetActiveGeneration(&EQ_BoardState);
    EQ_DebugResponseTargetGeneration = EQ_BoardControl.target_generation;
    EQ_DebugResponseTransitionActive =
        Equalizer_HasPendingTransition(&EQ_BoardState);
    if (EqualizerResponse_CopyActive(&EQ_BoardState, &active_response) != 0)
    {
        EQ_DebugResponseTransitionProgress =
            active_response.transition_progress;
        EQ_DebugResponseActivePathType = active_response.path_type;
    }
    if (EqualizerResponse_CopyCommand(
            &EQ_BoardControl, &command_response) != 0)
    {
        EQ_DebugResponseTargetValid =
            command_response.target_response_valid;
    }
    else
    {
        EQ_DebugResponseTargetValid = 0;
    }
}

#if EQ_ENABLE_LCD_DISPLAY != 0
static int EQ_UiFloatToTenths(float value)
{
    float scaled;

    if (value > 20.0f)
    {
        value = 20.0f;
    }
    else if (value < -20.0f)
    {
        value = -20.0f;
    }
    scaled = value * 10.0f;
    return (scaled >= 0.0f) ?
        (int)(scaled + 0.5f) : (int)(scaled - 0.5f);
}

#if EQ_ENABLE_TEN_BAND_EDITOR != 0
static void EQ_SyncUiEditorDebug(void)
{
    int band;

    EQ_DebugUiEditorSelectedBand = EQ_UiEditorState.selected_band;
    EQ_DebugUiEditorDraftDirty = EQ_UiEditorState.draft_dirty;
    EQ_DebugUiEditorSubmittedValid = EQ_UiEditorState.submitted_valid;
    EQ_DebugUiEditorApplyStatus = EQ_UiEditorState.apply_status;
    EQ_DebugUiEditorSubmittedSequence =
        EQ_UiEditorState.submitted_sequence;
    EQ_DebugUiEditorAppliedSequence = EQ_UiEditorState.applied_sequence;
    for (band = 0; band < EQ_NUM_BANDS; band++)
    {
        EQ_DebugUiEditorDraftGainHalfDb[band] =
            EQ_UiEditorState.draft_gain_half_db[band];
        EQ_DebugUiEditorSubmittedGainHalfDb[band] =
            EQ_UiEditorState.submitted_gain_half_db[band];
        EQ_DebugUiEditorAppliedGainHalfDb[band] =
            EQ_UiEditorState.applied_gain_half_db[band];
    }
}

static int EQ_UiEditorDraftMatchesSubmitted(void)
{
    int band;

    if (EQ_UiEditorState.submitted_valid == 0U)
    {
        return 0;
    }
    for (band = 0; band < EQ_NUM_BANDS; band++)
    {
        if (EQ_UiEditorState.draft_gain_half_db[band] !=
            EQ_UiEditorState.submitted_gain_half_db[band])
        {
            return 0;
        }
    }
    return 1;
}

static void EQ_UpdateUiEditorFeedback(void)
{
    EQ_UI_GAIN_HALF_DB gains[EQ_NUM_BANDS];
    unsigned long active_generation;
    EQ_CONTROL_SEQUENCE applied_sequence;
    int applied_mode;
    int builder_state;
    int status;
    int band;

    active_generation = Equalizer_GetActiveGeneration(&EQ_BoardState);
    applied_sequence = EQ_BoardControl.applied_sequence;
    applied_mode = Equalizer_GetAppliedPreset(&EQ_BoardState);
    if ((EQ_UiEventVersion.initialized == 0U) ||
        (active_generation != EQ_UiEventVersion.active_generation) ||
        (applied_sequence != EQ_UiEventVersion.applied_sequence) ||
        (applied_mode != EQ_UiEventVersion.applied_mode))
    {
        for (band = 0; band < EQ_NUM_BANDS; band++)
        {
            gains[band] = EqualizerUi_GainDbToHalf(
                EQ_BoardState.active_gain_db[band]);
        }
        EqualizerUiEditor_ObserveApplied(
            &EQ_UiEditorState, gains, applied_mode, applied_sequence);
        EQ_DebugUiAppliedGainRefreshCount++;
    }

    status = EQ_UI_APPLY_APPLIED;
    if ((EQ_UiEditorState.draft_dirty != 0U) &&
        (EQ_UiEditorDraftMatchesSubmitted() == 0))
    {
        status = EQ_UI_APPLY_EDITING;
    }
    else if ((EQ_BoardControl.last_error != EQ_CONTROL_ERROR_NONE) ||
             (EQ_BoardControl.builder.last_error !=
              EQ_CONTROL_ERROR_NONE))
    {
        status = EQ_UI_APPLY_ERROR;
    }
    else if ((EQ_UiEditorState.submitted_valid != 0U) &&
             (EQ_UiEditorState.applied_sequence ==
              EQ_UiEditorState.submitted_sequence))
    {
        status = EQ_UI_APPLY_APPLIED;
    }
    else if ((EQ_BoardControl.installed_sequence ==
              EQ_UiEditorState.submitted_sequence) &&
             Equalizer_HasPendingTransition(&EQ_BoardState))
    {
        status = EQ_UI_APPLY_TRANSITION;
    }
    else if ((EQ_BoardControl.ready_candidate.valid != 0) &&
             (EQ_BoardControl.ready_candidate.request_sequence ==
              EQ_UiEditorState.submitted_sequence))
    {
        status = EQ_UI_APPLY_READY;
    }
    else
    {
        builder_state = EQ_BoardControl.builder.state;
        if ((EQ_BoardControl.builder.request_sequence ==
             EQ_UiEditorState.submitted_sequence) &&
            ((builder_state == EQ_BUILDER_DESIGN_SECTION) ||
             (builder_state == EQ_BUILDER_SCAN_HEADROOM) ||
             (builder_state == EQ_BUILDER_FINALIZE)))
        {
            status = EQ_UI_APPLY_BUILDING;
        }
        else
        {
            status = EQ_UI_APPLY_QUEUED;
        }
    }
    EqualizerUiEditor_SetApplyStatus(&EQ_UiEditorState, status);
    EQ_DebugUiDraftVersion = EQ_UiEditorState.draft_version;
    EQ_SyncUiEditorDebug();
    EQ_DebugUiDisplayedPage = EqualizerDisplay_GetDisplayedPage();
    EQ_DebugUiPageBuilding =
        (unsigned int)EqualizerDisplay_IsPageBuilding();
}

static int EQ_UiSnapshotEventChanged(void)
{
    int requested_page;

    requested_page = (EQ_DebugUiRequestedPage == EQ_UI_PAGE_EQ_EDITOR) ?
        EQ_UI_PAGE_EQ_EDITOR : EQ_UI_PAGE_DYNAMIC_STATUS;
    if (EQ_DebugUiRequestedPage != requested_page)
    {
        EQ_DebugUiRequestedPage = requested_page;
    }
    if (EQ_UiEventVersion.initialized == 0U)
        return 1;
    if (EQ_UiEventVersion.analyzer_analysis_count !=
        EQ_DebugAnalyzerAnalysisCount)
        return 1;
    if (EQ_UiEventVersion.active_generation !=
        Equalizer_GetActiveGeneration(&EQ_BoardState))
        return 1;
    if (EQ_UiEventVersion.applied_sequence !=
        EQ_BoardControl.applied_sequence)
        return 1;
    if (EQ_UiEventVersion.submitted_sequence !=
        EQ_UiEditorState.submitted_sequence)
        return 1;
    if (EQ_UiEventVersion.draft_version != EQ_UiEditorState.draft_version)
        return 1;
    if (EQ_UiEventVersion.applied_mode != EQ_DebugAppliedMode)
        return 1;
    if ((EQ_UiEventVersion.smart_enabled !=
         (int)EQ_DebugSmartBassEnabled) ||
        (EQ_UiEventVersion.smart_strength != EQ_DebugSmartBassStrength) ||
        (EQ_UiEventVersion.smart_active !=
         (int)EQ_DebugSmartBassProcessingActive))
        return 1;
    if ((EQ_UiEventVersion.clarity_enabled !=
         (int)EQ_DebugDynamicClarityEnabled) ||
        (EQ_UiEventVersion.clarity_strength !=
         EQ_DebugDynamicClarityStrength) ||
        (EQ_UiEventVersion.clarity_active !=
         (int)EQ_DebugDynamicClarityProcessingActive))
        return 1;
    if ((EQ_UiEventVersion.guard_enabled !=
         (int)EQ_DebugHarshnessGuardEnabled) ||
        (EQ_UiEventVersion.guard_strength !=
         EQ_DebugHarshnessGuardStrength) ||
        (EQ_UiEventVersion.guard_active !=
         (int)EQ_DebugHarshnessGuardProcessingActive))
        return 1;
    if ((EQ_UiEventVersion.page != requested_page) ||
        (EQ_UiEventVersion.selected_band !=
         EQ_UiEditorState.selected_band) ||
        (EQ_UiEventVersion.apply_status != EQ_UiEditorState.apply_status))
        return 1;
    return 0;
}

static void EQ_RecordUiSnapshotEvent(void)
{
    EQ_UiEventVersion.analyzer_analysis_count =
        EQ_DebugAnalyzerAnalysisCount;
    EQ_UiEventVersion.active_generation =
        Equalizer_GetActiveGeneration(&EQ_BoardState);
    EQ_UiEventVersion.applied_sequence = EQ_BoardControl.applied_sequence;
    EQ_UiEventVersion.submitted_sequence =
        EQ_UiEditorState.submitted_sequence;
    EQ_UiEventVersion.draft_version = EQ_UiEditorState.draft_version;
    EQ_UiEventVersion.applied_mode = EQ_DebugAppliedMode;
    EQ_UiEventVersion.smart_enabled = (int)EQ_DebugSmartBassEnabled;
    EQ_UiEventVersion.smart_strength = EQ_DebugSmartBassStrength;
    EQ_UiEventVersion.smart_active =
        (int)EQ_DebugSmartBassProcessingActive;
    EQ_UiEventVersion.clarity_enabled =
        (int)EQ_DebugDynamicClarityEnabled;
    EQ_UiEventVersion.clarity_strength = EQ_DebugDynamicClarityStrength;
    EQ_UiEventVersion.clarity_active =
        (int)EQ_DebugDynamicClarityProcessingActive;
    EQ_UiEventVersion.guard_enabled =
        (int)EQ_DebugHarshnessGuardEnabled;
    EQ_UiEventVersion.guard_strength = EQ_DebugHarshnessGuardStrength;
    EQ_UiEventVersion.guard_active =
        (int)EQ_DebugHarshnessGuardProcessingActive;
    EQ_UiEventVersion.page = EQ_DebugUiRequestedPage;
    EQ_UiEventVersion.selected_band = EQ_UiEditorState.selected_band;
    EQ_UiEventVersion.apply_status = EQ_UiEditorState.apply_status;
    EQ_UiEventVersion.initialized = 1U;
}
#endif

static void EQ_BuildUiSnapshot(EQ_UI_SNAPSHOT *snapshot)
{
    int tenths;

    memset(snapshot, 0, sizeof(*snapshot));
    snapshot->applied_preset = EQ_DebugAppliedMode;
    snapshot->smart_enabled =
        (EQ_DebugSmartBassEnabled != 0U) ? 1 : 0;
    snapshot->smart_strength = EQ_DebugSmartBassStrength;
    snapshot->smart_active = (int)EQ_DebugSmartBassProcessingActive;
    snapshot->clarity_enabled =
        (EQ_DebugDynamicClarityEnabled != 0U) ? 1 : 0;
    snapshot->clarity_strength = EQ_DebugDynamicClarityStrength;
    snapshot->clarity_active =
        (int)EQ_DebugDynamicClarityProcessingActive;
    snapshot->guard_enabled =
        (EQ_DebugHarshnessGuardEnabled != 0U) ? 1 : 0;
    snapshot->guard_strength = EQ_DebugHarshnessGuardStrength;
    snapshot->guard_active =
        (int)EQ_DebugHarshnessGuardProcessingActive;
    snapshot->analyzer_valid = (EQ_DebugAnalyzerValid != 0U) ? 1 : 0;
    snapshot->analyzer_warm = (EQ_DebugAnalyzerWarmup != 0U) ? 1 : 0;

    tenths = EQ_UiFloatToTenths(EQ_DebugAnalyzerBassDb);
    snapshot->band_pixel[0] = EqualizerUi_DbTenthsToPixel(tenths);
    tenths = EQ_UiFloatToTenths(EQ_DebugAnalyzerMudDb);
    snapshot->band_pixel[1] = EqualizerUi_DbTenthsToPixel(tenths);
    tenths = EQ_UiFloatToTenths(EQ_DebugAnalyzerPresenceDb);
    snapshot->band_pixel[2] = EqualizerUi_DbTenthsToPixel(tenths);
    tenths = EQ_UiFloatToTenths(EQ_DebugAnalyzerBrightnessDb);
    snapshot->band_pixel[3] = EqualizerUi_DbTenthsToPixel(tenths);
#if EQ_ENABLE_TEN_BAND_EDITOR != 0
    snapshot->page = EQ_DebugUiRequestedPage;
    EqualizerUiEditor_CopyToSnapshot(&EQ_UiEditorState, snapshot);
#endif
}

#if EQ_ENABLE_TEN_BAND_EDITOR != 0
static void EQ_RequestUiSnapshotIfChanged(EQ_UI_SNAPSHOT *snapshot,
                                          unsigned long process_frame,
                                          int force)
{
    EQ_UpdateUiEditorFeedback();
    if ((force == 0) &&
        (EQ_UiSnapshotLastRequestFrame == process_frame))
    {
        EQ_DebugUiSnapshotSkippedCount++;
        return;
    }
    if ((force == 0) && (EQ_UiSnapshotEventChanged() == 0))
    {
        EQ_DebugUiSnapshotSkippedCount++;
        return;
    }
    EQ_BuildUiSnapshot(snapshot);
    EqualizerDisplay_RequestSnapshot(snapshot, process_frame);
    EQ_UiSnapshotLastRequestFrame = process_frame;
    EQ_RecordUiSnapshotEvent();
    EQ_DebugUiSnapshotBuildCount++;
}
#endif
#endif

#if EQ_ENABLE_PROJECT33_TOUCH != 0
static int EQ_NextUiStrength(int strength)
{
    if (strength <= 1)
    {
        return 2;
    }
    if (strength == 2)
    {
        return 3;
    }
    return 1;
}

static void EQ_RecordAcceptedTouchAction(int action)
{
    if (EQ_TouchAcceptedThisPress != 0U)
    {
        EQ_DebugTouchDuplicateActionCount++;
    }
    EQ_TouchAcceptedThisPress = 1U;

    if (EqualizerUi_ActionToPreset(action) != EQ_PRESET_NONE)
    {
        EQ_DebugTouchPresetRequestCount++;
    }
    else if ((action == EQ_UI_ACTION_SMART_TOGGLE) ||
             (action == EQ_UI_ACTION_CLARITY_TOGGLE) ||
             (action == EQ_UI_ACTION_GUARD_TOGGLE))
    {
        EQ_DebugTouchDynamicEnableRequestCount++;
    }
    else if ((action == EQ_UI_ACTION_SMART_STRENGTH) ||
             (action == EQ_UI_ACTION_CLARITY_STRENGTH) ||
             (action == EQ_UI_ACTION_GUARD_STRENGTH))
    {
        EQ_DebugTouchDynamicStrengthRequestCount++;
    }
#if EQ_ENABLE_TEN_BAND_EDITOR_TOUCH != 0
    else if ((action >= EQ_UI_ACTION_EDITOR_BAND_0) &&
             (action <= EQ_UI_ACTION_EDITOR_RESET_FLAT))
    {
        EQ_DebugTouchEditorActionCount++;
    }
#endif
}

#if EQ_ENABLE_TEN_BAND_EDITOR_TOUCH != 0
static int EQ_SubmitUiEditorRequest(int reset_flat);
#endif

static int EQ_ApplyUiAction(int action)
{
    int preset;

    preset = EqualizerUi_ActionToPreset(action);
    if (preset != EQ_PRESET_NONE)
    {
        EQ_DebugDiagPath = EQ_DIAG_PRESET;
        EQ_DebugMode = preset;
        return 1;
    }
#if EQ_ENABLE_TEN_BAND_EDITOR_TOUCH != 0
    if (action == EQ_UI_ACTION_PAGE_SWITCH)
    {
        EQ_DebugUiRequestedPage =
            (EQ_DebugUiRequestedPage == EQ_UI_PAGE_EQ_EDITOR) ?
            EQ_UI_PAGE_DYNAMIC_STATUS : EQ_UI_PAGE_EQ_EDITOR;
        EQ_DebugUiPageSwitchCount++;
        return 1;
    }
    if ((action >= EQ_UI_ACTION_EDITOR_BAND_0) &&
        (action <= EQ_UI_ACTION_EDITOR_BAND_9))
    {
        (void)EqualizerUiEditor_SelectBand(
            &EQ_UiEditorState,
            action - EQ_UI_ACTION_EDITOR_BAND_0);
        return 1;
    }
    if (action == EQ_UI_ACTION_EDITOR_MINUS)
    {
        (void)EqualizerUiEditor_StepSelected(&EQ_UiEditorState, -1);
        return 1;
    }
    if (action == EQ_UI_ACTION_EDITOR_PLUS)
    {
        (void)EqualizerUiEditor_StepSelected(&EQ_UiEditorState, 1);
        return 1;
    }
    if (action == EQ_UI_ACTION_EDITOR_APPLY)
    {
        return EQ_SubmitUiEditorRequest(0);
    }
    if (action == EQ_UI_ACTION_EDITOR_RESET_FLAT)
    {
        return EQ_SubmitUiEditorRequest(1);
    }
#endif
    if (action == EQ_UI_ACTION_SMART_TOGGLE)
    {
        EQ_DebugSmartBassEnabled =
            (EQ_DebugSmartBassEnabled == 0U) ? 1U : 0U;
        return 1;
    }
    if (action == EQ_UI_ACTION_SMART_STRENGTH)
    {
        EQ_DebugSmartBassStrength =
            EQ_NextUiStrength(EQ_DebugSmartBassStrength);
        return 1;
    }
    if (action == EQ_UI_ACTION_CLARITY_TOGGLE)
    {
        EQ_DebugDynamicClarityEnabled =
            (EQ_DebugDynamicClarityEnabled == 0U) ? 1U : 0U;
        return 1;
    }
    if (action == EQ_UI_ACTION_CLARITY_STRENGTH)
    {
        EQ_DebugDynamicClarityStrength =
            EQ_NextUiStrength(EQ_DebugDynamicClarityStrength);
        return 1;
    }
    if (action == EQ_UI_ACTION_GUARD_TOGGLE)
    {
        EQ_DebugHarshnessGuardEnabled =
            (EQ_DebugHarshnessGuardEnabled == 0U) ? 1U : 0U;
        return 1;
    }
    if (action == EQ_UI_ACTION_GUARD_STRENGTH)
    {
        EQ_DebugHarshnessGuardStrength =
            EQ_NextUiStrength(EQ_DebugHarshnessGuardStrength);
        return 1;
    }
    return 0;
}

static int EQ_ServiceUiTouch(unsigned char flag_ad_done,
                             unsigned long process_frame,
                             int *accepted_action)
{
    TouchScanResult result;
    unsigned int audio_before;
    int screen_x;
    int screen_y;
    int rejected;
    int action;
    int invalid_coordinate;
#if defined(__TI_COMPILER_VERSION__) || defined(__TMS320C6X__)
    unsigned int cycle_start;
#endif

    *accepted_action = EQ_UI_ACTION_NONE;
    if ((FLAG_TOUCH == 0U) && (EQ_TouchState.press_latched == 0U))
    {
        return 0;
    }
    if (EQ_TouchLastServiceFrame == process_frame)
    {
        return 0;
    }
    audio_before = ((FLAG_AD != 0) ? 0x01U : 0U) |
                   ((FLAG_DA != 0) ? 0x02U : 0U) |
                   ((flag_ad_done != 0) ? 0x04U : 0U) |
                   ((EQ_FrameServicePending != 0U) ? 0x08U : 0U);
    if (audio_before != 0U)
    {
        return 0;
    }
#if defined(__TI_COMPILER_VERSION__) || defined(__TMS320C6X__)
    cycle_start = TSCL;
#endif
    FLAG_TOUCH = 0U;
    result = Touch_ScanRaw();
    EQ_TouchLastServiceFrame = process_frame;
#if defined(__TI_COMPILER_VERSION__) || defined(__TMS320C6X__)
    EQ_DebugTouchLastCycles = (unsigned long)(TSCL - cycle_start);
    if (EQ_DebugTouchLastCycles > EQ_DebugTouchMaxCycles)
    {
        EQ_DebugTouchMaxCycles = EQ_DebugTouchLastCycles;
    }
#else
    EQ_DebugTouchLastCycles = 0UL;
#endif
    if (result == TOUCH_SCAN_RELEASE)
    {
        EQ_DebugTouchPressed = 0U;
        EQ_TouchAcceptedThisPress = 0U;
        (void)EqualizerUiTouch_Process(
            &EQ_TouchState, EqualizerDisplay_GetDisplayedPage(),
            EqualizerDisplay_IsPageBuilding(),
            0, EQ_DebugTouchScreenX,
            EQ_DebugTouchScreenY, &rejected);
        return 1;
    }
    if (result == TOUCH_SCAN_ERROR)
    {
        EQ_DebugTouchRejectedCount++;
        return 1;
    }
    if (result != TOUCH_SCAN_DOWN)
    {
        return 1;
    }

    EQ_DebugTouchRawX = Touch_X;
    EQ_DebugTouchRawY = Touch_Y;
    invalid_coordinate =
        (Touch_X < EQ_TouchTransform.raw_x_min) ||
        (Touch_X > EQ_TouchTransform.raw_x_max) ||
        (Touch_Y < EQ_TouchTransform.raw_y_min) ||
        (Touch_Y > EQ_TouchTransform.raw_y_max);
    if (EqualizerUi_MapTouchRawToScreen(
            &EQ_TouchTransform, Touch_X, Touch_Y,
            &screen_x, &screen_y) == 0)
    {
        EQ_DebugTouchInvalidCoordinateCount++;
        EQ_DebugTouchRejectedCount++;
        return 1;
    }
    if (invalid_coordinate != 0)
    {
        EQ_DebugTouchInvalidCoordinateCount++;
    }
    EQ_DebugTouchScreenX = screen_x;
    EQ_DebugTouchScreenY = screen_y;
    EQ_DebugTouchPressed = 1U;
    rejected = 0;
    action = EqualizerUiTouch_Process(
        &EQ_TouchState, EqualizerDisplay_GetDisplayedPage(),
        EqualizerDisplay_IsPageBuilding(),
        1, screen_x, screen_y, &rejected);
    if (rejected != 0)
    {
        EQ_DebugTouchRejectedCount++;
    }
    if (action == EQ_UI_ACTION_NONE)
    {
        return 1;
    }
    if (EQ_TouchLastActionFrame == process_frame)
    {
        EQ_DebugTouchRejectedCount++;
        return 1;
    }
    if (EQ_ApplyUiAction(action) == 0)
    {
        EQ_DebugTouchRejectedCount++;
        return 1;
    }
#if EQ_ENABLE_TEN_BAND_EDITOR != 0
    EQ_SyncUiEditorDebug();
#endif
    EQ_TouchLastActionFrame = process_frame;
    EQ_DebugTouchLastAction = action;
    if ((action > EQ_UI_ACTION_NONE) &&
        ((unsigned int)action < EQ_TOUCH_ACTION_DIAGNOSTIC_COUNT))
    {
        EQ_DebugTouchActionHistogram[action]++;
    }
    EQ_RecordAcceptedTouchAction(action);
    EQ_DebugTouchActionCount++;
    *accepted_action = action;
    return 1;
}
#endif

static void EQ_FillControlRequest(EQ_CONTROL_REQUEST *request)
{
    int band;

    memset(request, 0, sizeof(*request));
    request->preset = EQ_PRESET_NONE;
    for (band = 0; band < EQ_NUM_BANDS; band++)
    {
        request->shadow_gain_db[band] =
            EQ_BoardControl.shadow_gain_db[band];
    }
}

#if EQ_ENABLE_TEN_BAND_EDITOR_TOUCH != 0
static int EQ_SubmitUiEditorRequest(int reset_flat)
{
    EQ_CONTROL_REQUEST request;
    EQ_CONTROL_SEQUENCE submitted;

    if ((EQ_AppliedDiagPath != EQ_DIAG_PRESET) ||
        (EQ_NormalizeDiagPath(EQ_DebugDiagPath) != EQ_DIAG_PRESET))
    {
        EqualizerUiEditor_SetApplyStatus(
            &EQ_UiEditorState, EQ_UI_APPLY_ERROR);
        return 0;
    }
    if (reset_flat != 0)
    {
        (void)EqualizerUiEditor_SetDraftFlat(&EQ_UiEditorState);
    }
    else if (EqualizerUiEditor_HasSubmittableDraft(
                 &EQ_UiEditorState) == 0)
    {
        return 1;
    }
    EQ_FillControlRequest(&request);
    if (reset_flat != 0)
    {
        request.command = EQ_CONTROL_RESET_FLAT;
        request.preset = EQ_PRESET_FLAT;
    }
    else
    {
        request.command = EQ_CONTROL_SET_ALL;
        request.preset = EQ_PRESET_CUSTOM;
        EqualizerUiEditor_CopyDraftDb(
            &EQ_UiEditorState, request.shadow_gain_db);
    }
    submitted = EqualizerControl_SubmitRequest(
        &EQ_ControlMailbox, &request);
    if (submitted == 0U)
    {
        EqualizerUiEditor_SetApplyStatus(
            &EQ_UiEditorState, EQ_UI_APPLY_ERROR);
        return 0;
    }
    EqualizerUiEditor_MarkSubmitted(&EQ_UiEditorState, submitted);
    return 1;
}
#endif

static int EQ_ServiceMode(void)
{
    EQ_CONTROL_REQUEST request;
    EQ_CONTROL_SEQUENCE submitted;
    int diag_path;
    int mode;
    int submit_target;

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
    if ((diag_path == EQ_DIAG_RAW_COPY) ||
        (diag_path == EQ_DIAG_FLOAT_COPY))
    {
        if (diag_path != EQ_AppliedDiagPath)
        {
            Equalizer_SetBypass(&EQ_BoardState, 0);
            Equalizer_SetCoreMode(&EQ_BoardState,
                (diag_path == EQ_DIAG_RAW_COPY) ?
                EQ_CORE_RAW_COPY : EQ_CORE_FLOAT_COPY);
            EqualizerControl_RebaseAfterDirectPathChange(
                &EQ_BoardControl, &EQ_BoardState);
            EQ_LastServicedMode = EQ_PRESET_NONE;
            EQ_AppliedDiagPath = diag_path;
        }
        EQ_UpdateDebugGains();
        return 1;
    }

    submit_target = (diag_path != EQ_AppliedDiagPath) ? 1 : 0;
    if ((diag_path == EQ_DIAG_PRESET) &&
        (mode != EQ_LastServicedMode))
    {
        submit_target = 1;
    }
    if (submit_target != 0)
    {
        Equalizer_SetBypass(&EQ_BoardState, 0);
        Equalizer_SetCoreMode(&EQ_BoardState, EQ_CORE_RBJ_CASCADE);
        if ((EQ_AppliedDiagPath == EQ_DIAG_RAW_COPY) ||
            (EQ_AppliedDiagPath == EQ_DIAG_FLOAT_COPY))
        {
            Equalizer_SetIdentityHold(&EQ_BoardState, 1);
        }
        EQ_FillControlRequest(&request);
        if (diag_path == EQ_DIAG_FLAT)
        {
            request.command = EQ_CONTROL_RESET_FLAT;
            EQ_LastServicedMode = EQ_PRESET_FLAT;
        }
        else if (diag_path == EQ_DIAG_SINGLE_BAND)
        {
            int band;

            request.command = EQ_CONTROL_SET_ALL;
            request.preset = EQ_PRESET_CUSTOM;
            for (band = 0; band < EQ_NUM_BANDS; band++)
            {
                request.shadow_gain_db[band] = 0.0f;
            }
            request.shadow_gain_db[5] = 3.0f;
            EQ_LastServicedMode = EQ_PRESET_NONE;
        }
        else
        {
            request.command = EQ_CONTROL_APPLY_PRESET;
            request.preset = mode;
            EQ_LastServicedMode = mode;
        }
        submitted = EqualizerControl_SubmitRequest(
            &EQ_ControlMailbox, &request);
        if (submitted != 0U)
        {
            EQ_AppliedDiagPath = diag_path;
        }
    }
    EQ_UpdateDebugGains();
    return 0;
}

static void EQ_UpdateModeServiceTiming(unsigned long cycles)
{
    EQ_DebugModeServiceLastCycles = cycles;
    if (cycles > EQ_DebugModeServiceMaxCycles)
    {
        EQ_DebugModeServiceMaxCycles = cycles;
    }
}

static int EQ_ServiceModeTimed(void)
{
    int direct_path;
#if defined(__TI_COMPILER_VERSION__) || defined(__TMS320C6X__)
    unsigned int cycle_start = TSCL;

    direct_path = EQ_ServiceMode();
    EQ_UpdateModeServiceTiming((unsigned long)(TSCL - cycle_start));
#else
    direct_path = EQ_ServiceMode();
    EQ_UpdateModeServiceTiming(0UL);
#endif
    return direct_path;
}

static void EQ_UpdateBuilderTiming(unsigned long cycles)
{
    EQ_DebugBuilderLastCycles = cycles;
    EQ_DebugBuilderLastMs = (float)cycles / EQ_CPU_CYCLES_PER_MS;
    if (cycles > EQ_DebugBuilderMaxCycles)
    {
        EQ_DebugBuilderMaxCycles = cycles;
        EQ_DebugBuilderMaxMs = EQ_DebugBuilderLastMs;
    }
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

#if EQ_ENABLE_SMART_BASS != 0
static void EQ_SyncSmartBassDebug(void)
{
    EQ_DebugSmartBassProcessingActive =
        (unsigned int)EQ_SmartBassState.processing_active;
    EQ_DebugSmartBassInputBassDb =
        EQ_SmartBassState.latest_bass_relative_db;
    EQ_DebugSmartBassInputRmsDbfs = EQ_SmartBassState.latest_rms_dbfs;
    EQ_DebugSmartBassRequestedLevel = EQ_SmartBassState.target_level;
    EQ_DebugSmartBassAppliedLevel = EQ_SmartBassState.active_level;
    EQ_DebugSmartBassPendingLevel = EQ_SmartBassState.pending_level;
    EQ_DebugSmartBassRequestedGainDb =
        SmartBass_GetRequestedGainDb(&EQ_SmartBassState);
    EQ_DebugSmartBassAppliedGainDb =
        SmartBass_GetAppliedGainDb(&EQ_SmartBassState);
    EQ_DebugSmartBassTransitionActive =
        (unsigned int)EQ_SmartBassState.transition_active;
    EQ_DebugSmartBassTransitionProgress =
        SmartBass_GetTransitionProgress(&EQ_SmartBassState);
    EQ_DebugSmartBassDecisionCount = EQ_SmartBassState.decision_count;
    EQ_DebugSmartBassLevelChangeCount =
        EQ_SmartBassState.level_change_count;
    EQ_DebugSmartBassTransitionCount =
        EQ_SmartBassState.transition_count;
    EQ_DebugSmartBassInvalidReleaseCount =
        EQ_SmartBassState.invalid_release_count;
    EQ_DebugSmartBassSaturationCount =
        EQ_SmartBassState.saturation_count;
    EQ_DebugSmartBassNonFiniteCount =
        EQ_SmartBassState.nonfinite_count;
    EQ_DebugSmartBassReason = SmartBass_GetReason(&EQ_SmartBassState);
}

static void EQ_ServiceSmartBassRuntimeControl(void)
{
    int requested_enabled;

    EQ_DebugSmartBassEnabled =
        (EQ_DebugSmartBassEnabled != 0U) ? 1U : 0U;
    (void)SmartBass_SetStrength(
        &EQ_SmartBassState, EQ_DebugSmartBassStrength);
    EQ_DebugSmartBassStrength = EQ_SmartBassState.strength;
    requested_enabled = ((EQ_DebugSmartBassEnabled != 0U) &&
                         (EQ_DebugAnalyzerEnabled != 0U) &&
                         (EQ_AnalyzerLastEnabled != 0U) &&
                         (EQ_SmartBassAnalyzerFault == 0U)) ? 1 : 0;
    (void)SmartBass_SetEnabled(&EQ_SmartBassState, requested_enabled);
}

static void EQ_MarkSmartBassAnalyzerUnavailable(void)
{
    EQ_SmartBassAnalyzerFault = 1U;
    if (EQ_SmartBassState.initialized != 0)
    {
        SmartBass_InvalidateAnalysisEpoch(&EQ_SmartBassState);
        (void)SmartBass_SetEnabled(&EQ_SmartBassState, 0);
        EQ_SyncSmartBassDebug();
    }
}

static void EQ_UpdateSmartBassTiming(unsigned long cycles)
{
    EQ_DebugSmartBassLastCycles = cycles;
    if (cycles > EQ_DebugSmartBassMaxCycles)
    {
        EQ_DebugSmartBassMaxCycles = cycles;
    }
}
#endif

#if EQ_ENABLE_DYNAMIC_CLARITY != 0
static void EQ_SyncDynamicClarityDebug(void)
{
    EQ_DebugDynamicClarityProcessingActive =
        (unsigned int)EQ_DynamicClarityState.processing_active;
    EQ_DebugDynamicClarityMudDb =
        EQ_DynamicClarityState.latest_mud_relative_db;
    EQ_DebugDynamicClarityPresenceDb =
        EQ_DynamicClarityState.latest_presence_relative_db;
    EQ_DebugDynamicClarityMaskingDb =
        EQ_DynamicClarityState.latest_masking_db;
    EQ_DebugDynamicClarityRmsDbfs =
        EQ_DynamicClarityState.latest_rms_dbfs;
    EQ_DebugDynamicClarityRequestedLevel =
        EQ_DynamicClarityState.target_level;
    EQ_DebugDynamicClarityAppliedLevel =
        EQ_DynamicClarityState.active_level;
    EQ_DebugDynamicClarityPendingLevel =
        EQ_DynamicClarityState.pending_level;
    EQ_DebugDynamicClarityRequestedGainDb =
        DynamicClarity_GetRequestedGainDb(&EQ_DynamicClarityState);
    EQ_DebugDynamicClarityAppliedGainDb =
        DynamicClarity_GetAppliedGainDb(&EQ_DynamicClarityState);
    EQ_DebugDynamicClarityTransitionActive =
        (unsigned int)EQ_DynamicClarityState.transition_active;
    EQ_DebugDynamicClarityTransitionProgress =
        DynamicClarity_GetTransitionProgress(&EQ_DynamicClarityState);
    EQ_DebugDynamicClarityReason =
        DynamicClarity_GetReason(&EQ_DynamicClarityState);
    EQ_DebugDynamicClarityDecisionCount =
        EQ_DynamicClarityState.decision_count;
    EQ_DebugDynamicClarityLevelChangeCount =
        EQ_DynamicClarityState.level_change_count;
    EQ_DebugDynamicClarityTransitionCount =
        EQ_DynamicClarityState.transition_count;
    EQ_DebugDynamicClarityInvalidReleaseCount =
        EQ_DynamicClarityState.invalid_release_count;
    EQ_DebugDynamicClaritySaturationCount =
        EQ_DynamicClarityState.saturation_count;
    EQ_DebugDynamicClarityNonFiniteCount =
        EQ_DynamicClarityState.nonfinite_count;
}

static void EQ_ServiceDynamicClarityRuntimeControl(void)
{
    int requested_enabled;

    EQ_DebugDynamicClarityEnabled =
        (EQ_DebugDynamicClarityEnabled != 0U) ? 1U : 0U;
    (void)DynamicClarity_SetStrength(
        &EQ_DynamicClarityState, EQ_DebugDynamicClarityStrength);
    EQ_DebugDynamicClarityStrength = EQ_DynamicClarityState.strength;
    requested_enabled =
        ((EQ_DebugDynamicClarityEnabled != 0U) &&
         (EQ_DebugAnalyzerEnabled != 0U) &&
         (EQ_AnalyzerLastEnabled != 0U) &&
         (EQ_DynamicClarityAnalyzerFault == 0U)) ? 1 : 0;
    (void)DynamicClarity_SetEnabled(
        &EQ_DynamicClarityState, requested_enabled);
}

static void EQ_MarkDynamicClarityAnalyzerUnavailable(void)
{
    EQ_DynamicClarityAnalyzerFault = 1U;
    if (EQ_DynamicClarityState.initialized != 0)
    {
        DynamicClarity_InvalidateAnalysisEpoch(
            &EQ_DynamicClarityState);
        (void)DynamicClarity_SetEnabled(&EQ_DynamicClarityState, 0);
        EQ_SyncDynamicClarityDebug();
    }
}

static void EQ_UpdateDynamicClarityTiming(unsigned long cycles)
{
    EQ_DebugDynamicClarityLastCycles = cycles;
    if (cycles > EQ_DebugDynamicClarityMaxCycles)
    {
        EQ_DebugDynamicClarityMaxCycles = cycles;
    }
}

#if EQ_ENABLE_DYNAMIC_CLARITY_TRANSITION_CAPTURE != 0
static void EQ_ServiceDynamicClarityTransitionCapture(void)
{
    int result;

    if ((EQ_DebugDynamicClarityTransitionCaptureRequest != 0U) &&
        (EQ_DebugDynamicClarityTransitionCaptureActive == 0U))
    {
        EqualizerCapture_Reset();
        result = DynamicClarity_DiagnosticForceStableLevel(
            &EQ_DynamicClarityState,
            EQ_DebugDynamicClarityTransitionCaptureBaseLevel);
        EQ_DebugDynamicClarityTransitionCaptureResult = result;
        EQ_DebugDynamicClarityTransitionCaptureRequest = 0U;
        EQ_DebugDynamicClarityTransitionCaptureDone = 0U;
        EQ_DebugDynamicClarityTransitionCaptureOverride = 1U;
        EQ_DebugDynamicClarityTransitionCaptureActive = 1U;
        EQ_DebugDynamicClarityTransitionCaptureRequestFrame =
            EQ_DebugProcessFrames + 1UL;
        EQ_DebugDynamicClarityTransitionCaptureTriggerFrame = 0UL;
        EQ_DebugDynamicClarityTransitionCaptureDoneFrame = 0UL;
        EQ_DynamicClarityCapturePrerollRemaining =
            EQ_DYNAMIC_CLARITY_CAPTURE_PREROLL_FRAMES;
        EQ_DynamicClarityCapturePhase = 1U;
        return;
    }
    if (EQ_DebugDynamicClarityTransitionCaptureActive == 0U)
    {
        return;
    }

    if (EQ_DynamicClarityCapturePhase == 1U)
    {
        if (EQ_DynamicClarityCapturePrerollRemaining != 0U)
        {
            EQ_DynamicClarityCapturePrerollRemaining--;
        }
        if (EQ_DynamicClarityCapturePrerollRemaining == 0U)
        {
            EQ_TriggerCaptureRequest =
                EQ_CAPTURE_TRIGGER_DYNAMIC_CLARITY;
            EQ_DynamicClarityCapturePhase = 2U;
        }
        return;
    }

    if ((EQ_DynamicClarityCapturePhase == 2U) &&
        (EQ_TriggerCaptureArmedReady != 0U))
    {
        if (EQ_DebugDynamicClarityTransitionCaptureTargetLevel !=
            EQ_DebugDynamicClarityTransitionCaptureBaseLevel)
        {
            result = DynamicClarity_DiagnosticRequestLevel(
                &EQ_DynamicClarityState,
                EQ_DebugDynamicClarityTransitionCaptureTargetLevel);
            EQ_DebugDynamicClarityTransitionCaptureResult = result;
            if (result != DYNAMIC_CLARITY_RESULT_UPDATED)
            {
                EQ_DebugDynamicClarityTransitionCaptureActive = 0U;
                EQ_DynamicClarityCapturePhase = 0U;
                return;
            }
        }
        EqualizerCapture_NotifyEvent(
            EQ_CAPTURE_TRIGGER_DYNAMIC_CLARITY);
        EQ_DebugDynamicClarityTransitionCaptureTriggerFrame =
            EQ_DebugProcessFrames + 1UL;
        EQ_DynamicClarityCapturePhase = 3U;
        return;
    }

    if ((EQ_DynamicClarityCapturePhase == 3U) &&
        (EQ_TriggerCaptureReady != 0U))
    {
        EQ_DebugDynamicClarityTransitionCaptureDone = 1U;
        EQ_DebugDynamicClarityTransitionCaptureActive = 0U;
        EQ_DebugDynamicClarityTransitionCaptureDoneFrame =
            EQ_DebugProcessFrames;
        EQ_DynamicClarityCapturePhase = 0U;
    }
}
#endif

#if EQ_ENABLE_DYNAMIC_CLARITY_TIMING_DIAGNOSTICS != 0
static int EQ_ClassifyDynamicClarityPath(
    const DYNAMIC_CLARITY_STATE *state)
{
    if (state->transition_active == 0)
    {
        return (state->active_level == 0) ?
            DYNAMIC_CLARITY_PATH_IDENTITY :
            DYNAMIC_CLARITY_PATH_STABLE_FILTER;
    }
    if (state->active_level == 0)
    {
        return DYNAMIC_CLARITY_PATH_TRANSITION_0_TO_FILTER;
    }
    if (state->pending_level == 0)
    {
        return DYNAMIC_CLARITY_PATH_TRANSITION_FILTER_TO_0;
    }
    return DYNAMIC_CLARITY_PATH_TRANSITION_FILTER_TO_FILTER;
}

static void EQ_ResetDynamicClarityTimingDiagnostics(void)
{
    memset((void *)EQ_DebugDynamicClarityTimingFrameCount, 0,
           sizeof(EQ_DebugDynamicClarityTimingFrameCount));
    memset((void *)EQ_DebugDynamicClarityTimingLastCycles, 0,
           sizeof(EQ_DebugDynamicClarityTimingLastCycles));
    memset((void *)EQ_DebugDynamicClarityTimingMaxCycles, 0,
           sizeof(EQ_DebugDynamicClarityTimingMaxCycles));
    memset((void *)EQ_DebugDynamicClarityTimingMaxProcessFrame, 0,
           sizeof(EQ_DebugDynamicClarityTimingMaxProcessFrame));
    memset((void *)EQ_DebugDynamicClarityTimingMaxActiveLevel, 0,
           sizeof(EQ_DebugDynamicClarityTimingMaxActiveLevel));
    memset((void *)EQ_DebugDynamicClarityTimingMaxPendingLevel, 0,
           sizeof(EQ_DebugDynamicClarityTimingMaxPendingLevel));
    memset((void *)EQ_DebugDynamicClarityTimingMaxTargetLevel, 0,
           sizeof(EQ_DebugDynamicClarityTimingMaxTargetLevel));
    memset((void *)EQ_DebugDynamicClarityTimingMaxTransitionRemaining, 0,
           sizeof(EQ_DebugDynamicClarityTimingMaxTransitionRemaining));
    memset((void *)EQ_DebugDynamicClarityTimingMaxAnalyzerCount, 0,
           sizeof(EQ_DebugDynamicClarityTimingMaxAnalyzerCount));
    memset((void *)EQ_DebugDynamicClarityTimingMaxAdFrames, 0,
           sizeof(EQ_DebugDynamicClarityTimingMaxAdFrames));
    memset((void *)EQ_DebugDynamicClarityTimingMaxDaFrames, 0,
           sizeof(EQ_DebugDynamicClarityTimingMaxDaFrames));
    memset((void *)EQ_DebugDynamicClarityTimingMaxFlagAd, 0,
           sizeof(EQ_DebugDynamicClarityTimingMaxFlagAd));
    memset((void *)EQ_DebugDynamicClarityTimingMaxFlagDa, 0,
           sizeof(EQ_DebugDynamicClarityTimingMaxFlagDa));
    memset((void *)EQ_DebugDynamicClarityTimingMaxDeadlineCount, 0,
           sizeof(EQ_DebugDynamicClarityTimingMaxDeadlineCount));
    memset((void *)EQ_DebugDynamicClarityTimingMaxLatencyMissCount, 0,
           sizeof(EQ_DebugDynamicClarityTimingMaxLatencyMissCount));
    memset((void *)EQ_DebugDynamicClarityTimingMaxOverlapCount, 0,
           sizeof(EQ_DebugDynamicClarityTimingMaxOverlapCount));
    memset((void *)EQ_DebugDynamicClarityTimingMaxDroppedCount, 0,
           sizeof(EQ_DebugDynamicClarityTimingMaxDroppedCount));
    memset((void *)EQ_DebugDynamicClarityTimingMaxUpdateCount, 0,
           sizeof(EQ_DebugDynamicClarityTimingMaxUpdateCount));
}

static void EQ_UpdateDynamicClarityPathTiming(
    int path, unsigned long cycles,
    int active_level, int pending_level, int target_level,
    int transition_remaining)
{
    EQ_DebugDynamicClarityTimingFrameCount[path]++;
    EQ_DebugDynamicClarityTimingLastCycles[path] = cycles;
    if (cycles > EQ_DebugDynamicClarityTimingMaxCycles[path])
    {
        EQ_DebugDynamicClarityTimingMaxCycles[path] = cycles;
        EQ_DebugDynamicClarityTimingMaxProcessFrame[path] =
            EQ_DebugProcessFrames + 1UL;
        EQ_DebugDynamicClarityTimingMaxActiveLevel[path] = active_level;
        EQ_DebugDynamicClarityTimingMaxPendingLevel[path] = pending_level;
        EQ_DebugDynamicClarityTimingMaxTargetLevel[path] = target_level;
        EQ_DebugDynamicClarityTimingMaxTransitionRemaining[path] =
            transition_remaining;
        EQ_DebugDynamicClarityTimingMaxAnalyzerCount[path] =
            EQ_DebugAnalyzerAnalysisCount;
        EQ_DebugDynamicClarityTimingMaxAdFrames[path] = EQ_DebugAdFrames;
        EQ_DebugDynamicClarityTimingMaxDaFrames[path] = EQ_DebugDaFrames;
        EQ_DebugDynamicClarityTimingMaxFlagAd[path] =
            (unsigned int)FLAG_AD;
        EQ_DebugDynamicClarityTimingMaxFlagDa[path] =
            (unsigned int)FLAG_DA;
        EQ_DebugDynamicClarityTimingMaxDeadlineCount[path] =
            EQ_DebugDeadlineMissCount;
        EQ_DebugDynamicClarityTimingMaxLatencyMissCount[path] =
            EQ_DebugFrameLatencyDeadlineMissCount;
        EQ_DebugDynamicClarityTimingMaxOverlapCount[path] =
            EQ_DebugFrameServiceOverlapCount;
        EQ_DebugDynamicClarityTimingMaxDroppedCount[path] =
            EQ_DebugFrameServiceDroppedCount;
        EQ_DebugDynamicClarityTimingMaxUpdateCount[path]++;
    }
}
#endif
#endif

#if EQ_ENABLE_HARSHNESS_GUARD != 0
static void EQ_SyncHarshnessGuardDebug(void)
{
    EQ_DebugHarshnessGuardProcessingActive =
        (unsigned int)EQ_HarshnessGuardState.processing_active;
    EQ_DebugHarshnessGuardBrightnessDb =
        EQ_HarshnessGuardState.latest_brightness_relative_db;
    EQ_DebugHarshnessGuardPresenceDb =
        EQ_HarshnessGuardState.latest_presence_relative_db;
    EQ_DebugHarshnessGuardExcessDb =
        EQ_HarshnessGuardState.latest_harshness_db;
    EQ_DebugHarshnessGuardRmsDbfs =
        EQ_HarshnessGuardState.latest_rms_dbfs;
    EQ_DebugHarshnessGuardRequestedLevel =
        EQ_HarshnessGuardState.target_level;
    EQ_DebugHarshnessGuardAppliedLevel =
        EQ_HarshnessGuardState.active_level;
    EQ_DebugHarshnessGuardPendingLevel =
        EQ_HarshnessGuardState.pending_level;
    EQ_DebugHarshnessGuardRequestedGainDb =
        HarshnessGuard_GetRequestedGainDb(&EQ_HarshnessGuardState);
    EQ_DebugHarshnessGuardAppliedGainDb =
        HarshnessGuard_GetAppliedGainDb(&EQ_HarshnessGuardState);
    EQ_DebugHarshnessGuardTransitionActive =
        (unsigned int)EQ_HarshnessGuardState.transition_active;
    EQ_DebugHarshnessGuardTransitionProgress =
        HarshnessGuard_GetTransitionProgress(&EQ_HarshnessGuardState);
    EQ_DebugHarshnessGuardReason =
        HarshnessGuard_GetReason(&EQ_HarshnessGuardState);
    EQ_DebugHarshnessGuardDecisionCount =
        EQ_HarshnessGuardState.decision_count;
    EQ_DebugHarshnessGuardLevelChangeCount =
        EQ_HarshnessGuardState.level_change_count;
    EQ_DebugHarshnessGuardTransitionCount =
        EQ_HarshnessGuardState.transition_count;
    EQ_DebugHarshnessGuardInvalidReleaseCount =
        EQ_HarshnessGuardState.invalid_release_count;
    EQ_DebugHarshnessGuardSaturationCount =
        EQ_HarshnessGuardState.saturation_count;
    EQ_DebugHarshnessGuardNonFiniteCount =
        EQ_HarshnessGuardState.nonfinite_count;
}

static void EQ_ServiceHarshnessGuardRuntimeControl(void)
{
    int requested_enabled;

    EQ_DebugHarshnessGuardEnabled =
        (EQ_DebugHarshnessGuardEnabled != 0U) ? 1U : 0U;
    (void)HarshnessGuard_SetStrength(
        &EQ_HarshnessGuardState, EQ_DebugHarshnessGuardStrength);
    EQ_DebugHarshnessGuardStrength = EQ_HarshnessGuardState.strength;
    requested_enabled =
        ((EQ_DebugHarshnessGuardEnabled != 0U) &&
         (EQ_DebugAnalyzerEnabled != 0U) &&
         (EQ_AnalyzerLastEnabled != 0U) &&
         ((EQ_HarshnessGuardAnalyzerFault == 0U) ||
          (EQ_HarshnessGuardState.requested_enabled != 0))) ? 1 : 0;
    (void)HarshnessGuard_SetEnabled(
        &EQ_HarshnessGuardState, requested_enabled);
}

static void EQ_MarkHarshnessGuardAnalyzerUnavailable(void)
{
    EQ_HarshnessGuardAnalyzerFault = 1U;
    if (EQ_HarshnessGuardState.initialized != 0)
    {
        HarshnessGuard_InvalidateAnalysisEpoch(
            &EQ_HarshnessGuardState);
        (void)HarshnessGuard_SetEnabled(
            &EQ_HarshnessGuardState, 0);
        EQ_SyncHarshnessGuardDebug();
    }
}

static void EQ_MarkHarshnessGuardAnalyzerReset(void)
{
    EQ_HarshnessGuardAnalyzerFault = 1U;
    if (EQ_HarshnessGuardState.initialized != 0)
    {
        HarshnessGuard_InvalidateAnalysisEpoch(
            &EQ_HarshnessGuardState);
        (void)HarshnessGuard_SetEnabled(
            &EQ_HarshnessGuardState, 0);
        EQ_SyncHarshnessGuardDebug();
    }
}

static void EQ_UpdateHarshnessGuardTiming(unsigned long cycles)
{
    EQ_DebugHarshnessGuardLastCycles = cycles;
    if (cycles > EQ_DebugHarshnessGuardMaxCycles)
    {
        EQ_DebugHarshnessGuardMaxCycles = cycles;
    }
}

#if EQ_ENABLE_FOUR_WAY_TRANSITION_DIAGNOSTICS != 0
static void EQ_UpdateFourWayTransitionDiagnostics(void)
{
    unsigned int mask;
    int armed_mode;

    mask = 0U;
    if (Equalizer_GetTransitionRemaining(&EQ_BoardState) > 0)
    {
        mask |= 0x01U;
    }
    if (EQ_SmartBassState.transition_active != 0)
    {
        mask |= 0x02U;
    }
    if (EQ_DynamicClarityState.transition_active != 0)
    {
        mask |= 0x04U;
    }
    if (EQ_HarshnessGuardState.transition_active != 0)
    {
        mask |= 0x08U;
    }

    armed_mode = EQ_DebugFourWayTransitionArmMode;
    if ((armed_mode >= EQ_PRESET_FLAT) &&
        (armed_mode < EQ_PRESET_COUNT) &&
        ((mask & 0x0eU) == 0x0eU))
    {
        EQ_DebugMode = armed_mode;
        EQ_DebugFourWayTransitionArmMode = EQ_PRESET_NONE;
        EQ_DebugFourWayTransitionArmCount++;
    }

    EQ_DebugFourWayTransitionMask = mask;
    if (mask == 0x0fU)
    {
        if (EQ_DebugFourWayTransitionOverlapCount == 0UL)
        {
            EQ_DebugFourWayTransitionFirstFrame =
                EQ_DebugProcessFrames + 1UL;
        }
        EQ_DebugFourWayTransitionOverlapCount++;
    }
}
#endif

#if EQ_ENABLE_HARSHNESS_GUARD_TRANSITION_CAPTURE != 0
static void EQ_ServiceHarshnessGuardTransitionCapture(void)
{
    int result;

    if ((EQ_DebugHarshnessGuardTransitionCaptureRequest != 0U) &&
        (EQ_DebugHarshnessGuardTransitionCaptureActive == 0U))
    {
        EqualizerCapture_Reset();
        result = HarshnessGuard_DiagnosticForceStableLevel(
            &EQ_HarshnessGuardState,
            EQ_DebugHarshnessGuardTransitionCaptureBaseLevel);
        EQ_DebugHarshnessGuardTransitionCaptureResult = result;
        EQ_DebugHarshnessGuardTransitionCaptureRequest = 0U;
        EQ_DebugHarshnessGuardTransitionCaptureDone = 0U;
        EQ_DebugHarshnessGuardTransitionCaptureOverride = 1U;
        EQ_DebugHarshnessGuardTransitionCaptureActive = 1U;
        EQ_DebugHarshnessGuardTransitionCaptureRequestFrame =
            EQ_DebugProcessFrames + 1UL;
        EQ_DebugHarshnessGuardTransitionCaptureTriggerFrame = 0UL;
        EQ_DebugHarshnessGuardTransitionCaptureDoneFrame = 0UL;
        EQ_HarshnessGuardCapturePrerollRemaining =
            EQ_HARSHNESS_GUARD_CAPTURE_PREROLL_FRAMES;
        EQ_HarshnessGuardCapturePhase = 1U;
        return;
    }
    if (EQ_DebugHarshnessGuardTransitionCaptureActive == 0U)
    {
        return;
    }

    if (EQ_HarshnessGuardCapturePhase == 1U)
    {
        if (EQ_HarshnessGuardCapturePrerollRemaining != 0U)
        {
            EQ_HarshnessGuardCapturePrerollRemaining--;
        }
        if (EQ_HarshnessGuardCapturePrerollRemaining == 0U)
        {
            EQ_TriggerCaptureRequest =
                EQ_CAPTURE_TRIGGER_HARSHNESS_GUARD;
            EQ_HarshnessGuardCapturePhase = 2U;
        }
        return;
    }

    if ((EQ_HarshnessGuardCapturePhase == 2U) &&
        (EQ_TriggerCaptureArmedReady != 0U))
    {
        if (EQ_DebugHarshnessGuardTransitionCaptureTargetLevel !=
            EQ_DebugHarshnessGuardTransitionCaptureBaseLevel)
        {
            result = HarshnessGuard_DiagnosticRequestLevel(
                &EQ_HarshnessGuardState,
                EQ_DebugHarshnessGuardTransitionCaptureTargetLevel);
            EQ_DebugHarshnessGuardTransitionCaptureResult = result;
            if (result != HARSHNESS_GUARD_RESULT_UPDATED)
            {
                EQ_DebugHarshnessGuardTransitionCaptureActive = 0U;
                EQ_HarshnessGuardCapturePhase = 0U;
                return;
            }
        }
        EqualizerCapture_NotifyEvent(
            EQ_CAPTURE_TRIGGER_HARSHNESS_GUARD);
        EQ_DebugHarshnessGuardTransitionCaptureTriggerFrame =
            EQ_DebugProcessFrames + 1UL;
        EQ_HarshnessGuardCapturePhase = 3U;
        return;
    }

    if ((EQ_HarshnessGuardCapturePhase == 3U) &&
        (EQ_TriggerCaptureReady != 0U))
    {
        EQ_DebugHarshnessGuardTransitionCaptureDone = 1U;
        EQ_DebugHarshnessGuardTransitionCaptureActive = 0U;
        EQ_DebugHarshnessGuardTransitionCaptureDoneFrame =
            EQ_DebugProcessFrames;
        EQ_HarshnessGuardCapturePhase = 0U;
    }
}
#endif
#endif

#if EQ_ENABLE_AUDIO_FEATURE_ANALYZER != 0
static void EQ_ClearPublishedAnalyzerState(int preserve_harshness_state)
{
    EQ_DebugAnalyzerPending = 0U;
    EQ_DebugAnalyzerValid = 0U;
    EQ_DebugAnalyzerWarmup = 0U;
    EQ_DebugAnalyzerAnalysisCount = 0UL;
    EQ_DebugAnalyzerPeakDbfs = -240.0f;
    EQ_DebugAnalyzerRmsDbfs = -240.0f;
    EQ_DebugAnalyzerBassDb = 0.0f;
    EQ_DebugAnalyzerMudDb = 0.0f;
    EQ_DebugAnalyzerPresenceDb = 0.0f;
    EQ_DebugAnalyzerBrightnessDb = 0.0f;
    EQ_DebugUartFeatureRequest = 0U;
    EQ_AnalyzerLastDeferredFrame = ~0UL;
    EqualizerUartFeatureAudit_Init(&EQ_UartFeatureAudit);
    EQ_SyncUartFeatureAuditDebug();
#if EQ_ENABLE_SMART_BASS != 0
    EQ_MarkSmartBassAnalyzerUnavailable();
#endif
#if EQ_ENABLE_DYNAMIC_CLARITY != 0
    EQ_MarkDynamicClarityAnalyzerUnavailable();
#endif
#if EQ_ENABLE_HARSHNESS_GUARD != 0
    if (preserve_harshness_state != 0)
    {
        EQ_MarkHarshnessGuardAnalyzerReset();
    }
    else
    {
        EQ_MarkHarshnessGuardAnalyzerUnavailable();
    }
#endif
}

static void EQ_ResetAnalyzerRuntime(void)
{
    AudioFeatureAnalyzer_Reset(&EQ_AudioAnalyzerState);
    memset(EQ_AnalyzerInput, 0, sizeof(EQ_AnalyzerInput));
    EQ_ClearPublishedAnalyzerState(1);
    EQ_DebugAnalyzerEpoch++;
}

static int EQ_ServiceAnalyzerControl(int builder_eligible,
                                     unsigned char flag_ad_done)
{
    int action;
    int audio_safe;

    audio_safe = EqualizerBackgroundService_IsAudioSafeFinalCheck(
        FLAG_AD, FLAG_DA, flag_ad_done, EQ_FrameServicePending);
    action = EqualizerAnalyzerRuntime_Decide(
        &EQ_AnalyzerLastEnabled,
        EQ_DebugAnalyzerEnabled,
        EQ_DebugAnalyzerResetRequest,
        audio_safe,
        builder_eligible);
    if (action == EQ_ANALYZER_ACTION_DISABLE)
    {
        EQ_ClearPublishedAnalyzerState(0);
        return 1;
    }
    if ((action == EQ_ANALYZER_ACTION_ENABLE_RESET) ||
        (action == EQ_ANALYZER_ACTION_MANUAL_RESET))
    {
        EQ_ResetAnalyzerRuntime();
        EQ_DebugAnalyzerResetRequest = 0U;
        return 1;
    }
    return 0;
}

static void EQ_RecordAnalyzerDeferred(void)
{
    if (EQ_AnalyzerLastDeferredFrame != EQ_DebugProcessFrames)
    {
        EQ_AnalyzerLastDeferredFrame = EQ_DebugProcessFrames;
        EQ_DebugAnalyzerDeferredCount++;
    }
}

static void EQ_ObserveAnalyzerInputFrame(void)
{
    int cadence_result;

    if ((EQ_DebugAnalyzerEnabled == 0U) ||
        (EQ_AnalyzerLastEnabled == 0U))
    {
        return;
    }
    cadence_result = AudioFeatureAnalyzer_ObserveFrame(
        &EQ_AudioAnalyzerState);
    if (cadence_result != 1)
    {
        return;
    }
    if (EQ_DebugAnalyzerPending != 0U)
    {
        EQ_DebugAnalyzerPendingOverwriteCount++;
    }
    memcpy(EQ_AnalyzerInput, EQ_AD_Buffer1, sizeof(EQ_AnalyzerInput));
    EQ_DebugAnalyzerPending = 1U;
}

static void EQ_PublishAnalyzerSnapshot(void)
{
    AUDIO_FEATURE_SNAPSHOT snapshot;

    AudioFeatureAnalyzer_GetSnapshot(&EQ_AudioAnalyzerState, &snapshot);
    EQ_DebugAnalyzerValid = (unsigned int)snapshot.valid;
    EQ_DebugAnalyzerWarmup = (unsigned int)snapshot.warmup_complete;
    EQ_DebugAnalyzerAnalysisCount = snapshot.analysis_count;
    EQ_DebugAnalyzerPeakDbfs = snapshot.peak_dbfs;
    EQ_DebugAnalyzerRmsDbfs = snapshot.rms_dbfs;
    EQ_DebugAnalyzerBassDb =
        snapshot.relative_db[AUDIO_FEATURE_BASS];
    EQ_DebugAnalyzerMudDb =
        snapshot.relative_db[AUDIO_FEATURE_MUD];
    EQ_DebugAnalyzerPresenceDb =
        snapshot.relative_db[AUDIO_FEATURE_PRESENCE];
    EQ_DebugAnalyzerBrightnessDb =
        snapshot.relative_db[AUDIO_FEATURE_BRIGHTNESS];
#if EQ_ENABLE_SMART_BASS != 0
    EQ_SmartBassAnalyzerFault = 0U;
    EQ_ServiceSmartBassRuntimeControl();
    (void)SmartBass_UpdateFromFeature(&EQ_SmartBassState, &snapshot);
    EQ_SyncSmartBassDebug();
#endif
#if EQ_ENABLE_DYNAMIC_CLARITY != 0
    EQ_DynamicClarityAnalyzerFault =
        (snapshot.valid != 0) ? 0U : 1U;
    EQ_ServiceDynamicClarityRuntimeControl();
#if EQ_ENABLE_DYNAMIC_CLARITY_TRANSITION_CAPTURE != 0
    if (EQ_DebugDynamicClarityTransitionCaptureOverride == 0U)
#endif
    {
    (void)DynamicClarity_UpdateFromFeature(
        &EQ_DynamicClarityState, &snapshot);
    }
    EQ_SyncDynamicClarityDebug();
#endif
#if EQ_ENABLE_HARSHNESS_GUARD != 0
    EQ_HarshnessGuardAnalyzerFault =
        ((snapshot.valid != 0) &&
         (snapshot.warmup_complete != 0)) ? 0U : 1U;
    EQ_ServiceHarshnessGuardRuntimeControl();
#if EQ_ENABLE_HARSHNESS_GUARD_TRANSITION_CAPTURE != 0
    if (EQ_DebugHarshnessGuardTransitionCaptureOverride == 0U)
#endif
    {
    (void)HarshnessGuard_UpdateFromFeature(
        &EQ_HarshnessGuardState, &snapshot);
    }
    if (EQ_HarshnessGuardState.reason ==
        HARSHNESS_GUARD_REASON_INVALID)
    {
        EQ_HarshnessGuardAnalyzerFault = 1U;
    }
    EQ_SyncHarshnessGuardDebug();
#endif
    EQ_DebugAnalyzerPublicationCount++;
}

static int EQ_ServiceAnalyzer(void)
{
    unsigned long cycles;
    int result;
#if defined(__TI_COMPILER_VERSION__) || defined(__TMS320C6X__)
    unsigned int cycle_start;

    cycle_start = TSCL;
#endif

    EQ_DebugAnalyzerPending = 0U;
    result = AudioFeatureAnalyzer_AnalyzeObservedFrame(
        &EQ_AudioAnalyzerState, EQ_AnalyzerInput,
        AUDIO_FEATURE_FRAME_LEN);
#if defined(__TI_COMPILER_VERSION__) || defined(__TMS320C6X__)
    cycles = (unsigned long)(TSCL - cycle_start);
#else
    cycles = 0UL;
#endif
    EQ_DebugAnalyzerRunCount++;
    EQ_DebugAnalyzerLastCycles = cycles;
    if (cycles > EQ_DebugAnalyzerMaxCycles)
    {
        EQ_DebugAnalyzerMaxCycles = cycles;
    }
    if (result == 1)
    {
        EQ_PublishAnalyzerSnapshot();
    }
    else if (result < 0)
    {
        EQ_DebugAnalyzerValid = 0U;
        EQ_DebugAnalyzerWarmup = 0U;
#if EQ_ENABLE_SMART_BASS != 0
        EQ_MarkSmartBassAnalyzerUnavailable();
#endif
#if EQ_ENABLE_DYNAMIC_CLARITY != 0
        EQ_MarkDynamicClarityAnalyzerUnavailable();
#endif
#if EQ_ENABLE_HARSHNESS_GUARD != 0
        EQ_MarkHarshnessGuardAnalyzerUnavailable();
#endif
    }
    return result;
}
#endif /* EQ_ENABLE_AUDIO_FEATURE_ANALYZER */

static void EQ_CaptureAdcFrame(void)
{
    unsigned long mode_change_before;
    unsigned long mode_change_after;
    int static_eq_transition_active;
    int dynamic_transition_active;
#if defined(__TI_COMPILER_VERSION__) || defined(__TMS320C6X__)
    unsigned int cycle_start;
    unsigned int cycle_end;
    unsigned long cycle_delta;
#if EQ_ENABLE_SMART_BASS != 0
    unsigned int smart_bass_cycle_start;
#endif
#if EQ_ENABLE_DYNAMIC_CLARITY != 0
    unsigned int dynamic_clarity_cycle_start;
#endif
#if EQ_ENABLE_HARSHNESS_GUARD != 0
    unsigned int harshness_guard_cycle_start;
#endif
#endif
#if EQ_ENABLE_SMART_BASS != 0
    unsigned long smart_bass_cycles;
#endif
#if EQ_ENABLE_DYNAMIC_CLARITY != 0
    unsigned long dynamic_clarity_cycles;
#if EQ_ENABLE_DYNAMIC_CLARITY_TIMING_DIAGNOSTICS != 0
    int dynamic_clarity_path;
    int dynamic_clarity_active_level;
    int dynamic_clarity_pending_level;
    int dynamic_clarity_target_level;
    int dynamic_clarity_transition_remaining;
#endif
#endif
#if EQ_ENABLE_HARSHNESS_GUARD != 0
    unsigned long harshness_guard_cycles;
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

#if EQ_ENABLE_AUDIO_FEATURE_ANALYZER != 0
    EQ_ObserveAnalyzerInputFrame();
#endif

#if defined(__TI_COMPILER_VERSION__) || defined(__TMS320C6X__)
    cycle_start = TSCL;
#endif
#if EQ_ENABLE_SMART_BASS != 0
    EQ_ServiceSmartBassRuntimeControl();
#endif
#if EQ_ENABLE_DYNAMIC_CLARITY != 0
    EQ_ServiceDynamicClarityRuntimeControl();
#endif
#if EQ_ENABLE_HARSHNESS_GUARD != 0
    EQ_ServiceHarshnessGuardRuntimeControl();
#endif
    static_eq_transition_active =
        (Equalizer_GetTransitionRemaining(&EQ_BoardState) > 0) ? 1 : 0;
    dynamic_transition_active = 0;
#if EQ_ENABLE_SMART_BASS != 0
    if (EQ_SmartBassState.transition_active != 0)
    {
        dynamic_transition_active = 1;
    }
#endif
#if EQ_ENABLE_DYNAMIC_CLARITY != 0
    if (EQ_DynamicClarityState.transition_active != 0)
    {
        dynamic_transition_active = 1;
    }
#endif
#if EQ_ENABLE_HARSHNESS_GUARD != 0
    if (EQ_HarshnessGuardState.transition_active != 0)
    {
        dynamic_transition_active = 1;
    }
#endif
    if ((static_eq_transition_active != 0) &&
        (dynamic_transition_active != 0))
    {
        EQ_DebugStaticDynamicTransitionOverlapFrameCount++;
    }
    mode_change_before = Equalizer_GetModeChangeCount(&EQ_BoardState);
    Equalizer_ProcessFrame(&EQ_BoardState, EQ_AD_Buffer1, EQ_DA_Buffer1,
                           ADC_SAMPLE_1024);
#if EQ_ENABLE_SMART_BASS != 0
#if defined(__TI_COMPILER_VERSION__) || defined(__TMS320C6X__)
    smart_bass_cycle_start = TSCL;
#endif
    (void)SmartBass_ProcessFrame(
        &EQ_SmartBassState, EQ_DA_Buffer1, EQ_DA_Buffer1,
        ADC_SAMPLE_1024);
#if defined(__TI_COMPILER_VERSION__) || defined(__TMS320C6X__)
    smart_bass_cycles =
        (unsigned long)(TSCL - smart_bass_cycle_start);
#else
    smart_bass_cycles = 0UL;
#endif
    EQ_UpdateSmartBassTiming(smart_bass_cycles);
    EQ_SyncSmartBassDebug();
#endif
#if EQ_ENABLE_DYNAMIC_CLARITY != 0
#if EQ_ENABLE_DYNAMIC_CLARITY_TRANSITION_CAPTURE != 0
    EQ_ServiceDynamicClarityTransitionCapture();
#endif
#if EQ_ENABLE_DYNAMIC_CLARITY_TIMING_DIAGNOSTICS != 0
    dynamic_clarity_path =
        EQ_ClassifyDynamicClarityPath(&EQ_DynamicClarityState);
    dynamic_clarity_active_level = EQ_DynamicClarityState.active_level;
    dynamic_clarity_pending_level = EQ_DynamicClarityState.pending_level;
    dynamic_clarity_target_level = EQ_DynamicClarityState.target_level;
    dynamic_clarity_transition_remaining =
        EQ_DynamicClarityState.transition_remaining;
#endif
#if defined(__TI_COMPILER_VERSION__) || defined(__TMS320C6X__)
    dynamic_clarity_cycle_start = TSCL;
#endif
    (void)DynamicClarity_ProcessFrame(
        &EQ_DynamicClarityState, EQ_DA_Buffer1, EQ_DA_Buffer1,
        ADC_SAMPLE_1024);
#if defined(__TI_COMPILER_VERSION__) || defined(__TMS320C6X__)
    dynamic_clarity_cycles =
        (unsigned long)(TSCL - dynamic_clarity_cycle_start);
#else
    dynamic_clarity_cycles = 0UL;
#endif
    EQ_UpdateDynamicClarityTiming(dynamic_clarity_cycles);
#if EQ_ENABLE_DYNAMIC_CLARITY_TIMING_DIAGNOSTICS != 0
    EQ_UpdateDynamicClarityPathTiming(
        dynamic_clarity_path, dynamic_clarity_cycles,
        dynamic_clarity_active_level, dynamic_clarity_pending_level,
        dynamic_clarity_target_level,
        dynamic_clarity_transition_remaining);
#endif
    EQ_SyncDynamicClarityDebug();
#endif
#if EQ_ENABLE_HARSHNESS_GUARD != 0
#if EQ_ENABLE_HARSHNESS_GUARD_TRANSITION_CAPTURE != 0
    EQ_ServiceHarshnessGuardTransitionCapture();
#endif
#if defined(__TI_COMPILER_VERSION__) || defined(__TMS320C6X__)
    harshness_guard_cycle_start = TSCL;
#endif
    (void)HarshnessGuard_ProcessFrame(
        &EQ_HarshnessGuardState, EQ_DA_Buffer1, EQ_DA_Buffer1,
        ADC_SAMPLE_1024);
#if defined(__TI_COMPILER_VERSION__) || defined(__TMS320C6X__)
    harshness_guard_cycles =
        (unsigned long)(TSCL - harshness_guard_cycle_start);
#else
    harshness_guard_cycles = 0UL;
#endif
    EQ_UpdateHarshnessGuardTiming(harshness_guard_cycles);
    EQ_SyncHarshnessGuardDebug();
#if EQ_ENABLE_FOUR_WAY_TRANSITION_DIAGNOSTICS != 0
    EQ_UpdateFourWayTransitionDiagnostics();
#endif
#endif
    mode_change_after = Equalizer_GetModeChangeCount(&EQ_BoardState);
    EQ_DebugProcessFrames++;
    if (EQ_DebugInitStage < 10UL)
    {
        EQ_DebugInitStage = 10UL;
    }
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
    int audio_serviced;
    int builder_serviced;
    int analyzer_serviced;
#endif
    unsigned int builder_audio_before;
    unsigned int builder_audio_after;
#if EQ_ENABLE_AUDIO_FEATURE_ANALYZER != 0
    unsigned int analyzer_audio_before;
    unsigned int analyzer_audio_after;
#endif
    int background_kind;
    int analyzer_eligible;
#if EQ_ENABLE_AUDIO_FEATURE_ANALYZER != 0
    int analyzer_result;
#endif
    int builder_eligible;
    int builder_result;
    int direct_path;
    int uart_eligible;
#if EQ_ENABLE_LCD_DISPLAY != 0
    EQ_LCD_SERVICE_POLICY lcd_policy;
    EQ_LCD_FAULT_POLICY lcd_fault_policy;
    EQ_UI_SNAPSHOT lcd_snapshot;
    unsigned long lcd_disable_reason;
    unsigned int lcd_audio_before;
    unsigned int lcd_audio_after;
    int lcd_job;
    int lcd_eligible;
    int lcd_has_eligible_job;
    int lcd_policy_decision;
#endif
#if EQ_ENABLE_PROJECT33_TOUCH != 0
    int touch_serviced;
    int touch_action;
#endif

    flag_ad_done = 0;
    EQ_DebugInitStage = 1UL;
    EQ_DebugFlagAdDone = 0U;
#if EQ_ENABLE_LCD_DISPLAY != 0
    EqualizerLcdPolicy_Init(&lcd_policy);
    EqualizerLcdFaultPolicy_Init(&lcd_fault_policy);
#endif
    Sys_Init();
    EQ_UartInit();
    EQ_UartReportStage(1UL);
    Key_Init();
    EQ_KeepBuildFingerprint();

    Adc_Init(ADC_50KHZ, ADC_SAMPLE_1024);
    EQ_DebugInitStage = 2UL;
    EQ_UartReportStage(2UL);
    Dac_Init(DAC_50KHZ, DAC_SAMPLE_1024, DAC_CHANNEL_ALL);
    EQ_DebugInitStage = 3UL;
    EQ_UartReportStage(3UL);
    Equalizer_Init(&EQ_BoardState);
    EQ_DebugInitStage = 4UL;
    EQ_UartReportStage(4UL);
    memset(&EQ_ControlMailbox, 0, sizeof(EQ_ControlMailbox));
    EqualizerControl_Init(&EQ_BoardControl, &EQ_BoardState);
    EqualizerBackgroundService_Init(&EQ_BackgroundService);
#if EQ_ENABLE_AUDIO_FEATURE_ANALYZER != 0
    AudioFeatureAnalyzer_Init(&EQ_AudioAnalyzerState);
    memset(EQ_AnalyzerInput, 0, sizeof(EQ_AnalyzerInput));
    EQ_DebugAnalyzerEnabled = EQ_ANALYZER_RUNTIME_DEFAULT_ENABLED;
    EqualizerAnalyzerRuntime_Init(&EQ_AnalyzerLastEnabled);
    EQ_ClearPublishedAnalyzerState(0);
    EQ_DebugAnalyzerResetRequest = 0U;
#endif
#if EQ_ENABLE_SMART_BASS != 0
    SmartBass_Init(&EQ_SmartBassState);
    EQ_SmartBassAnalyzerFault = 1U;
    EQ_DebugSmartBassEnabled = 0U;
    EQ_DebugSmartBassStrength = SMART_BASS_STRENGTH_MEDIUM;
    EQ_DebugSmartBassLastCycles = 0UL;
    EQ_DebugSmartBassMaxCycles = 0UL;
    EQ_SyncSmartBassDebug();
#endif
#if EQ_ENABLE_DYNAMIC_CLARITY != 0
    DynamicClarity_Init(&EQ_DynamicClarityState);
    EQ_DynamicClarityAnalyzerFault = 1U;
    EQ_DebugDynamicClarityEnabled = 0U;
    EQ_DebugDynamicClarityStrength =
        DYNAMIC_CLARITY_STRENGTH_MEDIUM;
    EQ_DebugDynamicClarityLastCycles = 0UL;
    EQ_DebugDynamicClarityMaxCycles = 0UL;
#if EQ_ENABLE_DYNAMIC_CLARITY_TRANSITION_CAPTURE != 0
    EQ_DynamicClarityCapturePhase = 0U;
    EQ_DynamicClarityCapturePrerollRemaining = 0U;
    EQ_DebugDynamicClarityTransitionCaptureRequest = 0U;
    EQ_DebugDynamicClarityTransitionCaptureActive = 0U;
    EQ_DebugDynamicClarityTransitionCaptureDone = 0U;
    EQ_DebugDynamicClarityTransitionCaptureOverride = 0U;
    EQ_DebugDynamicClarityTransitionCaptureResult = 0;
#endif
#if EQ_ENABLE_DYNAMIC_CLARITY_TIMING_DIAGNOSTICS != 0
    EQ_ResetDynamicClarityTimingDiagnostics();
#endif
    EQ_SyncDynamicClarityDebug();
#endif
#if EQ_ENABLE_HARSHNESS_GUARD != 0
    HarshnessGuard_Init(&EQ_HarshnessGuardState);
    EQ_HarshnessGuardAnalyzerFault = 1U;
    EQ_DebugHarshnessGuardEnabled = 0U;
    EQ_DebugHarshnessGuardStrength =
        HARSHNESS_GUARD_STRENGTH_MEDIUM;
    EQ_DebugHarshnessGuardLastCycles = 0UL;
    EQ_DebugHarshnessGuardMaxCycles = 0UL;
#if EQ_ENABLE_FOUR_WAY_TRANSITION_DIAGNOSTICS != 0
    EQ_DebugFourWayTransitionMask = 0U;
    EQ_DebugFourWayTransitionOverlapCount = 0UL;
    EQ_DebugFourWayTransitionFirstFrame = 0UL;
    EQ_DebugFourWayTransitionArmMode = EQ_PRESET_NONE;
    EQ_DebugFourWayTransitionArmCount = 0UL;
#endif
#if EQ_ENABLE_HARSHNESS_GUARD_TRANSITION_CAPTURE != 0
    EQ_HarshnessGuardCapturePhase = 0U;
    EQ_HarshnessGuardCapturePrerollRemaining = 0U;
    EQ_DebugHarshnessGuardTransitionCaptureRequest = 0U;
    EQ_DebugHarshnessGuardTransitionCaptureActive = 0U;
    EQ_DebugHarshnessGuardTransitionCaptureDone = 0U;
    EQ_DebugHarshnessGuardTransitionCaptureOverride = 0U;
    EQ_DebugHarshnessGuardTransitionCaptureResult = 0;
#endif
    EQ_SyncHarshnessGuardDebug();
#endif
    EQ_AppliedDiagPath = EQ_DIAG_PRESET;
    EQ_LastServicedMode = EQ_PRESET_FLAT;
    EQ_UpdateDebugGains();
    EQ_ClearDacBuffers();
#if EQ_ENABLE_LCD_DISPLAY != 0
    EqualizerDisplay_Init();
#if EQ_ENABLE_TEN_BAND_EDITOR != 0
    EqualizerUiEditor_Init(&EQ_UiEditorState);
    EQ_SyncUiEditorDebug();
    memset(&EQ_UiEventVersion, 0, sizeof(EQ_UiEventVersion));
    EQ_UiSnapshotLastRequestFrame = ~0UL;
    EQ_DebugUiRequestedPage = EQ_UI_PAGE_DYNAMIC_STATUS;
    EQ_DebugUiDisplayedPage = EQ_UI_PAGE_DYNAMIC_STATUS;
    EQ_DebugUiPageBuilding = 0U;
    EQ_DebugUiSnapshotBuildCount = 0UL;
    EQ_DebugUiSnapshotSkippedCount = 0UL;
    EQ_DebugUiAppliedGainRefreshCount = 0UL;
    EQ_DebugUiDraftVersion = 0UL;
    EQ_DebugUiPageSwitchCount = 0UL;
#endif
#if EQ_ENABLE_PROJECT33_TOUCH != 0
    Touch_Init();
    EqualizerUiTouch_Init(&EQ_TouchState);
    EqualizerUi_DefaultTouchTransform(&EQ_TouchTransform);
    EQ_TouchTransform.raw_x_min = EQ_UI_TOUCH_RAW_X_MIN;
    EQ_TouchTransform.raw_x_max = EQ_UI_TOUCH_RAW_X_MAX;
    EQ_TouchTransform.raw_y_min = EQ_UI_TOUCH_RAW_Y_MIN;
    EQ_TouchTransform.raw_y_max = EQ_UI_TOUCH_RAW_Y_MAX;
    EQ_TouchTransform.swap_xy = EQ_UI_TOUCH_SWAP_XY;
    EQ_TouchTransform.flip_x = EQ_UI_TOUCH_FLIP_X;
    EQ_TouchTransform.flip_y = EQ_UI_TOUCH_FLIP_Y;
    EQ_TouchLastActionFrame = ~0UL;
    EQ_TouchLastServiceFrame = ~0UL;
    EQ_TouchAcceptedThisPress = 0U;
    EQ_DebugTouchRawX = 0U;
    EQ_DebugTouchRawY = 0U;
    EQ_DebugTouchScreenX = 0;
    EQ_DebugTouchScreenY = 0;
    EQ_DebugTouchPressed = 0U;
    EQ_DebugTouchLastAction = EQ_UI_ACTION_NONE;
    EQ_DebugTouchActionCount = 0UL;
    EQ_DebugTouchRejectedCount = 0UL;
    EQ_DebugTouchInvalidCoordinateCount = 0UL;
    EQ_DebugTouchPresetRequestCount = 0UL;
    EQ_DebugTouchDynamicEnableRequestCount = 0UL;
    EQ_DebugTouchDynamicStrengthRequestCount = 0UL;
    EQ_DebugTouchEditorActionCount = 0UL;
    EQ_DebugTouchDuplicateActionCount = 0UL;
    memset((void *)EQ_DebugTouchActionHistogram, 0,
           sizeof(EQ_DebugTouchActionHistogram));
    EQ_DebugTouchLastCycles = 0UL;
    EQ_DebugTouchMaxCycles = 0UL;
#endif
#if EQ_ENABLE_TEN_BAND_EDITOR != 0
    EQ_RequestUiSnapshotIfChanged(
        &lcd_snapshot, EQ_DebugProcessFrames, 1);
#else
    EQ_BuildUiSnapshot(&lcd_snapshot);
    EqualizerDisplay_RequestSnapshot(
        &lcd_snapshot, EQ_DebugProcessFrames);
#endif
    (void)EqualizerDisplay_DrawStaticLayout();
    EqualizerDisplay_BeginRuntime();
#endif

    EQ_DebugInitStage = 5UL;
    EQ_UartReportStage(5UL);

#if defined(__TI_COMPILER_VERSION__) || defined(__TMS320C6X__)
    TSCL = 0;
#endif

    Adc_Start();
    EQ_DebugInitStage = 6UL;
    EQ_UartReportStage(6UL);
    Dac_Start();
    EQ_DebugInitStage = 7UL;
    EQ_UartReportStage(7UL);

    EQ_DebugInitStage = 8UL;
    EQ_UartReportStage(8UL);
    while (1)
    {
#if EQ_ENABLE_LCD_DISPLAY != 0
        audio_serviced = 0;
        builder_serviced = 0;
        analyzer_serviced = 0;
#endif
#if EQ_ENABLE_PROJECT33_TOUCH != 0
        touch_serviced = 0;
        touch_action = EQ_UI_ACTION_NONE;
#endif
        if (FLAG_AD == 1)
        {
#if EQ_ENABLE_LCD_DISPLAY != 0
            audio_serviced = 1;
#endif
            EQ_BeginFrameService();
            EQ_BeginFrameActiveSegment();
            FLAG_AD = 0;
            flag_ad_done = 1;
            EQ_DebugFlagAdDone = 1U;
            EQ_DebugAdFrames++;
            if (EQ_DebugInitStage < 9UL)
            {
                EQ_DebugInitStage = 9UL;
            }
            EQ_CaptureAdcFrame();
            EQ_EndFrameActiveSegment();
        }

        if ((FLAG_DA == 1) && (flag_ad_done == 1))
        {
#if EQ_ENABLE_LCD_DISPLAY != 0
            audio_serviced = 1;
#endif
            EQ_BeginFrameActiveSegment();
            FLAG_DA = 0;
            flag_ad_done = 0;
            EQ_DebugFlagAdDone = 0U;
            EQ_DebugDaFrames++;
            EQ_FillDacInactiveBuffer();
            EQ_EndFrameActiveSegment();
            EQ_EndFrameService();
#if EQ_ENABLE_AUDIO_FEATURE_ANALYZER != 0
            EQ_CompleteUartFeatureAudit();
#endif
            if (EQ_DebugInitStage < 11UL)
            {
                EQ_DebugInitStage = 11UL;
                EQ_UartReportStage(11UL);
            }
        }

        if ((FLAG_AD == 0) && (FLAG_DA == 0) && (flag_ad_done == 0))
        {
            if (EQ_FrameServicePending != 0U)
            {
                continue;
            }
            EqualizerControl_ObserveFrameBoundary(
                &EQ_BoardControl, &EQ_BoardState);
            direct_path = EQ_ServiceModeTimed();
            if (direct_path != 0)
            {
#if EQ_ENABLE_AUDIO_FEATURE_ANALYZER != 0
                builder_eligible = EqualizerControl_BuilderEligible(
                    &EQ_BoardControl, &EQ_BoardState);
                if (EQ_ServiceAnalyzerControl(
                        builder_eligible, flag_ad_done))
                {
                    EqualizerBackgroundService_Record(
                        &EQ_BackgroundService, EQ_DebugProcessFrames,
                        EQ_BACKGROUND_ANALYZER);
                }
#endif
                continue;
            }
            EqualizerControl_ServiceMailbox(
                &EQ_BoardControl, &EQ_ControlMailbox, &EQ_BoardState);
            EqualizerControl_InvalidateStaleWork(&EQ_BoardControl);
            EqualizerControl_TryInstallReady(
                &EQ_BoardControl, &EQ_BoardState);
            EQ_UpdateDebugGains();

#if EQ_ENABLE_LCD_DISPLAY != 0
#if EQ_ENABLE_TEN_BAND_EDITOR != 0
            EQ_RequestUiSnapshotIfChanged(
                &lcd_snapshot, EQ_DebugProcessFrames, 0);
#else
            EQ_BuildUiSnapshot(&lcd_snapshot);
            EqualizerDisplay_RequestSnapshot(
                &lcd_snapshot, EQ_DebugProcessFrames);
#endif
            EqualizerDisplay_AuditHardware(
                EQ_DebugProcessFrames, 0);
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

#if EQ_ENABLE_PROJECT33_TOUCH != 0
            if (audio_serviced == 0)
            {
                touch_serviced = EQ_ServiceUiTouch(
                    flag_ad_done, EQ_DebugProcessFrames, &touch_action);
            }
            if (touch_action != EQ_UI_ACTION_NONE)
            {
                EqualizerLcdPolicy_RecordControlChange(
                    &lcd_policy, EQ_DebugProcessFrames);
            }
            if (touch_serviced != 0)
            {
                continue;
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

            builder_eligible = EqualizerControl_BuilderEligible(
                &EQ_BoardControl, &EQ_BoardState);
#if EQ_ENABLE_AUDIO_FEATURE_ANALYZER != 0
            if (EQ_ServiceAnalyzerControl(
                    builder_eligible, flag_ad_done))
            {
                EqualizerBackgroundService_Record(
                    &EQ_BackgroundService, EQ_DebugProcessFrames,
                    EQ_BACKGROUND_ANALYZER);
                continue;
            }
            analyzer_eligible = EqualizerAnalyzer_CanService(
                FLAG_AD, FLAG_DA, flag_ad_done,
                EQ_FrameServicePending,
                builder_eligible,
                EQ_AnalyzerLastEnabled,
                EQ_DebugAnalyzerPending);
            uart_eligible =
                (EQ_DebugAnalyzerEnabled != 0U) &&
                (EQ_DebugAnalyzerValid != 0U) &&
                (EQ_DebugAnalyzerWarmup != 0U) &&
                (EQ_DebugUartFeatureRequest != 0U) &&
                (EQ_DebugUartFeatureAuditPending == 0U) &&
                (builder_eligible == 0) &&
                (analyzer_eligible == 0);
#else
            analyzer_eligible = 0;
            uart_eligible = 0;
#endif
#if EQ_ENABLE_LCD_DISPLAY != 0
            builder_serviced =
                ((EQ_BackgroundService.consumed_frame_valid != 0) &&
                 (EQ_BackgroundService.consumed_frame ==
                  EQ_DebugProcessFrames) &&
                 (EQ_BackgroundService.consumed_kind ==
                  EQ_BACKGROUND_BUILDER)) ? 1 : 0;
            analyzer_serviced =
                ((EQ_BackgroundService.consumed_frame_valid != 0) &&
                 (EQ_BackgroundService.consumed_frame ==
                  EQ_DebugProcessFrames) &&
                 (EQ_BackgroundService.consumed_kind ==
                  EQ_BACKGROUND_ANALYZER)) ? 1 : 0;
            lcd_eligible = 0;
            lcd_has_eligible_job = EqualizerDisplay_HasEligibleJob(
                EQ_DebugProcessFrames);
            if (EqualizerLcdPolicy_CanService(
                    &lcd_policy, EQ_DebugProcessFrames,
                    FLAG_AD, FLAG_DA, flag_ad_done,
                    EQ_FrameServicePending,
                    audio_serviced,
#if EQ_ENABLE_PROJECT33_TOUCH != 0
                    touch_serviced,
#else
                    0,
#endif
                    builder_serviced, analyzer_serviced,
                    lcd_has_eligible_job))
            {
                lcd_audio_before = ((FLAG_AD != 0) ? 0x01U : 0U) |
                                   ((FLAG_DA != 0) ? 0x02U : 0U) |
                                   ((flag_ad_done != 0) ? 0x04U : 0U) |
                                   ((EQ_FrameServicePending != 0U) ?
                                    0x08U : 0U);
                lcd_policy_decision = EqualizerLcdPolicy_Decide(
                    &lcd_policy, EQ_DebugProcessFrames,
                    FLAG_AD, FLAG_DA, flag_ad_done,
                    EQ_FrameServicePending,
                    (lcd_audio_before & 0x01U) != 0U,
                    (lcd_audio_before & 0x02U) != 0U,
                    (lcd_audio_before & 0x04U) != 0U,
                    (lcd_audio_before & 0x08U) != 0U,
                    audio_serviced,
#if EQ_ENABLE_PROJECT33_TOUCH != 0
                    touch_serviced,
#else
                    0,
#endif
                    builder_serviced, analyzer_serviced,
                    lcd_has_eligible_job);
                if (lcd_policy_decision == EQ_LCD_POLICY_DEFER)
                {
                    if (EqualizerLcdPolicy_RecordDeferred(
                            &lcd_policy, EQ_DebugProcessFrames))
                    {
                        EQ_DebugLcdDeferredAudioCount++;
#if EQ_ENABLE_LCD_JOB_TIMING_CAPTURE != 0
                        EqualizerDisplay_RecordDeferredByAudio(
                            EQ_DebugProcessFrames);
#endif
                    }
                }
                else if (lcd_policy_decision == EQ_LCD_POLICY_SERVICE)
                {
                    lcd_eligible = 1;
                }
            }
            else if ((audio_serviced != 0) &&
                     (lcd_has_eligible_job != 0) &&
                     EqualizerLcdPolicy_RecordDeferred(
                         &lcd_policy, EQ_DebugProcessFrames))
            {
                EQ_DebugLcdDeferredAudioCount++;
#if EQ_ENABLE_LCD_JOB_TIMING_CAPTURE != 0
                EqualizerDisplay_RecordDeferredByAudio(
                    EQ_DebugProcessFrames);
#endif
            }
#else
            background_kind = EQ_BACKGROUND_NONE;
#endif
            background_kind = EqualizerBackgroundService_Decide(
                &EQ_BackgroundService, EQ_DebugProcessFrames,
                FLAG_AD, FLAG_DA, flag_ad_done,
                EQ_FrameServicePending,
                builder_eligible,
                analyzer_eligible,
                uart_eligible,
#if EQ_ENABLE_LCD_DISPLAY != 0
                lcd_eligible);
#else
                0);
#endif
            if ((background_kind == EQ_BACKGROUND_NONE) &&
                (builder_eligible != 0) &&
                (EqualizerBackgroundService_IsAudioSafeFinalCheck(
                    FLAG_AD, FLAG_DA, flag_ad_done,
                    EQ_FrameServicePending) == 0))
            {
                EQ_DebugBuilderDeferredAudioCount++;
            }
#if EQ_ENABLE_AUDIO_FEATURE_ANALYZER != 0
            if ((EQ_AnalyzerLastEnabled != 0U) &&
                (EQ_DebugAnalyzerPending != 0U) &&
                (builder_eligible != 0))
            {
                EQ_RecordAnalyzerDeferred();
            }
#endif
            if (background_kind == EQ_BACKGROUND_BUILDER)
            {
                unsigned long builder_cycles;
#if defined(__TI_COMPILER_VERSION__) || defined(__TMS320C6X__)
                unsigned int builder_cycle_start;

                builder_cycle_start = TSCL;
#endif
                if (EqualizerBackgroundService_IsAudioSafeFinalCheck(
                        FLAG_AD, FLAG_DA, flag_ad_done,
                        EQ_FrameServicePending) == 0)
                {
                    EQ_DebugBuilderDeferredAudioCount++;
                    continue;
                }
                builder_audio_before =
                    ((FLAG_AD != 0) ? 0x01U : 0U) |
                    ((FLAG_DA != 0) ? 0x02U : 0U) |
                    ((flag_ad_done != 0) ? 0x04U : 0U) |
                    ((EQ_FrameServicePending != 0U) ? 0x08U : 0U);
                builder_result = EqualizerControl_ServiceOneBuilderSlice(
                    &EQ_BoardControl);
                builder_audio_after =
                    ((FLAG_AD != 0) ? 0x01U : 0U) |
                    ((FLAG_DA != 0) ? 0x02U : 0U) |
                    ((flag_ad_done != 0) ? 0x04U : 0U) |
                    ((EQ_FrameServicePending != 0U) ? 0x08U : 0U);
                if ((builder_audio_before == 0U) &&
                    (builder_audio_after != 0U))
                {
                    EQ_DebugBuilderAudioArrivedDuringSliceCount++;
                }
#if defined(__TI_COMPILER_VERSION__) || defined(__TMS320C6X__)
                builder_cycles =
                    (unsigned long)(TSCL - builder_cycle_start);
#else
                builder_cycles = 0UL;
#endif
                EQ_UpdateBuilderTiming(builder_cycles);
                if (builder_result == EQ_BUILDER_WORKED)
                {
                    EqualizerBackgroundService_Record(
                        &EQ_BackgroundService, EQ_DebugProcessFrames,
                        EQ_BACKGROUND_BUILDER);
                }
                EQ_UpdateDebugGains();
                continue;
            }
#if EQ_ENABLE_AUDIO_FEATURE_ANALYZER != 0
            if (background_kind == EQ_BACKGROUND_ANALYZER)
            {
                if (!EqualizerAnalyzer_CanService(
                        FLAG_AD, FLAG_DA, flag_ad_done,
                        EQ_FrameServicePending,
                        builder_eligible,
                        EQ_AnalyzerLastEnabled,
                        EQ_DebugAnalyzerPending))
                {
                    EQ_RecordAnalyzerDeferred();
                    continue;
                }
                analyzer_audio_before =
                    ((FLAG_AD != 0) ? 0x01U : 0U) |
                    ((FLAG_DA != 0) ? 0x02U : 0U) |
                    ((flag_ad_done != 0) ? 0x04U : 0U) |
                    ((EQ_FrameServicePending != 0U) ? 0x08U : 0U);
                analyzer_result = EQ_ServiceAnalyzer();
                analyzer_audio_after =
                    ((FLAG_AD != 0) ? 0x01U : 0U) |
                    ((FLAG_DA != 0) ? 0x02U : 0U) |
                    ((flag_ad_done != 0) ? 0x04U : 0U) |
                    ((EQ_FrameServicePending != 0U) ? 0x08U : 0U);
                if ((analyzer_audio_before == 0U) &&
                    (analyzer_audio_after != 0U))
                {
                    EQ_DebugAnalyzerAudioArrivedCount++;
                }
                (void)analyzer_result;
                EqualizerBackgroundService_Record(
                    &EQ_BackgroundService, EQ_DebugProcessFrames,
                    EQ_BACKGROUND_ANALYZER);
                continue;
            }
            if (background_kind == EQ_BACKGROUND_UART)
            {
                if ((EqualizerBackgroundService_IsAudioSafeFinalCheck(
                         FLAG_AD, FLAG_DA, flag_ad_done,
                         EQ_FrameServicePending) == 0) ||
                    (builder_eligible != 0) ||
                    (EQ_DebugAnalyzerEnabled == 0U) ||
                    (EQ_DebugAnalyzerValid == 0U) ||
                    (EQ_DebugAnalyzerWarmup == 0U) ||
                    (EQ_DebugUartFeatureAuditPending != 0U) ||
                    (EQ_DebugUartFeatureRequest == 0U))
                {
                    continue;
                }
                EQ_UartReportFeatureOnce(flag_ad_done);
                EqualizerBackgroundService_Record(
                    &EQ_BackgroundService, EQ_DebugProcessFrames,
                    EQ_BACKGROUND_UART);
                continue;
            }
#endif
#if EQ_ENABLE_LCD_DISPLAY != 0
            if (background_kind == EQ_BACKGROUND_LCD)
            {
                if (EqualizerBackgroundService_IsAudioSafeFinalCheck(
                        FLAG_AD, FLAG_DA, flag_ad_done,
                        EQ_FrameServicePending) == 0)
                {
                    continue;
                }
                lcd_job = EqualizerDisplay_ServiceOneJob(
                    EQ_DebugProcessFrames);
                lcd_audio_after = ((FLAG_AD != 0) ? 0x01U : 0U) |
                                  ((FLAG_DA != 0) ? 0x02U : 0U) |
                                  ((flag_ad_done != 0) ? 0x04U : 0U) |
                                  ((EQ_FrameServicePending != 0U) ?
                                   0x08U : 0U);
                if ((lcd_audio_before == 0U) && (lcd_audio_after != 0U))
                {
                    EQ_DebugLcdAudioArrivedDuringDrawCount++;
#if EQ_ENABLE_LCD_JOB_TIMING_CAPTURE != 0
                    EqualizerDisplay_RecordAudioArrivalDuringDraw();
#endif
                    EqualizerCapture_NotifyEvent(
                        EQ_CAPTURE_TRIGGER_AUDIO_DURING_LCD);
                }
                if (lcd_job != EQ_LCD_JOB_NONE)
                {
                    EqualizerCapture_NotifyEvent(
                        EQ_CAPTURE_TRIGGER_LCD_JOB);
                    EqualizerLcdPolicy_RecordService(
                        &lcd_policy, EQ_DebugProcessFrames, 1);
                    EqualizerBackgroundService_Record(
                        &EQ_BackgroundService, EQ_DebugProcessFrames,
                        EQ_BACKGROUND_LCD);
                }
                continue;
            }
#endif
        }

#if EQ_ENABLE_LCD_DISPLAY != 0
        else if ((EqualizerDisplay_HasEligibleJob(
                      EQ_DebugProcessFrames) != 0) &&
                 ((FLAG_AD != 0) || (FLAG_DA != 0) ||
                  (flag_ad_done != 0) ||
                  (EQ_FrameServicePending != 0U)) &&
                 EqualizerLcdPolicy_RecordDeferred(
                     &lcd_policy, EQ_DebugProcessFrames))
        {
            EQ_DebugLcdDeferredAudioCount++;
#if EQ_ENABLE_LCD_JOB_TIMING_CAPTURE != 0
            EqualizerDisplay_RecordDeferredByAudio(
                EQ_DebugProcessFrames);
#endif
        }
#endif
#if EQ_ENABLE_AUDIO_FEATURE_ANALYZER != 0
        if ((EQ_AnalyzerLastEnabled != 0U) &&
            (EQ_DebugAnalyzerPending != 0U) &&
            ((FLAG_AD != 0) || (FLAG_DA != 0) ||
             (flag_ad_done != 0) ||
             (EQ_FrameServicePending != 0U)))
        {
            EQ_RecordAnalyzerDeferred();
        }
#endif
    }
}

#else

void Equalizer_Flow_Example(void)
{
}

#endif
