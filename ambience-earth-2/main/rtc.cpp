#include "rtc.h"

#include <Arduino.h>
#include <Wire.h>

namespace {
const uint8_t kRtcAddress = 0x68;
// Shared I2C bus with the FT6206 touch controller.
const uint8_t kI2cSda = 10;
const uint8_t kI2cScl = 8;

uint8_t bcdToDec(uint8_t value) {
  return static_cast<uint8_t>((value >> 4) * 10 + (value & 0x0F));
}

uint8_t decToBcd(uint8_t value) {
  return static_cast<uint8_t>(((value / 10) << 4) | (value % 10));
}

uint8_t decodeHour(uint8_t value) {
  if (value & 0x40) {
    uint8_t hour = bcdToDec(value & 0x1F);
    bool pm = (value & 0x20) != 0;
    if (pm && hour != 12) hour = static_cast<uint8_t>(hour + 12);
    if (!pm && hour == 12) hour = 0;
    return hour;
  }

  return bcdToDec(value & 0x3F);
}

uint16_t makeDayKey(uint8_t year, uint8_t month, uint8_t day) {
  return static_cast<uint16_t>(year) * 372u + static_cast<uint16_t>(month) * 31u + day;
}

bool readRegisters(uint8_t startReg, uint8_t *buffer, uint8_t length) {
  for (uint8_t attempt = 0; attempt < 2; ++attempt) {
    Wire.beginTransmission(kRtcAddress);
    Wire.write(startReg);
    if (Wire.endTransmission() == 0) {
      uint8_t readCount = Wire.requestFrom(kRtcAddress, length);
      if (readCount == length) {
        for (uint8_t i = 0; i < length; ++i) {
          buffer[i] = Wire.read();
        }
        return true;
      }
    }
    delay(2);
  }
  return false;
}
} // namespace

uint8_t daysInMonth(uint8_t month, uint8_t year) {
  static const uint8_t kDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (month == 0 || month > 12) return 31;
  uint8_t days = kDays[month - 1];
  if (month == 2 && (year % 4u) == 0) days = 29;
  return days;
}

void initRtc() {
  Wire.begin(kI2cSda, kI2cScl);
}

bool rtcReadMinutesAndDay(uint16_t *minutesOfDay, uint16_t *dayKey) {
  uint8_t hourVal = 0;
  uint8_t minuteVal = 0;
  uint8_t dayVal = 0;
  uint8_t monthVal = 0;
  uint8_t yearVal = 0;
  if (!rtcReadDateTime(&hourVal, &minuteVal, &dayVal, &monthVal, &yearVal)) return false;

  if (minutesOfDay) {
    *minutesOfDay = static_cast<uint16_t>(hourVal) * 60u + minuteVal;
  }
  if (dayKey) {
    *dayKey = makeDayKey(yearVal, monthVal, dayVal);
  }
  return true;
}

bool rtcReadDateTime(uint8_t *hour, uint8_t *minute, uint8_t *day, uint8_t *month, uint8_t *year) {
  uint8_t data[7];
  if (!readRegisters(0x00, data, sizeof(data))) return false;

  uint8_t minuteVal = bcdToDec(data[1] & 0x7F);
  uint8_t hourVal = decodeHour(data[2]);
  uint8_t dayVal = bcdToDec(data[4] & 0x3F);
  uint8_t monthVal = bcdToDec(data[5] & 0x1F);
  uint8_t yearVal = bcdToDec(data[6]);

  if (minuteVal > 59 || hourVal > 23 || dayVal == 0 || monthVal == 0 || monthVal > 12) return false;
  if (dayVal > daysInMonth(monthVal, yearVal)) return false;

  if (minute) *minute = minuteVal;
  if (hour) *hour = hourVal;
  if (day) *day = dayVal;
  if (month) *month = monthVal;
  if (year) *year = yearVal;
  return true;
}

bool rtcSetDateTime(uint8_t hour, uint8_t minute, uint8_t second, uint8_t day, uint8_t month, uint8_t year) {
  if (hour > 23 || minute > 59 || second > 59) return false;
  if (day == 0 || month == 0 || month > 12) return false;
  if (day > daysInMonth(month, year)) return false;

  Wire.beginTransmission(kRtcAddress);
  Wire.write(static_cast<uint8_t>(0x00));
  Wire.write(decToBcd(second));
  Wire.write(decToBcd(minute));
  Wire.write(decToBcd(hour));
  Wire.write(static_cast<uint8_t>(0x01));  // Day of week (unused)
  Wire.write(decToBcd(day));
  Wire.write(decToBcd(month));
  Wire.write(decToBcd(year));
  return Wire.endTransmission() == 0;
}

bool rtcIsWithinWindow(uint16_t nowMinutes, uint16_t startMinutes, uint16_t durationMinutes) {
  if (durationMinutes == 0) return false;
  if (durationMinutes > 1439) durationMinutes = 1439;
  if (startMinutes > 1439) startMinutes = 1439;
  uint16_t endMinutes = static_cast<uint16_t>(startMinutes + durationMinutes);
  if (endMinutes >= 1440) endMinutes = static_cast<uint16_t>(endMinutes - 1440);
  if (startMinutes <= endMinutes) {
    return nowMinutes >= startMinutes && nowMinutes < endMinutes;
  }
  return nowMinutes >= startMinutes || nowMinutes < endMinutes;
}
