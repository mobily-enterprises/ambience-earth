#include "logs.h"
#include <EEPROM.h>
#include <Arduino.h>

LogEntry logEntry;
uint8_t lastWrittenLogSlot; 
uint8_t lastWrittenLogSeq;

void readLogEntry(LogEntry &logEntry, int slotNumber) {
  // Calculate the EEPROM address for the given slot number
  int eepromAddress = EEPROM_START_ADDRESS + (slotNumber) * EEPROM_SLOT_SIZE;
  
  //Serial.println("Slot: "); Serial.println(slotNumber);
  
  // Read data from EEPROM into memory
  for (unsigned int i = 0; i < sizeof(LogEntry); i++) {
    byte value = EEPROM.read(eepromAddress + i);
    *((byte*)&logEntry + i) = value;
  }
  /*
  Serial.println("Mills: "); Serial.println(logEntry.millisStart);
  Serial.println("seq: "); Serial.println(logEntry.seq);
  Serial.println("---");
  */
}

void writeLogEntry(const LogEntry &logEntry, int slotNumber) {
    // Calculate the EEPROM address for the given slot number
    int eepromAddress = EEPROM_START_ADDRESS + (slotNumber) * EEPROM_SLOT_SIZE;

    // Write data to EEPROM
    for (unsigned int i = 0; i < sizeof(LogEntry); i++) {
        byte value = *((byte*)&logEntry + i);
        EEPROM.write(eepromAddress + i, value);
        delay(5); // Delay to ensure EEPROM write completes
    }
}
void addLogEntry(LogEntry &logEntry) {
  uint8_t newSlotNumber = getNextSlot(lastWrittenLogSlot);
 
   logEntry.seq = ++lastWrittenLogSeq;
   /*
   Serial.print("Next slot:"); Serial.println(newSlotNumber);
   Serial.print("With seq:"); Serial.println(lastWrittenLogSeq);
   */
   writeLogEntry(logEntry, newSlotNumber);
   lastWrittenLogSlot = newSlotNumber;
}

uint8_t findSlotNumberOfHighestSeqStartLog() {
  // Initialize highestMillisStart with 0
  unsigned long highestSeq = 0;
  int highestSeqSlot = -1; // Initialize with an invalid slot number

  // Iterate through each slot
  for (int slotNumber = 0; slotNumber < EEPROM_MAX_SLOTS; slotNumber++) {
    // Read log entry from EEPROM
    LogEntry currentLogEntry;
    readLogEntry(currentLogEntry, slotNumber);

    // Check if millisStart is higher than current highestMillisStart
    if (currentLogEntry.seq >= highestSeq) {
      // Update highestMillisStart and highestMillisStartSlot
      highestSeq = currentLogEntry.seq;
      highestSeqSlot = slotNumber;
    }
  }

  // This will only ever happen on the FIRST log entry, where there is no
  // "winner" as all timestamps will be 0. In that case, return EEPROM_MAX_SLOTS
  // so that the slot "1" will be the next one
  if (highestSeqSlot == -1) return EEPROM_MAX_SLOTS - 1;
  return highestSeqSlot;
}


uint8_t getNextSlot(int lastWrittenLogSlot) {
    // Calculate the next slot number
    uint8_t nextLogSlot = lastWrittenLogSlot + 1;
    if (nextLogSlot >= EEPROM_MAX_SLOTS) nextLogSlot = 0;
    return nextLogSlot;
}

uint8_t getPreviousSlot(int lastWrittenLogSlot) {
    // Calculate the previous slot number
    int previousSlot = (lastWrittenLogSlot + EEPROM_MAX_SLOTS - 1) % EEPROM_MAX_SLOTS;

    return previousSlot;
}

void initLogs() {
  /*
  Serial.println("LOG SLOT SIZE:");
  Serial.println(EEPROM_SLOT_SIZE);
  Serial.println("MEM AVAILABLE:");
  Serial.println((1024 - 256));
  Serial.println("MAX SLOTS:");
  Serial.println(EEPROM_MAX_SLOTS);
  */

  lastWrittenLogSlot = findSlotNumberOfHighestSeqStartLog();
  readLogEntry(logEntry, lastWrittenLogSlot);
  lastWrittenLogSeq = logEntry.seq;

  /*
  Serial.print("LAST WRITTEN SLOT:");
  Serial.println(lastWrittenLogSlot);
  Serial.print("LAST WRIEEN LOG SEQ:");
  Serial.println(lastWrittenLogSeq);
  */

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
  lastWrittenLogSlot = EEPROM_MAX_SLOTS - 1;
}
