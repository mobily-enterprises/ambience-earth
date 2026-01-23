#include "sim.h"

#include "app_utils.h"
#include "config.h"
#include "feedSlots.h"
#include "feeding.h"
#include "feedingUtils.h"
#include "logs.h"
#include "moistureSensor.h"
#include "pumps.h"
#include "rtc.h"
#include "runoffSensor.h"
#include "volume.h"

static uint32_t g_last_tick_ms = 0;
static uint32_t g_last_toggle_ms = 0;
static uint32_t g_last_values_log_ms = 0;
static const uint8_t kMoistureBaselineSentinel = 127;
static const uint8_t kDefaultRtcHour = 9;
static const uint8_t kDefaultRtcMinute = 16;
static const uint8_t kDefaultRtcDay = 14;
static const uint8_t kDefaultRtcMonth = 2;
static const uint8_t kDefaultRtcYear = 25;

/*
 * sync_sim_time_from_rtc
 * Copies the RTC date/time into g_sim.now.
 * Example:
 *   if (!sync_sim_time_from_rtc()) { ... }
 */
static bool sync_sim_time_from_rtc() {
  uint8_t hour = 0;
  uint8_t minute = 0;
  uint8_t day = 0;
  uint8_t month = 0;
  uint8_t year = 0;
  if (!rtcReadDateTime(&hour, &minute, &day, &month, &year)) return false;

  g_sim.now.hour = hour;
  g_sim.now.minute = minute;
  g_sim.now.day = day;
  g_sim.now.month = month;
  g_sim.now.year = 2000 + year;
  g_sim.now.second = 0;
  return true;
}

/*
 * seed_rtc_if_invalid
 * Seeds the RTC with a default time if it fails to read.
 * Example:
 *   seed_rtc_if_invalid();
 */
static void seed_rtc_if_invalid() {
  if (sync_sim_time_from_rtc()) return;
  rtcSetDateTime(kDefaultRtcHour, kDefaultRtcMinute, 0,
                 kDefaultRtcDay, kDefaultRtcMonth, kDefaultRtcYear);
  sync_sim_time_from_rtc();
}

/*
 * slot_pack_matches
 * Returns true if the given packed slot is all zeros.
 * Example:
 *   if (slot_pack_matches(config.feedSlotsPacked[i])) { ... }
 */
static bool slot_pack_matches(const uint8_t packed[FEED_SLOT_PACKED_SIZE]) {
  for (int i = 0; i < FEED_SLOT_PACKED_SIZE; ++i) {
    if (packed[i] != 0) return false;
  }
  return true;
}

/*
 * slots_are_empty
 * Returns true if all stored slot packs are empty.
 * Example:
 *   if (slots_are_empty()) { seed_default_slots(); }
 */
static bool slots_are_empty() {
  for (int i = 0; i < FEED_SLOT_COUNT; ++i) {
    if (!slot_pack_matches(config.feedSlotsPacked[i])) return false;
    if (config.feedSlotNames[i][0] != '\0') return false;
  }
  return true;
}

/*
 * seed_default_slots
 * Seeds default feed slots into config for simulation.
 * Example:
 *   seed_default_slots();
 */
static void seed_default_slots() {
  for (int i = 0; i < FEED_SLOT_COUNT; ++i) {
    FeedSlot slot = {};
    if (i == 0) {
      slot.windowStartMinutes = 0;
      slot.windowDurationMinutes = 0;
      slot.minGapMinutes = 60;
      slot.maxVolumeMl = 50;
      slot.runoffHold5s = 0;
      slot.flags = static_cast<uint8_t>(FEED_SLOT_ENABLED | FEED_SLOT_HAS_TIME_WINDOW |
                                        FEED_SLOT_RUNOFF_AVOID);
      config.runoffExpectation[i] = 2;
      strncpy(config.feedSlotNames[i], "INIT", FEED_SLOT_NAME_LENGTH);
      config.feedSlotNames[i][FEED_SLOT_NAME_LENGTH] = '\0';
    } else if (i == 1) {
      slot.windowStartMinutes = 30;
      slot.windowDurationMinutes = 0;
      slot.minGapMinutes = 60;
      slot.maxVolumeMl = 800;
      slot.runoffHold5s = 6;
      slot.flags = static_cast<uint8_t>(FEED_SLOT_ENABLED | FEED_SLOT_HAS_TIME_WINDOW |
                                        FEED_SLOT_RUNOFF_REQUIRED | FEED_SLOT_BASELINE_SETTER);
      config.runoffExpectation[i] = 1;
      strncpy(config.feedSlotNames[i], "SOAK", FEED_SLOT_NAME_LENGTH);
      config.feedSlotNames[i][FEED_SLOT_NAME_LENGTH] = '\0';
    } else if (i == 2) {
      slot.windowStartMinutes = 120;
      slot.windowDurationMinutes = 14 * 60;
      slot.moistureBelow = kMoistureBaselineSentinel;
      slot.moistureTarget = kMoistureBaselineSentinel;
      slot.minGapMinutes = 60;
      slot.maxVolumeMl = 50;
      slot.runoffHold5s = 0;
      slot.flags = static_cast<uint8_t>(FEED_SLOT_ENABLED | FEED_SLOT_HAS_TIME_WINDOW |
                                        FEED_SLOT_HAS_MOISTURE_BELOW | FEED_SLOT_HAS_MOISTURE_TARGET |
                                        FEED_SLOT_RUNOFF_AVOID | FEED_SLOT_BASELINE_SETTER);
      config.runoffExpectation[i] = 2;
      strncpy(config.feedSlotNames[i], "REC", FEED_SLOT_NAME_LENGTH);
      config.feedSlotNames[i][FEED_SLOT_NAME_LENGTH] = '\0';
    } else {
      slot.flags = 0;
      config.feedSlotNames[i][0] = '\0';
      config.runoffExpectation[i] = 0;
    }
    packFeedSlot(config.feedSlotsPacked[i], &slot);
  }
  saveConfig();
}

/*
 * update_setup_flags
 * Updates g_setup flags based on the config state.
 * Example:
 *   update_setup_flags();
 */
static void update_setup_flags() {
  g_setup.time_date = (config.flags & CONFIG_FLAG_TIME_SET) != 0;
  g_setup.max_daily = (config.flags & CONFIG_FLAG_MAX_DAILY_SET) != 0 || config.maxDailyWaterMl != 0;
  g_setup.lights = (config.flags & CONFIG_FLAG_LIGHTS_ON_SET) != 0 &&
                   (config.flags & CONFIG_FLAG_LIGHTS_OFF_SET) != 0;
  g_setup.moisture_cal = config.moistSensorCalibrationDry != 1024 && config.moistSensorCalibrationSoaked != 0;
  g_setup.dripper_cal = (config.flags & CONFIG_FLAG_DRIPPER_CALIBRATED) != 0 &&
                        (config.flags & CONFIG_FLAG_PULSE_SET) != 0;
}

/*
 * sync_slots_from_config
 * Mirrors config feed slots into the UI slot list.
 * Example:
 *   sync_slots_from_config();
 */
static void sync_slots_from_config() {
  for (int i = 0; i < kSlotCount; ++i) {
    FeedSlot slot = {};
    unpackFeedSlot(&slot, config.feedSlotsPacked[i]);
    Slot &ui = g_slots[i];
    ui.enabled = slotFlag(&slot, FEED_SLOT_ENABLED);
    strncpy(ui.name, config.feedSlotNames[i], sizeof(ui.name) - 1);
    ui.name[sizeof(ui.name) - 1] = '\0';
    ui.use_window = slotFlag(&slot, FEED_SLOT_HAS_TIME_WINDOW);
    ui.start_hour = static_cast<int>(slot.windowStartMinutes / 60);
    ui.start_min = static_cast<int>(slot.windowStartMinutes % 60);
    ui.window_duration_min = slot.windowDurationMinutes;
    if (!slotFlag(&slot, FEED_SLOT_HAS_MOISTURE_BELOW)) {
      ui.start_mode = MODE_OFF;
      ui.start_value = 0;
    } else if (slot.moistureBelow == kMoistureBaselineSentinel) {
      ui.start_mode = MODE_BASELINE;
      ui.start_value = 0;
    } else {
      ui.start_mode = MODE_PERCENT;
      ui.start_value = slot.moistureBelow;
    }
    if (!slotFlag(&slot, FEED_SLOT_HAS_MOISTURE_TARGET)) {
      ui.target_mode = MODE_OFF;
      ui.target_value = 0;
    } else if (slot.moistureTarget == kMoistureBaselineSentinel) {
      ui.target_mode = MODE_BASELINE;
      ui.target_value = 0;
    } else {
      ui.target_mode = MODE_PERCENT;
      ui.target_value = slot.moistureTarget;
    }
    ui.max_ml = slot.maxVolumeMl;
    ui.stop_on_runoff = slotFlag(&slot, FEED_SLOT_RUNOFF_REQUIRED);
    uint8_t pref = config.runoffExpectation[i];
    if (pref == 1) {
      ui.runoff_mode = RUNOFF_MUST;
    } else if (pref == 2) {
      ui.runoff_mode = RUNOFF_AVOID;
    } else if (slotFlag(&slot, FEED_SLOT_RUNOFF_AVOID)) {
      ui.runoff_mode = RUNOFF_AVOID;
    } else if (ui.stop_on_runoff) {
      ui.runoff_mode = RUNOFF_MUST;
    } else {
      ui.runoff_mode = RUNOFF_NEITHER;
    }
    ui.baseline_setter = slotFlag(&slot, FEED_SLOT_BASELINE_SETTER);
    ui.min_gap_min = slot.minGapMinutes;
  }
}

/*
 * update_sim_values
 * Updates g_sim fields from config, sensors, and feeding runtime.
 * Example:
 *   update_sim_values();
 */
static void update_sim_values() {
  g_sim.baseline_x = config.baselineX;
  g_sim.baseline_y = config.baselineY;
  g_sim.baseline_delay_min = config.baselineDelayMinutes;
  g_sim.max_daily_ml = config.maxDailyWaterMl;

  g_sim.lights_on_hour = static_cast<int>(config.lightsOnMinutes / 60);
  g_sim.lights_on_min = static_cast<int>(config.lightsOnMinutes % 60);
  g_sim.lights_off_hour = static_cast<int>(config.lightsOffMinutes / 60);
  g_sim.lights_off_min = static_cast<int>(config.lightsOffMinutes % 60);

  g_sim.moisture_raw_dry = config.moistSensorCalibrationDry;
  g_sim.moisture_raw_wet = config.moistSensorCalibrationSoaked;

  g_sim.paused = !feedingIsEnabled();
  g_sim.runoff = runoffDetected();

  uint16_t raw = soilSensorWindowLastRaw();
  g_sim.moisture_raw = raw;
  g_sim.moisture = soilMoistureAsPercentage(getSoilMoisture());

  uint8_t dryback = 0;
  g_sim.dryback = getDrybackPercent(&dryback) ? dryback : 0;

  g_sim.last_feed_ml = lastFeedMl;
  g_sim.last_feed_uptime = millisAtEndOfLastFeed ? (millisAtEndOfLastFeed / 1000UL) : 0;
  g_sim.daily_total_ml = getDailyFeedTotalMlNow();

  FeedStatus status = {};
  if (feedingGetStatus(&status)) {
    g_sim.feeding_active = true;
    g_sim.feeding_slot = status.slotIndex;
    g_sim.feeding_elapsed = status.elapsedSeconds;
    g_sim.feeding_max_ml = status.maxVolumeMl;
    g_sim.feeding_water_ml = msToVolumeMl(static_cast<uint32_t>(status.elapsedSeconds) * 1000UL,
                                          config.dripperMsPerLiter);
  } else {
    g_sim.feeding_active = false;
    g_sim.feeding_elapsed = 0;
    g_sim.feeding_water_ml = 0;
    g_sim.feeding_max_ml = 0;
  }

  if (config.dripperMsPerLiter > 0) {
    uint64_t ml_per_hour = (3600000ULL * 1000ULL) / config.dripperMsPerLiter;
    g_sim.dripper_ml_per_hour = static_cast<int>(ml_per_hour);
  } else {
    g_sim.dripper_ml_per_hour = 0;
  }
}

void sim_start_feed(int slot_index) {
  feedingForceFeed(static_cast<uint8_t>(slot_index));
}

void sim_init() {
  initConfig();
  initRtc();
  initRunoffSensor();
  initPumps();
  initMoistureSensor();

  if (slots_are_empty()) {
    seed_default_slots();
  }

  seed_rtc_if_invalid();
  g_sim.uptime_sec = 0;
  g_sim.info_toggle = 0;

  millisAtEndOfLastFeed = 0;
  lastFeedMl = 0;

  sync_slots_from_config();
  update_setup_flags();
  feedingBaselineInit();
  setSoilSensorLazy();

  g_last_tick_ms = millis();
  g_last_toggle_ms = g_last_tick_ms;
  g_last_values_log_ms = g_last_tick_ms;

  add_log(build_boot_log());
}

void sim_factory_reset() {
  restoreDefaultConfig();
  saveConfig();
  applySimDefaults();
  logs_wipe();

  if (slots_are_empty()) {
    seed_default_slots();
  }
  sync_slots_from_config();
  update_setup_flags();

  seed_rtc_if_invalid();
  g_sim.uptime_sec = 0;
  g_sim.info_toggle = 0;
  millisAtEndOfLastFeed = 0;
  lastFeedMl = 0;

  feedingBaselineInit();
  setSoilSensorLazy();

  g_last_tick_ms = millis();
  g_last_toggle_ms = g_last_tick_ms;
  g_last_values_log_ms = g_last_tick_ms;

  add_log(build_boot_log());
}

void sim_tick() {
  uint32_t now_ms = millis();
  if (g_last_tick_ms == 0) g_last_tick_ms = now_ms;
  while (now_ms - g_last_tick_ms >= 1000) {
    g_last_tick_ms += 1000;
    g_sim.uptime_sec += 1;
    if (!sync_sim_time_from_rtc()) {
      advance_time_one_sec(&g_sim.now);
    }
  }

  runSoilSensorLazyReadings();
  feedingTick();
  feedingBaselineTick();

  if (soilSensorReady() && (now_ms - g_last_values_log_ms >= kLogValuesIntervalMs)) {
    g_last_values_log_ms = now_ms;
    add_log(build_value_log());
  }

  if (now_ms - g_last_toggle_ms >= 5000) {
    g_last_toggle_ms = now_ms;
    g_sim.info_toggle = 1 - g_sim.info_toggle;
  }

  update_sim_values();
}
