/**
 * user_audio_feature_analyzer.c
 *
 * Independent read-only audio feature analyzer. This module has no board,
 * UART, display, or equalizer dependencies.
 */

#include "user_audio_feature_analyzer.h"
#include "user_spectral_fft.h"
#include "math.h"
#include "string.h"

#define AUDIO_FEATURE_PI              3.14159265358979323846f
#define AUDIO_FEATURE_PCM_SCALE       (1.0f / 32768.0f)
#define AUDIO_FEATURE_POWER_EPSILON   1.0e-20f
#define AUDIO_FEATURE_AMPLITUDE_FLOOR 1.0e-12f

static float AudioFeature_AmplitudeDbfs(float value)
{
    if (value < AUDIO_FEATURE_AMPLITUDE_FLOOR)
    {
        value = AUDIO_FEATURE_AMPLITUDE_FLOOR;
    }
    return 20.0f * log10f(value);
}

static int AudioFeature_BandForFrequency(float frequency_hz)
{
    if ((frequency_hz >= 20.0f) && (frequency_hz < 200.0f))
    {
        return AUDIO_FEATURE_BASS;
    }
    if ((frequency_hz >= 200.0f) && (frequency_hz < 800.0f))
    {
        return AUDIO_FEATURE_MUD;
    }
    if ((frequency_hz >= 800.0f) && (frequency_hz < 4000.0f))
    {
        return AUDIO_FEATURE_PRESENCE;
    }
    if ((frequency_hz >= 4000.0f) && (frequency_hz <= 16000.0f))
    {
        return AUDIO_FEATURE_BRIGHTNESS;
    }
    return -1;
}

static float AudioFeature_Smooth(float previous,
                                 float current,
                                 float attack_alpha,
                                 float release_alpha)
{
    float alpha;

    alpha = (current > previous) ? attack_alpha : release_alpha;
    return alpha * previous + (1.0f - alpha) * current;
}

static void AudioFeature_UpdateRelative(AUDIO_FEATURE_ANALYZER *state)
{
    int band;
    float denominator;

    denominator = state->smoothed_global_density +
                  AUDIO_FEATURE_POWER_EPSILON;
    for (band = 0; band < AUDIO_FEATURE_NUM_BANDS; band++)
    {
        state->relative_db[band] = 10.0f * log10f(
            (state->smoothed_density[band] +
             AUDIO_FEATURE_POWER_EPSILON) / denominator);
    }
}

void AudioFeatureAnalyzer_Init(AUDIO_FEATURE_ANALYZER *state)
{
    int index;
    float phase;

    if (state == 0)
    {
        return;
    }
    memset(state, 0, sizeof(*state));
    state->period_frames = AUDIO_FEATURE_DEFAULT_PERIOD;
    state->attack_alpha = AUDIO_FEATURE_DEFAULT_ATTACK_ALPHA;
    state->release_alpha = AUDIO_FEATURE_DEFAULT_RELEASE_ALPHA;
    state->peak_dbfs = AudioFeature_AmplitudeDbfs(0.0f);
    state->rms_dbfs = AudioFeature_AmplitudeDbfs(0.0f);

    for (index = 0; index < AUDIO_FEATURE_FFT_SIZE; index++)
    {
        phase = (2.0f * AUDIO_FEATURE_PI * (float)index) /
                (float)(AUDIO_FEATURE_FFT_SIZE - 1);
        state->window[index] = 0.5f - 0.5f * cosf(phase);
    }
    state->initialized = SpectralFFT_GenerateTwiddles(
        state->twiddle_re, state->twiddle_im, AUDIO_FEATURE_FFT_SIZE);
}

void AudioFeatureAnalyzer_Reset(AUDIO_FEATURE_ANALYZER *state)
{
    int band;

    if (state == 0)
    {
        return;
    }
    if (state->initialized == 0)
    {
        AudioFeatureAnalyzer_Init(state);
        return;
    }
    memset(state->fft_re, 0, sizeof(state->fft_re));
    memset(state->fft_im, 0, sizeof(state->fft_im));
    state->peak_dbfs = AudioFeature_AmplitudeDbfs(0.0f);
    state->rms_dbfs = AudioFeature_AmplitudeDbfs(0.0f);
    state->global_density = 0.0f;
    state->smoothed_global_density = 0.0f;
    for (band = 0; band < AUDIO_FEATURE_NUM_BANDS; band++)
    {
        state->raw_density[band] = 0.0f;
        state->smoothed_density[band] = 0.0f;
        state->relative_db[band] = 0.0f;
    }
    state->input_frame_count = 0UL;
    state->analysis_count = 0UL;
    state->skipped_frame_count = 0UL;
    state->valid_analysis_count = 0UL;
    state->valid = 0;
    state->warmup_complete = 0;
}

int AudioFeatureAnalyzer_SetPeriod(AUDIO_FEATURE_ANALYZER *state,
                                   unsigned int period_frames)
{
    if ((state == 0) || (state->initialized == 0) ||
        (period_frames == 0U))
    {
        return 0;
    }
    state->period_frames = period_frames;
    return 1;
}

int AudioFeatureAnalyzer_SetSmoothing(AUDIO_FEATURE_ANALYZER *state,
                                      float attack_alpha,
                                      float release_alpha)
{
    if ((state == 0) || (state->initialized == 0) ||
        (attack_alpha < 0.0f) || (attack_alpha >= 1.0f) ||
        (release_alpha < 0.0f) || (release_alpha >= 1.0f))
    {
        return 0;
    }
    state->attack_alpha = attack_alpha;
    state->release_alpha = release_alpha;
    return 1;
}

int AudioFeatureAnalyzer_ProcessFrame(AUDIO_FEATURE_ANALYZER *state,
                                      const short *input,
                                      int sample_count)
{
    int index;
    int band;
    int band_counts[AUDIO_FEATURE_NUM_BANDS];
    int global_count;
    float band_power[AUDIO_FEATURE_NUM_BANDS];
    float global_power;
    float mean;
    float peak;
    float rms_power;
    float power_scale;

    if ((state == 0) || (input == 0) ||
        (sample_count != AUDIO_FEATURE_FRAME_LEN) ||
        (state->initialized == 0) || (state->period_frames == 0U))
    {
        return -1;
    }

    state->input_frame_count++;
    if ((state->input_frame_count % state->period_frames) != 0UL)
    {
        state->skipped_frame_count++;
        return 0;
    }
    state->analysis_count++;

    mean = 0.0f;
    for (index = 0; index < AUDIO_FEATURE_FRAME_LEN; index++)
    {
        mean += (float)input[index];
    }
    mean /= (float)AUDIO_FEATURE_FRAME_LEN;

    peak = 0.0f;
    rms_power = 0.0f;
    for (index = 0; index < AUDIO_FEATURE_FRAME_LEN; index++)
    {
        float centered;
        float magnitude;

        centered = ((float)input[index] - mean) * AUDIO_FEATURE_PCM_SCALE;
        magnitude = (centered < 0.0f) ? -centered : centered;
        if (magnitude > peak)
        {
            peak = magnitude;
        }
        rms_power += centered * centered;
        state->fft_re[index] = centered * state->window[index];
        state->fft_im[index] = 0.0f;
    }
    rms_power /= (float)AUDIO_FEATURE_FRAME_LEN;
    state->peak_dbfs = AudioFeature_AmplitudeDbfs(peak);
    state->rms_dbfs = AudioFeature_AmplitudeDbfs(sqrtf(rms_power));

    if (SpectralFFT_Forward(state->fft_re, state->fft_im,
                            state->twiddle_re, state->twiddle_im,
                            AUDIO_FEATURE_FFT_SIZE) == 0)
    {
        return -2;
    }
    for (band = 0; band < AUDIO_FEATURE_NUM_BANDS; band++)
    {
        band_power[band] = 0.0f;
        band_counts[band] = 0;
    }
    global_power = 0.0f;
    global_count = 0;
    power_scale = 2.0f /
                  ((float)AUDIO_FEATURE_FFT_SIZE *
                   (float)AUDIO_FEATURE_FFT_SIZE);

    for (index = 1; index < AUDIO_FEATURE_FFT_SIZE / 2; index++)
    {
        float frequency_hz;
        float power;

        frequency_hz = ((float)index * AUDIO_FEATURE_SAMPLE_RATE) /
                       (float)AUDIO_FEATURE_FFT_SIZE;
        band = AudioFeature_BandForFrequency(frequency_hz);
        if (band >= 0)
        {
            power = (state->fft_re[index] * state->fft_re[index] +
                     state->fft_im[index] * state->fft_im[index]) *
                    power_scale;
            band_power[band] += power;
            band_counts[band]++;
            global_power += power;
            global_count++;
        }
    }

    state->valid = (state->rms_dbfs >= AUDIO_FEATURE_SILENCE_DBFS) ? 1 : 0;
    for (band = 0; band < AUDIO_FEATURE_NUM_BANDS; band++)
    {
        float current_density;

        current_density = 0.0f;
        if ((state->valid != 0) && (band_counts[band] > 0))
        {
            current_density = band_power[band] / (float)band_counts[band];
        }
        state->raw_density[band] = current_density;
        state->smoothed_density[band] = AudioFeature_Smooth(
            state->smoothed_density[band], current_density,
            state->attack_alpha, state->release_alpha);
    }
    state->global_density = 0.0f;
    if ((state->valid != 0) && (global_count > 0))
    {
        state->global_density = global_power / (float)global_count;
    }
    state->smoothed_global_density = AudioFeature_Smooth(
        state->smoothed_global_density, state->global_density,
        state->attack_alpha, state->release_alpha);
    AudioFeature_UpdateRelative(state);

    if (state->valid != 0)
    {
        state->valid_analysis_count++;
        if (state->valid_analysis_count >= 3UL)
        {
            state->warmup_complete = 1;
        }
    }
    return 1;
}

void AudioFeatureAnalyzer_GetSnapshot(const AUDIO_FEATURE_ANALYZER *state,
                                      AUDIO_FEATURE_SNAPSHOT *snapshot)
{
    int band;

    if (snapshot == 0)
    {
        return;
    }
    memset(snapshot, 0, sizeof(*snapshot));
    if (state == 0)
    {
        return;
    }
    snapshot->peak_dbfs = state->peak_dbfs;
    snapshot->rms_dbfs = state->rms_dbfs;
    snapshot->global_density = state->global_density;
    for (band = 0; band < AUDIO_FEATURE_NUM_BANDS; band++)
    {
        snapshot->raw_density[band] = state->raw_density[band];
        snapshot->smoothed_density[band] = state->smoothed_density[band];
        snapshot->relative_db[band] = state->relative_db[band];
    }
    snapshot->input_frame_count = state->input_frame_count;
    snapshot->analysis_count = state->analysis_count;
    snapshot->skipped_frame_count = state->skipped_frame_count;
    snapshot->valid_analysis_count = state->valid_analysis_count;
    snapshot->period_frames = state->period_frames;
    snapshot->valid = state->valid;
    snapshot->warmup_complete = state->warmup_complete;
}
