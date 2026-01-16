#pragma once

#include "app_state.h"
#include <stdio.h>

/*
 * clamp_int
 * Clamps an integer to an inclusive [min,max] range.
 * Example:
 *   int safe = clamp_int(raw_value, 0, 100);
 */
inline int clamp_int(int value, int min_value, int max_value) {
  if (value < min_value) return min_value;
  if (value > max_value) return max_value;
  return value;
}

/*
 * days_in_month
 * Returns the number of days in the given month (non-leap-year simulation).
 * Example:
 *   int dim = days_in_month(g_sim.now.month, g_sim.now.year);
 */
inline int days_in_month(int month, int year) {
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

/*
 * advance_time_one_sec
 * Advances a DateTime by one second with simple calendar rollover.
 * Example:
 *   advance_time_one_sec(&g_sim.now);
 */
inline void advance_time_one_sec(DateTime *dt) {
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

/*
 * format_time
 * Formats time as HH:MM into the provided buffer.
 * Example:
 *   char buf[8] = {0};
 *   format_time(g_sim.now, buf, sizeof(buf));
 */
inline void format_time(const DateTime &dt, char *buf, size_t len) {
  snprintf(buf, len, "%02d:%02d", dt.hour, dt.minute);
}

/*
 * format_date
 * Formats date as MM/DD/YYYY into the provided buffer.
 * Example:
 *   char buf[12] = {0};
 *   format_date(g_sim.now, buf, sizeof(buf));
 */
inline void format_date(const DateTime &dt, char *buf, size_t len) {
  snprintf(buf, len, "%02d/%02d/%04d", dt.month, dt.day, dt.year);
}

/*
 * format_datetime
 * Formats date/time as MM/DD HH:MM into the provided buffer.
 * Example:
 *   char buf[16] = {0};
 *   format_datetime(g_sim.now, buf, sizeof(buf));
 */
inline void format_datetime(const DateTime &dt, char *buf, size_t len) {
  snprintf(buf, len, "%02d/%02d %02d:%02d", dt.month, dt.day, dt.hour, dt.minute);
}

/*
 * format_ago
 * Formats a duration into a short human-friendly string.
 * Example:
 *   char buf[12] = {0};
 *   format_ago(uptime_delta, buf, sizeof(buf));
 */
inline void format_ago(uint32_t seconds, char *buf, size_t len) {
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
