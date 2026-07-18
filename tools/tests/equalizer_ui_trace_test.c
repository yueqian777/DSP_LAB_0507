#include "user_equalizer_display.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PREVIEW_COUNT 10

static const char * const s_preview_names[PREVIEW_COUNT] =
{
    "dynamic_status_initial",
    "dynamic_status_all_armed",
    "dynamic_status_all_active",
    "eq_editor_flat",
    "eq_editor_vocal",
    "eq_editor_draft_125hz_plus2",
    "eq_editor_building_custom",
    "eq_editor_custom_applied",
    "eq_editor_gain_limits",
    "page_transition_intermediate"
};

static void make_snapshot(EQ_UI_SNAPSHOT *snapshot)
{
    int band;

    memset(snapshot, 0, sizeof(*snapshot));
    snapshot->applied_preset = EQ_PRESET_FLAT;
    snapshot->smart_strength = 2;
    snapshot->clarity_strength = 2;
    snapshot->guard_strength = 2;
    snapshot->analyzer_valid = 1;
    snapshot->analyzer_warm = 1;
    for (band = 0; band < EQ_UI_ANALYZER_COUNT; band++)
    {
        snapshot->band_pixel[band] = EqualizerUi_DbTenthsToPixel(0);
    }
    snapshot->page = EQ_UI_PAGE_DYNAMIC_STATUS;
    snapshot->editor_selected_band = 0;
    snapshot->editor_apply_status = EQ_UI_APPLY_APPLIED;
    snapshot->editor_submitted_valid = 1U;
}

static void set_editor_preset(EQ_UI_SNAPSHOT *snapshot, int preset)
{
    float gains_db[EQ_NUM_BANDS];
    int band;

    if (Equalizer_CopyPresetGainsDb(preset, gains_db) == 0)
    {
        return;
    }
    snapshot->applied_preset = preset;
    for (band = 0; band < EQ_NUM_BANDS; band++)
    {
        EQ_UI_GAIN_HALF_DB gain;

        gain = EqualizerUi_GainDbToHalf(gains_db[band]);
        snapshot->applied_gain_half_db[band] = gain;
        snapshot->draft_gain_half_db[band] = gain;
        snapshot->submitted_gain_half_db[band] = gain;
    }
}

static int flush_jobs(unsigned long frame, int maximum)
{
    int completed;
    int attempts;

    completed = 0;
    for (attempts = 0; attempts < 10000; attempts++)
    {
        int job;

        if (!EqualizerDisplay_HasPendingJob() || (completed >= maximum))
        {
            break;
        }
        job = EqualizerDisplay_ServiceOneJob(frame + (unsigned long)attempts);
        if (job != EQ_LCD_JOB_NONE)
        {
            completed++;
        }
    }
    return completed;
}

static void write_gain_array(FILE *output,
                             const EQ_UI_GAIN_HALF_DB gains[EQ_NUM_BANDS])
{
    int band;

    fputc('[', output);
    for (band = 0; band < EQ_NUM_BANDS; band++)
    {
        if (band != 0)
        {
            fputc(',', output);
        }
        fprintf(output, "%d", (int)gains[band]);
    }
    fputc(']', output);
}

static int write_source_metadata(const char *path, const char *commit,
                                 const char *name,
                                 const EQ_UI_SNAPSHOT *snapshot,
                                 int completed_jobs)
{
    FILE *output;

    output = fopen(path, "w");
    if (output == 0)
    {
        return 0;
    }
    fprintf(output,
            "{\n  \"source_commit\": \"%s\",\n"
            "  \"preview\": \"%s\",\n"
            "  \"requested_page\": %d,\n"
            "  \"displayed_page\": %d,\n"
            "  \"page_building\": %d,\n"
            "  \"snapshot\": {\"applied_preset\": %d, "
            "\"selected_band\": %d, \"apply_status\": %d},\n"
            "  \"applied_gain_half_db\": ",
            commit, name, snapshot->page,
            EqualizerDisplay_GetDisplayedPage(),
            EqualizerDisplay_IsPageBuilding(),
            snapshot->applied_preset,
            snapshot->editor_selected_band,
            snapshot->editor_apply_status);
    write_gain_array(output, snapshot->applied_gain_half_db);
    fputs(",\n  \"draft_gain_half_db\": ", output);
    write_gain_array(output, snapshot->draft_gain_half_db);
    fprintf(output,
            ",\n  \"pending_jobs\": %d,\n"
            "  \"completed_jobs\": %d,\n"
            "  \"bounds_failures\": %lu,\n"
            "  \"primitive_count\": %lu,\n"
            "  \"trace_record_count\": %lu\n}\n",
            EqualizerDisplay_HasPendingJob(), completed_jobs,
            EQ_DebugLcdBoundsFailureCount,
            EqualizerDisplay_TestPrimitiveCount(),
            EqualizerDisplay_TestTraceRecordCount());
    fclose(output);
    return 1;
}

static int render_preview(const char *directory, const char *commit,
                          int preview_index)
{
    EQ_UI_SNAPSHOT snapshot;
    char trace_path[512];
    char metadata_path[512];
    const char *name;
    int completed_jobs;
    int band;

    name = s_preview_names[preview_index];
    snprintf(trace_path, sizeof(trace_path), "%s/%s.jsonl",
             directory, name);
    snprintf(metadata_path, sizeof(metadata_path), "%s/%s.source.json",
             directory, name);
    make_snapshot(&snapshot);
    if (preview_index == 1)
    {
        snapshot.smart_enabled = 1;
        snapshot.clarity_enabled = 1;
        snapshot.guard_enabled = 1;
    }
    else if (preview_index == 2)
    {
        snapshot.smart_enabled = 1;
        snapshot.clarity_enabled = 1;
        snapshot.guard_enabled = 1;
        snapshot.smart_level = 3;
        snapshot.clarity_level = 3;
        snapshot.guard_level = 3;
    }
    else if (preview_index >= 3)
    {
        snapshot.page = EQ_UI_PAGE_EQ_EDITOR;
    }
    if (preview_index == 4)
    {
        set_editor_preset(&snapshot, EQ_PRESET_VOCAL);
    }
    else if (preview_index == 5)
    {
        snapshot.editor_selected_band = 2;
        snapshot.draft_gain_half_db[2] = 4;
        snapshot.editor_draft_dirty = 1U;
        snapshot.editor_draft_version = 1UL;
        snapshot.editor_apply_status = EQ_UI_APPLY_EDITING;
    }
    else if (preview_index == 6)
    {
        snapshot.editor_selected_band = 2;
        snapshot.draft_gain_half_db[2] = 4;
        snapshot.submitted_gain_half_db[2] = 4;
        snapshot.editor_draft_dirty = 1U;
        snapshot.editor_draft_version = 1UL;
        snapshot.editor_submitted_sequence = 2U;
        snapshot.editor_apply_status = EQ_UI_APPLY_BUILDING;
    }
    else if (preview_index == 7)
    {
        static const int custom[EQ_NUM_BANDS] =
        {
            6, 4, 2, 0, -2, -2, 0, 2, 4, 6
        };

        snapshot.applied_preset = EQ_PRESET_CUSTOM;
        snapshot.editor_applied_sequence = 2U;
        snapshot.editor_submitted_sequence = 2U;
        for (band = 0; band < EQ_NUM_BANDS; band++)
        {
            snapshot.applied_gain_half_db[band] =
                (EQ_UI_GAIN_HALF_DB)custom[band];
            snapshot.draft_gain_half_db[band] =
                (EQ_UI_GAIN_HALF_DB)custom[band];
            snapshot.submitted_gain_half_db[band] =
                (EQ_UI_GAIN_HALF_DB)custom[band];
        }
    }
    else if (preview_index == 8)
    {
        snapshot.applied_preset = EQ_PRESET_CUSTOM;
        for (band = 0; band < EQ_NUM_BANDS; band++)
        {
            EQ_UI_GAIN_HALF_DB gain;

            gain = (EQ_UI_GAIN_HALF_DB)((band & 1) ?
                EQ_UI_GAIN_HALF_DB_MIN : EQ_UI_GAIN_HALF_DB_MAX);
            snapshot.applied_gain_half_db[band] = gain;
            snapshot.draft_gain_half_db[band] = gain;
            snapshot.submitted_gain_half_db[band] = gain;
        }
    }

    EqualizerDisplay_Init();
    EQ_DebugLcdRuntimeMask = EQ_UI_RUNTIME_ALL;
    if (!EqualizerDisplay_TestTraceOpen(trace_path))
    {
        return 0;
    }
    if (EqualizerDisplay_DrawStaticLayout() != 1)
    {
        EqualizerDisplay_TestTraceClose();
        return 0;
    }
    EqualizerDisplay_BeginRuntime();
    EqualizerDisplay_RequestSnapshot(&snapshot, 10UL);
    completed_jobs = flush_jobs(10UL,
        (preview_index == 9) ? 9 : 1000);
    EqualizerDisplay_TestTraceClose();
    if (EQ_DebugLcdBoundsFailureCount != 0UL)
    {
        return 0;
    }
    return write_source_metadata(metadata_path, commit, name,
                                 &snapshot, completed_jobs);
}

int main(int argc, char **argv)
{
    const char *commit;
    int preview;
    int failures;

    if (argc != 3)
    {
        fprintf(stderr, "usage: %s OUTPUT_DIR COMMIT\n", argv[0]);
        return 2;
    }
    commit = argv[2];
    failures = 0;
    for (preview = 0; preview < PREVIEW_COUNT; preview++)
    {
        if (!render_preview(argv[1], commit, preview))
        {
            fprintf(stderr, "preview failed: %s\n",
                    s_preview_names[preview]);
            failures++;
        }
    }
    printf("equalizer_ui_trace previews=%d failures=%d\n",
           PREVIEW_COUNT, failures);
    return (failures == 0) ? 0 : 1;
}
