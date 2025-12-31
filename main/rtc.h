#ifndef RTC_H
#define RTC_H

#include <stdint.h>

void initRtc();
bool rtcReadMinutesAndDay(uint16_t *minutesOfDay, uint16_t *dayKey);
bool rtcReadDateTime(uint8_t *hour, uint8_t *minute, uint8_t *day, uint8_t *month, uint8_t *year);
bool rtcSetDateTime(uint8_t hour, uint8_t minute, uint8_t second, uint8_t day, uint8_t month, uint8_t year);
bool rtcIsOk();

#endif  // RTC_H
