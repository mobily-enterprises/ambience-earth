#include "logs.h"

#include "app_utils.h"
#include <string.h>

/*
 * add_log
 * Inserts a log entry at the head of the log buffer, shifting older entries.
 * Example:
 *   add_log(build_boot_log());
 */
void add_log(const LogEntry &entry) {
  if (g_log_count < kMaxLogs) {
    g_log_count++;
  }
  for (int i = g_log_count - 1; i > 0; --i) {
    g_logs[i] = g_logs[i - 1];
  }
  g_logs[0] = entry;
}

/*
 * build_value_log
 * Builds a type-2 values log from the current simulated state.
 * Example:
 *   LogEntry entry = build_value_log();
 */
LogEntry build_value_log() {
  LogEntry entry = {};
  entry.type = 2;
  entry.dt = g_sim.now;
  entry.soil_pct = g_sim.moisture;
  entry.dryback = g_sim.dryback;
  return entry;
}

/*
 * build_boot_log
 * Builds a type-0 boot/marker log from the current simulated state.
 * Example:
 *   add_log(build_boot_log());
 */
LogEntry build_boot_log() {
  LogEntry entry = {};
  entry.type = 0;
  entry.dt = g_sim.now;
  entry.soil_pct = g_sim.moisture;
  entry.dryback = g_sim.dryback;
  return entry;
}

/*
 * build_feed_log
 * Builds a type-1 feed log from the current feeding session.
 * Example:
 *   LogEntry entry = build_feed_log();
 */
LogEntry build_feed_log() {
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
