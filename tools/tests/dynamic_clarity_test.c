#include <math.h>
#include <stdio.h>
#include <string.h>

#include "user_dynamic_clarity.h"
#include "user_smart_bass.h"

#define TEST_PI 3.14159265358979323846f

static int failures = 0;
static unsigned long analysis_sequence = 0UL;

typedef struct
{
    double error_sum;
    double error_square_sum;
    double max_error;
    unsigned long clip_count;
    unsigned long sample_count;
} QUANTIZATION_METRIC;

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
    return isfinite(value) ? 1 : 0;
}

static int IntAbs(int value)
{
    return (value < 0) ? -value : value;
}

static void FillPattern(short *frame, int salt)
{
    int index;

    for (index = 0; index < DYNAMIC_CLARITY_MAX_FRAME_LEN; index++)
    {
        frame[index] =
            (short)((((index + salt) * 7919) % 60001) - 30000);
    }
}

static void FillSine(short *frame, float frequency_hz, float amplitude)
{
    int index;

    for (index = 0; index < DYNAMIC_CLARITY_MAX_FRAME_LEN; index++)
    {
        float value;

        value = amplitude * sinf(2.0f * TEST_PI * frequency_hz *
                                 (float)index /
                                 DYNAMIC_CLARITY_SAMPLE_RATE);
        frame[index] = (short)((value >= 0.0f) ?
                               (value + 0.5f) : (value - 0.5f));
    }
}

static void FillMusicLike(short *frame)
{
    int index;

    for (index = 0; index < DYNAMIC_CLARITY_MAX_FRAME_LEN; index++)
    {
        float time;
        float value;

        time = (float)index / DYNAMIC_CLARITY_SAMPLE_RATE;
        value = 1500.0f * sinf(2.0f * TEST_PI * 100.0f * time) +
                1200.0f * sinf(2.0f * TEST_PI * 400.0f * time) +
                 900.0f * sinf(2.0f * TEST_PI * 1000.0f * time) +
                 600.0f * sinf(2.0f * TEST_PI * 3000.0f * time);
        frame[index] = (short)((value >= 0.0f) ?
                               (value + 0.5f) : (value - 0.5f));
    }
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

static void UpdateQuantizationMetric(QUANTIZATION_METRIC *metric,
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
        metric->clip_count++;
    }
}

static double QuantizationRmsError(const QUANTIZATION_METRIC *metric)
{
    if (metric->sample_count == 0UL)
    {
        return 0.0;
    }
    return sqrt(metric->error_square_sum / (double)metric->sample_count);
}

static double QuantizationDcError(const QUANTIZATION_METRIC *metric)
{
    if (metric->sample_count == 0UL)
    {
        return 0.0;
    }
    return fabs(metric->error_sum / (double)metric->sample_count);
}

static double QuantizationResidualDbfs(const QUANTIZATION_METRIC *metric)
{
    double rms_error;

    rms_error = QuantizationRmsError(metric);
    if (rms_error <= 1.0e-20)
    {
        return -240.0;
    }
    return 20.0 * log10(rms_error / 32768.0);
}

static AUDIO_FEATURE_SNAPSHOT MakeSnapshot(float mud_db,
                                           float presence_db,
                                           float rms_dbfs,
                                           int valid,
                                           int warmup)
{
    AUDIO_FEATURE_SNAPSHOT snapshot;

    memset(&snapshot, 0, sizeof(snapshot));
    analysis_sequence++;
    snapshot.analysis_count = analysis_sequence;
    snapshot.relative_db[AUDIO_FEATURE_MUD] = mud_db;
    snapshot.relative_db[AUDIO_FEATURE_PRESENCE] = presence_db;
    snapshot.rms_dbfs = rms_dbfs;
    snapshot.valid = valid;
    snapshot.warmup_complete = warmup;
    return snapshot;
}

static void SettleTransitions(DYNAMIC_CLARITY_STATE *state)
{
    short frame[DYNAMIC_CLARITY_MAX_FRAME_LEN];
    int guard;

    memset(frame, 0, sizeof(frame));
    guard = 0;
    while ((state->transition_active != 0) && (guard < 64))
    {
        Check(DynamicClarity_ProcessFrame(
                  state, frame, frame,
                  DYNAMIC_CLARITY_MAX_FRAME_LEN) ==
                  DYNAMIC_CLARITY_RESULT_UPDATED,
              "transition frame processes successfully");
        guard++;
    }
    Check(guard < 64,
          "transition settles within a bounded number of frames");
}

static void DriveToLevel(DYNAMIC_CLARITY_STATE *state,
                         int requested_level)
{
    int guard;

    guard = 0;
    while ((state->active_level != requested_level) && (guard < 32))
    {
        AUDIO_FEATURE_SNAPSHOT snapshot;
        int before;

        before = state->active_level;
        snapshot = MakeSnapshot(20.0f, 0.0f, -18.0f, 1, 1);
        Check(DynamicClarity_UpdateFromFeature(state, &snapshot) ==
                  DYNAMIC_CLARITY_RESULT_UPDATED,
              "high-masking feature decision is accepted");
        Check(IntAbs(state->pending_level - before) <= 1,
              "attack never requests more than one adjacent level");
        SettleTransitions(state);
        Check(IntAbs(state->active_level - before) <= 1,
              "one analysis increases at most one applied level");
        guard++;
    }
    Check(state->active_level == requested_level,
          "requested strength ceiling is reached gradually");
}

static void SettleSmartBass(SMART_BASS_STATE *state)
{
    short frame[SMART_BASS_MAX_FRAME_LEN];
    int guard;

    memset(frame, 0, sizeof(frame));
    guard = 0;
    while ((state->transition_active != 0) && (guard < 64))
    {
        (void)SmartBass_ProcessFrame(
            state, frame, frame, SMART_BASS_MAX_FRAME_LEN);
        guard++;
    }
    Check(guard < 64, "Smart Bass regression transition settles");
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
          "Smart Bass regression reaches the requested level");
}

static float BiquadGainDb(const DYNAMIC_CLARITY_BIQUAD *coefficient,
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
    magnitude = sqrt(real_value * real_value +
                     imag_value * imag_value);
    if (magnitude < 1.0e-20)
    {
        return -240.0f;
    }
    return (float)(20.0 * log10(magnitude));
}

static void TestInitializationIdentityAndPeakingApi(void)
{
    static const float static_gains_db[5] =
        { -12.0f, -3.0f, 0.0f, 6.0f, 12.0f };
    DYNAMIC_CLARITY_STATE state;
    EQ_BIQUAD public_section;
    EQ_BIQUAD static_section;
    short input[DYNAMIC_CLARITY_MAX_FRAME_LEN];
    short before[DYNAMIC_CLARITY_MAX_FRAME_LEN];
    short output[DYNAMIC_CLARITY_MAX_FRAME_LEN];
    unsigned long scans_before;
    int gain_index;
    int peaking_bit_exact;
    int section;

    DynamicClarity_Init(&state);
    Check(state.initialized == 1,
          "init marks Dynamic Clarity initialized");
    Check(state.requested_enabled == 0,
          "init defaults runtime disabled");
    Check(state.strength == DYNAMIC_CLARITY_STRENGTH_MEDIUM,
          "init defaults to medium strength");
    Check(state.active_level == 0 && state.target_level == 0,
          "init starts at identity level");
    Check(state.transition_total == DYNAMIC_CLARITY_TRANSITION_SAMPLES,
          "init selects the exact 80 ms transition length");
    Check(sizeof(state.coefficient_table) ==
              sizeof(DYNAMIC_CLARITY_BIQUAD) *
              DYNAMIC_CLARITY_LEVEL_COUNT,
          "state owns exactly seven fixed coefficient sets");

    scans_before = EQ_DebugHeadroomScanCount;
    Check(Equalizer_DesignRbjPeakingAt(
              &public_section, DYNAMIC_CLARITY_CENTER_HZ, -3.0f,
              DYNAMIC_CLARITY_BANDWIDTH_OCTAVES) == 1,
          "public peaking design accepts the clarity contract");
    Check(EQ_DebugHeadroomScanCount == scans_before,
          "public peaking design does not run a headroom scan");
    peaking_bit_exact = 1;
    for (section = 1; section < (EQ_NUM_BANDS - 1); section++)
    {
        for (gain_index = 0; gain_index < 5; gain_index++)
        {
            if ((Equalizer_DesignRbjSection(
                     &static_section, section,
                     static_gains_db[gain_index]) != 1) ||
                (Equalizer_DesignRbjPeakingAt(
                     &public_section, Equalizer_GetBandCenterHz(section),
                     static_gains_db[gain_index], 1.0f) != 1) ||
                (memcmp(&static_section, &public_section,
                        sizeof(public_section)) != 0))
            {
                peaking_bit_exact = 0;
            }
        }
    }
    Check(peaking_bit_exact != 0,
          "one-octave public peaking math is bit exact for all static peaking sections");
    Check(Equalizer_DesignRbjPeakingAt(0, 400.0f, -3.0f, 1.0f) == 0,
          "public peaking rejects null output");
    Check(Equalizer_DesignRbjPeakingAt(
              &public_section, NAN, -3.0f, 1.0f) == 0,
          "public peaking rejects nonfinite frequency");
    Check(Equalizer_DesignRbjPeakingAt(
              &public_section, 400.0f, INFINITY, 1.0f) == 0,
          "public peaking rejects nonfinite gain");
    Check(Equalizer_DesignRbjPeakingAt(
              &public_section, 400.0f, -3.0f, NAN) == 0,
          "public peaking rejects nonfinite bandwidth");
    Check(Equalizer_DesignRbjPeakingAt(
              &public_section, 0.0f, -3.0f, 1.0f) == 0 &&
              Equalizer_DesignRbjPeakingAt(
                  &public_section, EQ_SAMPLE_RATE * 0.5f,
                  -3.0f, 1.0f) == 0,
          "public peaking rejects frequencies outside open Nyquist range");
    Check(Equalizer_DesignRbjPeakingAt(
              &public_section, 400.0f, EQ_GAIN_MIN_DB - 0.1f,
              1.0f) == 0,
          "public peaking rejects out-of-range gain");
    Check(Equalizer_DesignRbjPeakingAt(
              &public_section, 400.0f, -3.0f, 0.0f) == 0 &&
              Equalizer_DesignRbjPeakingAt(
                  &public_section, 400.0f, -3.0f, 9.0f) == 0,
          "public peaking rejects unreasonable bandwidth");

    FillPattern(input, 0);
    memcpy(before, input, sizeof(input));
    memset(output, 0, sizeof(output));
    Check(DynamicClarity_ProcessFrame(
              &state, input, output,
              DYNAMIC_CLARITY_MAX_FRAME_LEN) ==
              DYNAMIC_CLARITY_RESULT_UPDATED,
          "disabled identity frame succeeds");
    Check(memcmp(input, output, sizeof(input)) == 0,
          "disabled non-in-place output is sample exact");
    Check(memcmp(input, before, sizeof(input)) == 0,
          "disabled non-in-place input remains immutable");
    Check(DynamicClarity_ProcessFrame(
              &state, input, input,
              DYNAMIC_CLARITY_MAX_FRAME_LEN) ==
              DYNAMIC_CLARITY_RESULT_UPDATED,
          "disabled in-place frame succeeds");
    Check(memcmp(input, before, sizeof(input)) == 0,
          "disabled in-place output is sample exact");
    Check(state.processing_active == 0,
          "identity path does not report active filtering");

    Check(DynamicClarity_SetEnabled(&state, 1) ==
              DYNAMIC_CLARITY_RESULT_UPDATED,
          "runtime enable is accepted");
    Check(DynamicClarity_ProcessFrame(
              &state, input, output,
              DYNAMIC_CLARITY_MAX_FRAME_LEN) ==
              DYNAMIC_CLARITY_RESULT_UPDATED,
          "enabled level-zero frame succeeds");
    Check(memcmp(input, output, sizeof(input)) == 0,
          "enabled level zero remains sample exact");

    printf("dynamic_clarity_state_bytes=%u transition_samples=%d "
           "identity_exact=1 peaking_static_bit_exact=1 "
           "peaking_static_bit_exact_cases=40\n",
           (unsigned int)sizeof(state),
           DYNAMIC_CLARITY_TRANSITION_SAMPLES);
}

static void TestMaskingMappingStrengthAndAttack(void)
{
    static const float masking_points[5] =
        { 4.0f, 6.5f, 9.0f, 11.5f, 14.0f };
    static const int expected_medium[5] = { 0, 1, 2, 3, 4 };
    static const int maximum_levels[4] = { 0, 2, 4, 6 };
    DYNAMIC_CLARITY_STATE state;
    AUDIO_FEATURE_SNAPSHOT snapshot;
    AUDIO_FEATURE_SNAPSHOT before;
    unsigned long decision_before;
    int point;
    int strength;

    DynamicClarity_Init(&state);
    (void)DynamicClarity_SetEnabled(&state, 1);
    snapshot = MakeSnapshot(7.25f, -2.75f, -18.0f, 1, 1);
    before = snapshot;
    Check(DynamicClarity_UpdateFromFeature(&state, &snapshot) ==
              DYNAMIC_CLARITY_RESULT_UPDATED,
          "masking feature is accepted");
    Check(fabsf(state.latest_masking_db - 10.0f) < 1.0e-6f,
          "masking equals mud minus presence");
    Check(FloatFinite(state.latest_masking_db),
          "published masking remains finite");
    Check(memcmp(&snapshot, &before, sizeof(snapshot)) == 0,
          "feature update does not modify the snapshot");
    decision_before = state.decision_count;
    Check(DynamicClarity_UpdateFromFeature(&state, &snapshot) ==
              DYNAMIC_CLARITY_RESULT_NO_CHANGE &&
              state.decision_count == decision_before,
          "unchanged analysis count cannot repeat a decision");

    for (point = 0; point < 5; point++)
    {
        DynamicClarity_Init(&state);
        (void)DynamicClarity_SetEnabled(&state, 1);
        snapshot = MakeSnapshot(masking_points[point], 0.0f,
                                -18.0f, 1, 1);
        Check(DynamicClarity_UpdateFromFeature(&state, &snapshot) ==
                  DYNAMIC_CLARITY_RESULT_UPDATED,
              "mapping feature is accepted");
        Check(state.target_level == expected_medium[point],
              "medium mapping quantizes the 4-14 dB span monotonically");
        Check(DynamicClarity_GetRequestedGainDb(&state) <= 0.0f &&
              DynamicClarity_GetRequestedGainDb(&state) >= -3.0f,
              "mapping always stays within zero to minus three dB");
    }

    for (strength = DYNAMIC_CLARITY_STRENGTH_OFF;
         strength <= DYNAMIC_CLARITY_STRENGTH_HIGH; strength++)
    {
        DynamicClarity_Init(&state);
        Check(DynamicClarity_SetStrength(&state, strength) >= 0,
              "strength selection is accepted");
        (void)DynamicClarity_SetEnabled(&state, 1);
        if (maximum_levels[strength] > 0)
        {
            DriveToLevel(&state, maximum_levels[strength]);
        }
        Check(state.active_level <= maximum_levels[strength],
              "applied level respects the strength ceiling");
        Check(DynamicClarity_GetAppliedGainDb(&state) >=
                  -DYNAMIC_CLARITY_LEVEL_STEP_DB *
                  (float)maximum_levels[strength] - 0.001f,
              "applied attenuation respects the strength ceiling");
    }

    printf("masking_formula_exact=1 snapshot_immutable=1 "
           "mapping_span_db=4,14 strength_max_levels=0,2,4,6 "
           "attack_step_max=1\n");
}

static void ReleaseCauseToIdentity(float mud_db,
                                   float presence_db,
                                   float rms_dbfs,
                                   int valid,
                                   int warmup,
                                   int expected_reason,
                                   int expect_invalid_count)
{
    DYNAMIC_CLARITY_STATE state;
    AUDIO_FEATURE_SNAPSHOT snapshot;
    int guard;

    DynamicClarity_Init(&state);
    (void)DynamicClarity_SetEnabled(&state, 1);
    DriveToLevel(&state, 2);
    guard = 0;
    while ((state.active_level > 0) && (guard < 8))
    {
        snapshot = MakeSnapshot(mud_db, presence_db, rms_dbfs,
                                valid, warmup);
        (void)DynamicClarity_UpdateFromFeature(&state, &snapshot);
        Check(state.reason == expected_reason,
              "first release confirmation preserves its cause reason");
        snapshot = MakeSnapshot(mud_db, presence_db, rms_dbfs,
                                valid, warmup);
        (void)DynamicClarity_UpdateFromFeature(&state, &snapshot);
        Check(state.reason == DYNAMIC_CLARITY_REASON_RELEASING,
              "second release confirmation reports releasing");
        SettleTransitions(&state);
        guard++;
    }
    Check(guard < 8 && state.active_level == 0,
          "release cause returns the applied level to identity");
    if (expect_invalid_count != 0)
    {
        Check(state.invalid_release_count > 0UL,
              "failed eligibility gates are counted during release");
    }
}

static void TestGatesReleaseDebounceAndDisable(void)
{
    DYNAMIC_CLARITY_STATE state;
    AUDIO_FEATURE_SNAPSHOT snapshot;
    short frame[DYNAMIC_CLARITY_MAX_FRAME_LEN];
    short before[DYNAMIC_CLARITY_MAX_FRAME_LEN];
    unsigned long debounce_transition_delta;
    unsigned long transition_before;
    int index;

    DynamicClarity_Init(&state);
    (void)DynamicClarity_SetEnabled(&state, 1);
    snapshot = MakeSnapshot(4.0f, 0.0f, -18.0f, 1, 1);
    (void)DynamicClarity_UpdateFromFeature(&state, &snapshot);
    Check(state.target_level == 0 &&
              state.reason == DYNAMIC_CLARITY_REASON_BELOW_THRESHOLD,
          "exact 4 dB masking boundary requests identity");

    DynamicClarity_Reset(&state);
    (void)DynamicClarity_SetEnabled(&state, 1);
    snapshot = MakeSnapshot(2.0f, -12.0f, -45.0f, 1, 1);
    (void)DynamicClarity_UpdateFromFeature(&state, &snapshot);
    Check(state.target_level == 4 &&
              state.reason == DYNAMIC_CLARITY_REASON_MASKING,
          "exact RMS, Mud, Presence, and full-mask boundaries are eligible");

    ReleaseCauseToIdentity(20.0f, 0.0f, -18.0f, 0, 1,
                           DYNAMIC_CLARITY_REASON_INVALID, 1);
    ReleaseCauseToIdentity(20.0f, 0.0f, -18.0f, 1, 0,
                           DYNAMIC_CLARITY_REASON_NOT_WARM, 1);
    ReleaseCauseToIdentity(20.0f, 0.0f, -45.01f, 1, 1,
                           DYNAMIC_CLARITY_REASON_LOW_LEVEL, 1);
    ReleaseCauseToIdentity(-0.01f, -12.0f, -18.0f, 1, 1,
                           DYNAMIC_CLARITY_REASON_WEAK_MUD, 1);
    ReleaseCauseToIdentity(3.0f, -12.01f, -18.0f, 1, 1,
                           DYNAMIC_CLARITY_REASON_WEAK_PRESENCE, 1);
    ReleaseCauseToIdentity(4.0f, 0.0f, -18.0f, 1, 1,
                           DYNAMIC_CLARITY_REASON_BELOW_THRESHOLD, 0);

    DynamicClarity_Init(&state);
    (void)DynamicClarity_SetEnabled(&state, 1);
    DriveToLevel(&state, 1);
    transition_before = state.transition_count;
    for (index = 0; index < 8; index++)
    {
        float masking_db;

        masking_db = ((index & 1) == 0) ? 3.9f : 5.4f;
        snapshot = MakeSnapshot(masking_db, 0.0f, -18.0f, 1, 1);
        (void)DynamicClarity_UpdateFromFeature(&state, &snapshot);
    }
    Check(state.active_level == 1 && state.transition_active == 0,
          "threshold alternation does not chatter the applied level");
    Check(state.transition_count == transition_before,
          "threshold alternation does not start repeated transitions");
    debounce_transition_delta = state.transition_count - transition_before;

    snapshot = MakeSnapshot(20.0f, 0.0f, -18.0f, 1, 1);
    (void)DynamicClarity_UpdateFromFeature(&state, &snapshot);
    Check(state.transition_active == 1 && state.pending_level == 2,
          "new attack starts before explicit disable");
    Check(DynamicClarity_SetEnabled(&state, 0) ==
              DYNAMIC_CLARITY_RESULT_UPDATED,
          "explicit disable is accepted during transition");
    Check(state.transition_active == 1 && state.pending_level == 2,
          "disable does not overwrite the in-flight endpoint");
    SettleTransitions(&state);
    Check(state.active_level == 0 && state.processing_active == 0,
          "disable chains adjacent transitions until identity");
    FillPattern(frame, 1);
    memcpy(before, frame, sizeof(frame));
    (void)DynamicClarity_ProcessFrame(
        &state, frame, frame, DYNAMIC_CLARITY_MAX_FRAME_LEN);
    Check(memcmp(frame, before, sizeof(frame)) == 0,
          "released level zero returns to exact identity");

    printf("gate_causes_to_zero=6 release_confirmations=2 "
           "debounce_transitions=%lu disable_identity_exact=1\n",
           debounce_transition_delta);
}

static void TestQueueTransitionAndEpoch(void)
{
    DYNAMIC_CLARITY_STATE state;
    AUDIO_FEATURE_SNAPSHOT snapshot;
    short frame[DYNAMIC_CLARITY_MAX_FRAME_LEN];
    short impulse_input[DYNAMIC_CLARITY_MAX_FRAME_LEN];
    short impulse_before[DYNAMIC_CLARITY_MAX_FRAME_LEN];
    short impulse_output[DYNAMIC_CLARITY_MAX_FRAME_LEN];
    float previous_progress;
    unsigned long decision_before;
    unsigned long transition_saturation;
    int frame_index;

    DynamicClarity_Init(&state);
    (void)DynamicClarity_SetStrength(
        &state, DYNAMIC_CLARITY_STRENGTH_HIGH);
    (void)DynamicClarity_SetEnabled(&state, 1);
    snapshot = MakeSnapshot(20.0f, 0.0f, -18.0f, 1, 1);
    (void)DynamicClarity_UpdateFromFeature(&state, &snapshot);
    Check(state.active_level == 0 && state.pending_level == 1,
          "first high request starts only zero-to-one transition");
    snapshot = MakeSnapshot(20.0f, 0.0f, -18.0f, 1, 1);
    (void)DynamicClarity_UpdateFromFeature(&state, &snapshot);
    Check(state.pending_level == 1 && state.queued_level_valid == 1 &&
              state.queued_level == 2,
          "second request queues latest adjacent level without overwrite");
    SettleTransitions(&state);
    Check(state.active_level == 2 && state.transition_active == 0,
          "queued request applies only after the in-flight endpoint");

    DynamicClarity_Reset(&state);
    (void)DynamicClarity_SetStrength(
        &state, DYNAMIC_CLARITY_STRENGTH_HIGH);
    (void)DynamicClarity_SetEnabled(&state, 1);
    snapshot = MakeSnapshot(20.0f, 0.0f, -18.0f, 1, 1);
    (void)DynamicClarity_UpdateFromFeature(&state, &snapshot);
    snapshot = MakeSnapshot(20.0f, 0.0f, -18.0f, 1, 1);
    (void)DynamicClarity_UpdateFromFeature(&state, &snapshot);
    snapshot = MakeSnapshot(4.0f, 0.0f, -18.0f, 1, 1);
    (void)DynamicClarity_UpdateFromFeature(&state, &snapshot);
    Check(state.pending_level == 1 && state.queued_level_valid == 0,
          "latest lower target cancels a stale queued attack");

    FillSine(frame, 400.0f, 24000.0f);
    previous_progress = 0.0f;
    for (frame_index = 0; frame_index < 4; frame_index++)
    {
        float progress;

        (void)DynamicClarity_ProcessFrame(
            &state, frame, frame, DYNAMIC_CLARITY_MAX_FRAME_LEN);
        progress = DynamicClarity_GetTransitionProgress(&state);
        Check(FloatFinite(progress),
              "transition progress remains finite");
        if (state.transition_active != 0)
        {
            Check(progress >= previous_progress,
                  "transition progress is monotonic");
            previous_progress = progress;
        }
    }
    Check(state.active_level == 1 && state.transition_active == 0,
          "latest target does not overwrite the in-flight endpoint");
    Check(state.saturation_count == 0UL,
          "normal transition stimulus does not saturate");
    transition_saturation = state.saturation_count;

    snapshot = MakeSnapshot(4.0f, 0.0f, -18.0f, 1, 1);
    (void)DynamicClarity_UpdateFromFeature(&state, &snapshot);
    Check(state.pending_level == 0 && state.transition_active == 1,
          "second lower observation starts one-step release");
    SettleTransitions(&state);
    Check(state.active_level == 0,
          "latest lower target eventually applies");

    DynamicClarity_Reset(&state);
    (void)DynamicClarity_SetStrength(
        &state, DYNAMIC_CLARITY_STRENGTH_HIGH);
    (void)DynamicClarity_SetEnabled(&state, 1);
    DriveToLevel(&state, 3);
    snapshot = MakeSnapshot(4.0f, 0.0f, -18.0f, 1, 1);
    (void)DynamicClarity_UpdateFromFeature(&state, &snapshot);
    snapshot = MakeSnapshot(4.0f, 0.0f, -18.0f, 1, 1);
    (void)DynamicClarity_UpdateFromFeature(&state, &snapshot);
    Check(state.transition_active != 0 && state.pending_level == 2,
          "confirmed release starts one adjacent transition");
    snapshot = MakeSnapshot(4.0f, 0.0f, -18.0f, 1, 1);
    (void)DynamicClarity_UpdateFromFeature(&state, &snapshot);
    snapshot = MakeSnapshot(4.0f, 0.0f, -18.0f, 1, 1);
    (void)DynamicClarity_UpdateFromFeature(&state, &snapshot);
    Check(state.queued_level_valid != 0 && state.queued_level == 1,
          "two more confirmations queue the next adjacent release");
    snapshot = MakeSnapshot(4.0f, 0.0f, -18.0f, 1, 1);
    (void)DynamicClarity_UpdateFromFeature(&state, &snapshot);
    Check(state.queued_level_valid != 0 && state.queued_level == 1,
          "repeated release observation preserves the confirmed queue");
    SettleTransitions(&state);
    Check(state.active_level == 1,
          "preserved queued release applies after the first endpoint");

    DynamicClarity_Reset(&state);
    (void)DynamicClarity_SetEnabled(&state, 1);
    snapshot = MakeSnapshot(20.0f, 0.0f, -18.0f, 1, 1);
    (void)DynamicClarity_UpdateFromFeature(&state, &snapshot);
    decision_before = state.decision_count;
    (void)DynamicClarity_SetEnabled(&state, 0);
    (void)DynamicClarity_UpdateFromFeature(&state, &snapshot);
    DynamicClarity_InvalidateAnalysisEpoch(&state);
    Check(state.latest_analysis_count == ~0UL,
          "analysis epoch invalidation discards the prior count");
    (void)DynamicClarity_SetEnabled(&state, 1);
    Check(DynamicClarity_UpdateFromFeature(&state, &snapshot) ==
              DYNAMIC_CLARITY_RESULT_UPDATED &&
              state.decision_count == (decision_before + 1UL),
          "first snapshot after epoch reset accepts a reused count");

    DynamicClarity_Reset(&state);
    (void)DynamicClarity_SetEnabled(&state, 1);
    snapshot = MakeSnapshot(20.0f, 0.0f, -18.0f, 1, 1);
    (void)DynamicClarity_UpdateFromFeature(&state, &snapshot);
    for (frame_index = 0; frame_index < 4; frame_index++)
    {
        memset(impulse_input, 0, sizeof(impulse_input));
        if (frame_index == 0)
        {
            impulse_input[0] = 29490;
        }
        memcpy(impulse_before, impulse_input, sizeof(impulse_input));
        (void)DynamicClarity_ProcessFrame(
            &state, impulse_input, impulse_output,
            DYNAMIC_CLARITY_MAX_FRAME_LEN);
        Check(memcmp(impulse_input, impulse_before,
                     sizeof(impulse_input)) == 0,
              "0.9FS impulse input remains immutable");
    }
    Check(state.saturation_count == 0UL,
          "0.9FS impulse transition does not saturate");

    printf("latest_wins=1 queued_applied=1 queued_release_preserved=1 "
           "transition_progress_monotonic=1 sequence_epoch_reset=1 "
           "transition_saturation=%lu impulse_09fs_saturation=%lu\n",
           transition_saturation, state.saturation_count);
}

static void TestFrequencyResponse(void)
{
    static const float frequencies[6] =
        { 100.0f, 250.0f, 400.0f, 800.0f, 2000.0f, 8000.0f };
    DYNAMIC_CLARITY_STATE state;
    float level6_center_db;
    float low_abs_max;
    float high_abs_max;
    float positive_max;
    int frequency;
    int level;

    DynamicClarity_Init(&state);
    low_abs_max = 0.0f;
    high_abs_max = 0.0f;
    positive_max = -1000.0f;
    for (frequency = 0; frequency < 6; frequency++)
    {
        float previous_gain;

        previous_gain = 0.0f;
        for (level = 0; level < DYNAMIC_CLARITY_LEVEL_COUNT; level++)
        {
            float gain_db;

            gain_db = BiquadGainDb(
                &state.coefficient_table[level], frequencies[frequency]);
            Check(FloatFinite(gain_db),
                  "fixed-level response is finite");
            if ((frequency == 2) && (level > 0))
            {
                Check(gain_db <= previous_gain + 0.002f,
                      "400 Hz attenuation grows monotonically");
            }
            if (gain_db > positive_max)
            {
                positive_max = gain_db;
            }
            if ((frequency == 0) && (fabsf(gain_db) > low_abs_max))
            {
                low_abs_max = fabsf(gain_db);
            }
            if ((frequency == 5) && (fabsf(gain_db) > high_abs_max))
            {
                high_abs_max = fabsf(gain_db);
            }
            previous_gain = gain_db;
        }
    }
    level6_center_db = BiquadGainDb(
        &state.coefficient_table[6], DYNAMIC_CLARITY_CENTER_HZ);
    Check(fabsf(level6_center_db + 3.0f) < 0.08f,
          "level six is approximately minus 3 dB at 400 Hz");
    Check(low_abs_max < 0.20f,
          "100 Hz response remains close to zero dB");
    Check(high_abs_max < 0.01f,
          "8 kHz response remains effectively zero dB");
    Check(positive_max < 0.002f,
          "all checked fixed-level responses avoid positive gain");

    printf("level6_400hz_db=%.5f low_100hz_abs_max_db=%.6f "
           "high_8khz_abs_max_db=%.6f "
           "response_positive_max_db=%.6f\n",
           level6_center_db, low_abs_max, high_abs_max, positive_max);
}

static void TestNumericalGuardsImmutabilityAndDeterminism(void)
{
    DYNAMIC_CLARITY_STATE first;
    DYNAMIC_CLARITY_STATE second;
    AUDIO_FEATURE_SNAPSHOT snapshot;
    AUDIO_FEATURE_SNAPSHOT before_snapshot;
    short input[DYNAMIC_CLARITY_MAX_FRAME_LEN];
    short before[DYNAMIC_CLARITY_MAX_FRAME_LEN];
    short output_first[DYNAMIC_CLARITY_MAX_FRAME_LEN];
    short output_second[DYNAMIC_CLARITY_MAX_FRAME_LEN];

    DynamicClarity_Init(&first);
    DynamicClarity_Init(&second);
    Check(DynamicClarity_SetStrength(&first, -99) ==
              DYNAMIC_CLARITY_RESULT_UPDATED &&
              first.strength == DYNAMIC_CLARITY_STRENGTH_OFF,
          "negative strength normalizes to OFF");
    Check(DynamicClarity_SetStrength(&first, 99) ==
              DYNAMIC_CLARITY_RESULT_UPDATED &&
              first.strength == DYNAMIC_CLARITY_STRENGTH_HIGH,
          "large strength normalizes to HIGH");
    Check(DynamicClarity_ProcessFrame(0, input, output_first, 1) ==
              DYNAMIC_CLARITY_RESULT_ERROR,
          "null state is rejected");
    Check(DynamicClarity_ProcessFrame(&first, 0, output_first, 1) ==
              DYNAMIC_CLARITY_RESULT_ERROR,
          "null input is rejected");
    Check(DynamicClarity_ProcessFrame(&first, input, 0, 1) ==
              DYNAMIC_CLARITY_RESULT_ERROR,
          "null output is rejected");
    Check(DynamicClarity_ProcessFrame(&first, input, output_first, 0) ==
              DYNAMIC_CLARITY_RESULT_ERROR,
          "zero sample count is rejected");
    Check(DynamicClarity_ProcessFrame(
              &first, input, output_first,
              DYNAMIC_CLARITY_MAX_FRAME_LEN + 1) ==
              DYNAMIC_CLARITY_RESULT_ERROR,
          "oversized sample count is rejected");
    Check(DynamicClarity_UpdateFromFeature(0, &snapshot) ==
              DYNAMIC_CLARITY_RESULT_ERROR,
          "null feature state is rejected");
    Check(DynamicClarity_UpdateFromFeature(&first, 0) ==
              DYNAMIC_CLARITY_RESULT_ERROR,
          "null feature snapshot is rejected");

    DynamicClarity_Reset(&first);
    (void)DynamicClarity_SetEnabled(&first, 1);
    snapshot = MakeSnapshot(NAN, 0.0f, -18.0f, 1, 1);
    before_snapshot = snapshot;
    (void)DynamicClarity_UpdateFromFeature(&first, &snapshot);
    Check(first.target_level == 0 && first.nonfinite_count == 1UL,
          "nonfinite feature requests bounded identity release");
    Check(FloatFinite(first.latest_mud_relative_db) &&
              FloatFinite(first.latest_presence_relative_db) &&
              FloatFinite(first.latest_masking_db) &&
              FloatFinite(first.latest_rms_dbfs),
          "nonfinite feature is not published into runtime state");
    Check(memcmp(&snapshot, &before_snapshot, sizeof(snapshot)) == 0,
          "nonfinite feature handling still preserves the snapshot");

    DynamicClarity_Reset(&first);
    (void)DynamicClarity_SetEnabled(&first, 1);
    snapshot = MakeSnapshot(20.0f, INFINITY, -18.0f, 1, 1);
    (void)DynamicClarity_UpdateFromFeature(&first, &snapshot);
    Check(first.target_level == 0 && first.nonfinite_count == 1UL &&
              FloatFinite(first.latest_presence_relative_db),
          "infinite Presence is contained by the identity release");

    DynamicClarity_Reset(&first);
    (void)DynamicClarity_SetEnabled(&first, 1);
    snapshot = MakeSnapshot(20.0f, 0.0f, -INFINITY, 1, 1);
    (void)DynamicClarity_UpdateFromFeature(&first, &snapshot);
    Check(first.target_level == 0 && first.nonfinite_count == 1UL &&
              FloatFinite(first.latest_rms_dbfs),
          "infinite RMS is contained by the identity release");

    DynamicClarity_Reset(&first);
    DynamicClarity_Reset(&second);
    (void)DynamicClarity_SetEnabled(&first, 1);
    (void)DynamicClarity_SetEnabled(&second, 1);
    snapshot = MakeSnapshot(20.0f, 0.0f, -18.0f, 1, 1);
    (void)DynamicClarity_UpdateFromFeature(&first, &snapshot);
    (void)DynamicClarity_UpdateFromFeature(&second, &snapshot);
    FillPattern(input, 2);
    memcpy(before, input, sizeof(input));
    (void)DynamicClarity_ProcessFrame(
        &first, input, output_first, DYNAMIC_CLARITY_MAX_FRAME_LEN);
    (void)DynamicClarity_ProcessFrame(
        &second, input, output_second, DYNAMIC_CLARITY_MAX_FRAME_LEN);
    Check(memcmp(input, before, sizeof(input)) == 0,
          "non-in-place processing leaves input immutable");
    Check(memcmp(output_first, output_second, sizeof(output_first)) == 0,
          "equal initial state and input produce deterministic output");

    first.active_level = 1;
    first.pending_level = 1;
    first.transition_active = 0;
    first.active_state.s1 = NAN;
    (void)DynamicClarity_ProcessFrame(
        &first, input, output_first, DYNAMIC_CLARITY_MAX_FRAME_LEN);
    Check(first.active_level == 0 && first.processing_active == 0 &&
              first.nonfinite_count > 0UL,
          "nonfinite filter state enters safe identity");

    printf("invalid_argument_checks=7 deterministic=1 input_immutable=1 "
           "nonfinite_count=%lu\n", first.nonfinite_count);
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

static void TestPcm16ChainQuantization(void)
{
    QUANTIZATION_METRIC metric_b;
    QUANTIZATION_METRIC metric_c;
    QUANTIZATION_METRIC metric_d;
    QUANTIZATION_METRIC metric_e;
    EQ_STATE equalizer;
    SMART_BASS_STATE smart_b;
    SMART_BASS_STATE smart_c;
    SMART_BASS_STATE smart_e;
    DYNAMIC_CLARITY_STATE clarity_b;
    DYNAMIC_CLARITY_STATE clarity_d;
    DYNAMIC_CLARITY_STATE clarity_e;
    short chain_a[DYNAMIC_CLARITY_MAX_FRAME_LEN];
    short chain_b[DYNAMIC_CLARITY_MAX_FRAME_LEN];
    short chain_c[DYNAMIC_CLARITY_MAX_FRAME_LEN];
    short chain_d[DYNAMIC_CLARITY_MAX_FRAME_LEN];
    short chain_e[DYNAMIC_CLARITY_MAX_FRAME_LEN];
    short input[DYNAMIC_CLARITY_MAX_FRAME_LEN];
    int index;
    int vector;

    memset(&metric_b, 0, sizeof(metric_b));
    memset(&metric_c, 0, sizeof(metric_c));
    memset(&metric_d, 0, sizeof(metric_d));
    memset(&metric_e, 0, sizeof(metric_e));

    for (vector = 0; vector < 4; vector++)
    {
        float clarity_d_s1;
        float clarity_d_s2;
        float clarity_e_s1;
        float clarity_e_s2;
        float smart_c_s1;
        float smart_c_s2;
        float smart_e_s1;
        float smart_e_s2;

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
            FillSine(input, 100.0f, 4125.0f);
        }
        else
        {
            FillMusicLike(input);
        }

        Equalizer_Init(&equalizer);
        Equalizer_ProcessFrame(
            &equalizer, input, chain_a,
            DYNAMIC_CLARITY_MAX_FRAME_LEN);

        memcpy(chain_b, chain_a, sizeof(chain_b));
        SmartBass_Init(&smart_b);
        DynamicClarity_Init(&clarity_b);
        (void)SmartBass_ProcessFrame(
            &smart_b, chain_b, chain_b,
            DYNAMIC_CLARITY_MAX_FRAME_LEN);
        (void)DynamicClarity_ProcessFrame(
            &clarity_b, chain_b, chain_b,
            DYNAMIC_CLARITY_MAX_FRAME_LEN);
        Check(memcmp(chain_a, chain_b, sizeof(chain_a)) == 0,
              "PCM16 chain B is exactly equal to static EQ chain A");

        PrepareSmartBassAudioLevel(&smart_c, 4);
        (void)SmartBass_ProcessFrame(
            &smart_c, chain_a, chain_c,
            DYNAMIC_CLARITY_MAX_FRAME_LEN);

        PrepareDynamicClarityAudioLevel(&clarity_d, 4);
        (void)DynamicClarity_ProcessFrame(
            &clarity_d, chain_a, chain_d,
            DYNAMIC_CLARITY_MAX_FRAME_LEN);

        PrepareSmartBassAudioLevel(&smart_e, 4);
        PrepareDynamicClarityAudioLevel(&clarity_e, 4);
        (void)SmartBass_ProcessFrame(
            &smart_e, chain_a, chain_e,
            DYNAMIC_CLARITY_MAX_FRAME_LEN);
        (void)DynamicClarity_ProcessFrame(
            &clarity_e, chain_e, chain_e,
            DYNAMIC_CLARITY_MAX_FRAME_LEN);

        smart_c_s1 = 0.0f;
        smart_c_s2 = 0.0f;
        clarity_d_s1 = 0.0f;
        clarity_d_s2 = 0.0f;
        smart_e_s1 = 0.0f;
        smart_e_s2 = 0.0f;
        clarity_e_s1 = 0.0f;
        clarity_e_s2 = 0.0f;
        for (index = 0; index < DYNAMIC_CLARITY_MAX_FRAME_LEN; index++)
        {
            float ideal_c;
            float ideal_d;
            float ideal_e;

            ideal_c = ProcessFloatBiquad(
                &smart_c.coefficient_table[4],
                &smart_c_s1, &smart_c_s2, (float)chain_a[index]);
            ideal_d = ProcessFloatBiquad(
                &clarity_d.coefficient_table[4],
                &clarity_d_s1, &clarity_d_s2, (float)chain_a[index]);
            ideal_e = ProcessFloatBiquad(
                &smart_e.coefficient_table[4],
                &smart_e_s1, &smart_e_s2, (float)chain_a[index]);
            ideal_e = ProcessFloatBiquad(
                &clarity_e.coefficient_table[4],
                &clarity_e_s1, &clarity_e_s2, ideal_e);

            UpdateQuantizationMetric(
                &metric_b, chain_b[index], (float)chain_a[index]);
            UpdateQuantizationMetric(&metric_c, chain_c[index], ideal_c);
            UpdateQuantizationMetric(&metric_d, chain_d[index], ideal_d);
            UpdateQuantizationMetric(&metric_e, chain_e[index], ideal_e);
        }
    }

    Check(metric_b.max_error == 0.0,
          "disabled dynamic chain introduces zero quantization error");
    Check(metric_c.max_error <= 0.501,
          "single Smart Bass PCM16 conversion stays within half a sample");
    Check(metric_d.max_error <= 0.501,
          "single Clarity PCM16 conversion stays within half a sample");
    Check(metric_e.max_error <= 1.5,
          "two active PCM16 stages keep accumulated error bounded");
    Check(QuantizationRmsError(&metric_e) <= 0.6,
          "two active PCM16 stages keep RMS error bounded");
    Check((metric_b.clip_count + metric_c.clip_count +
           metric_d.clip_count + metric_e.clip_count) == 0UL,
          "headroom stress vectors do not clip in the A-E digital chain");
    Check(QuantizationDcError(&metric_e) <= 0.1,
          "two active PCM16 stages add negligible DC error");

    printf("quant_vectors=4 quant_chain_b_exact_a=1 "
           "quant_chain_b_max_error=%.6f "
           "quant_chain_c_max_error=%.6f quant_chain_c_rms_error=%.6f "
           "quant_chain_d_max_error=%.6f quant_chain_d_rms_error=%.6f "
           "quant_chain_e_max_error=%.6f quant_chain_e_rms_error=%.6f "
           "quant_chain_e_residual_dbfs=%.6f "
           "quant_chain_e_dc_error=%.6f quant_clip_count=%lu "
           "quant_evidence_label=HOST_DIGITAL_SIMULATION\n",
           metric_b.max_error,
           metric_c.max_error, QuantizationRmsError(&metric_c),
           metric_d.max_error, QuantizationRmsError(&metric_d),
           metric_e.max_error, QuantizationRmsError(&metric_e),
           QuantizationResidualDbfs(&metric_e),
           QuantizationDcError(&metric_e),
           metric_b.clip_count + metric_c.clip_count +
           metric_d.clip_count + metric_e.clip_count);
}

static void TestOffChainRegressions(void)
{
    EQ_STATE eq_reference;
    EQ_STATE eq_dynamic_off;
    SMART_BASS_STATE smart_reference;
    SMART_BASS_STATE smart_chained;
    SMART_BASS_STATE smart_disabled;
    DYNAMIC_CLARITY_STATE clarity_disabled;
    short input[DYNAMIC_CLARITY_MAX_FRAME_LEN];
    short eq_output_reference[DYNAMIC_CLARITY_MAX_FRAME_LEN];
    short eq_output_chained[DYNAMIC_CLARITY_MAX_FRAME_LEN];
    short smart_output_reference[DYNAMIC_CLARITY_MAX_FRAME_LEN];
    short smart_output_chained[DYNAMIC_CLARITY_MAX_FRAME_LEN];
    int frame_index;

    Equalizer_Init(&eq_reference);
    Equalizer_Init(&eq_dynamic_off);
    Equalizer_ApplyPreset(&eq_reference, EQ_PRESET_VOCAL);
    Equalizer_ApplyPreset(&eq_dynamic_off, EQ_PRESET_VOCAL);
    SmartBass_Init(&smart_disabled);
    DynamicClarity_Init(&clarity_disabled);
    for (frame_index = 0; frame_index < 8; frame_index++)
    {
        FillPattern(input, frame_index);
        Equalizer_ProcessFrame(
            &eq_reference, input, eq_output_reference,
            DYNAMIC_CLARITY_MAX_FRAME_LEN);
        Equalizer_ProcessFrame(
            &eq_dynamic_off, input, eq_output_chained,
            DYNAMIC_CLARITY_MAX_FRAME_LEN);
        (void)SmartBass_ProcessFrame(
            &smart_disabled, eq_output_chained, eq_output_chained,
            DYNAMIC_CLARITY_MAX_FRAME_LEN);
        (void)DynamicClarity_ProcessFrame(
            &clarity_disabled, eq_output_chained, eq_output_chained,
            DYNAMIC_CLARITY_MAX_FRAME_LEN);
        Check(memcmp(eq_output_reference, eq_output_chained,
                     sizeof(eq_output_reference)) == 0,
              "both dynamic modules OFF preserve static EQ sample exact");
    }

    SmartBass_Init(&smart_reference);
    SmartBass_Init(&smart_chained);
    (void)SmartBass_SetEnabled(&smart_reference, 1);
    (void)SmartBass_SetEnabled(&smart_chained, 1);
    DriveSmartBassToLevel(&smart_reference, 2);
    DriveSmartBassToLevel(&smart_chained, 2);
    DynamicClarity_Reset(&clarity_disabled);
    for (frame_index = 0; frame_index < 8; frame_index++)
    {
        FillPattern(input, frame_index + 11);
        (void)SmartBass_ProcessFrame(
            &smart_reference, input, smart_output_reference,
            DYNAMIC_CLARITY_MAX_FRAME_LEN);
        (void)SmartBass_ProcessFrame(
            &smart_chained, input, smart_output_chained,
            DYNAMIC_CLARITY_MAX_FRAME_LEN);
        (void)DynamicClarity_ProcessFrame(
            &clarity_disabled,
            smart_output_chained, smart_output_chained,
            DYNAMIC_CLARITY_MAX_FRAME_LEN);
        Check(memcmp(smart_output_reference, smart_output_chained,
                     sizeof(smart_output_reference)) == 0,
              "Dynamic Clarity OFF preserves Smart Bass sample exact");
    }

    Check(smart_disabled.saturation_count == 0UL &&
              clarity_disabled.saturation_count == 0UL,
          "OFF-chain regression introduces no saturation");
    printf("eq_off_chain_exact=1 smart_bass_off_chain_exact=1 "
           "chain_evidence_label=HOST_DIGITAL_SIMULATION\n");
}

int main(void)
{
    TestInitializationIdentityAndPeakingApi();
    TestMaskingMappingStrengthAndAttack();
    TestGatesReleaseDebounceAndDisable();
    TestQueueTransitionAndEpoch();
    TestFrequencyResponse();
    TestNumericalGuardsImmutabilityAndDeterminism();
    TestPcm16ChainQuantization();
    TestOffChainRegressions();

    printf("dynamic_clarity failures=%d\n", failures);
    if (failures == 0)
    {
        printf("dynamic_clarity: PASS\n");
    }
    return (failures == 0) ? 0 : 1;
}
