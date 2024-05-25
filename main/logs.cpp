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
int8_t currentSeq;

void readLogEntry(int16_t slot = -1) {
  Serial.print("readLogEntry()  parameter: ");
  Serial.println(slot);
  
  Serial.print("Current slot:");
  Serial.println(currentSlot);


  int16_t slotToRead = slot == -1 ? currentSlot : slot;
  Serial.print("Actually reading slot: ");
  Serial.println(slotToRead);
  
  // Calculate the EEPROM address for the given slot number
  int eepromAddress = EEPROM_START_ADDRESS + (slotToRead) * EEPROM_SLOT_SIZE;
  
  // Serial.print("EEPROM ADDRESS:");
  // Serial.println(eepromAddress);
  
  // Read data from EEPROM into memory
  for (unsigned int i = 0; i < EEPROM_SLOT_SIZE; i++) {
    byte value = EEPROM.read(eepromAddress + i);
    *((byte*)logBuffer + i) = value;
  } 
}

void goToLatestSlot () {
// Initialize highestMillisStart with 0
  currentSeq = 0;
  currentSlot = -1;
  Serial.println("goToLatestSlot()");
  

  for (int slotNumber = 0; slotNumber < EEPROM_TOTAL_SLOTS; slotNumber++) {
    Serial.print("SLOT ");
    Serial.print(slotNumber);
    Serial.print(": ");
    // Read log entry from EEPROM
    readLogEntry(slotNumber);

    uint8_t currentLogEntrySeq = *static_cast<uint8_t*>(logBuffer);
    uint16_t millis = *reinterpret_cast<uint16_t*>(static_cast<uint8_t*>(logBuffer) + 1);
    
    Serial.print("Seq and ms: ");
    Serial.print(currentLogEntrySeq);
    Serial.print(", ");
    Serial.println(millis);
    
    if (currentLogEntrySeq > currentSeq) {
      currentSeq = currentLogEntrySeq;
      currentSlot = slotNumber;
    }    
  }

  Serial.print("RESULT: ");
  Serial.print(currentSeq);
  Serial.print(", ");
  Serial.print(currentSlot);
  Serial.println(". ");


   // No slot was ever assigned: leaves -1 one as the current one,
   // so that the next entry will go on slot 0 (which is what we want)  

  if (currentSlot == -1) {
    clearLogEntry(logBuffer);
  } else {
    readLogEntry(currentSlot);
  }
}


void writeLogEntry(void* buffer) {

  Serial.println("writeLogEntry()"); 
  Serial.print("SLOT AND SEQ BEFORE: ");
  Serial.print(currentSlot);
  Serial.print(", ");
  Serial.println(currentSeq);

  // Next slot, next SEQ
  Serial.println("Calling goToLatestSlot() AND goToNextLogSlot()");
  goToLatestSlot();
  goToNextLogSlot(true);

  currentSeq++;

  Serial.println("DONE Calling goToLatestSlot() AND goToNextLogSlot()");

  Serial.print("SLOT AND SEQ AFTER: ");
  Serial.print(currentSlot);
  Serial.print(", ");
  Serial.println(currentSeq);
  
  // Calculate the EEPROM address for the given slot number
  int eepromAddress = EEPROM_START_ADDRESS + (currentSlot) * EEPROM_SLOT_SIZE;

  // Set the current seq, ALWAYS assumed to be the first 8 bites of the bffer
  memcpy(buffer, &currentSeq, sizeof(uint8_t));

  Serial.println("SEQ IN BUFFER:");
  Serial.println(*static_cast<uint8_t*>(buffer));
  Serial.println("MILLIS:");
  Serial.println(*reinterpret_cast<uint16_t*>(static_cast<uint8_t*>(buffer) + 1));

  // Write data to EEPROM
  for (unsigned int i = 0; i < EEPROM_SLOT_SIZE; i++) {
    byte value = *((byte*)buffer + i);
    EEPROM.write(eepromAddress + i, value);
    delay(5); // Delay to ensure EEPROM write completes
  }

  // Re-read the log entry, 
  readLogEntry();
}

bool noLogs() {
  uint8_t currentLogEntrySeq = *static_cast<uint8_t*>(logBuffer);
  
  Serial.print("EH CHE CAZZ: ");
  Serial.println(currentLogEntrySeq);
  
  Serial.print("MILLIS: ");
  Serial.println(*reinterpret_cast<uint16_t*>(static_cast<uint8_t*>(logBuffer) + 1));


  return !currentLogEntrySeq;
}




uint8_t goToNextLogSlot(bool force = false) {
  int8_t currentSlotBeforeChange = currentSlot;

  Serial.println("goToNextLogSlot()");
  Serial.println("Current slot:");
  Serial.println(currentSlot);
  currentSlot ++;
  Serial.print("New current slot: ");
  Serial.println(currentSlot);
  
  if (currentSlot >= EEPROM_TOTAL_SLOTS) currentSlot = 0;
  Serial.print("Actual current slot: ");
  Serial.println(currentSlot);
  readLogEntry();

  uint8_t currentLogEntrySeq = *static_cast<uint8_t*>(logBuffer);
  if (!currentLogEntrySeq && !force){
    currentSlot = currentSlotBeforeChange;
    readLogEntry();
    return false;
  }
  return true;

}

uint8_t goToPreviousLogSlot(bool force = false) {
  int8_t currentSlotBeforeChange = currentSlot;

  Serial.println("goToPreviousLogSlot()");
  
  Serial.println("Current slot:");
  Serial.println(currentSlot);
  
  currentSlot --;
  Serial.print("New current slot: ");
  Serial.println(currentSlot);
 
  if (currentSlot < 0) currentSlot = EEPROM_TOTAL_SLOTS - 1;
  Serial.print("Actual current slot: ");
  Serial.println(currentSlot);
  readLogEntry();

  uint8_t currentLogEntrySeq = *static_cast<uint8_t*>(logBuffer);
  if (!currentLogEntrySeq && !force){
    currentSlot = currentSlotBeforeChange;
    readLogEntry();
    return false;
  }
  return true;
}

void initLogs(void *buffer, int eepromSize, int startAddress, int logsMemory, int slotSize) {
  logBuffer = buffer;
  EEPROM_SIZE = eepromSize;
  EEPROM_START_ADDRESS = startAddress;
  EEPROM_LOGS_MEMORY = logsMemory;
  EEPROM_SLOT_SIZE = slotSize;
  EEPROM_TOTAL_SLOTS = ((EEPROM_SIZE - EEPROM_START_ADDRESS) / EEPROM_SLOT_SIZE);


  // Will set currentSlot and currentSeq;
  goToLatestSlot();

  Serial.println("EEPROM_SIZE");
  Serial.println(EEPROM_SIZE); 
  Serial.println("EEPROM_START_ADDRESS"); 
  Serial.println(EEPROM_START_ADDRESS);
  Serial.println("EEPROM_LOGS_MEMORY");
  Serial.println(EEPROM_LOGS_MEMORY);
  Serial.println("EEPROM_SLOT_SIZE");
  Serial.println(EEPROM_SLOT_SIZE);
  Serial.println("EEPROM_TOTAL_SLOTS");
  Serial.println(EEPROM_TOTAL_SLOTS);

  
}

void clearLogEntry(void *buffer) {
  memset(buffer, 0, EEPROM_SLOT_SIZE);
}

void wipeLogs() {
  for (uint16_t i = EEPROM_START_ADDRESS; i < EEPROM_START_ADDRESS + (EEPROM_TOTAL_SLOTS * EEPROM_SLOT_SIZE); i++) {
    EEPROM.write(i, 0);
  }
  currentSlot = - 1;
  currentSeq = 0;
  clearLogEntry(logBuffer);
}

int8_t getCurrentSlot() {
  return currentSlot;
}