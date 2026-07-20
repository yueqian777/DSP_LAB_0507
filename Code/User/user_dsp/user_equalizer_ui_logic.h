/**
 * user_equalizer_ui_logic.h
 *
 * Hardware-independent Project 3.3 UI state, layout, and touch logic.
 */

#ifndef _USER_EQUALIZER_UI_LOGIC_H_
#define _USER_EQUALIZER_UI_LOGIC_H_

#include "user_equalizer.h"

#ifndef EQ_ENABLE_TEN_BAND_EDITOR
#define EQ_ENABLE_TEN_BAND_EDITOR 0
#endif

#ifndef EQ_ENABLE_TEN_BAND_EDITOR_TOUCH
#define EQ_ENABLE_TEN_BAND_EDITOR_TOUCH 0
#endif

#define EQ_UI_SCREEN_WIDTH  800
#define EQ_UI_SCREEN_HEIGHT 480

#define EQ_UI_PRESET_COUNT  5
#define EQ_UI_CHAIN_COUNT   3
#define EQ_UI_ANALYZER_COUNT 4
#define EQ_UI_DYNAMIC_COUNT 3
#define EQ_UI_DYNAMIC_HITBOX_COUNT 12
#if EQ_ENABLE_TEN_BAND_EDITOR != 0
#define EQ_UI_EDITOR_HITBOX_COUNT 20
#define EQ_UI_HITBOX_COUNT EQ_UI_EDITOR_HITBOX_COUNT
#else
#define EQ_UI_HITBOX_COUNT EQ_UI_DYNAMIC_HITBOX_COUNT
#endif

#define EQ_UI_RUNTIME_PRESET   0x01U
#define EQ_UI_RUNTIME_CHAIN    0x02U
#define EQ_UI_RUNTIME_DYNAMICS 0x04U
#define EQ_UI_RUNTIME_ANALYZER 0x08U
#define EQ_UI_RUNTIME_EDITOR   0x10U
#define EQ_UI_RUNTIME_PAGE     0x20U
#if EQ_ENABLE_TEN_BAND_EDITOR != 0
#define EQ_UI_RUNTIME_ALL      0x3FU
#else
#define EQ_UI_RUNTIME_ALL      0x0FU
#endif

#define EQ_UI_JOB_NONE       0
#define EQ_UI_JOB_PRESET_0   1
#define EQ_UI_JOB_PRESET_4   (EQ_UI_JOB_PRESET_0 + 4)
#define EQ_UI_JOB_CHAIN_0    6
#define EQ_UI_JOB_CHAIN_2    (EQ_UI_JOB_CHAIN_0 + 2)
#define EQ_UI_JOB_ANALYZER_0 9
#define EQ_UI_JOB_ANALYZER_3 (EQ_UI_JOB_ANALYZER_0 + 3)
#define EQ_UI_JOB_DYNAMIC_0  13
#define EQ_UI_JOB_DYNAMIC_2  (EQ_UI_JOB_DYNAMIC_0 + 2)
#if EQ_ENABLE_TEN_BAND_EDITOR != 0
#define EQ_UI_JOB_EDITOR_BAND_0 16
#define EQ_UI_JOB_EDITOR_BAND_9 (EQ_UI_JOB_EDITOR_BAND_0 + 9)
#define EQ_UI_JOB_EDITOR_FIELDS 26
#define EQ_UI_JOB_PAGE_TILE     27
#define EQ_UI_JOB_COUNT         27
#define EQ_UI_CATEGORY_COUNT    6
#else
#define EQ_UI_JOB_COUNT      15
#define EQ_UI_CATEGORY_COUNT 4
#endif

typedef char EQ_UI_JOB_COUNT_MUST_FIT_MASK[
    (EQ_UI_JOB_COUNT <= 32) ? 1 : -1];

#define EQ_UI_DYNAMIC_FIELD_ENABLED  0x01U
#define EQ_UI_DYNAMIC_FIELD_STRENGTH 0x02U
#define EQ_UI_DYNAMIC_FIELD_LEVEL    0x04U
#define EQ_UI_DYNAMIC_FIELD_ALL      0x07U

#define EQ_UI_ANALYZER_FIELD_BAR   0x01U
#define EQ_UI_ANALYZER_FIELD_VALUE 0x02U
#define EQ_UI_ANALYZER_FIELD_ALL   0x03U

#if EQ_ENABLE_TEN_BAND_EDITOR != 0
#define EQ_UI_EDITOR_FIELD_SELECTED_BAND 0x01U
#define EQ_UI_EDITOR_FIELD_DRAFT_GAIN    0x02U
#define EQ_UI_EDITOR_FIELD_APPLIED_GAIN  0x04U
#define EQ_UI_EDITOR_FIELD_CUSTOM_STATE  0x08U
#define EQ_UI_EDITOR_FIELD_APPLY_STATE   0x10U
#define EQ_UI_EDITOR_FIELD_ALL           0x1FU

#define EQ_UI_GAIN_HALF_DB_MIN (-36)
#define EQ_UI_GAIN_HALF_DB_MAX 24
#define EQ_UI_EDITOR_BAR_TOP 136
#define EQ_UI_EDITOR_BAR_BOTTOM 286
#define EQ_UI_EDITOR_DRAW_TOP (EQ_UI_EDITOR_BAR_TOP + 1)
#define EQ_UI_EDITOR_DRAW_BOTTOM (EQ_UI_EDITOR_BAR_BOTTOM - 1)
#define EQ_UI_EDITOR_ZERO_PIXEL 196
#define EQ_UI_EDITOR_MAX_STRIP_HEIGHT 16

/* Each page tile redraws one complete UI region in the hidden framebuffer. */
#define EQ_UI_PAGE_TILE_SWITCH                  0U
#define EQ_UI_PAGE_TILE_TITLE                   1U
#define EQ_UI_PAGE_TILE_PRESET_FIRST            2U
#define EQ_UI_PAGE_TILE_PRESET_LAST             6U
#define EQ_UI_PAGE_TILE_EDITOR_BAND_FIRST       7U
#define EQ_UI_PAGE_TILE_EDITOR_BAND_LAST        16U
#define EQ_UI_PAGE_TILE_EDITOR_CONTROLS         17U
#define EQ_UI_PAGE_TILE_EDITOR_FIELD_FIRST      18U
#define EQ_UI_PAGE_TILE_EDITOR_FIELD_LAST       22U
#define EQ_UI_PAGE_TILE_EDITOR_SWAP             23U
#define EQ_UI_PAGE_TILE_EDITOR_COUNT            24U
#define EQ_UI_PAGE_TILE_DYNAMIC_CHAIN           7U
#define EQ_UI_PAGE_TILE_DYNAMIC_ANALYZER_FIRST  8U
#define EQ_UI_PAGE_TILE_DYNAMIC_ANALYZER_LAST   11U
#define EQ_UI_PAGE_TILE_DYNAMIC_ROW_FIRST       12U
#define EQ_UI_PAGE_TILE_DYNAMIC_ROW_LAST        14U
#define EQ_UI_PAGE_TILE_DYNAMIC_SWAP            15U
#define EQ_UI_PAGE_TILE_DYNAMIC_COUNT           16U
#endif

#define EQ_UI_ANALYZER_MIN_TENTHS_DB (-200)
#define EQ_UI_ANALYZER_MAX_TENTHS_DB 200
#define EQ_UI_ANALYZER_BAR_TOP       124
#define EQ_UI_ANALYZER_BAR_BOTTOM    298
#define EQ_UI_ANALYZER_DRAW_TOP      (EQ_UI_ANALYZER_BAR_TOP + 1)
#define EQ_UI_ANALYZER_DRAW_BOTTOM   (EQ_UI_ANALYZER_BAR_BOTTOM - 1)
#define EQ_UI_ANALYZER_ZERO_PIXEL \
    ((EQ_UI_ANALYZER_BAR_TOP + EQ_UI_ANALYZER_BAR_BOTTOM) / 2)
#define EQ_UI_ANALYZER_HYSTERESIS_PX 2
#define EQ_UI_ANALYZER_MAX_STRIP_HEIGHT 16
#define EQ_UI_ANALYZER_VALUE_DELTA_DB 2
#define EQ_UI_ANALYZER_VALUE_MAX_AGE_FRAMES 50UL
#define EQ_UI_ANALYZER_MAX_AGE_FRAMES \
    EQ_UI_ANALYZER_VALUE_MAX_AGE_FRAMES

#define EQ_UI_PRESET_MIN_GAP_FRAMES  2UL
#define EQ_UI_DYNAMIC_MIN_GAP_FRAMES 4UL
#define EQ_UI_CHAIN_MIN_GAP_FRAMES   8UL
#define EQ_UI_ANALYZER_MIN_GAP_FRAMES 8UL
#define EQ_UI_STEADY_MIN_GAP_FRAMES  7UL

#if EQ_ENABLE_TEN_BAND_EDITOR != 0
#define EQ_UI_EDITOR_MIN_GAP_FRAMES 2UL
#define EQ_UI_PAGE_MIN_GAP_FRAMES   2UL
#endif

typedef enum
{
    EQ_UI_PAGE_DYNAMIC_STATUS = 0,
    EQ_UI_PAGE_EQ_EDITOR = 1
} EQ_UI_PAGE;

#if EQ_ENABLE_TEN_BAND_EDITOR != 0
typedef signed char EQ_UI_GAIN_HALF_DB;

typedef enum
{
    EQ_UI_APPLY_EDITING = 0,
    EQ_UI_APPLY_QUEUED,
    EQ_UI_APPLY_BUILDING,
    EQ_UI_APPLY_READY,
    EQ_UI_APPLY_TRANSITION,
    EQ_UI_APPLY_APPLIED,
    EQ_UI_APPLY_ERROR
} EQ_UI_APPLY_STATUS;

typedef struct
{
    EQ_UI_GAIN_HALF_DB applied_gain_half_db[EQ_NUM_BANDS];
    EQ_UI_GAIN_HALF_DB draft_gain_half_db[EQ_NUM_BANDS];
    EQ_UI_GAIN_HALF_DB submitted_gain_half_db[EQ_NUM_BANDS];
    EQ_CONTROL_SEQUENCE applied_sequence;
    EQ_CONTROL_SEQUENCE submitted_sequence;
    unsigned long draft_version;
    int applied_preset;
    int selected_band;
    int apply_status;
    unsigned int draft_dirty;
    unsigned int submitted_valid;
} EQ_UI_EDITOR_STATE;
#endif

typedef struct
{
    int x;
    int y;
    int w;
    int h;
} EQ_UI_RECT;

typedef struct
{
    int applied_preset;

    int smart_enabled;
    int smart_strength;
    int smart_level;

    int clarity_enabled;
    int clarity_strength;
    int clarity_level;

    int guard_enabled;
    int guard_strength;
    int guard_level;

    int analyzer_valid;
    int analyzer_warm;
    int band_value_db[EQ_UI_ANALYZER_COUNT];
    int band_pixel[EQ_UI_ANALYZER_COUNT];
#if EQ_ENABLE_TEN_BAND_EDITOR != 0
    EQ_UI_GAIN_HALF_DB applied_gain_half_db[EQ_NUM_BANDS];
    EQ_UI_GAIN_HALF_DB draft_gain_half_db[EQ_NUM_BANDS];
    EQ_UI_GAIN_HALF_DB submitted_gain_half_db[EQ_NUM_BANDS];
    EQ_CONTROL_SEQUENCE editor_applied_sequence;
    EQ_CONTROL_SEQUENCE editor_submitted_sequence;
    unsigned long editor_draft_version;
    int page;
    int editor_selected_band;
    int editor_apply_status;
    unsigned int editor_draft_dirty;
    unsigned int editor_submitted_valid;
#endif
} EQ_UI_SNAPSHOT;

typedef struct
{
    EQ_UI_SNAPSHOT requested;
    EQ_UI_SNAPSHOT displayed;
    unsigned long dirty_mask;
    unsigned long displayed_valid_mask;
    unsigned long band_last_display_frame[EQ_UI_ANALYZER_COUNT];
    unsigned long band_last_value_frame[EQ_UI_ANALYZER_COUNT];
    unsigned long category_last_service_frame[EQ_UI_CATEGORY_COUNT];
    unsigned long last_service_frame;
    unsigned long request_frame;
    unsigned int dynamic_field_mask[EQ_UI_DYNAMIC_COUNT];
    unsigned int analyzer_field_mask[EQ_UI_ANALYZER_COUNT];
    unsigned int runtime_mask;
    unsigned int requested_valid;
    unsigned int preset_cursor;
    unsigned int dynamic_cursor;
    unsigned int chain_cursor;
    unsigned int analyzer_cursor;
    unsigned int non_analyzer_streak;
    unsigned int category_last_service_valid_mask;
    unsigned int last_service_frame_valid;
    unsigned char preset_displayed_selected[EQ_UI_PRESET_COUNT];
    unsigned char chain_displayed_enabled[EQ_UI_CHAIN_COUNT];
    unsigned char analyzer_displayed_valid[EQ_UI_ANALYZER_COUNT];
    unsigned char analyzer_displayed_warm[EQ_UI_ANALYZER_COUNT];
    unsigned char analyzer_displayed_field_valid[EQ_UI_ANALYZER_COUNT];
    unsigned char dynamic_displayed_field_valid[EQ_UI_DYNAMIC_COUNT];
#if EQ_ENABLE_TEN_BAND_EDITOR != 0
    unsigned int editor_field_mask;
    unsigned int editor_band_cursor;
    unsigned int page_tile_index;
    unsigned int page_tile_count;
    int displayed_page;
    int requested_page;
    int page_target;
    unsigned int page_building;
    unsigned char editor_band_displayed_valid[EQ_NUM_BANDS];
    unsigned char editor_band_displayed_selected[EQ_NUM_BANDS];
    EQ_UI_GAIN_HALF_DB editor_displayed_applied_gain[EQ_NUM_BANDS];
    int editor_displayed_pixel[EQ_NUM_BANDS];
    unsigned int editor_displayed_field_valid;
#endif
} EQ_UI_STATE;

typedef struct
{
    unsigned int raw_x_min;
    unsigned int raw_x_max;
    unsigned int raw_y_min;
    unsigned int raw_y_max;
    unsigned int swap_xy;
    unsigned int flip_x;
    unsigned int flip_y;
} EQ_UI_TOUCH_TRANSFORM;

typedef enum
{
    EQ_UI_ACTION_NONE = 0,
    EQ_UI_ACTION_PRESET_FLAT,
    EQ_UI_ACTION_PRESET_BASS,
    EQ_UI_ACTION_PRESET_VOCAL,
    EQ_UI_ACTION_PRESET_TREBLE,
    EQ_UI_ACTION_PRESET_V_SHAPE,
    EQ_UI_ACTION_SMART_TOGGLE,
    EQ_UI_ACTION_SMART_STRENGTH,
    EQ_UI_ACTION_CLARITY_TOGGLE,
    EQ_UI_ACTION_CLARITY_STRENGTH,
    EQ_UI_ACTION_GUARD_TOGGLE,
    EQ_UI_ACTION_GUARD_STRENGTH,
    EQ_UI_ACTION_PAGE_SWITCH
#if EQ_ENABLE_TEN_BAND_EDITOR != 0
    , EQ_UI_ACTION_EDITOR_BAND_0,
    EQ_UI_ACTION_EDITOR_BAND_1,
    EQ_UI_ACTION_EDITOR_BAND_2,
    EQ_UI_ACTION_EDITOR_BAND_3,
    EQ_UI_ACTION_EDITOR_BAND_4,
    EQ_UI_ACTION_EDITOR_BAND_5,
    EQ_UI_ACTION_EDITOR_BAND_6,
    EQ_UI_ACTION_EDITOR_BAND_7,
    EQ_UI_ACTION_EDITOR_BAND_8,
    EQ_UI_ACTION_EDITOR_BAND_9,
    EQ_UI_ACTION_EDITOR_MINUS,
    EQ_UI_ACTION_EDITOR_PLUS,
    EQ_UI_ACTION_EDITOR_APPLY,
    EQ_UI_ACTION_EDITOR_RESET_FLAT
#endif
} EQ_UI_ACTION;

typedef struct
{
    EQ_UI_RECT rect;
    int action;
} EQ_UI_HITBOX;

typedef struct
{
    unsigned int press_latched;
    int latched_action;
} EQ_UI_TOUCH_STATE;

extern const EQ_UI_RECT EQ_UI_PRESET_RECTS[EQ_UI_PRESET_COUNT];
extern const EQ_UI_RECT EQ_UI_CHAIN_RECTS[EQ_UI_CHAIN_COUNT];
extern const EQ_UI_RECT EQ_UI_ANALYZER_RECTS[EQ_UI_ANALYZER_COUNT];
extern const EQ_UI_RECT EQ_UI_DYNAMIC_RECTS[EQ_UI_DYNAMIC_COUNT];
extern const EQ_UI_RECT EQ_UI_DYNAMIC_TOGGLE_RECTS[EQ_UI_DYNAMIC_COUNT];
extern const EQ_UI_RECT EQ_UI_DYNAMIC_STRENGTH_RECTS[EQ_UI_DYNAMIC_COUNT];
extern const EQ_UI_RECT EQ_UI_DYNAMIC_LEVEL_RECTS[EQ_UI_DYNAMIC_COUNT];
extern const EQ_UI_RECT EQ_UI_PAGE_SWITCH_RECT;
extern const EQ_UI_HITBOX
    EQ_UI_DYNAMIC_HITBOXES[EQ_UI_DYNAMIC_HITBOX_COUNT];
#if EQ_ENABLE_TEN_BAND_EDITOR != 0
extern const EQ_UI_RECT EQ_UI_EDITOR_BAND_RECTS[EQ_NUM_BANDS];
extern const EQ_UI_RECT EQ_UI_EDITOR_MINUS_RECT;
extern const EQ_UI_RECT EQ_UI_EDITOR_PLUS_RECT;
extern const EQ_UI_RECT EQ_UI_EDITOR_APPLY_RECT;
extern const EQ_UI_RECT EQ_UI_EDITOR_RESET_RECT;
extern const EQ_UI_RECT EQ_UI_EDITOR_FIELDS_RECT;
extern const EQ_UI_HITBOX
    EQ_UI_EDITOR_HITBOXES[EQ_UI_EDITOR_HITBOX_COUNT];
#endif

void EqualizerUiLogic_Init(EQ_UI_STATE *state);
void EqualizerUiLogic_Request(EQ_UI_STATE *state,
                              const EQ_UI_SNAPSHOT *snapshot,
                              unsigned int runtime_mask,
                              unsigned long process_frame);
int EqualizerUiLogic_HasPending(const EQ_UI_STATE *state);
int EqualizerUiLogic_HasEligibleJob(const EQ_UI_STATE *state,
                                    unsigned long process_frame);
int EqualizerUiLogic_SelectJob(EQ_UI_STATE *state,
                               unsigned long process_frame);
unsigned int EqualizerUiLogic_DynamicFieldMask(
    const EQ_UI_STATE *state, int dynamic_index);
unsigned int EqualizerUiLogic_AnalyzerFieldMask(
    const EQ_UI_STATE *state, int analyzer_index);
unsigned int EqualizerUiLogic_AnalyzerNextField(
    const EQ_UI_STATE *state, int analyzer_index);
int EqualizerUiLogic_AnalyzerNextPixel(
    const EQ_UI_STATE *state, int analyzer_index);
void EqualizerUiLogic_CompleteJob(EQ_UI_STATE *state, int job,
                                  unsigned long process_frame);
void EqualizerUiLogic_CompleteDynamicField(
    EQ_UI_STATE *state, int job, unsigned int completed_fields,
    unsigned long process_frame);
void EqualizerUiLogic_CompleteAnalyzerField(
    EQ_UI_STATE *state, int job, unsigned int completed_field,
    unsigned long process_frame);
#if EQ_ENABLE_TEN_BAND_EDITOR != 0
unsigned int EqualizerUiLogic_EditorFieldMask(const EQ_UI_STATE *state);
unsigned int EqualizerUiLogic_EditorNextField(const EQ_UI_STATE *state);
int EqualizerUiLogic_EditorNextPixel(const EQ_UI_STATE *state,
                                     int band_index);
void EqualizerUiLogic_CompleteEditorBand(
    EQ_UI_STATE *state, int job, unsigned long process_frame);
void EqualizerUiLogic_CompleteEditorField(
    EQ_UI_STATE *state, unsigned int completed_field,
    unsigned long process_frame);
void EqualizerUiLogic_CompletePageTile(
    EQ_UI_STATE *state, unsigned long process_frame);
int EqualizerUiLogic_GetDisplayedPage(const EQ_UI_STATE *state);
int EqualizerUiLogic_IsPageBuilding(const EQ_UI_STATE *state);
unsigned int EqualizerUiLogic_GetPageTileIndex(const EQ_UI_STATE *state);
unsigned int EqualizerUiLogic_GetPageTileCount(const EQ_UI_STATE *state);
#endif
void EqualizerUiLogic_Cancel(EQ_UI_STATE *state);

int EqualizerUi_DbTenthsToPixel(int tenths_db);
int EqualizerUi_RoundTenthsToDb(int tenths_db);
int EqualizerUi_RectContains(const EQ_UI_RECT *rect, int x, int y);
int EqualizerUi_ValidateLayout(void);

void EqualizerUi_DefaultTouchTransform(EQ_UI_TOUCH_TRANSFORM *transform);
int EqualizerUi_MapTouchRawToScreen(
    const EQ_UI_TOUCH_TRANSFORM *transform,
    unsigned int raw_x, unsigned int raw_y,
    int *screen_x, int *screen_y);
int EqualizerUi_HitTest(int page, int screen_x, int screen_y);
void EqualizerUiTouch_Init(EQ_UI_TOUCH_STATE *state);
int EqualizerUiTouch_Process(EQ_UI_TOUCH_STATE *state,
                             int page, int page_building,
                             int pressed, int screen_x, int screen_y,
                             int *rejected);
int EqualizerUi_ActionToPreset(int action);

#if EQ_ENABLE_TEN_BAND_EDITOR != 0
void EqualizerUiEditor_Init(EQ_UI_EDITOR_STATE *state);
int EqualizerUiEditor_SelectBand(EQ_UI_EDITOR_STATE *state, int band);
int EqualizerUiEditor_StepSelected(EQ_UI_EDITOR_STATE *state,
                                   int delta_half_db);
int EqualizerUiEditor_SetDraftFlat(EQ_UI_EDITOR_STATE *state);
int EqualizerUiEditor_HasSubmittableDraft(
    const EQ_UI_EDITOR_STATE *state);
void EqualizerUiEditor_CopyDraftDb(
    const EQ_UI_EDITOR_STATE *state, float gains_db[EQ_NUM_BANDS]);
void EqualizerUiEditor_MarkSubmitted(
    EQ_UI_EDITOR_STATE *state, EQ_CONTROL_SEQUENCE sequence);
void EqualizerUiEditor_ObserveApplied(
    EQ_UI_EDITOR_STATE *state,
    const EQ_UI_GAIN_HALF_DB gains[EQ_NUM_BANDS],
    int preset, EQ_CONTROL_SEQUENCE applied_sequence);
void EqualizerUiEditor_SetApplyStatus(EQ_UI_EDITOR_STATE *state,
                                      int status);
void EqualizerUiEditor_CopyToSnapshot(
    const EQ_UI_EDITOR_STATE *state, EQ_UI_SNAPSHOT *snapshot);
EQ_UI_GAIN_HALF_DB EqualizerUi_GainDbToHalf(float gain_db);
#endif

#endif /* _USER_EQUALIZER_UI_LOGIC_H_ */
