#include "rtc.h"

#include <Arduino.h>
#include <Wire.h>

namespace {
const uint8_t kRtcAddress = 0x68;

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
  Wire.beginTransmission(kRtcAddress);
  Wire.write(startReg);
  if (Wire.endTransmission() != 0) return false;

  uint8_t readCount = Wire.requestFrom(kRtcAddress, length);
  if (readCount != length) return false;

  for (uint8_t i = 0; i < length; ++i) {
    buffer[i] = Wire.read();
  }
  return true;
}
}

void initRtc() {
  Wire.begin();
}

bool rtcReadMinutesAndDay(uint16_t *minutesOfDay, uint16_t *dayKey) {
  uint8_t data[7];
  if (!readRegisters(0x00, data, sizeof(data))) return false;

  uint8_t minute = bcdToDec(data[1] & 0x7F);
  uint8_t hour = decodeHour(data[2]);
  uint8_t day = bcdToDec(data[4] & 0x3F);
  uint8_t month = bcdToDec(data[5] & 0x1F);
  uint8_t year = bcdToDec(data[6]);

  if (minute > 59 || hour > 23 || day == 0 || month == 0 || month > 12) return false;

  if (minutesOfDay) {
    *minutesOfDay = static_cast<uint16_t>(hour) * 60u + minute;
  }
  if (dayKey) {
    *dayKey = makeDayKey(year, month, day);
  }
  return true;
}

bool rtcReadDateTime(uint8_t *hour, uint8_t *minute, uint8_t *day, uint8_t *month, uint8_t *year) {
  uint8_t data[7];
  if (!readRegisters(0x00, data, sizeof(data))) return false;

  if (minute) *minute = bcdToDec(data[1] & 0x7F);
  if (hour) *hour = decodeHour(data[2]);
  if (day) *day = bcdToDec(data[4] & 0x3F);
  if (month) *month = bcdToDec(data[5] & 0x1F);
  if (year) *year = bcdToDec(data[6]);
  return true;
}

bool rtcSetDateTime(uint8_t hour, uint8_t minute, uint8_t second, uint8_t day, uint8_t month, uint8_t year) {
  if (hour > 23 || minute > 59 || second > 59) return false;
  if (day == 0 || day > 31) return false;
  if (month == 0 || month > 12) return false;

  Wire.beginTransmission(kRtcAddress);
  Wire.write((uint8_t)0x00);
  Wire.write(decToBcd(second));
  Wire.write(decToBcd(minute));
  Wire.write(decToBcd(hour));
  Wire.write((uint8_t)0x01);  // Day of week (unused)
  Wire.write(decToBcd(day));
  Wire.write(decToBcd(month));
  Wire.write(decToBcd(year));
  return Wire.endTransmission() == 0;
}
