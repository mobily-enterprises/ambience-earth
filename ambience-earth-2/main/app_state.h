#pragma once

#define LV_CONF_INCLUDE_SIMPLE
#include <lvgl.h>
#include <stddef.h>
#include <stdint.h>

constexpr uint16_t kScreenWidth = 320;
constexpr uint16_t kScreenHeight = 240;
constexpr uint16_t kTouchWidth = 240;
constexpr uint16_t kTouchHeight = 320;
constexpr uint16_t kBufferLines = 20;

constexpr uint32_t kFeedIntervalSec = 180;
constexpr uint32_t kFeedDurationSec = 12;
constexpr uint32_t kScreensaverDelayMs = 15000;
constexpr uint32_t kScreensaverMoveMs = 2000;

constexpr int kSlotCount = 6;
constexpr int kMaxLogs = 24;
constexpr int kScreenStackMax = 10;
constexpr int kBindingPoolSize = 20;
constexpr int kOptionGroupSize = 10;
constexpr int kOptionButtonSize = 20;

extern const lv_color_t kColorBg;
extern const lv_color_t kColorHeader;
extern const lv_color_t kColorMuted;
extern const lv_color_t kColorAccent;

enum ScreenId {
  SCREEN_INFO = 0,
  SCREEN_MENU,
  SCREEN_LOGS,
  SCREEN_FEEDING_MENU,
  SCREEN_SLOTS_LIST,
  SCREEN_SLOT_SUMMARY,
  SCREEN_SLOT_WIZARD,
  SCREEN_SETTINGS_MENU,
  SCREEN_TIME_DATE,
  SCREEN_CAL_MENU,
  SCREEN_CAL_MOIST,
  SCREEN_CAL_FLOW,
  SCREEN_TEST_MENU,
  SCREEN_TEST_SENSORS,
  SCREEN_TEST_PUMPS,
  SCREEN_RESET_MENU,
  SCREEN_INITIAL_SETUP,
  SCREEN_NUMBER_INPUT,
  SCREEN_TIME_RANGE_INPUT,
};

enum MoistureMode {
  MODE_OFF = 0,
  MODE_PERCENT = 1,
  MODE_BASELINE = 2
};

enum RunoffMode {
  RUNOFF_NEITHER = 0,
  RUNOFF_MUST = 1,
  RUNOFF_AVOID = 2
};

enum OptionTargetType {
  OPTION_TARGET_INT = 0,
  OPTION_TARGET_BOOL = 1
};

struct DateTime {
  int year;
  int month;
  int day;
  int hour;
  int minute;
  int second;
};

struct SimState {
  DateTime now;
  uint32_t uptime_sec;
  int moisture;
  int dryback;
  int last_feed_ml;
  uint32_t last_feed_uptime;
  int daily_total_ml;
  int baseline_x;
  int baseline_y;
  int baseline_delay_min;
  int max_daily_ml;
  int lights_on_hour;
  int lights_on_min;
  int lights_off_hour;
  int lights_off_min;
  bool paused;
  bool feeding_active;
  int feeding_slot;
  int feeding_elapsed;
  int feeding_water_ml;
  int feeding_max_ml;
  bool runoff;
  int info_toggle;
  int moisture_raw;
  int dripper_ml_per_hour;
  int moisture_raw_dry;
  int moisture_raw_wet;
  uint32_t last_values_log_uptime;
};

struct SetupFlags {
  bool time_date;
  bool max_daily;
  bool lights;
  bool moisture_cal;
  bool dripper_cal;
};

struct Slot {
  bool enabled;
  char name[16];
  bool use_window;
  int start_hour;
  int start_min;
  int window_duration_min;
  MoistureMode start_mode;
  int start_value;
  MoistureMode target_mode;
  int target_value;
  int max_ml;
  bool stop_on_runoff;
  RunoffMode runoff_mode;
  int runoff_baseline;
  int min_gap_min;
};

struct LogEntry {
  uint8_t type;
  DateTime dt;
  int soil_pct;
  int dryback;
  int slot;
  int start_moisture;
  int end_moisture;
  int volume_ml;
  int daily_total_ml;
  bool runoff;
  char stop_reason[12];
};

struct ScreenNode {
  ScreenId id;
  lv_obj_t *root;
};

struct InfoRefs {
  lv_obj_t *container;
  lv_obj_t *line0;
  lv_obj_t *line1;
  lv_obj_t *line2;
  lv_obj_t *line3;
  lv_obj_t *star;
};

struct LogsRefs {
  lv_obj_t *header;
  lv_obj_t *line1;
  lv_obj_t *line2;
  lv_obj_t *line3;
  lv_obj_t *index_label;
};

struct SlotSummaryRefs {
  lv_obj_t *lines[4];
};

struct WizardRefs {
  lv_obj_t *step_label;
  lv_obj_t *content;
  lv_obj_t *next_btn;
  lv_obj_t *next_label;
  lv_obj_t *back_btn;
  lv_obj_t *text_area;
};

struct TimeDateRefs {
  lv_obj_t *step_label;
  lv_obj_t *content;
  lv_obj_t *next_btn;
  lv_obj_t *next_label;
  lv_obj_t *back_btn;
};

struct CalMoistRefs {
  lv_obj_t *mode_label;
  lv_obj_t *raw_label;
  lv_obj_t *percent_label;
};

struct CalFlowRefs {
  lv_obj_t *step_label;
  lv_obj_t *status_label;
  lv_obj_t *action_label;
};

struct TestSensorsRefs {
  lv_obj_t *raw_label;
  lv_obj_t *percent_label;
  lv_obj_t *runoff_label;
};

struct PumpTestRefs {
  lv_obj_t *status_label;
};

struct NumberInputContext {
  const char *title;
  int value;
  int min;
  int max;
  int step;
  const char *unit;
  int *target;
  void (*on_done)();
};

struct TimeRangeContext {
  const char *title;
  int on_hour;
  int on_min;
  int off_hour;
  int off_min;
  void (*on_done)();
};

struct NumberBinding {
  int *value;
  int min;
  int max;
  int step;
  lv_obj_t *label;
  const char *fmt;
};

struct OptionGroup {
  void *target;
  OptionTargetType target_type;
  int count;
  lv_obj_t *btns[4];
  int values[4];
};

struct OptionButtonData {
  OptionGroup *group;
  int index;
};

typedef void (*PromptHandler)(int option_id, int context);

struct PromptContext {
  lv_obj_t *overlay;
  PromptHandler handler;
  int context;
};

extern SimState g_sim;
extern SetupFlags g_setup;
extern Slot g_slots[kSlotCount];
extern Slot g_edit_slot;
extern LogEntry g_logs[kMaxLogs];
extern int g_log_count;
extern int g_log_index;
extern int g_selected_slot;
extern bool g_force_feed_mode;

extern ScreenNode g_screen_stack[kScreenStackMax];
extern int g_screen_stack_size;
extern ScreenId g_active_screen;

extern InfoRefs g_info_refs;
extern LogsRefs g_logs_refs;
extern SlotSummaryRefs g_slot_summary_refs;
extern WizardRefs g_wizard_refs;
extern TimeDateRefs g_time_date_refs;
extern CalMoistRefs g_cal_moist_refs;
extern CalFlowRefs g_cal_flow_refs;
extern TestSensorsRefs g_test_sensors_refs;
extern PumpTestRefs g_pump_test_refs;
extern NumberInputContext g_number_ctx;
extern TimeRangeContext g_time_range_ctx;
extern PromptContext g_prompt;

extern NumberBinding g_binding_pool[kBindingPoolSize];
extern int g_binding_count;
extern OptionGroup g_option_groups[kOptionGroupSize];
extern int g_option_group_count;
extern OptionButtonData g_option_button_data[kOptionButtonSize];
extern int g_option_button_count;

extern int g_wizard_step;
extern int g_time_date_step;
extern DateTime g_time_date_edit;
extern int g_cal_flow_step;
extern bool g_cal_flow_running;
extern int g_cal_moist_mode;
extern int g_pump_test_cycle;
extern uint32_t g_pump_test_last_ms;
extern lv_obj_t *g_pause_menu_label;
extern lv_obj_t *g_initial_setup_list;

extern lv_timer_t *g_ui_timer;
extern lv_obj_t *g_debug_label;

extern uint32_t g_last_touch_ms;
extern bool g_screensaver_active;
extern uint32_t g_last_screensaver_move_ms;

void reset_binding_pool();
void reset_option_pool();
NumberBinding *alloc_binding();
OptionGroup *alloc_option_group();
OptionButtonData *alloc_option_button();
