#ifndef LOGS_H
#define LOGS_H

#include <Arduino.h>
#include <EEPROM.h>

#define LOG_FORMAT_VERSION 2

enum LogStopReason : uint8_t {
  LOG_STOP_NONE = 0,
  LOG_STOP_MOISTURE = 1,
  LOG_STOP_RUNOFF = 2,
  LOG_STOP_MAX_RUNTIME = 3,
  LOG_STOP_DISABLED = 4,
  LOG_STOP_UI_PAUSE = 5
};

enum LogStartReason : uint8_t {
  LOG_START_NONE = 0,
  LOG_START_TIME = 1,
  LOG_START_MOISTURE = 2
};

enum LogFlags : uint8_t {
  LOG_FLAG_PULSED = 1u << 0
};

typedef struct {
  uint8_t seq;
  uint8_t entryType : 3;
  uint8_t stopReason : 3;
  uint8_t startReason : 2;
  uint8_t slotIndex : 4;
  uint8_t flags : 4;
  uint8_t trayWaterLevelBefore;
  uint8_t trayWaterLevelAfter;
  uint8_t soilMoistureBefore;
  uint8_t soilMoistureAfter;
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
void initLogs(void *buffer, int eepromSize, int startAddress, int logsMemory, int slotSize, void (*callback)()=nullptr);

void goToLatestSlot(void (*callback)()=nullptr);
void clearLogEntry(void *buffer);
void wipeLogs();
int8_t getCurrentLogSlot();
int8_t getTotalLogSlots();
void goToLogSlot (uint8_t slot);

// Epoch-aware numbering helpers
uint16_t getLogEpoch();
uint16_t getBrowseEpoch();
uint32_t getAbsoluteLogNumber();

#endif LOGS_H
