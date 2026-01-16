#include "ui_flow.h"

#include "app_utils.h"
#include "logs.h"
#include "sim.h"
#include "ui_components.h"
#include "ui_screens.h"
#include <Arduino.h>
#include <string.h>

/*
 * set_active_screen
 * Updates the active screen id and debug label text.
 * Example:
 *   set_active_screen(SCREEN_MENU);
 */
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

/*
 * push_screen
 * Builds and pushes a new screen onto the stack, then loads it.
 * Example:
 *   push_screen(SCREEN_LOGS);
 */
static void push_screen(ScreenId id) {
  if (g_screen_stack_size >= static_cast<int>(sizeof(g_screen_stack) / sizeof(g_screen_stack[0]))) return;
  prompt_close();
  lv_obj_t *root = build_screen(id);
  g_screen_stack[g_screen_stack_size++] = {id, root};
  lv_scr_load(root);
  set_active_screen(id);
}

/*
 * pop_screen
 * Pops the current screen and returns to the previous one.
 * Example:
 *   pop_screen();
 */
static void pop_screen() {
  if (g_screen_stack_size <= 1) return;
  prompt_close();
  lv_obj_t *old = g_screen_stack[g_screen_stack_size - 1].root;
  g_screen_stack_size--;
  lv_scr_load(g_screen_stack[g_screen_stack_size - 1].root);
  set_active_screen(g_screen_stack[g_screen_stack_size - 1].id);
  lv_obj_del(old);
}

/*
 * ui_timer_cb
 * LVGL timer callback that ticks the sim and refreshes the UI.
 * Example:
 *   lv_timer_create(ui_timer_cb, 200, nullptr);
 */
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

/*
 * back_event
 * Event handler that navigates back to the previous screen.
 * Example:
 *   lv_obj_add_event_cb(btn, back_event, LV_EVENT_CLICKED, nullptr);
 */
void back_event(lv_event_t *) {
  pop_screen();
}

/*
 * open_menu_event
 * Event handler that opens the main menu screen.
 * Example:
 *   lv_obj_add_event_cb(btn, open_menu_event, LV_EVENT_CLICKED, nullptr);
 */
void open_menu_event(lv_event_t *) {
  push_screen(SCREEN_MENU);
}

/*
 * open_logs_event
 * Event handler that opens the logs viewer screen.
 * Example:
 *   lv_obj_add_event_cb(btn, open_logs_event, LV_EVENT_CLICKED, nullptr);
 */
void open_logs_event(lv_event_t *) {
  push_screen(SCREEN_LOGS);
}

/*
 * open_feeding_event
 * Event handler that opens the feeding menu.
 * Example:
 *   lv_obj_add_event_cb(btn, open_feeding_event, LV_EVENT_CLICKED, nullptr);
 */
void open_feeding_event(lv_event_t *) {
  push_screen(SCREEN_FEEDING_MENU);
}

/*
 * open_force_feed_event
 * Event handler that enters force-feed mode and shows slots.
 * Example:
 *   lv_obj_add_event_cb(btn, open_force_feed_event, LV_EVENT_CLICKED, nullptr);
 */
void open_force_feed_event(lv_event_t *) {
  g_force_feed_mode = true;
  push_screen(SCREEN_SLOTS_LIST);
}

/*
 * pause_prompt_handler
 * Prompt callback that toggles the paused flag and updates the menu label.
 * Example:
 *   show_prompt("Pause", "Feeding paused", opts, 1, pause_prompt_handler, 0);
 */
static void pause_prompt_handler(int option, int) {
  if (option == 0) {
    g_sim.paused = !g_sim.paused;
    if (g_pause_menu_label) {
      lv_label_set_text(g_pause_menu_label, g_sim.paused ? "Unpause feeding" : "Pause feeding");
    }
  }
}

/*
 * toggle_pause_event
 * Event handler that shows a pause/unpause confirmation prompt.
 * Example:
 *   lv_obj_add_event_cb(btn, toggle_pause_event, LV_EVENT_CLICKED, nullptr);
 */
void toggle_pause_event(lv_event_t *) {
  const char *options[] = {"OK"};
  const char *title = g_sim.paused ? "Unpause feeding" : "Pause feeding";
  const char *message = g_sim.paused ? "Feeding resumed" : "Feeding paused";
  show_prompt(title, message, options, 1, pause_prompt_handler, 0);
}

/*
 * open_settings_menu_event
 * Event handler that opens the settings menu screen.
 * Example:
 *   lv_obj_add_event_cb(btn, open_settings_menu_event, LV_EVENT_CLICKED, nullptr);
 */
void open_settings_menu_event(lv_event_t *) {
  push_screen(SCREEN_SETTINGS_MENU);
}

/*
 * open_time_date_event
 * Event handler that opens the time/date wizard and seeds edit values.
 * Example:
 *   lv_obj_add_event_cb(btn, open_time_date_event, LV_EVENT_CLICKED, nullptr);
 */
void open_time_date_event(lv_event_t *) {
  g_time_date_edit = g_sim.now;
  g_time_date_step = 0;
  push_screen(SCREEN_TIME_DATE);
}

/*
 * open_feeding_schedule_event
 * Event handler that opens the feeding schedule slots list.
 * Example:
 *   lv_obj_add_event_cb(btn, open_feeding_schedule_event, LV_EVENT_CLICKED, nullptr);
 */
void open_feeding_schedule_event(lv_event_t *) {
  g_force_feed_mode = false;
  push_screen(SCREEN_SLOTS_LIST);
}

/*
 * open_max_daily_event
 * Event handler that opens the max daily water number input.
 * Example:
 *   lv_obj_add_event_cb(btn, open_max_daily_event, LV_EVENT_CLICKED, nullptr);
 */
void open_max_daily_event(lv_event_t *) {
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

/*
 * open_baseline_x_event
 * Event handler that opens the baseline X number input.
 * Example:
 *   lv_obj_add_event_cb(btn, open_baseline_x_event, LV_EVENT_CLICKED, nullptr);
 */
void open_baseline_x_event(lv_event_t *) {
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

/*
 * open_baseline_y_event
 * Event handler that opens the baseline Y number input.
 * Example:
 *   lv_obj_add_event_cb(btn, open_baseline_y_event, LV_EVENT_CLICKED, nullptr);
 */
void open_baseline_y_event(lv_event_t *) {
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

/*
 * open_baseline_delay_event
 * Event handler that opens the baseline delay number input.
 * Example:
 *   lv_obj_add_event_cb(btn, open_baseline_delay_event, LV_EVENT_CLICKED, nullptr);
 */
void open_baseline_delay_event(lv_event_t *) {
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

/*
 * open_lights_event
 * Event handler that opens the lights on/off time range input.
 * Example:
 *   lv_obj_add_event_cb(btn, open_lights_event, LV_EVENT_CLICKED, nullptr);
 */
void open_lights_event(lv_event_t *) {
  g_time_range_ctx.title = "Lights on/off";
  g_time_range_ctx.on_hour = g_sim.lights_on_hour;
  g_time_range_ctx.on_min = g_sim.lights_on_min;
  g_time_range_ctx.off_hour = g_sim.lights_off_hour;
  g_time_range_ctx.off_min = g_sim.lights_off_min;
  g_time_range_ctx.on_done = []() { g_setup.lights = true; };
  push_screen(SCREEN_TIME_RANGE_INPUT);
}

/*
 * slot_select_event
 * Event handler that selects a slot or starts force-feed prompt.
 * Example:
 *   lv_obj_add_event_cb(btn, slot_select_event, LV_EVENT_CLICKED,
 *                       reinterpret_cast<void *>(static_cast<intptr_t>(i)));
 */
void slot_select_event(lv_event_t *event) {
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

/*
 * edit_slot_event
 * Event handler that opens the slot edit wizard.
 * Example:
 *   lv_obj_add_event_cb(btn, edit_slot_event, LV_EVENT_CLICKED, nullptr);
 */
void edit_slot_event(lv_event_t *) {
  g_edit_slot = g_slots[g_selected_slot];
  g_wizard_step = 0;
  push_screen(SCREEN_SLOT_WIZARD);
}

/*
 * feed_now_event
 * Event handler that triggers an immediate feed and shows a prompt.
 * Example:
 *   lv_obj_add_event_cb(btn, feed_now_event, LV_EVENT_CLICKED, nullptr);
 */
void feed_now_event(lv_event_t *) {
  sim_start_feed(g_selected_slot);
  const char *options[] = {"OK"};
  show_prompt("Feed", "Feeding started", options, 1, nullptr, 0);
}

/*
 * logs_prev_event
 * Event handler that navigates to the previous log entry.
 * Example:
 *   lv_obj_add_event_cb(btn, logs_prev_event, LV_EVENT_CLICKED, nullptr);
 */
void logs_prev_event(lv_event_t *) {
  if (g_log_index + 1 < g_log_count) g_log_index++;
  update_logs_screen();
}

/*
 * logs_next_event
 * Event handler that navigates to the next log entry.
 * Example:
 *   lv_obj_add_event_cb(btn, logs_next_event, LV_EVENT_CLICKED, nullptr);
 */
void logs_next_event(lv_event_t *) {
  if (g_log_index > 0) g_log_index--;
  update_logs_screen();
}

/*
 * wizard_apply_name
 * Copies the current text area value into the editable slot name.
 * Example:
 *   wizard_apply_name();
 */
static void wizard_apply_name() {
  if (!g_wizard_refs.text_area) return;
  const char *text = lv_textarea_get_text(g_wizard_refs.text_area);
  strncpy(g_edit_slot.name, text, sizeof(g_edit_slot.name) - 1);
  g_edit_slot.name[sizeof(g_edit_slot.name) - 1] = '\0';
}

/*
 * wizard_back_event
 * Event handler that moves the slot wizard to the previous step.
 * Example:
 *   lv_obj_add_event_cb(btn, wizard_back_event, LV_EVENT_CLICKED, nullptr);
 */
void wizard_back_event(lv_event_t *) {
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

/*
 * wizard_next_event
 * Event handler that advances the slot wizard to the next step.
 * Example:
 *   lv_obj_add_event_cb(btn, wizard_next_event, LV_EVENT_CLICKED, nullptr);
 */
void wizard_next_event(lv_event_t *) {
  if (g_wizard_step == 1) {
    wizard_apply_name();
  }
  if (g_wizard_step < 8) {
    g_wizard_step++;
    wizard_render_step();
  }
}

/*
 * wizard_save_handler
 * Applies the slot save/discard decision from the wizard.
 * Example:
 *   wizard_save_handler(0, 0);
 */
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

/*
 * wizard_save_event
 * Event handler for Save/Discard/Cancel buttons in the wizard.
 * Example:
 *   lv_obj_add_event_cb(btn, wizard_save_event, LV_EVENT_CLICKED,
 *                       reinterpret_cast<void *>(static_cast<intptr_t>(i)));
 */
void wizard_save_event(lv_event_t *event) {
  int option = static_cast<int>(reinterpret_cast<intptr_t>(lv_event_get_user_data(event)));
  if (option == 2) {
    return;
  }
  wizard_save_handler(option, 0);
}

/*
 * time_date_back_event
 * Event handler that moves back in the time/date wizard.
 * Example:
 *   lv_obj_add_event_cb(btn, time_date_back_event, LV_EVENT_CLICKED, nullptr);
 */
void time_date_back_event(lv_event_t *) {
  if (g_time_date_step == 0) {
    pop_screen();
    return;
  }
  g_time_date_step = clamp_int(g_time_date_step - 1, 0, 2);
  time_date_render_step();
}

/*
 * time_date_next_event
 * Event handler that advances the time/date wizard.
 * Example:
 *   lv_obj_add_event_cb(btn, time_date_next_event, LV_EVENT_CLICKED, nullptr);
 */
void time_date_next_event(lv_event_t *event) {
  (void)event;
  if (g_time_date_step >= 2) return;
  g_time_date_step++;
  time_date_render_step();
}

/*
 * time_date_save_event
 * Event handler that saves the edited time/date values.
 * Example:
 *   lv_obj_add_event_cb(btn, time_date_save_event, LV_EVENT_CLICKED, nullptr);
 */
void time_date_save_event(lv_event_t *) {
  g_sim.now = g_time_date_edit;
  g_setup.time_date = true;
  maybe_refresh_initial_setup();
  pop_screen();
}

/*
 * time_date_cancel_event
 * Event handler that cancels the time/date wizard.
 * Example:
 *   lv_obj_add_event_cb(btn, time_date_cancel_event, LV_EVENT_CLICKED, nullptr);
 */
void time_date_cancel_event(lv_event_t *) {
  pop_screen();
}

/*
 * open_cal_menu_event
 * Event handler that opens the calibration menu.
 * Example:
 *   lv_obj_add_event_cb(btn, open_cal_menu_event, LV_EVENT_CLICKED, nullptr);
 */
void open_cal_menu_event(lv_event_t *) {
  push_screen(SCREEN_CAL_MENU);
}

/*
 * open_test_menu_event
 * Event handler that opens the test peripherals menu.
 * Example:
 *   lv_obj_add_event_cb(btn, open_test_menu_event, LV_EVENT_CLICKED, nullptr);
 */
void open_test_menu_event(lv_event_t *) {
  push_screen(SCREEN_TEST_MENU);
}

/*
 * open_reset_menu_event
 * Event handler that opens the reset menu.
 * Example:
 *   lv_obj_add_event_cb(btn, open_reset_menu_event, LV_EVENT_CLICKED, nullptr);
 */
void open_reset_menu_event(lv_event_t *) {
  push_screen(SCREEN_RESET_MENU);
}

/*
 * open_cal_moist_event
 * Event handler that opens the moisture calibration screen.
 * Example:
 *   lv_obj_add_event_cb(btn, open_cal_moist_event, LV_EVENT_CLICKED, nullptr);
 */
void open_cal_moist_event(lv_event_t *) {
  g_cal_moist_mode = 0;
  push_screen(SCREEN_CAL_MOIST);
}

/*
 * open_cal_flow_event
 * Event handler that opens the flow calibration screen.
 * Example:
 *   lv_obj_add_event_cb(btn, open_cal_flow_event, LV_EVENT_CLICKED, nullptr);
 */
void open_cal_flow_event(lv_event_t *) {
  g_cal_flow_step = 0;
  g_cal_flow_running = false;
  push_screen(SCREEN_CAL_FLOW);
}

/*
 * open_test_sensors_event
 * Event handler that opens the sensor test screen.
 * Example:
 *   lv_obj_add_event_cb(btn, open_test_sensors_event, LV_EVENT_CLICKED, nullptr);
 */
void open_test_sensors_event(lv_event_t *) {
  push_screen(SCREEN_TEST_SENSORS);
}

/*
 * open_test_pumps_event
 * Event handler that opens the pump test screen.
 * Example:
 *   lv_obj_add_event_cb(btn, open_test_pumps_event, LV_EVENT_CLICKED, nullptr);
 */
void open_test_pumps_event(lv_event_t *) {
  push_screen(SCREEN_TEST_PUMPS);
}

/*
 * reset_logs_handler
 * Prompt callback that clears logs and inserts a boot log.
 * Example:
 *   show_prompt("Reset logs", "Clear?", opts, 2, reset_logs_handler, 0);
 */
static void reset_logs_handler(int option, int) {
  if (option == 0) {
    g_log_count = 0;
    add_log(build_boot_log());
  }
}

/*
 * reset_logs_event
 * Event handler that shows the reset logs confirmation prompt.
 * Example:
 *   lv_obj_add_event_cb(btn, reset_logs_event, LV_EVENT_CLICKED, nullptr);
 */
void reset_logs_event(lv_event_t *) {
  const char *options[] = {"Reset", "Cancel"};
  show_prompt("Reset logs", "Clear all logs?", options, 2, reset_logs_handler, 0);
}

/*
 * reset_factory_handler
 * Prompt callback that resets the simulation to defaults.
 * Example:
 *   show_prompt("Reset data", "Factory reset?", opts, 2, reset_factory_handler, 0);
 */
static void reset_factory_handler(int option, int) {
  if (option == 0) {
    sim_init();
  }
}

/*
 * reset_factory_event
 * Event handler that shows the factory reset confirmation prompt.
 * Example:
 *   lv_obj_add_event_cb(btn, reset_factory_event, LV_EVENT_CLICKED, nullptr);
 */
void reset_factory_event(lv_event_t *) {
  const char *options[] = {"Reset", "Cancel"};
  show_prompt("Reset data", "Factory reset?", options, 2, reset_factory_handler, 0);
}

/*
 * open_initial_setup_event
 * Event handler that opens the initial setup checklist screen.
 * Example:
 *   lv_obj_add_event_cb(btn, open_initial_setup_event, LV_EVENT_CLICKED, nullptr);
 */
void open_initial_setup_event(lv_event_t *) {
  push_screen(SCREEN_INITIAL_SETUP);
}

/*
 * number_input_ok_event
 * Event handler that commits the number input to its target.
 * Example:
 *   lv_obj_add_event_cb(btn, number_input_ok_event, LV_EVENT_CLICKED, nullptr);
 */
void number_input_ok_event(lv_event_t *) {
  if (g_number_ctx.target) {
    *g_number_ctx.target = g_number_ctx.value;
  }
  if (g_number_ctx.on_done) {
    g_number_ctx.on_done();
  }
  maybe_refresh_initial_setup();
  pop_screen();
}

/*
 * time_range_ok_event
 * Event handler that commits the lights on/off time range.
 * Example:
 *   lv_obj_add_event_cb(btn, time_range_ok_event, LV_EVENT_CLICKED, nullptr);
 */
void time_range_ok_event(lv_event_t *) {
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

/*
 * cal_moist_mode_event
 * Event handler that selects dry/wet calibration mode.
 * Example:
 *   lv_obj_add_event_cb(btn, cal_moist_mode_event, LV_EVENT_CLICKED,
 *                       reinterpret_cast<void *>(static_cast<intptr_t>(1)));
 */
void cal_moist_mode_event(lv_event_t *event) {
  int mode = static_cast<int>(reinterpret_cast<intptr_t>(lv_event_get_user_data(event)));
  g_cal_moist_mode = mode;
  update_cal_moist_screen();
}

/*
 * cal_moist_save_event
 * Event handler that saves the current calibration reading.
 * Example:
 *   lv_obj_add_event_cb(btn, cal_moist_save_event, LV_EVENT_CLICKED, nullptr);
 */
void cal_moist_save_event(lv_event_t *) {
  if (g_cal_moist_mode == 1) g_sim.moisture_raw_dry = g_sim.moisture_raw;
  if (g_cal_moist_mode == 2) g_sim.moisture_raw_wet = g_sim.moisture_raw;
  g_setup.moisture_cal = true;
  maybe_refresh_initial_setup();
  const char *options[] = {"OK"};
  show_prompt("Saved", "Calibration stored", options, 1, nullptr, 0);
}

/*
 * cal_flow_action_event
 * Event handler that starts/stops flow calibration or saves results.
 * Example:
 *   lv_obj_add_event_cb(btn, cal_flow_action_event, LV_EVENT_CLICKED, nullptr);
 */
void cal_flow_action_event(lv_event_t *) {
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

/*
 * cal_flow_next_event
 * Event handler that advances the flow calibration step.
 * Example:
 *   lv_obj_add_event_cb(btn, cal_flow_next_event, LV_EVENT_CLICKED, nullptr);
 */
void cal_flow_next_event(lv_event_t *) {
  if (g_cal_flow_step < 2) {
    g_cal_flow_step++;
    g_cal_flow_running = false;
    if (g_cal_flow_step == 2) {
      g_sim.dripper_ml_per_hour = 1600 + static_cast<int>(g_sim.uptime_sec % 400);
    }
    update_cal_flow_screen();
  }
}

/*
 * build_ui
 * Applies theme, pushes the initial screen, and starts the UI timer.
 * Example:
 *   build_ui();
 */
void build_ui() {
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
