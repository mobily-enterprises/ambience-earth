#include "app_state.h"

const lv_color_t kColorBg = lv_color_hex(0x101418);
const lv_color_t kColorHeader = lv_color_hex(0x1f2933);
const lv_color_t kColorMuted = lv_color_hex(0x94a3b8);
const lv_color_t kColorAccent = lv_color_hex(0x38bdf8);

SimState g_sim = {};
SetupFlags g_setup = {};
Slot g_slots[kSlotCount] = {};
Slot g_edit_slot = {};
LogEntry g_logs[kMaxLogs] = {};
int g_log_count = 0;
int g_log_index = 0;
int g_selected_slot = 0;
bool g_force_feed_mode = false;

ScreenNode g_screen_stack[kScreenStackMax] = {};
int g_screen_stack_size = 0;
ScreenId g_active_screen = SCREEN_INFO;

InfoRefs g_info_refs = {};
LogsRefs g_logs_refs = {};
SlotSummaryRefs g_slot_summary_refs = {};
WizardRefs g_wizard_refs = {};
TimeDateRefs g_time_date_refs = {};
CalMoistRefs g_cal_moist_refs = {};
CalFlowRefs g_cal_flow_refs = {};
TestSensorsRefs g_test_sensors_refs = {};
PumpTestRefs g_pump_test_refs = {};
NumberInputContext g_number_ctx = {};
TimeRangeContext g_time_range_ctx = {};
PromptContext g_prompt = {};

NumberBinding g_binding_pool[kBindingPoolSize] = {};
int g_binding_count = 0;
OptionGroup g_option_groups[kOptionGroupSize] = {};
int g_option_group_count = 0;
OptionButtonData g_option_button_data[kOptionButtonSize] = {};
int g_option_button_count = 0;

int g_wizard_step = 0;
int g_time_date_step = 0;
DateTime g_time_date_edit = {};
int g_cal_flow_step = 0;
bool g_cal_flow_running = false;
int g_cal_moist_mode = 0;
int g_pump_test_cycle = 0;
uint32_t g_pump_test_last_ms = 0;

lv_obj_t *g_pause_menu_label = nullptr;
lv_obj_t *g_initial_setup_list = nullptr;

lv_timer_t *g_ui_timer = nullptr;
lv_obj_t *g_debug_label = nullptr;

uint32_t g_last_touch_ms = 0;
bool g_screensaver_active = false;
uint32_t g_last_screensaver_move_ms = 0;

/*
 * reset_binding_pool
 * Clears the number binding pool before rebuilding a dynamic screen.
 * Example:
 *   reset_binding_pool();
 */
void reset_binding_pool() {
  g_binding_count = 0;
}

/*
 * reset_option_pool
 * Clears option group/button pools before rebuilding option rows.
 * Example:
 *   reset_option_pool();
 */
void reset_option_pool() {
  g_option_group_count = 0;
  g_option_button_count = 0;
}

/*
 * alloc_binding
 * Allocates a NumberBinding from the pool for number selector widgets.
 * Example:
 *   NumberBinding *b = alloc_binding();
 */
NumberBinding *alloc_binding() {
  if (g_binding_count >= kBindingPoolSize) return nullptr;
  return &g_binding_pool[g_binding_count++];
}

/*
 * alloc_option_group
 * Allocates an OptionGroup used to manage a set of checkable buttons.
 * Example:
 *   OptionGroup *group = alloc_option_group();
 */
OptionGroup *alloc_option_group() {
  if (g_option_group_count >= kOptionGroupSize) return nullptr;
  OptionGroup *group = &g_option_groups[g_option_group_count++];
  group->count = 0;
  group->target = nullptr;
  group->target_type = OPTION_TARGET_INT;
  return group;
}

/*
 * alloc_option_button
 * Allocates an OptionButtonData entry for a checkable option button.
 * Example:
 *   OptionButtonData *data = alloc_option_button();
 */
OptionButtonData *alloc_option_button() {
  if (g_option_button_count >= kOptionButtonSize) return nullptr;
  return &g_option_button_data[g_option_button_count++];
}
