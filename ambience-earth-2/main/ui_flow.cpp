#include "ui_flow.h"

#include "app_utils.h"
#include "config.h"
#include "feedSlots.h"
#include "feeding.h"
#include "feedingUtils.h"
#include "logs.h"
#include "moistureSensor.h"
#include "pumps.h"
#include "rtc.h"
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
    case SCREEN_FEEDING_STATUS: name = "FEED"; break;
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
 * replace_top_screen
 * Replaces the current top screen without changing the stack depth.
 * Example:
 *   replace_top_screen(SCREEN_INFO);
 */
static void replace_top_screen(ScreenId id) {
  if (g_screen_stack_size <= 0) return;
  if (g_screen_stack[g_screen_stack_size - 1].id == id) return;
  prompt_close();
  lv_obj_t *old = g_screen_stack[g_screen_stack_size - 1].root;
  lv_obj_t *root = build_screen(id);
  g_screen_stack[g_screen_stack_size - 1] = {id, root};
  lv_scr_load(root);
  set_active_screen(id);
  lv_obj_del(old);
}

/*
 * sync_feeding_screen
 * Swaps between info and feeding screens based on runtime state.
 * Example:
 *   sync_feeding_screen();
 */
static void sync_feeding_screen() {
  bool feeding = feedingIsActive();
  if (feeding) {
    if (g_active_screen == SCREEN_INFO) {
      replace_top_screen(SCREEN_FEEDING_STATUS);
    }
  } else {
    if (g_active_screen == SCREEN_FEEDING_STATUS) {
      replace_top_screen(SCREEN_INFO);
    }
  }
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
  if (id != SCREEN_INFO) {
    feedingPauseForUi();
  }
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
  ScreenId popped_id = g_screen_stack[g_screen_stack_size - 1].id;
  lv_obj_t *old = g_screen_stack[g_screen_stack_size - 1].root;
  g_screen_stack_size--;
  lv_scr_load(g_screen_stack[g_screen_stack_size - 1].root);
  set_active_screen(g_screen_stack[g_screen_stack_size - 1].id);
  // If feeding was stopped by entering the menu, avoid a one-frame flash of the
  // feeding screen while the timer-based sync catches up.
  if (g_active_screen == SCREEN_FEEDING_STATUS && !feedingIsActive()) {
    replace_top_screen(SCREEN_INFO);
  }
  if (g_screen_stack[g_screen_stack_size - 1].id == SCREEN_INFO) {
    feedingResumeAfterUi();
  }
  if (popped_id == SCREEN_TEST_SENSORS || popped_id == SCREEN_CAL_MOIST) {
    setSoilSensorLazy();
  }
  if (popped_id == SCREEN_CAL_FLOW || popped_id == SCREEN_TEST_PUMPS) {
    g_cal_flow_running = false;
    closeLineIn();
  }
  lv_obj_del(old);
}

/*
 * pop_to_root
 * Pops screens until the root screen is active.
 * Example:
 *   pop_to_root();
 */
static void pop_to_root() {
  while (g_screen_stack_size > 1) {
    pop_screen();
  }
}

/*
 * ui_timer_cb
 * LVGL timer callback that ticks the sim and refreshes the UI.
 * Example:
 *   lv_timer_create(ui_timer_cb, 200, nullptr);
 */
static void ui_timer_cb(lv_timer_t *) {
  uint32_t now_ms = millis();
  sim_tick();
  update_screensaver(now_ms);
  sync_feeding_screen();
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
  feedingPauseForUi();
  feedingTick();
  push_screen(SCREEN_MENU);
}

/*
 * open_logs_event
 * Event handler that opens the logs viewer screen.
 * Example:
 *   lv_obj_add_event_cb(btn, open_logs_event, LV_EVENT_CLICKED, nullptr);
 */
void open_logs_event(lv_event_t *) {
  feedingClearRunoffWarning();
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
    bool enabled = feedingIsEnabled();
    feedingSetEnabled(!enabled);
    if (g_pause_menu_label) {
      lv_label_set_text(g_pause_menu_label, enabled ? "Unpause feeding" : "Pause feeding");
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
  bool enabled = feedingIsEnabled();
  const char *title = enabled ? "Pause feeding" : "Unpause feeding";
  const char *message = enabled ? "Feeding paused" : "Feeding resumed";
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
  uint8_t hour = 0;
  uint8_t minute = 0;
  uint8_t day = 1;
  uint8_t month = 1;
  uint8_t year = 25;
  rtcReadDateTime(&hour, &minute, &day, &month, &year);
  g_time_date_edit.year = 2000 + year;
  g_time_date_edit.month = month;
  g_time_date_edit.day = day;
  g_time_date_edit.hour = hour;
  g_time_date_edit.minute = minute;
  g_time_date_edit.second = 0;
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
  g_number_ctx.value = config.maxDailyWaterMl;
  g_number_ctx.min = 100;
  g_number_ctx.max = 5000;
  g_number_ctx.step = 100;
  g_number_ctx.unit = "ml";
  g_number_ctx.target = nullptr;
  g_number_ctx.on_done = []() {
    config.maxDailyWaterMl = static_cast<uint16_t>(g_number_ctx.value);
    config.flags |= CONFIG_FLAG_MAX_DAILY_SET;
    saveConfig();
    g_setup.max_daily = true;
  };
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
  g_number_ctx.value = clampBaselineOffset(config.baselineX);
  g_number_ctx.min = 2;
  g_number_ctx.max = 20;
  g_number_ctx.step = 2;
  g_number_ctx.unit = "%";
  g_number_ctx.target = nullptr;
  g_number_ctx.on_done = []() {
    config.baselineX = clampBaselineOffset(static_cast<uint8_t>(g_number_ctx.value));
    saveConfig();
  };
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
  g_number_ctx.value = clampBaselineOffset(config.baselineY);
  g_number_ctx.min = 2;
  g_number_ctx.max = 20;
  g_number_ctx.step = 2;
  g_number_ctx.unit = "%";
  g_number_ctx.target = nullptr;
  g_number_ctx.on_done = []() {
    config.baselineY = clampBaselineOffset(static_cast<uint8_t>(g_number_ctx.value));
    saveConfig();
  };
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
  g_number_ctx.value = config.baselineDelayMinutes ? config.baselineDelayMinutes : 5;
  g_number_ctx.min = 5;
  g_number_ctx.max = 120;
  g_number_ctx.step = 5;
  g_number_ctx.unit = "min";
  g_number_ctx.target = nullptr;
  g_number_ctx.on_done = []() {
    config.baselineDelayMinutes = static_cast<uint8_t>(g_number_ctx.value);
    saveConfig();
  };
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
  g_time_range_ctx.on_hour = static_cast<int>(config.lightsOnMinutes / 60);
  g_time_range_ctx.on_min = static_cast<int>(config.lightsOnMinutes % 60);
  g_time_range_ctx.off_hour = static_cast<int>(config.lightsOffMinutes / 60);
  g_time_range_ctx.off_min = static_cast<int>(config.lightsOffMinutes % 60);
  g_time_range_ctx.on_done = []() {
    config.flags |= CONFIG_FLAG_LIGHTS_ON_SET;
    config.flags |= CONFIG_FLAG_LIGHTS_OFF_SET;
    saveConfig();
    g_setup.lights = true;
  };
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
                    feedingResumeAfterUi();
                    feedingForceFeed(static_cast<uint8_t>(context));
                    g_force_feed_mode = false;
                    pop_to_root();
                    // Force-feed starts while UI is still on another screen; swap immediately
                    // to the feeding screen so we don't flash the info screen before the timer
                    // tick runs and performs the auto swap.
                    if (feedingIsActive()) {
                      replace_top_screen(SCREEN_FEEDING_STATUS);
                    }
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
  g_wizard_step = 1;
  push_screen(SCREEN_SLOT_WIZARD);
}

/*
 * feed_now_event
 * Event handler that triggers an immediate feed and shows a prompt.
 * Example:
 *   lv_obj_add_event_cb(btn, feed_now_event, LV_EVENT_CLICKED, nullptr);
 */
void feed_now_event(lv_event_t *) {
  feedingResumeAfterUi();
  feedingForceFeed(static_cast<uint8_t>(g_selected_slot));
  pop_to_root();
  // Same immediate swap as force-feed prompt to avoid a brief info-screen flash.
  if (feedingIsActive()) {
    replace_top_screen(SCREEN_FEEDING_STATUS);
  }
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
 * save_slot_to_config
 * Persists the edited slot to config storage using feed-slot packing.
 * Example:
 *   save_slot_to_config(g_selected_slot, g_edit_slot);
 */
static void save_slot_to_config(int slot_index, const Slot &slot) {
  if (slot_index < 0 || slot_index >= kSlotCount) return;
  FeedSlot existing = {};
  unpackFeedSlot(&existing, config.feedSlotsPacked[slot_index]);
  uint8_t runoff_hold = existing.runoffHold5s ? existing.runoffHold5s : 6;

  FeedSlot packed = {};
  packed.runoffHold5s = runoff_hold;
  packed.flags = 0;

  if (slot.enabled) packed.flags |= FEED_SLOT_ENABLED;
  if (slot.use_window) packed.flags |= FEED_SLOT_HAS_TIME_WINDOW;
  if (slot.start_mode != MODE_OFF) packed.flags |= FEED_SLOT_HAS_MOISTURE_BELOW;
  if (slot.target_mode != MODE_OFF) packed.flags |= FEED_SLOT_HAS_MOISTURE_TARGET;
  if (slot.min_gap_min > 0) packed.flags |= FEED_SLOT_HAS_MIN_SINCE;
  if (slot.stop_on_runoff) packed.flags |= FEED_SLOT_RUNOFF_REQUIRED;
  if (slot.runoff_mode == RUNOFF_AVOID) packed.flags |= FEED_SLOT_RUNOFF_AVOID;
  if (slot.stop_on_runoff && slot.runoff_mode == RUNOFF_MUST && slot.baseline_setter) {
    packed.flags |= FEED_SLOT_BASELINE_SETTER;
  }

  if (slot.use_window) {
    int start_minutes = clamp_int(slot.start_hour, 0, 23) * 60 +
                        clamp_int(slot.start_min, 0, 59);
    packed.windowStartMinutes = static_cast<uint16_t>(clamp_int(start_minutes, 0, 1439));
    packed.windowDurationMinutes = static_cast<uint16_t>(clamp_int(slot.window_duration_min, 0, 1439));
  } else {
    packed.windowStartMinutes = 0;
    packed.windowDurationMinutes = 0;
  }

  const uint8_t kBaselineSentinel = 127;
  if (slot.start_mode == MODE_BASELINE) {
    packed.moistureBelow = kBaselineSentinel;
  } else if (slot.start_mode == MODE_PERCENT) {
    packed.moistureBelow = static_cast<uint8_t>(clamp_int(slot.start_value, 0, 100));
  } else {
    packed.moistureBelow = 0;
  }

  if (slot.target_mode == MODE_BASELINE) {
    packed.moistureTarget = kBaselineSentinel;
  } else if (slot.target_mode == MODE_PERCENT) {
    packed.moistureTarget = static_cast<uint8_t>(clamp_int(slot.target_value, 0, 100));
  } else {
    packed.moistureTarget = 0;
  }

  packed.minGapMinutes = static_cast<uint16_t>(clamp_int(slot.min_gap_min, 0, 4095));
  packed.maxVolumeMl = static_cast<uint16_t>(clamp_int(slot.max_ml, 0, 1500));

  packFeedSlot(config.feedSlotsPacked[slot_index], &packed);

  strncpy(config.feedSlotNames[slot_index], slot.name, FEED_SLOT_NAME_LENGTH);
  config.feedSlotNames[slot_index][FEED_SLOT_NAME_LENGTH] = '\0';

  uint8_t pref = 0;
  if (slot.runoff_mode == RUNOFF_MUST) pref = 1;
  else if (slot.runoff_mode == RUNOFF_AVOID) pref = 2;
  config.runoffExpectation[slot_index] = pref;

  saveConfig();
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
  g_wizard_step = clamp_int(g_wizard_step - 1, 0, 4);
  wizard_render_step();
}

/*
 * wizard_name_done_event
 * Event handler that commits the slot name and advances the wizard.
 * Example:
 *   lv_obj_add_event_cb(btn, wizard_name_done_event, LV_EVENT_CLICKED, nullptr);
 */
void wizard_name_done_event(lv_event_t *) {
  if (g_wizard_step != 0) return;
  wizard_apply_name();
}

/*
 * wizard_name_cancel_event
 * Event handler that abandons name edits and returns to the previous step.
 * Example:
 *   lv_obj_add_event_cb(btn, wizard_name_cancel_event, LV_EVENT_CLICKED, nullptr);
 */
void wizard_name_cancel_event(lv_event_t *) {
  if (g_wizard_step != 0) return;
  wizard_apply_name();
}

/*
 * wizard_next_event
 * Event handler that advances the slot wizard to the next step.
 * Example:
 *   lv_obj_add_event_cb(btn, wizard_next_event, LV_EVENT_CLICKED, nullptr);
 */
void wizard_next_event(lv_event_t *) {
  if (g_wizard_step == 0) {
    wizard_apply_name();
  }
  if (g_wizard_step < 4) {
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
    save_slot_to_config(g_selected_slot, g_edit_slot);
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
  uint8_t year = 0;
  if (g_time_date_edit.year >= 2000) {
    year = static_cast<uint8_t>(g_time_date_edit.year - 2000);
  } else {
    year = static_cast<uint8_t>(g_time_date_edit.year % 100);
  }
  if (rtcSetDateTime(static_cast<uint8_t>(g_time_date_edit.hour),
                     static_cast<uint8_t>(g_time_date_edit.minute),
                     static_cast<uint8_t>(g_time_date_edit.second),
                     static_cast<uint8_t>(g_time_date_edit.day),
                     static_cast<uint8_t>(g_time_date_edit.month),
                     year)) {
    config.flags |= CONFIG_FLAG_TIME_SET;
    saveConfig();
    g_setup.time_date = true;
    maybe_refresh_initial_setup();
  }
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
  g_cal_moist_avg_raw = 0;
  g_cal_moist_window_done = false;
  g_cal_moist_prompt_shown = false;
  setSoilSensorLazy();
  push_screen(SCREEN_CAL_MOIST);
}

/*
 * cal_moist_back_event
 * Event handler that cancels moisture calibration and returns.
 * Example:
 *   lv_obj_add_event_cb(btn, cal_moist_back_event, LV_EVENT_CLICKED, nullptr);
 */
void cal_moist_back_event(lv_event_t *) {
  setSoilSensorLazy();
  g_cal_moist_mode = 0;
  g_cal_moist_avg_raw = 0;
  g_cal_moist_window_done = false;
  g_cal_moist_prompt_shown = false;
  pop_screen();
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
  g_cal_flow_start_ms = 0;
  g_cal_flow_elapsed_ms = 0;
  g_cal_flow_target_ml_per_hour = static_cast<int>(config.pulseTargetUnits) * 200;
  if (g_cal_flow_target_ml_per_hour <= 0) {
    g_cal_flow_target_ml_per_hour = 4000;
  }
  closeLineIn();
  push_screen(SCREEN_CAL_FLOW);
}

/*
 * open_test_sensors_event
 * Event handler that opens the sensor test screen.
 * Example:
 *   lv_obj_add_event_cb(btn, open_test_sensors_event, LV_EVENT_CLICKED, nullptr);
 */
void open_test_sensors_event(lv_event_t *) {
  setSoilSensorRealTime();
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
    logs_wipe();
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
    sim_factory_reset();
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
  int on_minutes = clamp_int(g_time_range_ctx.on_hour, 0, 23) * 60 +
                   clamp_int(g_time_range_ctx.on_min, 0, 59);
  int off_minutes = clamp_int(g_time_range_ctx.off_hour, 0, 23) * 60 +
                    clamp_int(g_time_range_ctx.off_min, 0, 59);
  config.lightsOnMinutes = static_cast<uint16_t>(on_minutes);
  config.lightsOffMinutes = static_cast<uint16_t>(off_minutes);
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
  if (g_cal_moist_mode != 0 && !g_cal_moist_window_done) return;
  int mode = static_cast<int>(reinterpret_cast<intptr_t>(lv_event_get_user_data(event)));
  g_cal_moist_mode = mode;
  g_cal_moist_avg_raw = 0;
  g_cal_moist_window_done = false;
  g_cal_moist_prompt_shown = false;
  if (g_cal_moist_mode != 0) {
    soilSensorWindowStart();
  }
  update_cal_moist_screen();
}

/*
 * cal_moist_done_prompt
 * Prompt callback that closes the calibration screen after saving.
 * Example:
 *   show_prompt("Save?", "Calibration stored", opts, 2, cal_moist_done_prompt, 0);
 */
void cal_moist_done_prompt(int, int) {
  g_cal_moist_mode = 0;
  g_cal_moist_avg_raw = 0;
  g_cal_moist_window_done = false;
  g_cal_moist_prompt_shown = false;
  pop_screen();
}

/*
 * cal_moist_save_event
 * Event handler that saves the current calibration reading.
 * Example:
 *   lv_obj_add_event_cb(btn, cal_moist_save_event, LV_EVENT_CLICKED, nullptr);
 */
void cal_moist_save_event(lv_event_t *) {
  if (g_cal_moist_mode != 1 && g_cal_moist_mode != 2) {
    const char *options[] = {"OK"};
    show_prompt("Select mode", "Choose Dry or Wet first", options, 1, nullptr, 0);
    return;
  }
  uint16_t value = g_cal_moist_window_done ? g_cal_moist_avg_raw : soilSensorWindowLastRaw();
  if (g_cal_moist_mode == 1) {
    config.moistSensorCalibrationDry = value;
  } else if (g_cal_moist_mode == 2) {
    config.moistSensorCalibrationSoaked = value;
  }
  saveConfig();
  g_setup.moisture_cal = true;
  maybe_refresh_initial_setup();
  setSoilSensorLazy();
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
    g_cal_flow_running = false;
    closeLineIn();
    uint32_t per_liter = config.dripperMsPerLiter;
    if (g_cal_flow_elapsed_ms > 0) {
      per_liter = g_cal_flow_elapsed_ms * 10UL;
      if (per_liter < g_cal_flow_elapsed_ms) per_liter = 0xFFFFFFFFUL;
    }

    int target_ml = g_cal_flow_target_ml_per_hour;
    if (target_ml <= 0) target_ml = static_cast<int>(config.pulseTargetUnits) * 200;
    if (target_ml <= 0) target_ml = 4000;
    uint8_t target_units = static_cast<uint8_t>(target_ml / 200);

    uint8_t on_sec = 0;
    uint8_t off_sec = 0;
    if (target_units >= 5 && target_units <= 50 && per_liter) {
      uint32_t full_rate = 3600000000UL / per_liter;
      uint32_t target_rate = static_cast<uint32_t>(target_units) * 200UL;
      if (full_rate > target_rate) {
        uint32_t diff = full_rate - target_rate;
        uint8_t on = 10;
        uint32_t off_calc = static_cast<uint32_t>(on) * diff / target_rate;
        if (off_calc > 60) {
          on = 5;
          off_calc = static_cast<uint32_t>(on) * diff / target_rate;
        }
        if (off_calc > 255) off_calc = 255;
        on_sec = on;
        off_sec = static_cast<uint8_t>(off_calc);
      }
    }

    config.dripperMsPerLiter = per_liter;
    config.pulseTargetUnits = target_units;
    config.pulseOnSeconds = on_sec;
    config.pulseOffSeconds = off_sec;
    config.flags |= CONFIG_FLAG_DRIPPER_CALIBRATED;
    config.flags |= CONFIG_FLAG_PULSE_SET;
    saveConfig();

    g_setup.dripper_cal = true;
    maybe_refresh_initial_setup();
    const char *options[] = {"OK"};
    show_prompt("Saved", "Flow calibrated", options, 1, nullptr, 0);
    return;
  }
  if (!g_cal_flow_running) {
    g_cal_flow_running = true;
    g_cal_flow_start_ms = millis();
    if (g_cal_flow_step == 1) g_cal_flow_elapsed_ms = 0;
    openLineIn();
  } else {
    g_cal_flow_running = false;
    closeLineIn();
    if (g_cal_flow_step == 1) {
      uint32_t elapsed = millis() - g_cal_flow_start_ms;
      if (elapsed == 0) elapsed = 1;
      g_cal_flow_elapsed_ms = elapsed;
    }
  }
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
    if (g_cal_flow_running) {
      g_cal_flow_running = false;
      closeLineIn();
    }
    g_cal_flow_step++;
    g_cal_flow_running = false;
    g_cal_flow_start_ms = 0;
    if (g_cal_flow_step == 2) {
      if (g_cal_flow_target_ml_per_hour <= 0) {
        g_cal_flow_target_ml_per_hour = static_cast<int>(config.pulseTargetUnits) * 200;
        if (g_cal_flow_target_ml_per_hour <= 0) g_cal_flow_target_ml_per_hour = 4000;
      }
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

  // TODO/FIXME: remove forced wizard step after layout tuning is complete.
  g_selected_slot = 0;
  g_edit_slot = g_slots[g_selected_slot];
  g_wizard_step = 0;
  push_screen(SCREEN_SLOT_WIZARD);

  g_debug_label = nullptr;

#ifdef WOKWI_SIM
  const uint32_t kUiTimerPeriodMs = 1200;
#else
  const uint32_t kUiTimerPeriodMs = 200;
#endif
  g_ui_timer = lv_timer_create(ui_timer_cb, kUiTimerPeriodMs, nullptr);
}
