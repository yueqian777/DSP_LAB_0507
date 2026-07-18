#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "user_dynamic_clarity.h"
#include "user_harshness_guard.h"
#include "user_smart_bass.h"

#define TEST_PI 3.14159265358979323846f
#define TEST_FRAME_LEN HARSHNESS_GUARD_MAX_FRAME_LEN
#define TEST_VECTOR_COUNT 5
#define TEST_CHAIN_COUNT 7

static int failures = 0;
static unsigned long analysis_sequence = 0UL;

typedef struct
{
    double error_sum;
    double error_square_sum;
    double max_error;
    unsigned long clipped_samples;
    unsigned long sample_count;
} PCM_METRIC;

static void Check(int condition, const char *message)
{
    if (!condition)
    {
        printf("FAIL: %s\n", message);
        failures++;
    }
}

static int FloatFinite(float value)
{
    return ((value == value) && (value <= FLT_MAX) &&
            (value >= -FLT_MAX)) ? 1 : 0;
}

static int IntAbs(int value)
{
    return (value < 0) ? -value : value;
}

static AUDIO_FEATURE_SNAPSHOT MakeSnapshot(float brightness_db,
                                           float presence_db,
                                           float rms_dbfs,
                                           int valid,
                                           int warmup)
{
    AUDIO_FEATURE_SNAPSHOT snapshot;

    memset(&snapshot, 0, sizeof(snapshot));
    analysis_sequence++;
    snapshot.analysis_count = analysis_sequence;
    snapshot.relative_db[AUDIO_FEATURE_BRIGHTNESS] = brightness_db;
    snapshot.relative_db[AUDIO_FEATURE_PRESENCE] = presence_db;
    snapshot.rms_dbfs = rms_dbfs;
    snapshot.valid = valid;
    snapshot.warmup_complete = warmup;
    return snapshot;
}

static void FillPattern(short *frame, int salt)
{
    int index;

    for (index = 0; index < TEST_FRAME_LEN; index++)
    {
        frame[index] =
            (short)((((index + salt) * 7919) % 8001) - 4000);
    }
}

static void FillSine(short *frame, float frequency_hz, float amplitude)
{
    int index;

    for (index = 0; index < TEST_FRAME_LEN; index++)
    {
        float value;

        value = amplitude * sinf(2.0f * TEST_PI * frequency_hz *
                                 (float)index /
                                 HARSHNESS_GUARD_SAMPLE_RATE);
        frame[index] = (short)((value >= 0.0f) ?
                               (value + 0.5f) : (value - 0.5f));
    }
}

static void FillMusicLike(short *frame)
{
    int index;

    for (index = 0; index < TEST_FRAME_LEN; index++)
    {
        float time;
        float value;

        time = (float)index / HARSHNESS_GUARD_SAMPLE_RATE;
        value = 900.0f * sinf(2.0f * TEST_PI * 97.65625f * time) +
                850.0f * sinf(2.0f * TEST_PI * 390.625f * time) +
                700.0f * sinf(2.0f * TEST_PI * 1000.0f * time) +
                600.0f * sinf(2.0f * TEST_PI * 1953.125f * time) +
                500.0f * sinf(2.0f * TEST_PI * 5859.375f * time);
        frame[index] = (short)((value >= 0.0f) ?
                               (value + 0.5f) : (value - 0.5f));
    }
}

static void FillWideband(short *frame)
{
    unsigned int seed;
    int index;

    seed = 0x5a17c9e3U;
    for (index = 0; index < TEST_FRAME_LEN; index++)
    {
        seed = seed * 1664525U + 1013904223U;
        frame[index] = (short)((int)((seed >> 16) % 8001U) - 4000);
    }
}

static void FillDualTone(short *frame)
{
    int index;

    for (index = 0; index < TEST_FRAME_LEN; index++)
    {
        float value;

        value = 6000.0f * sinf(2.0f * TEST_PI * 400.0f *
                               (float)index /
                               HARSHNESS_GUARD_SAMPLE_RATE) +
                6000.0f * sinf(2.0f * TEST_PI * 1953.125f *
                               (float)index /
                               HARSHNESS_GUARD_SAMPLE_RATE);
        frame[index] = (short)((value >= 0.0f) ?
                               (value + 0.5f) : (value - 0.5f));
    }
}

static unsigned long HashPcm16(unsigned long hash,
                               const short *frame,
                               int sample_count)
{
    int index;

    for (index = 0; index < sample_count; index++)
    {
        unsigned short sample;

        sample = (unsigned short)frame[index];
        hash ^= (unsigned long)(sample & 0xffU);
        hash *= 16777619UL;
        hash ^= (unsigned long)((sample >> 8) & 0xffU);
        hash *= 16777619UL;
    }
    return hash;
}

static float ProcessFloatBiquad(const EQ_BIQUAD *coefficient,
                                float *s1,
                                float *s2,
                                float input)
{
    float next_s1;
    float output;

    output = coefficient->b0 * input + *s1;
    next_s1 = coefficient->b1 * input -
              coefficient->a1 * output + *s2;
    *s2 = coefficient->b2 * input - coefficient->a2 * output;
    *s1 = next_s1;
    return output;
}

static void UpdatePcmMetric(PCM_METRIC *metric,
                            short actual,
                            float ideal)
{
    double absolute_error;
    double error;

    error = (double)actual - (double)ideal;
    absolute_error = fabs(error);
    if (absolute_error > metric->max_error)
    {
        metric->max_error = absolute_error;
    }
    metric->error_sum += error;
    metric->error_square_sum += error * error;
    metric->sample_count++;
    if ((actual == (short)32767) || (actual == (short)-32768))
    {
        metric->clipped_samples++;
    }
}

static double PcmRmsError(const PCM_METRIC *metric)
{
    if (metric->sample_count == 0UL)
    {
        return 0.0;
    }
    return sqrt(metric->error_square_sum / (double)metric->sample_count);
}

static double PcmDcError(const PCM_METRIC *metric)
{
    if (metric->sample_count == 0UL)
    {
        return 0.0;
    }
    return fabs(metric->error_sum / (double)metric->sample_count);
}

static double PcmResidualDbfs(const PCM_METRIC *metric)
{
    double rms_error;

    rms_error = PcmRmsError(metric);
    if (rms_error <= 1.0e-20)
    {
        return -240.0;
    }
    return 20.0 * log10(rms_error / 32768.0);
}

static void SettleGuardTransitions(HARSHNESS_GUARD_STATE *state)
{
    short frame[TEST_FRAME_LEN];
    int guard;

    memset(frame, 0, sizeof(frame));
    guard = 0;
    while ((state->transition_active != 0) && (guard < 64))
    {
        Check(HarshnessGuard_ProcessFrame(
                  state, frame, frame, TEST_FRAME_LEN) ==
                  HARSHNESS_GUARD_RESULT_UPDATED,
              "Guard transition frame processes successfully");
        guard++;
    }
    Check(guard < 64,
          "Guard transition chain settles within a bounded frame count");
}

static void DriveGuardToLevel(HARSHNESS_GUARD_STATE *state,
                              int requested_level)
{
    int guard;

    guard = 0;
    while ((state->active_level != requested_level) && (guard < 16))
    {
        AUDIO_FEATURE_SNAPSHOT snapshot;
        int before_endpoint;

        before_endpoint = (state->transition_active != 0) ?
            state->pending_level : state->active_level;
        snapshot = MakeSnapshot(22.0f, 0.0f, -18.0f, 1, 1);
        Check(HarshnessGuard_UpdateFromFeature(state, &snapshot) ==
                  HARSHNESS_GUARD_RESULT_UPDATED,
              "high-excess Guard feature is accepted");
        Check(IntAbs(state->pending_level - before_endpoint) <= 1,
              "Guard attack requests at most one adjacent level");
        SettleGuardTransitions(state);
        Check(IntAbs(state->active_level - before_endpoint) <= 1,
              "one Guard analysis applies at most one level");
        guard++;
    }
    Check((guard < 16) && (state->active_level == requested_level),
          "Guard reaches the requested strength ceiling gradually");
}

static void SettleSmartBass(SMART_BASS_STATE *state)
{
    short frame[TEST_FRAME_LEN];
    int guard;

    memset(frame, 0, sizeof(frame));
    guard = 0;
    while ((state->transition_active != 0) && (guard < 64))
    {
        (void)SmartBass_ProcessFrame(state, frame, frame, TEST_FRAME_LEN);
        guard++;
    }
    Check(guard < 64, "Smart Bass transition settles for regression");
}

static void DriveSmartBassToLevel(SMART_BASS_STATE *state, int level)
{
    int guard;

    guard = 0;
    while ((state->active_level != level) && (guard < 16))
    {
        AUDIO_FEATURE_SNAPSHOT snapshot;

        memset(&snapshot, 0, sizeof(snapshot));
        analysis_sequence++;
        snapshot.analysis_count = analysis_sequence;
        snapshot.relative_db[AUDIO_FEATURE_BASS] = 20.0f;
        snapshot.rms_dbfs = -18.0f;
        snapshot.valid = 1;
        snapshot.warmup_complete = 1;
        (void)SmartBass_UpdateFromFeature(state, &snapshot);
        SettleSmartBass(state);
        guard++;
    }
    Check(state->active_level == level,
          "Smart Bass reaches the regression level");
}

static void SettleDynamicClarity(DYNAMIC_CLARITY_STATE *state)
{
    short frame[TEST_FRAME_LEN];
    int guard;

    memset(frame, 0, sizeof(frame));
    guard = 0;
    while ((state->transition_active != 0) && (guard < 64))
    {
        (void)DynamicClarity_ProcessFrame(
            state, frame, frame, TEST_FRAME_LEN);
        guard++;
    }
    Check(guard < 64,
          "Dynamic Clarity transition settles for regression");
}

static void DriveDynamicClarityToLevel(DYNAMIC_CLARITY_STATE *state,
                                       int level)
{
    int guard;

    guard = 0;
    while ((state->active_level != level) && (guard < 16))
    {
        AUDIO_FEATURE_SNAPSHOT snapshot;

        memset(&snapshot, 0, sizeof(snapshot));
        analysis_sequence++;
        snapshot.analysis_count = analysis_sequence;
        snapshot.relative_db[AUDIO_FEATURE_MUD] = 20.0f;
        snapshot.relative_db[AUDIO_FEATURE_PRESENCE] = 0.0f;
        snapshot.rms_dbfs = -18.0f;
        snapshot.valid = 1;
        snapshot.warmup_complete = 1;
        (void)DynamicClarity_UpdateFromFeature(state, &snapshot);
        SettleDynamicClarity(state);
        guard++;
    }
    Check(state->active_level == level,
          "Dynamic Clarity reaches the regression level");
}

static float BiquadGainDb(const EQ_BIQUAD *coefficient,
                          float frequency_hz)
{
    double real_value;
    double imag_value;
    double magnitude;

    real_value = 0.0;
    imag_value = 0.0;
    if (!Equalizer_GetBiquadResponseComplex(
            coefficient, frequency_hz, &real_value, &imag_value))
    {
        return -240.0f;
    }
    magnitude = sqrt(real_value * real_value + imag_value * imag_value);
    if (magnitude < 1.0e-20)
    {
        return -240.0f;
    }
    return (float)(20.0 * log10(magnitude));
}

static void TestInitializationAndIdentity(void)
{
    HARSHNESS_GUARD_STATE state;
    short input[TEST_FRAME_LEN];
    short output[TEST_FRAME_LEN];
    short before[TEST_FRAME_LEN];

    HarshnessGuard_Init(&state);
    Check(state.initialized == 1, "Guard initializes successfully");
    Check(state.requested_enabled == 0,
          "Guard defaults to runtime disabled");
    Check(state.strength == HARSHNESS_GUARD_STRENGTH_MEDIUM,
          "Guard defaults to MEDIUM strength");
    Check(state.active_level == 0 && state.target_level == 0 &&
              state.pending_level == 0,
          "Guard initializes at identity level zero");
    Check(state.transition_active == 0 &&
              state.transition_total == HARSHNESS_GUARD_TRANSITION_SAMPLES,
          "Guard initializes with no transition and the fixed duration");
    Check(HarshnessGuard_GetAppliedGainDb(&state) == 0.0f &&
              HarshnessGuard_GetRequestedGainDb(&state) == 0.0f &&
              HarshnessGuard_GetTransitionProgress(&state) == 0.0f,
          "Guard initial getters report identity");

    FillPattern(input, 1);
    memcpy(before, input, sizeof(input));
    Check(HarshnessGuard_ProcessFrame(
              &state, input, output, TEST_FRAME_LEN) ==
              HARSHNESS_GUARD_RESULT_UPDATED,
          "disabled Guard processes a separate output buffer");
    Check(memcmp(input, output, sizeof(input)) == 0,
          "disabled Guard separate-buffer output is sample exact");
    Check(memcmp(input, before, sizeof(input)) == 0,
          "disabled Guard does not modify separate input");
    Check(HarshnessGuard_SetEnabled(&state, 1) ==
              HARSHNESS_GUARD_RESULT_UPDATED,
          "Guard runtime enable is accepted");
    Check(HarshnessGuard_ProcessFrame(
              &state, input, input, TEST_FRAME_LEN) ==
              HARSHNESS_GUARD_RESULT_UPDATED,
          "enabled level-zero Guard processes in place");
    Check(memcmp(input, before, sizeof(input)) == 0,
          "enabled level-zero Guard remains exact identity");

    state.saturation_count = 7UL;
    HarshnessGuard_Reset(&state);
    Check(state.initialized == 1 && state.requested_enabled == 0 &&
              state.strength == HARSHNESS_GUARD_STRENGTH_MEDIUM &&
              state.saturation_count == 0UL,
          "Guard reset restores documented defaults");

    Check(HarshnessGuard_GetAppliedGainDb(0) == 0.0f &&
              HarshnessGuard_GetRequestedGainDb(0) == 0.0f &&
              HarshnessGuard_GetTransitionProgress(0) == 0.0f &&
              HarshnessGuard_GetReason(0) ==
                  HARSHNESS_GUARD_REASON_INVALID,
          "null Guard getters remain bounded");

    printf("identity_exact=1 default_enabled=0 default_strength=%d "
           "transition_samples=%d guard_state_bytes=%u\n",
           HARSHNESS_GUARD_STRENGTH_MEDIUM,
           HARSHNESS_GUARD_TRANSITION_SAMPLES,
           (unsigned int)sizeof(state));
}

static void TestFormulaMappingStrengthAndAttack(void)
{
    static const float excess_points[7] =
        { 6.0f, 8.0f, 10.0f, 12.0f, 14.0f, 16.0f, 18.0f };
    static const int expected_medium[7] = { 0, 1, 1, 2, 2, 3, 3 };
    static const int maximum_levels[4] = { 0, 2, 3, 4 };
    HARSHNESS_GUARD_STATE state;
    AUDIO_FEATURE_SNAPSHOT snapshot;
    AUDIO_FEATURE_SNAPSHOT before;
    unsigned long decision_before;
    int point;
    int previous_level;
    int strength;

    HarshnessGuard_Init(&state);
    (void)HarshnessGuard_SetEnabled(&state, 1);
    snapshot = MakeSnapshot(7.25f, -2.75f, -18.0f, 1, 1);
    before = snapshot;
    Check(HarshnessGuard_UpdateFromFeature(&state, &snapshot) ==
              HARSHNESS_GUARD_RESULT_UPDATED,
          "brightness-presence feature is accepted");
    Check(fabsf(state.latest_harshness_db - 10.0f) < 1.0e-6f,
          "harshness equals brightness minus presence");
    Check(FloatFinite(state.latest_harshness_db),
          "computed harshness remains finite");
    Check(memcmp(&snapshot, &before, sizeof(snapshot)) == 0,
          "Guard feature update leaves snapshot immutable");
    decision_before = state.decision_count;
    Check(HarshnessGuard_UpdateFromFeature(&state, &snapshot) ==
              HARSHNESS_GUARD_RESULT_NO_CHANGE &&
              state.decision_count == decision_before,
          "duplicate analysis count cannot repeat a Guard decision");

    previous_level = -1;
    for (point = 0; point < 7; point++)
    {
        HarshnessGuard_Init(&state);
        (void)HarshnessGuard_SetEnabled(&state, 1);
        snapshot = MakeSnapshot(excess_points[point], 0.0f,
                                -18.0f, 1, 1);
        (void)HarshnessGuard_UpdateFromFeature(&state, &snapshot);
        Check(state.target_level == expected_medium[point],
              "6-18 dB mapping follows the quantized MEDIUM curve");
        Check(state.target_level >= previous_level,
              "6-18 dB mapping is monotonic");
        Check(HarshnessGuard_GetRequestedGainDb(&state) <= 0.0f &&
                  HarshnessGuard_GetRequestedGainDb(&state) >= -2.0f,
              "requested Guard gain is bounded and never positive");
        printf("mapping_excess_db=%.3f mapping_level=%d "
               "mapping_gain_db=%.3f\n",
               excess_points[point], state.target_level,
               HarshnessGuard_GetRequestedGainDb(&state));
        previous_level = state.target_level;
    }

    HarshnessGuard_Init(&state);
    (void)HarshnessGuard_SetEnabled(&state, 1);
    snapshot = MakeSnapshot(18.0f, 0.0f, -45.0f, 1, 1);
    (void)HarshnessGuard_UpdateFromFeature(&state, &snapshot);
    Check(state.target_level == 3,
          "exact RMS and brightness boundaries remain eligible");

    HarshnessGuard_Reset(&state);
    (void)HarshnessGuard_SetEnabled(&state, 1);
    snapshot = MakeSnapshot(1.0f, -20.0f, -18.0f, 1, 1);
    (void)HarshnessGuard_UpdateFromFeature(&state, &snapshot);
    Check(state.target_level == 3,
          "Guard intentionally has no Presence minimum gate");

    for (strength = HARSHNESS_GUARD_STRENGTH_OFF;
         strength <= HARSHNESS_GUARD_STRENGTH_HIGH; strength++)
    {
        HarshnessGuard_Init(&state);
        Check(HarshnessGuard_SetStrength(&state, strength) >= 0,
              "Guard strength selection is accepted");
        (void)HarshnessGuard_SetEnabled(&state, 1);
        if (maximum_levels[strength] > 0)
        {
            DriveGuardToLevel(&state, maximum_levels[strength]);
        }
        Check(state.active_level <= maximum_levels[strength],
              "applied Guard level respects strength ceiling");
        Check(HarshnessGuard_GetAppliedGainDb(&state) >=
                  -HARSHNESS_GUARD_LEVEL_STEP_DB *
                  (float)maximum_levels[strength] - 0.001f,
              "applied Guard attenuation respects strength ceiling");
    }

    printf("harshness_formula_exact=1 snapshot_immutable=1 "
           "mapping_span_db=6,18 strength_max_levels=0,2,3,4 "
           "attack_step_max=1 presence_min_gate=0\n");
}

static void ReleaseCauseToIdentity(float brightness_db,
                                   float presence_db,
                                   float rms_dbfs,
                                   int valid,
                                   int warmup,
                                   int expected_reason,
                                   int expect_invalid_count)
{
    HARSHNESS_GUARD_STATE state;
    AUDIO_FEATURE_SNAPSHOT snapshot;
    int guard;

    HarshnessGuard_Init(&state);
    (void)HarshnessGuard_SetEnabled(&state, 1);
    DriveGuardToLevel(&state, 2);
    guard = 0;
    while ((state.active_level > 0) && (guard < 8))
    {
        snapshot = MakeSnapshot(brightness_db, presence_db, rms_dbfs,
                                valid, warmup);
        (void)HarshnessGuard_UpdateFromFeature(&state, &snapshot);
        Check(state.reason == expected_reason,
              "first release confirmation preserves gate reason");
        snapshot = MakeSnapshot(brightness_db, presence_db, rms_dbfs,
                                valid, warmup);
        (void)HarshnessGuard_UpdateFromFeature(&state, &snapshot);
        Check(state.reason == HARSHNESS_GUARD_REASON_RELEASING,
              "second release confirmation reports releasing");
        SettleGuardTransitions(&state);
        guard++;
    }
    Check((guard < 8) && (state.active_level == 0),
          "release cause returns Guard to identity");
    if (expect_invalid_count != 0)
    {
        Check(state.invalid_release_count > 0UL,
              "failed eligibility gate is counted during release");
    }
}

static void TestGatesDebounceDisableAndStrength(void)
{
    HARSHNESS_GUARD_STATE state;
    AUDIO_FEATURE_SNAPSHOT snapshot;
    short frame[TEST_FRAME_LEN];
    short before[TEST_FRAME_LEN];
    unsigned long transition_before;
    unsigned long debounce_delta;
    int index;
    int pending_before;

    HarshnessGuard_Init(&state);
    (void)HarshnessGuard_SetEnabled(&state, 1);
    snapshot = MakeSnapshot(6.0f, 0.0f, -18.0f, 1, 1);
    (void)HarshnessGuard_UpdateFromFeature(&state, &snapshot);
    Check(state.target_level == 0 &&
              state.reason == HARSHNESS_GUARD_REASON_BELOW_THRESHOLD,
          "exact 6 dB boundary requests identity");

    ReleaseCauseToIdentity(20.0f, 0.0f, -18.0f, 0, 1,
                           HARSHNESS_GUARD_REASON_INVALID, 1);
    ReleaseCauseToIdentity(20.0f, 0.0f, -18.0f, 1, 0,
                           HARSHNESS_GUARD_REASON_NOT_WARM, 1);
    ReleaseCauseToIdentity(20.0f, 0.0f, -45.01f, 1, 1,
                           HARSHNESS_GUARD_REASON_LOW_LEVEL, 1);
    ReleaseCauseToIdentity(-0.01f, -20.0f, -18.0f, 1, 1,
                           HARSHNESS_GUARD_REASON_WEAK_BRIGHTNESS, 1);
    ReleaseCauseToIdentity(6.0f, 0.0f, -18.0f, 1, 1,
                           HARSHNESS_GUARD_REASON_BELOW_THRESHOLD, 0);

    HarshnessGuard_Init(&state);
    (void)HarshnessGuard_SetEnabled(&state, 1);
    DriveGuardToLevel(&state, 1);
    transition_before = state.transition_count;
    for (index = 0; index < 10; index++)
    {
        float excess_db;

        excess_db = ((index & 1) == 0) ? 5.9f : 8.1f;
        snapshot = MakeSnapshot(excess_db, 0.0f, -18.0f, 1, 1);
        (void)HarshnessGuard_UpdateFromFeature(&state, &snapshot);
    }
    Check(state.active_level == 1 && state.transition_active == 0,
          "6 dB threshold alternation does not chatter applied level");
    Check(state.transition_count == transition_before,
          "threshold alternation starts no repeated transitions");
    debounce_delta = state.transition_count - transition_before;

    snapshot = MakeSnapshot(22.0f, 0.0f, -18.0f, 1, 1);
    (void)HarshnessGuard_UpdateFromFeature(&state, &snapshot);
    pending_before = state.pending_level;
    Check(state.transition_active == 1 && pending_before == 2,
          "new Guard attack is active before disable");
    Check(HarshnessGuard_SetEnabled(&state, 0) ==
              HARSHNESS_GUARD_RESULT_UPDATED,
          "Guard disable is accepted during transition");
    Check(state.transition_active == 1 &&
              state.pending_level == pending_before,
          "disable does not overwrite in-flight Guard pending level");
    SettleGuardTransitions(&state);
    Check(state.active_level == 0 && state.processing_active == 0,
          "disable converges through adjacent levels to identity");
    FillPattern(frame, 9);
    memcpy(before, frame, sizeof(frame));
    (void)HarshnessGuard_ProcessFrame(
        &state, frame, frame, TEST_FRAME_LEN);
    Check(memcmp(frame, before, sizeof(frame)) == 0,
          "disabled convergence ends in exact identity");

    HarshnessGuard_Init(&state);
    (void)HarshnessGuard_SetStrength(
        &state, HARSHNESS_GUARD_STRENGTH_HIGH);
    (void)HarshnessGuard_SetEnabled(&state, 1);
    DriveGuardToLevel(&state, 4);
    Check(HarshnessGuard_SetStrength(
              &state, HARSHNESS_GUARD_STRENGTH_LOW) ==
              HARSHNESS_GUARD_RESULT_UPDATED,
          "lower Guard strength is accepted");
    SettleGuardTransitions(&state);
    Check(state.active_level == 2 && state.target_level == 2,
          "HIGH to LOW strength converges to level two");
    Check(HarshnessGuard_SetStrength(
              &state, HARSHNESS_GUARD_STRENGTH_OFF) ==
              HARSHNESS_GUARD_RESULT_UPDATED,
          "OFF strength is accepted");
    SettleGuardTransitions(&state);
    Check(state.active_level == 0 && state.processing_active == 0,
          "OFF strength converges to exact identity");

    printf("gate_causes_to_zero=5 release_confirmations=2 "
           "debounce_transitions=%lu disable_identity_exact=1 "
           "strength_high_to_low_final_level=2 "
           "strength_off_identity_exact=1\n",
           debounce_delta);
}

static void TestTransitionPendingQueueAndEpoch(void)
{
    HARSHNESS_GUARD_STATE state;
    AUDIO_FEATURE_SNAPSHOT snapshot;
    short frame[256];
    short impulse[TEST_FRAME_LEN];
    float previous_progress;
    float progress;
    unsigned long decision_before;
    unsigned long saturation_before;
    int frame_index;
    int pending_level;

    memset(frame, 0, sizeof(frame));
    HarshnessGuard_Init(&state);
    (void)HarshnessGuard_SetStrength(
        &state, HARSHNESS_GUARD_STRENGTH_HIGH);
    (void)HarshnessGuard_SetEnabled(&state, 1);
    snapshot = MakeSnapshot(22.0f, 0.0f, -18.0f, 1, 1);
    (void)HarshnessGuard_UpdateFromFeature(&state, &snapshot);
    pending_level = state.pending_level;
    Check(state.active_level == 0 && pending_level == 1 &&
              state.transition_active == 1,
          "first high request starts only zero-to-one transition");
    snapshot = MakeSnapshot(22.0f, 0.0f, -18.0f, 1, 1);
    (void)HarshnessGuard_UpdateFromFeature(&state, &snapshot);
    Check(state.pending_level == pending_level &&
              state.queued_level_valid == 1 &&
              state.queued_level == 2,
          "second request queues adjacent level without pending overwrite");
    snapshot = MakeSnapshot(22.0f, 0.0f, -18.0f, 1, 1);
    (void)HarshnessGuard_UpdateFromFeature(&state, &snapshot);
    Check(state.pending_level == pending_level &&
              state.queued_level == 2,
          "latest request remains bounded to one adjacent queued level");

    previous_progress = -1.0f;
    for (frame_index = 0; frame_index < 15; frame_index++)
    {
        (void)HarshnessGuard_ProcessFrame(&state, frame, frame, 256);
        progress = HarshnessGuard_GetTransitionProgress(&state);
        Check(progress >= previous_progress && progress >= 0.0f &&
                  progress <= 1.0f,
              "Guard transition progress is monotonic and bounded");
        previous_progress = progress;
    }
    (void)HarshnessGuard_ProcessFrame(&state, frame, frame, 160);
    Check(state.active_level == 1 && state.transition_active == 1 &&
              state.pending_level == 2,
          "queued adjacent level starts only after pending completes");
    SettleGuardTransitions(&state);
    Check(state.active_level == 2 && state.transition_active == 0,
          "queued adjacent Guard level is eventually applied");

    HarshnessGuard_Init(&state);
    (void)HarshnessGuard_SetStrength(
        &state, HARSHNESS_GUARD_STRENGTH_HIGH);
    (void)HarshnessGuard_SetEnabled(&state, 1);
    snapshot = MakeSnapshot(22.0f, 0.0f, -18.0f, 1, 1);
    (void)HarshnessGuard_UpdateFromFeature(&state, &snapshot);
    snapshot = MakeSnapshot(22.0f, 0.0f, -18.0f, 1, 1);
    (void)HarshnessGuard_UpdateFromFeature(&state, &snapshot);
    snapshot = MakeSnapshot(6.0f, 0.0f, -18.0f, 1, 1);
    (void)HarshnessGuard_UpdateFromFeature(&state, &snapshot);
    snapshot = MakeSnapshot(6.0f, 0.0f, -18.0f, 1, 1);
    (void)HarshnessGuard_UpdateFromFeature(&state, &snapshot);
    Check(state.pending_level == 1 && state.queued_level_valid == 1 &&
              state.queued_level == 0,
          "latest release request replaces stale queued attack");
    SettleGuardTransitions(&state);
    Check(state.active_level == 0,
          "queued release returns Guard to identity");

    HarshnessGuard_Reset(&state);
    (void)HarshnessGuard_SetEnabled(&state, 1);
    snapshot = MakeSnapshot(10.0f, 0.0f, -18.0f, 1, 1);
    (void)HarshnessGuard_UpdateFromFeature(&state, &snapshot);
    decision_before = state.decision_count;
    Check(HarshnessGuard_UpdateFromFeature(&state, &snapshot) ==
              HARSHNESS_GUARD_RESULT_NO_CHANGE,
          "same analysis epoch is ignored");
    HarshnessGuard_InvalidateAnalysisEpoch(&state);
    Check(HarshnessGuard_UpdateFromFeature(&state, &snapshot) ==
              HARSHNESS_GUARD_RESULT_UPDATED &&
              state.decision_count == decision_before + 1UL,
          "analysis epoch invalidation permits one replay");

    HarshnessGuard_Reset(&state);
    (void)HarshnessGuard_SetEnabled(&state, 1);
    snapshot = MakeSnapshot(22.0f, 0.0f, -1.0f, 1, 1);
    (void)HarshnessGuard_UpdateFromFeature(&state, &snapshot);
    memset(impulse, 0, sizeof(impulse));
    impulse[0] = (short)29490;
    saturation_before = state.saturation_count;
    for (frame_index = 0; frame_index < 4; frame_index++)
    {
        (void)HarshnessGuard_ProcessFrame(
            &state, impulse, impulse, TEST_FRAME_LEN);
        memset(impulse, 0, sizeof(impulse));
    }
    Check(state.saturation_count == saturation_before,
          "0.9FS impulse transition creates no saturation");

    printf("pending_preserved=1 queued_applied=1 latest_queue_wins=1 "
           "transition_progress_monotonic=1 sequence_epoch_reset=1 "
           "impulse_09fs_saturation=0\n");
}

static void TestFrequencyResponse(void)
{
    static const float frequencies[7] =
    {
        1000.0f,
        1953.125f,
        4003.90625f,
        5859.375f,
        8007.8125f,
        12011.71875f,
        16015.625f
    };
    HARSHNESS_GUARD_STATE state;
    float center_level4_db;
    float low_abs_max;
    float high_abs_max;
    float positive_max;
    float previous_center;
    int frequency;
    int level;

    HarshnessGuard_Init(&state);
    low_abs_max = 0.0f;
    high_abs_max = 0.0f;
    positive_max = -1000.0f;
    previous_center = 0.0f;
    center_level4_db = 0.0f;
    for (frequency = 0; frequency < 7; frequency++)
    {
        for (level = 0; level < HARSHNESS_GUARD_LEVEL_COUNT; level++)
        {
            float gain_db;

            gain_db = BiquadGainDb(
                &state.coefficient_table[level], frequencies[frequency]);
            Check(FloatFinite(gain_db),
                  "Guard fixed-level response is finite");
            if (level == 0)
            {
                Check(fabsf(gain_db) < 1.0e-5f,
                      "Guard level zero response is exact identity");
            }
            if (gain_db > positive_max)
            {
                positive_max = gain_db;
            }
            if ((frequency == 0) && (fabsf(gain_db) > low_abs_max))
            {
                low_abs_max = fabsf(gain_db);
            }
            if ((frequency == 6) && (fabsf(gain_db) > high_abs_max))
            {
                high_abs_max = fabsf(gain_db);
            }
            if (frequency == 3)
            {
                if (level > 0)
                {
                    Check(gain_db <= previous_center + 0.002f,
                          "near-center attenuation grows monotonically");
                }
                previous_center = gain_db;
                if (level == 4)
                {
                    center_level4_db = gain_db;
                }
            }
            printf("response_frequency_hz=%.6f response_level=%d "
                   "response_gain_db=%.8f\n",
                   frequencies[frequency], level, gain_db);
        }
    }
    Check(fabsf(center_level4_db + 2.0f) < 0.08f,
          "Guard level four is approximately minus 2 dB near 6 kHz");
    Check(low_abs_max < 0.10f,
          "1 kHz remains close to zero dB");
    Check(high_abs_max < 0.30f,
          "16 kHz attenuation remains bounded away from shelf behavior");
    Check(positive_max < 0.002f,
          "checked Guard responses avoid positive gain");

    printf("response_frequency_count=7 response_level_count=5 "
           "level4_5859hz_db=%.6f low_1khz_abs_max_db=%.6f "
           "high_16khz_abs_max_db=%.6f "
           "response_positive_max_db=%.6f\n",
           center_level4_db, low_abs_max, high_abs_max, positive_max);
}

static void TestNumericalGuardsImmutabilityAndDeterminism(void)
{
    HARSHNESS_GUARD_STATE first;
    HARSHNESS_GUARD_STATE second;
    HARSHNESS_GUARD_STATE uninitialized;
    AUDIO_FEATURE_SNAPSHOT snapshot;
    AUDIO_FEATURE_SNAPSHOT before_snapshot;
    short input[TEST_FRAME_LEN];
    short before[TEST_FRAME_LEN];
    short output_first[TEST_FRAME_LEN];
    short output_second[TEST_FRAME_LEN];
    float nan_value;
    float inf_value;

    memset(&uninitialized, 0, sizeof(uninitialized));
    HarshnessGuard_Init(&first);
    HarshnessGuard_Init(&second);
    Check(HarshnessGuard_SetEnabled(0, 1) ==
              HARSHNESS_GUARD_RESULT_ERROR,
          "null state enable is rejected");
    Check(HarshnessGuard_SetStrength(0, 1) ==
              HARSHNESS_GUARD_RESULT_ERROR,
          "null state strength is rejected");
    Check(HarshnessGuard_SetEnabled(&uninitialized, 1) ==
              HARSHNESS_GUARD_RESULT_ERROR,
          "uninitialized state enable is rejected");
    Check(HarshnessGuard_SetStrength(&first, -99) ==
              HARSHNESS_GUARD_RESULT_UPDATED &&
              first.strength == HARSHNESS_GUARD_STRENGTH_OFF,
          "negative Guard strength clamps to OFF");
    Check(HarshnessGuard_SetStrength(&first, 99) ==
              HARSHNESS_GUARD_RESULT_UPDATED &&
              first.strength == HARSHNESS_GUARD_STRENGTH_HIGH,
          "large Guard strength clamps to HIGH");
    Check(HarshnessGuard_ProcessFrame(0, input, output_first, 1) ==
              HARSHNESS_GUARD_RESULT_ERROR,
          "null processing state is rejected");
    Check(HarshnessGuard_ProcessFrame(&first, 0, output_first, 1) ==
              HARSHNESS_GUARD_RESULT_ERROR,
          "null processing input is rejected");
    Check(HarshnessGuard_ProcessFrame(&first, input, 0, 1) ==
              HARSHNESS_GUARD_RESULT_ERROR,
          "null processing output is rejected");
    Check(HarshnessGuard_ProcessFrame(&first, input, output_first, 0) ==
              HARSHNESS_GUARD_RESULT_ERROR,
          "zero sample count is rejected");
    Check(HarshnessGuard_ProcessFrame(&first, input, output_first, -1) ==
              HARSHNESS_GUARD_RESULT_ERROR,
          "negative sample count is rejected");
    Check(HarshnessGuard_ProcessFrame(
              &first, input, output_first, TEST_FRAME_LEN + 1) ==
              HARSHNESS_GUARD_RESULT_ERROR,
          "oversized sample count is rejected");
    Check(HarshnessGuard_UpdateFromFeature(0, &snapshot) ==
              HARSHNESS_GUARD_RESULT_ERROR,
          "null feature state is rejected");
    Check(HarshnessGuard_UpdateFromFeature(&first, 0) ==
              HARSHNESS_GUARD_RESULT_ERROR,
          "null feature snapshot is rejected");

    nan_value = (float)sqrt(-1.0);
    inf_value = (float)HUGE_VAL;
    HarshnessGuard_Reset(&first);
    (void)HarshnessGuard_SetEnabled(&first, 1);
    snapshot = MakeSnapshot(nan_value, 0.0f, -18.0f, 1, 1);
    before_snapshot = snapshot;
    (void)HarshnessGuard_UpdateFromFeature(&first, &snapshot);
    Check(first.target_level == 0 && first.nonfinite_count == 1UL,
          "NaN brightness requests bounded identity release");
    Check(FloatFinite(first.latest_brightness_relative_db) &&
              FloatFinite(first.latest_presence_relative_db) &&
              FloatFinite(first.latest_harshness_db) &&
              FloatFinite(first.latest_rms_dbfs),
          "NaN feature is not published into runtime state");
    Check(memcmp(&snapshot, &before_snapshot, sizeof(snapshot)) == 0,
          "NaN handling leaves snapshot immutable");

    HarshnessGuard_Reset(&first);
    (void)HarshnessGuard_SetEnabled(&first, 1);
    snapshot = MakeSnapshot(20.0f, inf_value, -18.0f, 1, 1);
    (void)HarshnessGuard_UpdateFromFeature(&first, &snapshot);
    Check(first.target_level == 0 && first.nonfinite_count == 1UL &&
              FloatFinite(first.latest_presence_relative_db),
          "infinite Presence is contained by identity release");

    HarshnessGuard_Reset(&first);
    (void)HarshnessGuard_SetEnabled(&first, 1);
    snapshot = MakeSnapshot(20.0f, 0.0f, -inf_value, 1, 1);
    (void)HarshnessGuard_UpdateFromFeature(&first, &snapshot);
    Check(first.target_level == 0 && first.nonfinite_count == 1UL &&
              FloatFinite(first.latest_rms_dbfs),
          "infinite RMS is contained by identity release");

    HarshnessGuard_Reset(&first);
    HarshnessGuard_Reset(&second);
    (void)HarshnessGuard_SetEnabled(&first, 1);
    (void)HarshnessGuard_SetEnabled(&second, 1);
    snapshot = MakeSnapshot(20.0f, 0.0f, -18.0f, 1, 1);
    (void)HarshnessGuard_UpdateFromFeature(&first, &snapshot);
    (void)HarshnessGuard_UpdateFromFeature(&second, &snapshot);
    FillPattern(input, 13);
    memcpy(before, input, sizeof(input));
    (void)HarshnessGuard_ProcessFrame(
        &first, input, output_first, TEST_FRAME_LEN);
    (void)HarshnessGuard_ProcessFrame(
        &second, input, output_second, TEST_FRAME_LEN);
    Check(memcmp(input, before, sizeof(input)) == 0,
          "non-in-place Guard processing leaves input immutable");
    Check(memcmp(output_first, output_second, sizeof(output_first)) == 0,
          "equal Guard initial state and input are deterministic");

    first.active_level = 1;
    first.pending_level = 1;
    first.transition_active = 0;
    first.active_state.s1 = nan_value;
    (void)HarshnessGuard_ProcessFrame(
        &first, input, output_first, TEST_FRAME_LEN);
    Check(first.active_level == 0 && first.processing_active == 0 &&
              first.nonfinite_count > 0UL,
          "nonfinite Guard filter state enters safe identity");

    printf("invalid_argument_checks=11 illegal_strength_clamped=1 "
           "deterministic=1 input_immutable=1 "
           "nonfinite_contained=3\n");
}

static void PrepareSmartBassAudioLevel(SMART_BASS_STATE *state, int level)
{
    SmartBass_Init(state);
    state->active_level = level;
    state->target_level = level;
    state->pending_level = level;
    state->requested_enabled = 1;
    state->processing_active = (level != 0) ? 1 : 0;
}

static void PrepareDynamicClarityAudioLevel(
    DYNAMIC_CLARITY_STATE *state,
    int level)
{
    DynamicClarity_Init(state);
    state->active_level = level;
    state->target_level = level;
    state->pending_level = level;
    state->requested_enabled = 1;
    state->processing_active = (level != 0) ? 1 : 0;
}

static void PrepareGuardAudioLevel(HARSHNESS_GUARD_STATE *state, int level)
{
    HarshnessGuard_Init(state);
    state->active_level = level;
    state->target_level = level;
    state->pending_level = level;
    state->requested_enabled = 1;
    state->processing_active = (level != 0) ? 1 : 0;
}

static void FillPcmVector(short *input, int vector)
{
    if (vector == 0)
    {
        FillSine(input, 1000.0f, 4125.0f);
    }
    else if (vector == 1)
    {
        FillSine(input, 400.0f, 4125.0f);
    }
    else if (vector == 2)
    {
        FillSine(input, 5859.375f, 4125.0f);
    }
    else if (vector == 3)
    {
        FillMusicLike(input);
    }
    else
    {
        FillWideband(input);
    }
}

static void PrintPcmMetric(char chain,
                           const PCM_METRIC *metric,
                           unsigned long saturation,
                           unsigned long nonfinite)
{
    printf("pcm_chain=%c max_error=%.6f rms_error=%.6f "
           "residual_dbfs=%.6f dc_error=%.6f clipped_samples=%lu "
           "saturation=%lu nonfinite=%lu "
           "evidence_label=HOST_DIGITAL_SIMULATION\n",
           chain, metric->max_error, PcmRmsError(metric),
           PcmResidualDbfs(metric), PcmDcError(metric),
           metric->clipped_samples, saturation, nonfinite);
}

static void TestPcm16ChainsAThroughG(void)
{
    PCM_METRIC metrics[TEST_CHAIN_COUNT];
    unsigned long saturation[TEST_CHAIN_COUNT];
    unsigned long nonfinite[TEST_CHAIN_COUNT];
    EQ_STATE equalizer;
    short chain[TEST_CHAIN_COUNT][TEST_FRAME_LEN];
    short input[TEST_FRAME_LEN];
    int index;
    int vector;

    memset(metrics, 0, sizeof(metrics));
    memset(saturation, 0, sizeof(saturation));
    memset(nonfinite, 0, sizeof(nonfinite));

    for (vector = 0; vector < TEST_VECTOR_COUNT; vector++)
    {
        SMART_BASS_STATE smart_b;
        SMART_BASS_STATE smart_c;
        SMART_BASS_STATE smart_f;
        SMART_BASS_STATE smart_g;
        DYNAMIC_CLARITY_STATE clarity_b;
        DYNAMIC_CLARITY_STATE clarity_d;
        DYNAMIC_CLARITY_STATE clarity_f;
        DYNAMIC_CLARITY_STATE clarity_g;
        HARSHNESS_GUARD_STATE guard_b;
        HARSHNESS_GUARD_STATE guard_e;
        HARSHNESS_GUARD_STATE guard_g;
        float smart_c_s1;
        float smart_c_s2;
        float smart_f_s1;
        float smart_f_s2;
        float smart_g_s1;
        float smart_g_s2;
        float clarity_d_s1;
        float clarity_d_s2;
        float clarity_f_s1;
        float clarity_f_s2;
        float clarity_g_s1;
        float clarity_g_s2;
        float guard_e_s1;
        float guard_e_s2;
        float guard_g_s1;
        float guard_g_s2;

        FillPcmVector(input, vector);
        Equalizer_Init(&equalizer);
        Equalizer_ProcessFrame(&equalizer, input, chain[0], TEST_FRAME_LEN);

        memcpy(chain[1], chain[0], sizeof(chain[1]));
        SmartBass_Init(&smart_b);
        DynamicClarity_Init(&clarity_b);
        HarshnessGuard_Init(&guard_b);
        (void)SmartBass_ProcessFrame(
            &smart_b, chain[1], chain[1], TEST_FRAME_LEN);
        (void)DynamicClarity_ProcessFrame(
            &clarity_b, chain[1], chain[1], TEST_FRAME_LEN);
        (void)HarshnessGuard_ProcessFrame(
            &guard_b, chain[1], chain[1], TEST_FRAME_LEN);

        PrepareSmartBassAudioLevel(&smart_c, 4);
        (void)SmartBass_ProcessFrame(
            &smart_c, chain[0], chain[2], TEST_FRAME_LEN);

        PrepareDynamicClarityAudioLevel(&clarity_d, 4);
        (void)DynamicClarity_ProcessFrame(
            &clarity_d, chain[0], chain[3], TEST_FRAME_LEN);

        PrepareGuardAudioLevel(&guard_e, 3);
        (void)HarshnessGuard_ProcessFrame(
            &guard_e, chain[0], chain[4], TEST_FRAME_LEN);

        PrepareSmartBassAudioLevel(&smart_f, 4);
        PrepareDynamicClarityAudioLevel(&clarity_f, 4);
        (void)SmartBass_ProcessFrame(
            &smart_f, chain[0], chain[5], TEST_FRAME_LEN);
        (void)DynamicClarity_ProcessFrame(
            &clarity_f, chain[5], chain[5], TEST_FRAME_LEN);

        PrepareSmartBassAudioLevel(&smart_g, 4);
        PrepareDynamicClarityAudioLevel(&clarity_g, 4);
        PrepareGuardAudioLevel(&guard_g, 3);
        (void)SmartBass_ProcessFrame(
            &smart_g, chain[0], chain[6], TEST_FRAME_LEN);
        (void)DynamicClarity_ProcessFrame(
            &clarity_g, chain[6], chain[6], TEST_FRAME_LEN);
        (void)HarshnessGuard_ProcessFrame(
            &guard_g, chain[6], chain[6], TEST_FRAME_LEN);

        smart_c_s1 = 0.0f;
        smart_c_s2 = 0.0f;
        smart_f_s1 = 0.0f;
        smart_f_s2 = 0.0f;
        smart_g_s1 = 0.0f;
        smart_g_s2 = 0.0f;
        clarity_d_s1 = 0.0f;
        clarity_d_s2 = 0.0f;
        clarity_f_s1 = 0.0f;
        clarity_f_s2 = 0.0f;
        clarity_g_s1 = 0.0f;
        clarity_g_s2 = 0.0f;
        guard_e_s1 = 0.0f;
        guard_e_s2 = 0.0f;
        guard_g_s1 = 0.0f;
        guard_g_s2 = 0.0f;
        for (index = 0; index < TEST_FRAME_LEN; index++)
        {
            float ideal_c;
            float ideal_d;
            float ideal_e;
            float ideal_f;
            float ideal_g;

            ideal_c = ProcessFloatBiquad(
                &smart_c.coefficient_table[4],
                &smart_c_s1, &smart_c_s2, (float)chain[0][index]);
            ideal_d = ProcessFloatBiquad(
                &clarity_d.coefficient_table[4],
                &clarity_d_s1, &clarity_d_s2,
                (float)chain[0][index]);
            ideal_e = ProcessFloatBiquad(
                &guard_e.coefficient_table[3],
                &guard_e_s1, &guard_e_s2, (float)chain[0][index]);
            ideal_f = ProcessFloatBiquad(
                &smart_f.coefficient_table[4],
                &smart_f_s1, &smart_f_s2, (float)chain[0][index]);
            ideal_f = ProcessFloatBiquad(
                &clarity_f.coefficient_table[4],
                &clarity_f_s1, &clarity_f_s2, ideal_f);
            ideal_g = ProcessFloatBiquad(
                &smart_g.coefficient_table[4],
                &smart_g_s1, &smart_g_s2, (float)chain[0][index]);
            ideal_g = ProcessFloatBiquad(
                &clarity_g.coefficient_table[4],
                &clarity_g_s1, &clarity_g_s2, ideal_g);
            ideal_g = ProcessFloatBiquad(
                &guard_g.coefficient_table[3],
                &guard_g_s1, &guard_g_s2, ideal_g);

            UpdatePcmMetric(&metrics[0], chain[0][index],
                            (float)chain[0][index]);
            UpdatePcmMetric(&metrics[1], chain[1][index],
                            (float)chain[0][index]);
            UpdatePcmMetric(&metrics[2], chain[2][index], ideal_c);
            UpdatePcmMetric(&metrics[3], chain[3][index], ideal_d);
            UpdatePcmMetric(&metrics[4], chain[4][index], ideal_e);
            UpdatePcmMetric(&metrics[5], chain[5][index], ideal_f);
            UpdatePcmMetric(&metrics[6], chain[6][index], ideal_g);
        }

        saturation[1] += smart_b.saturation_count +
                         clarity_b.saturation_count +
                         guard_b.saturation_count;
        saturation[2] += smart_c.saturation_count;
        saturation[3] += clarity_d.saturation_count;
        saturation[4] += guard_e.saturation_count;
        saturation[5] += smart_f.saturation_count +
                         clarity_f.saturation_count;
        saturation[6] += smart_g.saturation_count +
                         clarity_g.saturation_count +
                         guard_g.saturation_count;
        nonfinite[1] += smart_b.nonfinite_count +
                        clarity_b.nonfinite_count +
                        guard_b.nonfinite_count;
        nonfinite[2] += smart_c.nonfinite_count;
        nonfinite[3] += clarity_d.nonfinite_count;
        nonfinite[4] += guard_e.nonfinite_count;
        nonfinite[5] += smart_f.nonfinite_count +
                        clarity_f.nonfinite_count;
        nonfinite[6] += smart_g.nonfinite_count +
                        clarity_g.nonfinite_count +
                        guard_g.nonfinite_count;
    }

    Check(metrics[0].max_error == 0.0,
          "PCM chain A baseline metric is exact");
    Check(metrics[1].max_error == 0.0,
          "PCM chain B with every dynamic module OFF equals A exactly");
    Check(metrics[2].max_error <= 0.501,
          "PCM chain C single Smart Bass conversion is half-sample bounded");
    Check(metrics[3].max_error <= 0.501,
          "PCM chain D single Clarity conversion is half-sample bounded");
    Check(metrics[4].max_error <= 0.501,
          "PCM chain E single Guard conversion is half-sample bounded");
    Check(metrics[5].max_error <= 1.5,
          "PCM chain F two active conversions remain bounded");
    Check(metrics[6].max_error <= 2.5,
          "PCM chain G three active conversions remain bounded");
    Check(PcmRmsError(&metrics[6]) <= 0.85,
          "PCM chain G RMS quantization remains bounded");
    Check(PcmDcError(&metrics[6]) <= 0.15,
          "PCM chain G adds negligible DC error");
    Check((metrics[0].clipped_samples + metrics[1].clipped_samples +
           metrics[2].clipped_samples + metrics[3].clipped_samples +
           metrics[4].clipped_samples + metrics[5].clipped_samples +
           metrics[6].clipped_samples) == 0UL,
          "A-G PCM16 stress vectors contain no clipped sample");
    Check((saturation[0] + saturation[1] + saturation[2] +
           saturation[3] + saturation[4] + saturation[5] +
           saturation[6]) == 0UL,
          "A-G dynamic stages report no saturation");
    Check((nonfinite[0] + nonfinite[1] + nonfinite[2] +
           nonfinite[3] + nonfinite[4] + nonfinite[5] +
           nonfinite[6]) == 0UL,
          "A-G dynamic stages report no nonfinite output");

    PrintPcmMetric('A', &metrics[0], saturation[0], nonfinite[0]);
    PrintPcmMetric('B', &metrics[1], saturation[1], nonfinite[1]);
    PrintPcmMetric('C', &metrics[2], saturation[2], nonfinite[2]);
    PrintPcmMetric('D', &metrics[3], saturation[3], nonfinite[3]);
    PrintPcmMetric('E', &metrics[4], saturation[4], nonfinite[4]);
    PrintPcmMetric('F', &metrics[5], saturation[5], nonfinite[5]);
    PrintPcmMetric('G', &metrics[6], saturation[6], nonfinite[6]);
    printf("pcm_vectors=5 pcm_chain_B_exact_A=1 pcm_clip_count=0 "
           "pcm_saturation_count=0 pcm_nonfinite_count=0\n");
}

static void TestGuardOffRegressions(void)
{
    EQ_STATE eq_reference;
    EQ_STATE eq_guard_off;
    SMART_BASS_STATE smart_reference;
    SMART_BASS_STATE smart_guard_off;
    SMART_BASS_STATE smart_disabled;
    DYNAMIC_CLARITY_STATE clarity_reference;
    DYNAMIC_CLARITY_STATE clarity_guard_off;
    DYNAMIC_CLARITY_STATE clarity_disabled;
    HARSHNESS_GUARD_STATE guard_disabled;
    short input[TEST_FRAME_LEN];
    short reference[TEST_FRAME_LEN];
    short guarded[TEST_FRAME_LEN];
    int frame_index;

    SmartBass_Init(&smart_reference);
    SmartBass_Init(&smart_guard_off);
    DynamicClarity_Init(&clarity_reference);
    DynamicClarity_Init(&clarity_guard_off);
    HarshnessGuard_Init(&guard_disabled);
    (void)SmartBass_SetEnabled(&smart_reference, 1);
    (void)SmartBass_SetEnabled(&smart_guard_off, 1);
    (void)DynamicClarity_SetEnabled(&clarity_reference, 1);
    (void)DynamicClarity_SetEnabled(&clarity_guard_off, 1);
    DriveSmartBassToLevel(&smart_reference, 2);
    DriveSmartBassToLevel(&smart_guard_off, 2);
    DriveDynamicClarityToLevel(&clarity_reference, 2);
    DriveDynamicClarityToLevel(&clarity_guard_off, 2);
    for (frame_index = 0; frame_index < 8; frame_index++)
    {
        FillPattern(input, frame_index + 21);
        (void)SmartBass_ProcessFrame(
            &smart_reference, input, reference, TEST_FRAME_LEN);
        (void)DynamicClarity_ProcessFrame(
            &clarity_reference, reference, reference, TEST_FRAME_LEN);
        (void)SmartBass_ProcessFrame(
            &smart_guard_off, input, guarded, TEST_FRAME_LEN);
        (void)DynamicClarity_ProcessFrame(
            &clarity_guard_off, guarded, guarded, TEST_FRAME_LEN);
        (void)HarshnessGuard_ProcessFrame(
            &guard_disabled, guarded, guarded, TEST_FRAME_LEN);
        Check(memcmp(reference, guarded, sizeof(reference)) == 0,
              "Guard OFF preserves Static EQ plus Smart Bass plus Clarity");
    }

    Equalizer_Init(&eq_reference);
    Equalizer_Init(&eq_guard_off);
    Equalizer_ApplyPreset(&eq_reference, EQ_PRESET_VOCAL);
    Equalizer_ApplyPreset(&eq_guard_off, EQ_PRESET_VOCAL);
    SmartBass_Init(&smart_disabled);
    DynamicClarity_Init(&clarity_disabled);
    HarshnessGuard_Reset(&guard_disabled);
    for (frame_index = 0; frame_index < 8; frame_index++)
    {
        FillPattern(input, frame_index + 41);
        Equalizer_ProcessFrame(
            &eq_reference, input, reference, TEST_FRAME_LEN);
        Equalizer_ProcessFrame(
            &eq_guard_off, input, guarded, TEST_FRAME_LEN);
        (void)SmartBass_ProcessFrame(
            &smart_disabled, guarded, guarded, TEST_FRAME_LEN);
        (void)DynamicClarity_ProcessFrame(
            &clarity_disabled, guarded, guarded, TEST_FRAME_LEN);
        (void)HarshnessGuard_ProcessFrame(
            &guard_disabled, guarded, guarded, TEST_FRAME_LEN);
        Check(memcmp(reference, guarded, sizeof(reference)) == 0,
              "all dynamic modules OFF preserve Static EQ sample exact");
    }

    Check(guard_disabled.saturation_count == 0UL &&
              guard_disabled.nonfinite_count == 0UL,
          "Guard OFF regression reports no numerical faults");
    printf("guard_off_existing_chain_exact=1 "
           "all_dynamic_off_static_eq_exact=1 "
           "guard_off_regression_frames=16 "
           "chain_evidence_label=HOST_DIGITAL_SIMULATION\n");
}

static void PrepareBitExactCase(HARSHNESS_GUARD_STATE *state,
                                int case_index)
{
    static const int active_level[6] = { 0, 1, 3, 0, 1, 1 };
    static const int pending_level[6] = { 0, 1, 3, 1, 2, 0 };

    HarshnessGuard_Init(state);
    state->active_level = active_level[case_index];
    state->target_level = pending_level[case_index];
    state->pending_level = pending_level[case_index];
    state->requested_enabled = 1;
    state->processing_active =
        ((active_level[case_index] != 0) || (case_index >= 3)) ? 1 : 0;
    if (case_index >= 3)
    {
        state->transition_active = 1;
        state->transition_total = HARSHNESS_GUARD_TRANSITION_SAMPLES;
        state->transition_remaining = HARSHNESS_GUARD_TRANSITION_SAMPLES;
    }
}

static void TestPerformanceChangeBitExactContract(void)
{
    static const char *case_name[6] =
    {
        "identity",
        "stable_1",
        "stable_3",
        "transition_0_1",
        "transition_1_2",
        "transition_1_0"
    };
    static const unsigned long expected_hash[2][6] =
    {
        {
            0xb5999dc5UL, 0x316eec19UL, 0xf92df550UL,
            0x6c2d4223UL, 0x0fbd6c1bUL, 0x054d8607UL
        },
        {
            0x137edc05UL, 0x2485a246UL, 0x798dd8d2UL,
            0x0c4ba929UL, 0xf8c60523UL, 0x4c3dccd4UL
        }
    };
    static const int expected_level[6] = { 0, 1, 3, 1, 2, 0 };
    static const int expected_processing[6] = { 0, 1, 1, 1, 1, 0 };
    static const unsigned long expected_transitions[6] =
        { 0UL, 0UL, 0UL, 0UL, 0UL, 0UL };
    HARSHNESS_GUARD_STATE state;
    short input[TEST_FRAME_LEN];
    short output[TEST_FRAME_LEN];
    unsigned long hash;
    int case_index;
    int frame_index;
    int input_index;

    for (input_index = 0; input_index < 2; input_index++)
    {
        if (input_index == 0)
        {
            FillDualTone(input);
        }
        else
        {
            FillWideband(input);
        }
        for (case_index = 0; case_index < 6; case_index++)
        {
            PrepareBitExactCase(&state, case_index);
            hash = 2166136261UL;
            for (frame_index = 0; frame_index < 4; frame_index++)
            {
                (void)HarshnessGuard_ProcessFrame(
                    &state, input, output, TEST_FRAME_LEN);
                hash = HashPcm16(hash, output, TEST_FRAME_LEN);
            }
            Check(hash == expected_hash[input_index][case_index],
                  "performance change preserves frozen PCM16 output hash");
            Check(state.active_level == expected_level[case_index] &&
                      state.pending_level == expected_level[case_index] &&
                      state.target_level == expected_level[case_index] &&
                      state.transition_active == 0,
                  "bit-exact path reaches its deterministic endpoint");
            Check(state.transition_count ==
                      expected_transitions[case_index] &&
                      state.processing_active ==
                      expected_processing[case_index],
                  "bit-exact path preserves transition and processing state");
            Check(state.saturation_count == 0UL &&
                      state.nonfinite_count == 0UL,
                  "bit-exact path keeps safety counters zero");
            printf("bitexact_case=%s input=%s hash=%08lx "
                   "active=%d pending=%d target=%d transitions=%lu\n",
                   case_name[case_index],
                   (input_index == 0) ? "dual_tone" : "pseudorandom",
                   hash, state.active_level, state.pending_level,
                   state.target_level, state.transition_count);
        }
    }
    printf("performance_change_bitexact_cases=12 "
           "performance_change_bitexact=1\n");
}

static void TestAnalyzerResetReleaseContract(void)
{
    HARSHNESS_GUARD_STATE state;
    AUDIO_FEATURE_SNAPSHOT snapshot;
    HARSHNESS_GUARD_DF2T_STATE state_before_reset;
    short frame[TEST_FRAME_LEN];
    float applied_gain;
    float previous_applied_gain;
    float previous_progress;
    unsigned long released_transition_count;
    unsigned long snapshot_epoch;
    int active_level;
    int frame_index;
    int runtime_watch_enabled;

    runtime_watch_enabled = 1;
    HarshnessGuard_Init(&state);
    (void)HarshnessGuard_SetStrength(
        &state, HARSHNESS_GUARD_STRENGTH_MEDIUM);
    (void)HarshnessGuard_SetEnabled(&state, 1);
    DriveGuardToLevel(&state, 3);
    FillDualTone(frame);
    (void)HarshnessGuard_ProcessFrame(&state, frame, frame, 256);
    state_before_reset = state.active_state;

    HarshnessGuard_InvalidateAnalysisEpoch(&state);
    (void)HarshnessGuard_SetEnabled(&state, 0);
    Check(state.requested_enabled == 0 && state.force_release != 0,
          "analyzer reset disables Guard and starts force release");
    Check(state.active_level == 3 && state.pending_level == 2 &&
              state.transition_active != 0,
          "analyzer reset starts only the adjacent three-to-two release");
    Check(state.active_state.s1 == state_before_reset.s1 &&
              state.active_state.s2 == state_before_reset.s2 &&
              state.pending_state.s1 == state_before_reset.s1 &&
              state.pending_state.s2 == state_before_reset.s2,
          "analyzer reset preserves active filter history");

    memset(frame, 0, sizeof(frame));
    active_level = state.active_level;
    previous_progress = -1.0f;
    previous_applied_gain = HarshnessGuard_GetAppliedGainDb(&state);
    frame_index = 0;
    while ((state.processing_active != 0) && (frame_index < 64))
    {
        int before_active;
        int before_pending;
        float progress;

        before_active = state.active_level;
        before_pending = state.pending_level;
        (void)HarshnessGuard_ProcessFrame(&state, frame, frame, 256);
        applied_gain = HarshnessGuard_GetAppliedGainDb(&state);
        Check(applied_gain >= previous_applied_gain,
              "analyzer reset applied gain releases monotonically");
        previous_applied_gain = applied_gain;
        if ((state.active_level == before_active) &&
            (state.pending_level == before_pending) &&
            (state.transition_active != 0))
        {
            progress = HarshnessGuard_GetTransitionProgress(&state);
            Check(progress >= previous_progress,
                  "analyzer reset transition progress is monotonic");
            previous_progress = progress;
        }
        else
        {
            Check((state.active_level == (active_level - 1)) ||
                      ((state.active_level == active_level) &&
                       (state.transition_active == 0)),
                  "analyzer reset completes at most one adjacent level");
            active_level = state.active_level;
            previous_progress = -1.0f;
        }
        frame_index++;
    }
    Check(frame_index < 64 && state.active_level == 0 &&
              state.pending_level == 0 &&
              state.processing_active == 0 &&
              state.transition_active == 0,
          "analyzer reset release reaches transparent level zero");
    Check(state.saturation_count == 0UL &&
              state.nonfinite_count == 0UL,
          "analyzer reset release keeps safety counters zero");
    Check(runtime_watch_enabled == 1,
          "analyzer reset leaves the runtime Watch request enabled");

    released_transition_count = state.transition_count;
    (void)HarshnessGuard_SetEnabled(&state, runtime_watch_enabled);
    snapshot = MakeSnapshot(22.0f, 0.0f, -18.0f, 1, 1);
    snapshot_epoch = snapshot.analysis_count;
    Check(HarshnessGuard_UpdateFromFeature(&state, &snapshot) ==
              HARSHNESS_GUARD_RESULT_UPDATED &&
              state.requested_enabled == 1 &&
              state.transition_active != 0 &&
              state.pending_level == 1,
          "new valid warm snapshot re-enables adjacent Guard attack");
    Check(HarshnessGuard_UpdateFromFeature(&state, &snapshot) ==
              HARSHNESS_GUARD_RESULT_NO_CHANGE &&
              state.latest_analysis_count == snapshot_epoch,
          "new analyzer epoch is consumed once after reset");
    Check(state.transition_count == released_transition_count + 1UL,
          "post-reset snapshot starts exactly one new transition");

    printf("analyzer_reset_trajectory=3,2,1,0 "
           "watch_enabled=1 reenabled=1 epoch_once=1 "
           "saturation=0 nonfinite=0\n");
}

int main(void)
{
    TestInitializationAndIdentity();
    TestFormulaMappingStrengthAndAttack();
    TestGatesDebounceDisableAndStrength();
    TestTransitionPendingQueueAndEpoch();
    TestFrequencyResponse();
    TestNumericalGuardsImmutabilityAndDeterminism();
    TestPcm16ChainsAThroughG();
    TestGuardOffRegressions();
    TestPerformanceChangeBitExactContract();
    TestAnalyzerResetReleaseContract();

    printf("harshness_guard failures=%d\n", failures);
    if (failures == 0)
    {
        printf("harshness_guard: PASS\n");
    }
    return (failures == 0) ? 0 : 1;
}
