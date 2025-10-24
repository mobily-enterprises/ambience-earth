#include "logs.h"
#include <EEPROM.h>
#include <Arduino.h>

int EEPROM_SIZE;
int EEPROM_START_ADDRESS;
int EEPROM_LOGS_MEMORY;
int EEPROM_SLOT_SIZE;
int EEPROM_TOTAL_SLOTS;

void* logBuffer; 
int8_t currentSlot;

// Treat 8-bit sequence numbers with wrap-around semantics.
// Returns true if 'a' is later than 'b' assuming differences < 128.
static inline bool seqIsLater(uint8_t a, uint8_t b) {
  return (int8_t)(a - b) > 0;
}

// A slot is considered empty if seq == 0 (we always avoid writing 0).
static inline bool slotIsEmpty(uint8_t seq) {
  return seq == 0;
}

int8_t getCurrentLogSlot() {
  return currentSlot;
}

int8_t getTotalLogSlots() {
  return EEPROM_TOTAL_SLOTS;
}

void readLogEntry(int16_t slot = -1) {
  // Serial.print("readLogEntry()  parameter: ");
  // Serial.println(slot);
  
  // Serial.print("Current slot:");
  // Serial.println(currentSlot);


  int16_t slotToRead = slot == -1 ? currentSlot : slot;
  // Serial.print("Actually reading slot: ");
  // Serial.println(slotToRead);
  
  // Calculate the EEPROM address for the given slot number
  int eepromAddress = EEPROM_START_ADDRESS + (slotToRead) * EEPROM_SLOT_SIZE;
  
  // Read data from EEPROM into memory
  for (unsigned int i = 0; i < EEPROM_SLOT_SIZE; i++) {
    byte value = EEPROM.read(eepromAddress + i);
    *((byte*)logBuffer + i) = value;
  }
  
}

void goToLogSlot (uint8_t slot) {
  currentSlot = slot;
  readLogEntry();
}


void goToLatestSlot (void (*callback)()=nullptr) {
  // Find the latest slot using wrap-aware sequence comparison.
  // Ignore empty slots (seq == 0). With <128 slots, wrap is safe.
  bool found = false;
  uint8_t bestSeq = 0;
  int8_t bestSlot = -1;

  currentSlot = -1;

  for (int slotNumber = 0; slotNumber < EEPROM_TOTAL_SLOTS; slotNumber++) {
    readLogEntry(slotNumber);
    if (callback) callback();

    uint8_t seq = *static_cast<uint8_t*>(logBuffer);
    if (slotIsEmpty(seq)) continue;

    if (!found) {
      bestSeq = seq;
      bestSlot = slotNumber;
      found = true;
    } else if (seqIsLater(seq, bestSeq)) {
      bestSeq = seq;
      bestSlot = slotNumber;
    }
  }

  currentSlot = bestSlot;
  if (currentSlot == -1) {
    clearLogEntry(logBuffer);
  } else {
    readLogEntry(currentSlot);
  }
}


void writeLogEntry(void* buffer) {

  // Serial.println("writeLogEntry()"); 
  // Serial.print("SLOT BEFORE: ");
  // Serial.println(currentSlot);
  
  // Next slot, next SEQ
  // // Serial.println("Calling goToLatestSlot() AND goToNextLogSlot()");
  goToLatestSlot();

  // Serial.print("Latest slot's seq: ");
  // Serial.println(*static_cast<uint8_t*>(logBuffer));

  uint8_t newSeq = (*static_cast<uint8_t*>(logBuffer));
  newSeq++;
  // Keep 0 reserved for "empty" so wrap 255->1
  if (newSeq == 0) newSeq = 1;
  // Serial.print("New seq: ");
  // Serial.println(newSeq);

  goToNextLogSlot(true);

  // Serial.println("DONE Calling goToLatestSlot() AND goToNextLogSlot()");

  // Serial.print("SLOT AFTER: ");
  // Serial.println(currentSlot);
  
  // Calculate the EEPROM address for the given slot number
  int eepromAddress = EEPROM_START_ADDRESS + (currentSlot) * EEPROM_SLOT_SIZE;
  uint8_t currentLogEntrySeq = *static_cast<uint8_t*>(logBuffer);

  // Set the current seq, ALWAYS assumed to be the first 8 bites of the bffer
  memcpy(buffer, &newSeq, sizeof(uint8_t));

  // Serial.print("SEQ IN BUFFER:");
  // Serial.print(*static_cast<uint8_t*>(buffer));
  // Serial.print(" MILLIS:");
  // Serial.println(*reinterpret_cast<uint16_t*>(static_cast<uint8_t*>(buffer) + 1));


  // Write data to EEPROM with minimal wear
  for (unsigned int i = 0; i < EEPROM_SLOT_SIZE; i++) {
    byte value = *((byte*)buffer + i);
    EEPROM.update(eepromAddress + i, value);
  }

  // Re-read the log entry, 
  readLogEntry();
}

bool noLogs() {
  uint8_t currentLogEntrySeq = *static_cast<uint8_t*>(logBuffer);
  
  // Serial.print("EH CHE CAZZ: ");
  // Serial.println(currentLogEntrySeq);
  
  // Serial.print("MILLIS: ");
  // Serial.println(*reinterpret_cast<uint16_t*>(static_cast<uint8_t*>(logBuffer) + 1));


  return !currentLogEntrySeq;
}




uint8_t goToNextLogSlot(bool force = false) {
  int8_t originalSlot = currentSlot;

  // First log ever: target slot 0
  if (originalSlot < 0) {
    currentSlot = 0;
    return true;
  }

  currentSlot = originalSlot;
  readLogEntry();
  uint8_t originalLogEntrySeq = *static_cast<uint8_t*>(logBuffer);
  
  // Serial.println("goToNextLogSlot()");
  // Serial.println("Current slot:");
  // Serial.println(currentSlot);
  currentSlot ++;
  // Serial.print("New current slot: ");
  // Serial.println(currentSlot);
  
  if (currentSlot >= EEPROM_TOTAL_SLOTS) currentSlot = 0;
  // Serial.print("Actual current slot: ");
  // Serial.println(currentSlot);
  readLogEntry();

  if (force) return true;
  uint8_t newLogEntrySeq = *static_cast<uint8_t*>(logBuffer);
  if (!slotIsEmpty(newLogEntrySeq) && seqIsLater(newLogEntrySeq, originalLogEntrySeq)) {
    return true;
  }

  currentSlot = originalSlot;
  readLogEntry();
  return false;

}

uint8_t goToPreviousLogSlot(bool force = false) {
  int8_t originalSlot = currentSlot;
  if (originalSlot < 0) originalSlot = 0; // normalise if uninitialised
  currentSlot = originalSlot;
  readLogEntry();
  uint8_t originalLogEntrySeq = *static_cast<uint8_t*>(logBuffer);

  currentSlot --;
  if (currentSlot < 0) currentSlot = EEPROM_TOTAL_SLOTS - 1;
  readLogEntry();

  if (force) return true;

  uint8_t newLogEntrySeq = *static_cast<uint8_t*>(logBuffer);
  // Allow moving back only if the target is a valid older entry
  if (!slotIsEmpty(newLogEntrySeq) && seqIsLater(originalLogEntrySeq, newLogEntrySeq)) {
    return true;
  }

  currentSlot = originalSlot;
  readLogEntry();
  return false;
}

void initLogs(void *buffer, int eepromSize, int startAddress, int logsMemory, int slotSize, void (*callback)()=nullptr) {
  logBuffer = buffer;
  EEPROM_SIZE = eepromSize;
  EEPROM_START_ADDRESS = startAddress;
  EEPROM_LOGS_MEMORY = logsMemory;
  EEPROM_SLOT_SIZE = slotSize;
  EEPROM_TOTAL_SLOTS = ((EEPROM_SIZE - EEPROM_START_ADDRESS) / EEPROM_SLOT_SIZE);


  // Will set currentSlot
  goToLatestSlot(callback);

  // Serial.println("EEPROM_SIZE");
  // Serial.println(EEPROM_SIZE); 
  // Serial.println("EEPROM_START_ADDRESS"); 
  // Serial.println(EEPROM_START_ADDRESS);
  // Serial.println("EEPROM_LOGS_MEMORY");
  // Serial.println(EEPROM_LOGS_MEMORY);
  // Serial.println("EEPROM_SLOT_SIZE");
  // Serial.println(EEPROM_SLOT_SIZE);
  // Serial.println("EEPROM_TOTAL_SLOTS");
  // Serial.println(EEPROM_TOTAL_SLOTS); 
}



void clearLogEntry(void *buffer) {
  memset(buffer, 0, EEPROM_SLOT_SIZE);
}

void wipeLogs() {
  for (uint16_t i = EEPROM_START_ADDRESS; i < EEPROM_START_ADDRESS + (EEPROM_TOTAL_SLOTS * EEPROM_SLOT_SIZE); i++) {
    EEPROM.update(i, 0);
  }
  currentSlot = - 1;
  clearLogEntry(logBuffer);
}

int8_t getCurrentSlot() {
  return currentSlot;
}
