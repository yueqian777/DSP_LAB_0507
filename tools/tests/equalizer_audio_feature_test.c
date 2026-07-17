#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "user_audio_feature_analyzer.h"

#define TEST_PI 3.14159265358979323846f
#define TEST_POWER_EPSILON 1.0e-30f
#define TEST_NOISE_ANALYSES 64

static int failures = 0;
static int input_immutable_checks = 0;
static int invalid_call_checks = 0;
static int deterministic_snapshot_checks = 0;

static void Check(int condition, const char *message)
{
    if (!condition)
    {
        printf("FAIL: %s\n", message);
        failures++;
    }
}

static short FloatToPcm(float value)
{
    float scaled;
    int rounded;

    if (value > 0.999969f)
    {
        value = 0.999969f;
    }
    if (value < -1.0f)
    {
        value = -1.0f;
    }
    scaled = value * 32768.0f;
    rounded = (int)(scaled + ((scaled >= 0.0f) ? 0.5f : -0.5f));
    if (rounded > 32767)
    {
        rounded = 32767;
    }
    if (rounded < -32768)
    {
        rounded = -32768;
    }
    return (short)rounded;
}

static float PeakFromDbfs(float dbfs)
{
    return powf(10.0f, dbfs / 20.0f);
}

static void FillSine(short *frame, int bin, float peak)
{
    int index;

    for (index = 0; index < AUDIO_FEATURE_FRAME_LEN; index++)
    {
        float phase;

        phase = (2.0f * TEST_PI * (float)bin * (float)index) /
                (float)AUDIO_FEATURE_FFT_SIZE;
        frame[index] = FloatToPcm(peak * sinf(phase));
    }
}

static void FillFourTone(short *frame, const float *peaks)
{
    static const int bins[AUDIO_FEATURE_NUM_BANDS] = { 2, 8, 40, 164 };
    int index;
    int band;

    for (index = 0; index < AUDIO_FEATURE_FRAME_LEN; index++)
    {
        float value;

        value = 0.0f;
        for (band = 0; band < AUDIO_FEATURE_NUM_BANDS; band++)
        {
            float phase;

            phase = (2.0f * TEST_PI * (float)bins[band] * (float)index) /
                    (float)AUDIO_FEATURE_FFT_SIZE;
            value += peaks[band] * sinf(phase);
        }
        frame[index] = FloatToPcm(value);
    }
}

static uint32_t NextRandom(uint32_t *state)
{
    uint32_t value;

    value = *state;
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    *state = value;
    return value;
}

static void FillWhiteNoise(short *frame, uint32_t *seed, float peak)
{
    int index;

    for (index = 0; index < AUDIO_FEATURE_FRAME_LEN; index++)
    {
        int value;
        float normalized;

        value = (int)((NextRandom(seed) >> 16) & 0xffffU) - 32768;
        normalized = (float)value / 32768.0f;
        frame[index] = FloatToPcm(peak * normalized);
    }
}

static int SnapshotIsFinite(const AUDIO_FEATURE_SNAPSHOT *snapshot)
{
    int band;

    if ((!isfinite(snapshot->peak_dbfs)) ||
        (!isfinite(snapshot->rms_dbfs)) ||
        (!isfinite(snapshot->global_density)))
    {
        return 0;
    }
    for (band = 0; band < AUDIO_FEATURE_NUM_BANDS; band++)
    {
        if ((!isfinite(snapshot->raw_density[band])) ||
            (!isfinite(snapshot->smoothed_density[band])) ||
            (!isfinite(snapshot->relative_db[band])))
        {
            return 0;
        }
    }
    return 1;
}

static int DominantBand(const float *density)
{
    int band;
    int dominant;

    dominant = 0;
    for (band = 1; band < AUDIO_FEATURE_NUM_BANDS; band++)
    {
        if (density[band] > density[dominant])
        {
            dominant = band;
        }
    }
    return dominant;
}

static float DominanceMarginDb(const float *density, int expected_band)
{
    int band;
    float strongest_other;

    strongest_other = 0.0f;
    for (band = 0; band < AUDIO_FEATURE_NUM_BANDS; band++)
    {
        if ((band != expected_band) && (density[band] > strongest_other))
        {
            strongest_other = density[band];
        }
    }
    return 10.0f * log10f(
        (density[expected_band] + TEST_POWER_EPSILON) /
        (strongest_other + TEST_POWER_EPSILON));
}

static int AnalyzeFresh(const short *frame, AUDIO_FEATURE_SNAPSHOT *snapshot)
{
    AUDIO_FEATURE_ANALYZER state;
    int result;

    AudioFeatureAnalyzer_Init(&state);
    Check(AudioFeatureAnalyzer_SetPeriod(&state, 1U) == 1,
          "fresh analyzer accepts period one");
    Check(AudioFeatureAnalyzer_SetSmoothing(&state, 0.0f, 0.0f) == 1,
          "fresh analyzer accepts unsmoothed test mode");
    result = AudioFeatureAnalyzer_ProcessFrame(
        &state, frame, AUDIO_FEATURE_FRAME_LEN);
    AudioFeatureAnalyzer_GetSnapshot(&state, snapshot);
    return result;
}

static void TestSilenceFiniteInvalidAndInputImmutable(void)
{
    AUDIO_FEATURE_ANALYZER state;
    AUDIO_FEATURE_SNAPSHOT snapshot;
    short frame[AUDIO_FEATURE_FRAME_LEN];
    short before[AUDIO_FEATURE_FRAME_LEN];
    int band;
    int result;

    memset(frame, 0, sizeof(frame));
    memcpy(before, frame, sizeof(frame));
    AudioFeatureAnalyzer_Init(&state);
    Check(state.initialized == 1, "init marks analyzer initialized");
    Check(state.period_frames == AUDIO_FEATURE_DEFAULT_PERIOD,
          "init selects the default cadence");
    Check(AudioFeatureAnalyzer_SetPeriod(&state, 1U) == 1,
          "silence test accepts period one");
    result = AudioFeatureAnalyzer_ProcessFrame(
        &state, frame, AUDIO_FEATURE_FRAME_LEN);
    AudioFeatureAnalyzer_GetSnapshot(&state, &snapshot);

    Check(result == 1, "silence reaches an analysis boundary");
    Check(snapshot.valid == 0, "silence is explicitly invalid");
    Check(snapshot.warmup_complete == 0,
          "silence does not complete valid warmup");
    Check(snapshot.analysis_count == 1UL,
          "silence still counts as a bounded analysis");
    Check(snapshot.valid_analysis_count == 0UL,
          "silence does not increment valid analysis count");
    Check(SnapshotIsFinite(&snapshot), "silence snapshot is entirely finite");
    for (band = 0; band < AUDIO_FEATURE_NUM_BANDS; band++)
    {
        Check(snapshot.raw_density[band] == 0.0f,
              "silence raw density is zero");
        Check(snapshot.smoothed_density[band] == 0.0f,
              "silence smoothed density is zero");
    }
    Check(memcmp(frame, before, sizeof(frame)) == 0,
          "silence input remains immutable");
    input_immutable_checks++;
    printf("silence_valid=%d silence_rms_dbfs=%.3f silence_finite=1\n",
           snapshot.valid, snapshot.rms_dbfs);
}

static void TestToneDominanceAndBassBoost(void)
{
    static const int bins[AUDIO_FEATURE_NUM_BANDS] = { 2, 8, 40, 164 };
    static const char *names[AUDIO_FEATURE_NUM_BANDS] =
        { "bass", "mud", "presence", "brightness" };
    AUDIO_FEATURE_SNAPSHOT snapshot;
    short frame[AUDIO_FEATURE_FRAME_LEN];
    short before[AUDIO_FEATURE_FRAME_LEN];
    float minimum_margin;
    int band;

    minimum_margin = 1000.0f;
    for (band = 0; band < AUDIO_FEATURE_NUM_BANDS; band++)
    {
        float frequency_hz;
        float margin_db;

        FillSine(frame, bins[band], PeakFromDbfs(-18.0f));
        memcpy(before, frame, sizeof(frame));
        Check(AnalyzeFresh(frame, &snapshot) == 1,
              "bin-centered tone is analyzed");
        Check(snapshot.valid == 1, "bin-centered tone is valid");
        Check(DominantBand(snapshot.raw_density) == band,
              "bin-centered tone selects its contract band");
        margin_db = DominanceMarginDb(snapshot.raw_density, band);
        Check(margin_db >= 12.0f,
              "bin-centered tone has at least 12 dB dominance");
        if (margin_db < minimum_margin)
        {
            minimum_margin = margin_db;
        }
        Check(memcmp(frame, before, sizeof(frame)) == 0,
              "tone input remains immutable");
        input_immutable_checks++;
        frequency_hz = ((float)bins[band] * AUDIO_FEATURE_SAMPLE_RATE) /
                       (float)AUDIO_FEATURE_FFT_SIZE;
        printf("tone_%s_hz=%.3f tone_%s_margin_db=%.3f\n",
               names[band], frequency_hz, names[band], margin_db);
    }

    {
        float peaks[AUDIO_FEATURE_NUM_BANDS];
        float boost_db;
        float margin_db;

        peaks[AUDIO_FEATURE_BASS] = 0.16f;
        peaks[AUDIO_FEATURE_MUD] = 0.04f;
        peaks[AUDIO_FEATURE_PRESENCE] = 0.04f;
        peaks[AUDIO_FEATURE_BRIGHTNESS] = 0.04f;
        FillFourTone(frame, peaks);
        Check(AnalyzeFresh(frame, &snapshot) == 1,
              "bass-boosted four-tone frame is analyzed");
        boost_db = 20.0f * log10f(
            peaks[AUDIO_FEATURE_BASS] / peaks[AUDIO_FEATURE_MUD]);
        margin_db = DominanceMarginDb(snapshot.raw_density,
                                      AUDIO_FEATURE_BASS);
        Check(fabsf(boost_db - 12.0412f) < 0.01f,
              "four-tone bass stimulus is boosted by 12 dB");
        Check(DominantBand(snapshot.raw_density) == AUDIO_FEATURE_BASS,
              "12 dB bass boost is dominant");
        Check(margin_db >= 12.0f,
              "12 dB bass boost clears the dominance threshold");
        printf("bass_boost_db=%.3f bass_margin_db=%.3f "
               "tone_dominance_min_db=%.3f\n",
               boost_db, margin_db, minimum_margin);
    }
}

static void TestRmsDelta(void)
{
    AUDIO_FEATURE_SNAPSHOT loud;
    AUDIO_FEATURE_SNAPSHOT quiet;
    short frame[AUDIO_FEATURE_FRAME_LEN];
    float delta_db;

    FillSine(frame, 41, PeakFromDbfs(-12.0f));
    Check(AnalyzeFresh(frame, &loud) == 1, "minus 12 dBFS tone is analyzed");
    FillSine(frame, 41, PeakFromDbfs(-18.0f));
    Check(AnalyzeFresh(frame, &quiet) == 1,
          "minus 18 dBFS tone is analyzed");
    delta_db = loud.rms_dbfs - quiet.rms_dbfs;
    Check(fabsf(delta_db - 6.0206f) <= 0.15f,
          "minus 12/minus 18 RMS delta is 6.02 dB within tolerance");
    printf("rms_minus12_dbfs=%.3f rms_minus18_dbfs=%.3f "
           "rms_delta_db=%.3f rms_delta_error_db=%.3f\n",
           loud.rms_dbfs, quiet.rms_dbfs, delta_db,
           fabsf(delta_db - 6.0206f));
}

static void TestRelativeAmplitudeInvariance(void)
{
    AUDIO_FEATURE_SNAPSHOT high;
    AUDIO_FEATURE_SNAPSHOT low;
    short frame[AUDIO_FEATURE_FRAME_LEN];
    float high_peaks[AUDIO_FEATURE_NUM_BANDS];
    float low_peaks[AUDIO_FEATURE_NUM_BANDS];
    float maximum_delta;
    int band;

    for (band = 0; band < AUDIO_FEATURE_NUM_BANDS; band++)
    {
        high_peaks[band] = 0.10f;
        low_peaks[band] = 0.025f;
    }
    FillFourTone(frame, high_peaks);
    Check(AnalyzeFresh(frame, &high) == 1,
          "high-level four-tone frame is analyzed");
    FillFourTone(frame, low_peaks);
    Check(AnalyzeFresh(frame, &low) == 1,
          "low-level four-tone frame is analyzed");

    maximum_delta = 0.0f;
    for (band = 0; band < AUDIO_FEATURE_NUM_BANDS; band++)
    {
        float delta;

        delta = fabsf(high.relative_db[band] - low.relative_db[band]);
        if (delta > maximum_delta)
        {
            maximum_delta = delta;
        }
    }
    Check(maximum_delta < 0.25f,
          "relative band levels are amplitude invariant below 0.25 dB");
    printf("relative_invariance_max_db=%.4f "
           "relative_invariance_limit_db=0.2500\n", maximum_delta);
}

static void TestSeededNoiseFairnessAndDeterminism(void)
{
    AUDIO_FEATURE_ANALYZER first;
    AUDIO_FEATURE_ANALYZER second;
    AUDIO_FEATURE_SNAPSHOT first_snapshot;
    AUDIO_FEATURE_SNAPSHOT second_snapshot;
    AUDIO_FEATURE_SNAPSHOT repeated_snapshot;
    short frame[AUDIO_FEATURE_FRAME_LEN];
    short before[AUDIO_FEATURE_FRAME_LEN];
    double density_sum[AUDIO_FEATURE_NUM_BANDS];
    uint32_t seed;
    float minimum_db;
    float maximum_db;
    float spread_db;
    int analysis;
    int band;

    AudioFeatureAnalyzer_Init(&first);
    AudioFeatureAnalyzer_Init(&second);
    Check(AudioFeatureAnalyzer_SetPeriod(&first, 1U) == 1,
          "first noise analyzer accepts period one");
    Check(AudioFeatureAnalyzer_SetPeriod(&second, 1U) == 1,
          "second noise analyzer accepts period one");
    Check(AudioFeatureAnalyzer_SetSmoothing(&first, 0.0f, 0.0f) == 1,
          "first noise analyzer accepts direct smoothing");
    Check(AudioFeatureAnalyzer_SetSmoothing(&second, 0.0f, 0.0f) == 1,
          "second noise analyzer accepts direct smoothing");
    for (band = 0; band < AUDIO_FEATURE_NUM_BANDS; band++)
    {
        density_sum[band] = 0.0;
    }

    seed = UINT32_C(0x6d2b79f5);
    for (analysis = 0; analysis < TEST_NOISE_ANALYSES; analysis++)
    {
        FillWhiteNoise(frame, &seed, 0.50f);
        memcpy(before, frame, sizeof(frame));
        Check(AudioFeatureAnalyzer_ProcessFrame(
                  &first, frame, AUDIO_FEATURE_FRAME_LEN) == 1,
              "first seeded noise frame is analyzed");
        Check(AudioFeatureAnalyzer_ProcessFrame(
                  &second, frame, AUDIO_FEATURE_FRAME_LEN) == 1,
              "second seeded noise frame is analyzed");
        AudioFeatureAnalyzer_GetSnapshot(&first, &first_snapshot);
        AudioFeatureAnalyzer_GetSnapshot(&second, &second_snapshot);
        Check(memcmp(&first_snapshot, &second_snapshot,
                     sizeof(first_snapshot)) == 0,
              "equal seeded histories produce byte-identical snapshots");
        deterministic_snapshot_checks++;
        Check(memcmp(frame, before, sizeof(frame)) == 0,
              "seeded noise input remains immutable");
        input_immutable_checks++;
        Check(first_snapshot.valid == 1,
              "seeded white noise analysis is valid");
        Check(SnapshotIsFinite(&first_snapshot),
              "seeded white noise snapshot is finite");
        for (band = 0; band < AUDIO_FEATURE_NUM_BANDS; band++)
        {
            density_sum[band] += first_snapshot.raw_density[band];
        }
    }

    memset(&repeated_snapshot, 0xa5, sizeof(repeated_snapshot));
    AudioFeatureAnalyzer_GetSnapshot(&first, &repeated_snapshot);
    Check(memcmp(&first_snapshot, &repeated_snapshot,
                 sizeof(first_snapshot)) == 0,
          "repeated snapshot reads are byte deterministic");
    deterministic_snapshot_checks++;

    minimum_db = 1000.0f;
    maximum_db = -1000.0f;
    for (band = 0; band < AUDIO_FEATURE_NUM_BANDS; band++)
    {
        float average_density;
        float density_db;

        average_density = (float)(density_sum[band] /
                                  (double)TEST_NOISE_ANALYSES);
        Check(isfinite(average_density) && (average_density > 0.0f),
              "average seeded noise density is finite and positive");
        density_db = 10.0f * log10f(average_density);
        if (density_db < minimum_db)
        {
            minimum_db = density_db;
        }
        if (density_db > maximum_db)
        {
            maximum_db = density_db;
        }
        printf("noise_band_%d_density_db=%.3f\n", band, density_db);
    }
    spread_db = maximum_db - minimum_db;
    Check(first_snapshot.analysis_count >= 64UL,
          "seeded noise performs at least 64 analyses");
    Check(first_snapshot.analysis_count == TEST_NOISE_ANALYSES,
          "seeded noise analysis count is exact");
    Check(first_snapshot.valid_analysis_count == TEST_NOISE_ANALYSES,
          "all seeded noise analyses are valid");
    Check(first_snapshot.warmup_complete == 1,
          "seeded noise completes analyzer warmup");
    Check(spread_db < 1.50f,
          "white-noise density normalization is fair across bands");
    printf("noise_analyses=%lu noise_density_spread_db=%.3f "
           "noise_density_limit_db=1.500 deterministic_snapshots=%d\n",
           first_snapshot.analysis_count, spread_db,
           deterministic_snapshot_checks);
}

static void TestCadencePeriodEight(void)
{
    AUDIO_FEATURE_ANALYZER state;
    AUDIO_FEATURE_SNAPSHOT snapshot;
    short frame[AUDIO_FEATURE_FRAME_LEN];
    int frame_index;
    int update_count;

    FillSine(frame, 41, PeakFromDbfs(-18.0f));
    AudioFeatureAnalyzer_Init(&state);
    update_count = 0;
    for (frame_index = 1; frame_index <= 80; frame_index++)
    {
        int result;
        int expected_update;

        result = AudioFeatureAnalyzer_ProcessFrame(
            &state, frame, AUDIO_FEATURE_FRAME_LEN);
        expected_update = ((frame_index % 8) == 0) ? 1 : 0;
        Check(result == expected_update,
              "period-eight return value occurs only on cadence boundary");
        if (result == 1)
        {
            update_count++;
        }
    }
    AudioFeatureAnalyzer_GetSnapshot(&state, &snapshot);
    Check(update_count == 10, "period eight produces 10 updates in 80 frames");
    Check(snapshot.input_frame_count == 80UL,
          "cadence counts all 80 input frames");
    Check(snapshot.analysis_count == 10UL,
          "cadence snapshot counts 10 analyses");
    Check(snapshot.skipped_frame_count == 70UL,
          "cadence snapshot counts 70 skipped frames");
    Check(snapshot.period_frames == 8U,
          "cadence snapshot reports period eight");
    printf("cadence_updates=%d cadence_frames=80 cadence_period=%u "
           "cadence_skipped=%lu\n",
           update_count, snapshot.period_frames,
           snapshot.skipped_frame_count);
}

static void TestSplitObserveAndAnalyze(void)
{
    AUDIO_FEATURE_ANALYZER state;
    AUDIO_FEATURE_SNAPSHOT snapshot;
    short frame[AUDIO_FEATURE_FRAME_LEN];
    int frame_index;
    int due_count;
    int result;

    FillSine(frame, 40, PeakFromDbfs(-12.0f));
    AudioFeatureAnalyzer_Init(&state);
    Check(AudioFeatureAnalyzer_SetPeriod(&state, 8U) == 1,
          "split analyzer accepts period eight");
    due_count = 0;
    for (frame_index = 0; frame_index < 80; frame_index++)
    {
        result = AudioFeatureAnalyzer_ObserveFrame(&state);
        Check((result == 0) || (result == 1),
              "split cadence observation is bounded");
        if (result == 1)
        {
            due_count++;
            Check(AudioFeatureAnalyzer_AnalyzeObservedFrame(
                      &state, frame, AUDIO_FEATURE_FRAME_LEN) == 1,
                  "split due frame performs one heavy analysis");
        }
    }
    AudioFeatureAnalyzer_GetSnapshot(&state, &snapshot);
    Check(due_count == 10, "split cadence exposes ten due frames");
    Check(snapshot.input_frame_count == 80UL,
          "split observation counts every actual frame");
    Check(snapshot.analysis_count == 10UL,
          "split heavy path counts only completed FFT analyses");
    Check(snapshot.skipped_frame_count == 70UL,
          "split cadence counts seventy skipped frames");

    Check(AudioFeatureAnalyzer_AnalyzeObservedFrame(
              &state, frame, AUDIO_FEATURE_FRAME_LEN) == 1,
          "standalone heavy path accepts a captured due snapshot");
    AudioFeatureAnalyzer_GetSnapshot(&state, &snapshot);
    Check(snapshot.input_frame_count == 80UL,
          "heavy analysis does not invent a new ADC frame");
    Check(snapshot.analysis_count == 11UL,
          "standalone heavy analysis increments only analysis count");
    printf("split_due=%d split_analyses=%lu split_frames=%lu "
           "split_skipped=%lu\n",
           due_count, snapshot.analysis_count - 1UL,
           snapshot.input_frame_count, snapshot.skipped_frame_count);
}

static void TestAttackFasterThanRelease(void)
{
    AUDIO_FEATURE_ANALYZER attack_state;
    AUDIO_FEATURE_ANALYZER release_state;
    AUDIO_FEATURE_SNAPSHOT before_attack;
    AUDIO_FEATURE_SNAPSHOT after_attack;
    AUDIO_FEATURE_SNAPSHOT before_release;
    AUDIO_FEATURE_SNAPSHOT after_release;
    short low[AUDIO_FEATURE_FRAME_LEN];
    short high[AUDIO_FEATURE_FRAME_LEN];
    float attack_progress;
    float release_progress;
    int index;

    FillSine(low, 41, 0.02f);
    FillSine(high, 41, 0.20f);

    AudioFeatureAnalyzer_Init(&attack_state);
    Check(AudioFeatureAnalyzer_SetPeriod(&attack_state, 1U) == 1,
          "attack analyzer accepts period one");
    for (index = 0; index < 32; index++)
    {
        Check(AudioFeatureAnalyzer_ProcessFrame(
                  &attack_state, low, AUDIO_FEATURE_FRAME_LEN) == 1,
              "attack baseline frame is analyzed");
    }
    AudioFeatureAnalyzer_GetSnapshot(&attack_state, &before_attack);
    Check(AudioFeatureAnalyzer_ProcessFrame(
              &attack_state, high, AUDIO_FEATURE_FRAME_LEN) == 1,
          "attack step frame is analyzed");
    AudioFeatureAnalyzer_GetSnapshot(&attack_state, &after_attack);

    AudioFeatureAnalyzer_Init(&release_state);
    Check(AudioFeatureAnalyzer_SetPeriod(&release_state, 1U) == 1,
          "release analyzer accepts period one");
    for (index = 0; index < 32; index++)
    {
        Check(AudioFeatureAnalyzer_ProcessFrame(
                  &release_state, high, AUDIO_FEATURE_FRAME_LEN) == 1,
              "release baseline frame is analyzed");
    }
    AudioFeatureAnalyzer_GetSnapshot(&release_state, &before_release);
    Check(AudioFeatureAnalyzer_ProcessFrame(
              &release_state, low, AUDIO_FEATURE_FRAME_LEN) == 1,
          "release step frame is analyzed");
    AudioFeatureAnalyzer_GetSnapshot(&release_state, &after_release);

    attack_progress =
        (after_attack.smoothed_density[AUDIO_FEATURE_PRESENCE] -
         before_attack.smoothed_density[AUDIO_FEATURE_PRESENCE]) /
        (after_attack.raw_density[AUDIO_FEATURE_PRESENCE] -
         before_attack.smoothed_density[AUDIO_FEATURE_PRESENCE]);
    release_progress =
        (before_release.smoothed_density[AUDIO_FEATURE_PRESENCE] -
         after_release.smoothed_density[AUDIO_FEATURE_PRESENCE]) /
        (before_release.smoothed_density[AUDIO_FEATURE_PRESENCE] -
         after_release.raw_density[AUDIO_FEATURE_PRESENCE]);
    Check(isfinite(attack_progress) && isfinite(release_progress),
          "attack and release progress are finite");
    Check(fabsf(attack_progress -
                (1.0f - AUDIO_FEATURE_DEFAULT_ATTACK_ALPHA)) < 0.02f,
          "attack progress matches the configured coefficient");
    Check(fabsf(release_progress -
                (1.0f - AUDIO_FEATURE_DEFAULT_RELEASE_ALPHA)) < 0.02f,
          "release progress matches the configured coefficient");
    Check(attack_progress > release_progress,
          "attack is faster than release");
    printf("attack_progress=%.3f release_progress=%.3f "
           "attack_release_ratio=%.3f\n",
           attack_progress, release_progress,
           attack_progress / release_progress);
}

static void TestResetRestartsHistory(void)
{
    AUDIO_FEATURE_ANALYZER state;
    AUDIO_FEATURE_SNAPSHOT snapshot;
    short frame[AUDIO_FEATURE_FRAME_LEN];
    int index;

    FillSine(frame, 20, PeakFromDbfs(-18.0f));
    AudioFeatureAnalyzer_Init(&state);
    Check(AudioFeatureAnalyzer_SetSmoothing(&state, 0.0f, 0.0f) == 1,
          "reset test accepts zero smoothing");
    for (index = 0; index < 3; index++)
    {
        Check(AudioFeatureAnalyzer_AnalyzeObservedFrame(
                  &state, frame, AUDIO_FEATURE_FRAME_LEN) == 1,
              "reset setup analysis succeeds");
    }
    AudioFeatureAnalyzer_GetSnapshot(&state, &snapshot);
    Check(snapshot.analysis_count == 3UL,
          "reset setup reaches three analyses");
    Check(snapshot.warmup_complete == 1,
          "reset setup reaches warmup");

    AudioFeatureAnalyzer_Reset(&state);
    AudioFeatureAnalyzer_GetSnapshot(&state, &snapshot);
    Check(snapshot.analysis_count == 0UL,
          "reset clears analysis count");
    Check(snapshot.valid_analysis_count == 0UL,
          "reset clears valid analysis count");
    Check(snapshot.valid == 0,
          "reset clears valid");
    Check(snapshot.warmup_complete == 0,
          "reset clears warmup");

    Check(AudioFeatureAnalyzer_AnalyzeObservedFrame(
              &state, frame, AUDIO_FEATURE_FRAME_LEN) == 1,
          "first post-reset analysis succeeds");
    AudioFeatureAnalyzer_GetSnapshot(&state, &snapshot);
    Check(snapshot.analysis_count == 1UL,
          "post-reset analysis count restarts at one");
    Check(snapshot.warmup_complete == 0,
          "post-reset warmup restarts from cold state");
}

static void TestInvalidCallsAreBounded(void)
{
    AUDIO_FEATURE_ANALYZER state;
    AUDIO_FEATURE_ANALYZER before;
    AUDIO_FEATURE_ANALYZER uninitialized;
    AUDIO_FEATURE_SNAPSHOT snapshot;
    AUDIO_FEATURE_SNAPSHOT zero_snapshot;
    short frame[AUDIO_FEATURE_FRAME_LEN];
    float invalid_values[3];
    int invalid_index;

    FillSine(frame, 41, PeakFromDbfs(-18.0f));
    AudioFeatureAnalyzer_Init(0);
    AudioFeatureAnalyzer_Reset(0);
    AudioFeatureAnalyzer_GetSnapshot(0, 0);
    invalid_call_checks += 3;

    AudioFeatureAnalyzer_Init(&state);
    before = state;
    Check(AudioFeatureAnalyzer_ProcessFrame(
              &state, 0, AUDIO_FEATURE_FRAME_LEN) == -1,
          "NULL input is rejected");
    Check(memcmp(&state, &before, sizeof(state)) == 0,
          "NULL input leaves analyzer state unchanged");
    invalid_call_checks++;

    Check(AudioFeatureAnalyzer_ProcessFrame(
              &state, frame, AUDIO_FEATURE_FRAME_LEN - 1) == -1,
          "short input length is rejected");
    Check(memcmp(&state, &before, sizeof(state)) == 0,
          "short input leaves analyzer state unchanged");
    invalid_call_checks++;

    Check(AudioFeatureAnalyzer_ProcessFrame(
              &state, frame, AUDIO_FEATURE_FRAME_LEN + 1) == -1,
          "long input length is rejected");
    Check(memcmp(&state, &before, sizeof(state)) == 0,
          "long input leaves analyzer state unchanged");
    invalid_call_checks++;

    Check(AudioFeatureAnalyzer_ProcessFrame(
              0, frame, AUDIO_FEATURE_FRAME_LEN) == -1,
          "NULL analyzer state is rejected");
    invalid_call_checks++;

    memset(&uninitialized, 0, sizeof(uninitialized));
    Check(AudioFeatureAnalyzer_ProcessFrame(
              &uninitialized, frame, AUDIO_FEATURE_FRAME_LEN) == -1,
          "uninitialized analyzer state is rejected");
    invalid_call_checks++;

    memset(&snapshot, 0xa5, sizeof(snapshot));
    memset(&zero_snapshot, 0, sizeof(zero_snapshot));
    AudioFeatureAnalyzer_GetSnapshot(0, &snapshot);
    Check(memcmp(&snapshot, &zero_snapshot, sizeof(snapshot)) == 0,
          "NULL-state snapshot is bounded and fully zeroed");
    invalid_call_checks++;

    Check(AudioFeatureAnalyzer_SetPeriod(0, 1U) == 0,
          "NULL-state period update is rejected");
    Check(AudioFeatureAnalyzer_SetPeriod(&state, 0U) == 0,
          "zero period is rejected");
    Check(AudioFeatureAnalyzer_SetSmoothing(0, 0.5f, 0.5f) == 0,
          "NULL-state smoothing update is rejected");
    Check(AudioFeatureAnalyzer_SetSmoothing(&state, -0.1f, 0.5f) == 0,
          "negative attack coefficient is rejected");
    Check(AudioFeatureAnalyzer_SetSmoothing(&state, 0.5f, 1.0f) == 0,
          "release coefficient of one is rejected");
    invalid_call_checks += 5;

    invalid_values[0] = NAN;
    invalid_values[1] = INFINITY;
    invalid_values[2] = -INFINITY;
    for (invalid_index = 0; invalid_index < 3; invalid_index++)
    {
        before = state;
        Check(AudioFeatureAnalyzer_SetSmoothing(
                  &state, invalid_values[invalid_index], 0.5f) == 0,
              "non-finite attack coefficient is rejected");
        Check(memcmp(&state, &before, sizeof(state)) == 0,
              "rejected attack coefficient leaves state unchanged");
        Check(AudioFeatureAnalyzer_SetSmoothing(
                  &state, 0.5f, invalid_values[invalid_index]) == 0,
              "non-finite release coefficient is rejected");
        Check(memcmp(&state, &before, sizeof(state)) == 0,
              "rejected release coefficient leaves state unchanged");
        invalid_call_checks += 2;
    }

    AudioFeatureAnalyzer_Init(&state);
    Check(AudioFeatureAnalyzer_SetSmoothing(&state, 0.0f, 0.0f) == 1,
          "finite validation setup accepts zero smoothing");
    Check(AudioFeatureAnalyzer_AnalyzeObservedFrame(
              &state, frame, AUDIO_FEATURE_FRAME_LEN) == 1,
          "finite validation setup frame succeeds");
    state.attack_alpha = NAN;
    state.release_alpha = NAN;
    Check(AudioFeatureAnalyzer_AnalyzeObservedFrame(
              &state, frame, AUDIO_FEATURE_FRAME_LEN) ==
              AUDIO_FEATURE_ANALYZE_ERROR_NONFINITE,
          "non-finite analysis is rejected with a distinct error");
    Check(state.valid == 0,
          "non-finite analysis invalidates the analyzer");
    AudioFeatureAnalyzer_GetSnapshot(&state, &snapshot);
    Check(SnapshotIsFinite(&snapshot),
          "non-finite analysis never publishes NaN or infinity");

    printf("invalid_call_checks=%d input_immutable_checks=%d\n",
           invalid_call_checks, input_immutable_checks);
}

int main(void)
{
    TestSilenceFiniteInvalidAndInputImmutable();
    TestToneDominanceAndBassBoost();
    TestRmsDelta();
    TestRelativeAmplitudeInvariance();
    TestSeededNoiseFairnessAndDeterminism();
    TestCadencePeriodEight();
    TestSplitObserveAndAnalyze();
    TestAttackFasterThanRelease();
    TestResetRestartsHistory();
    TestInvalidCallsAreBounded();

    printf("failures=%d\n", failures);
    if (failures != 0)
    {
        printf("equalizer audio feature: FAIL\n");
        return 1;
    }
    printf("equalizer audio feature: PASS\n");
    return 0;
}
