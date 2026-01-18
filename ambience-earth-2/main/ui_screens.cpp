#include "ui_screens.h"

#include "app_utils.h"
#include "config.h"
#include "feeding.h"
#include "feedingUtils.h"
#include "logs.h"
#include "moistureSensor.h"
#include "pumps.h"
#include "rtc.h"
#include "runoffSensor.h"
#include "ui_components.h"
#include "volume.h"
#include "day_night_icons.h"

static const uint16_t kPlantPixelBg = 0x10A3;
static const uint16_t kPlantPixelLeaf = 0x4D6A;
static const uint16_t kPlantPixelLeafHi = 0x8E09;
static const uint16_t kPlantPixelPot = 0x8B6C;

static const int16_t kPlantW = 16;
static const int16_t kPlantH = 16;
static const uint32_t kPlantScale = 768; // 3x
static const int16_t kPlantDrawW = static_cast<int16_t>(kPlantW * kPlantScale / 256);
static const int16_t kPlantDrawH = static_cast<int16_t>(kPlantH * kPlantScale / 256);
static const uint32_t kDayNightScale = 144; // ~18px for 32px icons
static const int16_t kPlantIconGap = 8;

static const uint16_t kPlantPixels[kPlantW * kPlantH] = {
  kPlantPixelBg, kPlantPixelBg, kPlantPixelBg, kPlantPixelBg, kPlantPixelBg, kPlantPixelBg, kPlantPixelBg, kPlantPixelBg,
  kPlantPixelBg, kPlantPixelBg, kPlantPixelBg, kPlantPixelBg, kPlantPixelBg, kPlantPixelBg, kPlantPixelBg, kPlantPixelBg,
  kPlantPixelBg, kPlantPixelBg, kPlantPixelBg, kPlantPixelBg, kPlantPixelBg, kPlantPixelBg, kPlantPixelLeafHi, kPlantPixelLeafHi,
  kPlantPixelLeafHi, kPlantPixelLeafHi, kPlantPixelBg, kPlantPixelBg, kPlantPixelBg, kPlantPixelBg, kPlantPixelBg, kPlantPixelBg,
  kPlantPixelBg, kPlantPixelBg, kPlantPixelBg, kPlantPixelBg, kPlantPixelLeafHi, kPlantPixelLeafHi, kPlantPixelLeaf, kPlantPixelLeaf,
  kPlantPixelLeaf, kPlantPixelLeaf, kPlantPixelLeafHi, kPlantPixelLeafHi, kPlantPixelBg, kPlantPixelBg, kPlantPixelBg, kPlantPixelBg,
  kPlantPixelBg, kPlantPixelBg, kPlantPixelBg, kPlantPixelLeafHi, kPlantPixelLeaf, kPlantPixelLeaf, kPlantPixelLeaf, kPlantPixelLeaf,
  kPlantPixelLeaf, kPlantPixelLeaf, kPlantPixelLeaf, kPlantPixelLeaf, kPlantPixelLeafHi, kPlantPixelBg, kPlantPixelBg, kPlantPixelBg,
  kPlantPixelBg, kPlantPixelBg, kPlantPixelBg, kPlantPixelBg, kPlantPixelLeaf, kPlantPixelLeaf, kPlantPixelLeaf, kPlantPixelLeaf,
  kPlantPixelLeaf, kPlantPixelLeaf, kPlantPixelLeaf, kPlantPixelLeaf, kPlantPixelBg, kPlantPixelBg, kPlantPixelBg, kPlantPixelBg,
  kPlantPixelBg, kPlantPixelBg, kPlantPixelBg, kPlantPixelBg, kPlantPixelBg, kPlantPixelLeaf, kPlantPixelLeaf, kPlantPixelLeaf,
  kPlantPixelLeaf, kPlantPixelLeaf, kPlantPixelLeaf, kPlantPixelBg, kPlantPixelBg, kPlantPixelBg, kPlantPixelBg, kPlantPixelBg,
  kPlantPixelBg, kPlantPixelBg, kPlantPixelBg, kPlantPixelBg, kPlantPixelBg, kPlantPixelBg, kPlantPixelLeaf, kPlantPixelLeaf,
  kPlantPixelLeaf, kPlantPixelLeaf, kPlantPixelBg, kPlantPixelBg, kPlantPixelBg, kPlantPixelBg, kPlantPixelBg, kPlantPixelBg,
  kPlantPixelBg, kPlantPixelBg, kPlantPixelBg, kPlantPixelBg, kPlantPixelBg, kPlantPixelBg, kPlantPixelBg, kPlantPixelPot,
  kPlantPixelPot, kPlantPixelBg, kPlantPixelBg, kPlantPixelBg, kPlantPixelBg, kPlantPixelBg, kPlantPixelBg, kPlantPixelBg,
  kPlantPixelBg, kPlantPixelBg, kPlantPixelBg, kPlantPixelBg, kPlantPixelBg, kPlantPixelPot, kPlantPixelPot, kPlantPixelPot,
  kPlantPixelPot, kPlantPixelBg, kPlantPixelBg, kPlantPixelBg, kPlantPixelBg, kPlantPixelBg, kPlantPixelBg, kPlantPixelBg,
  kPlantPixelBg, kPlantPixelBg, kPlantPixelBg, kPlantPixelBg, kPlantPixelBg, kPlantPixelPot, kPlantPixelPot, kPlantPixelPot,
  kPlantPixelPot, kPlantPixelBg, kPlantPixelBg, kPlantPixelBg, kPlantPixelBg, kPlantPixelBg, kPlantPixelBg, kPlantPixelBg,
  kPlantPixelBg, kPlantPixelBg, kPlantPixelBg, kPlantPixelBg, kPlantPixelBg, kPlantPixelPot, kPlantPixelPot, kPlantPixelPot,
  kPlantPixelPot, kPlantPixelBg, kPlantPixelBg, kPlantPixelBg, kPlantPixelBg, kPlantPixelBg, kPlantPixelBg, kPlantPixelBg,
  kPlantPixelBg, kPlantPixelBg, kPlantPixelBg, kPlantPixelBg, kPlantPixelPot, kPlantPixelPot, kPlantPixelPot, kPlantPixelPot,
  kPlantPixelPot, kPlantPixelPot, kPlantPixelBg, kPlantPixelBg, kPlantPixelBg, kPlantPixelBg, kPlantPixelBg, kPlantPixelBg,
  kPlantPixelBg, kPlantPixelBg, kPlantPixelBg, kPlantPixelBg, kPlantPixelPot, kPlantPixelPot, kPlantPixelPot, kPlantPixelPot,
  kPlantPixelPot, kPlantPixelPot, kPlantPixelBg, kPlantPixelBg, kPlantPixelBg, kPlantPixelBg, kPlantPixelBg, kPlantPixelBg,
  kPlantPixelBg, kPlantPixelBg, kPlantPixelBg, kPlantPixelBg, kPlantPixelBg, kPlantPixelBg, kPlantPixelPot, kPlantPixelPot,
  kPlantPixelPot, kPlantPixelPot, kPlantPixelBg, kPlantPixelBg, kPlantPixelBg, kPlantPixelBg, kPlantPixelBg, kPlantPixelBg,
  kPlantPixelBg, kPlantPixelBg, kPlantPixelBg, kPlantPixelBg, kPlantPixelBg, kPlantPixelBg, kPlantPixelBg, kPlantPixelBg,
  kPlantPixelBg, kPlantPixelBg, kPlantPixelBg, kPlantPixelBg, kPlantPixelBg, kPlantPixelBg, kPlantPixelBg, kPlantPixelBg,
  kPlantPixelBg, kPlantPixelBg, kPlantPixelBg, kPlantPixelBg, kPlantPixelBg, kPlantPixelBg, kPlantPixelBg, kPlantPixelBg,
  kPlantPixelBg, kPlantPixelBg, kPlantPixelBg, kPlantPixelBg, kPlantPixelBg, kPlantPixelBg, kPlantPixelBg, kPlantPixelBg
};

static const lv_img_dsc_t kPlantImage = {
  .header = {
    .magic = LV_IMAGE_HEADER_MAGIC,
    .cf = LV_COLOR_FORMAT_RGB565,
    .flags = 0,
    .w = kPlantW,
    .h = kPlantH,
    .stride = kPlantW * 2,
    .reserved_2 = 0
  },
  .data_size = sizeof(kPlantPixels),
  .data = reinterpret_cast<const uint8_t *>(kPlantPixels),
  .reserved = nullptr,
  .reserved_2 = nullptr
};
#include <Arduino.h>
#include <stdio.h>
#include <string.h>

void back_event(lv_event_t *);
void open_menu_event(lv_event_t *);
void open_logs_event(lv_event_t *);
void open_feeding_event(lv_event_t *);
void open_force_feed_event(lv_event_t *);
void toggle_pause_event(lv_event_t *);
void open_settings_menu_event(lv_event_t *);
void open_time_date_event(lv_event_t *);
void open_feeding_schedule_event(lv_event_t *);
void open_max_daily_event(lv_event_t *);
void open_baseline_x_event(lv_event_t *);
void open_baseline_y_event(lv_event_t *);
void open_baseline_delay_event(lv_event_t *);
void open_lights_event(lv_event_t *);
void slot_select_event(lv_event_t *);
void edit_slot_event(lv_event_t *);
void feed_now_event(lv_event_t *);
void logs_prev_event(lv_event_t *);
void logs_next_event(lv_event_t *);
void wizard_back_event(lv_event_t *);
void wizard_next_event(lv_event_t *);
void wizard_save_event(lv_event_t *);
void time_date_back_event(lv_event_t *);
void time_date_next_event(lv_event_t *);
void time_date_save_event(lv_event_t *);
void time_date_cancel_event(lv_event_t *);
void open_cal_menu_event(lv_event_t *);
void open_test_menu_event(lv_event_t *);
void open_reset_menu_event(lv_event_t *);
void open_cal_moist_event(lv_event_t *);
void open_cal_flow_event(lv_event_t *);
void open_test_sensors_event(lv_event_t *);
void open_test_pumps_event(lv_event_t *);
void reset_logs_event(lv_event_t *);
void reset_factory_event(lv_event_t *);
void open_initial_setup_event(lv_event_t *);
void number_input_ok_event(lv_event_t *);
void time_range_ok_event(lv_event_t *);
void cal_moist_mode_event(lv_event_t *);
void cal_moist_save_event(lv_event_t *);
void cal_moist_back_event(lv_event_t *);
void cal_moist_done_prompt(int, int);
void cal_flow_action_event(lv_event_t *);
void cal_flow_next_event(lv_event_t *);
void wizard_name_done_event(lv_event_t *);
void wizard_name_cancel_event(lv_event_t *);

/*
 * setup_complete
 * Returns true when all initial setup flags are satisfied.
 * Example:
 *   if (!setup_complete()) { ... }
 */
static bool setup_complete() {
  return g_setup.time_date && g_setup.max_daily && g_setup.lights && g_setup.moisture_cal && g_setup.dripper_cal;
}

/*
 * update_info_screen
 * Refreshes the info screen labels and handles screensaver visibility.
 * Example:
 *   update_info_screen();
 */
static void update_info_screen() {
  if (!g_info_refs.moist_value) return;

  if (g_screensaver_active) {
    lv_obj_add_flag(g_info_refs.main, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(g_info_refs.menu_row, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(g_info_refs.screensaver_root, LV_OBJ_FLAG_HIDDEN);
    if (g_debug_label) lv_obj_add_flag(g_debug_label, LV_OBJ_FLAG_HIDDEN);
    if (feedingRunoffWarning()) {
      lv_label_set_text(g_info_refs.screensaver_icon, "!");
      lv_obj_clear_flag(g_info_refs.screensaver_icon, LV_OBJ_FLAG_HIDDEN);
    } else if (!feedingIsEnabled()) {
      lv_label_set_text(g_info_refs.screensaver_icon, LV_SYMBOL_PAUSE);
      lv_obj_clear_flag(g_info_refs.screensaver_icon, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(g_info_refs.screensaver_icon, LV_OBJ_FLAG_HIDDEN);
    }
    return;
  }

  lv_obj_clear_flag(g_info_refs.main, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(g_info_refs.menu_row, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(g_info_refs.screensaver_root, LV_OBJ_FLAG_HIDDEN);
  if (g_debug_label) lv_obj_clear_flag(g_debug_label, LV_OBJ_FLAG_HIDDEN);

  char time_buf[8] = {0};
  format_time(g_sim.now, time_buf, sizeof(time_buf));

  uint16_t now_minutes = static_cast<uint16_t>(g_sim.now.hour * 60 + g_sim.now.minute);
  uint16_t on_minutes = config.lightsOnMinutes;
  uint16_t off_minutes = config.lightsOffMinutes;
  uint16_t duration = (off_minutes >= on_minutes)
                        ? static_cast<uint16_t>(off_minutes - on_minutes)
                        : static_cast<uint16_t>(1440 - on_minutes + off_minutes);
  bool day_now = (on_minutes != off_minutes) && rtcIsWithinWindow(now_minutes, on_minutes, duration);

  char last_buf[32] = {0};
  if (!millisAtEndOfLastFeed) {
    snprintf(last_buf, sizeof(last_buf), "Never");
  } else {
    unsigned long elapsed_minutes = (millis() - millisAtEndOfLastFeed) / 60000UL;
    unsigned long tenths = (elapsed_minutes + 3) / 6;
    char ago_buf[12] = {0};
    snprintf(ago_buf, sizeof(ago_buf), "%lu.%luh",
             tenths / 10UL, static_cast<unsigned long>(tenths % 10UL));
    snprintf(last_buf, sizeof(last_buf), "%sh ago", ago_buf);
  }
  // DEBUG: preview last-feed display.
  snprintf(last_buf, sizeof(last_buf), "12.0h ago 800ml");

  uint8_t day_lo = LOG_BASELINE_UNSET;
  uint8_t day_hi = LOG_BASELINE_UNSET;
  uint16_t daily_total = getDailyFeedTotalMlNow(&day_lo, &day_hi);
  uint8_t baseline = 0;
  bool has_baseline = feedingGetBaselinePercent(&baseline);

  FeedStatus status = {};
  if (feedingGetStatus(&status)) {
    lv_label_set_text_fmt(g_info_refs.moist_value, "S%d", status.slotIndex + 1);
    lv_label_set_text(g_info_refs.baseline_value, "--");
    lv_label_set_text(g_info_refs.dry_value, "--");
    lv_label_set_text(g_info_refs.minmax_value, "--/--%");

    lv_label_set_text(g_info_refs.time_value, time_buf);
    lv_label_set_text(g_info_refs.last_value, last_buf);
    lv_label_set_text_fmt(g_info_refs.today_value, "%dml", daily_total);
    if (day_now) {
      lv_obj_clear_flag(g_info_refs.day_night_icon, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(g_info_refs.night_icon, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(g_info_refs.day_night_icon, LV_OBJ_FLAG_HIDDEN);
      lv_obj_clear_flag(g_info_refs.night_icon, LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_add_flag(g_info_refs.status_icon, LV_OBJ_FLAG_HIDDEN);
    if (status.moistureReady) {
      lv_label_set_text_fmt(g_info_refs.status_value, "Pump %s | Mst %d%% | %ds",
                            status.pumpOn ? "ON" : "OFF",
                            status.moisturePercent, status.elapsedSeconds);
    } else {
      lv_label_set_text_fmt(g_info_refs.status_value, "Pump %s | Mst --%% | %ds",
                            status.pumpOn ? "ON" : "OFF",
                            status.elapsedSeconds);
    }

    uint16_t delivered = msToVolumeMl(static_cast<uint32_t>(status.elapsedSeconds) * 1000UL,
                                      config.dripperMsPerLiter);
    if (g_info_refs.totals_value) {
      lv_label_set_text_fmt(g_info_refs.totals_value, "Max %dml | W %dml",
                            status.maxVolumeMl, delivered);
    }
    return;
  }

  uint8_t dryback = 0;
  bool has_dryback = getDrybackPercent(&dryback);
  lv_label_set_text_fmt(g_info_refs.moist_value, "%d%%", g_sim.moisture);

  char baseline_buf[8] = {0};
  char dryback_buf[8] = {0};
  char minmax_buf[16] = {0};

  if (has_baseline) {
    snprintf(baseline_buf, sizeof(baseline_buf), "%d%%", baseline);
  } else {
    snprintf(baseline_buf, sizeof(baseline_buf), "--%%");
  }

  if (has_dryback) {
    snprintf(dryback_buf, sizeof(dryback_buf), "%d%%", dryback);
  } else {
    snprintf(dryback_buf, sizeof(dryback_buf), "--%%");
  }

  if (day_lo != LOG_BASELINE_UNSET && day_hi != LOG_BASELINE_UNSET) {
    snprintf(minmax_buf, sizeof(minmax_buf), "%d/%d%%", day_lo, day_hi);
  } else {
    snprintf(minmax_buf, sizeof(minmax_buf), "--/--%%");
  }

  lv_label_set_text(g_info_refs.baseline_value, baseline_buf);
  lv_label_set_text(g_info_refs.dry_value, dryback_buf);
  lv_label_set_text(g_info_refs.minmax_value, minmax_buf);
  lv_label_set_text_fmt(g_info_refs.today_value, "%dml", daily_total);
  lv_label_set_text(g_info_refs.time_value, time_buf);
  lv_label_set_text(g_info_refs.last_value, last_buf);
  if (day_now) {
    lv_obj_clear_flag(g_info_refs.day_night_icon, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(g_info_refs.night_icon, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(g_info_refs.day_night_icon, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(g_info_refs.night_icon, LV_OBJ_FLAG_HIDDEN);
  }
  lv_label_set_text(g_info_refs.status_value, "");
  if (feedingRunoffWarning()) {
    lv_label_set_text(g_info_refs.status_icon, "!");
    lv_obj_clear_flag(g_info_refs.status_icon, LV_OBJ_FLAG_HIDDEN);
  } else if (!feedingIsEnabled()) {
    lv_label_set_text(g_info_refs.status_icon, LV_SYMBOL_PAUSE);
    lv_obj_clear_flag(g_info_refs.status_icon, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(g_info_refs.status_icon, LV_OBJ_FLAG_HIDDEN);
  }
  char line3_buf[64] = {0};
  if (day_lo != LOG_BASELINE_UNSET && day_hi != LOG_BASELINE_UNSET) {
    if (has_baseline) {
      snprintf(line3_buf, sizeof(line3_buf), "Today %dml | BL %d%% | L%d H%d",
               daily_total, baseline, day_lo, day_hi);
    } else {
      snprintf(line3_buf, sizeof(line3_buf), "Today %dml | BL -- | L%d H%d",
               daily_total, day_lo, day_hi);
    }
  } else {
    if (has_baseline) {
      snprintf(line3_buf, sizeof(line3_buf), "Today %dml | BL %d%% | L-- H--",
               daily_total, baseline);
    } else {
      snprintf(line3_buf, sizeof(line3_buf), "Today %dml | BL -- | L-- H--",
               daily_total);
    }
  }
  if (g_info_refs.totals_value) {
    lv_label_set_text(g_info_refs.totals_value, line3_buf);
  }
}

/*
 * update_logs_screen
 * Refreshes the log viewer labels for the current log index.
 * Example:
 *   update_logs_screen();
 */
void update_logs_screen() {
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

  DateTime dt = {};
  dt.month = entry.startMonth;
  dt.day = entry.startDay;
  dt.year = 2000 + entry.startYear;
  dt.hour = entry.startHour;
  dt.minute = entry.startMinute;
  dt.second = 0;

  char dt_buf[16] = {0};
  format_datetime(dt, dt_buf, sizeof(dt_buf));

  if (entry.entryType == 0) {
    lv_label_set_text_fmt(g_logs_refs.header, "# BootUp");
    lv_label_set_text_fmt(g_logs_refs.line1, "At: %s", dt_buf);
    lv_label_set_text_fmt(g_logs_refs.line2, "Soil: %d%%", entry.soilMoistureBefore);
    if (entry.drybackPercent != LOG_BASELINE_UNSET) {
      lv_label_set_text_fmt(g_logs_refs.line3, "Db: %d%%", entry.drybackPercent);
    } else {
      lv_label_set_text(g_logs_refs.line3, "Db: --");
    }
  } else if (entry.entryType == 1) {
    static const char *kStopLabels[] = {"---", "Mst", "Run", "Max", "Off", "Cfg", "Day", "Cal"};
    uint8_t reason = entry.stopReason;
    if (reason >= (sizeof(kStopLabels) / sizeof(kStopLabels[0]))) reason = 0;
    lv_label_set_text_fmt(g_logs_refs.header, "# Feed S%d %s", entry.slotIndex + 1, kStopLabels[reason]);
    lv_label_set_text_fmt(g_logs_refs.line1, "Start: %s", dt_buf);

    uint16_t volume_ml = entry.feedMl;
    if (!volume_ml && entry.millisEnd > entry.millisStart) {
      uint32_t elapsed = entry.millisEnd - entry.millisStart;
      volume_ml = msToVolumeMl(elapsed, config.dripperMsPerLiter);
    }
    lv_label_set_text_fmt(g_logs_refs.line2, "V:%dml Mst:%d-%d%%",
                          volume_ml, entry.soilMoistureBefore, entry.soilMoistureAfter);

    bool warn = (entry.flags & LOG_FLAG_RUNOFF_ANY) != 0;
    char db_buf[8] = {0};
    if (entry.drybackPercent != LOG_BASELINE_UNSET) {
      snprintf(db_buf, sizeof(db_buf), "%d%%", entry.drybackPercent);
    } else {
      snprintf(db_buf, sizeof(db_buf), "--");
    }
    lv_label_set_text_fmt(g_logs_refs.line3, "T:%dml%s Db:%s%s",
                          entry.dailyTotalMl,
                          warn ? "!" : "",
                          db_buf,
                          (entry.flags & LOG_FLAG_RUNOFF_SEEN) ? " R" : "");
  } else {
    lv_label_set_text_fmt(g_logs_refs.header, "# Values");
    lv_label_set_text_fmt(g_logs_refs.line1, "At: %s", dt_buf);
    lv_label_set_text_fmt(g_logs_refs.line2, "Moisture: %d%%", entry.soilMoistureBefore);
    if (entry.drybackPercent != LOG_BASELINE_UNSET) {
      lv_label_set_text_fmt(g_logs_refs.line3, "Db: %d%%", entry.drybackPercent);
    } else {
      lv_label_set_text(g_logs_refs.line3, "Db: --");
    }
  }

  if (g_logs_refs.index_label) {
    lv_label_set_text_fmt(g_logs_refs.index_label, "%d/%d", g_log_index + 1, g_log_count);
  }
}

/*
 * update_cal_moist_screen
 * Updates moisture calibration labels based on current readings.
 * Example:
 *   update_cal_moist_screen();
 */
void update_cal_moist_screen() {
  if (!g_cal_moist_refs.raw_label) return;

  if (g_cal_moist_refs.choice_row) {
    if (g_cal_moist_mode == 0) {
      lv_obj_clear_flag(g_cal_moist_refs.choice_row, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(g_cal_moist_refs.choice_row, LV_OBJ_FLAG_HIDDEN);
    }
  }

  SoilSensorWindowStats stats = {};
  if (g_cal_moist_mode != 0 && !g_cal_moist_window_done && soilSensorWindowTick(&stats)) {
    g_cal_moist_avg_raw = stats.avgRaw;
    g_cal_moist_window_done = true;
    setSoilSensorLazy();
  }

  if (g_cal_moist_mode == 0) {
    lv_label_set_text(g_cal_moist_refs.mode_label, "Select Dry or Wet");
    lv_label_set_text(g_cal_moist_refs.raw_label, "Dry/Wet: --");
    lv_label_set_text(g_cal_moist_refs.percent_label, "Tap a mode to start 15s average");
    return;
  }

  const char *mode_name = (g_cal_moist_mode == 1) ? "Dry" : "Wet";
  lv_label_set_text_fmt(g_cal_moist_refs.mode_label, "Calibrate %s", mode_name);

  uint16_t value = g_cal_moist_window_done && g_cal_moist_avg_raw
                     ? g_cal_moist_avg_raw
                     : soilSensorWindowLastRaw();
  lv_label_set_text_fmt(g_cal_moist_refs.raw_label, "%s: %d", mode_name, value);

  if (!g_cal_moist_window_done) {
    uint32_t remaining_ms = soilSensorCalWindowRemainingMs();
    uint32_t remaining_s = (remaining_ms + 999) / 1000;
    if (remaining_s > 0) {
      lv_label_set_text_fmt(g_cal_moist_refs.percent_label,
                            "Hold sensor %s (%lus left)", mode_name,
                            static_cast<unsigned long>(remaining_s));
    } else {
      lv_label_set_text_fmt(g_cal_moist_refs.percent_label, "Hold sensor %s (15s avg)", mode_name);
    }
    return;
  }

  lv_label_set_text(g_cal_moist_refs.percent_label, "Average captured");

  if (!g_cal_moist_prompt_shown) {
    if (g_cal_moist_mode == 1) {
      config.moistSensorCalibrationDry = g_cal_moist_avg_raw;
    } else if (g_cal_moist_mode == 2) {
      config.moistSensorCalibrationSoaked = g_cal_moist_avg_raw;
    }
    saveConfig();
    g_setup.moisture_cal = true;
    maybe_refresh_initial_setup();
    g_cal_moist_prompt_shown = true;
    const char *options[] = {"OK", "Back"};
    show_prompt("Save?", "Calibration stored", options, 2, cal_moist_done_prompt, 0);
  }
}

/*
 * update_test_sensors_screen
 * Updates sensor test screen readings.
 * Example:
 *   update_test_sensors_screen();
 */
static void update_test_sensors_screen() {
  if (!g_test_sensors_refs.raw_label) return;
  getSoilMoisture();
  uint16_t raw = soilSensorGetRealtimeRaw();
  uint8_t pct = soilMoistureAsPercentage(raw);
  lv_label_set_text_fmt(g_test_sensors_refs.raw_label, "Raw: %d", raw);
  lv_label_set_text_fmt(g_test_sensors_refs.percent_label, "Moisture: %d%%", pct);
  lv_label_set_text_fmt(g_test_sensors_refs.runoff_label, "Runoff: %s", runoffDetected() ? "1" : "0");
}

/*
 * update_cal_flow_screen
 * Updates the flow calibration step and status labels.
 * Example:
 *   update_cal_flow_screen();
 */
void update_cal_flow_screen() {
  if (!g_cal_flow_refs.step_label) return;
  const char *step_title = "";
  if (g_cal_flow_step == 0) step_title = "Prime pump";
  else if (g_cal_flow_step == 1) step_title = "Fill 100ml";
  else step_title = "Compute flow";
  lv_label_set_text_fmt(g_cal_flow_refs.step_label, "Step %d: %s", g_cal_flow_step + 1, step_title);

  if (g_cal_flow_step == 2) {
    uint32_t per_liter = g_cal_flow_elapsed_ms ? (g_cal_flow_elapsed_ms * 10UL) : 0;
    uint32_t ml_per_hour = per_liter ? (3600000000UL / per_liter) : 0;
    if (ml_per_hour) {
      lv_label_set_text_fmt(g_cal_flow_refs.status_label, "Flow: %lu ml/h", static_cast<unsigned long>(ml_per_hour));
    } else {
      lv_label_set_text(g_cal_flow_refs.status_label, "Flow: n/a");
    }
  } else {
    if (g_cal_flow_step == 1 && g_cal_flow_running && g_cal_flow_start_ms) {
      uint32_t elapsed = (millis() - g_cal_flow_start_ms) / 1000UL;
      lv_label_set_text_fmt(g_cal_flow_refs.status_label, "Filling... %lus",
                            static_cast<unsigned long>(elapsed));
    } else {
      lv_label_set_text_fmt(g_cal_flow_refs.status_label, "State: %s", g_cal_flow_running ? "Running" : "Stopped");
    }
  }

  if (g_cal_flow_refs.action_label) {
    if (g_cal_flow_step == 2) {
      lv_label_set_text(g_cal_flow_refs.action_label, "Save");
    } else {
      lv_label_set_text(g_cal_flow_refs.action_label, g_cal_flow_running ? "Stop" : "Start");
    }
  }

  if (g_cal_flow_refs.target_row) {
    if (g_cal_flow_step == 2) {
      lv_obj_clear_flag(g_cal_flow_refs.target_row, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(g_cal_flow_refs.target_row, LV_OBJ_FLAG_HIDDEN);
    }
  }
}

/*
 * update_pump_test_screen
 * Updates pump test status text and timing.
 * Example:
 *   update_pump_test_screen();
 */
static void update_pump_test_screen() {
  if (!g_pump_test_refs.status_label) return;
  uint32_t now_ms = millis();
  if (g_pump_test_cycle >= 6) {
    closeLineIn();
    lv_label_set_text(g_pump_test_refs.status_label, "Done. Press Back.");
    return;
  }
  if (now_ms - g_pump_test_last_ms >= 1000) {
    g_pump_test_last_ms = now_ms;
    g_pump_test_cycle++;
    if (g_pump_test_cycle % 2 == 0) openLineIn();
    else closeLineIn();
  }
  if (g_pump_test_cycle >= 6) {
    closeLineIn();
    lv_label_set_text(g_pump_test_refs.status_label, "Done. Press Back.");
  } else {
    int cycle = (g_pump_test_cycle / 2) + 1;
    if (cycle > 3) cycle = 3;
    lv_label_set_text_fmt(g_pump_test_refs.status_label, "Blinking... %d/3", cycle);
  }
}

/*
 * update_screensaver
 * Toggles screensaver state and moves the plant when idle.
 * Example:
 *   update_screensaver(millis());
 */
void update_screensaver(uint32_t now_ms) {
  if (g_active_screen != SCREEN_INFO) {
    g_screensaver_active = false;
    return;
  }
  if (feedingIsActive()) {
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

  if (g_screensaver_active && g_info_refs.screensaver_plant) {
    if (now_ms - g_last_screensaver_move_ms > kScreensaverMoveMs) {
      int16_t max_x = static_cast<int16_t>(kScreenWidth - kPlantDrawW);
      int16_t max_y = static_cast<int16_t>(kScreenHeight - (kPlantDrawH + kPlantIconGap + 16));
      if (max_x < 0) max_x = 0;
      if (max_y < 0) max_y = 0;

      int16_t x = static_cast<int16_t>(random(0, max_x + 1));
      int16_t y = static_cast<int16_t>(random(0, max_y + 1));

      g_last_screensaver_move_ms = now_ms;
      lv_obj_set_pos(g_info_refs.screensaver_plant, x, y);
    }
  }
}

/*
 * update_active_screen
 * Routes periodic updates to the currently active screen.
 * Example:
 *   update_active_screen();
 */
void update_active_screen() {
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

/*
 * build_info_screen
 * Builds the idle/feeding info screen layout.
 * Example:
 *   lv_obj_t *screen = build_info_screen();
 */
static lv_obj_t *build_info_screen() {
  lv_obj_t *screen = create_screen_root();
  lv_obj_set_style_pad_all(screen, 2, 0);

  lv_obj_t *main = lv_obj_create(screen);
  lv_obj_set_width(main, LV_PCT(100));
  lv_obj_set_flex_flow(main, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_bg_opa(main, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(main, 0, 0);
  lv_obj_set_style_pad_row(main, 3, 0);
  lv_obj_set_flex_grow(main, 1);
  lv_obj_clear_flag(main, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *soil_band = lv_obj_create(main);
  lv_obj_set_width(soil_band, LV_PCT(100));
  lv_obj_set_height(soil_band, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(soil_band, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_bg_opa(soil_band, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(soil_band, 0, 0);
  lv_obj_set_style_pad_all(soil_band, 0, 0);
  lv_obj_set_style_pad_column(soil_band, 6, 0);
  lv_obj_set_flex_align(soil_band, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  lv_obj_t *moist_col = lv_obj_create(soil_band);
  lv_obj_set_width(moist_col, LV_PCT(25));
  lv_obj_set_flex_grow(moist_col, 1);
  lv_obj_set_height(moist_col, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(moist_col, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_bg_opa(moist_col, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(moist_col, 0, 0);
  lv_obj_set_style_pad_all(moist_col, 0, 0);
  lv_obj_set_style_pad_row(moist_col, 1, 0);

  lv_obj_t *moist_label = lv_label_create(moist_col);
  lv_label_set_text(moist_label, "Moisture");
  lv_obj_set_style_text_font(moist_label, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(moist_label, kColorMuted, 0);
  lv_obj_set_style_text_align(moist_label, LV_TEXT_ALIGN_CENTER, 0);

  lv_obj_t *moist_value = lv_label_create(moist_col);
  lv_obj_set_style_text_font(moist_value, &lv_font_montserrat_18, 0);
  lv_label_set_text(moist_value, "--%");
  lv_obj_set_style_text_align(moist_value, LV_TEXT_ALIGN_CENTER, 0);

  lv_obj_t *baseline_col = lv_obj_create(soil_band);
  lv_obj_set_width(baseline_col, LV_PCT(25));
  lv_obj_set_flex_grow(baseline_col, 1);
  lv_obj_set_height(baseline_col, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(baseline_col, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_bg_opa(baseline_col, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(baseline_col, 0, 0);
  lv_obj_set_style_pad_all(baseline_col, 0, 0);
  lv_obj_set_style_pad_row(baseline_col, 1, 0);

  lv_obj_t *baseline_label = lv_label_create(baseline_col);
  lv_label_set_text(baseline_label, "Baseline");
  lv_obj_set_style_text_font(baseline_label, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(baseline_label, kColorMuted, 0);
  lv_obj_set_style_text_align(baseline_label, LV_TEXT_ALIGN_CENTER, 0);

  lv_obj_t *baseline_value = lv_label_create(baseline_col);
  lv_obj_set_style_text_font(baseline_value, &lv_font_montserrat_18, 0);
  lv_label_set_text(baseline_value, "--%");
  lv_obj_set_style_text_align(baseline_value, LV_TEXT_ALIGN_CENTER, 0);

  lv_obj_t *dryback_col = lv_obj_create(soil_band);
  lv_obj_set_width(dryback_col, LV_PCT(25));
  lv_obj_set_flex_grow(dryback_col, 1);
  lv_obj_set_height(dryback_col, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(dryback_col, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_bg_opa(dryback_col, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(dryback_col, 0, 0);
  lv_obj_set_style_pad_all(dryback_col, 0, 0);
  lv_obj_set_style_pad_row(dryback_col, 1, 0);

  lv_obj_t *dryback_label = lv_label_create(dryback_col);
  lv_label_set_text(dryback_label, "Dryback");
  lv_obj_set_style_text_font(dryback_label, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(dryback_label, kColorMuted, 0);
  lv_obj_set_style_text_align(dryback_label, LV_TEXT_ALIGN_CENTER, 0);

  lv_obj_t *dry_value = lv_label_create(dryback_col);
  lv_obj_set_style_text_font(dry_value, &lv_font_montserrat_18, 0);
  lv_label_set_text(dry_value, "--%");
  lv_obj_set_style_text_align(dry_value, LV_TEXT_ALIGN_CENTER, 0);

  lv_obj_t *minmax_col = lv_obj_create(soil_band);
  lv_obj_set_width(minmax_col, LV_PCT(25));
  lv_obj_set_flex_grow(minmax_col, 1);
  lv_obj_set_height(minmax_col, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(minmax_col, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_bg_opa(minmax_col, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(minmax_col, 0, 0);
  lv_obj_set_style_pad_all(minmax_col, 0, 0);
  lv_obj_set_style_pad_row(minmax_col, 1, 0);

  lv_obj_t *minmax_label = lv_label_create(minmax_col);
  lv_label_set_text(minmax_label, "Min/Max");
  lv_obj_set_style_text_font(minmax_label, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(minmax_label, kColorMuted, 0);
  lv_obj_set_style_text_align(minmax_label, LV_TEXT_ALIGN_CENTER, 0);

  lv_obj_t *minmax_value = lv_label_create(minmax_col);
  lv_obj_set_style_text_font(minmax_value, &lv_font_montserrat_18, 0);
  lv_label_set_text(minmax_value, "--/--%");
  lv_obj_set_style_text_align(minmax_value, LV_TEXT_ALIGN_CENTER, 0);

  lv_obj_t *time_band = lv_obj_create(main);
  lv_obj_set_width(time_band, LV_PCT(100));
  lv_obj_set_height(time_band, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(time_band, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_bg_opa(time_band, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(time_band, 0, 0);
  lv_obj_set_style_pad_all(time_band, 0, 0);
  lv_obj_set_style_pad_column(time_band, 8, 0);
  lv_obj_set_flex_align(time_band, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

  lv_obj_t *time_block = lv_obj_create(time_band);
  lv_obj_set_width(time_block, 110);
  lv_obj_set_flex_grow(time_block, 0);
  lv_obj_set_height(time_block, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(time_block, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_bg_opa(time_block, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(time_block, 0, 0);
  lv_obj_set_style_pad_all(time_block, 0, 0);
  lv_obj_set_style_pad_row(time_block, 1, 0);

  lv_obj_t *time_label = lv_label_create(time_block);
  lv_label_set_text(time_label, "Time");
  lv_obj_set_style_text_font(time_label, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(time_label, kColorMuted, 0);

  lv_obj_t *time_row = lv_obj_create(time_block);
  lv_obj_set_height(time_row, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(time_row, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_bg_opa(time_row, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(time_row, 0, 0);
  lv_obj_set_style_pad_all(time_row, 0, 0);
  lv_obj_set_style_pad_column(time_row, 1, 0);
  lv_obj_set_flex_align(time_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  lv_obj_t *time_value = lv_label_create(time_row);
  lv_obj_set_style_text_font(time_value, &lv_font_montserrat_18, 0);
  lv_label_set_text(time_value, "00:00");

  lv_obj_t *day_night_icon = lv_img_create(time_row);
  lv_img_set_src(day_night_icon, &kSunImage);
  lv_image_set_scale(day_night_icon, kDayNightScale);

  lv_obj_t *night_icon = lv_img_create(time_row);
  lv_img_set_src(night_icon, &kMoonImage);
  lv_image_set_scale(night_icon, kDayNightScale);
  lv_obj_add_flag(night_icon, LV_OBJ_FLAG_HIDDEN);

  lv_obj_t *last_block = lv_obj_create(time_band);
  lv_obj_set_width(last_block, LV_PCT(100));
  lv_obj_set_flex_grow(last_block, 1);
  lv_obj_set_height(last_block, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(last_block, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_bg_opa(last_block, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(last_block, 0, 0);
  lv_obj_set_style_pad_all(last_block, 0, 0);
  lv_obj_set_style_pad_row(last_block, 2, 0);

  lv_obj_t *last_label = lv_label_create(last_block);
  lv_label_set_text(last_label, "Last feed");
  lv_obj_set_style_text_font(last_label, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(last_label, kColorMuted, 0);

  lv_obj_t *last_value = lv_label_create(last_block);
  lv_obj_set_style_text_font(last_value, &lv_font_montserrat_14, 0);
  lv_label_set_text(last_value, "Never");

  lv_obj_t *today_block = lv_obj_create(time_band);
  lv_obj_set_width(today_block, LV_SIZE_CONTENT);
  lv_obj_set_flex_grow(today_block, 0);
  lv_obj_set_height(today_block, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(today_block, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_bg_opa(today_block, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(today_block, 0, 0);
  lv_obj_set_style_pad_all(today_block, 0, 0);
  lv_obj_set_style_pad_row(today_block, 2, 0);
  lv_obj_add_flag(today_block, LV_OBJ_FLAG_FLOATING);
  lv_obj_align(today_block, LV_ALIGN_RIGHT_MID, 0, 0);

  lv_obj_t *today_label = lv_label_create(today_block);
  lv_label_set_text(today_label, "Today");
  lv_obj_set_style_text_font(today_label, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(today_label, kColorMuted, 0);

  lv_obj_t *today_value = lv_label_create(today_block);
  lv_obj_set_style_text_font(today_value, &lv_font_montserrat_14, 0);
  lv_label_set_text(today_value, "0ml");

  lv_obj_t *status_row = lv_obj_create(time_band);
  lv_obj_set_height(status_row, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(status_row, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_bg_opa(status_row, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(status_row, 0, 0);
  lv_obj_set_style_pad_all(status_row, 0, 0);
  lv_obj_set_style_pad_column(status_row, 6, 0);
  lv_obj_set_flex_align(status_row, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_add_flag(status_row, LV_OBJ_FLAG_FLOATING);
  lv_obj_align(status_row, LV_ALIGN_RIGHT_MID, 0, 0);

  lv_obj_t *status_value = lv_label_create(status_row);
  lv_obj_set_style_text_font(status_value, &lv_font_montserrat_14, 0);
  lv_label_set_text(status_value, "DAY");

  lv_obj_t *status_icon = lv_label_create(status_row);
  lv_obj_set_style_text_font(status_icon, &lv_font_montserrat_18, 0);
  lv_label_set_text(status_icon, "");
  lv_obj_add_flag(status_icon, LV_OBJ_FLAG_HIDDEN);

  lv_obj_t *menu_row = lv_obj_create(screen);
  lv_obj_set_width(menu_row, LV_PCT(100));
  lv_obj_set_height(menu_row, 36);
  lv_obj_set_style_bg_opa(menu_row, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(menu_row, 0, 0);
  lv_obj_set_style_pad_all(menu_row, 0, 0);
  lv_obj_set_flex_flow(menu_row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(menu_row, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  lv_obj_t *menu_btn = lv_btn_create(menu_row);
  lv_obj_set_size(menu_btn, 34, 34);
  lv_obj_add_event_cb(menu_btn, open_menu_event, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *menu_label = lv_label_create(menu_btn);
  lv_label_set_text(menu_label, LV_SYMBOL_LIST);
  lv_obj_center(menu_label);

  lv_obj_t *screensaver = lv_obj_create(screen);
  lv_obj_set_size(screensaver, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_opa(screensaver, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(screensaver, 0, 0);
  lv_obj_clear_flag(screensaver, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(screensaver, LV_OBJ_FLAG_FLOATING);
  lv_obj_add_flag(screensaver, LV_OBJ_FLAG_HIDDEN);

  lv_obj_t *plant_wrap = lv_obj_create(screensaver);
  lv_obj_set_size(plant_wrap, kPlantDrawW, kPlantDrawH + kPlantIconGap + 16);
  lv_obj_set_style_bg_opa(plant_wrap, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(plant_wrap, 0, 0);
  lv_obj_clear_flag(plant_wrap, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *plant = lv_img_create(plant_wrap);
  lv_img_set_src(plant, &kPlantImage);
  lv_image_set_scale(plant, kPlantScale);
  lv_obj_align(plant, LV_ALIGN_TOP_MID, 0, 0);

  lv_obj_t *status = lv_label_create(plant_wrap);
  lv_obj_set_style_text_font(status, &lv_font_montserrat_18, 0);
  lv_obj_set_style_text_color(status, kColorMuted, 0);
  lv_label_set_text(status, "");
  lv_obj_align(status, LV_ALIGN_TOP_MID, 0, kPlantDrawH + kPlantIconGap);
  lv_obj_add_flag(status, LV_OBJ_FLAG_HIDDEN);

  g_info_refs.main = main;
  g_info_refs.menu_row = menu_row;
  g_info_refs.menu_btn = menu_btn;
  g_info_refs.moist_value = moist_value;
  g_info_refs.dry_value = dry_value;
  g_info_refs.baseline_value = baseline_value;
  g_info_refs.minmax_value = minmax_value;
  g_info_refs.time_value = time_value;
  g_info_refs.today_value = today_value;
  g_info_refs.status_value = status_value;
  g_info_refs.status_icon = status_icon;
  g_info_refs.day_night_icon = day_night_icon;
  g_info_refs.night_icon = night_icon;
  g_info_refs.last_value = last_value;
  g_info_refs.totals_value = nullptr;
  g_info_refs.screensaver_root = screensaver;
  g_info_refs.screensaver_plant = plant_wrap;
  g_info_refs.screensaver_icon = status;

  update_info_screen();
  return screen;
}

/*
 * build_menu_screen
 * Builds the main menu screen layout.
 * Example:
 *   lv_obj_t *screen = build_menu_screen();
 */
static lv_obj_t *build_menu_screen() {
  lv_obj_t *screen = create_screen_root();
  create_header(screen, "Main Menu", true, back_event);

  lv_obj_t *grid = create_menu_grid(screen);
  int tile_count = 0;
  add_menu_tile(grid, "Logs", open_logs_event, nullptr, nullptr);
  tile_count++;
  add_menu_tile(grid, "Feeding", open_feeding_event, nullptr, nullptr);
  tile_count++;
  add_menu_tile(grid, "Force feed", open_force_feed_event, nullptr, nullptr);
  tile_count++;

  lv_obj_t *pause_label = nullptr;
  add_menu_tile(grid, feedingIsEnabled() ? "Pause feeding" : "Unpause feeding",
                toggle_pause_event, nullptr, &pause_label);
  tile_count++;
  g_pause_menu_label = pause_label;

  add_menu_tile(grid, "Settings", open_settings_menu_event, nullptr, nullptr);
  tile_count++;
  if (tile_count % 2 != 0) add_menu_spacer(grid);

  return screen;
}

/*
 * build_logs_screen
 * Builds the logs viewer screen layout.
 * Example:
 *   lv_obj_t *screen = build_logs_screen();
 */
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

/*
 * build_feeding_menu_screen
 * Builds the feeding menu screen layout.
 * Example:
 *   lv_obj_t *screen = build_feeding_menu_screen();
 */
static lv_obj_t *build_feeding_menu_screen() {
  lv_obj_t *screen = create_screen_root();
  create_header(screen, "Feeding", true, back_event);

  lv_obj_t *grid = create_menu_grid(screen);
  int tile_count = 0;
  add_menu_tile(grid, "Schedule", open_feeding_schedule_event, nullptr, nullptr);
  tile_count++;
  add_menu_tile(grid, "Max daily", open_max_daily_event, nullptr, nullptr);
  tile_count++;
  add_menu_tile(grid, "Baseline - X", open_baseline_x_event, nullptr, nullptr);
  tile_count++;
  add_menu_tile(grid, "Baseline - Y", open_baseline_y_event, nullptr, nullptr);
  tile_count++;
  add_menu_tile(grid, "Baseline delay", open_baseline_delay_event, nullptr, nullptr);
  tile_count++;
  add_menu_tile(grid, "Lights on/off", open_lights_event, nullptr, nullptr);
  tile_count++;
  if (tile_count % 2 != 0) add_menu_spacer(grid);

  return screen;
}

/*
 * build_slot_summary_text
 * Formats a slot summary into four text lines.
 * Example:
 *   char *lines[4] = {nullptr};
 *   build_slot_summary_text(g_slots[0], 0, lines, 4);
 */
static void format_hhmm(char *out, size_t len, uint16_t minutes) {
  if (minutes > 1439) minutes = 1439;
  uint16_t hour = minutes / 60;
  uint16_t minute = minutes % 60;
  snprintf(out, len, "%02u:%02u", static_cast<unsigned>(hour), static_cast<unsigned>(minute));
}

static void format_offset(char *out, size_t len, uint16_t minutes) {
  if (len == 0) return;
  out[0] = '+';
  format_hhmm(out + 1, (len > 0) ? len - 1 : 0, minutes);
}

static void build_slot_summary_text(const Slot &slot, int slot_index, char *lines[4], size_t len) {
  static char line0[64];
  static char line1[64];
  static char line2[64];
  static char line3[64];

  const char *name = slot.name[0] ? slot.name : "(Empty)";
  snprintf(line0, sizeof(line0), "S%d %s (%s)%s",
           slot_index + 1, name, slot.enabled ? "On" : "Off",
           slot.baseline_setter ? " Settr" : "");

  line1[0] = '\0';
  if (!slot.use_window && slot.start_mode == MODE_OFF) {
    snprintf(line1, sizeof(line1), "Start: N/A");
  } else {
    if (slot.use_window) {
      char start_buf[8] = {0};
      char dur_buf[8] = {0};
      format_offset(start_buf, sizeof(start_buf), static_cast<uint16_t>(slot.start_hour * 60 + slot.start_min));
      if (slot.window_duration_min > 0) {
        format_hhmm(dur_buf, sizeof(dur_buf), static_cast<uint16_t>(slot.window_duration_min));
        snprintf(line1, sizeof(line1), "T%s/%s", start_buf, dur_buf);
      } else {
        snprintf(line1, sizeof(line1), "T%s", start_buf);
      }
    }
    if (slot.start_mode != MODE_OFF) {
      char cond[24] = {0};
      if (slot.start_mode == MODE_PERCENT) {
        snprintf(cond, sizeof(cond), "M<%d%%", slot.start_value);
      } else {
        uint8_t baseline = 0;
        if (feedingGetBaselinePercent(&baseline)) {
          uint8_t value = baselineMinus(baseline, config.baselineX);
          if (value == 0) snprintf(cond, sizeof(cond), "M<--");
          else snprintf(cond, sizeof(cond), "M<%d%%", value);
        } else {
          snprintf(cond, sizeof(cond), "M<--");
        }
      }
      if (line1[0] != '\0') strncat(line1, " ", sizeof(line1) - strlen(line1) - 1);
      strncat(line1, cond, sizeof(line1) - strlen(line1) - 1);
    }
  }

  bool has_target = slot.target_mode != MODE_OFF;
  bool has_runoff = slot.stop_on_runoff;
  if (!has_target && !has_runoff) {
    snprintf(line2, sizeof(line2), "Stop: N/A");
  } else {
    snprintf(line2, sizeof(line2), "Stop:");
    if (has_target) {
      char target_buf[24] = {0};
      if (slot.target_mode == MODE_PERCENT) {
        snprintf(target_buf, sizeof(target_buf), " M>%d%%", slot.target_value);
      } else {
        uint8_t baseline = 0;
        if (feedingGetBaselinePercent(&baseline)) {
          uint8_t value = baselineMinus(baseline, config.baselineY);
          if (value == 0) snprintf(target_buf, sizeof(target_buf), " M>--");
          else snprintf(target_buf, sizeof(target_buf), " M>%d%%", value);
        } else {
          snprintf(target_buf, sizeof(target_buf), " M>--");
        }
      }
      strncat(line2, target_buf, sizeof(line2) - strlen(line2) - 1);
    }
    if (has_runoff) {
      char runoff_buf[16] = {0};
      const char suffix = (slot.runoff_mode == RUNOFF_MUST) ? '+'
                          : (slot.runoff_mode == RUNOFF_AVOID) ? '-' : '\0';
      if (suffix) {
        snprintf(runoff_buf, sizeof(runoff_buf), " Runoff%c", suffix);
      } else {
        snprintf(runoff_buf, sizeof(runoff_buf), " Runoff");
      }
      strncat(line2, runoff_buf, sizeof(line2) - strlen(line2) - 1);
    } else if (slot.runoff_mode != RUNOFF_NEITHER) {
      char runoff_buf[8] = {0};
      snprintf(runoff_buf, sizeof(runoff_buf), " R%c",
               slot.runoff_mode == RUNOFF_MUST ? '+' : '-');
      strncat(line2, runoff_buf, sizeof(line2) - strlen(line2) - 1);
    }
  }

  if (slot.max_ml > 0) {
    snprintf(line3, sizeof(line3), "Max:%dml  D:%dm", slot.max_ml, slot.min_gap_min);
  } else {
    snprintf(line3, sizeof(line3), "Max:N/A  D:%dm", slot.min_gap_min);
  }

  lines[0] = line0;
  lines[1] = line1;
  lines[2] = line2;
  lines[3] = line3;
  (void)len;
}

/*
 * update_slot_summary_screen
 * Refreshes the slot summary labels for the selected slot.
 * Example:
 *   update_slot_summary_screen();
 */
void update_slot_summary_screen() {
  if (!g_slot_summary_refs.lines[0]) return;
  char *lines[4] = {nullptr};
  build_slot_summary_text(g_slots[g_selected_slot], g_selected_slot, lines, 4);
  for (int i = 0; i < 4; ++i) {
    lv_label_set_text(g_slot_summary_refs.lines[i], lines[i]);
  }
}

/*
 * build_slots_list_screen
 * Builds the feeding slots list screen.
 * Example:
 *   lv_obj_t *screen = build_slots_list_screen();
 */
static lv_obj_t *build_slots_list_screen() {
  lv_obj_t *screen = create_screen_root();
  lv_obj_set_style_pad_all(screen, 4, 0);
  lv_obj_set_style_pad_row(screen, 2, 0);
  create_header(screen, g_force_feed_mode ? "Force feed" : "Feeding slots", true, back_event);

  lv_obj_t *grid = create_menu_grid(screen);
  lv_obj_set_style_pad_all(grid, 0, 0);
  lv_obj_set_style_pad_row(grid, 2, 0);
  lv_obj_set_style_pad_column(grid, 2, 0);
  lv_obj_clear_flag(grid, LV_OBJ_FLAG_SCROLLABLE);
  for (int i = 0; i < kSlotCount; ++i) {
    lv_obj_t *btn = lv_btn_create(grid);
    lv_obj_set_width(btn, LV_PCT(48));
    lv_obj_set_height(btn, 40);
    lv_obj_set_style_pad_all(btn, 3, 0);
    lv_obj_set_style_pad_row(btn, 1, 0);
    lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_COLUMN);
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(btn, slot_select_event, LV_EVENT_CLICKED, reinterpret_cast<void *>(static_cast<intptr_t>(i)));

    const char *slot_name = g_slots[i].name[0] ? g_slots[i].name : "(Empty)";
    lv_obj_t *name = lv_label_create(btn);
    lv_label_set_text_fmt(name, "S%d %s", i + 1, slot_name);
    lv_label_set_long_mode(name, LV_LABEL_LONG_CLIP);
    lv_obj_set_width(name, LV_PCT(100));
    lv_obj_set_style_text_font(name, &lv_font_montserrat_12, 0);

    lv_obj_t *summary = lv_label_create(btn);
    char summary_buf[48] = {0};
    snprintf(summary_buf, sizeof(summary_buf), "%s%s",
             g_slots[i].enabled ? "ON" : "OFF",
             g_slots[i].baseline_setter ? " Settr" : "");
    if (g_slots[i].use_window) {
      char start_buf[8] = {0};
      char dur_buf[8] = {0};
      format_offset(start_buf, sizeof(start_buf),
                    static_cast<uint16_t>(g_slots[i].start_hour * 60 + g_slots[i].start_min));
      if (g_slots[i].window_duration_min > 0) {
        format_hhmm(dur_buf, sizeof(dur_buf), static_cast<uint16_t>(g_slots[i].window_duration_min));
        strncat(summary_buf, " T", sizeof(summary_buf) - strlen(summary_buf) - 1);
        strncat(summary_buf, start_buf, sizeof(summary_buf) - strlen(summary_buf) - 1);
        strncat(summary_buf, "/", sizeof(summary_buf) - strlen(summary_buf) - 1);
        strncat(summary_buf, dur_buf, sizeof(summary_buf) - strlen(summary_buf) - 1);
      } else {
        strncat(summary_buf, " T", sizeof(summary_buf) - strlen(summary_buf) - 1);
        strncat(summary_buf, start_buf, sizeof(summary_buf) - strlen(summary_buf) - 1);
      }
    } else {
      strncat(summary_buf, " Window: Off", sizeof(summary_buf) - strlen(summary_buf) - 1);
    }
    lv_label_set_text(summary, summary_buf);
    lv_label_set_long_mode(summary, LV_LABEL_LONG_CLIP);
    lv_obj_set_width(summary, LV_PCT(100));
    lv_obj_set_style_text_color(summary, kColorMuted, 0);
    lv_obj_set_style_text_font(summary, &lv_font_montserrat_12, 0);
  }

  return screen;
}

/*
 * build_slot_summary_screen
 * Builds the slot summary screen with edit/feed actions.
 * Example:
 *   lv_obj_t *screen = build_slot_summary_screen();
 */
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

static void wizard_refresh_cb(void *);
static void option_select_event(lv_event_t *event);

/*
 * build_slot_wizard_screen
 * Builds the slot edit wizard shell (content + footer controls).
 * Example:
 *   lv_obj_t *screen = build_slot_wizard_screen();
 */
static lv_obj_t *build_slot_wizard_screen() {
  lv_obj_t *screen = create_screen_root();
  lv_obj_set_style_pad_all(screen, 6, 0);
  lv_obj_set_style_pad_row(screen, 4, 0);
  lv_obj_t *header = create_header(screen, "Edit slot", false, nullptr);

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
  lv_obj_set_height(footer, 32);
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

  g_wizard_refs.header = header;
  g_wizard_refs.step_label = step_label;
  g_wizard_refs.content = content;
  g_wizard_refs.next_btn = next_btn;
  g_wizard_refs.next_label = next_label;
  g_wizard_refs.back_btn = back_btn;
  g_wizard_refs.footer = footer;
  g_wizard_refs.text_area = nullptr;

  wizard_render_step();
  return screen;
}

/*
 * build_settings_menu_screen
 * Builds the settings menu screen layout.
 * Example:
 *   lv_obj_t *screen = build_settings_menu_screen();
 */
static lv_obj_t *build_settings_menu_screen() {
  lv_obj_t *screen = create_screen_root();
  create_header(screen, "Settings", true, back_event);

  lv_obj_t *grid = create_menu_grid(screen);
  int tile_count = 0;
  if (!setup_complete()) {
    add_menu_tile(grid, "Initial setup", open_initial_setup_event, nullptr, nullptr);
    tile_count++;
  }
  add_menu_tile(grid, "Set time/date", open_time_date_event, nullptr, nullptr);
  tile_count++;
  add_menu_tile(grid, "Calibrate", open_cal_menu_event, nullptr, nullptr);
  tile_count++;
  add_menu_tile(grid, "Test peripherals", open_test_menu_event, nullptr, nullptr);
  tile_count++;
  add_menu_tile(grid, "Reset", open_reset_menu_event, nullptr, nullptr);
  tile_count++;
  if (tile_count % 2 != 0) add_menu_spacer(grid);

  return screen;
}

/*
 * build_time_date_screen
 * Builds the time/date wizard shell.
 * Example:
 *   lv_obj_t *screen = build_time_date_screen();
 */
static lv_obj_t *build_time_date_screen() {
  lv_obj_t *screen = create_screen_root();
  lv_obj_set_style_pad_all(screen, 6, 0);
  lv_obj_set_style_pad_row(screen, 4, 0);
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
  lv_obj_set_height(footer, 32);
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

/*
 * build_cal_menu_screen
 * Builds the calibration menu screen layout.
 * Example:
 *   lv_obj_t *screen = build_cal_menu_screen();
 */
static lv_obj_t *build_cal_menu_screen() {
  lv_obj_t *screen = create_screen_root();
  create_header(screen, "Calibrate", true, back_event);

  lv_obj_t *list = create_menu_list(screen);
  add_menu_item(list, "Cal moist sensor", nullptr, open_cal_moist_event, nullptr, nullptr);
  add_menu_item(list, "Calibrate flow", nullptr, open_cal_flow_event, nullptr, nullptr);

  return screen;
}

/*
 * build_cal_moist_screen
 * Builds the moisture calibration screen layout.
 * Example:
 *   lv_obj_t *screen = build_cal_moist_screen();
 */
static lv_obj_t *build_cal_moist_screen() {
  lv_obj_t *screen = create_screen_root();
  create_header(screen, "Cal moisture", true, cal_moist_back_event);

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

  g_cal_moist_refs.mode_label = mode_label;
  g_cal_moist_refs.raw_label = raw_label;
  g_cal_moist_refs.percent_label = percent_label;
  g_cal_moist_refs.choice_row = row;

  update_cal_moist_screen();
  return screen;
}

/*
 * build_cal_flow_screen
 * Builds the flow calibration screen layout.
 * Example:
 *   lv_obj_t *screen = build_cal_flow_screen();
 */
static lv_obj_t *build_cal_flow_screen() {
  lv_obj_t *screen = create_screen_root();
  create_header(screen, "Cal flow", true, back_event);

  reset_binding_pool();

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

  lv_obj_t *target_row = create_number_selector(content, "Target ml/h", &g_cal_flow_target_ml_per_hour, 200, 10000, 200, "%d");
  lv_obj_add_flag(target_row, LV_OBJ_FLAG_HIDDEN);

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
  g_cal_flow_refs.target_row = target_row;

  update_cal_flow_screen();
  return screen;
}

/*
 * build_test_menu_screen
 * Builds the test peripherals menu screen layout.
 * Example:
 *   lv_obj_t *screen = build_test_menu_screen();
 */
static lv_obj_t *build_test_menu_screen() {
  lv_obj_t *screen = create_screen_root();
  create_header(screen, "Test peripherals", true, back_event);

  lv_obj_t *list = create_menu_list(screen);
  add_menu_item(list, "Test sensors", nullptr, open_test_sensors_event, nullptr, nullptr);
  add_menu_item(list, "Test pumps", nullptr, open_test_pumps_event, nullptr, nullptr);

  return screen;
}

/*
 * build_test_sensors_screen
 * Builds the sensor test screen layout.
 * Example:
 *   lv_obj_t *screen = build_test_sensors_screen();
 */
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

/*
 * build_test_pumps_screen
 * Builds the pump test screen layout.
 * Example:
 *   lv_obj_t *screen = build_test_pumps_screen();
 */
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
  openLineIn();

  update_pump_test_screen();
  return screen;
}

/*
 * build_reset_menu_screen
 * Builds the reset menu screen layout.
 * Example:
 *   lv_obj_t *screen = build_reset_menu_screen();
 */
static lv_obj_t *build_reset_menu_screen() {
  lv_obj_t *screen = create_screen_root();
  create_header(screen, "Reset", true, back_event);

  lv_obj_t *list = create_menu_list(screen);
  add_menu_item(list, "Reset logs", nullptr, reset_logs_event, nullptr, nullptr);
  add_menu_item(list, "Reset data", nullptr, reset_factory_event, nullptr, nullptr);

  return screen;
}

/*
 * refresh_initial_setup_screen
 * Rebuilds the initial setup checklist based on completion flags.
 * Example:
 *   refresh_initial_setup_screen();
 */
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

/*
 * maybe_refresh_initial_setup
 * Refreshes the initial setup list if that screen is active.
 * Example:
 *   maybe_refresh_initial_setup();
 */
void maybe_refresh_initial_setup() {
  if (g_active_screen == SCREEN_INITIAL_SETUP) {
    refresh_initial_setup_screen();
  }
}

/*
 * build_initial_setup_screen
 * Builds the initial setup checklist screen layout.
 * Example:
 *   lv_obj_t *screen = build_initial_setup_screen();
 */
static lv_obj_t *build_initial_setup_screen() {
  lv_obj_t *screen = create_screen_root();
  create_header(screen, "Initial setup", true, back_event);

  lv_obj_t *list = create_menu_list(screen);
  g_initial_setup_list = list;
  refresh_initial_setup_screen();

  return screen;
}

/*
 * build_number_input_screen
 * Builds a generic number input screen using g_number_ctx.
 * Example:
 *   lv_obj_t *screen = build_number_input_screen();
 */
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

/*
 * build_time_range_screen
 * Builds a time range input screen using g_time_range_ctx.
 * Example:
 *   lv_obj_t *screen = build_time_range_screen();
 */
static lv_obj_t *build_time_range_screen() {
  lv_obj_t *screen = create_screen_root();
  lv_obj_set_style_pad_all(screen, 6, 0);
  lv_obj_set_style_pad_row(screen, 4, 0);
  create_header(screen, g_time_range_ctx.title ? g_time_range_ctx.title : "Lights", true, back_event);

  reset_binding_pool();

  lv_obj_t *content = lv_obj_create(screen);
  lv_obj_set_width(content, LV_PCT(100));
  lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(content, 0, 0);
  lv_obj_set_style_pad_row(content, 2, 0);
  lv_obj_set_flex_grow(content, 1);
  lv_obj_set_flex_align(content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);

  create_time_input_block(content, "Lights on", &g_time_range_ctx.on_hour, &g_time_range_ctx.on_min);
  create_time_input_block(content, "Lights off", &g_time_range_ctx.off_hour, &g_time_range_ctx.off_min);

  lv_obj_t *ok_btn = lv_btn_create(content);
  lv_obj_set_width(ok_btn, LV_PCT(100));
  lv_obj_set_height(ok_btn, 32);
  lv_obj_add_event_cb(ok_btn, time_range_ok_event, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *ok_label = lv_label_create(ok_btn);
  lv_label_set_text(ok_label, "OK");
  lv_obj_center(ok_label);

  return screen;
}

/*
 * build_screen
 * Factory that builds a screen based on the screen id.
 * Example:
 *   lv_obj_t *root = build_screen(SCREEN_LOGS);
 */
lv_obj_t *build_screen(ScreenId id) {
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

/*
 * option_select_event
 * Event handler that updates an OptionGroup and refreshes dependent UI.
 * Example:
 *   lv_obj_add_event_cb(btn, option_select_event, LV_EVENT_CLICKED, data);
 */
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

/*
 * wizard_render_step
 * Builds the current slot wizard step content in-place.
 * Example:
 *   wizard_render_step();
 */
void wizard_render_step() {
  if (!g_wizard_refs.content) return;
  reset_binding_pool();
  reset_option_pool();
  lv_obj_clean(g_wizard_refs.content);
  g_wizard_refs.text_area = nullptr;

  lv_label_set_text_fmt(g_wizard_refs.step_label, "Step %d/9", g_wizard_step + 1);
  if (g_wizard_refs.header) {
    if (g_wizard_step == 1) lv_obj_add_flag(g_wizard_refs.header, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_clear_flag(g_wizard_refs.header, LV_OBJ_FLAG_HIDDEN);
  }
  if (g_wizard_refs.step_label) {
    if (g_wizard_step == 1) lv_obj_add_flag(g_wizard_refs.step_label, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_clear_flag(g_wizard_refs.step_label, LV_OBJ_FLAG_HIDDEN);
  }
  if (g_wizard_refs.footer) {
    if (g_wizard_step == 1) lv_obj_add_flag(g_wizard_refs.footer, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_clear_flag(g_wizard_refs.footer, LV_OBJ_FLAG_HIDDEN);
  }
  lv_obj_set_style_pad_row(g_wizard_refs.content, g_wizard_step == 1 ? 2 : 6, 0);

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
    KeyboardInputRefs refs = create_keyboard_input(g_wizard_refs.content, g_edit_slot.name,
                                                   wizard_name_done_event, wizard_name_cancel_event, nullptr);
    g_wizard_refs.text_area = refs.text_area;
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

    if (g_edit_slot.start_mode == MODE_PERCENT) {
      create_number_selector(g_wizard_refs.content, "Value", &g_edit_slot.start_value, 20, 90, 1, "%d");
    } else if (g_edit_slot.start_mode == MODE_BASELINE) {
      uint8_t baseline = 0;
      lv_obj_t *note = lv_label_create(g_wizard_refs.content);
      if (feedingGetBaselinePercent(&baseline)) {
        uint8_t value = baselineMinus(baseline, config.baselineX);
        if (value == 0) {
          lv_label_set_text(note, "Baseline: --");
        } else {
          lv_label_set_text_fmt(note, "Baseline: %d%%", value);
        }
      } else {
        lv_label_set_text(note, "Baseline: --");
      }
      lv_obj_set_style_text_color(note, kColorMuted, 0);
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

    if (g_edit_slot.target_mode == MODE_PERCENT) {
      create_number_selector(g_wizard_refs.content, "Value", &g_edit_slot.target_value, 20, 95, 1, "%d");
    } else if (g_edit_slot.target_mode == MODE_BASELINE) {
      uint8_t baseline = 0;
      lv_obj_t *note = lv_label_create(g_wizard_refs.content);
      if (feedingGetBaselinePercent(&baseline)) {
        uint8_t value = baselineMinus(baseline, config.baselineY);
        if (value == 0) {
          lv_label_set_text(note, "Baseline: --");
        } else {
          lv_label_set_text_fmt(note, "Baseline: %d%%", value);
        }
      } else {
        lv_label_set_text(note, "Baseline: --");
      }
      lv_obj_set_style_text_color(note, kColorMuted, 0);
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

    if (g_edit_slot.stop_on_runoff && g_edit_slot.runoff_mode == RUNOFF_MUST) {
      OptionGroup *baseline_group = alloc_option_group();
      if (!baseline_group) return;
      baseline_group->target = &g_edit_slot.baseline_setter;
      baseline_group->target_type = OPTION_TARGET_BOOL;

      lv_obj_t *label = lv_label_create(g_wizard_refs.content);
      lv_label_set_text(label, "Baseline setter?");
      lv_obj_set_style_text_color(label, kColorMuted, 0);

      lv_obj_t *baseline_row = lv_obj_create(g_wizard_refs.content);
      lv_obj_set_width(baseline_row, LV_PCT(100));
      lv_obj_set_style_bg_opa(baseline_row, LV_OPA_TRANSP, 0);
      lv_obj_set_style_border_width(baseline_row, 0, 0);
      lv_obj_set_style_pad_all(baseline_row, 0, 0);
      lv_obj_set_style_pad_gap(baseline_row, 6, 0);
      lv_obj_set_flex_flow(baseline_row, LV_FLEX_FLOW_ROW);
      lv_obj_clear_flag(baseline_row, LV_OBJ_FLAG_SCROLLABLE);

      const char *blabels[] = {"No", "Yes"};
      int bvalues[] = {0, 1};
      for (int i = 0; i < 2; ++i) {
        lv_obj_t *btn = lv_btn_create(baseline_row);
        lv_obj_add_flag(btn, LV_OBJ_FLAG_CHECKABLE);
        OptionButtonData *data = alloc_option_button();
        if (!data) continue;
        data->group = baseline_group;
        data->index = baseline_group->count;
        baseline_group->btns[baseline_group->count] = btn;
        baseline_group->values[baseline_group->count] = bvalues[i];
        baseline_group->count++;
        lv_obj_add_event_cb(btn, option_select_event, LV_EVENT_CLICKED, data);
        lv_obj_t *lab = lv_label_create(btn);
        lv_label_set_text(lab, blabels[i]);
        lv_obj_center(lab);
      }
      if (g_edit_slot.baseline_setter) lv_obj_add_state(baseline_group->btns[1], LV_STATE_CHECKED);
      else lv_obj_add_state(baseline_group->btns[0], LV_STATE_CHECKED);
    } else {
      g_edit_slot.baseline_setter = false;
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

/*
 * wizard_refresh_cb
 * LVGL async callback to rebuild wizard content after option changes.
 * Example:
 *   lv_async_call(wizard_refresh_cb, nullptr);
 */
static void wizard_refresh_cb(void *) {
  if (g_active_screen == SCREEN_SLOT_WIZARD) {
    wizard_render_step();
  }
}

/*
 * time_date_render_step
 * Builds the current time/date wizard step content in-place.
 * Example:
 *   time_date_render_step();
 */
void time_date_render_step() {
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
