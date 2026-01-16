#include "logs.h"

#include <Arduino.h>
#include <LittleFS.h>
#include <stddef.h>
#include <string.h>

#include "config.h"
#include "feeding.h"
#include "moistureSensor.h"
#include "rtc.h"

namespace {
constexpr uint8_t kLogMetaSize = 3;
constexpr uint8_t kHeadRecordSize = 4;
constexpr uint16_t kHeadRingDivisor = 20;
const char *kLogFilePath = "/logs.bin";

static bool g_logs_ready = false;
static uint16_t g_total_slots = 0;
static uint16_t g_head_record_count = 0;
static uint32_t g_log_start_offset = 0;
static int16_t g_current_slot = -1;
static int16_t g_latest_slot = -1;
static int16_t g_head_index = -1;
static uint16_t g_log_epoch = 0;
static uint16_t g_browse_epoch = 0;
static LogEntry g_log_buffer = {};

/*
 * ensure_fs
 * Mounts LittleFS so logs can be read/written.
 * Example:
 *   if (!ensure_fs()) return;
 */
static bool ensure_fs() {
  if (g_logs_ready) return true;
  if (LittleFS.begin()) return true;
  return LittleFS.begin(true);
}

/*
 * ensure_log_file_size
 * Creates or resizes the log file to the expected size.
 * Example:
 *   ensure_log_file_size(kLogStoreBytes);
 */
static bool ensure_log_file_size(size_t size) {
  File f = LittleFS.open(kLogFilePath, "r");
  if (!f) {
    File nf = LittleFS.open(kLogFilePath, "w+");
    if (!nf) return false;
    if (size > 0) {
      nf.seek(size - 1);
      nf.write(static_cast<uint8_t>(0));
    }
    nf.close();
    return true;
  }
  size_t existing = f.size();
  f.close();
  if (existing == size) return true;
  LittleFS.remove(kLogFilePath);
  File nf = LittleFS.open(kLogFilePath, "w+");
  if (!nf) return false;
  if (size > 0) {
    nf.seek(size - 1);
    nf.write(static_cast<uint8_t>(0));
  }
  nf.close();
  return true;
}

/*
 * read_block
 * Reads a byte range from the log file.
 * Example:
 *   read_block(offset, buffer, sizeof(buffer));
 */
static bool read_block(uint32_t offset, void *buffer, size_t len) {
  File f = LittleFS.open(kLogFilePath, "r");
  if (!f) return false;
  if (!f.seek(offset)) {
    f.close();
    return false;
  }
  size_t read_len = f.read(static_cast<uint8_t *>(buffer), len);
  f.close();
  return read_len == len;
}

/*
 * write_block
 * Writes a byte range into the log file.
 * Example:
 *   write_block(offset, data, sizeof(data));
 */
static bool write_block(uint32_t offset, const void *data, size_t len) {
  File f = LittleFS.open(kLogFilePath, "r+");
  if (!f) return false;
  if (!f.seek(offset)) {
    f.close();
    return false;
  }
  size_t written = f.write(static_cast<const uint8_t *>(data), len);
  f.flush();
  f.close();
  return written == len;
}

/*
 * slot_offset
 * Returns the byte offset for a given log slot.
 * Example:
 *   uint32_t offset = slot_offset(3);
 */
static uint32_t slot_offset(int16_t slot) {
  return g_log_start_offset + static_cast<uint32_t>(slot) * sizeof(LogEntry);
}

/*
 * read_slot
 * Reads a log slot into the provided buffer.
 * Example:
 *   LogEntry entry = {};
 *   read_slot(0, &entry);
 */
static bool read_slot(int16_t slot, LogEntry *out) {
  if (!out || slot < 0 || slot >= static_cast<int16_t>(g_total_slots)) return false;
  if (!read_block(slot_offset(slot), out, sizeof(LogEntry))) {
    memset(out, 0, sizeof(LogEntry));
    return false;
  }
  return true;
}

/*
 * write_slot
 * Writes a log entry into the specified slot.
 * Example:
 *   write_slot(0, entry);
 */
static bool write_slot(int16_t slot, const LogEntry &entry) {
  if (slot < 0 || slot >= static_cast<int16_t>(g_total_slots)) return false;
  return write_block(slot_offset(slot), &entry, sizeof(LogEntry));
}

/*
 * seqIsLater
 * Compares 16-bit sequences with wrap-around semantics.
 * Example:
 *   if (seqIsLater(new_seq, old_seq)) { ... }
 */
static inline bool seqIsLater(uint16_t a, uint16_t b) {
  return static_cast<int16_t>(a - b) > 0;
}

/*
 * slotIsEmpty
 * Returns true if a log slot is empty.
 * Example:
 *   if (slotIsEmpty(entry.seq)) { ... }
 */
static inline bool slotIsEmpty(uint16_t seq) {
  return seq == 0;
}

/*
 * calcDrybackFromBaseline
 * Calculates dryback percent from baseline and soil percent.
 * Example:
 *   uint8_t db = 0;
 *   calcDrybackFromBaseline(soilPct, &db);
 */
static bool calcDrybackFromBaseline(uint8_t soilPercent, uint8_t *outPercent) {
  uint8_t baseline = 0;
  if (!feedingGetBaselinePercent(&baseline)) return false;
  if (outPercent) {
    *outPercent = (baseline > soilPercent) ? static_cast<uint8_t>(baseline - soilPercent) : 0;
  }
  return true;
}

/*
 * calcLightDayKey
 * Creates a day key aligned to lights-on time.
 * Example:
 *   uint16_t key = 0;
 *   calcLightDayKey(y, m, d, h, min, lightsOnMinutes, &key);
 */
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

/*
 * readEpochFromFlash
 * Loads the persisted epoch counter.
 * Example:
 *   uint16_t epoch = readEpochFromFlash();
 */
static uint16_t readEpochFromFlash() {
  uint8_t data[2] = {0};
  if (!read_block(0, data, sizeof(data))) return 0;
  if (data[0] == 0xFF && data[1] == 0xFF) return 0;
  return static_cast<uint16_t>(data[0]) | (static_cast<uint16_t>(data[1]) << 8);
}

/*
 * writeEpochToFlash
 * Stores the epoch counter in log metadata.
 * Example:
 *   writeEpochToFlash(g_log_epoch);
 */
static void writeEpochToFlash(uint16_t epoch) {
  uint8_t data[2];
  data[0] = static_cast<uint8_t>(epoch & 0xFF);
  data[1] = static_cast<uint8_t>((epoch >> 8) & 0xFF);
  write_block(0, data, sizeof(data));
}

/*
 * readVersionFromFlash
 * Reads the stored log format version.
 * Example:
 *   uint8_t ver = readVersionFromFlash();
 */
static uint8_t readVersionFromFlash() {
  uint8_t version = 0;
  if (!read_block(2, &version, 1)) return 0;
  if (version == 0xFF) return 0;
  return version;
}

/*
 * writeVersionToFlash
 * Writes the log format version.
 * Example:
 *   writeVersionToFlash(LOG_FORMAT_VERSION);
 */
static void writeVersionToFlash(uint8_t version) {
  write_block(2, &version, 1);
}

/*
 * writeHeadRecord
 * Appends a (seq, slot) record to the head ring.
 * Example:
 *   writeHeadRecord(seq, slot);
 */
static void writeHeadRecord(uint16_t seq, uint16_t slot) {
  if (g_head_record_count == 0) return;
  if (g_head_index < 0 || g_head_index + 1 >= static_cast<int16_t>(g_head_record_count)) g_head_index = 0;
  else g_head_index++;
  uint32_t addr = kLogMetaSize + static_cast<uint32_t>(g_head_index) * kHeadRecordSize;
  uint8_t data[4];
  data[0] = static_cast<uint8_t>(seq & 0xFF);
  data[1] = static_cast<uint8_t>(seq >> 8);
  data[2] = static_cast<uint8_t>(slot & 0xFF);
  data[3] = static_cast<uint8_t>(slot >> 8);
  write_block(addr, data, sizeof(data));
}

/*
 * stampLightDayKey
 * Updates the lightDayKey on a log entry.
 * Example:
 *   stampLightDayKey(&entry);
 */
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

/*
 * clear_ram_logs
 * Clears the in-memory log list used by the UI.
 * Example:
 *   clear_ram_logs();
 */
static void clear_ram_logs() {
  memset(g_logs, 0, sizeof(g_logs));
  g_log_count = 0;
  g_log_index = 0;
}

/*
 * cache_log_entry
 * Adds a log entry to the in-memory list.
 * Example:
 *   cache_log_entry(entry);
 */
static void cache_log_entry(const LogEntry &entry) {
  if (g_log_count < kMaxLogs) {
    for (int i = g_log_count; i > 0; --i) {
      g_logs[i] = g_logs[i - 1];
    }
    g_logs[0] = entry;
    g_log_count++;
  } else if (kMaxLogs > 0) {
    for (int i = kMaxLogs - 1; i > 0; --i) {
      g_logs[i] = g_logs[i - 1];
    }
    g_logs[0] = entry;
  }
  g_log_index = 0;
}

/*
 * refresh_ram_logs
 * Rebuilds the in-memory log list from persisted storage.
 * Example:
 *   refresh_ram_logs();
 */
static void refresh_ram_logs() {
  clear_ram_logs();
  int16_t savedSlot = g_current_slot;
  uint16_t savedEpoch = g_browse_epoch;

  goToLatestSlot();
  if (g_current_slot >= 0) {
    do {
      if (g_log_count >= kMaxLogs) break;
      g_logs[g_log_count++] = g_log_buffer;
    } while (goToPreviousLogSlot());
  }

  g_browse_epoch = savedEpoch;
  if (savedSlot >= 0) {
    g_current_slot = savedSlot;
    readLogEntry();
  } else {
    g_current_slot = savedSlot;
  }
  g_log_index = 0;
}
} // namespace

void logs_init() {
  if (!ensure_fs()) return;
  if (!ensure_log_file_size(kLogStoreBytes)) return;

  size_t logArea = kLogStoreBytes;
  size_t ringBytes = (logArea > kLogMetaSize) ? ((logArea - kLogMetaSize) / kHeadRingDivisor) : 0;
  ringBytes &= ~static_cast<size_t>(3);
  if (ringBytes < kHeadRecordSize) ringBytes = kHeadRecordSize;

  g_head_record_count = static_cast<uint16_t>(ringBytes / kHeadRecordSize);
  g_log_start_offset = static_cast<uint32_t>(kLogMetaSize + ringBytes);
  if (logArea > kLogMetaSize + ringBytes) {
    g_total_slots = static_cast<uint16_t>((logArea - kLogMetaSize - ringBytes) / sizeof(LogEntry));
  } else {
    g_total_slots = 0;
  }

  uint8_t storedVersion = readVersionFromFlash();
  if (storedVersion != LOG_FORMAT_VERSION) {
    wipeLogs();
  }

  g_log_epoch = readEpochFromFlash();
  g_browse_epoch = g_log_epoch;
  g_latest_slot = -1;
  g_head_index = -1;

  uint16_t bestSeq = 0;
  for (uint16_t i = 0; i < g_head_record_count; ++i) {
    uint8_t data[4];
    uint32_t addr = kLogMetaSize + static_cast<uint32_t>(i) * kHeadRecordSize;
    if (!read_block(addr, data, sizeof(data))) continue;
    uint16_t seq = static_cast<uint16_t>(data[0]) | (static_cast<uint16_t>(data[1]) << 8);
    uint16_t slot = static_cast<uint16_t>(data[2]) | (static_cast<uint16_t>(data[3]) << 8);
    if (seq == 0 || slot >= g_total_slots) continue;
    if (g_head_index < 0 || seqIsLater(seq, bestSeq)) {
      bestSeq = seq;
      g_latest_slot = static_cast<int16_t>(slot);
      g_head_index = static_cast<int16_t>(i);
    }
  }

  if (g_latest_slot >= 0) {
    g_current_slot = g_latest_slot;
    readLogEntry(g_current_slot);
  } else {
    g_current_slot = -1;
    clearLogEntry(&g_log_buffer);
  }
  g_browse_epoch = g_log_epoch;
  g_logs_ready = true;

  refresh_ram_logs();
}

void logs_wipe() {
  wipeLogs();
  refresh_ram_logs();
}

void add_log(const LogEntry &entry) {
  LogEntry copy = entry;
  writeLogEntry(static_cast<void *>(&copy));
}

LogEntry build_value_log() {
  LogEntry entry = {};
  uint8_t soilMoisture = soilMoistureAsPercentage(getSoilMoisture());
  initLogEntryCommon(&entry, 2, LOG_STOP_NONE, LOG_START_NONE, 0, 0, soilMoisture, 0);
  entry.millisStart = millis();
  rtcReadDateTime(&entry.startHour, &entry.startMinute, &entry.startDay,
                  &entry.startMonth, &entry.startYear);
  return entry;
}

LogEntry build_boot_log() {
  LogEntry entry = {};
  uint8_t soilMoistureBefore = soilMoistureAsPercentage(getSoilMoisture());
  initLogEntryCommon(&entry, 0, LOG_STOP_NONE, LOG_START_NONE, 0, 0, soilMoistureBefore, 0);
  entry.millisStart = 0;
  entry.millisEnd = millis();
  rtcReadDateTime(&entry.startHour, &entry.startMinute, &entry.startDay,
                  &entry.startMonth, &entry.startYear);
  return entry;
}

LogEntry build_feed_log() {
  LogEntry entry = {};
  uint8_t soilMoisture = soilMoistureAsPercentage(getSoilMoisture());
  initLogEntryCommon(&entry, 1, LOG_STOP_NONE, LOG_START_USER, 0, 0, soilMoisture, soilMoisture);
  entry.millisStart = millis();
  rtcReadDateTime(&entry.startHour, &entry.startMinute, &entry.startDay,
                  &entry.startMonth, &entry.startYear);
  return entry;
}

void readLogEntry(int16_t slot) {
  int16_t slotToRead = (slot == -1) ? g_current_slot : slot;
  if (slotToRead < 0) {
    clearLogEntry(&g_log_buffer);
    return;
  }
  if (!read_slot(slotToRead, &g_log_buffer)) {
    clearLogEntry(&g_log_buffer);
  }
}

void goToLatestSlot() {
  if (g_latest_slot >= 0) {
    g_current_slot = g_latest_slot;
    readLogEntry(g_current_slot);
    g_browse_epoch = g_log_epoch;
    return;
  }
  g_current_slot = -1;
  clearLogEntry(&g_log_buffer);
  g_browse_epoch = g_log_epoch;
}

int16_t getCurrentLogSlot() {
  return g_current_slot;
}

void writeLogEntry(void *buffer) {
  if (!buffer) return;

  goToLatestSlot();
  uint16_t latestSeq = g_log_buffer.seq;
  uint16_t newSeq = static_cast<uint16_t>(latestSeq + 1);
  if (newSeq == 0) {
    newSeq = 1;
    g_log_epoch++;
    writeEpochToFlash(g_log_epoch);
  }

  goToNextLogSlot(true);
  LogEntry *entry = static_cast<LogEntry *>(buffer);
  entry->seq = newSeq;
  stampLightDayKey(entry);
  write_slot(g_current_slot, *entry);

  readLogEntry();
  writeHeadRecord(newSeq, static_cast<uint16_t>(g_current_slot));
  g_latest_slot = g_current_slot;

  cache_log_entry(*entry);
}

bool noLogs() {
  return g_log_buffer.seq == 0;
}

/*
 * goToAdjacentLogSlot
 * Moves the current slot pointer forward/backward with epoch tracking.
 * Example:
 *   goToAdjacentLogSlot(1, false);
 */
static uint8_t goToAdjacentLogSlot(int8_t direction, bool force) {
  int16_t originalSlot = g_current_slot;

  if (direction > 0 && originalSlot < 0) {
    g_current_slot = 0;
    return true;
  }

  if (originalSlot < 0) originalSlot = 0;
  g_current_slot = originalSlot;
  readLogEntry();
  uint16_t originalSeq = g_log_buffer.seq;

  g_current_slot = static_cast<int16_t>(g_current_slot + direction);
  if (g_current_slot >= static_cast<int16_t>(g_total_slots)) g_current_slot = 0;
  if (g_current_slot < 0) g_current_slot = static_cast<int16_t>(g_total_slots - 1);
  readLogEntry();

  if (force) return true;

  uint16_t newSeq = g_log_buffer.seq;
  if (direction > 0) {
    if (!slotIsEmpty(newSeq) && seqIsLater(newSeq, originalSeq)) {
      if (originalSeq == 0xFFFF && newSeq == 1) g_browse_epoch++;
      return true;
    }
  } else {
    if (!slotIsEmpty(newSeq) && seqIsLater(originalSeq, newSeq)) {
      if (originalSeq == 1 && newSeq == 0xFFFF) g_browse_epoch--;
      return true;
    }
  }

  g_current_slot = originalSlot;
  readLogEntry();
  return false;
}

uint8_t goToNextLogSlot(bool force) {
  return goToAdjacentLogSlot(1, force);
}

uint8_t goToPreviousLogSlot(bool force) {
  return goToAdjacentLogSlot(-1, force);
}

void initLogs(void *, int, int, int, int) {
  logs_init();
}

void clearLogEntry(void *buffer) {
  if (!buffer) return;
  memset(buffer, 0, sizeof(LogEntry));
  LogEntry *entry = static_cast<LogEntry *>(buffer);
  entry->baselinePercent = LOG_BASELINE_UNSET;
  entry->drybackPercent = LOG_BASELINE_UNSET;
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
  if (g_head_record_count > 0) {
    uint8_t blank[kHeadRecordSize];
    memset(blank, 0xFF, sizeof(blank));
    for (uint16_t i = 0; i < g_head_record_count; ++i) {
      uint32_t addr = kLogMetaSize + static_cast<uint32_t>(i) * kHeadRecordSize;
      write_block(addr, blank, sizeof(blank));
    }
  }

  LogEntry blankEntry = {};
  for (uint16_t slot = 0; slot < g_total_slots; ++slot) {
    write_slot(static_cast<int16_t>(slot), blankEntry);
  }

  g_log_epoch = 0;
  g_browse_epoch = 0;
  writeEpochToFlash(g_log_epoch);
  writeVersionToFlash(LOG_FORMAT_VERSION);
  g_current_slot = -1;
  g_latest_slot = -1;
  g_head_index = -1;
  clearLogEntry(&g_log_buffer);
}

bool patchLogBaselinePercent(int16_t slot, uint8_t baselinePercent) {
  if (slot < 0 || slot >= static_cast<int16_t>(g_total_slots)) return false;
  uint32_t offset = slot_offset(slot) + static_cast<uint32_t>(offsetof(LogEntry, baselinePercent));
  return write_block(offset, &baselinePercent, sizeof(baselinePercent));
}

bool findLatestBaselineEntries(LogEntry *outLatestSetter, int16_t *outLatestSetterSlot,
                               LogEntry *outLatestWithBaseline, int16_t *outLatestWithBaselineSlot,
                               LogEntry *outLatestFeed) {
  int16_t savedSlot = g_current_slot;
  uint16_t savedBrowseEpoch = g_browse_epoch;
  bool foundSetter = false;
  bool foundWithBaseline = false;
  int16_t foundSetterSlot = -1;
  int16_t foundBaselineSlot = -1;

  goToLatestSlot();
  if (g_current_slot >= 0) {
    do {
      LogEntry *entry = &g_log_buffer;
      if (entry->entryType != 1) continue;
      if (outLatestFeed && outLatestFeed->entryType == 0) {
        *outLatestFeed = *entry;
      }
      if ((entry->flags & LOG_FLAG_BASELINE_SETTER) == 0) continue;
      if ((entry->flags & LOG_FLAG_RUNOFF_SEEN) == 0) continue;

      if (!foundSetter) {
        foundSetter = true;
        foundSetterSlot = g_current_slot;
        if (outLatestSetter) *outLatestSetter = *entry;
      }
      if (!foundWithBaseline && entry->baselinePercent != LOG_BASELINE_UNSET) {
        foundWithBaseline = true;
        foundBaselineSlot = g_current_slot;
        if (outLatestWithBaseline) *outLatestWithBaseline = *entry;
      }
      if (foundSetter && foundWithBaseline) break;
    } while (goToPreviousLogSlot());
  }

  g_browse_epoch = savedBrowseEpoch;
  if (savedSlot >= 0) {
    g_current_slot = savedSlot;
    readLogEntry();
  } else {
    g_current_slot = savedSlot;
  }

  if (outLatestSetterSlot) *outLatestSetterSlot = foundSetter ? foundSetterSlot : -1;
  if (outLatestWithBaselineSlot) *outLatestWithBaselineSlot = foundWithBaseline ? foundBaselineSlot : -1;
  return foundSetter || foundWithBaseline;
}

uint32_t getAbsoluteLogNumber() {
  return (static_cast<uint32_t>(g_browse_epoch) << 16) | g_log_buffer.seq;
}

uint16_t getDailyFeedTotalMlAt(uint8_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute,
                               uint8_t *outMin, uint8_t *outMax) {
  uint16_t targetKey = 0;
  if (!calcLightDayKey(year, month, day, hour, minute, config.lightsOnMinutes, &targetKey)) return 0;

  int16_t savedSlot = g_current_slot;
  uint16_t savedBrowseEpoch = g_browse_epoch;
  uint16_t total = 0;
  bool foundTotal = false;
  uint8_t minVal = LOG_BASELINE_UNSET;
  uint8_t maxVal = LOG_BASELINE_UNSET;

  goToLatestSlot();
  if (g_current_slot >= 0) {
    do {
      LogEntry *entry = &g_log_buffer;
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

  g_browse_epoch = savedBrowseEpoch;
  if (savedSlot >= 0) {
    g_current_slot = savedSlot;
    readLogEntry();
  } else {
    g_current_slot = savedSlot;
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

bool getDrybackPercent(uint8_t *outPercent) {
  if (!outPercent) return false;
  if (!soilSensorReady()) return false;
  uint8_t soilPercent = soilMoistureAsPercentage(getSoilMoisture());
  return calcDrybackFromBaseline(soilPercent, outPercent);
}
