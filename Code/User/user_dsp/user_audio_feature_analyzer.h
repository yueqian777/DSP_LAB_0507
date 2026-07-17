/**
 * user_audio_feature_analyzer.h
 *
 * Read-only 1024-point audio content analysis for Project 3.3.
 */

#ifndef _USER_AUDIO_FEATURE_ANALYZER_H_
#define _USER_AUDIO_FEATURE_ANALYZER_H_

#define AUDIO_FEATURE_SAMPLE_RATE    50000.0f
#define AUDIO_FEATURE_FRAME_LEN      1024
#define AUDIO_FEATURE_FFT_SIZE       1024
#define AUDIO_FEATURE_POS_BINS       513
#define AUDIO_FEATURE_NUM_BANDS      4
#define AUDIO_FEATURE_DEFAULT_PERIOD 8

#define AUDIO_FEATURE_BASS       0
#define AUDIO_FEATURE_MUD        1
#define AUDIO_FEATURE_PRESENCE   2
#define AUDIO_FEATURE_BRIGHTNESS 3

#define AUDIO_FEATURE_DEFAULT_ATTACK_ALPHA  0.60f
#define AUDIO_FEATURE_DEFAULT_RELEASE_ALPHA 0.90f
#define AUDIO_FEATURE_SILENCE_DBFS          (-70.0f)

typedef struct
{
    float peak_dbfs;
    float rms_dbfs;
    float raw_density[AUDIO_FEATURE_NUM_BANDS];
    float smoothed_density[AUDIO_FEATURE_NUM_BANDS];
    float relative_db[AUDIO_FEATURE_NUM_BANDS];
    float global_density;
    unsigned long input_frame_count;
    unsigned long analysis_count;
    unsigned long skipped_frame_count;
    unsigned long valid_analysis_count;
    unsigned int period_frames;
    int valid;
    int warmup_complete;
} AUDIO_FEATURE_SNAPSHOT;

typedef struct
{
    float window[AUDIO_FEATURE_FFT_SIZE];
    float twiddle_re[AUDIO_FEATURE_FFT_SIZE / 2];
    float twiddle_im[AUDIO_FEATURE_FFT_SIZE / 2];
    float fft_re[AUDIO_FEATURE_FFT_SIZE];
    float fft_im[AUDIO_FEATURE_FFT_SIZE];
    float peak_dbfs;
    float rms_dbfs;
    float raw_density[AUDIO_FEATURE_NUM_BANDS];
    float smoothed_density[AUDIO_FEATURE_NUM_BANDS];
    float relative_db[AUDIO_FEATURE_NUM_BANDS];
    float global_density;
    float smoothed_global_density;
    float attack_alpha;
    float release_alpha;
    unsigned long input_frame_count;
    unsigned long analysis_count;
    unsigned long skipped_frame_count;
    unsigned long valid_analysis_count;
    unsigned int period_frames;
    int initialized;
    int valid;
    int warmup_complete;
} AUDIO_FEATURE_ANALYZER;

void AudioFeatureAnalyzer_Init(AUDIO_FEATURE_ANALYZER *state);
void AudioFeatureAnalyzer_Reset(AUDIO_FEATURE_ANALYZER *state);
int AudioFeatureAnalyzer_SetPeriod(AUDIO_FEATURE_ANALYZER *state,
                                   unsigned int period_frames);
int AudioFeatureAnalyzer_SetSmoothing(AUDIO_FEATURE_ANALYZER *state,
                                      float attack_alpha,
                                      float release_alpha);
int AudioFeatureAnalyzer_ObserveFrame(AUDIO_FEATURE_ANALYZER *state);
int AudioFeatureAnalyzer_AnalyzeObservedFrame(
    AUDIO_FEATURE_ANALYZER *state,
    const short *input,
    int sample_count);
int AudioFeatureAnalyzer_ProcessFrame(AUDIO_FEATURE_ANALYZER *state,
                                      const short *input,
                                      int sample_count);
void AudioFeatureAnalyzer_GetSnapshot(const AUDIO_FEATURE_ANALYZER *state,
                                      AUDIO_FEATURE_SNAPSHOT *snapshot);

#endif /* _USER_AUDIO_FEATURE_ANALYZER_H_ */
