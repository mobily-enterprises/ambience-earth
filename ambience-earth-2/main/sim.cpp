#include "sim.h"

#include "app_utils.h"
#include "logs.h"
#include <Arduino.h>

/*
 * init_slots
 * Seeds the default feeding slot definitions for the simulation.
 * Example:
 *   // Called inside sim_init() to populate g_slots.
 */
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

/*
 * sim_start_feed
 * Starts a simulated feed cycle for the given slot index.
 * Example:
 *   sim_start_feed(0);
 */
void sim_start_feed(int slot_index) {
  if (g_sim.feeding_active) return;
  g_sim.feeding_active = true;
  g_sim.feeding_slot = slot_index;
  g_sim.feeding_elapsed = 0;
  g_sim.feeding_water_ml = 0;
  g_sim.feeding_max_ml = g_slots[slot_index].max_ml;
}

/*
 * sim_end_feed
 * Ends the current simulated feed and records a feed log.
 * Example:
 *   // Called internally when duration or volume is reached.
 */
static void sim_end_feed() {
  g_sim.feeding_active = false;
  g_sim.last_feed_uptime = g_sim.uptime_sec;
  g_sim.last_feed_ml = g_sim.feeding_water_ml;
  g_sim.daily_total_ml += g_sim.feeding_water_ml;
  add_log(build_feed_log());
}

/*
 * sim_init
 * Initializes simulation state, slots, and initial logs.
 * Example:
 *   sim_init();
 */
void sim_init() {
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

  add_log(build_boot_log());
  add_log(build_value_log());
}

/*
 * sim_tick
 * Advances the simulation by one second and updates derived values.
 * Example:
 *   sim_tick();
 */
void sim_tick() {
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
