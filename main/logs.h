#ifndef LOGS_H
#define LOGS_H

#include <Arduino.h>
#include <EEPROM.h>

typedef struct {
  unsigned int seq : 8;
  unsigned long millisStart;
  unsigned long millisEnd;
  unsigned int entryType : 3;
  unsigned int actionId : 3;
  unsigned int trayWaterLevelBefore : 7;
  unsigned int soilMoistureBefore : 7;
  bool topFeed : 1;
  unsigned int outcome : 4;
  unsigned int trayWaterLevelAfter : 7;
  unsigned int soilMoistureAfter : 7;
  unsigned int padding1 : 11;
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

#endif LOGS_H