#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <Adafruit_FT6206.h>
#include <string.h>

#define LV_CONF_INCLUDE_SIMPLE
#include <lvgl.h>

static const uint16_t kScreenWidth = 240;
static const uint16_t kScreenHeight = 320;
static const uint16_t kBufferLines = 20;

static const uint8_t kTftSclk = 12;
static const uint8_t kTftMosi = 11;
static const uint8_t kTftMiso = 13;
static const uint8_t kTftCs = 15;
static const uint8_t kTftDc = 2;
static const uint8_t kTftRst = 4;
static const uint8_t kTftLed = 6;
static const uint8_t kTftRotation = 0;
static const uint32_t kTftSpiHz = 40000000;

static const uint8_t kTouchSda = 10;
static const uint8_t kTouchScl = 8;

static const uint32_t kBootSplashMs = 120;

static const uint32_t kFeedIntervalSec = 180;
static const uint32_t kFeedDurationSec = 12;
static const uint32_t kScreensaverDelayMs = 15000;
static const uint32_t kScreensaverMoveMs = 2000;

static const lv_color_t kColorBg = lv_color_hex(0x101418);
static const lv_color_t kColorHeader = lv_color_hex(0x1f2933);
static const lv_color_t kColorMuted = lv_color_hex(0x94a3b8);
static const lv_color_t kColorAccent = lv_color_hex(0x38bdf8);

static Adafruit_ILI9341 tft(kTftCs, kTftDc, kTftRst);
static Adafruit_FT6206 ctp;

static uint16_t buf1[kScreenWidth * kBufferLines] __attribute__((aligned(4)));

static bool touchReady = false;
static uint32_t g_last_touch_ms = 0;
static bool g_screensaver_active = false;
static uint32_t g_last_screensaver_move_ms = 0;

static lv_obj_t *g_debug_label = nullptr;

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

static SimState g_sim;
static SetupFlags g_setup;
static const int kSlotCount = 6;
static Slot g_slots[kSlotCount];
static Slot g_edit_slot;
static const int kMaxLogs = 24;
static LogEntry g_logs[kMaxLogs];
static int g_log_count = 0;
static int g_log_index = 0;
static int g_selected_slot = 0;
static bool g_force_feed_mode = false;

static ScreenNode g_screen_stack[10];
static int g_screen_stack_size = 0;
static ScreenId g_active_screen = SCREEN_INFO;

static InfoRefs g_info_refs;
static LogsRefs g_logs_refs;
static SlotSummaryRefs g_slot_summary_refs;
static WizardRefs g_wizard_refs;
static TimeDateRefs g_time_date_refs;
static CalMoistRefs g_cal_moist_refs;
static CalFlowRefs g_cal_flow_refs;
static TestSensorsRefs g_test_sensors_refs;
static PumpTestRefs g_pump_test_refs;
static NumberInputContext g_number_ctx;
static TimeRangeContext g_time_range_ctx;
static PromptContext g_prompt;

static NumberBinding g_binding_pool[20];
static int g_binding_count = 0;
static OptionGroup g_option_groups[10];
static int g_option_group_count = 0;
static OptionButtonData g_option_button_data[20];
static int g_option_button_count = 0;

static int g_wizard_step = 0;
static int g_time_date_step = 0;
static DateTime g_time_date_edit;
static int g_cal_flow_step = 0;
static bool g_cal_flow_running = false;
static int g_cal_moist_mode = 0;
static int g_pump_test_cycle = 0;
static uint32_t g_pump_test_last_ms = 0;
static lv_obj_t *g_pause_menu_label = nullptr;
static lv_obj_t *g_initial_setup_list = nullptr;

static lv_timer_t *g_ui_timer = nullptr;

static int clamp_int(int value, int min_value, int max_value) {
  if (value < min_value) return min_value;
  if (value > max_value) return max_value;
  return value;
}

static int days_in_month(int month, int year) {
  (void)year;
  switch (month) {
    case 1:
    case 3:
    case 5:
    case 7:
    case 8:
    case 10:
    case 12:
      return 31;
    case 4:
    case 6:
    case 9:
    case 11:
      return 30;
    case 2:
      return 28;
    default:
      return 30;
  }
}

static void advance_time_one_sec(DateTime *dt) {
  dt->second += 1;
  if (dt->second < 60) return;
  dt->second = 0;
  dt->minute += 1;
  if (dt->minute < 60) return;
  dt->minute = 0;
  dt->hour += 1;
  if (dt->hour < 24) return;
  dt->hour = 0;
  dt->day += 1;
  int dim = days_in_month(dt->month, dt->year);
  if (dt->day <= dim) return;
  dt->day = 1;
  dt->month += 1;
  if (dt->month <= 12) return;
  dt->month = 1;
  dt->year += 1;
}

static void format_time(const DateTime &dt, char *buf, size_t len) {
  snprintf(buf, len, "%02d:%02d", dt.hour, dt.minute);
}

static void format_date(const DateTime &dt, char *buf, size_t len) {
  snprintf(buf, len, "%02d/%02d/%04d", dt.month, dt.day, dt.year);
}

static void format_datetime(const DateTime &dt, char *buf, size_t len) {
  snprintf(buf, len, "%02d/%02d %02d:%02d", dt.month, dt.day, dt.hour, dt.minute);
}

static void format_ago(uint32_t seconds, char *buf, size_t len) {
  if (seconds < 60) {
    snprintf(buf, len, "%lus", static_cast<unsigned long>(seconds));
  } else if (seconds < 3600) {
    snprintf(buf, len, "%lum", static_cast<unsigned long>(seconds / 60));
  } else {
    unsigned long hours = seconds / 3600;
    unsigned long minutes = (seconds / 60) % 60;
    snprintf(buf, len, "%luh%02lum", hours, minutes);
  }
}

static bool setup_complete() {
  return g_setup.time_date && g_setup.max_daily && g_setup.lights && g_setup.moisture_cal && g_setup.dripper_cal;
}

static void reset_binding_pool() {
  g_binding_count = 0;
}

static void reset_option_pool() {
  g_option_group_count = 0;
  g_option_button_count = 0;
}

static NumberBinding *alloc_binding() {
  if (g_binding_count >= static_cast<int>(sizeof(g_binding_pool) / sizeof(g_binding_pool[0]))) return nullptr;
  return &g_binding_pool[g_binding_count++];
}

static OptionGroup *alloc_option_group() {
  if (g_option_group_count >= static_cast<int>(sizeof(g_option_groups) / sizeof(g_option_groups[0]))) return nullptr;
  OptionGroup *group = &g_option_groups[g_option_group_count++];
  group->count = 0;
  group->target = nullptr;
  group->target_type = OPTION_TARGET_INT;
  return group;
}

static OptionButtonData *alloc_option_button() {
  if (g_option_button_count >= static_cast<int>(sizeof(g_option_button_data) / sizeof(g_option_button_data[0]))) return nullptr;
  return &g_option_button_data[g_option_button_count++];
}

static void update_number_label(NumberBinding *binding) {
  if (!binding || !binding->label) return;
  if (binding->fmt) lv_label_set_text_fmt(binding->label, binding->fmt, *binding->value);
  else lv_label_set_text_fmt(binding->label, "%d", *binding->value);
}

static void wizard_refresh_cb(void *);

static void number_inc_event(lv_event_t *event) {
  NumberBinding *binding = static_cast<NumberBinding *>(lv_event_get_user_data(event));
  if (!binding || !binding->value) return;
  *binding->value = clamp_int(*binding->value + binding->step, binding->min, binding->max);
  update_number_label(binding);
}

static void number_dec_event(lv_event_t *event) {
  NumberBinding *binding = static_cast<NumberBinding *>(lv_event_get_user_data(event));
  if (!binding || !binding->value) return;
  *binding->value = clamp_int(*binding->value - binding->step, binding->min, binding->max);
  update_number_label(binding);
}

static void option_select_event(lv_event_t *event) {
  OptionButtonData *data = static_cast<OptionButtonData *>(lv_event_get_user_data(event));
  if (!data || !data->group || !data->group->target) return;
  OptionGroup *group = data->group;
  int index = data->index;
  if (index < 0 || index >= group->count) return;
  if (group->target_type == OPTION_TARGET_BOOL) {
    *static_cast<bool *>(group->target) = (group->values[index] != 0);
  } else {
    *static_cast<int *>(group->target) = group->values[index];
  }
  for (int i = 0; i < group->count; ++i) {
    if (group->btns[i]) {
      if (i == index) lv_obj_add_state(group->btns[i], LV_STATE_CHECKED);
      else lv_obj_clear_state(group->btns[i], LV_STATE_CHECKED);
    }
  }

  if (g_active_screen == SCREEN_SLOT_WIZARD) {
    if (g_wizard_step == 2 || g_wizard_step == 3 || g_wizard_step == 4 || g_wizard_step == 6) {
      lv_async_call(wizard_refresh_cb, nullptr);
    }
  }
}

static lv_obj_t *create_screen_root() {
  lv_obj_t *screen = lv_obj_create(nullptr);
  lv_obj_set_style_bg_color(screen, kColorBg, 0);
  lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
  lv_obj_set_style_text_color(screen, lv_color_white(), 0);
  lv_obj_set_style_pad_all(screen, 8, 0);
  lv_obj_set_style_pad_row(screen, 6, 0);
  lv_obj_set_flex_flow(screen, LV_FLEX_FLOW_COLUMN);
  lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
  return screen;
}

static lv_obj_t *create_header(lv_obj_t *parent, const char *title, bool show_back, lv_event_cb_t back_cb) {
  lv_obj_t *header = lv_obj_create(parent);
  lv_obj_set_size(header, LV_PCT(100), 30);
  lv_obj_set_style_bg_color(header, kColorHeader, 0);
  lv_obj_set_style_bg_opa(header, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(header, 0, 0);
  lv_obj_set_style_pad_all(header, 4, 0);
  lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *title_label = lv_label_create(header);
  lv_label_set_text(title_label, title);
  lv_obj_set_style_text_font(title_label, &lv_font_montserrat_18, 0);
  lv_obj_align(title_label, LV_ALIGN_CENTER, 0, 0);

  if (show_back) {
    lv_obj_t *back_btn = lv_btn_create(header);
    lv_obj_set_size(back_btn, 54, 22);
    lv_obj_align(back_btn, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_add_event_cb(back_btn, back_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *label = lv_label_create(back_btn);
    lv_label_set_text(label, "Back");
    lv_obj_center(label);
  }

  return header;
}

static lv_obj_t *create_menu_list(lv_obj_t *parent) {
  lv_obj_t *list = lv_obj_create(parent);
  lv_obj_set_width(list, LV_PCT(100));
  lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(list, 6, 0);
  lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(list, 0, 0);
  lv_obj_clear_flag(list, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_grow(list, 1);
  return list;
}

static lv_obj_t *add_menu_item(lv_obj_t *list, const char *label, const char *value,
                               lv_event_cb_t cb, void *user_data, lv_obj_t **out_label) {
  lv_obj_t *btn = lv_btn_create(list);
  lv_obj_set_width(btn, LV_PCT(100));
  lv_obj_set_height(btn, 36);
  lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, user_data);

  lv_obj_t *label_obj = lv_label_create(btn);
  lv_label_set_text(label_obj, label);
  lv_obj_align(label_obj, LV_ALIGN_LEFT_MID, 4, 0);

  if (out_label) *out_label = label_obj;

  if (value) {
    lv_obj_t *value_label = lv_label_create(btn);
    lv_label_set_text(value_label, value);
    lv_obj_align(value_label, LV_ALIGN_RIGHT_MID, -4, 0);
    return value_label;
  }
  return nullptr;
}

static lv_obj_t *create_number_selector(lv_obj_t *parent, const char *label_text, int *value,
                                        int min_value, int max_value, int step, const char *fmt) {
  lv_obj_t *row = lv_obj_create(parent);
  lv_obj_set_width(row, LV_PCT(100));
  lv_obj_set_height(row, 34);
  lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(row, 0, 0);
  lv_obj_set_style_pad_all(row, 0, 0);
  lv_obj_set_style_pad_gap(row, 6, 0);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *label = lv_label_create(row);
  lv_label_set_text(label, label_text);
  lv_obj_set_width(label, 90);

  lv_obj_t *dec_btn = lv_btn_create(row);
  lv_obj_set_size(dec_btn, 28, 28);
  lv_obj_t *dec_label = lv_label_create(dec_btn);
  lv_label_set_text(dec_label, "-");
  lv_obj_center(dec_label);

  lv_obj_t *value_label = lv_label_create(row);
  lv_obj_set_width(value_label, 50);

  lv_obj_t *inc_btn = lv_btn_create(row);
  lv_obj_set_size(inc_btn, 28, 28);
  lv_obj_t *inc_label = lv_label_create(inc_btn);
  lv_label_set_text(inc_label, "+");
  lv_obj_center(inc_label);

  NumberBinding *binding = alloc_binding();
  if (binding) {
    binding->value = value;
    binding->min = min_value;
    binding->max = max_value;
    binding->step = step;
    binding->label = value_label;
    binding->fmt = fmt;
    update_number_label(binding);
    lv_obj_add_event_cb(inc_btn, number_inc_event, LV_EVENT_CLICKED, binding);
    lv_obj_add_event_cb(dec_btn, number_dec_event, LV_EVENT_CLICKED, binding);
  }

  return row;
}

static lv_obj_t *create_time_input_block(lv_obj_t *parent, const char *title, int *hour, int *minute) {
  lv_obj_t *block = lv_obj_create(parent);
  lv_obj_set_width(block, LV_PCT(100));
  lv_obj_set_style_bg_opa(block, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(block, 0, 0);
  lv_obj_set_style_pad_all(block, 0, 0);
  lv_obj_set_style_pad_row(block, 4, 0);
  lv_obj_set_flex_flow(block, LV_FLEX_FLOW_COLUMN);
  lv_obj_clear_flag(block, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *label = lv_label_create(block);
  lv_label_set_text(label, title);
  lv_obj_set_style_text_color(label, kColorMuted, 0);

  create_number_selector(block, "Hour", hour, 0, 23, 1, "%02d");
  create_number_selector(block, "Minute", minute, 0, 59, 1, "%02d");

  return block;
}

static lv_obj_t *create_date_input_block(lv_obj_t *parent, const char *title, int *month, int *day, int *year) {
  lv_obj_t *block = lv_obj_create(parent);
  lv_obj_set_width(block, LV_PCT(100));
  lv_obj_set_style_bg_opa(block, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(block, 0, 0);
  lv_obj_set_style_pad_all(block, 0, 0);
  lv_obj_set_style_pad_row(block, 4, 0);
  lv_obj_set_flex_flow(block, LV_FLEX_FLOW_COLUMN);
  lv_obj_clear_flag(block, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *label = lv_label_create(block);
  lv_label_set_text(label, title);
  lv_obj_set_style_text_color(label, kColorMuted, 0);

  create_number_selector(block, "Month", month, 1, 12, 1, "%02d");
  create_number_selector(block, "Day", day, 1, 31, 1, "%02d");
  create_number_selector(block, "Year", year, 2024, 2035, 1, "%d");

  return block;
}

static void prompt_close() {
  if (g_prompt.overlay) {
    lv_obj_del(g_prompt.overlay);
    g_prompt.overlay = nullptr;
  }
}

static void prompt_button_event(lv_event_t *event) {
  int option_id = static_cast<int>(reinterpret_cast<intptr_t>(lv_event_get_user_data(event)));
  PromptHandler handler = g_prompt.handler;
  int context = g_prompt.context;
  prompt_close();
  if (handler) handler(option_id, context);
}

static void show_prompt(const char *title, const char *message, const char **options, int option_count,
                        PromptHandler handler, int context) {
  prompt_close();
  g_prompt.handler = handler;
  g_prompt.context = context;

  lv_obj_t *overlay = lv_obj_create(lv_layer_top());
  lv_obj_set_size(overlay, kScreenWidth, kScreenHeight);
  lv_obj_set_style_bg_color(overlay, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(overlay, LV_OPA_60, 0);
  lv_obj_set_style_border_width(overlay, 0, 0);
  lv_obj_clear_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);
  g_prompt.overlay = overlay;

  lv_obj_t *box = lv_obj_create(overlay);
  lv_obj_set_size(box, 200, 160);
  lv_obj_center(box);
  lv_obj_set_style_bg_color(box, kColorHeader, 0);
  lv_obj_set_style_border_width(box, 0, 0);
  lv_obj_set_style_pad_all(box, 8, 0);
  lv_obj_set_flex_flow(box, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(box, 6, 0);

  lv_obj_t *title_label = lv_label_create(box);
  lv_label_set_text(title_label, title);
  lv_obj_set_style_text_font(title_label, &lv_font_montserrat_18, 0);

  lv_obj_t *message_label = lv_label_create(box);
  lv_label_set_text(message_label, message);
  lv_obj_set_style_text_color(message_label, kColorMuted, 0);

  lv_obj_t *button_row = lv_obj_create(box);
  lv_obj_set_width(button_row, LV_PCT(100));
  lv_obj_set_height(button_row, 40);
  lv_obj_set_style_bg_opa(button_row, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(button_row, 0, 0);
  lv_obj_set_style_pad_all(button_row, 0, 0);
  lv_obj_set_style_pad_gap(button_row, 6, 0);
  lv_obj_set_flex_flow(button_row, LV_FLEX_FLOW_ROW);
  lv_obj_clear_flag(button_row, LV_OBJ_FLAG_SCROLLABLE);

  for (int i = 0; i < option_count; ++i) {
    lv_obj_t *btn = lv_btn_create(button_row);
    lv_obj_set_flex_grow(btn, 1);
    lv_obj_add_event_cb(btn, prompt_button_event, LV_EVENT_CLICKED, reinterpret_cast<void *>(static_cast<intptr_t>(i)));
    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, options[i]);
    lv_obj_center(label);
  }
}

static void set_active_screen(ScreenId id) {
  g_active_screen = id;
  if (!g_debug_label) return;
  const char *name = "";
  switch (id) {
    case SCREEN_INFO: name = "INFO"; break;
    case SCREEN_MENU: name = "MENU"; break;
    case SCREEN_LOGS: name = "LOGS"; break;
    case SCREEN_FEEDING_MENU: name = "FEED"; break;
    case SCREEN_SLOTS_LIST: name = "SLOTS"; break;
    case SCREEN_SLOT_SUMMARY: name = "SLOT"; break;
    case SCREEN_SLOT_WIZARD: name = "EDIT"; break;
    case SCREEN_SETTINGS_MENU: name = "SET"; break;
    case SCREEN_TIME_DATE: name = "TIME"; break;
    case SCREEN_CAL_MENU: name = "CAL"; break;
    case SCREEN_CAL_MOIST: name = "CALM"; break;
    case SCREEN_CAL_FLOW: name = "CALF"; break;
    case SCREEN_TEST_MENU: name = "TEST"; break;
    case SCREEN_TEST_SENSORS: name = "SENS"; break;
    case SCREEN_TEST_PUMPS: name = "PUMP"; break;
    case SCREEN_RESET_MENU: name = "RST"; break;
    case SCREEN_INITIAL_SETUP: name = "SETUP"; break;
    case SCREEN_NUMBER_INPUT: name = "NUM"; break;
    case SCREEN_TIME_RANGE_INPUT: name = "LIGHT"; break;
    default: name = "UI"; break;
  }
  lv_label_set_text_fmt(g_debug_label, "[%s]", name);
}

static void push_screen(ScreenId id);
static void pop_screen();
static void update_active_screen();

static uint32_t tick_get_cb() {
  return millis();
}

static void disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
  uint32_t w = static_cast<uint32_t>(area->x2 - area->x1 + 1);
  uint32_t h = static_cast<uint32_t>(area->y2 - area->y1 + 1);

  tft.startWrite();
  tft.setAddrWindow(area->x1, area->y1, w, h);
  tft.writePixels(reinterpret_cast<uint16_t *>(px_map), w * h, true);
  tft.endWrite();

  lv_display_flush_ready(disp);
}

static void touch_read(lv_indev_t *, lv_indev_data_t *data) {
  if (!touchReady || !ctp.touched()) {
    data->state = LV_INDEV_STATE_RELEASED;
    return;
  }

  TS_Point p = ctp.getPoint();
  int32_t x = map(p.x, 0, 240, 240, 0);
  int32_t y = map(p.y, 0, 320, 320, 0);

  if (x < 0) x = 0;
  else if (x >= kScreenWidth) x = kScreenWidth - 1;

  if (y < 0) y = 0;
  else if (y >= kScreenHeight) y = kScreenHeight - 1;

  data->point.x = static_cast<int16_t>(x);
  data->point.y = static_cast<int16_t>(y);
  data->state = LV_INDEV_STATE_PRESSED;

  g_last_touch_ms = millis();
  g_screensaver_active = false;
}

static void init_slots() {
  Slot defaults[kSlotCount] = {
    {true, "Morning", true, 6, 0, 90, MODE_PERCENT, 45, MODE_PERCENT, 62, 350, true, RUNOFF_NEITHER, 60, 120},
    {true, "Midday", true, 11, 0, 120, MODE_PERCENT, 48, MODE_PERCENT, 65, 300, false, RUNOFF_NEITHER, 60, 90},
    {true, "Evening", true, 16, 30, 120, MODE_PERCENT, 46, MODE_PERCENT, 63, 320, true, RUNOFF_AVOID, 60, 90},
    {false, "Slot 4", false, 0, 0, 60, MODE_OFF, 0, MODE_OFF, 0, 250, false, RUNOFF_NEITHER, 60, 90},
    {false, "Slot 5", false, 0, 0, 60, MODE_OFF, 0, MODE_OFF, 0, 250, false, RUNOFF_NEITHER, 60, 90},
    {false, "Slot 6", false, 0, 0, 60, MODE_OFF, 0, MODE_OFF, 0, 250, false, RUNOFF_NEITHER, 60, 90}
  };

  for (int i = 0; i < kSlotCount; ++i) {
    g_slots[i] = defaults[i];
  }
}

static void add_log(const LogEntry &entry) {
  if (g_log_count < kMaxLogs) {
    g_log_count++;
  }
  for (int i = g_log_count - 1; i > 0; --i) {
    g_logs[i] = g_logs[i - 1];
  }
  g_logs[0] = entry;
}

static LogEntry build_value_log() {
  LogEntry entry = {};
  entry.type = 2;
  entry.dt = g_sim.now;
  entry.soil_pct = g_sim.moisture;
  entry.dryback = g_sim.dryback;
  return entry;
}

static LogEntry build_boot_log() {
  LogEntry entry = {};
  entry.type = 0;
  entry.dt = g_sim.now;
  entry.soil_pct = g_sim.moisture;
  entry.dryback = g_sim.dryback;
  return entry;
}

static LogEntry build_feed_log() {
  LogEntry entry = {};
  entry.type = 1;
  entry.dt = g_sim.now;
  entry.slot = g_sim.feeding_slot + 1;
  entry.start_moisture = clamp_int(g_sim.moisture - 6, 0, 100);
  entry.end_moisture = g_sim.moisture;
  entry.volume_ml = g_sim.feeding_water_ml;
  entry.daily_total_ml = g_sim.daily_total_ml;
  entry.dryback = g_sim.dryback;
  entry.runoff = g_sim.runoff;
  strncpy(entry.stop_reason, g_sim.runoff ? "Runoff" : "Target", sizeof(entry.stop_reason) - 1);
  entry.stop_reason[sizeof(entry.stop_reason) - 1] = '\0';
  return entry;
}

static void sim_start_feed(int slot_index) {
  if (g_sim.feeding_active) return;
  g_sim.feeding_active = true;
  g_sim.feeding_slot = slot_index;
  g_sim.feeding_elapsed = 0;
  g_sim.feeding_water_ml = 0;
  g_sim.feeding_max_ml = g_slots[slot_index].max_ml;
}

static void sim_end_feed() {
  g_sim.feeding_active = false;
  g_sim.last_feed_uptime = g_sim.uptime_sec;
  g_sim.last_feed_ml = g_sim.feeding_water_ml;
  g_sim.daily_total_ml += g_sim.feeding_water_ml;
  add_log(build_feed_log());
}

static void sim_init() {
  g_sim.now.year = 2025;
  g_sim.now.month = 2;
  g_sim.now.day = 14;
  g_sim.now.hour = 9;
  g_sim.now.minute = 16;
  g_sim.now.second = 0;
  g_sim.uptime_sec = 600;
  g_sim.moisture = 52;
  g_sim.dryback = 10;
  g_sim.last_feed_ml = 320;
  g_sim.last_feed_uptime = 520;
  g_sim.daily_total_ml = 840;
  g_sim.baseline_x = 6;
  g_sim.baseline_y = 12;
  g_sim.baseline_delay_min = 25;
  g_sim.max_daily_ml = 1200;
  g_sim.lights_on_hour = 6;
  g_sim.lights_on_min = 0;
  g_sim.lights_off_hour = 20;
  g_sim.lights_off_min = 0;
  g_sim.paused = false;
  g_sim.feeding_active = false;
  g_sim.feeding_slot = 0;
  g_sim.feeding_elapsed = 0;
  g_sim.feeding_water_ml = 0;
  g_sim.feeding_max_ml = 350;
  g_sim.runoff = false;
  g_sim.info_toggle = 0;
  g_sim.moisture_raw = 700;
  g_sim.dripper_ml_per_hour = 1800;
  g_sim.moisture_raw_dry = 780;
  g_sim.moisture_raw_wet = 350;
  g_sim.last_values_log_uptime = 0;

  g_setup.time_date = false;
  g_setup.max_daily = false;
  g_setup.lights = false;
  g_setup.moisture_cal = false;
  g_setup.dripper_cal = false;

  init_slots();

  g_log_count = 0;
  add_log(build_boot_log());
  add_log(build_value_log());
}

static void sim_tick() {
  g_sim.uptime_sec += 1;
  advance_time_one_sec(&g_sim.now);

  if (g_sim.now.hour == 0 && g_sim.now.minute == 0 && g_sim.now.second == 0) {
    g_sim.daily_total_ml = 0;
  }

  if (!g_sim.paused && !g_sim.feeding_active) {
    if ((g_sim.uptime_sec - g_sim.last_feed_uptime) > kFeedIntervalSec) {
      sim_start_feed(static_cast<int>(g_sim.uptime_sec % kSlotCount));
    }
  }

  if (g_sim.feeding_active) {
    g_sim.feeding_elapsed += 1;
    g_sim.feeding_water_ml += 20;
    g_sim.moisture = clamp_int(g_sim.moisture + 1, 20, 90);
    if (g_sim.feeding_elapsed >= static_cast<int>(kFeedDurationSec) ||
        g_sim.feeding_water_ml >= g_sim.feeding_max_ml) {
      sim_end_feed();
    }
  } else {
    if ((g_sim.uptime_sec % 5) == 0) {
      g_sim.moisture = clamp_int(g_sim.moisture - 1, 20, 90);
    }
  }

  g_sim.runoff = g_sim.moisture > 80;
  g_sim.dryback = clamp_int(g_sim.baseline_y - (g_sim.moisture - 40), 0, 50);
  g_sim.moisture_raw = clamp_int(920 - g_sim.moisture * 6, 200, 950);

  if (g_sim.uptime_sec - g_sim.last_values_log_uptime >= 300) {
    g_sim.last_values_log_uptime = g_sim.uptime_sec;
    add_log(build_value_log());
  }

  if ((g_sim.uptime_sec % 5) == 0) {
    g_sim.info_toggle = 1 - g_sim.info_toggle;
  }
}

static void update_info_screen() {
  if (!g_info_refs.line0) return;

  if (g_screensaver_active) {
    lv_obj_add_flag(g_info_refs.container, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(g_info_refs.star, LV_OBJ_FLAG_HIDDEN);
    return;
  }

  lv_obj_clear_flag(g_info_refs.container, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(g_info_refs.star, LV_OBJ_FLAG_HIDDEN);

  if (g_sim.feeding_active) {
    lv_label_set_text_fmt(g_info_refs.line0, "Feeding S%d %ds/%ds", g_sim.feeding_slot + 1, 2, 3);
    lv_label_set_text_fmt(g_info_refs.line1, "Pump:%s Mst:%d%% %ds",
                          g_sim.feeding_active ? "ON" : "OFF",
                          g_sim.moisture, g_sim.feeding_elapsed);
    if (g_slots[g_sim.feeding_slot].target_mode != MODE_OFF) {
      lv_label_set_text_fmt(g_info_refs.line2, "Stop: M>%d%% %s",
                            g_slots[g_sim.feeding_slot].target_value,
                            g_slots[g_sim.feeding_slot].stop_on_runoff ? "Runoff" : "");
    } else {
      lv_label_set_text(g_info_refs.line2, "Stop: N/A");
    }
    lv_label_set_text_fmt(g_info_refs.line3, "Max:%dml W:%dml",
                          g_sim.feeding_max_ml, g_sim.feeding_water_ml);
    return;
  }

  char time_buf[8] = {0};
  format_time(g_sim.now, time_buf, sizeof(time_buf));
  lv_label_set_text_fmt(g_info_refs.line0, "Moist:%d%% Db:%d%%  %s", g_sim.moisture, g_sim.dryback, time_buf);

  bool day_now = false;
  int lights_on_min = g_sim.lights_on_hour * 60 + g_sim.lights_on_min;
  int lights_off_min = g_sim.lights_off_hour * 60 + g_sim.lights_off_min;
  int now_min = g_sim.now.hour * 60 + g_sim.now.minute;
  if (lights_on_min != lights_off_min) {
    if (lights_off_min > lights_on_min) {
      day_now = (now_min >= lights_on_min && now_min < lights_off_min);
    } else {
      day_now = (now_min >= lights_on_min || now_min < lights_off_min);
    }
  }

  if (g_sim.paused) {
    lv_label_set_text_fmt(g_info_refs.line1, "%s (PAUSED)", day_now ? "DAY" : "NIGHT");
  } else {
    lv_label_set_text(g_info_refs.line1, day_now ? "DAY" : "NIGHT");
  }

  char ago_buf[12] = {0};
  uint32_t elapsed = (g_sim.uptime_sec >= g_sim.last_feed_uptime)
                       ? (g_sim.uptime_sec - g_sim.last_feed_uptime)
                       : 0;
  format_ago(elapsed, ago_buf, sizeof(ago_buf));
  lv_label_set_text_fmt(g_info_refs.line2, "Last: %s ago %dml", ago_buf, g_sim.last_feed_ml);

  if (g_sim.info_toggle == 0) {
    lv_label_set_text_fmt(g_info_refs.line3, "Today: %dml  BL:%d%%", g_sim.daily_total_ml,
                          clamp_int(g_sim.moisture - g_sim.baseline_x, 10, 90));
  } else {
    lv_label_set_text_fmt(g_info_refs.line3, "Today: %dml  L%d H%d", g_sim.daily_total_ml,
                          clamp_int(g_sim.moisture - g_sim.baseline_x, 10, 90),
                          clamp_int(g_sim.moisture + g_sim.baseline_y, 10, 90));
  }
}

static void update_logs_screen() {
  if (!g_logs_refs.header) return;
  if (g_log_count == 0) {
    lv_label_set_text(g_logs_refs.header, "No logs");
    lv_label_set_text(g_logs_refs.line1, "");
    lv_label_set_text(g_logs_refs.line2, "");
    lv_label_set_text(g_logs_refs.line3, "");
    return;
  }

  if (g_log_index < 0) g_log_index = 0;
  if (g_log_index >= g_log_count) g_log_index = g_log_count - 1;
  const LogEntry &entry = g_logs[g_log_index];

  char dt_buf[16] = {0};
  format_datetime(entry.dt, dt_buf, sizeof(dt_buf));

  if (entry.type == 0) {
    lv_label_set_text_fmt(g_logs_refs.header, "# Boot");
    lv_label_set_text_fmt(g_logs_refs.line1, "At: %s", dt_buf);
    lv_label_set_text_fmt(g_logs_refs.line2, "Soil: %d%%", entry.soil_pct);
    lv_label_set_text_fmt(g_logs_refs.line3, "Db: %d%%", entry.dryback);
  } else if (entry.type == 1) {
    lv_label_set_text_fmt(g_logs_refs.header, "# Feed S%d %s", entry.slot, entry.stop_reason);
    lv_label_set_text_fmt(g_logs_refs.line1, "Start: %s", dt_buf);
    lv_label_set_text_fmt(g_logs_refs.line2, "V:%dml Mst:%d-%d%%", entry.volume_ml,
                          entry.start_moisture, entry.end_moisture);
    lv_label_set_text_fmt(g_logs_refs.line3, "T:%dml Db:%d%% %s",
                          entry.daily_total_ml, entry.dryback, entry.runoff ? "R" : "");
  } else {
    lv_label_set_text_fmt(g_logs_refs.header, "# Values");
    lv_label_set_text_fmt(g_logs_refs.line1, "At: %s", dt_buf);
    lv_label_set_text_fmt(g_logs_refs.line2, "Moisture: %d%%", entry.soil_pct);
    lv_label_set_text_fmt(g_logs_refs.line3, "Db: %d%%", entry.dryback);
  }

  if (g_logs_refs.index_label) {
    lv_label_set_text_fmt(g_logs_refs.index_label, "%d/%d", g_log_index + 1, g_log_count);
  }
}

static void update_cal_moist_screen() {
  if (!g_cal_moist_refs.raw_label) return;
  lv_label_set_text_fmt(g_cal_moist_refs.raw_label, "Raw: %d", g_sim.moisture_raw);
  lv_label_set_text_fmt(g_cal_moist_refs.percent_label, "Moist: %d%%", g_sim.moisture);
  if (g_cal_moist_mode == 1) {
    lv_label_set_text(g_cal_moist_refs.mode_label, "Mode: Dry");
  } else if (g_cal_moist_mode == 2) {
    lv_label_set_text(g_cal_moist_refs.mode_label, "Mode: Wet");
  } else {
    lv_label_set_text(g_cal_moist_refs.mode_label, "Mode: Choose");
  }
}

static void update_test_sensors_screen() {
  if (!g_test_sensors_refs.raw_label) return;
  lv_label_set_text_fmt(g_test_sensors_refs.raw_label, "Raw: %d", g_sim.moisture_raw);
  lv_label_set_text_fmt(g_test_sensors_refs.percent_label, "Moisture: %d%%", g_sim.moisture);
  lv_label_set_text_fmt(g_test_sensors_refs.runoff_label, "Runoff: %s", g_sim.runoff ? "1" : "0");
}

static void update_cal_flow_screen() {
  if (!g_cal_flow_refs.step_label) return;
  const char *step_title = "";
  if (g_cal_flow_step == 0) step_title = "Prime pump";
  else if (g_cal_flow_step == 1) step_title = "Fill 100ml";
  else step_title = "Compute flow";
  lv_label_set_text_fmt(g_cal_flow_refs.step_label, "Step %d: %s", g_cal_flow_step + 1, step_title);

  if (g_cal_flow_step == 2) {
    lv_label_set_text_fmt(g_cal_flow_refs.status_label, "Flow: %d ml/h", g_sim.dripper_ml_per_hour);
  } else {
    lv_label_set_text_fmt(g_cal_flow_refs.status_label, "State: %s", g_cal_flow_running ? "Running" : "Stopped");
  }

  if (g_cal_flow_refs.action_label) {
    if (g_cal_flow_step == 2) {
      lv_label_set_text(g_cal_flow_refs.action_label, "Save");
    } else {
      lv_label_set_text(g_cal_flow_refs.action_label, g_cal_flow_running ? "Stop" : "Start");
    }
  }
}

static void update_pump_test_screen() {
  if (!g_pump_test_refs.status_label) return;
  uint32_t now_ms = millis();
  if (g_pump_test_cycle >= 3) {
    lv_label_set_text(g_pump_test_refs.status_label, "Done. Press Back.");
    return;
  }
  if (now_ms - g_pump_test_last_ms >= 1000) {
    g_pump_test_last_ms = now_ms;
    g_pump_test_cycle++;
  }
  if (g_pump_test_cycle >= 3) {
    lv_label_set_text(g_pump_test_refs.status_label, "Done. Press Back.");
  } else {
    lv_label_set_text_fmt(g_pump_test_refs.status_label, "Blinking... %d/3", g_pump_test_cycle + 1);
  }
}

static void update_screensaver(uint32_t now_ms) {
  if (g_active_screen != SCREEN_INFO) {
    g_screensaver_active = false;
    return;
  }
  if (g_sim.feeding_active) {
    g_screensaver_active = false;
    return;
  }
  if (now_ms - g_last_touch_ms > kScreensaverDelayMs) {
    if (!g_screensaver_active) {
      g_screensaver_active = true;
      g_last_screensaver_move_ms = 0;
    }
  } else {
    g_screensaver_active = false;
  }

  if (g_screensaver_active && g_info_refs.star) {
    if (now_ms - g_last_screensaver_move_ms > kScreensaverMoveMs) {
      g_last_screensaver_move_ms = now_ms;
      int x = random(0, kScreenWidth - 10);
      int y = random(0, kScreenHeight - 10);
      lv_obj_set_pos(g_info_refs.star, x, y);
    }
  }
}

static void update_active_screen() {
  switch (g_active_screen) {
    case SCREEN_INFO:
      update_info_screen();
      break;
    case SCREEN_LOGS:
      update_logs_screen();
      break;
    case SCREEN_CAL_MOIST:
      update_cal_moist_screen();
      break;
    case SCREEN_TEST_SENSORS:
      update_test_sensors_screen();
      break;
    case SCREEN_CAL_FLOW:
      update_cal_flow_screen();
      break;
    case SCREEN_TEST_PUMPS:
      update_pump_test_screen();
      break;
    default:
      break;
  }
}

static void ui_timer_cb(lv_timer_t *) {
  static uint32_t last_ms = 0;
  uint32_t now_ms = millis();
  if (last_ms == 0) last_ms = now_ms;
  if (now_ms - last_ms >= 1000) {
    last_ms += 1000;
    sim_tick();
  }
  update_screensaver(now_ms);
  update_active_screen();
}

static void back_event(lv_event_t *) {
  pop_screen();
}

static void open_menu_event(lv_event_t *);
static void open_logs_event(lv_event_t *);
static void open_feeding_event(lv_event_t *);
static void open_force_feed_event(lv_event_t *);
static void toggle_pause_event(lv_event_t *);
static void open_settings_menu_event(lv_event_t *);
static void open_time_date_event(lv_event_t *);
static void open_feeding_schedule_event(lv_event_t *);
static void open_max_daily_event(lv_event_t *);
static void open_baseline_x_event(lv_event_t *);
static void open_baseline_y_event(lv_event_t *);
static void open_baseline_delay_event(lv_event_t *);
static void open_lights_event(lv_event_t *);
static void slot_select_event(lv_event_t *event);
static void edit_slot_event(lv_event_t *event);
static void feed_now_event(lv_event_t *event);
static void logs_prev_event(lv_event_t *event);
static void logs_next_event(lv_event_t *event);
static void wizard_back_event(lv_event_t *event);
static void wizard_next_event(lv_event_t *event);
static void wizard_save_event(lv_event_t *event);
static void time_date_back_event(lv_event_t *event);
static void time_date_next_event(lv_event_t *event);
static void time_date_save_event(lv_event_t *event);
static void time_date_cancel_event(lv_event_t *event);
static void open_cal_menu_event(lv_event_t *event);
static void open_test_menu_event(lv_event_t *event);
static void open_reset_menu_event(lv_event_t *event);
static void open_cal_moist_event(lv_event_t *event);
static void open_cal_flow_event(lv_event_t *event);
static void open_test_sensors_event(lv_event_t *event);
static void open_test_pumps_event(lv_event_t *event);
static void reset_logs_event(lv_event_t *event);
static void reset_factory_event(lv_event_t *event);
static void open_initial_setup_event(lv_event_t *event);
static void number_input_ok_event(lv_event_t *event);
static void time_range_ok_event(lv_event_t *event);
static void cal_moist_mode_event(lv_event_t *event);
static void cal_moist_save_event(lv_event_t *event);
static void cal_flow_action_event(lv_event_t *event);
static void cal_flow_next_event(lv_event_t *event);

static lv_obj_t *build_info_screen() {
  lv_obj_t *screen = create_screen_root();
  lv_obj_set_style_pad_all(screen, 12, 0);

  lv_obj_t *title = lv_label_create(screen);
  lv_label_set_text(title, "Ambience");
  lv_obj_set_style_text_font(title, &lv_font_montserrat_22, 0);
  lv_obj_set_style_text_color(title, kColorAccent, 0);
  lv_obj_set_width(title, LV_PCT(100));
  lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);

  lv_obj_t *container = lv_obj_create(screen);
  lv_obj_set_width(container, LV_PCT(100));
  lv_obj_set_flex_flow(container, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_bg_opa(container, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(container, 0, 0);
  lv_obj_set_style_pad_row(container, 6, 0);
  lv_obj_set_flex_grow(container, 1);
  lv_obj_clear_flag(container, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *line0 = lv_label_create(container);
  lv_obj_t *line1 = lv_label_create(container);
  lv_obj_t *line2 = lv_label_create(container);
  lv_obj_t *line3 = lv_label_create(container);

  lv_obj_set_style_text_font(line0, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_font(line1, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_font(line2, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_font(line3, &lv_font_montserrat_14, 0);

  lv_obj_t *menu_btn = lv_btn_create(screen);
  lv_obj_set_size(menu_btn, LV_PCT(100), 38);
  lv_obj_add_event_cb(menu_btn, open_menu_event, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *menu_label = lv_label_create(menu_btn);
  lv_label_set_text(menu_label, "Menu");
  lv_obj_center(menu_label);

  lv_obj_t *star = lv_label_create(screen);
  lv_label_set_text(star, "*");
  lv_obj_set_style_text_font(star, &lv_font_montserrat_18, 0);
  lv_obj_add_flag(star, LV_OBJ_FLAG_HIDDEN);

  g_info_refs.container = container;
  g_info_refs.line0 = line0;
  g_info_refs.line1 = line1;
  g_info_refs.line2 = line2;
  g_info_refs.line3 = line3;
  g_info_refs.star = star;

  update_info_screen();
  return screen;
}

static lv_obj_t *build_menu_screen() {
  lv_obj_t *screen = create_screen_root();
  create_header(screen, "Main Menu", true, back_event);

  lv_obj_t *list = create_menu_list(screen);
  add_menu_item(list, "Logs", nullptr, open_logs_event, nullptr, nullptr);
  add_menu_item(list, "Feeding", nullptr, open_feeding_event, nullptr, nullptr);
  add_menu_item(list, "Force feed", nullptr, open_force_feed_event, nullptr, nullptr);

  lv_obj_t *pause_label = nullptr;
  add_menu_item(list, g_sim.paused ? "Unpause feeding" : "Pause feeding", nullptr,
                toggle_pause_event, nullptr, &pause_label);
  g_pause_menu_label = pause_label;

  add_menu_item(list, "Settings", nullptr, open_settings_menu_event, nullptr, nullptr);

  return screen;
}

static lv_obj_t *build_logs_screen() {
  lv_obj_t *screen = create_screen_root();
  create_header(screen, "Logs", true, back_event);

  lv_obj_t *content = lv_obj_create(screen);
  lv_obj_set_width(content, LV_PCT(100));
  lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(content, 0, 0);
  lv_obj_set_style_pad_row(content, 6, 0);
  lv_obj_set_flex_grow(content, 1);
  lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *header = lv_label_create(content);
  lv_obj_set_style_text_font(header, &lv_font_montserrat_18, 0);

  lv_obj_t *line1 = lv_label_create(content);
  lv_obj_t *line2 = lv_label_create(content);
  lv_obj_t *line3 = lv_label_create(content);

  lv_obj_t *footer = lv_obj_create(screen);
  lv_obj_set_width(footer, LV_PCT(100));
  lv_obj_set_height(footer, 36);
  lv_obj_set_style_bg_opa(footer, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(footer, 0, 0);
  lv_obj_set_style_pad_all(footer, 0, 0);
  lv_obj_set_style_pad_gap(footer, 6, 0);
  lv_obj_set_flex_flow(footer, LV_FLEX_FLOW_ROW);
  lv_obj_clear_flag(footer, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *prev_btn = lv_btn_create(footer);
  lv_obj_set_flex_grow(prev_btn, 1);
  lv_obj_add_event_cb(prev_btn, logs_prev_event, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *prev_label = lv_label_create(prev_btn);
  lv_label_set_text(prev_label, "Prev");
  lv_obj_center(prev_label);

  lv_obj_t *index_label = lv_label_create(footer);
  lv_label_set_text(index_label, "1/1");
  lv_obj_set_style_text_color(index_label, kColorMuted, 0);
  lv_obj_align(index_label, LV_ALIGN_CENTER, 0, 0);

  lv_obj_t *next_btn = lv_btn_create(footer);
  lv_obj_set_flex_grow(next_btn, 1);
  lv_obj_add_event_cb(next_btn, logs_next_event, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *next_label = lv_label_create(next_btn);
  lv_label_set_text(next_label, "Next");
  lv_obj_center(next_label);

  g_logs_refs.header = header;
  g_logs_refs.line1 = line1;
  g_logs_refs.line2 = line2;
  g_logs_refs.line3 = line3;
  g_logs_refs.index_label = index_label;

  g_log_index = 0;
  update_logs_screen();
  return screen;
}

static lv_obj_t *build_feeding_menu_screen() {
  lv_obj_t *screen = create_screen_root();
  create_header(screen, "Feeding", true, back_event);

  lv_obj_t *list = create_menu_list(screen);
  add_menu_item(list, "Schedule", nullptr, open_feeding_schedule_event, nullptr, nullptr);
  add_menu_item(list, "Max daily", nullptr, open_max_daily_event, nullptr, nullptr);
  add_menu_item(list, "Baseline - X", nullptr, open_baseline_x_event, nullptr, nullptr);
  add_menu_item(list, "Baseline - Y", nullptr, open_baseline_y_event, nullptr, nullptr);
  add_menu_item(list, "Baseline delay", nullptr, open_baseline_delay_event, nullptr, nullptr);
  add_menu_item(list, "Lights on/off", nullptr, open_lights_event, nullptr, nullptr);

  return screen;
}

static void build_slot_summary_text(const Slot &slot, int slot_index, char *lines[4], size_t len) {
  static char line0[64];
  static char line1[64];
  static char line2[64];
  static char line3[64];

  snprintf(line0, sizeof(line0), "S%d %s [%s]", slot_index + 1, slot.name, slot.enabled ? "On" : "Off");

  if (slot.use_window) {
    snprintf(line1, sizeof(line1), "Start: %02d:%02d +%dmin", slot.start_hour, slot.start_min, slot.window_duration_min);
  } else {
    snprintf(line1, sizeof(line1), "Start: No time window");
  }
  if (slot.start_mode == MODE_PERCENT) {
    size_t len1 = strlen(line1);
    snprintf(line1 + len1, sizeof(line1) - len1, " M<%d%%", slot.start_value);
  } else if (slot.start_mode == MODE_BASELINE) {
    size_t len1 = strlen(line1);
    snprintf(line1 + len1, sizeof(line1) - len1, " B<%d", slot.start_value);
  }

  if (slot.target_mode == MODE_OFF) {
    snprintf(line2, sizeof(line2), "Stop: N/A");
  } else if (slot.target_mode == MODE_PERCENT) {
    snprintf(line2, sizeof(line2), "Stop: M>%d%%", slot.target_value);
  } else {
    snprintf(line2, sizeof(line2), "Stop: Baseline %d", slot.target_value);
  }

  snprintf(line3, sizeof(line3), "Max:%dml  Gap:%dmin", slot.max_ml, slot.min_gap_min);

  lines[0] = line0;
  lines[1] = line1;
  lines[2] = line2;
  lines[3] = line3;
  (void)len;
}

static void update_slot_summary_screen() {
  if (!g_slot_summary_refs.lines[0]) return;
  char *lines[4] = {nullptr};
  build_slot_summary_text(g_slots[g_selected_slot], g_selected_slot, lines, 4);
  for (int i = 0; i < 4; ++i) {
    lv_label_set_text(g_slot_summary_refs.lines[i], lines[i]);
  }
}

static lv_obj_t *build_slots_list_screen() {
  lv_obj_t *screen = create_screen_root();
  create_header(screen, g_force_feed_mode ? "Force feed" : "Feeding slots", true, back_event);

  lv_obj_t *list = create_menu_list(screen);
  for (int i = 0; i < kSlotCount; ++i) {
    lv_obj_t *btn = lv_btn_create(list);
    lv_obj_set_width(btn, LV_PCT(100));
    lv_obj_set_height(btn, 46);
    lv_obj_add_event_cb(btn, slot_select_event, LV_EVENT_CLICKED, reinterpret_cast<void *>(static_cast<intptr_t>(i)));

    lv_obj_t *name = lv_label_create(btn);
    lv_label_set_text_fmt(name, "S%d %s", i + 1, g_slots[i].name);
    lv_obj_align(name, LV_ALIGN_TOP_LEFT, 4, 2);

    lv_obj_t *summary = lv_label_create(btn);
    if (g_slots[i].use_window) {
      lv_label_set_text_fmt(summary, "%02d:%02d +%dmin", g_slots[i].start_hour, g_slots[i].start_min,
                            g_slots[i].window_duration_min);
    } else {
      lv_label_set_text(summary, "Window: Off");
    }
    lv_obj_set_style_text_color(summary, kColorMuted, 0);
    lv_obj_align(summary, LV_ALIGN_BOTTOM_LEFT, 4, -2);
  }

  return screen;
}

static lv_obj_t *build_slot_summary_screen() {
  lv_obj_t *screen = create_screen_root();
  char title[24] = {0};
  snprintf(title, sizeof(title), "Slot %d", g_selected_slot + 1);
  create_header(screen, title, true, back_event);

  lv_obj_t *content = lv_obj_create(screen);
  lv_obj_set_width(content, LV_PCT(100));
  lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(content, 0, 0);
  lv_obj_set_style_pad_row(content, 6, 0);
  lv_obj_set_flex_grow(content, 1);
  lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);

  char *lines[4] = {nullptr};
  build_slot_summary_text(g_slots[g_selected_slot], g_selected_slot, lines, 4);

  for (int i = 0; i < 4; ++i) {
    lv_obj_t *label = lv_label_create(content);
    lv_label_set_text(label, lines[i]);
    g_slot_summary_refs.lines[i] = label;
  }

  update_slot_summary_screen();

  lv_obj_t *footer = lv_obj_create(screen);
  lv_obj_set_width(footer, LV_PCT(100));
  lv_obj_set_height(footer, 36);
  lv_obj_set_style_bg_opa(footer, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(footer, 0, 0);
  lv_obj_set_style_pad_all(footer, 0, 0);
  lv_obj_set_style_pad_gap(footer, 6, 0);
  lv_obj_set_flex_flow(footer, LV_FLEX_FLOW_ROW);
  lv_obj_clear_flag(footer, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *edit_btn = lv_btn_create(footer);
  lv_obj_set_flex_grow(edit_btn, 1);
  lv_obj_add_event_cb(edit_btn, edit_slot_event, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *edit_label = lv_label_create(edit_btn);
  lv_label_set_text(edit_label, "Edit");
  lv_obj_center(edit_label);

  lv_obj_t *feed_btn = lv_btn_create(footer);
  lv_obj_set_flex_grow(feed_btn, 1);
  lv_obj_add_event_cb(feed_btn, feed_now_event, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *feed_label = lv_label_create(feed_btn);
  lv_label_set_text(feed_label, "Feed now");
  lv_obj_center(feed_label);

  return screen;
}

static void wizard_render_step();

static lv_obj_t *build_slot_wizard_screen() {
  lv_obj_t *screen = create_screen_root();
  create_header(screen, "Edit slot", false, nullptr);

  lv_obj_t *step_label = lv_label_create(screen);
  lv_obj_set_style_text_color(step_label, kColorMuted, 0);

  lv_obj_t *content = lv_obj_create(screen);
  lv_obj_set_width(content, LV_PCT(100));
  lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(content, 0, 0);
  lv_obj_set_style_pad_row(content, 6, 0);
  lv_obj_set_flex_grow(content, 1);
  lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *footer = lv_obj_create(screen);
  lv_obj_set_width(footer, LV_PCT(100));
  lv_obj_set_height(footer, 36);
  lv_obj_set_style_bg_opa(footer, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(footer, 0, 0);
  lv_obj_set_style_pad_all(footer, 0, 0);
  lv_obj_set_style_pad_gap(footer, 6, 0);
  lv_obj_set_flex_flow(footer, LV_FLEX_FLOW_ROW);
  lv_obj_clear_flag(footer, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *back_btn = lv_btn_create(footer);
  lv_obj_set_flex_grow(back_btn, 1);
  lv_obj_add_event_cb(back_btn, wizard_back_event, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *back_label = lv_label_create(back_btn);
  lv_label_set_text(back_label, "Back");
  lv_obj_center(back_label);

  lv_obj_t *next_btn = lv_btn_create(footer);
  lv_obj_set_flex_grow(next_btn, 1);
  lv_obj_add_event_cb(next_btn, wizard_next_event, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *next_label = lv_label_create(next_btn);
  lv_label_set_text(next_label, "Next");
  lv_obj_center(next_label);

  g_wizard_refs.step_label = step_label;
  g_wizard_refs.content = content;
  g_wizard_refs.next_btn = next_btn;
  g_wizard_refs.next_label = next_label;
  g_wizard_refs.back_btn = back_btn;
  g_wizard_refs.text_area = nullptr;

  wizard_render_step();
  return screen;
}

static lv_obj_t *build_settings_menu_screen() {
  lv_obj_t *screen = create_screen_root();
  create_header(screen, "Settings", true, back_event);

  lv_obj_t *list = create_menu_list(screen);
  if (!setup_complete()) {
    add_menu_item(list, "Initial setup", nullptr, open_initial_setup_event, nullptr, nullptr);
  }
  add_menu_item(list, "Set time/date", nullptr, open_time_date_event, nullptr, nullptr);
  add_menu_item(list, "Calibrate", nullptr, open_cal_menu_event, nullptr, nullptr);
  add_menu_item(list, "Test peripherals", nullptr, open_test_menu_event, nullptr, nullptr);
  add_menu_item(list, "Reset", nullptr, open_reset_menu_event, nullptr, nullptr);

  return screen;
}

static void time_date_render_step();

static lv_obj_t *build_time_date_screen() {
  lv_obj_t *screen = create_screen_root();
  create_header(screen, "Set time/date", false, nullptr);

  lv_obj_t *step_label = lv_label_create(screen);
  lv_obj_set_style_text_color(step_label, kColorMuted, 0);

  lv_obj_t *content = lv_obj_create(screen);
  lv_obj_set_width(content, LV_PCT(100));
  lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(content, 0, 0);
  lv_obj_set_style_pad_row(content, 6, 0);
  lv_obj_set_flex_grow(content, 1);
  lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *footer = lv_obj_create(screen);
  lv_obj_set_width(footer, LV_PCT(100));
  lv_obj_set_height(footer, 36);
  lv_obj_set_style_bg_opa(footer, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(footer, 0, 0);
  lv_obj_set_style_pad_all(footer, 0, 0);
  lv_obj_set_style_pad_gap(footer, 6, 0);
  lv_obj_set_flex_flow(footer, LV_FLEX_FLOW_ROW);
  lv_obj_clear_flag(footer, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *back_btn = lv_btn_create(footer);
  lv_obj_set_flex_grow(back_btn, 1);
  lv_obj_add_event_cb(back_btn, time_date_back_event, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *back_label = lv_label_create(back_btn);
  lv_label_set_text(back_label, "Back");
  lv_obj_center(back_label);

  lv_obj_t *next_btn = lv_btn_create(footer);
  lv_obj_set_flex_grow(next_btn, 1);
  lv_obj_add_event_cb(next_btn, time_date_next_event, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *next_label = lv_label_create(next_btn);
  lv_label_set_text(next_label, "Next");
  lv_obj_center(next_label);

  g_time_date_refs.step_label = step_label;
  g_time_date_refs.content = content;
  g_time_date_refs.next_btn = next_btn;
  g_time_date_refs.next_label = next_label;
  g_time_date_refs.back_btn = back_btn;

  time_date_render_step();
  return screen;
}

static lv_obj_t *build_cal_menu_screen() {
  lv_obj_t *screen = create_screen_root();
  create_header(screen, "Calibrate", true, back_event);

  lv_obj_t *list = create_menu_list(screen);
  add_menu_item(list, "Cal moist sensor", nullptr, open_cal_moist_event, nullptr, nullptr);
  add_menu_item(list, "Calibrate flow", nullptr, open_cal_flow_event, nullptr, nullptr);

  return screen;
}

static lv_obj_t *build_cal_moist_screen() {
  lv_obj_t *screen = create_screen_root();
  create_header(screen, "Cal moisture", true, back_event);

  lv_obj_t *content = lv_obj_create(screen);
  lv_obj_set_width(content, LV_PCT(100));
  lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(content, 0, 0);
  lv_obj_set_style_pad_row(content, 6, 0);
  lv_obj_set_flex_grow(content, 1);
  lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *mode_label = lv_label_create(content);
  lv_obj_set_style_text_color(mode_label, kColorMuted, 0);

  lv_obj_t *row = lv_obj_create(content);
  lv_obj_set_width(row, LV_PCT(100));
  lv_obj_set_height(row, 36);
  lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(row, 0, 0);
  lv_obj_set_style_pad_all(row, 0, 0);
  lv_obj_set_style_pad_gap(row, 6, 0);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *dry_btn = lv_btn_create(row);
  lv_obj_add_event_cb(dry_btn, cal_moist_mode_event, LV_EVENT_CLICKED, reinterpret_cast<void *>(static_cast<intptr_t>(1)));
  lv_obj_t *dry_label = lv_label_create(dry_btn);
  lv_label_set_text(dry_label, "Dry");
  lv_obj_center(dry_label);

  lv_obj_t *wet_btn = lv_btn_create(row);
  lv_obj_add_event_cb(wet_btn, cal_moist_mode_event, LV_EVENT_CLICKED, reinterpret_cast<void *>(static_cast<intptr_t>(2)));
  lv_obj_t *wet_label = lv_label_create(wet_btn);
  lv_label_set_text(wet_label, "Wet");
  lv_obj_center(wet_label);

  lv_obj_t *raw_label = lv_label_create(content);
  lv_obj_t *percent_label = lv_label_create(content);

  lv_obj_t *save_btn = lv_btn_create(content);
  lv_obj_set_width(save_btn, LV_PCT(100));
  lv_obj_add_event_cb(save_btn, cal_moist_save_event, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *save_label = lv_label_create(save_btn);
  lv_label_set_text(save_label, "Save" );
  lv_obj_center(save_label);

  g_cal_moist_refs.mode_label = mode_label;
  g_cal_moist_refs.raw_label = raw_label;
  g_cal_moist_refs.percent_label = percent_label;

  update_cal_moist_screen();
  return screen;
}

static lv_obj_t *build_cal_flow_screen() {
  lv_obj_t *screen = create_screen_root();
  create_header(screen, "Cal flow", true, back_event);

  lv_obj_t *content = lv_obj_create(screen);
  lv_obj_set_width(content, LV_PCT(100));
  lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(content, 0, 0);
  lv_obj_set_style_pad_row(content, 6, 0);
  lv_obj_set_flex_grow(content, 1);
  lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *step_label = lv_label_create(content);
  lv_obj_set_style_text_color(step_label, kColorMuted, 0);
  lv_obj_t *status_label = lv_label_create(content);

  lv_obj_t *action_btn = lv_btn_create(content);
  lv_obj_set_width(action_btn, LV_PCT(100));
  lv_obj_add_event_cb(action_btn, cal_flow_action_event, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *action_label = lv_label_create(action_btn);
  lv_label_set_text(action_label, "Start");
  lv_obj_center(action_label);

  lv_obj_t *next_btn = lv_btn_create(content);
  lv_obj_set_width(next_btn, LV_PCT(100));
  lv_obj_add_event_cb(next_btn, cal_flow_next_event, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *next_label = lv_label_create(next_btn);
  lv_label_set_text(next_label, "Next");
  lv_obj_center(next_label);

  g_cal_flow_refs.step_label = step_label;
  g_cal_flow_refs.status_label = status_label;
  g_cal_flow_refs.action_label = action_label;

  update_cal_flow_screen();
  return screen;
}

static lv_obj_t *build_test_menu_screen() {
  lv_obj_t *screen = create_screen_root();
  create_header(screen, "Test peripherals", true, back_event);

  lv_obj_t *list = create_menu_list(screen);
  add_menu_item(list, "Test sensors", nullptr, open_test_sensors_event, nullptr, nullptr);
  add_menu_item(list, "Test pumps", nullptr, open_test_pumps_event, nullptr, nullptr);

  return screen;
}

static lv_obj_t *build_test_sensors_screen() {
  lv_obj_t *screen = create_screen_root();
  create_header(screen, "Sensor test", true, back_event);

  lv_obj_t *content = lv_obj_create(screen);
  lv_obj_set_width(content, LV_PCT(100));
  lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(content, 0, 0);
  lv_obj_set_style_pad_row(content, 6, 0);
  lv_obj_set_flex_grow(content, 1);
  lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *raw_label = lv_label_create(content);
  lv_obj_t *percent_label = lv_label_create(content);
  lv_obj_t *runoff_label = lv_label_create(content);

  g_test_sensors_refs.raw_label = raw_label;
  g_test_sensors_refs.percent_label = percent_label;
  g_test_sensors_refs.runoff_label = runoff_label;

  update_test_sensors_screen();
  return screen;
}

static lv_obj_t *build_test_pumps_screen() {
  lv_obj_t *screen = create_screen_root();
  create_header(screen, "Pump test", true, back_event);

  lv_obj_t *content = lv_obj_create(screen);
  lv_obj_set_width(content, LV_PCT(100));
  lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(content, 0, 0);
  lv_obj_set_style_pad_row(content, 6, 0);
  lv_obj_set_flex_grow(content, 1);
  lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *label = lv_label_create(content);
  lv_label_set_text(label, "Device blinks three times...");
  lv_obj_set_style_text_color(label, kColorMuted, 0);

  lv_obj_t *status_label = lv_label_create(content);

  g_pump_test_refs.status_label = status_label;
  g_pump_test_cycle = 0;
  g_pump_test_last_ms = millis();

  update_pump_test_screen();
  return screen;
}

static lv_obj_t *build_reset_menu_screen() {
  lv_obj_t *screen = create_screen_root();
  create_header(screen, "Reset", true, back_event);

  lv_obj_t *list = create_menu_list(screen);
  add_menu_item(list, "Reset logs", nullptr, reset_logs_event, nullptr, nullptr);
  add_menu_item(list, "Reset data", nullptr, reset_factory_event, nullptr, nullptr);

  return screen;
}

static void refresh_initial_setup_screen() {
  if (!g_initial_setup_list) return;
  lv_obj_clean(g_initial_setup_list);
  bool has_items = false;
  if (!g_setup.time_date) {
    add_menu_item(g_initial_setup_list, "Time/date", nullptr, open_time_date_event, nullptr, nullptr);
    has_items = true;
  }
  if (!g_setup.max_daily) {
    add_menu_item(g_initial_setup_list, "Max daily water", nullptr, open_max_daily_event, nullptr, nullptr);
    has_items = true;
  }
  if (!g_setup.lights) {
    add_menu_item(g_initial_setup_list, "Lights on/off", nullptr, open_lights_event, nullptr, nullptr);
    has_items = true;
  }
  if (!g_setup.moisture_cal) {
    add_menu_item(g_initial_setup_list, "Moisture calibration", nullptr, open_cal_moist_event, nullptr, nullptr);
    has_items = true;
  }
  if (!g_setup.dripper_cal) {
    add_menu_item(g_initial_setup_list, "Dripper calibration", nullptr, open_cal_flow_event, nullptr, nullptr);
    has_items = true;
  }

  if (!has_items) {
    lv_obj_t *label = lv_label_create(g_initial_setup_list);
    lv_label_set_text(label, "Setup complete.");
    lv_obj_set_style_text_color(label, kColorMuted, 0);
  }
}

static void maybe_refresh_initial_setup() {
  if (g_active_screen == SCREEN_INITIAL_SETUP) {
    refresh_initial_setup_screen();
  }
}

static lv_obj_t *build_initial_setup_screen() {
  lv_obj_t *screen = create_screen_root();
  create_header(screen, "Initial setup", true, back_event);

  lv_obj_t *list = create_menu_list(screen);
  g_initial_setup_list = list;
  refresh_initial_setup_screen();

  return screen;
}

static lv_obj_t *build_number_input_screen() {
  lv_obj_t *screen = create_screen_root();
  create_header(screen, g_number_ctx.title ? g_number_ctx.title : "Value", true, back_event);

  reset_binding_pool();

  lv_obj_t *content = lv_obj_create(screen);
  lv_obj_set_width(content, LV_PCT(100));
  lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(content, 0, 0);
  lv_obj_set_style_pad_row(content, 6, 0);
  lv_obj_set_flex_grow(content, 1);
  lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);

  create_number_selector(content, "Value", &g_number_ctx.value, g_number_ctx.min, g_number_ctx.max,
                         g_number_ctx.step, "%d");

  if (g_number_ctx.unit) {
    lv_obj_t *unit = lv_label_create(content);
    lv_label_set_text_fmt(unit, "Unit: %s", g_number_ctx.unit);
    lv_obj_set_style_text_color(unit, kColorMuted, 0);
  }

  lv_obj_t *ok_btn = lv_btn_create(content);
  lv_obj_set_width(ok_btn, LV_PCT(100));
  lv_obj_add_event_cb(ok_btn, number_input_ok_event, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *ok_label = lv_label_create(ok_btn);
  lv_label_set_text(ok_label, "OK");
  lv_obj_center(ok_label);

  return screen;
}

static lv_obj_t *build_time_range_screen() {
  lv_obj_t *screen = create_screen_root();
  create_header(screen, g_time_range_ctx.title ? g_time_range_ctx.title : "Lights", true, back_event);

  reset_binding_pool();

  lv_obj_t *content = lv_obj_create(screen);
  lv_obj_set_width(content, LV_PCT(100));
  lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(content, 0, 0);
  lv_obj_set_style_pad_row(content, 6, 0);
  lv_obj_set_flex_grow(content, 1);
  lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);

  create_time_input_block(content, "Lights on", &g_time_range_ctx.on_hour, &g_time_range_ctx.on_min);
  create_time_input_block(content, "Lights off", &g_time_range_ctx.off_hour, &g_time_range_ctx.off_min);

  lv_obj_t *ok_btn = lv_btn_create(content);
  lv_obj_set_width(ok_btn, LV_PCT(100));
  lv_obj_add_event_cb(ok_btn, time_range_ok_event, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *ok_label = lv_label_create(ok_btn);
  lv_label_set_text(ok_label, "OK");
  lv_obj_center(ok_label);

  return screen;
}

static lv_obj_t *build_screen(ScreenId id) {
  switch (id) {
    case SCREEN_INFO: return build_info_screen();
    case SCREEN_MENU: return build_menu_screen();
    case SCREEN_LOGS: return build_logs_screen();
    case SCREEN_FEEDING_MENU: return build_feeding_menu_screen();
    case SCREEN_SLOTS_LIST: return build_slots_list_screen();
    case SCREEN_SLOT_SUMMARY: return build_slot_summary_screen();
    case SCREEN_SLOT_WIZARD: return build_slot_wizard_screen();
    case SCREEN_SETTINGS_MENU: return build_settings_menu_screen();
    case SCREEN_TIME_DATE: return build_time_date_screen();
    case SCREEN_CAL_MENU: return build_cal_menu_screen();
    case SCREEN_CAL_MOIST: return build_cal_moist_screen();
    case SCREEN_CAL_FLOW: return build_cal_flow_screen();
    case SCREEN_TEST_MENU: return build_test_menu_screen();
    case SCREEN_TEST_SENSORS: return build_test_sensors_screen();
    case SCREEN_TEST_PUMPS: return build_test_pumps_screen();
    case SCREEN_RESET_MENU: return build_reset_menu_screen();
    case SCREEN_INITIAL_SETUP: return build_initial_setup_screen();
    case SCREEN_NUMBER_INPUT: return build_number_input_screen();
    case SCREEN_TIME_RANGE_INPUT: return build_time_range_screen();
    default: break;
  }
  return build_info_screen();
}

static void push_screen(ScreenId id) {
  if (g_screen_stack_size >= static_cast<int>(sizeof(g_screen_stack) / sizeof(g_screen_stack[0]))) return;
  prompt_close();
  lv_obj_t *root = build_screen(id);
  g_screen_stack[g_screen_stack_size++] = {id, root};
  lv_scr_load(root);
  set_active_screen(id);
}

static void pop_screen() {
  if (g_screen_stack_size <= 1) return;
  prompt_close();
  lv_obj_t *old = g_screen_stack[g_screen_stack_size - 1].root;
  g_screen_stack_size--;
  lv_scr_load(g_screen_stack[g_screen_stack_size - 1].root);
  set_active_screen(g_screen_stack[g_screen_stack_size - 1].id);
  lv_obj_del(old);
}

static void open_menu_event(lv_event_t *) {
  push_screen(SCREEN_MENU);
}

static void open_logs_event(lv_event_t *) {
  push_screen(SCREEN_LOGS);
}

static void open_feeding_event(lv_event_t *) {
  push_screen(SCREEN_FEEDING_MENU);
}

static void open_force_feed_event(lv_event_t *) {
  g_force_feed_mode = true;
  push_screen(SCREEN_SLOTS_LIST);
}

static void pause_prompt_handler(int option, int) {
  if (option == 0) {
    g_sim.paused = !g_sim.paused;
    if (g_pause_menu_label) {
      lv_label_set_text(g_pause_menu_label, g_sim.paused ? "Unpause feeding" : "Pause feeding");
    }
  }
}

static void toggle_pause_event(lv_event_t *) {
  const char *options[] = {"OK"};
  const char *title = g_sim.paused ? "Unpause feeding" : "Pause feeding";
  const char *message = g_sim.paused ? "Feeding resumed" : "Feeding paused";
  show_prompt(title, message, options, 1, pause_prompt_handler, 0);
}

static void open_settings_menu_event(lv_event_t *) {
  push_screen(SCREEN_SETTINGS_MENU);
}

static void open_time_date_event(lv_event_t *) {
  g_time_date_edit = g_sim.now;
  g_time_date_step = 0;
  push_screen(SCREEN_TIME_DATE);
}

static void open_feeding_schedule_event(lv_event_t *) {
  g_force_feed_mode = false;
  push_screen(SCREEN_SLOTS_LIST);
}

static void open_max_daily_event(lv_event_t *) {
  g_number_ctx.title = "Max daily";
  g_number_ctx.value = g_sim.max_daily_ml;
  g_number_ctx.min = 100;
  g_number_ctx.max = 5000;
  g_number_ctx.step = 100;
  g_number_ctx.unit = "ml";
  g_number_ctx.target = &g_sim.max_daily_ml;
  g_number_ctx.on_done = []() { g_setup.max_daily = true; };
  push_screen(SCREEN_NUMBER_INPUT);
}

static void open_baseline_x_event(lv_event_t *) {
  g_number_ctx.title = "Baseline - X";
  g_number_ctx.value = g_sim.baseline_x;
  g_number_ctx.min = 1;
  g_number_ctx.max = 20;
  g_number_ctx.step = 1;
  g_number_ctx.unit = "%";
  g_number_ctx.target = &g_sim.baseline_x;
  g_number_ctx.on_done = nullptr;
  push_screen(SCREEN_NUMBER_INPUT);
}

static void open_baseline_y_event(lv_event_t *) {
  g_number_ctx.title = "Baseline - Y";
  g_number_ctx.value = g_sim.baseline_y;
  g_number_ctx.min = 1;
  g_number_ctx.max = 30;
  g_number_ctx.step = 1;
  g_number_ctx.unit = "%";
  g_number_ctx.target = &g_sim.baseline_y;
  g_number_ctx.on_done = nullptr;
  push_screen(SCREEN_NUMBER_INPUT);
}

static void open_baseline_delay_event(lv_event_t *) {
  g_number_ctx.title = "Baseline delay";
  g_number_ctx.value = g_sim.baseline_delay_min;
  g_number_ctx.min = 5;
  g_number_ctx.max = 120;
  g_number_ctx.step = 5;
  g_number_ctx.unit = "min";
  g_number_ctx.target = &g_sim.baseline_delay_min;
  g_number_ctx.on_done = nullptr;
  push_screen(SCREEN_NUMBER_INPUT);
}

static void open_lights_event(lv_event_t *) {
  g_time_range_ctx.title = "Lights on/off";
  g_time_range_ctx.on_hour = g_sim.lights_on_hour;
  g_time_range_ctx.on_min = g_sim.lights_on_min;
  g_time_range_ctx.off_hour = g_sim.lights_off_hour;
  g_time_range_ctx.off_min = g_sim.lights_off_min;
  g_time_range_ctx.on_done = []() { g_setup.lights = true; };
  push_screen(SCREEN_TIME_RANGE_INPUT);
}

static void slot_select_event(lv_event_t *event) {
  int slot_index = static_cast<int>(reinterpret_cast<intptr_t>(lv_event_get_user_data(event)));
  if (slot_index < 0 || slot_index >= kSlotCount) return;
  g_selected_slot = slot_index;

  if (g_force_feed_mode) {
    const char *options[] = {"Feed", "Cancel"};
    show_prompt("Force feed", "Start feed now?", options, 2,
                [](int option, int context) {
                  if (option == 0) {
                    sim_start_feed(context);
                  }
                }, slot_index);
    return;
  }

  push_screen(SCREEN_SLOT_SUMMARY);
}

static void edit_slot_event(lv_event_t *) {
  g_edit_slot = g_slots[g_selected_slot];
  g_wizard_step = 0;
  push_screen(SCREEN_SLOT_WIZARD);
}

static void feed_now_event(lv_event_t *) {
  sim_start_feed(g_selected_slot);
  const char *options[] = {"OK"};
  show_prompt("Feed", "Feeding started", options, 1, nullptr, 0);
}

static void logs_prev_event(lv_event_t *) {
  if (g_log_index + 1 < g_log_count) g_log_index++;
  update_logs_screen();
}

static void logs_next_event(lv_event_t *) {
  if (g_log_index > 0) g_log_index--;
  update_logs_screen();
}

static void wizard_apply_name() {
  if (!g_wizard_refs.text_area) return;
  const char *text = lv_textarea_get_text(g_wizard_refs.text_area);
  strncpy(g_edit_slot.name, text, sizeof(g_edit_slot.name) - 1);
  g_edit_slot.name[sizeof(g_edit_slot.name) - 1] = '\0';
}

static void wizard_back_event(lv_event_t *) {
  if (g_wizard_step == 0) {
    pop_screen();
    return;
  }
  if (g_wizard_step == 1) {
    wizard_apply_name();
  }
  g_wizard_step = clamp_int(g_wizard_step - 1, 0, 8);
  wizard_render_step();
}

static void wizard_next_event(lv_event_t *) {
  if (g_wizard_step == 1) {
    wizard_apply_name();
  }
  if (g_wizard_step < 8) {
    g_wizard_step++;
    wizard_render_step();
  }
}

static void wizard_save_handler(int option, int) {
  if (option == 0) {
    g_slots[g_selected_slot] = g_edit_slot;
    update_slot_summary_screen();
    pop_screen();
  } else if (option == 1) {
    update_slot_summary_screen();
    pop_screen();
  }
}

static void wizard_save_event(lv_event_t *event) {
  int option = static_cast<int>(reinterpret_cast<intptr_t>(lv_event_get_user_data(event)));
  if (option == 2) {
    return;
  }
  wizard_save_handler(option, 0);
}

static void wizard_render_step() {
  if (!g_wizard_refs.content) return;
  reset_binding_pool();
  reset_option_pool();
  lv_obj_clean(g_wizard_refs.content);
  g_wizard_refs.text_area = nullptr;

  lv_label_set_text_fmt(g_wizard_refs.step_label, "Step %d/9", g_wizard_step + 1);

  if (g_wizard_step == 0) {
    lv_obj_t *label = lv_label_create(g_wizard_refs.content);
    lv_label_set_text(label, "Enable slot?");

    OptionGroup *group = alloc_option_group();
    if (!group) return;
    group->target = &g_edit_slot.enabled;
    group->target_type = OPTION_TARGET_BOOL;

    lv_obj_t *row = lv_obj_create(g_wizard_refs.content);
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_set_style_pad_gap(row, 6, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    const char *labels[] = {"Off", "On"};
    int values[] = {0, 1};
    for (int i = 0; i < 2; ++i) {
      lv_obj_t *btn = lv_btn_create(row);
      lv_obj_add_flag(btn, LV_OBJ_FLAG_CHECKABLE);
      OptionButtonData *data = alloc_option_button();
      if (!data) continue;
      data->group = group;
      data->index = group->count;
      group->btns[group->count] = btn;
      group->values[group->count] = values[i];
      group->count++;
      lv_obj_add_event_cb(btn, option_select_event, LV_EVENT_CLICKED, data);
      lv_obj_t *lab = lv_label_create(btn);
      lv_label_set_text(lab, labels[i]);
      lv_obj_center(lab);
    }
    if (g_edit_slot.enabled) lv_obj_add_state(group->btns[1], LV_STATE_CHECKED);
    else lv_obj_add_state(group->btns[0], LV_STATE_CHECKED);
  } else if (g_wizard_step == 1) {
    lv_obj_t *label = lv_label_create(g_wizard_refs.content);
    lv_label_set_text(label, "Edit name");

    lv_obj_t *text_area = lv_textarea_create(g_wizard_refs.content);
    lv_textarea_set_one_line(text_area, true);
    lv_textarea_set_text(text_area, g_edit_slot.name);
    g_wizard_refs.text_area = text_area;

    lv_obj_t *keyboard = lv_keyboard_create(g_wizard_refs.content);
    lv_obj_set_height(keyboard, 110);
    lv_keyboard_set_textarea(keyboard, text_area);
  } else if (g_wizard_step == 2) {
    lv_obj_t *label = lv_label_create(g_wizard_refs.content);
    lv_label_set_text(label, "Time window?");

    OptionGroup *group = alloc_option_group();
    if (!group) return;
    group->target = &g_edit_slot.use_window;
    group->target_type = OPTION_TARGET_BOOL;

    lv_obj_t *row = lv_obj_create(g_wizard_refs.content);
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_set_style_pad_gap(row, 6, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    const char *labels[] = {"No", "Yes"};
    int values[] = {0, 1};
    for (int i = 0; i < 2; ++i) {
      lv_obj_t *btn = lv_btn_create(row);
      lv_obj_add_flag(btn, LV_OBJ_FLAG_CHECKABLE);
      OptionButtonData *data = alloc_option_button();
      if (!data) continue;
      data->group = group;
      data->index = group->count;
      group->btns[group->count] = btn;
      group->values[group->count] = values[i];
      group->count++;
      lv_obj_add_event_cb(btn, option_select_event, LV_EVENT_CLICKED, data);
      lv_obj_t *lab = lv_label_create(btn);
      lv_label_set_text(lab, labels[i]);
      lv_obj_center(lab);
    }
    if (g_edit_slot.use_window) lv_obj_add_state(group->btns[1], LV_STATE_CHECKED);
    else lv_obj_add_state(group->btns[0], LV_STATE_CHECKED);

    if (g_edit_slot.use_window) {
      create_time_input_block(g_wizard_refs.content, "Start time", &g_edit_slot.start_hour, &g_edit_slot.start_min);
      create_number_selector(g_wizard_refs.content, "Duration", &g_edit_slot.window_duration_min, 15, 240, 5, "%d");
    }
  } else if (g_wizard_step == 3) {
    lv_obj_t *label = lv_label_create(g_wizard_refs.content);
    lv_label_set_text(label, "Start when moist <");

    OptionGroup *group = alloc_option_group();
    if (!group) return;
    group->target = reinterpret_cast<int *>(&g_edit_slot.start_mode);
    group->target_type = OPTION_TARGET_INT;

    lv_obj_t *row = lv_obj_create(g_wizard_refs.content);
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_set_style_pad_gap(row, 6, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    const char *labels[] = {"Off", "%", "Base"};
    int values[] = {MODE_OFF, MODE_PERCENT, MODE_BASELINE};
    for (int i = 0; i < 3; ++i) {
      lv_obj_t *btn = lv_btn_create(row);
      lv_obj_add_flag(btn, LV_OBJ_FLAG_CHECKABLE);
      OptionButtonData *data = alloc_option_button();
      if (!data) continue;
      data->group = group;
      data->index = group->count;
      group->btns[group->count] = btn;
      group->values[group->count] = values[i];
      group->count++;
      lv_obj_add_event_cb(btn, option_select_event, LV_EVENT_CLICKED, data);
      lv_obj_t *lab = lv_label_create(btn);
      lv_label_set_text(lab, labels[i]);
      lv_obj_center(lab);
    }
    if (g_edit_slot.start_mode == MODE_OFF) lv_obj_add_state(group->btns[0], LV_STATE_CHECKED);
    if (g_edit_slot.start_mode == MODE_PERCENT) lv_obj_add_state(group->btns[1], LV_STATE_CHECKED);
    if (g_edit_slot.start_mode == MODE_BASELINE) lv_obj_add_state(group->btns[2], LV_STATE_CHECKED);

    if (g_edit_slot.start_mode != MODE_OFF) {
      create_number_selector(g_wizard_refs.content, "Value", &g_edit_slot.start_value, 20, 90, 1, "%d");
    }
  } else if (g_wizard_step == 4) {
    lv_obj_t *label = lv_label_create(g_wizard_refs.content);
    lv_label_set_text(label, "Stop when moist >");

    OptionGroup *group = alloc_option_group();
    if (!group) return;
    group->target = reinterpret_cast<int *>(&g_edit_slot.target_mode);
    group->target_type = OPTION_TARGET_INT;

    lv_obj_t *row = lv_obj_create(g_wizard_refs.content);
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_set_style_pad_gap(row, 6, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    const char *labels[] = {"Off", "%", "Base"};
    int values[] = {MODE_OFF, MODE_PERCENT, MODE_BASELINE};
    for (int i = 0; i < 3; ++i) {
      lv_obj_t *btn = lv_btn_create(row);
      lv_obj_add_flag(btn, LV_OBJ_FLAG_CHECKABLE);
      OptionButtonData *data = alloc_option_button();
      if (!data) continue;
      data->group = group;
      data->index = group->count;
      group->btns[group->count] = btn;
      group->values[group->count] = values[i];
      group->count++;
      lv_obj_add_event_cb(btn, option_select_event, LV_EVENT_CLICKED, data);
      lv_obj_t *lab = lv_label_create(btn);
      lv_label_set_text(lab, labels[i]);
      lv_obj_center(lab);
    }
    if (g_edit_slot.target_mode == MODE_OFF) lv_obj_add_state(group->btns[0], LV_STATE_CHECKED);
    if (g_edit_slot.target_mode == MODE_PERCENT) lv_obj_add_state(group->btns[1], LV_STATE_CHECKED);
    if (g_edit_slot.target_mode == MODE_BASELINE) lv_obj_add_state(group->btns[2], LV_STATE_CHECKED);

    if (g_edit_slot.target_mode != MODE_OFF) {
      create_number_selector(g_wizard_refs.content, "Value", &g_edit_slot.target_value, 20, 95, 1, "%d");
    }
  } else if (g_wizard_step == 5) {
    lv_obj_t *label = lv_label_create(g_wizard_refs.content);
    lv_label_set_text(label, "Max volume (ml)");
    create_number_selector(g_wizard_refs.content, "Max", &g_edit_slot.max_ml, 50, 1500, 10, "%d");
  } else if (g_wizard_step == 6) {
    lv_obj_t *label = lv_label_create(g_wizard_refs.content);
    lv_label_set_text(label, "Stop on runoff?");

    OptionGroup *group = alloc_option_group();
    if (!group) return;
    group->target = &g_edit_slot.stop_on_runoff;
    group->target_type = OPTION_TARGET_BOOL;

    lv_obj_t *row = lv_obj_create(g_wizard_refs.content);
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_set_style_pad_gap(row, 6, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    const char *labels[] = {"No", "Yes"};
    int values[] = {0, 1};
    for (int i = 0; i < 2; ++i) {
      lv_obj_t *btn = lv_btn_create(row);
      lv_obj_add_flag(btn, LV_OBJ_FLAG_CHECKABLE);
      OptionButtonData *data = alloc_option_button();
      if (!data) continue;
      data->group = group;
      data->index = group->count;
      group->btns[group->count] = btn;
      group->values[group->count] = values[i];
      group->count++;
      lv_obj_add_event_cb(btn, option_select_event, LV_EVENT_CLICKED, data);
      lv_obj_t *lab = lv_label_create(btn);
      lv_label_set_text(lab, labels[i]);
      lv_obj_center(lab);
    }
    if (g_edit_slot.stop_on_runoff) lv_obj_add_state(group->btns[1], LV_STATE_CHECKED);
    else lv_obj_add_state(group->btns[0], LV_STATE_CHECKED);

    if (g_edit_slot.stop_on_runoff) {
      OptionGroup *runoff_group = alloc_option_group();
      if (!runoff_group) return;
      runoff_group->target = reinterpret_cast<int *>(&g_edit_slot.runoff_mode);
      runoff_group->target_type = OPTION_TARGET_INT;

      lv_obj_t *runoff_row = lv_obj_create(g_wizard_refs.content);
      lv_obj_set_width(runoff_row, LV_PCT(100));
      lv_obj_set_style_bg_opa(runoff_row, LV_OPA_TRANSP, 0);
      lv_obj_set_style_border_width(runoff_row, 0, 0);
      lv_obj_set_style_pad_all(runoff_row, 0, 0);
      lv_obj_set_style_pad_gap(runoff_row, 6, 0);
      lv_obj_set_flex_flow(runoff_row, LV_FLEX_FLOW_ROW);
      lv_obj_clear_flag(runoff_row, LV_OBJ_FLAG_SCROLLABLE);

      const char *rlabels[] = {"Neither", "Must", "Avoid"};
      int rvalues[] = {RUNOFF_NEITHER, RUNOFF_MUST, RUNOFF_AVOID};
      for (int i = 0; i < 3; ++i) {
        lv_obj_t *btn = lv_btn_create(runoff_row);
        lv_obj_add_flag(btn, LV_OBJ_FLAG_CHECKABLE);
        OptionButtonData *data = alloc_option_button();
        if (!data) continue;
        data->group = runoff_group;
        data->index = runoff_group->count;
        runoff_group->btns[runoff_group->count] = btn;
        runoff_group->values[runoff_group->count] = rvalues[i];
        runoff_group->count++;
        lv_obj_add_event_cb(btn, option_select_event, LV_EVENT_CLICKED, data);
        lv_obj_t *lab = lv_label_create(btn);
        lv_label_set_text(lab, rlabels[i]);
        lv_obj_center(lab);
      }
      if (g_edit_slot.runoff_mode == RUNOFF_NEITHER) lv_obj_add_state(runoff_group->btns[0], LV_STATE_CHECKED);
      if (g_edit_slot.runoff_mode == RUNOFF_MUST) lv_obj_add_state(runoff_group->btns[1], LV_STATE_CHECKED);
      if (g_edit_slot.runoff_mode == RUNOFF_AVOID) lv_obj_add_state(runoff_group->btns[2], LV_STATE_CHECKED);
      create_number_selector(g_wizard_refs.content, "Runoff base", &g_edit_slot.runoff_baseline, 40, 90, 1, "%d");
    }
  } else if (g_wizard_step == 7) {
    lv_obj_t *label = lv_label_create(g_wizard_refs.content);
    lv_label_set_text(label, "Min time between (min)");
    create_number_selector(g_wizard_refs.content, "Min gap", &g_edit_slot.min_gap_min, 10, 360, 5, "%d");
  } else if (g_wizard_step == 8) {
    lv_obj_t *label = lv_label_create(g_wizard_refs.content);
    lv_label_set_text(label, "Save changes?");
    lv_obj_t *row = lv_obj_create(g_wizard_refs.content);
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_height(row, 40);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_set_style_pad_gap(row, 6, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    const char *labels[] = {"Save", "Discard", "Cancel"};
    for (int i = 0; i < 3; ++i) {
      lv_obj_t *btn = lv_btn_create(row);
      lv_obj_set_flex_grow(btn, 1);
      lv_obj_add_event_cb(btn, wizard_save_event, LV_EVENT_CLICKED,
                          reinterpret_cast<void *>(static_cast<intptr_t>(i)));
      lv_obj_t *lab = lv_label_create(btn);
      lv_label_set_text(lab, labels[i]);
      lv_obj_center(lab);
    }

    lv_obj_add_flag(g_wizard_refs.next_btn, LV_OBJ_FLAG_HIDDEN);
    return;
  }

  lv_obj_clear_flag(g_wizard_refs.next_btn, LV_OBJ_FLAG_HIDDEN);
  if (g_wizard_step == 8) {
    lv_label_set_text(g_wizard_refs.next_label, "Done");
  } else {
    lv_label_set_text(g_wizard_refs.next_label, "Next");
  }
}

static void wizard_refresh_cb(void *) {
  if (g_active_screen == SCREEN_SLOT_WIZARD) {
    wizard_render_step();
  }
}

static void time_date_render_step() {
  if (!g_time_date_refs.content) return;
  reset_binding_pool();
  lv_obj_clean(g_time_date_refs.content);

  if (g_time_date_step == 0) {
    lv_label_set_text(g_time_date_refs.step_label, "Set time");
    create_time_input_block(g_time_date_refs.content, "Time", &g_time_date_edit.hour, &g_time_date_edit.minute);
    lv_obj_clear_flag(g_time_date_refs.next_btn, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(g_time_date_refs.next_label, "Next");
  } else if (g_time_date_step == 1) {
    lv_label_set_text(g_time_date_refs.step_label, "Set date");
    create_date_input_block(g_time_date_refs.content, "Date", &g_time_date_edit.month, &g_time_date_edit.day, &g_time_date_edit.year);
    lv_obj_clear_flag(g_time_date_refs.next_btn, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(g_time_date_refs.next_label, "Next");
  } else {
    lv_label_set_text(g_time_date_refs.step_label, "Save?");
    char dt_buf[20] = {0};
    format_datetime(g_time_date_edit, dt_buf, sizeof(dt_buf));
    lv_obj_t *summary = lv_label_create(g_time_date_refs.content);
    lv_label_set_text_fmt(summary, "New: %s", dt_buf);
    lv_obj_set_style_text_color(summary, kColorMuted, 0);
    lv_obj_t *row = lv_obj_create(g_time_date_refs.content);
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_height(row, 40);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_set_style_pad_gap(row, 6, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *save_btn = lv_btn_create(row);
    lv_obj_set_flex_grow(save_btn, 1);
    lv_obj_add_event_cb(save_btn, time_date_save_event, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *save_label = lv_label_create(save_btn);
    lv_label_set_text(save_label, "Save");
    lv_obj_center(save_label);

    lv_obj_t *cancel_btn = lv_btn_create(row);
    lv_obj_set_flex_grow(cancel_btn, 1);
    lv_obj_add_event_cb(cancel_btn, time_date_cancel_event, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *cancel_label = lv_label_create(cancel_btn);
    lv_label_set_text(cancel_label, "Cancel");
    lv_obj_center(cancel_label);

    lv_obj_add_flag(g_time_date_refs.next_btn, LV_OBJ_FLAG_HIDDEN);
  }
}

static void time_date_back_event(lv_event_t *) {
  if (g_time_date_step == 0) {
    pop_screen();
    return;
  }
  g_time_date_step = clamp_int(g_time_date_step - 1, 0, 2);
  time_date_render_step();
}

static void time_date_next_event(lv_event_t *event) {
  (void)event;
  if (g_time_date_step >= 2) return;
  g_time_date_step++;
  time_date_render_step();
}

static void time_date_save_event(lv_event_t *) {
  g_sim.now = g_time_date_edit;
  g_setup.time_date = true;
  maybe_refresh_initial_setup();
  pop_screen();
}

static void time_date_cancel_event(lv_event_t *) {
  pop_screen();
}

static void open_cal_menu_event(lv_event_t *) {
  push_screen(SCREEN_CAL_MENU);
}

static void open_test_menu_event(lv_event_t *) {
  push_screen(SCREEN_TEST_MENU);
}

static void open_reset_menu_event(lv_event_t *) {
  push_screen(SCREEN_RESET_MENU);
}

static void open_cal_moist_event(lv_event_t *) {
  g_cal_moist_mode = 0;
  push_screen(SCREEN_CAL_MOIST);
}

static void open_cal_flow_event(lv_event_t *) {
  g_cal_flow_step = 0;
  g_cal_flow_running = false;
  push_screen(SCREEN_CAL_FLOW);
}

static void open_test_sensors_event(lv_event_t *) {
  push_screen(SCREEN_TEST_SENSORS);
}

static void open_test_pumps_event(lv_event_t *) {
  push_screen(SCREEN_TEST_PUMPS);
}

static void reset_logs_handler(int option, int) {
  if (option == 0) {
    g_log_count = 0;
    add_log(build_boot_log());
  }
}

static void reset_logs_event(lv_event_t *) {
  const char *options[] = {"Reset", "Cancel"};
  show_prompt("Reset logs", "Clear all logs?", options, 2, reset_logs_handler, 0);
}

static void reset_factory_handler(int option, int) {
  if (option == 0) {
    sim_init();
  }
}

static void reset_factory_event(lv_event_t *) {
  const char *options[] = {"Reset", "Cancel"};
  show_prompt("Reset data", "Factory reset?", options, 2, reset_factory_handler, 0);
}

static void open_initial_setup_event(lv_event_t *) {
  push_screen(SCREEN_INITIAL_SETUP);
}

static void number_input_ok_event(lv_event_t *) {
  if (g_number_ctx.target) {
    *g_number_ctx.target = g_number_ctx.value;
  }
  if (g_number_ctx.on_done) {
    g_number_ctx.on_done();
  }
  maybe_refresh_initial_setup();
  pop_screen();
}

static void time_range_ok_event(lv_event_t *) {
  g_sim.lights_on_hour = g_time_range_ctx.on_hour;
  g_sim.lights_on_min = g_time_range_ctx.on_min;
  g_sim.lights_off_hour = g_time_range_ctx.off_hour;
  g_sim.lights_off_min = g_time_range_ctx.off_min;
  if (g_time_range_ctx.on_done) {
    g_time_range_ctx.on_done();
  }
  maybe_refresh_initial_setup();
  pop_screen();
}

static void cal_moist_mode_event(lv_event_t *event) {
  int mode = static_cast<int>(reinterpret_cast<intptr_t>(lv_event_get_user_data(event)));
  g_cal_moist_mode = mode;
  update_cal_moist_screen();
}

static void cal_moist_save_event(lv_event_t *) {
  if (g_cal_moist_mode == 1) g_sim.moisture_raw_dry = g_sim.moisture_raw;
  if (g_cal_moist_mode == 2) g_sim.moisture_raw_wet = g_sim.moisture_raw;
  g_setup.moisture_cal = true;
  maybe_refresh_initial_setup();
  const char *options[] = {"OK"};
  show_prompt("Saved", "Calibration stored", options, 1, nullptr, 0);
}

static void cal_flow_action_event(lv_event_t *) {
  if (g_cal_flow_step == 2) {
    g_setup.dripper_cal = true;
    maybe_refresh_initial_setup();
    const char *options[] = {"OK"};
    show_prompt("Saved", "Flow calibrated", options, 1, nullptr, 0);
    return;
  }
  g_cal_flow_running = !g_cal_flow_running;
  update_cal_flow_screen();
}

static void cal_flow_next_event(lv_event_t *) {
  if (g_cal_flow_step < 2) {
    g_cal_flow_step++;
    g_cal_flow_running = false;
    if (g_cal_flow_step == 2) {
      g_sim.dripper_ml_per_hour = 1600 + static_cast<int>(g_sim.uptime_sec % 400);
    }
    update_cal_flow_screen();
  }
}

static void build_ui() {
  lv_display_t *disp = lv_display_get_default();
  lv_theme_t *theme = lv_theme_default_init(disp, kColorAccent, kColorMuted, true, &lv_font_montserrat_14);
  lv_display_set_theme(disp, theme);

  push_screen(SCREEN_INFO);

  g_debug_label = lv_label_create(lv_layer_top());
  lv_label_set_text(g_debug_label, "[INFO]");
  lv_obj_set_style_text_font(g_debug_label, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(g_debug_label, kColorMuted, 0);
  lv_obj_align(g_debug_label, LV_ALIGN_TOP_RIGHT, -4, 4);

  g_ui_timer = lv_timer_create(ui_timer_cb, 200, nullptr);
}

void setup() {
  SPI.begin(kTftSclk, kTftMiso, kTftMosi, kTftCs);
  Wire.begin(kTouchSda, kTouchScl);

  pinMode(kTftLed, OUTPUT);
  digitalWrite(kTftLed, HIGH);

  tft.begin();
  tft.setRotation(kTftRotation);
  tft.setSPISpeed(kTftSpiHz);

  tft.fillScreen(ILI9341_NAVY);
  delay(kBootSplashMs);
  tft.fillScreen(ILI9341_BLACK);

  touchReady = ctp.begin(40);

  lv_init();
  lv_tick_set_cb(tick_get_cb);

  lv_display_t *disp = lv_display_create(kScreenWidth, kScreenHeight);
  lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
  lv_display_set_flush_cb(disp, disp_flush);
  lv_display_set_buffers(disp, buf1, nullptr, sizeof(buf1), LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_default(disp);

  lv_indev_t *indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, touch_read);
  lv_indev_set_display(indev, disp);

  randomSeed(micros());
  sim_init();
  build_ui();

  lv_refr_now(disp);
}

void loop() {
  uint32_t delayMs = lv_timer_handler();
  if (delayMs < 5) delayMs = 5;
  else if (delayMs > 20) delayMs = 20;
  delay(delayMs);
}
