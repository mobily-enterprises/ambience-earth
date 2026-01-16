#pragma once

#include "app_state.h"

#define LOG_FORMAT_VERSION 9

enum LogStopReason : uint8_t {
  LOG_STOP_NONE = 0,
  LOG_STOP_MOISTURE = 1,
  LOG_STOP_RUNOFF = 2,
  LOG_STOP_MAX_RUNTIME = 3,
  LOG_STOP_DISABLED = 4,
  LOG_STOP_UI_PAUSE = 5,
  LOG_STOP_MAX_DAILY_FEED_REACHED = 6,
  LOG_STOP_FEED_NOT_CALIBRATED = 7
};

enum LogStartReason : uint8_t {
  LOG_START_NONE = 0,
  LOG_START_TIME = 1,
  LOG_START_MOISTURE = 2,
  LOG_START_USER = 3
};

#define LOG_FLAG_RUNOFF_MISSING 0x01
#define LOG_FLAG_RUNOFF_UNEXPECTED 0x02
#define LOG_FLAG_RUNOFF_ANY (LOG_FLAG_RUNOFF_MISSING | LOG_FLAG_RUNOFF_UNEXPECTED)
#define LOG_FLAG_BASELINE_SETTER 0x04
#define LOG_FLAG_RUNOFF_SEEN 0x08
#define LOG_BASELINE_UNSET 0xFF

/*
 * logs_init
 * Initializes flash-backed log storage and loads recent logs into RAM.
 * Example:
 *   logs_init();
 */
void logs_init();

/*
 * logs_wipe
 * Clears all persisted logs and resets in-memory log state.
 * Example:
 *   logs_wipe();
 */
void logs_wipe();

/*
 * add_log
 * Adds a log entry to RAM and persists it to flash storage.
 * Example:
 *   add_log(build_boot_log());
 */
void add_log(const LogEntry &entry);

/*
 * build_value_log
 * Builds a type-2 values log from the current sensor state.
 * Example:
 *   LogEntry entry = build_value_log();
 */
LogEntry build_value_log();

/*
 * build_boot_log
 * Builds a type-0 boot/marker log entry.
 * Example:
 *   LogEntry entry = build_boot_log();
 */
LogEntry build_boot_log();

/*
 * build_feed_log
 * Builds a type-1 feed log from the current feeding state.
 * Example:
 *   LogEntry entry = build_feed_log();
 */
LogEntry build_feed_log();

/*
 * readLogEntry
 * Reads a log entry into the internal log buffer.
 * Example:
 *   readLogEntry();
 */
void readLogEntry(int16_t slot = -1);

/*
 * writeLogEntry
 * Persists a log entry and updates the head ring.
 * Example:
 *   writeLogEntry(&entry);
 */
void writeLogEntry(void *buffer);

/*
 * noLogs
 * Returns true if the log storage is empty.
 * Example:
 *   if (noLogs()) { ... }
 */
bool noLogs();

/*
 * goToNextLogSlot
 * Advances the current slot pointer to the next entry.
 * Example:
 *   goToNextLogSlot();
 */
uint8_t goToNextLogSlot(bool force = false);

/*
 * goToPreviousLogSlot
 * Moves the current slot pointer to the previous entry.
 * Example:
 *   goToPreviousLogSlot();
 */
uint8_t goToPreviousLogSlot(bool force = false);

/*
 * initLogs
 * Compatibility wrapper that initializes log storage.
 * Example:
 *   initLogs(nullptr, 0, 0, 0, 0);
 */
void initLogs(void *buffer, int eepromSize, int startAddress, int logsMemory, int slotSize);

/*
 * getCurrentLogSlot
 * Returns the current log slot index.
 * Example:
 *   int16_t slot = getCurrentLogSlot();
 */
int16_t getCurrentLogSlot();

/*
 * goToLatestSlot
 * Positions the current slot pointer at the newest log.
 * Example:
 *   goToLatestSlot();
 */
void goToLatestSlot();

/*
 * clearLogEntry
 * Zeros a log entry buffer and resets baseline fields.
 * Example:
 *   clearLogEntry(&entry);
 */
void clearLogEntry(void *buffer);

/*
 * initLogEntryCommon
 * Initializes a log entry with common fields.
 * Example:
 *   initLogEntryCommon(&entry, 2, LOG_STOP_NONE, LOG_START_NONE, 0, 0, soil, 0);
 */
void initLogEntryCommon(LogEntry *entry, uint8_t entryType, uint8_t stopReason,
                        uint8_t startReason, uint8_t slotIndex, uint8_t flags,
                        uint8_t soilBefore, uint8_t soilAfter);

/*
 * wipeLogs
 * Clears all log storage data and resets metadata.
 * Example:
 *   wipeLogs();
 */
void wipeLogs();

/*
 * patchLogBaselinePercent
 * Writes a baseline percent into an existing log slot.
 * Example:
 *   patchLogBaselinePercent(slot, baseline);
 */
bool patchLogBaselinePercent(int16_t slot, uint8_t baselinePercent);

/*
 * findLatestBaselineEntries
 * Finds recent baseline setter feeds for baseline tracking.
 * Example:
 *   findLatestBaselineEntries(&setter, &setterSlot, &withBaseline, &baselineSlot, &latestFeed);
 */
bool findLatestBaselineEntries(LogEntry *outLatestSetter, int16_t *outLatestSetterSlot,
                               LogEntry *outLatestWithBaseline, int16_t *outLatestWithBaselineSlot,
                               LogEntry *outLatestFeed);

/*
 * getAbsoluteLogNumber
 * Returns the absolute log sequence using epoch + seq.
 * Example:
 *   uint32_t num = getAbsoluteLogNumber();
 */
uint32_t getAbsoluteLogNumber();

/*
 * getDailyFeedTotalMlAt
 * Calculates the daily total at a specified date/time.
 * Example:
 *   uint16_t total = getDailyFeedTotalMlAt(y, m, d, h, min);
 */
uint16_t getDailyFeedTotalMlAt(uint8_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute,
                               uint8_t *outMin = nullptr, uint8_t *outMax = nullptr);

/*
 * getDailyFeedTotalMlNow
 * Calculates the daily total at the current simulated time.
 * Example:
 *   uint16_t total = getDailyFeedTotalMlNow();
 */
uint16_t getDailyFeedTotalMlNow(uint8_t *outMin = nullptr, uint8_t *outMax = nullptr);

/*
 * getDrybackPercent
 * Calculates dryback based on baseline and current moisture.
 * Example:
 *   uint8_t db = 0;
 *   if (getDrybackPercent(&db)) { ... }
 */
bool getDrybackPercent(uint8_t *outPercent);
