#pragma once

#include <stdint.h>

/*
 * initRtc
 * Initializes the RTC subsystem.
 * Example:
 *   initRtc();
 */
void initRtc();

/*
 * rtcReadMinutesAndDay
 * Reads minutes-of-day and a day key from the RTC.
 * Example:
 *   uint16_t minutes = 0;
 *   uint16_t dayKey = 0;
 *   rtcReadMinutesAndDay(&minutes, &dayKey);
 */
bool rtcReadMinutesAndDay(uint16_t *minutesOfDay, uint16_t *dayKey);

/*
 * rtcReadDateTime
 * Reads the current RTC date/time fields.
 * Example:
 *   uint8_t h = 0, m = 0, d = 0, mo = 0, y = 0;
 *   rtcReadDateTime(&h, &m, &d, &mo, &y);
 */
bool rtcReadDateTime(uint8_t *hour, uint8_t *minute, uint8_t *day, uint8_t *month, uint8_t *year);

/*
 * rtcSetDateTime
 * Updates the RTC date/time fields.
 * Example:
 *   rtcSetDateTime(10, 30, 0, 14, 2, 25);
 */
bool rtcSetDateTime(uint8_t hour, uint8_t minute, uint8_t second, uint8_t day, uint8_t month, uint8_t year);

/*
 * rtcIsWithinWindow
 * Returns true if the current time is within a window that may wrap midnight.
 * Example:
 *   if (rtcIsWithinWindow(now, start, duration)) { ... }
 */
bool rtcIsWithinWindow(uint16_t nowMinutes, uint16_t startMinutes, uint16_t durationMinutes);

/*
 * daysInMonth
 * Returns the number of days in the given month for a 2-digit year.
 * Example:
 *   uint8_t dim = daysInMonth(month, year);
 */
uint8_t daysInMonth(uint8_t month, uint8_t year);
