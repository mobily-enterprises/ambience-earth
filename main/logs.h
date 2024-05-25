#ifndef LOGS_H
#define LOGS_H

#include <Arduino.h>
#include <EEPROM.h>

void readLogEntry(int16_t slot = -1);
void writeLogEntry(void* buffer);
bool noLogs();
uint8_t goToNextLogSlot(bool force = false);
uint8_t goToPreviousLogSlot(bool force = false);
void initLogs(void *buffer, int eepromSize, int startAddress, int logsMemory, int slotSize);
void clearLogEntry(void *buffer);
void wipeLogs();

#endif LOGS_H