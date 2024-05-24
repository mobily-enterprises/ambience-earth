#include "logs.h"
#include <EEPROM.h>
#include <Arduino.h>

LogEntry logEntry;
uint8_t currentLogSlot;

void readLogEntryFromEEPROM(LogEntry &logEntry, int slotNumber) {
    // Calculate the EEPROM address for the given slot number
    int eepromAddress = EEPROM_START_ADDRESS + (slotNumber) * EEPROM_SLOT_SIZE;
    Serial.println("Address:");
    Serial.println(EEPROM_START_ADDRESS);
    Serial.println(slotNumber);
    Serial.println(EEPROM_SLOT_SIZE);
    Serial.println(eepromAddress);

    // Read data from EEPROM into memory
    for (unsigned int i = 0; i < sizeof(LogEntry); i++) {
        byte value = EEPROM.read(eepromAddress + i);
        *((byte*)&logEntry + i) = value;
    }
}

void writeLogEntryToEEPROM(const LogEntry &logEntry, int slotNumber) {
    // Calculate the EEPROM address for the given slot number
    int eepromAddress = EEPROM_START_ADDRESS + (slotNumber) * EEPROM_SLOT_SIZE;

    // Write data to EEPROM
    for (unsigned int i = 0; i < sizeof(LogEntry); i++) {
        byte value = *((byte*)&logEntry + i);
        EEPROM.write(eepromAddress + i, value);
        delay(5); // Delay to ensure EEPROM write completes
    }
}
void addLogEntry(const LogEntry &logEntry) {
  uint8_t slotNumber = getNextSlot(currentLogSlot);
 
   writeLogEntryToEEPROM(logEntry, currentLogSlot);
   currentLogSlot = slotNumber;
}

uint8_t findSlotNumberOfHighestMillisStartLog() {
    // Initialize highestMillisStart with 0
    unsigned long highestMillisStart = 0;
    int highestMillisStartSlot = -1; // Initialize with an invalid slot number

    // Iterate through each slot
    for (int slotNumber = 0; slotNumber < EEPROM_MAX_SLOTS; slotNumber++) {
        // Read log entry from EEPROM
        LogEntry currentLogEntry;
        readLogEntryFromEEPROM(currentLogEntry, slotNumber);

        // Check if millisStart is higher than current highestMillisStart
        if (currentLogEntry.millisStart > highestMillisStart) {
            // Update highestMillisStart and highestMillisStartSlot
            highestMillisStart = currentLogEntry.millisStart;
            highestMillisStartSlot = slotNumber;
        }
    }

    // This will only ever happen on the FIRST log entry, where there is no
    // "winner" as all timestamps will be 0. In that case, return EEPROM_MAX_SLOTS
    // so that the slot "1" will be the next one
    if (highestMillisStartSlot == -1) return EEPROM_MAX_SLOTS - 1;
    return highestMillisStartSlot;
}


uint8_t getNextSlot(int currentLogSlot) {
    // Calculate the next slot number
    int nextSlot = (currentLogSlot % EEPROM_MAX_SLOTS-1) + 1;

    return nextSlot;
}

uint8_t getPreviousSlot(int currentLogSlot) {
    // Calculate the previous slot number
    int previousSlot = (currentLogSlot + EEPROM_MAX_SLOTS - 1) % EEPROM_MAX_SLOTS;

    return previousSlot;
}

void initLogs() {
  currentLogSlot = findSlotNumberOfHighestMillisStartLog();
  Serial.print("AH:");
  Serial.println(currentLogSlot);
}

void clearLogEntry(LogEntry &logEntry) {
  logEntry.millisStart = 0;
  logEntry.millisEnd = 0;
  logEntry.actionId = 0;
  logEntry.trayWaterLevelBefore = 0;
  logEntry.soilMoistureBefore = 0;
  logEntry.topFeed = -0;
  logEntry.outcome = 0;
  logEntry.trayWaterLevelAfter = 0;
  logEntry.soilMoistureAfter = 0;
}

void wipeLogs() {
  for (uint16_t i = EEPROM_START_ADDRESS; i < EEPROM_SIZE; i++) {
    EEPROM.write(i, 0);
  }
}
