/**
 * user_equalizer_eval.c
 *
 * Offline evaluation for Project 3.3 graphic equalizer.
 */

#include "user_equalizer_eval.h"

#ifdef EQ_ALGO_ONLY

#include "user_equalizer.h"
#include "math.h"
#include "stdio.h"
#include "string.h"

#define EQ_EVAL_PI 3.14159265358979323846f
#define EQ_EVAL_EPS 1.0e-20f
#define EQ_EVAL_MEASURE_SAMPLES 4096
#define EQ_EVAL_WARMUP_SAMPLES 12000
#define EQ_EVAL_BUFFER_SAMPLES 4096

static float EQ_EvalBuffer[EQ_EVAL_BUFFER_SAMPLES];

static const int EQ_EvalPresets[5] =
{
    EQ_PRESET_FLAT,
    EQ_PRESET_BASS_BOOST,
    EQ_PRESET_VOCAL,
    EQ_PRESET_TREBLE_BOOST,
    EQ_PRESET_V_SHAPE
};

static const char *EQ_EvalPresetNames[5] =
{
    "flat",
    "bass_boost",
    "vocal",
    "treble_boost",
    "v_shape"
};

static int EQ_EvalIsBad(float x)
{
    if (x != x)
    {
        return 1;
    }
    if ((x > 1.0e30f) || (x < -1.0e30f))
    {
        return 1;
    }
    return 0;
}

static float EQ_EvalResponseDb(int preset, float freq_hz, float amplitude)
{
    EQ_STATE st;
    int i;
    int total;
    float phase;
    float step;
    double in_energy;
    double out_energy;

    Equalizer_Init(&st);
    Equalizer_ApplyPreset(&st, preset);
    total = EQ_EVAL_WARMUP_SAMPLES + EQ_EVAL_MEASURE_SAMPLES;
    phase = 0.0f;
    step = (2.0f * EQ_EVAL_PI * freq_hz) / EQ_SAMPLE_RATE;
    in_energy = 0.0;
    out_energy = 0.0;

    for (i = 0; i < total; i++)
    {
        float x;
        float y;

        x = amplitude * sinf(phase);
        phase += step;
        if (phase > (2.0f * EQ_EVAL_PI))
        {
            phase -= 2.0f * EQ_EVAL_PI;
        }
        Equalizer_ProcessFrameFloat(&st, &x, &y, 1);
        if (i >= EQ_EVAL_WARMUP_SAMPLES)
        {
            in_energy += (double)x * (double)x;
            out_energy += (double)y * (double)y;
        }
    }

    if (in_energy <= 0.0)
    {
        return -120.0f;
    }
    return (float)(10.0 * log10((out_energy + EQ_EVAL_EPS) /
                                (in_energy + EQ_EVAL_EPS)));
}

static int EQ_EvalWriteBandResponse(void)
{
    FILE *fp;
    int band;
    int freq_index;
    int fail;

    fp = fopen("equalizer_band_response.csv", "w");
    if (fp == 0)
    {
        printf("equalizer_band_response.csv open FAIL\n");
        return 1;
    }

    fprintf(fp, "band,band_center_hz,test_freq_hz,magnitude_db\n");
    fail = 0;
    for (band = 0; band < EQ_NUM_BANDS; band++)
    {
        float center;
        float center_mag;
        float best_mag;

        center = Equalizer_GetBandCenterHz(band);
        center_mag = Equalizer_GetBandMagnitudeDb(band, center);
        best_mag = center_mag;
        if ((center_mag < -12.0f) || (center_mag > 1.5f) ||
            (EQ_EvalIsBad(center_mag) != 0))
        {
            fail = 1;
        }
        for (freq_index = 0; freq_index < EQ_NUM_BANDS; freq_index++)
        {
            float freq;
            float mag_db;

            freq = Equalizer_GetBandCenterHz(freq_index);
            mag_db = Equalizer_GetBandMagnitudeDb(band, freq);
            fprintf(fp, "%d,%.3f,%.3f,%.6f\n",
                    band, center, freq, mag_db);
            if (EQ_EvalIsBad(mag_db) != 0)
            {
                fail = 1;
            }
            if (mag_db > best_mag)
            {
                best_mag = mag_db;
            }
        }
        if ((best_mag - center_mag) > 0.5f)
        {
            fail = 1;
        }
    }

    fclose(fp);
    printf("equalizer_band_response.csv %s\n", fail ? "FAIL" : "PASS");
    return fail;
}

static int EQ_EvalWriteSystemResponse(void)
{
    FILE *fp;
    int preset_index;
    int freq_index;
    int fail;

    fp = fopen("equalizer_system_response.csv", "w");
    if (fp == 0)
    {
        printf("equalizer_system_response.csv open FAIL\n");
        return 1;
    }

    fprintf(fp, "preset,test_freq_hz,response_db\n");
    fail = 0;
    for (preset_index = 0; preset_index < 5; preset_index++)
    {
        for (freq_index = 0; freq_index < EQ_NUM_BANDS; freq_index++)
        {
            float freq;
            float response_db;

            freq = Equalizer_GetBandCenterHz(freq_index);
            response_db = EQ_EvalResponseDb(EQ_EvalPresets[preset_index],
                                            freq, 12000.0f);
            fprintf(fp, "%s,%.3f,%.6f\n",
                    EQ_EvalPresetNames[preset_index], freq, response_db);
            if ((EQ_EvalIsBad(response_db) != 0) ||
                (response_db < -60.0f) || (response_db > 30.0f))
            {
                fail = 1;
            }
        }
    }

    fclose(fp);
    printf("equalizer_system_response.csv %s\n", fail ? "FAIL" : "PASS");
    return fail;
}

static void EQ_EvalFillSettledOutput(EQ_STATE *st, float freq_hz,
                                     float amplitude)
{
    int i;
    int total;
    int out_index;
    float phase;
    float step;

    total = EQ_EVAL_WARMUP_SAMPLES + EQ_EVAL_BUFFER_SAMPLES;
    out_index = 0;
    phase = 0.0f;
    step = (2.0f * EQ_EVAL_PI * freq_hz) / EQ_SAMPLE_RATE;

    for (i = 0; i < total; i++)
    {
        float x;
        float y;

        x = amplitude * sinf(phase);
        phase += step;
        if (phase > (2.0f * EQ_EVAL_PI))
        {
            phase -= 2.0f * EQ_EVAL_PI;
        }
        Equalizer_ProcessFrameFloat(st, &x, &y, 1);
        if (i >= EQ_EVAL_WARMUP_SAMPLES)
        {
            EQ_EvalBuffer[out_index] = y;
            out_index++;
        }
    }
}

static double EQ_EvalDftPower(const float *x, int n, float freq_hz)
{
    int i;
    double re;
    double im;
    double step;
    double phase;

    re = 0.0;
    im = 0.0;
    step = (2.0 * 3.14159265358979323846 * (double)freq_hz) /
           (double)EQ_SAMPLE_RATE;
    phase = 0.0;
    for (i = 0; i < n; i++)
    {
        double v;

        v = (double)x[i];
        re += v * cos(phase);
        im -= v * sin(phase);
        phase += step;
    }
    return re * re + im * im;
}

static unsigned long EQ_EvalRunClipCase(void)
{
    EQ_STATE st;
    short in[EQ_FRAME_LEN];
    short out[EQ_FRAME_LEN];
    int frame;
    int i;

    Equalizer_Init(&st);
    Equalizer_ApplyPreset(&st, EQ_PRESET_V_SHAPE);
    memset(out, 0, sizeof(out));

    for (frame = 0; frame < 16; frame++)
    {
        for (i = 0; i < EQ_FRAME_LEN; i++)
        {
            float t;
            float x;

            t = (float)(frame * EQ_FRAME_LEN + i) / EQ_SAMPLE_RATE;
            x = 24000.0f * sinf(2.0f * EQ_EVAL_PI * 62.5f * t) +
                20000.0f * sinf(2.0f * EQ_EVAL_PI * 8000.0f * t);
            if (x > 32767.0f)
            {
                x = 32767.0f;
            }
            if (x < -32768.0f)
            {
                x = -32768.0f;
            }
            in[i] = (short)x;
        }
        Equalizer_ProcessFrame(&st, in, out, EQ_FRAME_LEN);
    }
    return st.clip_count;
}

static int EQ_EvalWriteThdReport(void)
{
    FILE *fp;
    EQ_STATE st;
    double fund;
    double harm;
    double thd;
    double thd_db;
    int h;
    int fail;
    unsigned long clips;

    fp = fopen("equalizer_thd_report.csv", "w");
    if (fp == 0)
    {
        printf("equalizer_thd_report.csv open FAIL\n");
        return 1;
    }

    Equalizer_Init(&st);
    Equalizer_ApplyPreset(&st, EQ_PRESET_FLAT);
    EQ_EvalFillSettledOutput(&st, 1000.0f, 10000.0f);
    fund = EQ_EvalDftPower(EQ_EvalBuffer, EQ_EVAL_BUFFER_SAMPLES, 1000.0f);
    harm = 0.0;
    for (h = 2; h <= 5; h++)
    {
        harm += EQ_EvalDftPower(EQ_EvalBuffer, EQ_EVAL_BUFFER_SAMPLES,
                                1000.0f * (float)h);
    }
    thd = sqrt(harm / (fund + EQ_EVAL_EPS));
    thd_db = 20.0 * log10(thd + EQ_EVAL_EPS);
    clips = EQ_EvalRunClipCase();

    fprintf(fp, "test_freq_hz,thd_ratio,thd_db,clip_count\n");
    fprintf(fp, "1000.000,%.12f,%.6f,%lu\n", thd, thd_db, clips);
    fclose(fp);

    fail = 0;
    if ((EQ_EvalIsBad((float)thd_db) != 0) || (thd_db > -50.0))
    {
        fail = 1;
    }
    printf("equalizer_thd_report.csv thd_db=%.3f clip_count=%lu %s\n",
           thd_db, clips, fail ? "FAIL" : "PASS");
    return fail;
}

static int EQ_EvalWriteGroupDelayReport(void)
{
    FILE *fp;
    EQ_STATE st;
    int i;
    int peak_index;
    float peak_abs;
    int impulse_index;
    int delay;
    int fail;

    fp = fopen("equalizer_group_delay_report.csv", "w");
    if (fp == 0)
    {
        printf("equalizer_group_delay_report.csv open FAIL\n");
        return 1;
    }

    Equalizer_Init(&st);
    Equalizer_ApplyPreset(&st, EQ_PRESET_FLAT);
    memset(EQ_EvalBuffer, 0, sizeof(EQ_EvalBuffer));
    impulse_index = 32;
    peak_index = impulse_index;
    peak_abs = 0.0f;
    for (i = 0; i < EQ_EVAL_BUFFER_SAMPLES; i++)
    {
        float x;
        float y;
        float ay;

        x = (i == impulse_index) ? 20000.0f : 0.0f;
        Equalizer_ProcessFrameFloat(&st, &x, &y, 1);
        EQ_EvalBuffer[i] = y;
        ay = (y < 0.0f) ? -y : y;
        if (ay > peak_abs)
        {
            peak_abs = ay;
            peak_index = i;
        }
    }
    delay = peak_index - impulse_index;

    fprintf(fp, "impulse_index,peak_index,estimated_delay_samples,peak_abs\n");
    fprintf(fp, "%d,%d,%d,%.6f\n",
            impulse_index, peak_index, delay, peak_abs);
    fclose(fp);

    fail = ((delay < 0) || (delay > 512) || (peak_abs <= 0.0f)) ? 1 : 0;
    printf("equalizer_group_delay_report.csv delay=%d peak=%.3f %s\n",
           delay, peak_abs, fail ? "FAIL" : "PASS");
    return fail;
}

int EqualizerEval_OfflineTest_All(void)
{
    int failures;

    failures = 0;
    failures += EQ_EvalWriteBandResponse();
    failures += EQ_EvalWriteSystemResponse();
    failures += EQ_EvalWriteThdReport();
    failures += EQ_EvalWriteGroupDelayReport();
    printf("EqualizerEval_OfflineTest_All failures=%d\n", failures);
    return failures;
}

#ifdef EQUALIZER_TEST_MAIN
int main(void)
{
    return EqualizerEval_OfflineTest_All();
}
#endif

#else

int EqualizerEval_OfflineTest_All(void)
{
    return 0;
}

#endif
