/**
 * user_equalizer_ui_logic.h
 *
 * Hardware-independent Project 3.3 UI state, layout, and touch logic.
 */

#ifndef _USER_EQUALIZER_UI_LOGIC_H_
#define _USER_EQUALIZER_UI_LOGIC_H_

#include "user_equalizer.h"

#define EQ_UI_SCREEN_WIDTH  800
#define EQ_UI_SCREEN_HEIGHT 480

#define EQ_UI_PRESET_COUNT  5
#define EQ_UI_CHAIN_COUNT   3
#define EQ_UI_ANALYZER_COUNT 4
#define EQ_UI_DYNAMIC_COUNT 3
#define EQ_UI_HITBOX_COUNT  11

#define EQ_UI_RUNTIME_PRESET   0x01U
#define EQ_UI_RUNTIME_CHAIN    0x02U
#define EQ_UI_RUNTIME_DYNAMICS 0x04U
#define EQ_UI_RUNTIME_ANALYZER 0x08U
#define EQ_UI_RUNTIME_ALL      0x0FU

#define EQ_UI_JOB_NONE       0
#define EQ_UI_JOB_PRESET_0   1
#define EQ_UI_JOB_PRESET_4   (EQ_UI_JOB_PRESET_0 + 4)
#define EQ_UI_JOB_CHAIN_0    6
#define EQ_UI_JOB_CHAIN_2    (EQ_UI_JOB_CHAIN_0 + 2)
#define EQ_UI_JOB_ANALYZER_0 9
#define EQ_UI_JOB_ANALYZER_3 (EQ_UI_JOB_ANALYZER_0 + 3)
#define EQ_UI_JOB_DYNAMIC_0  13
#define EQ_UI_JOB_DYNAMIC_2  (EQ_UI_JOB_DYNAMIC_0 + 2)
#define EQ_UI_JOB_COUNT      15

#define EQ_UI_DYNAMIC_FIELD_ENABLED  0x01U
#define EQ_UI_DYNAMIC_FIELD_STRENGTH 0x02U
#define EQ_UI_DYNAMIC_FIELD_LEVEL    0x04U
#define EQ_UI_DYNAMIC_FIELD_ALL      0x07U

#define EQ_UI_ANALYZER_MIN_TENTHS_DB (-200)
#define EQ_UI_ANALYZER_MAX_TENTHS_DB 200
#define EQ_UI_ANALYZER_BAR_TOP       124
#define EQ_UI_ANALYZER_BAR_BOTTOM    298
#define EQ_UI_ANALYZER_HYSTERESIS_PX 2
#define EQ_UI_ANALYZER_MAX_AGE_FRAMES 50UL

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
} EQ_UI_SNAPSHOT;

typedef struct
{
    EQ_UI_SNAPSHOT requested;
    EQ_UI_SNAPSHOT displayed;
    unsigned long dirty_mask;
    unsigned long displayed_valid_mask;
    unsigned long band_last_display_frame[EQ_UI_ANALYZER_COUNT];
    unsigned int dynamic_field_mask[EQ_UI_DYNAMIC_COUNT];
    unsigned int runtime_mask;
    unsigned int requested_valid;
    unsigned int preset_cursor;
    unsigned int dynamic_cursor;
    unsigned int chain_cursor;
    unsigned int analyzer_cursor;
    unsigned int non_analyzer_streak;
    unsigned char preset_displayed_selected[EQ_UI_PRESET_COUNT];
    unsigned char chain_displayed_enabled[EQ_UI_CHAIN_COUNT];
    unsigned char analyzer_displayed_valid[EQ_UI_ANALYZER_COUNT];
    unsigned char analyzer_displayed_warm[EQ_UI_ANALYZER_COUNT];
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
    EQ_UI_ACTION_GUARD_STRENGTH
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
extern const EQ_UI_HITBOX EQ_UI_HITBOXES[EQ_UI_HITBOX_COUNT];

void EqualizerUiLogic_Init(EQ_UI_STATE *state);
void EqualizerUiLogic_Request(EQ_UI_STATE *state,
                              const EQ_UI_SNAPSHOT *snapshot,
                              unsigned int runtime_mask,
                              unsigned long process_frame);
int EqualizerUiLogic_HasPending(const EQ_UI_STATE *state);
int EqualizerUiLogic_SelectJob(EQ_UI_STATE *state);
unsigned int EqualizerUiLogic_DynamicFieldMask(
    const EQ_UI_STATE *state, int dynamic_index);
void EqualizerUiLogic_CompleteJob(EQ_UI_STATE *state, int job,
                                  unsigned long process_frame);
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
int EqualizerUi_HitTest(int screen_x, int screen_y);
void EqualizerUiTouch_Init(EQ_UI_TOUCH_STATE *state);
int EqualizerUiTouch_Process(EQ_UI_TOUCH_STATE *state,
                             int pressed, int screen_x, int screen_y,
                             int *rejected);
int EqualizerUi_ActionToPreset(int action);

#endif /* _USER_EQUALIZER_UI_LOGIC_H_ */
