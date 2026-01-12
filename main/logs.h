#ifndef LOGS_H
#define LOGS_H

#include <Arduino.h>
#include <EEPROM.h>

#define LOG_FORMAT_VERSION 8

enum LogStopReason : uint8_t {
  LOG_STOP_NONE = 0,
  LOG_STOP_MOISTURE = 1,
  LOG_STOP_RUNOFF = 2,
  LOG_STOP_MAX_RUNTIME = 3,
  LOG_STOP_DISABLED = 4,
  LOG_STOP_UI_PAUSE = 5,
  LOG_STOP_MAX_DAILY_FEED_REACHED = 6,
  LOG_STOP_FEED_NOT_CALIBRATED = 7
};

enum LogStartReason : uint8_t {
  LOG_START_NONE = 0,
  LOG_START_TIME = 1,
  LOG_START_MOISTURE = 2,
  LOG_START_USER = 3
};

#define LOG_FLAG_RUNOFF_MISSING 0x01
#define LOG_FLAG_RUNOFF_UNEXPECTED 0x02
#define LOG_FLAG_RUNOFF_ANY (LOG_FLAG_RUNOFF_MISSING | LOG_FLAG_RUNOFF_UNEXPECTED)
#define LOG_FLAG_BASELINE_SETTER 0x04
#define LOG_FLAG_RUNOFF_SEEN 0x08
#define LOG_BASELINE_UNSET 0xFF

typedef struct {
  uint16_t seq;
  uint8_t entryType : 3;
  uint8_t stopReason : 3;
  uint8_t startReason : 2;
  uint8_t slotIndex : 4;
  uint8_t flags : 4;
  uint8_t soilMoistureBefore;
  uint8_t soilMoistureAfter;
  uint8_t baselinePercent;
  uint8_t drybackPercent;
  uint16_t feedMl;
  uint16_t dailyTotalMl;
  uint16_t lightDayKey;
  uint8_t startYear;
  uint8_t startMonth;
  uint8_t startDay;
  uint8_t startHour;
  uint8_t startMinute;
  uint8_t endYear;
  uint8_t endMonth;
  uint8_t endDay;
  uint8_t endHour;
  uint8_t endMinute;
  unsigned long millisStart;
  unsigned long millisEnd;
} LogEntry;

void readLogEntry(int16_t slot = -1);
void writeLogEntry(void* buffer);
bool noLogs();
uint8_t goToNextLogSlot(bool force = false);
uint8_t goToPreviousLogSlot(bool force = false);
void initLogs(void *buffer, int eepromSize, int startAddress, int logsMemory, int slotSize);
int16_t getCurrentLogSlot();

void goToLatestSlot();
void clearLogEntry(void *buffer);
void initLogEntryCommon(LogEntry *entry, uint8_t entryType, uint8_t stopReason,
                        uint8_t startReason, uint8_t slotIndex, uint8_t flags,
                        uint8_t soilBefore, uint8_t soilAfter);
void wipeLogs();
bool patchLogBaselinePercent(int16_t slot, uint8_t baselinePercent);
bool findLatestBaselineEntries(LogEntry *outLatestSetter, int16_t *outLatestSetterSlot,
                               LogEntry *outLatestWithBaseline, int16_t *outLatestWithBaselineSlot,
                               LogEntry *outLatestFeed);

// Epoch-aware numbering helpers
uint32_t getAbsoluteLogNumber();
uint16_t getDailyFeedTotalMlAt(uint8_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute,
                               uint8_t *outMin = nullptr, uint8_t *outMax = nullptr);
uint16_t getDailyFeedTotalMlNow(uint8_t *outMin = nullptr, uint8_t *outMax = nullptr);
uint32_t getLightDayKeyAt(uint8_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute);
uint32_t getLightDayKeyNow();
bool getDrybackPercent(uint8_t *outPercent);

#endif LOGS_H
