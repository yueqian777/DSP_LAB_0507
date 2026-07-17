#include <math.h>
#include <stdio.h>
#include <string.h>

#include "user_smart_bass.h"

#define TEST_PI 3.14159265358979323846f

static int failures = 0;
static unsigned long analysis_sequence = 0UL;

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

static void FillPattern(short *frame)
{
    int index;

    for (index = 0; index < SMART_BASS_MAX_FRAME_LEN; index++)
    {
        frame[index] = (short)(((index * 7919) % 60001) - 30000);
    }
}

static void FillSine(short *frame, float frequency_hz, float amplitude)
{
    int index;

    for (index = 0; index < SMART_BASS_MAX_FRAME_LEN; index++)
    {
        float value;

        value = amplitude * sinf(2.0f * TEST_PI * frequency_hz *
                                 (float)index / SMART_BASS_SAMPLE_RATE);
        frame[index] = (short)((value >= 0.0f) ?
                               (value + 0.5f) : (value - 0.5f));
    }
}

static AUDIO_FEATURE_SNAPSHOT MakeSnapshot(float bass_db,
                                           float rms_dbfs,
                                           int valid,
                                           int warmup)
{
    AUDIO_FEATURE_SNAPSHOT snapshot;

    memset(&snapshot, 0, sizeof(snapshot));
    analysis_sequence++;
    snapshot.analysis_count = analysis_sequence;
    snapshot.relative_db[AUDIO_FEATURE_BASS] = bass_db;
    snapshot.rms_dbfs = rms_dbfs;
    snapshot.valid = valid;
    snapshot.warmup_complete = warmup;
    return snapshot;
}

static void SettleTransitions(SMART_BASS_STATE *state)
{
    short frame[SMART_BASS_MAX_FRAME_LEN];
    int guard;

    memset(frame, 0, sizeof(frame));
    guard = 0;
    while ((state->transition_active != 0) && (guard < 64))
    {
        Check(SmartBass_ProcessFrame(
                  state, frame, frame, SMART_BASS_MAX_FRAME_LEN) ==
                  SMART_BASS_RESULT_UPDATED,
              "transition frame processes successfully");
        guard++;
    }
    Check(guard < 64, "transition settles within a bounded number of frames");
}

static void DriveToLevel(SMART_BASS_STATE *state, int requested_level)
{
    int guard;

    guard = 0;
    while ((state->active_level != requested_level) && (guard < 32))
    {
        AUDIO_FEATURE_SNAPSHOT snapshot;
        int before;

        before = state->active_level;
        snapshot = MakeSnapshot(20.0f, -18.0f, 1, 1);
        Check(SmartBass_UpdateFromFeature(state, &snapshot) ==
                  SMART_BASS_RESULT_UPDATED,
              "high-bass feature decision is accepted");
        Check((state->pending_level - before) <= 1,
              "attack never requests more than one adjacent level");
        SettleTransitions(state);
        Check((state->active_level - before) <= 1,
              "one analysis increases at most one applied level");
        guard++;
    }
    Check(state->active_level == requested_level,
          "requested strength ceiling is reached gradually");
}

static float BiquadGainDb(const SMART_BASS_BIQUAD *coefficient,
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
    SMART_BASS_STATE state;
    EQ_STATE eq_state;
    EQ_STATE eq_before;
    EQ_STATE eq_reference;
    EQ_STATE eq_chained;
    EQ_BIQUAD designed;
    short input[SMART_BASS_MAX_FRAME_LEN];
    short output[SMART_BASS_MAX_FRAME_LEN];
    short in_place[SMART_BASS_MAX_FRAME_LEN];
    short reference_output[SMART_BASS_MAX_FRAME_LEN];
    short chained_output[SMART_BASS_MAX_FRAME_LEN];
    int frame_index;

    SmartBass_Init(&state);
    Check(state.initialized == 1, "init marks Smart Bass initialized");
    Check(state.requested_enabled == 0, "init defaults runtime disabled");
    Check(state.strength == SMART_BASS_STRENGTH_MEDIUM,
          "init defaults to medium strength");
    Check(state.active_level == 0 && state.target_level == 0,
          "init starts at identity level");
    Check(state.transition_total == SMART_BASS_TRANSITION_SAMPLES,
          "init selects the exact 80 ms transition length");
    Check(sizeof(state.coefficient_table) ==
              sizeof(SMART_BASS_BIQUAD) * SMART_BASS_LEVEL_COUNT,
          "state owns exactly seven fixed coefficient sets");

    memset(&eq_state, 0x5a, sizeof(eq_state));
    memcpy(&eq_before, &eq_state, sizeof(eq_state));
    {
        unsigned long scans_before;

        scans_before = EQ_DebugHeadroomScanCount;
        Check(Equalizer_DesignRbjLowShelfAt(
                  &designed, 125.0f, -1.5f, 1.0f) == 1,
              "public low-shelf design accepts the Smart Bass contract");
        Check(EQ_DebugHeadroomScanCount == scans_before,
              "public low-shelf design does not run a headroom scan");
    }
    Check(memcmp(&eq_state, &eq_before, sizeof(eq_state)) == 0,
          "public low-shelf design does not modify EQ state");

    FillPattern(input);
    memset(output, 0, sizeof(output));
    Check(SmartBass_ProcessFrame(&state, input, output,
                                 SMART_BASS_MAX_FRAME_LEN) ==
              SMART_BASS_RESULT_UPDATED,
          "disabled identity frame succeeds");
    Check(memcmp(input, output, sizeof(input)) == 0,
          "disabled non-in-place output is sample exact");
    memcpy(in_place, input, sizeof(input));
    Check(SmartBass_ProcessFrame(&state, in_place, in_place,
                                 SMART_BASS_MAX_FRAME_LEN) ==
              SMART_BASS_RESULT_UPDATED,
          "disabled in-place frame succeeds");
    Check(memcmp(input, in_place, sizeof(input)) == 0,
          "disabled in-place output is sample exact");
    Check(state.processing_active == 0,
          "identity path does not report active filtering");

    Check(SmartBass_SetEnabled(&state, 1) == SMART_BASS_RESULT_UPDATED,
          "runtime enable is accepted");
    Check(SmartBass_ProcessFrame(&state, input, output,
                                 SMART_BASS_MAX_FRAME_LEN) ==
              SMART_BASS_RESULT_UPDATED,
          "enabled level-zero frame succeeds");
    Check(memcmp(input, output, sizeof(input)) == 0,
          "enabled level zero remains sample exact");

    (void)SmartBass_SetEnabled(&state, 0);
    Equalizer_Init(&eq_reference);
    Equalizer_Init(&eq_chained);
    Equalizer_ApplyPreset(&eq_reference, EQ_PRESET_BASS_BOOST);
    Equalizer_ApplyPreset(&eq_chained, EQ_PRESET_BASS_BOOST);
    for (frame_index = 0; frame_index < 8; frame_index++)
    {
        FillPattern(input);
        Equalizer_ProcessFrame(&eq_reference, input, reference_output,
                               SMART_BASS_MAX_FRAME_LEN);
        Equalizer_ProcessFrame(&eq_chained, input, chained_output,
                               SMART_BASS_MAX_FRAME_LEN);
        (void)SmartBass_ProcessFrame(&state, chained_output, chained_output,
                                     SMART_BASS_MAX_FRAME_LEN);
        Check(memcmp(reference_output, chained_output,
                     sizeof(reference_output)) == 0,
              "disabled Smart Bass preserves the EQ output sample exactly");
    }
    printf("smart_bass_state_bytes=%u transition_samples=%d "
           "identity_exact=1 eq_off_chain_exact=1\n",
           (unsigned int)sizeof(state), SMART_BASS_TRANSITION_SAMPLES);
}

static void TestMappingStrengthAndAttack(void)
{
    static const float bass_points[5] = { 4.0f, 7.0f, 10.0f, 13.0f, 16.0f };
    static const int expected_medium[5] = { 0, 1, 2, 3, 4 };
    static const int maximum_levels[4] = { 0, 2, 4, 6 };
    SMART_BASS_STATE state;
    int point;
    int strength;

    for (point = 0; point < 5; point++)
    {
        AUDIO_FEATURE_SNAPSHOT snapshot;

        SmartBass_Init(&state);
        (void)SmartBass_SetEnabled(&state, 1);
        snapshot = MakeSnapshot(bass_points[point], -18.0f, 1, 1);
        Check(SmartBass_UpdateFromFeature(&state, &snapshot) ==
                  SMART_BASS_RESULT_UPDATED,
              "mapping feature is accepted");
        Check(state.target_level == expected_medium[point],
              "medium mapping quantizes the 4-16 dB span monotonically");
        Check(SmartBass_GetRequestedGainDb(&state) <= 0.0f,
              "mapping never requests positive gain");
    }

    for (strength = SMART_BASS_STRENGTH_OFF;
         strength <= SMART_BASS_STRENGTH_HIGH; strength++)
    {
        SmartBass_Init(&state);
        Check(SmartBass_SetStrength(&state, strength) >= 0,
              "strength selection is accepted");
        (void)SmartBass_SetEnabled(&state, 1);
        if (maximum_levels[strength] > 0)
        {
            DriveToLevel(&state, maximum_levels[strength]);
        }
        Check(state.active_level <= maximum_levels[strength],
              "applied level respects the strength ceiling");
        Check(SmartBass_GetAppliedGainDb(&state) >=
                  -SMART_BASS_LEVEL_STEP_DB *
                  (float)maximum_levels[strength] - 0.001f,
              "applied attenuation respects the strength ceiling");
    }
    printf("mapping_perturbation_db=1 strength_max_levels=0,2,4,6 "
           "attack_step_max=1\n");
}

static void TestReleaseDebounceAndDisable(void)
{
    SMART_BASS_STATE state;
    AUDIO_FEATURE_SNAPSHOT snapshot;
    short frame[SMART_BASS_MAX_FRAME_LEN];
    short before[SMART_BASS_MAX_FRAME_LEN];
    unsigned long transition_before;
    int level;

    SmartBass_Init(&state);
    (void)SmartBass_SetEnabled(&state, 1);
    DriveToLevel(&state, 4);
    Check(state.active_level == 4, "release test starts at medium maximum");

    snapshot = MakeSnapshot(0.0f, -18.0f, 1, 1);
    (void)SmartBass_UpdateFromFeature(&state, &snapshot);
    Check(state.active_level == 4 && state.transition_active == 0,
          "first release observation only arms confirmation");
    snapshot = MakeSnapshot(0.0f, -18.0f, 1, 1);
    (void)SmartBass_UpdateFromFeature(&state, &snapshot);
    Check(state.pending_level == 3 && state.transition_active == 1,
          "second release observation starts one adjacent transition");
    SettleTransitions(&state);
    Check(state.active_level == 3,
          "confirmed release decreases exactly one level");

    while (state.active_level > 1)
    {
        snapshot = MakeSnapshot(0.0f, -90.0f, 0, 0);
        (void)SmartBass_UpdateFromFeature(&state, &snapshot);
        snapshot = MakeSnapshot(0.0f, -90.0f, 0, 0);
        (void)SmartBass_UpdateFromFeature(&state, &snapshot);
        SettleTransitions(&state);
    }
    Check(state.invalid_release_count > 0UL,
          "invalid and low-level releases are counted");

    transition_before = state.transition_count;
    for (level = 0; level < 8; level++)
    {
        float bass_db;

        bass_db = ((level & 1) == 0) ? 3.9f : 5.6f;
        snapshot = MakeSnapshot(bass_db, -18.0f, 1, 1);
        (void)SmartBass_UpdateFromFeature(&state, &snapshot);
    }
    Check(state.active_level == 1 && state.transition_active == 0,
          "threshold alternation does not chatter the applied level");
    Check(state.transition_count == transition_before,
          "threshold alternation does not start repeated transitions");

    snapshot = MakeSnapshot(20.0f, -18.0f, 1, 1);
    (void)SmartBass_UpdateFromFeature(&state, &snapshot);
    Check(state.transition_active == 1 && state.pending_level == 2,
          "new attack starts before explicit disable");
    Check(SmartBass_SetEnabled(&state, 0) == SMART_BASS_RESULT_UPDATED,
          "explicit disable is accepted during transition");
    Check(state.transition_active == 1 && state.pending_level == 2,
          "disable does not overwrite the active transition");
    SettleTransitions(&state);
    Check(state.active_level == 0 && state.processing_active == 0,
          "disable chains adjacent transitions until identity");

    FillPattern(frame);
    memcpy(before, frame, sizeof(frame));
    (void)SmartBass_ProcessFrame(&state, frame, frame,
                                 SMART_BASS_MAX_FRAME_LEN);
    Check(memcmp(frame, before, sizeof(frame)) == 0,
          "disabled and released state returns to exact identity");
    printf("release_confirmations=2 debounce_transitions=%lu "
           "invalid_release_count=%lu\n",
           state.transition_count - transition_before,
           state.invalid_release_count);
}

static void ReleaseCauseToIdentity(float bass_db, float rms_dbfs,
                                   int valid, int warmup,
                                   int expected_reason)
{
    SMART_BASS_STATE state;
    AUDIO_FEATURE_SNAPSHOT snapshot;
    int guard;

    SmartBass_Init(&state);
    (void)SmartBass_SetEnabled(&state, 1);
    DriveToLevel(&state, 2);
    guard = 0;
    while ((state.active_level > 0) && (guard < 8))
    {
        snapshot = MakeSnapshot(bass_db, rms_dbfs, valid, warmup);
        (void)SmartBass_UpdateFromFeature(&state, &snapshot);
        Check(state.reason == expected_reason,
              "first release confirmation preserves its cause reason");
        snapshot = MakeSnapshot(bass_db, rms_dbfs, valid, warmup);
        (void)SmartBass_UpdateFromFeature(&state, &snapshot);
        Check(state.reason == SMART_BASS_REASON_RELEASING,
              "second release confirmation reports releasing");
        SettleTransitions(&state);
        guard++;
    }
    Check(guard < 8 && state.active_level == 0,
          "release cause returns the applied level to identity");
    Check(state.invalid_release_count > 0UL,
          "invalid, not-warm, or low-level release is counted");
}

static void TestDecisionBoundariesAndReleaseCauses(void)
{
    SMART_BASS_STATE state;
    AUDIO_FEATURE_SNAPSHOT snapshot;
    short frame[SMART_BASS_MAX_FRAME_LEN];
    unsigned long decision_before;
    int frame_index;

    SmartBass_Init(&state);
    (void)SmartBass_SetEnabled(&state, 1);
    snapshot = MakeSnapshot(4.0f, -18.0f, 1, 1);
    (void)SmartBass_UpdateFromFeature(&state, &snapshot);
    Check(state.target_level == 0 &&
              state.reason == SMART_BASS_REASON_BELOW_THRESHOLD,
          "the exact 4 dB boundary requests identity");
    decision_before = state.decision_count;
    Check(SmartBass_UpdateFromFeature(&state, &snapshot) ==
              SMART_BASS_RESULT_NO_CHANGE &&
              state.decision_count == decision_before,
          "an unchanged analysis count cannot repeat a decision");

    snapshot = MakeSnapshot(16.0f, -18.0f, 1, 1);
    (void)SmartBass_UpdateFromFeature(&state, &snapshot);
    Check(state.target_level == 4 &&
              state.reason == SMART_BASS_REASON_EXCESS_BASS,
          "the exact 16 dB boundary requests the medium ceiling");

    SmartBass_Reset(&state);
    (void)SmartBass_SetEnabled(&state, 1);
    snapshot = MakeSnapshot(20.0f, -45.0f, 1, 1);
    (void)SmartBass_UpdateFromFeature(&state, &snapshot);
    Check(state.target_level == 4 &&
              state.reason == SMART_BASS_REASON_EXCESS_BASS,
          "the exact minimum RMS boundary remains eligible");

    SmartBass_Reset(&state);
    (void)SmartBass_SetEnabled(&state, 1);
    snapshot = MakeSnapshot(20.0f, -45.01f, 1, 1);
    (void)SmartBass_UpdateFromFeature(&state, &snapshot);
    Check(state.target_level == 0 &&
              state.reason == SMART_BASS_REASON_LOW_LEVEL,
          "RMS below the minimum boundary requests release");

    ReleaseCauseToIdentity(20.0f, -18.0f, 0, 1,
                           SMART_BASS_REASON_INVALID);
    ReleaseCauseToIdentity(20.0f, -18.0f, 1, 0,
                           SMART_BASS_REASON_NOT_WARM);
    ReleaseCauseToIdentity(20.0f, -90.0f, 1, 1,
                           SMART_BASS_REASON_LOW_LEVEL);

    SmartBass_Init(&state);
    (void)SmartBass_SetStrength(&state, SMART_BASS_STRENGTH_HIGH);
    (void)SmartBass_SetEnabled(&state, 1);
    DriveToLevel(&state, 6);
    (void)SmartBass_SetStrength(&state, SMART_BASS_STRENGTH_LOW);
    Check(state.target_level == 2 && state.force_release != 0,
          "a lower non-OFF strength immediately sets the new ceiling");
    SettleTransitions(&state);
    Check(state.active_level == 2 && state.force_release == 0 &&
              state.processing_active == 1,
          "strength reduction converges smoothly to its bounded ceiling");

    SmartBass_Reset(&state);
    (void)SmartBass_SetStrength(&state, SMART_BASS_STRENGTH_HIGH);
    (void)SmartBass_SetEnabled(&state, 1);
    snapshot = MakeSnapshot(20.0f, -18.0f, 1, 1);
    (void)SmartBass_UpdateFromFeature(&state, &snapshot);
    snapshot = MakeSnapshot(20.0f, -18.0f, 1, 1);
    (void)SmartBass_UpdateFromFeature(&state, &snapshot);
    memset(frame, 0, sizeof(frame));
    for (frame_index = 0; frame_index < 4; frame_index++)
    {
        (void)SmartBass_ProcessFrame(&state, frame, frame,
                                     SMART_BASS_MAX_FRAME_LEN);
    }
    Check(state.transition_active != 0 && state.pending_level == 2,
          "queued attack advances to its next in-flight endpoint");
    snapshot = MakeSnapshot(20.0f, -18.0f, 1, 1);
    (void)SmartBass_UpdateFromFeature(&state, &snapshot);
    Check(state.queued_level_valid != 0 && state.queued_level == 3,
          "high strength queues an endpoint above the LOW ceiling");
    (void)SmartBass_SetStrength(&state, SMART_BASS_STRENGTH_LOW);
    Check(state.target_level == 2 && state.queued_level_valid == 0,
          "strength reduction discards a queued level above its ceiling");
    SettleTransitions(&state);
    Check(state.active_level == 2 && state.processing_active == 1,
          "queued attack cannot escape the reduced strength ceiling");

    snapshot = MakeSnapshot(20.0f, -18.0f, 1, 1);
    SmartBass_Reset(&state);
    (void)SmartBass_SetEnabled(&state, 1);
    (void)SmartBass_UpdateFromFeature(&state, &snapshot);
    decision_before = state.decision_count;
    (void)SmartBass_SetEnabled(&state, 0);
    (void)SmartBass_UpdateFromFeature(&state, &snapshot);
    SmartBass_InvalidateAnalysisEpoch(&state);
    Check(state.latest_analysis_count == ~0UL,
          "a repeated Analyzer fault invalidates the prior count epoch");
    Check(SmartBass_SetEnabled(&state, 0) ==
              SMART_BASS_RESULT_NO_CHANGE,
          "epoch invalidation works while Smart Bass is already disabled");
    (void)SmartBass_SetEnabled(&state, 1);
    Check(SmartBass_UpdateFromFeature(&state, &snapshot) ==
              SMART_BASS_RESULT_UPDATED &&
              state.decision_count == (decision_before + 1UL),
          "the first snapshot after reset is accepted despite count reuse");

    SmartBass_Reset(&state);
    (void)SmartBass_SetEnabled(&state, 1);
    DriveToLevel(&state, 3);
    snapshot = MakeSnapshot(0.0f, -18.0f, 1, 1);
    (void)SmartBass_UpdateFromFeature(&state, &snapshot);
    snapshot = MakeSnapshot(0.0f, -18.0f, 1, 1);
    (void)SmartBass_UpdateFromFeature(&state, &snapshot);
    Check(state.transition_active != 0 && state.pending_level == 2,
          "confirmed release starts the first adjacent transition");
    snapshot = MakeSnapshot(0.0f, -18.0f, 1, 1);
    (void)SmartBass_UpdateFromFeature(&state, &snapshot);
    snapshot = MakeSnapshot(0.0f, -18.0f, 1, 1);
    (void)SmartBass_UpdateFromFeature(&state, &snapshot);
    Check(state.queued_level_valid != 0 && state.queued_level == 1,
          "two more confirmations queue the next adjacent release");
    snapshot = MakeSnapshot(0.0f, -18.0f, 1, 1);
    (void)SmartBass_UpdateFromFeature(&state, &snapshot);
    Check(state.queued_level_valid != 0 && state.queued_level == 1,
          "a repeated release observation preserves the confirmed queue");
    SettleTransitions(&state);
    Check(state.active_level == 1,
          "the preserved queued release applies after the first endpoint");
    printf("decision_duplicate_ignored=1 release_causes_to_zero=3 "
           "strength_ceiling_converged=1 strength_queued_clamped=1 "
           "sequence_epoch_reset=1 queued_release_preserved=1\n");
}

static void TestTransitionLatestWinsAndProgress(void)
{
    SMART_BASS_STATE state;
    AUDIO_FEATURE_SNAPSHOT snapshot;
    short frame[SMART_BASS_MAX_FRAME_LEN];
    short impulse_input[SMART_BASS_MAX_FRAME_LEN];
    short impulse_before[SMART_BASS_MAX_FRAME_LEN];
    short impulse_output[SMART_BASS_MAX_FRAME_LEN];
    float previous_progress;
    unsigned long transition_saturation;
    int frame_index;

    SmartBass_Init(&state);
    (void)SmartBass_SetStrength(&state, SMART_BASS_STRENGTH_HIGH);
    (void)SmartBass_SetEnabled(&state, 1);
    snapshot = MakeSnapshot(20.0f, -18.0f, 1, 1);
    (void)SmartBass_UpdateFromFeature(&state, &snapshot);
    Check(state.active_level == 0 && state.pending_level == 1,
          "first high request starts only zero-to-one transition");
    snapshot = MakeSnapshot(20.0f, -18.0f, 1, 1);
    (void)SmartBass_UpdateFromFeature(&state, &snapshot);
    Check(state.pending_level == 1 && state.queued_level_valid == 1 &&
              state.queued_level == 2,
          "new request queues latest adjacent level without overwrite");
    SettleTransitions(&state);
    Check(state.active_level == 2 && state.transition_active == 0,
          "queued latest request starts after and applies beyond the endpoint");

    SmartBass_Reset(&state);
    (void)SmartBass_SetStrength(&state, SMART_BASS_STRENGTH_HIGH);
    (void)SmartBass_SetEnabled(&state, 1);
    snapshot = MakeSnapshot(20.0f, -18.0f, 1, 1);
    (void)SmartBass_UpdateFromFeature(&state, &snapshot);
    snapshot = MakeSnapshot(20.0f, -18.0f, 1, 1);
    (void)SmartBass_UpdateFromFeature(&state, &snapshot);

    snapshot = MakeSnapshot(0.0f, -18.0f, 1, 1);
    (void)SmartBass_UpdateFromFeature(&state, &snapshot);
    Check(state.pending_level == 1 && state.queued_level_valid == 0,
          "latest lower target cancels stale queued attack");

    FillSine(frame, 97.65625f, 24000.0f);
    previous_progress = 0.0f;
    for (frame_index = 0; frame_index < 4; frame_index++)
    {
        float progress;

        (void)SmartBass_ProcessFrame(&state, frame, frame,
                                     SMART_BASS_MAX_FRAME_LEN);
        progress = SmartBass_GetTransitionProgress(&state);
        Check(FloatFinite(progress), "transition progress remains finite");
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

    snapshot = MakeSnapshot(0.0f, -18.0f, 1, 1);
    (void)SmartBass_UpdateFromFeature(&state, &snapshot);
    Check(state.pending_level == 0 && state.transition_active == 1,
          "second lower observation starts one-step release");
    SettleTransitions(&state);
    Check(state.active_level == 0, "latest lower target eventually applies");

    SmartBass_Reset(&state);
    (void)SmartBass_SetEnabled(&state, 1);
    snapshot = MakeSnapshot(20.0f, -18.0f, 1, 1);
    (void)SmartBass_UpdateFromFeature(&state, &snapshot);
    for (frame_index = 0; frame_index < 4; frame_index++)
    {
        memset(impulse_input, 0, sizeof(impulse_input));
        if (frame_index == 0)
        {
            impulse_input[0] = 29490;
        }
        memcpy(impulse_before, impulse_input, sizeof(impulse_input));
        (void)SmartBass_ProcessFrame(
            &state, impulse_input, impulse_output,
            SMART_BASS_MAX_FRAME_LEN);
        Check(memcmp(impulse_input, impulse_before,
                     sizeof(impulse_input)) == 0,
              "0.9FS impulse input remains immutable");
    }
    Check(state.saturation_count == 0UL,
          "0.9FS impulse transition does not saturate");
    printf("latest_wins=1 queued_applied=1 transition_progress_monotonic=1 "
           "transition_saturation=%lu impulse_09fs_saturation=%lu\n",
           transition_saturation, state.saturation_count);
}

static void TestFrequencyResponse(void)
{
    static const float frequencies[5] =
        { 50.0f, 125.0f, 500.0f, 1000.0f, 8000.0f };
    SMART_BASS_STATE state;
    float level6_50_db;
    float high_abs_max;
    float positive_max;
    int frequency;
    int level;

    SmartBass_Init(&state);
    high_abs_max = 0.0f;
    positive_max = -1000.0f;
    for (frequency = 0; frequency < 5; frequency++)
    {
        float previous_gain;

        previous_gain = 0.0f;
        for (level = 0; level < SMART_BASS_LEVEL_COUNT; level++)
        {
            float gain_db;

            gain_db = BiquadGainDb(
                &state.coefficient_table[level], frequencies[frequency]);
            Check(FloatFinite(gain_db), "fixed-level response is finite");
            if (frequency <= 1 && level > 0)
            {
                Check(gain_db <= previous_gain + 0.002f,
                      "low-frequency attenuation grows monotonically");
            }
            if (gain_db > positive_max)
            {
                positive_max = gain_db;
            }
            if ((frequency == 4) && (fabsf(gain_db) > high_abs_max))
            {
                high_abs_max = fabsf(gain_db);
            }
            previous_gain = gain_db;
        }
    }
    level6_50_db = BiquadGainDb(
        &state.coefficient_table[6], 50.0f);
    Check(fabsf(level6_50_db + 3.0f) < 0.25f,
          "level six is approximately minus 3 dB at 50 Hz");
    Check(high_abs_max < 0.01f,
          "8 kHz response remains effectively zero dB");
    Check(positive_max < 0.002f,
          "all checked fixed-level responses avoid positive gain");
    printf("level6_50hz_db=%.5f high_8khz_abs_max_db=%.6f "
           "response_positive_max_db=%.6f\n",
           level6_50_db, high_abs_max, positive_max);
}

static void TestNumericalGuardsImmutabilityAndDeterminism(void)
{
    SMART_BASS_STATE first;
    SMART_BASS_STATE second;
    AUDIO_FEATURE_SNAPSHOT snapshot;
    short input[SMART_BASS_MAX_FRAME_LEN];
    short before[SMART_BASS_MAX_FRAME_LEN];
    short output_first[SMART_BASS_MAX_FRAME_LEN];
    short output_second[SMART_BASS_MAX_FRAME_LEN];

    SmartBass_Init(&first);
    SmartBass_Init(&second);
    Check(SmartBass_SetStrength(&first, -99) == SMART_BASS_RESULT_UPDATED &&
              first.strength == SMART_BASS_STRENGTH_OFF,
          "negative strength normalizes to OFF");
    Check(SmartBass_SetStrength(&first, 99) == SMART_BASS_RESULT_UPDATED &&
              first.strength == SMART_BASS_STRENGTH_HIGH,
          "large strength normalizes to HIGH");
    Check(SmartBass_ProcessFrame(0, input, output_first, 1) ==
              SMART_BASS_RESULT_ERROR,
          "null state is rejected");
    Check(SmartBass_ProcessFrame(&first, 0, output_first, 1) ==
              SMART_BASS_RESULT_ERROR,
          "null input is rejected");
    Check(SmartBass_ProcessFrame(&first, input, 0, 1) ==
              SMART_BASS_RESULT_ERROR,
          "null output is rejected");
    Check(SmartBass_ProcessFrame(&first, input, output_first, 0) ==
              SMART_BASS_RESULT_ERROR,
          "zero sample count is rejected");
    Check(SmartBass_ProcessFrame(&first, input, output_first,
                                 SMART_BASS_MAX_FRAME_LEN + 1) ==
              SMART_BASS_RESULT_ERROR,
          "oversized sample count is rejected");
    Check(SmartBass_UpdateFromFeature(0, &snapshot) ==
              SMART_BASS_RESULT_ERROR,
          "null feature state is rejected");
    Check(SmartBass_UpdateFromFeature(&first, 0) ==
              SMART_BASS_RESULT_ERROR,
          "null feature snapshot is rejected");

    SmartBass_Reset(&first);
    (void)SmartBass_SetEnabled(&first, 1);
    snapshot = MakeSnapshot(NAN, -18.0f, 1, 1);
    (void)SmartBass_UpdateFromFeature(&first, &snapshot);
    Check(first.target_level == 0 && first.nonfinite_count == 1UL,
          "nonfinite feature requests bounded identity release");
    Check(FloatFinite(first.latest_bass_relative_db) &&
              FloatFinite(first.latest_rms_dbfs),
          "nonfinite feature is not published into runtime state");

    SmartBass_Reset(&first);
    SmartBass_Reset(&second);
    (void)SmartBass_SetEnabled(&first, 1);
    (void)SmartBass_SetEnabled(&second, 1);
    snapshot = MakeSnapshot(20.0f, -18.0f, 1, 1);
    (void)SmartBass_UpdateFromFeature(&first, &snapshot);
    second.latest_analysis_count = snapshot.analysis_count - 1UL;
    (void)SmartBass_UpdateFromFeature(&second, &snapshot);
    FillPattern(input);
    memcpy(before, input, sizeof(input));
    (void)SmartBass_ProcessFrame(&first, input, output_first,
                                 SMART_BASS_MAX_FRAME_LEN);
    (void)SmartBass_ProcessFrame(&second, input, output_second,
                                 SMART_BASS_MAX_FRAME_LEN);
    Check(memcmp(input, before, sizeof(input)) == 0,
          "non-in-place processing leaves input immutable");
    Check(memcmp(output_first, output_second, sizeof(output_first)) == 0,
          "equal initial state and input produce deterministic output");

    first.active_level = 1;
    first.pending_level = 1;
    first.transition_active = 0;
    first.active_state.s1 = NAN;
    (void)SmartBass_ProcessFrame(&first, input, output_first,
                                 SMART_BASS_MAX_FRAME_LEN);
    Check(first.active_level == 0 && first.processing_active == 0 &&
              first.nonfinite_count > 0UL,
          "nonfinite filter state enters safe identity");
    printf("invalid_argument_checks=7 deterministic=1 input_immutable=1 "
           "nonfinite_count=%lu\n", first.nonfinite_count);
}

int main(void)
{
    TestInitializationAndIdentity();
    TestMappingStrengthAndAttack();
    TestReleaseDebounceAndDisable();
    TestDecisionBoundariesAndReleaseCauses();
    TestTransitionLatestWinsAndProgress();
    TestFrequencyResponse();
    TestNumericalGuardsImmutabilityAndDeterminism();

    printf("smart_bass failures=%d\n", failures);
    if (failures == 0)
    {
        printf("smart_bass: PASS\n");
    }
    return (failures == 0) ? 0 : 1;
}
