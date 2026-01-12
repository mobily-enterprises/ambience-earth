#include "logs.h"
#include <EEPROM.h>
#include <Arduino.h>
#include <stddef.h>
#include <Wire.h>
#include "config.h"
#include "feeding.h"
#include "messages.h"
#include "moistureSensor.h"
#include "rtc.h"

int EEPROM_SIZE;
int EEPROM_START_ADDRESS;   // start of log slots (after metadata)
int EEPROM_LOGS_MEMORY;
int EEPROM_SLOT_SIZE;
int EEPROM_TOTAL_SLOTS;
int EEPROM_META_ADDRESS;    // start of logs metadata (epoch + format version)

void* logBuffer; 
int16_t currentSlot;
static int16_t latestSlot = -1;
static uint16_t logEpoch = 0;     // Persisted across boots (EEPROM)
static uint16_t browseEpoch = 0;  // Session epoch aligned to current slot for UI
static const uint8_t kLogMetaSize = 3;
static const uint8_t kEepromPageSize = 32;
static const uint8_t kI2cChunk = 30;

extern Config config;

static void logEepromWriteByte(uint16_t address, uint8_t value) {
  Wire.beginTransmission(EXT_EEPROM_ADDR);
  Wire.write(static_cast<uint8_t>(address >> 8));
  Wire.write(static_cast<uint8_t>(address & 0xFF));
  Wire.write(value);
  Wire.endTransmission();
  delay(5);  // Allow EEPROM write cycle to complete.
}

static void logEepromReadBlock(uint16_t address, uint8_t *buffer, uint16_t len) {
  while (len) {
    uint8_t chunk = (len > kI2cChunk) ? kI2cChunk : static_cast<uint8_t>(len);
    Wire.beginTransmission(EXT_EEPROM_ADDR);
    Wire.write(static_cast<uint8_t>(address >> 8));
    Wire.write(static_cast<uint8_t>(address & 0xFF));
    Wire.endTransmission();
    Wire.requestFrom(static_cast<uint8_t>(EXT_EEPROM_ADDR), chunk);
    for (uint8_t i = 0; i < chunk; ++i) {
      buffer[i] = static_cast<uint8_t>(Wire.read());
    }
    address = static_cast<uint16_t>(address + chunk);
    buffer += chunk;
    len = static_cast<uint16_t>(len - chunk);
  }
}

static void logEepromWriteBlock(uint16_t address, const uint8_t *data, uint16_t len) {
  while (len) {
    uint8_t pageOffset = static_cast<uint8_t>(address & (kEepromPageSize - 1));
    uint8_t chunk = static_cast<uint8_t>(kEepromPageSize - pageOffset);
    if (chunk > len) chunk = static_cast<uint8_t>(len);
    if (chunk > kI2cChunk) chunk = kI2cChunk;
    Wire.beginTransmission(EXT_EEPROM_ADDR);
    Wire.write(static_cast<uint8_t>(address >> 8));
    Wire.write(static_cast<uint8_t>(address & 0xFF));
    Wire.write(data, chunk);
    Wire.endTransmission();
    delay(5);
    address = static_cast<uint16_t>(address + chunk);
    data += chunk;
    len = static_cast<uint16_t>(len - chunk);
  }
}

// Treat 16-bit sequence numbers with wrap-around semantics.
// Returns true if 'a' is later than 'b' assuming differences < 32768.
static inline bool seqIsLater(uint16_t a, uint16_t b) {
  return (int16_t)(a - b) > 0;
}

// A slot is considered empty if seq == 0 (we always avoid writing 0).
static inline bool slotIsEmpty(uint16_t seq) {
  return seq == 0;
}

static bool calcDrybackFromBaseline(uint8_t soilPercent, uint8_t *outPercent) {
  uint8_t baseline = 0;
  if (!feedingGetBaselinePercent(&baseline)) return false;
  if (outPercent) {
    *outPercent = (baseline > soilPercent) ? static_cast<uint8_t>(baseline - soilPercent) : 0;
  }
  return true;
}

static bool calcLightDayKey(uint8_t year, uint8_t month, uint8_t day,
                            uint8_t hour, uint8_t minute, uint16_t lightsOnMinutes,
                            uint16_t *outKey) {
  if (hour > 23 || minute > 59) return false;
  if (month == 0 || month > 12 || day == 0 || day > 31) return false;
  if (lightsOnMinutes > 1439) lightsOnMinutes = 0;

  uint16_t key = static_cast<uint16_t>(year) * 372u +
                 static_cast<uint16_t>(month) * 31u + day;
  uint16_t minutes = static_cast<uint16_t>(hour) * 60u + minute;
  if (minutes < lightsOnMinutes) {
    if (key == 0) return false;
    key--;
  }
  if (outKey) *outKey = key;
  return true;
}

static uint16_t readEpochFromEeprom() {
  uint8_t lo = static_cast<uint8_t>(msgReadByte(EEPROM_META_ADDRESS + 0));
  uint8_t hi = static_cast<uint8_t>(msgReadByte(EEPROM_META_ADDRESS + 1));
  // Treat 0xFFFF (erased) as 0
  if (lo == 0xFF && hi == 0xFF) return 0;
  uint16_t val = ((uint16_t)hi << 8) | lo;
  return val;
}

static uint8_t readLogVersionFromEeprom() {
  uint8_t version = static_cast<uint8_t>(msgReadByte(EEPROM_META_ADDRESS + 2));
  if (version == 0xFF) return 0;
  return version;
}

static void writeEpochToEeprom(uint16_t epoch) {
  uint8_t lo = epoch & 0xFF;
  uint8_t hi = (epoch >> 8) & 0xFF;
  logEepromWriteByte(EEPROM_META_ADDRESS + 0, lo);
  logEepromWriteByte(EEPROM_META_ADDRESS + 1, hi);
}

static void writeLogVersionToEeprom(uint8_t version) {
  logEepromWriteByte(EEPROM_META_ADDRESS + 2, version);
}

static void stampLightDayKey(LogEntry *entry) {
  if (!entry) return;

  uint8_t year = entry->startYear;
  uint8_t month = entry->startMonth;
  uint8_t day = entry->startDay;
  uint8_t hour = entry->startHour;
  uint8_t minute = entry->startMinute;

  if (entry->entryType == 1 && entry->endMonth && entry->endDay) {
    year = entry->endYear;
    month = entry->endMonth;
    day = entry->endDay;
    hour = entry->endHour;
    minute = entry->endMinute;
  }

  uint16_t key = 0;
  if (!calcLightDayKey(year, month, day, hour, minute, config.lightsOnMinutes, &key)) {
    key = 0;
  }
  entry->lightDayKey = key;
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
  logEepromReadBlock(static_cast<uint16_t>(eepromAddress),
                     static_cast<uint8_t*>(logBuffer),
                     static_cast<uint16_t>(EEPROM_SLOT_SIZE));
  
}

void goToLatestSlot () {
  if (latestSlot >= 0) {
    currentSlot = latestSlot;
    readLogEntry(currentSlot);
    browseEpoch = logEpoch;
    return;
  }

  // Find the latest slot using wrap-aware sequence comparison.
  // Ignore empty slots (seq == 0). With <32768 slots, wrap is safe.
  uint16_t bestSeq = 0;
  int16_t bestSlot = -1;

  currentSlot = -1;

  for (int slotNumber = 0; slotNumber < EEPROM_TOTAL_SLOTS; slotNumber++) {
    readLogEntry(slotNumber);
    uint16_t seq = *static_cast<uint16_t*>(logBuffer);
    if (slotIsEmpty(seq)) continue;

    if (bestSlot < 0) {
      bestSeq = seq;
      bestSlot = slotNumber;
    } else if (seqIsLater(seq, bestSeq)) {
      bestSeq = seq;
      bestSlot = slotNumber;
    }
  }

  currentSlot = bestSlot;
  if (currentSlot == -1) {
    clearLogEntry(logBuffer);
    // No logs present; align browsing epoch to persisted epoch
    browseEpoch = logEpoch;
  } else {
    readLogEntry(currentSlot);
    // Latest entry belongs to the current persisted epoch
    browseEpoch = logEpoch;
  }
  latestSlot = currentSlot;
}

int16_t getCurrentLogSlot() {
  return currentSlot;
}


void writeLogEntry(void* buffer) {

  // Serial.println("writeLogEntry()"); 
  // Serial.print("SLOT BEFORE: ");
  // Serial.println(currentSlot);
  
  // Next slot, next SEQ
  // // Serial.println("Calling goToLatestSlot() AND goToNextLogSlot()");
  goToLatestSlot();

  // Serial.print("Latest slot's seq: ");
  // Serial.println(*static_cast<uint16_t*>(logBuffer));

  uint16_t latestSeq = (*static_cast<uint16_t*>(logBuffer));
  uint16_t newSeq = latestSeq;
  newSeq++;
  // Keep 0 reserved for "empty" and bump epoch on wrap 0xFFFF->1
  if (newSeq == 0) {
    newSeq = 1;
    logEpoch++;
    writeEpochToEeprom(logEpoch);
  }
  // Serial.print("New seq: ");
  // Serial.println(newSeq);

  goToNextLogSlot(true);

  // Serial.println("DONE Calling goToLatestSlot() AND goToNextLogSlot()");

  // Serial.print("SLOT AFTER: ");
  // Serial.println(currentSlot);
  
  // Calculate the EEPROM address for the given slot number
  int eepromAddress = EEPROM_START_ADDRESS + (currentSlot) * EEPROM_SLOT_SIZE;

  // Set the current seq, ALWAYS assumed to be the first 16 bits of the buffer
  memcpy(buffer, &newSeq, sizeof(uint16_t));
  stampLightDayKey(static_cast<LogEntry*>(buffer));

  // Serial.print("SEQ IN BUFFER:");
  // Serial.print(*static_cast<uint16_t*>(buffer));
  // Serial.print(" MILLIS:");
  // Serial.println(*reinterpret_cast<uint16_t*>(static_cast<uint8_t*>(buffer) + 1));


  logEepromWriteBlock(static_cast<uint16_t>(eepromAddress),
                      static_cast<const uint8_t*>(buffer),
                      static_cast<uint16_t>(EEPROM_SLOT_SIZE));

  // Re-read the log entry, 
  readLogEntry();
  latestSlot = currentSlot;
}

bool noLogs() {
  uint16_t currentLogEntrySeq = *static_cast<uint16_t*>(logBuffer);
  
  // Serial.print("EH CHE CAZZ: ");
  // Serial.println(currentLogEntrySeq);
  
  // Serial.print("MILLIS: ");
  // Serial.println(*reinterpret_cast<uint16_t*>(static_cast<uint8_t*>(logBuffer) + 1));


  return !currentLogEntrySeq;
}




static uint8_t goToAdjacentLogSlot(int8_t direction, bool force) {
  int16_t originalSlot = currentSlot;

  // First log ever: target slot 0 for forward browsing.
  if (direction > 0 && originalSlot < 0) {
    currentSlot = 0;
    return true;
  }

  if (originalSlot < 0) originalSlot = 0; // normalise if uninitialised
  currentSlot = originalSlot;
  readLogEntry();
  uint16_t originalLogEntrySeq = *static_cast<uint16_t*>(logBuffer);

  currentSlot = static_cast<int16_t>(currentSlot + direction);
  if (currentSlot >= EEPROM_TOTAL_SLOTS) currentSlot = 0;
  if (currentSlot < 0) currentSlot = EEPROM_TOTAL_SLOTS - 1;
  readLogEntry();

  if (force) return true;

  uint16_t newLogEntrySeq = *static_cast<uint16_t*>(logBuffer);
  if (direction > 0) {
    if (!slotIsEmpty(newLogEntrySeq) && seqIsLater(newLogEntrySeq, originalLogEntrySeq)) {
      // Crossing 0xFFFF -> 1 means moving into a newer epoch during browsing
      if (originalLogEntrySeq == 0xFFFF && newLogEntrySeq == 1) browseEpoch++;
      return true;
    }
  } else {
    // Allow moving back only if the target is a valid older entry
    if (!slotIsEmpty(newLogEntrySeq) && seqIsLater(originalLogEntrySeq, newLogEntrySeq)) {
      // Crossing 1 -> 0xFFFF means moving into a previous epoch during browsing
      if (originalLogEntrySeq == 1 && newLogEntrySeq == 0xFFFF) browseEpoch--;
      return true;
    }
  }

  currentSlot = originalSlot;
  readLogEntry();
  return false;
}

uint8_t goToNextLogSlot(bool force = false) {
  return goToAdjacentLogSlot(1, force);
}

uint8_t goToPreviousLogSlot(bool force = false) {
  return goToAdjacentLogSlot(-1, force);
}

void initLogs(void *buffer, int eepromSize, int startAddress, int logsMemory, int slotSize) {
  logBuffer = buffer;
  EEPROM_SIZE = eepromSize;
  EEPROM_META_ADDRESS = startAddress;               // reserve bytes for epoch + format version
  EEPROM_START_ADDRESS = startAddress + kLogMetaSize;    // actual start of log slots
  EEPROM_LOGS_MEMORY = logsMemory;
  EEPROM_SLOT_SIZE = slotSize;
  if (EEPROM_LOGS_MEMORY > 0) {
    EEPROM_TOTAL_SLOTS = ((EEPROM_LOGS_MEMORY - kLogMetaSize) / EEPROM_SLOT_SIZE);
  } else {
    EEPROM_TOTAL_SLOTS = ((EEPROM_SIZE - EEPROM_START_ADDRESS) / EEPROM_SLOT_SIZE);
  }

  uint8_t storedVersion = readLogVersionFromEeprom();
  if (storedVersion != LOG_FORMAT_VERSION) {
    wipeLogs();
  }

  // Load persisted epoch
  logEpoch = readEpochFromEeprom();
  browseEpoch = logEpoch;
  latestSlot = -1;


  // Will set currentSlot
  goToLatestSlot();

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
  if (!buffer) return;
  static_cast<LogEntry*>(buffer)->baselinePercent = LOG_BASELINE_UNSET;
  static_cast<LogEntry*>(buffer)->drybackPercent = LOG_BASELINE_UNSET;
}

void initLogEntryCommon(LogEntry *entry, uint8_t entryType, uint8_t stopReason,
                        uint8_t startReason, uint8_t slotIndex, uint8_t flags,
                        uint8_t soilBefore, uint8_t soilAfter) {
  if (!entry) return;
  clearLogEntry(entry);
  entry->entryType = entryType;
  entry->stopReason = stopReason;
  entry->startReason = startReason;
  entry->slotIndex = slotIndex;
  entry->flags = flags;
  entry->soilMoistureBefore = soilBefore;
  entry->soilMoistureAfter = soilAfter;
  entry->drybackPercent = LOG_BASELINE_UNSET;
  uint8_t dryback = 0;
  if (calcDrybackFromBaseline(soilBefore, &dryback)) entry->drybackPercent = dryback;
}

void wipeLogs() {
  // Clear slots
  memset(logBuffer, 0, EEPROM_SLOT_SIZE);
  uint16_t addr = static_cast<uint16_t>(EEPROM_START_ADDRESS);
  for (int slot = 0; slot < EEPROM_TOTAL_SLOTS; ++slot) {
    logEepromWriteBlock(addr, static_cast<uint8_t*>(logBuffer), static_cast<uint16_t>(EEPROM_SLOT_SIZE));
    addr = static_cast<uint16_t>(addr + EEPROM_SLOT_SIZE);
  }
  // Reset epoch
  logEpoch = 0;
  browseEpoch = 0;
  writeEpochToEeprom(logEpoch);
  writeLogVersionToEeprom(LOG_FORMAT_VERSION);
  currentSlot = -1;
  latestSlot = -1;
  clearLogEntry(logBuffer);
}

bool patchLogBaselinePercent(int16_t slot, uint8_t baselinePercent) {
  if (slot < 0 || slot >= EEPROM_TOTAL_SLOTS) return false;
  int eepromAddress = EEPROM_START_ADDRESS + (slot * EEPROM_SLOT_SIZE)
                      + static_cast<int>(offsetof(LogEntry, baselinePercent));
  logEepromWriteByte(eepromAddress, baselinePercent);
  return true;
}

bool findLatestBaselineEntries(LogEntry *outLatestSetter, int16_t *outLatestSetterSlot,
                               LogEntry *outLatestWithBaseline, int16_t *outLatestWithBaselineSlot,
                               LogEntry *outLatestFeed) {
  int16_t savedSlot = currentSlot;
  uint16_t savedBrowseEpoch = browseEpoch;
  bool foundSetter = false;
  bool foundWithBaseline = false;
  int16_t foundSetterSlot = -1;
  int16_t foundBaselineSlot = -1;

  goToLatestSlot();
  if (currentSlot >= 0) {
    do {
      LogEntry *entry = static_cast<LogEntry*>(logBuffer);
      if (entry->entryType != 1) continue;
      if (outLatestFeed && outLatestFeed->entryType == 0) {
        *outLatestFeed = *entry;
      }
      if ((entry->flags & LOG_FLAG_BASELINE_SETTER) == 0) continue;
      if ((entry->flags & LOG_FLAG_RUNOFF_SEEN) == 0) continue;

      if (!foundSetter) {
        foundSetter = true;
        foundSetterSlot = currentSlot;
        if (outLatestSetter) *outLatestSetter = *entry;
      }
      if (!foundWithBaseline && entry->baselinePercent != LOG_BASELINE_UNSET) {
        foundWithBaseline = true;
        foundBaselineSlot = currentSlot;
        if (outLatestWithBaseline) *outLatestWithBaseline = *entry;
      }
      if (foundSetter && foundWithBaseline) break;
    } while (goToPreviousLogSlot());
  }

  browseEpoch = savedBrowseEpoch;
  if (savedSlot >= 0) {
    currentSlot = savedSlot;
    readLogEntry();
  } else {
    currentSlot = savedSlot;
  }

  if (outLatestSetterSlot) *outLatestSetterSlot = foundSetter ? foundSetterSlot : -1;
  if (outLatestWithBaselineSlot) *outLatestWithBaselineSlot = foundWithBaseline ? foundBaselineSlot : -1;
  return foundSetter || foundWithBaseline;
}

uint32_t getAbsoluteLogNumber() {
  uint16_t seq = *static_cast<uint16_t*>(logBuffer);
  return ((uint32_t)browseEpoch << 16) | seq;
}

uint16_t getDailyFeedTotalMlAt(uint8_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute,
                               uint8_t *outMin, uint8_t *outMax) {
  uint16_t targetKey = 0;
  if (!calcLightDayKey(year, month, day, hour, minute, config.lightsOnMinutes, &targetKey)) return 0;

  int16_t savedSlot = currentSlot;
  uint16_t savedBrowseEpoch = browseEpoch;
  uint16_t total = 0;
  bool foundTotal = false;
  uint8_t minVal = LOG_BASELINE_UNSET;
  uint8_t maxVal = LOG_BASELINE_UNSET;

  goToLatestSlot();
  if (currentSlot >= 0) {
    do {
      LogEntry *entry = static_cast<LogEntry*>(logBuffer);
      uint16_t entryKey = entry->lightDayKey;
      if (entryKey && entryKey < targetKey) break;
      if (entry->entryType == 1 && entryKey == targetKey) {
        if (!foundTotal) {
          total = entry->dailyTotalMl;
          foundTotal = true;
        }
      } else if (entry->entryType == 2 && entryKey == targetKey) {
        uint8_t val = entry->soilMoistureBefore;
        if (minVal == LOG_BASELINE_UNSET) {
          minVal = val;
          maxVal = val;
        } else {
          if (val < minVal) minVal = val;
          if (val > maxVal) maxVal = val;
        }
      }
    } while (goToPreviousLogSlot());
  }

  browseEpoch = savedBrowseEpoch;
  if (savedSlot >= 0) {
    currentSlot = savedSlot;
    readLogEntry();
  } else {
    currentSlot = savedSlot;
  }

  if (outMin) *outMin = minVal;
  if (outMax) *outMax = maxVal;

  return total;
}

uint16_t getDailyFeedTotalMlNow(uint8_t *outMin, uint8_t *outMax) {
  uint8_t hour = 0;
  uint8_t minute = 0;
  uint8_t day = 0;
  uint8_t month = 0;
  uint8_t year = 0;

  if (!rtcReadDateTime(&hour, &minute, &day, &month, &year)) return 0;
  return getDailyFeedTotalMlAt(year, month, day, hour, minute, outMin, outMax);
}

uint32_t getLightDayKeyAt(uint8_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute) {
  uint16_t key = 0;
  if (!calcLightDayKey(year, month, day, hour, minute, config.lightsOnMinutes, &key)) return 0xFFFFFFFFUL;
  return key;
}

uint32_t getLightDayKeyNow() {
  uint8_t hour = 0;
  uint8_t minute = 0;
  uint8_t day = 0;
  uint8_t month = 0;
  uint8_t year = 0;

  if (!rtcReadDateTime(&hour, &minute, &day, &month, &year)) return 0xFFFFFFFFUL;
  return getLightDayKeyAt(year, month, day, hour, minute);
}

bool getDrybackPercent(uint8_t *outPercent) {
  if (!outPercent) return false;
  if (!soilSensorReady()) return false;
  uint8_t soilPercent = soilMoistureAsPercentage(getSoilMoisture());
  return calcDrybackFromBaseline(soilPercent, outPercent);
}
