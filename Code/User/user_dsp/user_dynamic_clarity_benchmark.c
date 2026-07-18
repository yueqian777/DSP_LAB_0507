/**
 * user_dynamic_clarity_benchmark.c
 *
 * Board-only cycle capture for Dynamic Clarity and Smart Bass. When the
 * additional Harshness Guard benchmark gate is set, the same isolated runner
 * measures all three modules before audio services start.
 */

#include "user_equalizer_flow.h"
#include "user_dynamic_clarity_benchmark.h"

#if EQ_ENABLE_DYNAMIC_CLARITY_BENCHMARK != 0

#include "user_dynamic_clarity.h"
#include "user_smart_bass.h"
#if EQ_ENABLE_HARSHNESS_GUARD_BENCHMARK != 0
#include "user_harshness_guard.h"
#endif
#include "math.h"
#include "string.h"

#if defined(__TI_COMPILER_VERSION__) || defined(__TMS320C6X__)
#include "c6x.h"
#else
#error Dynamic Clarity benchmark is board-only.
#endif

#define EQ_BENCHMARK_MODULE_DYNAMIC 0
#define EQ_BENCHMARK_MODULE_SMART   1
#if EQ_ENABLE_HARSHNESS_GUARD_BENCHMARK != 0
#define EQ_BENCHMARK_MODULE_HARSHNESS 2
#endif

#define EQ_BENCHMARK_CASE_IDENTITY       0
#define EQ_BENCHMARK_CASE_STABLE_1       1
#define EQ_BENCHMARK_CASE_STABLE_4       2
#define EQ_BENCHMARK_CASE_TRANSITION_0_1 3
#define EQ_BENCHMARK_CASE_TRANSITION_1_2 4
#define EQ_BENCHMARK_CASE_TRANSITION_4_3 5
#define EQ_BENCHMARK_CASE_TRANSITION_1_0 6

#define EQ_BENCHMARK_PI 3.14159265358979323846

#pragma DATA_SECTION(EQ_BenchmarkInput, ".subband_l2")
#pragma DATA_SECTION(EQ_BenchmarkOutput, ".subband_l2")
#pragma DATA_SECTION(EQ_BenchmarkDynamicState, ".subband_l2")
#pragma DATA_SECTION(EQ_BenchmarkDynamicTemplate, ".subband_l2")
#pragma DATA_SECTION(EQ_BenchmarkSmartState, ".subband_l2")
#pragma DATA_SECTION(EQ_BenchmarkSmartTemplate, ".subband_l2")
#if EQ_ENABLE_HARSHNESS_GUARD_BENCHMARK != 0
#pragma DATA_SECTION(EQ_BenchmarkHarshnessState, ".subband_l2")
#pragma DATA_SECTION(EQ_BenchmarkHarshnessTemplate, ".subband_l2")
#endif

static short EQ_BenchmarkInput[EQ_DYNAMIC_CLARITY_BENCHMARK_FRAME_LEN];
static short EQ_BenchmarkOutput[EQ_DYNAMIC_CLARITY_BENCHMARK_FRAME_LEN];
static DYNAMIC_CLARITY_STATE EQ_BenchmarkDynamicState;
static DYNAMIC_CLARITY_STATE EQ_BenchmarkDynamicTemplate;
static SMART_BASS_STATE EQ_BenchmarkSmartState;
static SMART_BASS_STATE EQ_BenchmarkSmartTemplate;
#if EQ_ENABLE_HARSHNESS_GUARD_BENCHMARK != 0
static HARSHNESS_GUARD_STATE EQ_BenchmarkHarshnessState;
static HARSHNESS_GUARD_STATE EQ_BenchmarkHarshnessTemplate;
#endif

volatile unsigned int EQ_DebugDynamicClarityBenchmarkCompiled = 1U;
volatile unsigned int
    EQ_DebugDynamicClarityBenchmarkAudioServicesStarted = 0U;
volatile unsigned int EQ_DebugDynamicClarityBenchmarkStarted = 0U;
volatile unsigned int EQ_DebugDynamicClarityBenchmarkRunning = 0U;
volatile unsigned int EQ_DebugDynamicClarityBenchmarkReady = 0U;
volatile int EQ_DebugDynamicClarityBenchmarkStatus = 0;
volatile unsigned int EQ_DebugDynamicClarityBenchmarkCompletedJobs = 0U;
volatile unsigned long
    EQ_DebugDynamicClarityBenchmarkFirstCall[
        EQ_DYNAMIC_CLARITY_BENCHMARK_JOB_COUNT];
volatile unsigned long
    EQ_DebugDynamicClarityBenchmarkWarmMax[
        EQ_DYNAMIC_CLARITY_BENCHMARK_JOB_COUNT];
volatile unsigned long
    EQ_DebugDynamicClarityBenchmarkChecksum[
        EQ_DYNAMIC_CLARITY_BENCHMARK_JOB_COUNT];
volatile unsigned long
    EQ_DebugDynamicClarityBenchmarkCycles[
        EQ_DYNAMIC_CLARITY_BENCHMARK_CYCLE_COUNT];
volatile unsigned long
    EQ_DebugDynamicClarityBenchmarkDynamicSaturationCount = 0UL;
volatile unsigned long
    EQ_DebugDynamicClarityBenchmarkDynamicNonFiniteCount = 0UL;
volatile unsigned long
    EQ_DebugDynamicClarityBenchmarkSmartSaturationCount = 0UL;
volatile unsigned long
    EQ_DebugDynamicClarityBenchmarkSmartNonFiniteCount = 0UL;
#if EQ_ENABLE_HARSHNESS_GUARD_BENCHMARK != 0
volatile unsigned int EQ_DebugHarshnessGuardBenchmarkCompiled = 1U;
volatile unsigned long
    EQ_DebugDynamicClarityBenchmarkHarshnessSaturationCount = 0UL;
volatile unsigned long
    EQ_DebugDynamicClarityBenchmarkHarshnessNonFiniteCount = 0UL;
#endif

static short EQ_BenchmarkRoundAndClamp(double value)
{
    long rounded;

    rounded = (long)((value >= 0.0) ? (value + 0.5) : (value - 0.5));
    if (rounded > 32767L)
    {
        rounded = 32767L;
    }
    else if (rounded < -32768L)
    {
        rounded = -32768L;
    }
    return (short)rounded;
}

static double EQ_BenchmarkSine(double frequency_hz, int sample)
{
    return sin(2.0 * EQ_BENCHMARK_PI * frequency_hz * (double)sample /
               50000.0);
}

static void EQ_BenchmarkPrepareInput(int input_index)
{
    unsigned int random_state;
    int sample;
    double value;

    random_state = 0x13579bdfU;
    for (sample = 0;
         sample < EQ_DYNAMIC_CLARITY_BENCHMARK_FRAME_LEN;
         sample++)
    {
        if (input_index == 0)
        {
            value = 10000.0 * EQ_BenchmarkSine(400.0, sample);
        }
        else if (input_index == 1)
        {
            value = 6000.0 * EQ_BenchmarkSine(400.0, sample) +
                    6000.0 * EQ_BenchmarkSine(1953.125, sample);
        }
        else if (input_index == 2)
        {
            value = 3600.0 * EQ_BenchmarkSine(97.65625, sample) +
                    3000.0 * EQ_BenchmarkSine(400.0, sample) +
                    2600.0 * EQ_BenchmarkSine(976.5625, sample) +
                    2200.0 * EQ_BenchmarkSine(1953.125, sample) +
                    1600.0 * EQ_BenchmarkSine(5859.375, sample);
        }
        else
        {
            random_state = random_state * 1664525U + 1013904223U;
            value = (double)((int)((random_state >> 16) & 0x3fffU) -
                             8192);
        }
        EQ_BenchmarkInput[sample] = EQ_BenchmarkRoundAndClamp(value);
    }
}

static void EQ_BenchmarkCaseLevels(
    int module_index, int case_index, int *active_level, int *pending_level,
    int *transition_active)
{
    int medium_level;

    medium_level = 4;
#if EQ_ENABLE_HARSHNESS_GUARD_BENCHMARK != 0
    if (module_index == EQ_BENCHMARK_MODULE_HARSHNESS)
    {
        medium_level = 3;
    }
#else
    (void)module_index;
#endif
    *transition_active = 0;
    if (case_index == EQ_BENCHMARK_CASE_IDENTITY)
    {
        *active_level = 0;
        *pending_level = 0;
    }
    else if (case_index == EQ_BENCHMARK_CASE_STABLE_1)
    {
        *active_level = 1;
        *pending_level = 1;
    }
    else if (case_index == EQ_BENCHMARK_CASE_STABLE_4)
    {
        *active_level = medium_level;
        *pending_level = medium_level;
    }
    else
    {
        *transition_active = 1;
        if (case_index == EQ_BENCHMARK_CASE_TRANSITION_0_1)
        {
            *active_level = 0;
            *pending_level = 1;
        }
        else if (case_index == EQ_BENCHMARK_CASE_TRANSITION_1_2)
        {
            *active_level = 1;
            *pending_level = 2;
        }
        else if (case_index == EQ_BENCHMARK_CASE_TRANSITION_4_3)
        {
            *active_level = medium_level;
            *pending_level = medium_level - 1;
        }
        else
        {
            *active_level = 1;
            *pending_level = 0;
        }
    }
}

static void EQ_BenchmarkPrepareDynamicState(int case_index)
{
    int active_level;
    int pending_level;
    int transition_active;

    DynamicClarity_Init(&EQ_BenchmarkDynamicState);
    EQ_BenchmarkCaseLevels(EQ_BENCHMARK_MODULE_DYNAMIC, case_index,
                           &active_level, &pending_level,
                           &transition_active);
    EQ_BenchmarkDynamicState.active_level = active_level;
    EQ_BenchmarkDynamicState.target_level = pending_level;
    EQ_BenchmarkDynamicState.pending_level = pending_level;
    EQ_BenchmarkDynamicState.requested_enabled = 1;
    EQ_BenchmarkDynamicState.processing_active =
        ((active_level != 0) || (transition_active != 0)) ? 1 : 0;
    EQ_BenchmarkDynamicState.transition_active = transition_active;
    EQ_BenchmarkDynamicState.transition_total =
        DYNAMIC_CLARITY_TRANSITION_SAMPLES;
    EQ_BenchmarkDynamicState.transition_remaining =
        (transition_active != 0) ?
        DYNAMIC_CLARITY_TRANSITION_SAMPLES : 0;
    EQ_BenchmarkDynamicState.pending_state =
        EQ_BenchmarkDynamicState.active_state;
    EQ_BenchmarkDynamicTemplate = EQ_BenchmarkDynamicState;
}

static void EQ_BenchmarkPrepareSmartState(int case_index)
{
    int active_level;
    int pending_level;
    int transition_active;

    SmartBass_Init(&EQ_BenchmarkSmartState);
    EQ_BenchmarkCaseLevels(EQ_BENCHMARK_MODULE_SMART, case_index,
                           &active_level, &pending_level,
                           &transition_active);
    EQ_BenchmarkSmartState.active_level = active_level;
    EQ_BenchmarkSmartState.target_level = pending_level;
    EQ_BenchmarkSmartState.pending_level = pending_level;
    EQ_BenchmarkSmartState.requested_enabled = 1;
    EQ_BenchmarkSmartState.processing_active =
        ((active_level != 0) || (transition_active != 0)) ? 1 : 0;
    EQ_BenchmarkSmartState.transition_active = transition_active;
    EQ_BenchmarkSmartState.transition_total = SMART_BASS_TRANSITION_SAMPLES;
    EQ_BenchmarkSmartState.transition_remaining =
        (transition_active != 0) ? SMART_BASS_TRANSITION_SAMPLES : 0;
    EQ_BenchmarkSmartState.pending_state = EQ_BenchmarkSmartState.active_state;
    EQ_BenchmarkSmartTemplate = EQ_BenchmarkSmartState;
}

#if EQ_ENABLE_HARSHNESS_GUARD_BENCHMARK != 0
static void EQ_BenchmarkPrepareHarshnessState(int case_index)
{
    int active_level;
    int pending_level;
    int transition_active;

    HarshnessGuard_Init(&EQ_BenchmarkHarshnessState);
    EQ_BenchmarkCaseLevels(EQ_BENCHMARK_MODULE_HARSHNESS, case_index,
                           &active_level, &pending_level,
                           &transition_active);
    EQ_BenchmarkHarshnessState.active_level = active_level;
    EQ_BenchmarkHarshnessState.target_level = pending_level;
    EQ_BenchmarkHarshnessState.pending_level = pending_level;
    EQ_BenchmarkHarshnessState.requested_enabled = 1;
    EQ_BenchmarkHarshnessState.processing_active =
        ((active_level != 0) || (transition_active != 0)) ? 1 : 0;
    EQ_BenchmarkHarshnessState.transition_active = transition_active;
    EQ_BenchmarkHarshnessState.transition_total =
        HARSHNESS_GUARD_TRANSITION_SAMPLES;
    EQ_BenchmarkHarshnessState.transition_remaining =
        (transition_active != 0) ?
        HARSHNESS_GUARD_TRANSITION_SAMPLES : 0;
    EQ_BenchmarkHarshnessState.pending_state =
        EQ_BenchmarkHarshnessState.active_state;
    EQ_BenchmarkHarshnessTemplate = EQ_BenchmarkHarshnessState;
}
#endif

static int EQ_BenchmarkIsTransitionCase(int case_index)
{
    return (case_index >= EQ_BENCHMARK_CASE_TRANSITION_0_1) ? 1 : 0;
}

static unsigned long EQ_BenchmarkInvokeDynamic(int reset_transition)
{
    unsigned int cycle_start;
    unsigned long saturation_before;
    unsigned long nonfinite_before;
    int result;

    if (reset_transition != 0)
    {
        EQ_BenchmarkDynamicState = EQ_BenchmarkDynamicTemplate;
    }
    memcpy(EQ_BenchmarkOutput, EQ_BenchmarkInput,
           sizeof(EQ_BenchmarkOutput));
    saturation_before = EQ_BenchmarkDynamicState.saturation_count;
    nonfinite_before = EQ_BenchmarkDynamicState.nonfinite_count;
    cycle_start = TSCL;
    result = DynamicClarity_ProcessFrame(
        &EQ_BenchmarkDynamicState, EQ_BenchmarkOutput, EQ_BenchmarkOutput,
        EQ_DYNAMIC_CLARITY_BENCHMARK_FRAME_LEN);
    cycle_start = TSCL - cycle_start;
    EQ_DebugDynamicClarityBenchmarkDynamicSaturationCount +=
        EQ_BenchmarkDynamicState.saturation_count - saturation_before;
    EQ_DebugDynamicClarityBenchmarkDynamicNonFiniteCount +=
        EQ_BenchmarkDynamicState.nonfinite_count - nonfinite_before;
    if (result < 0)
    {
        EQ_DebugDynamicClarityBenchmarkStatus = -1;
    }
    return (unsigned long)cycle_start;
}

static unsigned long EQ_BenchmarkInvokeSmart(int reset_transition)
{
    unsigned int cycle_start;
    unsigned long saturation_before;
    unsigned long nonfinite_before;
    int result;

    if (reset_transition != 0)
    {
        EQ_BenchmarkSmartState = EQ_BenchmarkSmartTemplate;
    }
    memcpy(EQ_BenchmarkOutput, EQ_BenchmarkInput,
           sizeof(EQ_BenchmarkOutput));
    saturation_before = EQ_BenchmarkSmartState.saturation_count;
    nonfinite_before = EQ_BenchmarkSmartState.nonfinite_count;
    cycle_start = TSCL;
    result = SmartBass_ProcessFrame(
        &EQ_BenchmarkSmartState, EQ_BenchmarkOutput, EQ_BenchmarkOutput,
        EQ_DYNAMIC_CLARITY_BENCHMARK_FRAME_LEN);
    cycle_start = TSCL - cycle_start;
    EQ_DebugDynamicClarityBenchmarkSmartSaturationCount +=
        EQ_BenchmarkSmartState.saturation_count - saturation_before;
    EQ_DebugDynamicClarityBenchmarkSmartNonFiniteCount +=
        EQ_BenchmarkSmartState.nonfinite_count - nonfinite_before;
    if (result < 0)
    {
        EQ_DebugDynamicClarityBenchmarkStatus = -2;
    }
    return (unsigned long)cycle_start;
}

#if EQ_ENABLE_HARSHNESS_GUARD_BENCHMARK != 0
static unsigned long EQ_BenchmarkInvokeHarshness(int reset_transition)
{
    unsigned int cycle_start;
    unsigned long saturation_before;
    unsigned long nonfinite_before;
    int result;

    if (reset_transition != 0)
    {
        EQ_BenchmarkHarshnessState = EQ_BenchmarkHarshnessTemplate;
    }
    memcpy(EQ_BenchmarkOutput, EQ_BenchmarkInput,
           sizeof(EQ_BenchmarkOutput));
    saturation_before = EQ_BenchmarkHarshnessState.saturation_count;
    nonfinite_before = EQ_BenchmarkHarshnessState.nonfinite_count;
    cycle_start = TSCL;
    result = HarshnessGuard_ProcessFrame(
        &EQ_BenchmarkHarshnessState,
        EQ_BenchmarkOutput, EQ_BenchmarkOutput,
        EQ_DYNAMIC_CLARITY_BENCHMARK_FRAME_LEN);
    cycle_start = TSCL - cycle_start;
    EQ_DebugDynamicClarityBenchmarkHarshnessSaturationCount +=
        EQ_BenchmarkHarshnessState.saturation_count - saturation_before;
    EQ_DebugDynamicClarityBenchmarkHarshnessNonFiniteCount +=
        EQ_BenchmarkHarshnessState.nonfinite_count - nonfinite_before;
    if (result < 0)
    {
        EQ_DebugDynamicClarityBenchmarkStatus = -3;
    }
    return (unsigned long)cycle_start;
}
#endif

static unsigned long EQ_BenchmarkInvoke(
    int module_index, int reset_transition)
{
    if (module_index == EQ_BENCHMARK_MODULE_DYNAMIC)
    {
        return EQ_BenchmarkInvokeDynamic(reset_transition);
    }
#if EQ_ENABLE_HARSHNESS_GUARD_BENCHMARK != 0
    if (module_index == EQ_BENCHMARK_MODULE_HARSHNESS)
    {
        return EQ_BenchmarkInvokeHarshness(reset_transition);
    }
#endif
    return EQ_BenchmarkInvokeSmart(reset_transition);
}

static unsigned long EQ_BenchmarkOutputChecksum(void)
{
    unsigned long checksum;
    int sample;

    checksum = 2166136261UL;
    for (sample = 0;
         sample < EQ_DYNAMIC_CLARITY_BENCHMARK_FRAME_LEN;
         sample++)
    {
        checksum ^= (unsigned long)(unsigned short)EQ_BenchmarkOutput[sample];
        checksum *= 16777619UL;
    }
    return checksum;
}

static void EQ_BenchmarkRunJob(
    int module_index, int input_index, int case_index, int job_index)
{
    unsigned long cycles;
    unsigned long warm_max;
    unsigned long offset;
    int reset_transition;
    int iteration;

    reset_transition = EQ_BenchmarkIsTransitionCase(case_index);
    if (module_index == EQ_BENCHMARK_MODULE_DYNAMIC)
    {
        EQ_BenchmarkPrepareDynamicState(case_index);
    }
#if EQ_ENABLE_HARSHNESS_GUARD_BENCHMARK != 0
    else if (module_index == EQ_BENCHMARK_MODULE_HARSHNESS)
    {
        EQ_BenchmarkPrepareHarshnessState(case_index);
    }
#endif
    else
    {
        EQ_BenchmarkPrepareSmartState(case_index);
    }

    EQ_DebugDynamicClarityBenchmarkFirstCall[job_index] =
        EQ_BenchmarkInvoke(module_index, reset_transition);
    warm_max = 0UL;
    for (iteration = 0;
         iteration < EQ_DYNAMIC_CLARITY_BENCHMARK_WARMUP_COUNT;
         iteration++)
    {
        cycles = EQ_BenchmarkInvoke(
            module_index,
            (reset_transition != 0) && ((iteration % 4) == 0));
        if (cycles > warm_max)
        {
            warm_max = cycles;
        }
    }
    EQ_DebugDynamicClarityBenchmarkWarmMax[job_index] = warm_max;

    offset = (unsigned long)job_index *
             EQ_DYNAMIC_CLARITY_BENCHMARK_MEASURED_COUNT;
    for (iteration = 0;
         iteration < EQ_DYNAMIC_CLARITY_BENCHMARK_MEASURED_COUNT;
         iteration++)
    {
        EQ_DebugDynamicClarityBenchmarkCycles[offset + iteration] =
            EQ_BenchmarkInvoke(
                module_index,
                (reset_transition != 0) && ((iteration % 4) == 0));
    }
    EQ_DebugDynamicClarityBenchmarkChecksum[job_index] =
        EQ_BenchmarkOutputChecksum() ^ (unsigned long)input_index;
}

void DynamicClarityBenchmark_Run(void)
{
    int module_index;
    int input_index;
    int case_index;
    int job_index;

    EQ_DebugDynamicClarityBenchmarkStarted = 1U;
    EQ_DebugDynamicClarityBenchmarkRunning = 1U;
    EQ_DebugDynamicClarityBenchmarkReady = 0U;
    EQ_DebugDynamicClarityBenchmarkStatus = 0;
    EQ_DebugDynamicClarityBenchmarkCompletedJobs = 0U;
    EQ_DebugDynamicClarityBenchmarkDynamicSaturationCount = 0UL;
    EQ_DebugDynamicClarityBenchmarkDynamicNonFiniteCount = 0UL;
    EQ_DebugDynamicClarityBenchmarkSmartSaturationCount = 0UL;
    EQ_DebugDynamicClarityBenchmarkSmartNonFiniteCount = 0UL;
#if EQ_ENABLE_HARSHNESS_GUARD_BENCHMARK != 0
    EQ_DebugDynamicClarityBenchmarkHarshnessSaturationCount = 0UL;
    EQ_DebugDynamicClarityBenchmarkHarshnessNonFiniteCount = 0UL;
#endif
    TSCL = 0U;

    job_index = 0;
    for (module_index = 0;
         module_index < EQ_DYNAMIC_CLARITY_BENCHMARK_MODULE_COUNT;
         module_index++)
    {
        for (input_index = 0;
             input_index < EQ_DYNAMIC_CLARITY_BENCHMARK_INPUT_COUNT;
             input_index++)
        {
            EQ_BenchmarkPrepareInput(input_index);
            for (case_index = 0;
                 case_index < EQ_DYNAMIC_CLARITY_BENCHMARK_CASE_COUNT;
                 case_index++)
            {
                EQ_BenchmarkRunJob(module_index, input_index,
                                   case_index, job_index);
                job_index++;
                EQ_DebugDynamicClarityBenchmarkCompletedJobs =
                    (unsigned int)job_index;
            }
        }
    }

    if ((EQ_DebugDynamicClarityBenchmarkStatus == 0) &&
        (EQ_DebugDynamicClarityBenchmarkDynamicSaturationCount == 0UL) &&
        (EQ_DebugDynamicClarityBenchmarkDynamicNonFiniteCount == 0UL) &&
        (EQ_DebugDynamicClarityBenchmarkSmartSaturationCount == 0UL) &&
        (EQ_DebugDynamicClarityBenchmarkSmartNonFiniteCount == 0UL)
#if EQ_ENABLE_HARSHNESS_GUARD_BENCHMARK != 0
        &&
        (EQ_DebugDynamicClarityBenchmarkHarshnessSaturationCount == 0UL) &&
        (EQ_DebugDynamicClarityBenchmarkHarshnessNonFiniteCount == 0UL)
#endif
        )
    {
        EQ_DebugDynamicClarityBenchmarkStatus = 1;
    }
    EQ_DebugDynamicClarityBenchmarkRunning = 0U;
    EQ_DebugDynamicClarityBenchmarkReady = 1U;
    asm(" SWBP 0 ");
}

#endif /* EQ_ENABLE_DYNAMIC_CLARITY_BENCHMARK */
