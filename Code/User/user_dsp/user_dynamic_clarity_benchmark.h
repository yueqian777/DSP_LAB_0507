/**
 * user_dynamic_clarity_benchmark.h
 *
 * Compile-gated board microbenchmark. The production build contains no
 * benchmark state or control path when the gate is zero.
 */

#ifndef _USER_DYNAMIC_CLARITY_BENCHMARK_H_
#define _USER_DYNAMIC_CLARITY_BENCHMARK_H_

#ifndef EQ_ENABLE_DYNAMIC_CLARITY_BENCHMARK
#define EQ_ENABLE_DYNAMIC_CLARITY_BENCHMARK 0
#endif

#if EQ_ENABLE_DYNAMIC_CLARITY_BENCHMARK != 0

#if (EQ_ENABLE_DYNAMIC_CLARITY == 0) || (EQ_ENABLE_SMART_BASS == 0)
#error Dynamic Clarity benchmark requires Dynamic Clarity and Smart Bass.
#endif

#define EQ_DYNAMIC_CLARITY_BENCHMARK_INPUT_COUNT     4
#define EQ_DYNAMIC_CLARITY_BENCHMARK_CASE_COUNT      7
#define EQ_DYNAMIC_CLARITY_BENCHMARK_MODULE_COUNT    2
#define EQ_DYNAMIC_CLARITY_BENCHMARK_JOB_COUNT       \
    (EQ_DYNAMIC_CLARITY_BENCHMARK_INPUT_COUNT *      \
     EQ_DYNAMIC_CLARITY_BENCHMARK_CASE_COUNT *       \
     EQ_DYNAMIC_CLARITY_BENCHMARK_MODULE_COUNT)
#define EQ_DYNAMIC_CLARITY_BENCHMARK_WARMUP_COUNT    64
#define EQ_DYNAMIC_CLARITY_BENCHMARK_MEASURED_COUNT  4096
#define EQ_DYNAMIC_CLARITY_BENCHMARK_FRAME_LEN       1024
#define EQ_DYNAMIC_CLARITY_BENCHMARK_CYCLE_COUNT     \
    (EQ_DYNAMIC_CLARITY_BENCHMARK_JOB_COUNT *        \
     EQ_DYNAMIC_CLARITY_BENCHMARK_MEASURED_COUNT)

extern volatile unsigned int
    EQ_DebugDynamicClarityBenchmarkCompiled;
extern volatile unsigned int
    EQ_DebugDynamicClarityBenchmarkAudioServicesStarted;
extern volatile unsigned int EQ_DebugDynamicClarityBenchmarkStarted;
extern volatile unsigned int EQ_DebugDynamicClarityBenchmarkRunning;
extern volatile unsigned int EQ_DebugDynamicClarityBenchmarkReady;
extern volatile int EQ_DebugDynamicClarityBenchmarkStatus;
extern volatile unsigned int EQ_DebugDynamicClarityBenchmarkCompletedJobs;
extern volatile unsigned long
    EQ_DebugDynamicClarityBenchmarkFirstCall[
        EQ_DYNAMIC_CLARITY_BENCHMARK_JOB_COUNT];
extern volatile unsigned long
    EQ_DebugDynamicClarityBenchmarkWarmMax[
        EQ_DYNAMIC_CLARITY_BENCHMARK_JOB_COUNT];
extern volatile unsigned long
    EQ_DebugDynamicClarityBenchmarkChecksum[
        EQ_DYNAMIC_CLARITY_BENCHMARK_JOB_COUNT];
extern volatile unsigned long
    EQ_DebugDynamicClarityBenchmarkCycles[
        EQ_DYNAMIC_CLARITY_BENCHMARK_CYCLE_COUNT];
extern volatile unsigned long
    EQ_DebugDynamicClarityBenchmarkDynamicSaturationCount;
extern volatile unsigned long
    EQ_DebugDynamicClarityBenchmarkDynamicNonFiniteCount;
extern volatile unsigned long
    EQ_DebugDynamicClarityBenchmarkSmartSaturationCount;
extern volatile unsigned long
    EQ_DebugDynamicClarityBenchmarkSmartNonFiniteCount;

void DynamicClarityBenchmark_Run(void);

#endif /* EQ_ENABLE_DYNAMIC_CLARITY_BENCHMARK */

#endif /* _USER_DYNAMIC_CLARITY_BENCHMARK_H_ */
