#ifndef LOGS_H
#define LOGS_H

#include <Arduino.h>
#include <EEPROM.h>

void readLogEntry(int16_t slot = -1);
void writeLogEntry(void* buffer);
bool noLogs();
uint8_t initLib();
uint8_t goToNextSlot();
uint8_t getPreviousSlot();
void initLogs(void *buffer, int eepromSize, int startAddress, int logsMemory, int slotSize);
void clearLogEntry(void *buffer);
void wipeLogs();

#endif LOGS_H