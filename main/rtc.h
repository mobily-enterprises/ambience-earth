#ifndef RTC_H
#define RTC_H

#include <stdint.h>

void initRtc();
bool rtcReadMinutesAndDay(uint16_t *minutesOfDay, uint16_t *dayKey);

#endif  // RTC_H
