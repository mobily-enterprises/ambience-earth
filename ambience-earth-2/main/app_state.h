#pragma once

#define LV_CONF_INCLUDE_SIMPLE
#include <lvgl.h>
#include <stddef.h>
#include <stdint.h>

constexpr uint16_t kScreenWidth = 320;
constexpr uint16_t kScreenHeight = 240;
constexpr uint16_t kTouchWidth = 240;
constexpr uint16_t kTouchHeight = 320;
#ifdef WOKWI_SIM
constexpr uint16_t kBufferLines = 240;
#else
constexpr uint16_t kBufferLines = 20;
#endif

constexpr uint32_t kScreensaverDelayMs = 15000;
constexpr uint32_t kScreensaverMoveMs = 2000;
constexpr uint32_t kLogValuesIntervalMs = 3600000;

constexpr int kSlotCount = 8;
constexpr int kMaxLogs = 24;
constexpr size_t kLogStoreBytes = 48 * 1024;
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
  bool baseline_setter;
  int min_gap_min;
};

struct LogEntry {
  uint16_t seq;
  uint8_t entryType;
  uint8_t stopReason;
  uint8_t startReason;
  uint8_t slotIndex;
  uint8_t flags;
  uint8_t soilMoistureBefore;
  uint8_t soilMoistureAfter;
  uint8_t baselinePercent;
  uint8_t drybackPercent;
  uint16_t feedMl;
  uint16_t dailyTotalMl;
  uint16_t lightDayKey;
  uint8_t startYear;
  uint8_t startMonth;
  uint8_t startDay;
  uint8_t startHour;
  uint8_t startMinute;
  uint8_t endYear;
  uint8_t endMonth;
  uint8_t endDay;
  uint8_t endHour;
  uint8_t endMinute;
  uint32_t millisStart;
  uint32_t millisEnd;
};

struct ScreenNode {
  ScreenId id;
  lv_obj_t *root;
};

struct InfoRefs {
  lv_obj_t *main;
  lv_obj_t *menu_row;
  lv_obj_t *menu_btn;
  lv_obj_t *moist_value;
  lv_obj_t *dry_value;
  lv_obj_t *baseline_value;
  lv_obj_t *minmax_value;
  lv_obj_t *time_value;
  lv_obj_t *today_value;
  lv_obj_t *play_pause_icon;
  lv_obj_t *status_value;
  lv_obj_t *status_icon;
  lv_obj_t *day_night_icon;
  lv_obj_t *night_icon;
  lv_obj_t *last_value;
  lv_obj_t *totals_value;
  lv_obj_t *screensaver_root;
  lv_obj_t *screensaver_plant;
  lv_obj_t *screensaver_icon;
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
  lv_obj_t *header;
  lv_obj_t *step_label;
  lv_obj_t *content;
  lv_obj_t *next_btn;
  lv_obj_t *next_label;
  lv_obj_t *back_btn;
  lv_obj_t *footer;
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
  lv_obj_t *choice_row;
};

struct CalFlowRefs {
  lv_obj_t *step_label;
  lv_obj_t *status_label;
  lv_obj_t *action_label;
  lv_obj_t *target_row;
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
extern LogEntry newLogEntry;
extern int g_log_count;
extern int g_log_index;
extern int g_selected_slot;
extern bool g_force_feed_mode;
extern unsigned long int millisAtEndOfLastFeed;
extern uint16_t lastFeedMl;

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
extern uint32_t g_cal_flow_start_ms;
extern uint32_t g_cal_flow_elapsed_ms;
extern int g_cal_flow_target_ml_per_hour;
extern uint16_t g_cal_moist_avg_raw;
extern bool g_cal_moist_window_done;
extern int g_cal_moist_mode;
extern bool g_cal_moist_prompt_shown;
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
