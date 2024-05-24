#include <Arduino.h>
#ifndef LOGS_H
#define LOGS_H

#define LOG_OUTCOME_OK 0

const char PROGMEM LOG_ACTION_ID [] = "No log entries!"; 




typedef struct {
  unsigned long millisStart;
  unsigned long millisEnd;
  unsigned int actionId : 3;
  unsigned int trayWaterLevelBefore : 7;
  unsigned int soilMoistureBefore : 7;
  bool topFeed : 1;
  unsigned int outcome : 4;
  unsigned int trayWaterLevelAfter : 7;
  unsigned int soilMoistureAfter : 7;
  unsigned int padding1 : 24;
} LogEntry;

void readLogEntryFromEEPROM(LogEntry &logEntry, int slotNumber);
void writeLogEntryToEEPROM(const LogEntry &logEntry, int slotNumber);
uint8_t findSlotNumberOfHighestMillisStartLog();
void wipeLogs();
void initLogs();

uint8_t getNextSlot(int currentLogSlot);
uint8_t getPreviousSlot(int currentLogSlot);
void clearLogEntry(LogEntry &logEntry);

#include <EEPROM.h>

#define EEPROM_START_ADDRESS 256
#define EEPROM_SIZE 1024
#define EEPROM_SLOT_SIZE sizeof(LogEntry)
#define EEPROM_MAX_SLOTS ((1024 - 256) / EEPROM_SLOT_SIZE)


#endif LOGS_H