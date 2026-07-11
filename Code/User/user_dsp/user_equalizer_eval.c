/**
 * user_equalizer_eval.c
 *
 * Strict host-side quality checks for Project 3.3.  This file is compiled
 * only for EQ_ALGO_ONLY; the board build carries no offline test buffers.
 */

#include "user_equalizer_eval.h"

#ifdef EQ_ALGO_ONLY

#include "user_equalizer.h"
#include "math.h"
#include "stdio.h"
#include "string.h"

#define EQ_EVAL_PI 3.14159265358979323846
#define EQ_EVAL_EPS 1.0e-20
#define EQ_EVAL_LOG_POINTS 512
#define EQ_EVAL_FRAME_SAMPLES 1024
#define EQ_EVAL_SETTLE_SAMPLES 7000
#define EQ_EVAL_THD_SAMPLES 5000
#define EQ_EVAL_THD_BIN 100
#define EQ_EVAL_THD_MAX_DB (-60.0)
#define EQ_EVAL_LONG_SECONDS 60
#define EQ_EVAL_LONG_SAMPLES ((int)(EQ_SAMPLE_RATE * EQ_EVAL_LONG_SECONDS))

#define EQ_EVAL_PRESET_SINGLE_1K_PLUS3DB EQ_PRESET_COUNT
#define EQ_EVAL_PRESET_CASE_COUNT (EQ_PRESET_COUNT + 1)

static short EQ_EvalShortIn[EQ_EVAL_FRAME_SAMPLES * 2];
static short EQ_EvalShortOut[EQ_EVAL_FRAME_SAMPLES * 2];
static float EQ_EvalFloatIn[EQ_EVAL_FRAME_SAMPLES * 2];
static float EQ_EvalFloatOut[EQ_EVAL_FRAME_SAMPLES * 2];
static float EQ_EvalThdOut[EQ_EVAL_THD_SAMPLES];

static int EQ_EvalIsBad(float value)
{
    return ((value != value) || (value > 3.0e38f) || (value < -3.0e38f));
}

static float EQ_EvalAbs(float value)
{
    return (value < 0.0f) ? -value : value;
}

static float EQ_EvalDb(float value)
{
    if (value < (float)EQ_EVAL_EPS)
    {
        value = (float)EQ_EVAL_EPS;
    }
    return 20.0f * log10f(value);
}

static float EQ_EvalLogFrequency(int index)
{
    float log_min = logf(20.0f);
    float log_max = logf(20000.0f);
    float step = (log_max - log_min) / (float)(EQ_EVAL_LOG_POINTS - 1);

    return expf(log_min + step * (float)index);
}

static const char *EQ_EvalCoreName(int core)
{
    switch (core)
    {
        case EQ_CORE_RAW_COPY:
            return "raw_copy";
        case EQ_CORE_FLOAT_COPY:
            return "float_copy";
        case EQ_CORE_LEGACY:
            return "legacy";
        case EQ_CORE_RBJ_CASCADE:
            return "rbj_cascade";
        default:
            return "invalid";
    }
}

static const char *EQ_EvalPresetName(int preset)
{
    switch (preset)
    {
        case EQ_PRESET_FLAT:
            return "flat";
        case EQ_PRESET_BASS_BOOST:
            return "bass";
        case EQ_PRESET_VOCAL:
            return "vocal";
        case EQ_PRESET_TREBLE_BOOST:
            return "treble";
        case EQ_PRESET_V_SHAPE:
            return "v_shape";
        case EQ_EVAL_PRESET_SINGLE_1K_PLUS3DB:
            return "single_1k_plus3db";
        default:
            return "invalid";
    }
}

static const char *EQ_EvalSectionName(int type)
{
    switch (type)
    {
        case EQ_SECTION_LEGACY_BANDPASS:
            return "legacy_bandpass";
        case EQ_SECTION_LOW_SHELF:
            return "low_shelf";
        case EQ_SECTION_PEAKING:
            return "peaking";
        case EQ_SECTION_HIGH_SHELF:
            return "high_shelf";
        default:
            return "invalid";
    }
}

static void EQ_EvalSettle(EQ_STATE *st)
{
    int index;
    float input = 0.0f;
    float output;

    for (index = 0; index < EQ_EVAL_SETTLE_SAMPLES; index++)
    {
        Equalizer_ProcessFrameFloat(st, &input, &output, 1);
    }
}

static void EQ_EvalConfigure(EQ_STATE *st, int core, int preset)
{
    float gains[EQ_NUM_BANDS];
    int band;

    Equalizer_Init(st);
    Equalizer_SetCoreMode(st, core);
    if (preset == EQ_EVAL_PRESET_SINGLE_1K_PLUS3DB)
    {
        for (band = 0; band < EQ_NUM_BANDS; band++)
        {
            gains[band] = 0.0f;
        }
        gains[5] = 3.0f;
        Equalizer_SetAllGainsDb(st, gains);
    }
    else
    {
        Equalizer_ApplyPreset(st, preset);
    }
    EQ_EvalSettle(st);
}

static void EQ_EvalFillTransparencySignal(int signal_id, int count)
{
    int index;

    for (index = 0; index < count; index++)
    {
        short value;

        if (signal_id == 0)
        {
            value = (index == 0) ? 30000 : 0;
        }
        else if (signal_id == 1)
        {
            value = (short)(5000.0f * sinf(2.0f * (float)EQ_EVAL_PI *
                                           1000.0f * (float)index /
                                           EQ_SAMPLE_RATE));
        }
        else
        {
            int phase = index % 200;

            value = (short)((phase < 100) ?
                            (-12000 + phase * 240) :
                            (12000 - (phase - 100) * 240));
            if ((index % EQ_EVAL_FRAME_SAMPLES) == 0)
            {
                value = 28000;
            }
        }
        EQ_EvalShortIn[index] = value;
        EQ_EvalFloatIn[index] = (float)value / 32768.0f;
    }
}

static void EQ_EvalErrorShort(const short *input, const short *output,
                              int count, float *max_error, float *rms_error,
                              float *snr_db)
{
    int index;
    double error_sq = 0.0;
    double signal_sq = 0.0;

    *max_error = 0.0f;
    for (index = 0; index < count; index++)
    {
        float error = (float)output[index] - (float)input[index];

        if (EQ_EvalAbs(error) > *max_error)
        {
            *max_error = EQ_EvalAbs(error);
        }
        error_sq += (double)error * (double)error;
        signal_sq += (double)input[index] * (double)input[index];
    }
    *rms_error = (float)sqrt(error_sq / (double)count);
    *snr_db = (*rms_error == 0.0f) ? 300.0f :
              (float)(10.0 * log10(signal_sq / (error_sq + EQ_EVAL_EPS)));
}

static void EQ_EvalErrorFloat(const float *input, const float *output,
                              int count, float *max_error, float *rms_error,
                              float *snr_db)
{
    int index;
    double error_sq = 0.0;
    double signal_sq = 0.0;

    *max_error = 0.0f;
    for (index = 0; index < count; index++)
    {
        float error = output[index] - input[index];

        if (EQ_EvalAbs(error) > *max_error)
        {
            *max_error = EQ_EvalAbs(error);
        }
        error_sq += (double)error * (double)error;
        signal_sq += (double)input[index] * (double)input[index];
    }
    *rms_error = (float)sqrt(error_sq / (double)count);
    *snr_db = (*rms_error == 0.0f) ? 300.0f :
              (float)(10.0 * log10(signal_sq / (error_sq + EQ_EVAL_EPS)));
}

static int EQ_EvalWriteTransparencyReport(void)
{
    FILE *file;
    EQ_STATE st;
    int signal_id;
    int failures = 0;
    const int count = EQ_EVAL_FRAME_SAMPLES * 2;
    const char *signal_name[3] = { "impulse", "sine_1k", "frame_boundary" };

    file = fopen("equalizer_flat_transparency_report.csv", "w");
    if (file == 0)
    {
        return 1;
    }
    fprintf(file, "test_signal,path,sample_count,max_abs_error,rms_error,snr_db,clip_count,pass\n");
    for (signal_id = 0; signal_id < 3; signal_id++)
    {
        float max_error;
        float rms_error;
        float snr_db;
        int pass;
        EQ_STATE before;

        EQ_EvalFillTransparencySignal(signal_id, count);
        Equalizer_Init(&st);
        Equalizer_SetCoreMode(&st, EQ_CORE_RBJ_CASCADE);
        Equalizer_ApplyPreset(&st, EQ_PRESET_BASS_BOOST);
        EQ_EvalSettle(&st);
        Equalizer_SetBypass(&st, 1);
        before = st;
        Equalizer_ProcessFrame(&st, EQ_EvalShortIn, EQ_EvalShortOut, count);
        EQ_EvalErrorShort(EQ_EvalShortIn, EQ_EvalShortOut, count,
                          &max_error, &rms_error, &snr_db);
        pass = ((max_error == 0.0f) && (st.clip_count == 0UL) &&
                (memcmp(&before, &st, sizeof(st)) == 0));
        fprintf(file, "%s,hard_bypass_short,%d,%.6f,%.9f,%.3f,%lu,%s\n",
                signal_name[signal_id], count, max_error, rms_error, snr_db,
                st.clip_count, pass ? "PASS" : "FAIL");
        if (pass == 0)
        {
            failures++;
        }

        Equalizer_Init(&st);
        Equalizer_SetCoreMode(&st, EQ_CORE_FLOAT_COPY);
        Equalizer_ProcessFrame(&st, EQ_EvalShortIn, EQ_EvalShortOut, count);
        EQ_EvalErrorShort(EQ_EvalShortIn, EQ_EvalShortOut, count,
                          &max_error, &rms_error, &snr_db);
        pass = ((max_error == 0.0f) && (st.clip_count == 0UL) &&
                (EQ_DebugFloatCopyMaxError == 0.0f));
        fprintf(file, "%s,float_copy_short,%d,%.6f,%.9f,%.3f,%lu,%s\n",
                signal_name[signal_id], count, max_error, rms_error, snr_db,
                st.clip_count, pass ? "PASS" : "FAIL");
        if (pass == 0)
        {
            failures++;
        }

        Equalizer_Init(&st);
        Equalizer_SetCoreMode(&st, EQ_CORE_RBJ_CASCADE);
        Equalizer_ApplyPreset(&st, EQ_PRESET_BASS_BOOST);
        EQ_EvalSettle(&st);
        Equalizer_SetBypass(&st, 1);
        before = st;
        Equalizer_ProcessFrameFloat(&st, EQ_EvalFloatIn, EQ_EvalFloatOut,
                                    count);
        EQ_EvalErrorFloat(EQ_EvalFloatIn, EQ_EvalFloatOut, count,
                          &max_error, &rms_error, &snr_db);
        pass = ((max_error == 0.0f) && (st.clip_count == 0UL) &&
                (memcmp(&before, &st, sizeof(st)) == 0));
        fprintf(file, "%s,hard_bypass_float,%d,%.9f,%.12f,%.3f,%lu,%s\n",
                signal_name[signal_id], count, max_error, rms_error, snr_db,
                st.clip_count, pass ? "PASS" : "FAIL");
        if (pass == 0)
        {
            failures++;
        }

        Equalizer_Init(&st);
        Equalizer_SetCoreMode(&st, EQ_CORE_RBJ_CASCADE);
        Equalizer_ApplyPreset(&st, EQ_PRESET_FLAT);
        Equalizer_ProcessFrame(&st, EQ_EvalShortIn, EQ_EvalShortOut, count);
        EQ_EvalErrorShort(EQ_EvalShortIn, EQ_EvalShortOut, count,
                          &max_error, &rms_error, &snr_db);
        pass = ((max_error == 0.0f) && (st.clip_count == 0UL));
        fprintf(file, "%s,flat_short,%d,%.6f,%.9f,%.3f,%lu,%s\n",
                signal_name[signal_id], count, max_error, rms_error, snr_db,
                st.clip_count, pass ? "PASS" : "FAIL");
        if (pass == 0)
        {
            failures++;
        }

        Equalizer_Init(&st);
        Equalizer_SetCoreMode(&st, EQ_CORE_RBJ_CASCADE);
        Equalizer_ApplyPreset(&st, EQ_PRESET_FLAT);
        Equalizer_ProcessFrameFloat(&st, EQ_EvalFloatIn, EQ_EvalFloatOut,
                                    count);
        EQ_EvalErrorFloat(EQ_EvalFloatIn, EQ_EvalFloatOut, count,
                          &max_error, &rms_error, &snr_db);
        pass = ((max_error == 0.0f) && (st.clip_count == 0UL));
        fprintf(file, "%s,flat_float,%d,%.9f,%.12f,%.3f,%lu,%s\n",
                signal_name[signal_id], count, max_error, rms_error, snr_db,
                st.clip_count, pass ? "PASS" : "FAIL");
        if (pass == 0)
        {
            failures++;
        }
    }
    fclose(file);
    return failures;
}

static int EQ_EvalWriteBandResponse(void)
{
    FILE *file;
    int band;
    int index;
    int failures = 0;

    file = fopen("equalizer_band_response.csv", "w");
    if (file == 0)
    {
        return 1;
    }
    fprintf(file, "band,band_center_hz,freq_hz,magnitude_db,pass\n");
    for (band = 0; band < EQ_NUM_BANDS; band++)
    {
        for (index = 0; index < EQ_EVAL_LOG_POINTS; index++)
        {
            float frequency = EQ_EvalLogFrequency(index);
            float magnitude = Equalizer_GetBandMagnitudeDb(band, frequency);
            int pass = (EQ_EvalIsBad(magnitude) == 0);

            fprintf(file, "%d,%.6f,%.6f,%.9f,%s\n", band,
                    Equalizer_GetBandCenterHz(band), frequency, magnitude,
                    pass ? "PASS" : "FAIL");
            if (pass == 0)
            {
                failures++;
            }
        }
    }
    fclose(file);
    return failures;
}

static int EQ_EvalWriteLegacySystemResponse(void)
{
    FILE *response_file;
    FILE *summary_file;
    int preset;
    int index;
    int failures = 0;

    response_file = fopen("equalizer_legacy_system_response.csv", "w");
    summary_file = fopen("equalizer_legacy_summary.csv", "w");
    if ((response_file == 0) || (summary_file == 0))
    {
        if (response_file != 0)
        {
            fclose(response_file);
        }
        if (summary_file != 0)
        {
            fclose(summary_file);
        }
        return 1;
    }
    fprintf(response_file, "preset,freq_hz,magnitude_db,phase_deg,group_delay_samples\n");
    fprintf(summary_file, "preset,max_gain_db,min_gain_db,max_group_delay_samples,center_command_error_db,legacy_unsafe_preset,abnormal_peak,expected_headroom_db,pass\n");
    for (preset = EQ_PRESET_FLAT; preset < EQ_EVAL_PRESET_CASE_COUNT; preset++)
    {
        EQ_STATE st;
        float max_gain = -200.0f;
        float min_gain = 200.0f;
        float max_delay = 0.0f;
        float center_error = 0.0f;
        int pass = 1;
        int band;

        EQ_EvalConfigure(&st, EQ_CORE_LEGACY, preset);
        for (index = 0; index < EQ_EVAL_LOG_POINTS; index++)
        {
            float frequency = EQ_EvalLogFrequency(index);
            float magnitude = Equalizer_GetSystemMagnitudeDb(&st, frequency);
            float phase = Equalizer_GetSystemPhaseDeg(&st, frequency);
            float delay = Equalizer_GetSystemGroupDelaySamples(&st, frequency);

            if ((EQ_EvalIsBad(magnitude) != 0) || (EQ_EvalIsBad(phase) != 0) ||
                (EQ_EvalIsBad(delay) != 0))
            {
                pass = 0;
            }
            if (magnitude > max_gain)
            {
                max_gain = magnitude;
            }
            if (magnitude < min_gain)
            {
                min_gain = magnitude;
            }
            if (EQ_EvalAbs(delay) > max_delay)
            {
                max_delay = EQ_EvalAbs(delay);
            }
            fprintf(response_file, "%s,%.9g,%.9f,%.9f,%.9f\n",
                    EQ_EvalPresetName(preset), frequency, magnitude, phase,
                    delay);
        }
        for (band = 0; band < EQ_NUM_BANDS; band++)
        {
            float response = Equalizer_GetSystemMagnitudeDb(&st,
                                      Equalizer_GetBandCenterHz(band));
            float error = EQ_EvalAbs(response -
                                      Equalizer_GetBandTargetGainDb(&st, band));

            if (error > center_error)
            {
                center_error = error;
            }
        }
        fprintf(summary_file,
                "%s,%.9f,%.9f,%.9f,%.9f,%d,%d,%.9f,%s\n",
                EQ_EvalPresetName(preset), max_gain, min_gain, max_delay,
                center_error, EQ_LEGACY_UNSAFE_PRESET,
                (max_gain > EQ_GAIN_MAX_DB) ? 1 : 0, max_gain,
                pass ? "PASS" : "FAIL");
        if (pass == 0)
        {
            failures++;
        }
    }
    fclose(response_file);
    fclose(summary_file);
    return failures;
}

static int EQ_EvalWriteSystemResponse(void)
{
    FILE *response_file;
    FILE *delay_file;
    int core_index;
    int preset;
    int index;
    int failures = 0;
    const int cores[2] = { EQ_CORE_LEGACY, EQ_CORE_RBJ_CASCADE };

    response_file = fopen("equalizer_system_response.csv", "w");
    delay_file = fopen("equalizer_group_delay_report.csv", "w");
    if ((response_file == 0) || (delay_file == 0))
    {
        if (response_file != 0)
        {
            fclose(response_file);
        }
        if (delay_file != 0)
        {
            fclose(delay_file);
        }
        return 1;
    }
    fprintf(response_file, "core,preset,freq_hz,magnitude_db,phase_deg,group_delay_samples,predicted_peak_db,preamp_db,pass\n");
    fprintf(delay_file, "core,preset,max_group_delay_samples,pass\n");
    for (core_index = 0; core_index < 2; core_index++)
    {
        for (preset = EQ_PRESET_FLAT; preset < EQ_PRESET_COUNT; preset++)
        {
            EQ_STATE st;
            float max_delay = 0.0f;
            int pass = 1;

            EQ_EvalConfigure(&st, cores[core_index], preset);
            for (index = 0; index < EQ_EVAL_LOG_POINTS; index++)
            {
                float frequency = EQ_EvalLogFrequency(index);
                float magnitude = Equalizer_GetSystemMagnitudeDb(&st,
                                                                  frequency);
                float phase = Equalizer_GetSystemPhaseDeg(&st, frequency);
                float delay = Equalizer_GetSystemGroupDelaySamples(&st,
                                                                    frequency);

                if ((EQ_EvalIsBad(magnitude) != 0) ||
                    (EQ_EvalIsBad(phase) != 0) ||
                    (EQ_EvalIsBad(delay) != 0))
                {
                    pass = 0;
                }
                if (EQ_EvalAbs(delay) > max_delay)
                {
                    max_delay = EQ_EvalAbs(delay);
                }
                fprintf(response_file,
                        "%s,%s,%.9g,%.9f,%.9f,%.9f,%.9f,%.9f,%s\n",
                        EQ_EvalCoreName(cores[core_index]),
                        EQ_EvalPresetName(preset), frequency, magnitude, phase,
                        delay, Equalizer_GetPredictedPeakDb(&st),
                        Equalizer_GetPreampDb(&st), pass ? "PASS" : "FAIL");
            }
            fprintf(delay_file, "%s,%s,%.9f,%s\n",
                    EQ_EvalCoreName(cores[core_index]),
                    EQ_EvalPresetName(preset), max_delay,
                    pass ? "PASS" : "FAIL");
            if (pass == 0)
            {
                failures++;
            }
        }
    }
    fclose(response_file);
    fclose(delay_file);
    return failures;
}

static int EQ_EvalWriteCoefficients(void)
{
    FILE *file;
    int core_index;
    int preset;
    int section;
    int failures = 0;
    const int cores[2] = { EQ_CORE_LEGACY, EQ_CORE_RBJ_CASCADE };

    file = fopen("equalizer_coefficients.csv", "w");
    if (file == 0)
    {
        return 1;
    }
    fprintf(file, "core,preset,section,type,f0_hz,gain_db,q_or_bw,b0,b1,b2,a1,a2,pole_radius_1,pole_radius_2,pass\n");
    for (core_index = 0; core_index < 2; core_index++)
    {
        for (preset = EQ_PRESET_FLAT; preset < EQ_PRESET_COUNT; preset++)
        {
            EQ_STATE st;

            EQ_EvalConfigure(&st, cores[core_index], preset);
            for (section = 0; section < EQ_NUM_BANDS; section++)
            {
                EQ_SECTION_INFO info;
                int pass = Equalizer_GetCoefficientInfo(&st, section, &info);

                if ((EQ_EvalIsBad(info.b0) != 0) ||
                    (EQ_EvalIsBad(info.b1) != 0) ||
                    (EQ_EvalIsBad(info.b2) != 0) ||
                    (EQ_EvalIsBad(info.a1) != 0) ||
                    (EQ_EvalIsBad(info.a2) != 0) ||
                    (info.pole_radius_1 >= 1.0f) ||
                    (info.pole_radius_2 >= 1.0f))
                {
                    pass = 0;
                }
                fprintf(file,
                        "%s,%s,%d,%s,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%s\n",
                        EQ_EvalCoreName(info.core), EQ_EvalPresetName(preset),
                        section, EQ_EvalSectionName(info.type), info.f0_hz,
                        info.gain_db, info.q_or_bw, info.b0, info.b1, info.b2,
                        info.a1, info.a2, info.pole_radius_1,
                        info.pole_radius_2, pass ? "PASS" : "FAIL");
                if (pass == 0)
                {
                    failures++;
                }
            }
        }
    }
    fclose(file);
    return failures;
}

static float EQ_EvalShortPeak(const short *samples, int count)
{
    int index;
    float peak = 0.0f;

    for (index = 0; index < count; index++)
    {
        float value = EQ_EvalAbs((float)samples[index]);

        if (value > peak)
        {
            peak = value;
        }
    }
    return peak;
}

static short EQ_EvalStressTone(int sample_index, float level_dbfs)
{
    int band = (sample_index / 5000) % EQ_NUM_BANDS;
    float amplitude = powf(10.0f, level_dbfs / 20.0f) * 32767.0f;
    float phase = 2.0f * (float)EQ_EVAL_PI *
                  Equalizer_GetBandCenterHz(band) * (float)sample_index /
                  EQ_SAMPLE_RATE;
    float sample = amplitude * sinf(phase);

    return (short)((sample >= 0.0f) ? (sample + 0.5f) : (sample - 0.5f));
}

static int EQ_EvalWritePresetCacheReport(void)
{
    FILE *file;
    EQ_STATE st;
    unsigned long before;
    unsigned long after;
    int preset;
    int pass;

    file = fopen("equalizer_preset_cache_report.csv", "w");
    if (file == 0)
    {
        return 1;
    }
    Equalizer_Init(&st);
    before = EQ_DebugHeadroomScanCount;
    for (preset = EQ_PRESET_FLAT; preset < EQ_PRESET_COUNT; preset++)
    {
        Equalizer_ApplyPreset(&st, preset);
        EQ_EvalSettle(&st);
    }
    Equalizer_SetCoreMode(&st, EQ_CORE_RAW_COPY);
    Equalizer_ApplyPreset(&st, EQ_PRESET_BASS_BOOST);
    Equalizer_SetCoreMode(&st, EQ_CORE_RBJ_CASCADE);
    Equalizer_ApplySingleBand1kPlus3Db(&st);
    EQ_EvalSettle(&st);
    after = EQ_DebugHeadroomScanCount;
    pass = (after == before);
    fprintf(file, "scan_count_before_switches,scan_count_after_switches,pass\n");
    fprintf(file, "%lu,%lu,%s\n", before, after, pass ? "PASS" : "FAIL");
    fclose(file);
    return pass ? 0 : 1;
}

static int EQ_EvalWriteHeadroom(void)
{
    FILE *file;
    const float stress_levels_dbfs[4] = { -12.0f, -6.0f, -3.0f, -1.0f };
    int preset;
    int level_index;
    int failures = 0;

    file = fopen("equalizer_headroom_prediction.csv", "w");
    if (file == 0)
    {
        return 1;
    }
    fprintf(file, "preset,signal_id,input_peak_dbfs,output_peak_dbfs,predicted_peak_db,applied_preamp_db,clip_count,pass\n");
    for (preset = EQ_PRESET_FLAT; preset < EQ_PRESET_COUNT; preset++)
    {
        for (level_index = 0; level_index < 4; level_index++)
        {
            EQ_STATE st;
            float input_peak = 0.0f;
            float output_peak = 0.0f;
            int offset;
            int pass;

            EQ_EvalConfigure(&st, EQ_CORE_RBJ_CASCADE, preset);
            for (offset = 0; offset < 50000; offset += EQ_EVAL_FRAME_SAMPLES)
            {
                int count = 50000 - offset;
                int index;

                if (count > EQ_EVAL_FRAME_SAMPLES)
                {
                    count = EQ_EVAL_FRAME_SAMPLES;
                }
                for (index = 0; index < count; index++)
                {
                    EQ_EvalShortIn[index] = EQ_EvalStressTone(offset + index,
                        stress_levels_dbfs[level_index]);
                }
                Equalizer_ProcessFrame(&st, EQ_EvalShortIn, EQ_EvalShortOut,
                                       count);
                if (EQ_EvalShortPeak(EQ_EvalShortIn, count) > input_peak)
                {
                    input_peak = EQ_EvalShortPeak(EQ_EvalShortIn, count);
                }
                if (EQ_EvalShortPeak(EQ_EvalShortOut, count) > output_peak)
                {
                    output_peak = EQ_EvalShortPeak(EQ_EvalShortOut, count);
                }
            }
            pass = (st.clip_count == 0UL);
            fprintf(file, "%s,stress_%+.0fdBFS,%.9f,%.9f,%.9f,%.9f,%lu,%s\n",
                    EQ_EvalPresetName(preset), stress_levels_dbfs[level_index],
                    EQ_EvalDb(input_peak / 32767.0f),
                    EQ_EvalDb(output_peak / 32767.0f),
                    Equalizer_GetPredictedPeakDb(&st),
                    Equalizer_GetPreampDb(&st), st.clip_count,
                    pass ? "PASS" : "FAIL");
            if (pass == 0)
            {
                failures++;
            }
        }
        {
            EQ_STATE st;
            float input_peak = 0.0f;
            float output_peak = 0.0f;
            int index;
            int pass;

            EQ_EvalConfigure(&st, EQ_CORE_RBJ_CASCADE, preset);
            for (index = 0; index < 7000; index++)
            {
                short input = (index == 0) ? (short)(0.9f * 32767.0f) : 0;
                short output;

                Equalizer_ProcessFrame(&st, &input, &output, 1);
                if (EQ_EvalAbs((float)input) > input_peak)
                {
                    input_peak = EQ_EvalAbs((float)input);
                }
                if (EQ_EvalAbs((float)output) > output_peak)
                {
                    output_peak = EQ_EvalAbs((float)output);
                }
            }
            pass = (st.clip_count == 0UL);
            fprintf(file, "%s,impulse_0.9FS,%.9f,%.9f,%.9f,%.9f,%lu,%s\n",
                    EQ_EvalPresetName(preset),
                    EQ_EvalDb(input_peak / 32767.0f),
                    EQ_EvalDb(output_peak / 32767.0f),
                    Equalizer_GetPredictedPeakDb(&st),
                    Equalizer_GetPreampDb(&st), st.clip_count,
                    pass ? "PASS" : "FAIL");
            if (pass == 0)
            {
                failures++;
            }
        }
    }
    fclose(file);
    return failures;
}

static void EQ_EvalDft(const float *samples, int count, int bin,
                       double *real_out, double *imag_out)
{
    int index;
    double real = 0.0;
    double imag = 0.0;

    for (index = 0; index < count; index++)
    {
        double phase = 2.0 * EQ_EVAL_PI * (double)bin * (double)index /
                       (double)count;

        real += (double)samples[index] * cos(phase);
        imag -= (double)samples[index] * sin(phase);
    }
    *real_out = real;
    *imag_out = imag;
}

static double EQ_EvalBinPower(const float *samples, int count, int bin)
{
    double real;
    double imag;

    EQ_EvalDft(samples, count, bin, &real, &imag);
    return 2.0 * (real * real + imag * imag) /
           ((double)count * (double)count);
}

static int EQ_EvalWriteThdReport(void)
{
    FILE *file;
    int case_index;
    int failures = 0;
    const int cores[5] = { EQ_CORE_RAW_COPY, EQ_CORE_FLOAT_COPY,
                           EQ_CORE_RBJ_CASCADE, EQ_CORE_RBJ_CASCADE,
                           EQ_CORE_RBJ_CASCADE };
    const int presets[5] = { EQ_PRESET_FLAT, EQ_PRESET_FLAT,
                             EQ_PRESET_FLAT,
                             EQ_EVAL_PRESET_SINGLE_1K_PLUS3DB,
                             EQ_PRESET_BASS_BOOST };
    const char *labels[5] = { "raw", "float_copy", "flat",
                              "single_band", "mild_bass" };

    file = fopen("equalizer_thd_report.csv", "w");
    if (file == 0)
    {
        return 1;
    }
    fprintf(file, "case_id,fs_hz,sample_count,fund_bin,freq_hz,coherent,thd_ratio,thd_db,thd_limit_db,thdn_ratio,thdn_db,clip_count,pass\n");
    for (case_index = 0; case_index < 5; case_index++)
    {
        EQ_STATE st;
        int index;
        double fund_power;
        double harmonic_power = 0.0;
        double total_power = 0.0;
        double thd_ratio;
        double thdn_ratio;
        double thd_db;
        int harmonic;
        int pass;

        EQ_EvalConfigure(&st, cores[case_index], presets[case_index]);
        for (index = 0; index < EQ_EVAL_THD_SAMPLES; index++)
        {
            float input = 0.1f * sinf(2.0f * (float)EQ_EVAL_PI *
                                       (float)EQ_EVAL_THD_BIN * (float)index /
                                       (float)EQ_EVAL_THD_SAMPLES);

            Equalizer_ProcessFrameFloat(&st, &input, &EQ_EvalThdOut[index], 1);
            total_power += (double)EQ_EvalThdOut[index] *
                           (double)EQ_EvalThdOut[index];
        }
        total_power /= (double)EQ_EVAL_THD_SAMPLES;
        fund_power = EQ_EvalBinPower(EQ_EvalThdOut, EQ_EVAL_THD_SAMPLES,
                                     EQ_EVAL_THD_BIN);
        for (harmonic = 2;
             harmonic * EQ_EVAL_THD_BIN < (EQ_EVAL_THD_SAMPLES / 2);
             harmonic++)
        {
            harmonic_power += EQ_EvalBinPower(EQ_EvalThdOut,
                                               EQ_EVAL_THD_SAMPLES,
                                               harmonic * EQ_EVAL_THD_BIN);
        }
        thd_ratio = sqrt(harmonic_power / (fund_power + EQ_EVAL_EPS));
        thdn_ratio = sqrt(fmax(0.0, total_power - fund_power) /
                           (fund_power + EQ_EVAL_EPS));
        thd_db = 20.0 * log10(thd_ratio + EQ_EVAL_EPS);
        pass = ((st.clip_count == 0UL) && (fund_power > EQ_EVAL_EPS) &&
                (isfinite(thd_ratio) != 0) && (isfinite(thdn_ratio) != 0) &&
                (thd_db <= EQ_EVAL_THD_MAX_DB));
        fprintf(file, "%s,50000,%d,%d,1000.000,1,%.12f,%.9f,%.1f,%.12f,%.9f,%lu,%s\n",
                labels[case_index], EQ_EVAL_THD_SAMPLES, EQ_EVAL_THD_BIN,
                thd_ratio, thd_db, EQ_EVAL_THD_MAX_DB, thdn_ratio,
                20.0 * log10(thdn_ratio + EQ_EVAL_EPS), st.clip_count,
                pass ? "PASS" : "FAIL");
        if (pass == 0)
        {
            failures++;
        }
    }
    fclose(file);
    return failures;
}

static int EQ_EvalWriteExpectedClipReport(void)
{
    FILE *file;
    EQ_STATE st;
    int index;
    int total = 5000;
    int failures = 0;

    file = fopen("equalizer_expected_clip_report.csv", "w");
    if (file == 0)
    {
        return 1;
    }
    EQ_EvalConfigure(&st, EQ_CORE_LEGACY, EQ_PRESET_V_SHAPE);
    for (index = 0; index < total; index++)
    {
        float sample = 32767.0f * sinf(2.0f * (float)EQ_EVAL_PI * 31.25f *
                                        (float)index / EQ_SAMPLE_RATE);
        short input = (short)sample;
        short output;

        Equalizer_ProcessFrame(&st, &input, &output, 1);
    }
    fprintf(file, "test_name,core,preset,clip_count,pass\n");
    fprintf(file, "expected_clip_test,legacy,v_shape,%lu,%s\n", st.clip_count,
            (st.clip_count > 0UL) ? "PASS" : "FAIL");
    if (st.clip_count == 0UL)
    {
        failures++;
    }
    fclose(file);
    return failures;
}

static float EQ_EvalTriangle(int sample_index, int period)
{
    int phase = sample_index % period;
    float normalized = (float)phase / (float)period;

    return (normalized < 0.5f) ? (4.0f * normalized - 1.0f) :
                                  (3.0f - 4.0f * normalized);
}

static short EQ_EvalLongSignal(int signal_id, int sample_index,
                               unsigned long *random_state, float *pink)
{
    float value;

    *random_state = *random_state * 1664525UL + 1013904223UL;
    if (signal_id == 0)
    {
        value = ((float)((*random_state >> 8) & 0xffffUL) / 32767.5f - 1.0f) *
                0.10f;
    }
    else if (signal_id == 1)
    {
        float white = ((float)((*random_state >> 8) & 0xffffUL) / 32767.5f -
                       1.0f) * 0.35f;

        *pink = 0.985f * *pink + 0.015f * white;
        value = *pink;
    }
    else if (signal_id == 2)
    {
        value = 0.04f * (EQ_EvalTriangle(sample_index, 800) +
                         EQ_EvalTriangle(sample_index, 50) +
                         EQ_EvalTriangle(sample_index, 8));
    }
    else if (signal_id == 3)
    {
        float envelope = 0.25f + 0.75f *
                         (EQ_EvalTriangle(sample_index, 10000) + 1.0f) * 0.5f;

        value = envelope * 0.07f *
                (EQ_EvalTriangle(sample_index, 400) +
                 0.5f * EQ_EvalTriangle(sample_index, 200) +
                 0.25f * EQ_EvalTriangle(sample_index, 133));
    }
    else
    {
        float envelope = 0.45f + 0.55f *
                         (EQ_EvalTriangle(sample_index, 16000) + 1.0f) * 0.5f;

        value = envelope * 0.06f *
                (EQ_EvalTriangle(sample_index, 320) +
                 EQ_EvalTriangle(sample_index, 73) +
                 0.5f * EQ_EvalTriangle(sample_index, 29));
    }
    if (value > 0.95f)
    {
        value = 0.95f;
    }
    if (value < -0.95f)
    {
        value = -0.95f;
    }
    return (short)(value * 32767.0f);
}

static int EQ_EvalStateIsFinite(const EQ_STATE *st, float *max_state)
{
    int section;
    int pass = 1;

    *max_state = 0.0f;
    for (section = 0; section < EQ_NUM_BANDS; section++)
    {
        float values[8];
        int item;

        values[0] = st->legacy_x1[section];
        values[1] = st->legacy_x2[section];
        values[2] = st->legacy_y1[section];
        values[3] = st->legacy_y2[section];
        values[4] = st->active_bank.s1[section];
        values[5] = st->active_bank.s2[section];
        values[6] = st->pending_bank.s1[section];
        values[7] = st->pending_bank.s2[section];
        for (item = 0; item < 8; item++)
        {
            if (EQ_EvalIsBad(values[item]) != 0)
            {
                pass = 0;
            }
            if (EQ_EvalAbs(values[item]) > *max_state)
            {
                *max_state = EQ_EvalAbs(values[item]);
            }
        }
    }
    return pass;
}

static int EQ_EvalWriteLongStabilityReport(void)
{
    FILE *file;
    int signal_id;
    int failures = 0;
    const char *signal_name[5] = { "white", "pink", "multitone",
                                   "speech_like", "music_like" };

    file = fopen("equalizer_long_stability_report.csv", "w");
    if (file == 0)
    {
        return 1;
    }
    fprintf(file, "signal,sample_count,max_output_peak,max_internal_state,clip_count,nan_inf,pass\n");
    for (signal_id = 0; signal_id < 5; signal_id++)
    {
        EQ_STATE st;
        unsigned long random_state = 0x33a5UL + (unsigned long)signal_id;
        float pink = 0.0f;
        float max_output = 0.0f;
        float max_state;
        int offset;
        int finite;
        int pass;

        EQ_EvalConfigure(&st, EQ_CORE_RBJ_CASCADE, EQ_PRESET_V_SHAPE);
        for (offset = 0; offset < EQ_EVAL_LONG_SAMPLES;
             offset += EQ_EVAL_FRAME_SAMPLES)
        {
            int count = EQ_EVAL_LONG_SAMPLES - offset;
            int index;

            if (count > EQ_EVAL_FRAME_SAMPLES)
            {
                count = EQ_EVAL_FRAME_SAMPLES;
            }
            for (index = 0; index < count; index++)
            {
                EQ_EvalShortIn[index] = EQ_EvalLongSignal(signal_id,
                    offset + index, &random_state, &pink);
            }
            Equalizer_ProcessFrame(&st, EQ_EvalShortIn, EQ_EvalShortOut, count);
            if (EQ_EvalShortPeak(EQ_EvalShortOut, count) > max_output)
            {
                max_output = EQ_EvalShortPeak(EQ_EvalShortOut, count);
            }
        }
        finite = EQ_EvalStateIsFinite(&st, &max_state);
        pass = ((finite != 0) && (st.clip_count == 0UL) &&
                (max_output <= 32767.0f));
        fprintf(file, "%s,%d,%.6f,%.6f,%lu,%d,%s\n",
                signal_name[signal_id], EQ_EVAL_LONG_SAMPLES, max_output,
                max_state, st.clip_count, (finite != 0) ? 0 : 1,
                pass ? "PASS" : "FAIL");
        if (pass == 0)
        {
            failures++;
        }
    }
    fclose(file);
    return failures;
}

static int EQ_EvalWriteTransitionReport(void)
{
    FILE *file;
    const int source_presets[3] = { EQ_PRESET_FLAT, EQ_PRESET_BASS_BOOST,
                                    EQ_PRESET_V_SHAPE };
    const int target_presets[3] = { EQ_PRESET_BASS_BOOST, EQ_PRESET_FLAT,
                                    EQ_PRESET_FLAT };
    const char *labels[3] = { "flat_to_bass", "bass_to_flat",
                              "v_shape_to_flat" };
    int case_index;
    int failures = 0;

    file = fopen("equalizer_transition_report.csv", "w");
    if (file == 0)
    {
        return 1;
    }
    fprintf(file, "transition,baseline_max_sample_delta,switch_max_sample_delta,delta_ratio,transition_peak,transition_samples,flat_transparent_after,clip_count,pass\n");
    for (case_index = 0; case_index < 3; case_index++)
    {
        EQ_STATE st;
        float baseline_delta = 0.0f;
        float switch_delta = 0.0f;
        float transition_peak = 0.0f;
        float max_internal_state = 0.0f;
        float previous = 0.0f;
        int first = 1;
        int initial_remaining;
        int flat_transparent_after = 1;
        int index;
        int pass;

        EQ_EvalConfigure(&st, EQ_CORE_RBJ_CASCADE, source_presets[case_index]);
        for (index = 0; index < 5000; index++)
        {
            float input = 0.10f * sinf(2.0f * (float)EQ_EVAL_PI * 1000.0f *
                                        (float)index / EQ_SAMPLE_RATE);
            float output;

            Equalizer_ProcessFrameFloat(&st, &input, &output, 1);
            if ((first == 0) && (EQ_EvalAbs(output - previous) > baseline_delta))
            {
                baseline_delta = EQ_EvalAbs(output - previous);
            }
            previous = output;
            first = 0;
        }
        Equalizer_ApplyPreset(&st, target_presets[case_index]);
        initial_remaining = Equalizer_GetTransitionRemaining(&st);
        for (index = 0; index < EQ_EVAL_SETTLE_SAMPLES; index++)
        {
            float input = 0.10f * sinf(2.0f * (float)EQ_EVAL_PI * 1000.0f *
                                        (float)(index + 5000) / EQ_SAMPLE_RATE);
            float output;
            int in_transition = Equalizer_HasPendingTransition(&st);

            Equalizer_ProcessFrameFloat(&st, &input, &output, 1);
            if (in_transition != 0)
            {
                if (EQ_EvalAbs(output) > transition_peak)
                {
                    transition_peak = EQ_EvalAbs(output);
                }
            }
            if (EQ_EvalAbs(output - previous) > switch_delta)
            {
                switch_delta = EQ_EvalAbs(output - previous);
            }
            previous = output;
        }
        if (target_presets[case_index] == EQ_PRESET_FLAT)
        {
            EQ_EvalFillTransparencySignal(2, EQ_EVAL_FRAME_SAMPLES);
            Equalizer_ProcessFrame(&st, EQ_EvalShortIn, EQ_EvalShortOut,
                                   EQ_EVAL_FRAME_SAMPLES);
            for (index = 0; index < EQ_EVAL_FRAME_SAMPLES; index++)
            {
                if (EQ_EvalShortIn[index] != EQ_EvalShortOut[index])
                {
                    flat_transparent_after = 0;
                    break;
                }
            }
        }
        pass = ((st.clip_count == 0UL) &&
                (EQ_EvalStateIsFinite(&st, &max_internal_state) != 0) &&
                (initial_remaining == (int)(EQ_SAMPLE_RATE * EQ_TRANSITION_MS /
                                              1000.0f + 0.5f)) &&
                (switch_delta <= (baseline_delta * 8.0f + 0.02f)) &&
                (Equalizer_HasPendingTransition(&st) == 0) &&
                (flat_transparent_after != 0));
        fprintf(file, "%s,%.12f,%.12f,%.9f,%.12f,%d,%d,%lu,%s\n",
                labels[case_index], baseline_delta, switch_delta,
                switch_delta / (baseline_delta + 1.0e-12f), transition_peak,
                initial_remaining, flat_transparent_after, st.clip_count,
                pass ? "PASS" : "FAIL");
        if (pass == 0)
        {
            failures++;
        }
    }
    fclose(file);
    return failures;
}

int EqualizerEval_OfflineTest_All(void)
{
    int failures = 0;

    failures += EQ_EvalWriteTransparencyReport();
    failures += EQ_EvalWriteBandResponse();
    failures += EQ_EvalWriteLegacySystemResponse();
    failures += EQ_EvalWriteSystemResponse();
    failures += EQ_EvalWriteCoefficients();
    failures += EQ_EvalWritePresetCacheReport();
    failures += EQ_EvalWriteHeadroom();
    failures += EQ_EvalWriteThdReport();
    failures += EQ_EvalWriteExpectedClipReport();
    failures += EQ_EvalWriteLongStabilityReport();
    failures += EQ_EvalWriteTransitionReport();
    printf("EqualizerEval_OfflineTest_All failures=%d\n", failures);
    return failures;
}

#ifdef EQUALIZER_TEST_MAIN
int main(void)
{
    return (EqualizerEval_OfflineTest_All() == 0) ? 0 : 1;
}
#endif

#else

int EqualizerEval_OfflineTest_All(void)
{
    return -1;
}

#endif /* EQ_ALGO_ONLY */
