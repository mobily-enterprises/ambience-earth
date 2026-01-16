#include "logs.h"

#include "app_utils.h"
#include <LittleFS.h>
#include <string.h>

namespace {

constexpr uint8_t kLogFormatVersion = 1;
constexpr uint8_t kLogMetaSize = 3;
constexpr uint8_t kHeadRecordSize = 4;
constexpr uint16_t kHeadRingDivisor = 20;
const char *kLogFilePath = "/logs.bin";

struct PersistedLogEntry {
  uint16_t seq;
  LogEntry entry;
};

static bool g_logs_ready = false;
static uint16_t g_total_slots = 0;
static uint16_t g_head_record_count = 0;
static uint32_t g_log_start_offset = 0;
static int16_t g_current_slot = -1;
static int16_t g_latest_slot = -1;
static int16_t g_head_index = -1;
static uint16_t g_latest_seq = 0;
static uint16_t g_epoch = 0;
static uint16_t g_browse_epoch = 0;
static PersistedLogEntry g_slot_buffer = {};

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
  return g_log_start_offset + static_cast<uint32_t>(slot) * sizeof(PersistedLogEntry);
}

/*
 * read_slot
 * Reads a log slot into the provided buffer.
 * Example:
 *   PersistedLogEntry entry = {};
 *   read_slot(0, &entry);
 */
static bool read_slot(int16_t slot, PersistedLogEntry *out) {
  if (!out || slot < 0 || slot >= static_cast<int16_t>(g_total_slots)) return false;
  if (!read_block(slot_offset(slot), out, sizeof(PersistedLogEntry))) {
    memset(out, 0, sizeof(PersistedLogEntry));
    return false;
  }
  return true;
}

/*
 * write_slot
 * Writes a log entry into the specified slot.
 * Example:
 *   write_slot(0, record);
 */
static bool write_slot(int16_t slot, const PersistedLogEntry &entry) {
  if (slot < 0 || slot >= static_cast<int16_t>(g_total_slots)) return false;
  return write_block(slot_offset(slot), &entry, sizeof(PersistedLogEntry));
}

/*
 * seq_is_later
 * Compares 16-bit sequences with wrap-around semantics.
 * Example:
 *   if (seq_is_later(new_seq, old_seq)) { ... }
 */
static inline bool seq_is_later(uint16_t a, uint16_t b) {
  return static_cast<int16_t>(a - b) > 0;
}

/*
 * slot_is_empty
 * Returns true if a slot sequence indicates no data.
 * Example:
 *   if (slot_is_empty(entry.seq)) { ... }
 */
static inline bool slot_is_empty(uint16_t seq) {
  return seq == 0;
}

/*
 * read_epoch_from_flash
 * Loads the persisted epoch counter.
 * Example:
 *   uint16_t epoch = read_epoch_from_flash();
 */
static uint16_t read_epoch_from_flash() {
  uint8_t data[2] = {0};
  if (!read_block(0, data, sizeof(data))) return 0;
  if (data[0] == 0xFF && data[1] == 0xFF) return 0;
  return static_cast<uint16_t>(data[0]) | (static_cast<uint16_t>(data[1]) << 8);
}

/*
 * write_epoch_to_flash
 * Stores the epoch counter to metadata.
 * Example:
 *   write_epoch_to_flash(g_epoch);
 */
static void write_epoch_to_flash(uint16_t epoch) {
  uint8_t data[2];
  data[0] = static_cast<uint8_t>(epoch & 0xFF);
  data[1] = static_cast<uint8_t>((epoch >> 8) & 0xFF);
  write_block(0, data, sizeof(data));
}

/*
 * read_version_from_flash
 * Loads the stored log format version.
 * Example:
 *   uint8_t version = read_version_from_flash();
 */
static uint8_t read_version_from_flash() {
  uint8_t version = 0;
  if (!read_block(2, &version, 1)) return 0;
  if (version == 0xFF) return 0;
  return version;
}

/*
 * write_version_to_flash
 * Stores the log format version.
 * Example:
 *   write_version_to_flash(kLogFormatVersion);
 */
static void write_version_to_flash(uint8_t version) {
  write_block(2, &version, 1);
}

/*
 * write_head_record
 * Appends a (seq, slot) record to the rotating head ring.
 * Example:
 *   write_head_record(seq, slot);
 */
static void write_head_record(uint16_t seq, uint16_t slot) {
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
 * clear_ram_logs
 * Clears the in-memory log list used by the UI.
 * Example:
 *   clear_ram_logs();
 */
static void clear_ram_logs() {
  g_log_count = 0;
  g_log_index = 0;
}

/*
 * push_ram_log
 * Inserts a log entry into the in-memory list (newest first).
 * Example:
 *   push_ram_log(entry);
 */
static void push_ram_log(const LogEntry &entry) {
  if (g_log_count < kMaxLogs) {
    g_log_count++;
  }
  for (int i = g_log_count - 1; i > 0; --i) {
    g_logs[i] = g_logs[i - 1];
  }
  g_logs[0] = entry;
}

/*
 * write_zeroes
 * Writes zero-filled blocks over a file range.
 * Example:
 *   write_zeroes(offset, len);
 */
static void write_zeroes(uint32_t offset, size_t len) {
  uint8_t zeros[128] = {0};
  while (len > 0) {
    size_t chunk = (len > sizeof(zeros)) ? sizeof(zeros) : len;
    write_block(offset, zeros, chunk);
    offset += static_cast<uint32_t>(chunk);
    len -= chunk;
  }
}

/*
 * wipe_storage
 * Clears head ring and log slots, then resets metadata.
 * Example:
 *   wipe_storage();
 */
static void wipe_storage() {
  if (g_head_record_count > 0) {
    size_t ring_bytes = static_cast<size_t>(g_head_record_count) * kHeadRecordSize;
    write_zeroes(kLogMetaSize, ring_bytes);
  }
  size_t slots_bytes = static_cast<size_t>(g_total_slots) * sizeof(PersistedLogEntry);
  write_zeroes(g_log_start_offset, slots_bytes);
  g_epoch = 0;
  g_browse_epoch = 0;
  g_latest_slot = -1;
  g_head_index = -1;
  g_latest_seq = 0;
  g_current_slot = -1;
  write_epoch_to_flash(g_epoch);
  write_version_to_flash(kLogFormatVersion);
  clear_ram_logs();
}

/*
 * find_latest_from_head
 * Scans the head ring to locate the most recent log slot.
 * Example:
 *   find_latest_from_head();
 */
static void find_latest_from_head() {
  uint16_t best_seq = 0;
  g_latest_slot = -1;
  g_head_index = -1;
  for (uint16_t i = 0; i < g_head_record_count; ++i) {
    uint8_t data[4] = {0};
    uint32_t addr = kLogMetaSize + static_cast<uint32_t>(i) * kHeadRecordSize;
    if (!read_block(addr, data, sizeof(data))) continue;
    uint16_t seq = static_cast<uint16_t>(data[0]) | (static_cast<uint16_t>(data[1]) << 8);
    uint16_t slot = static_cast<uint16_t>(data[2]) | (static_cast<uint16_t>(data[3]) << 8);
    if (seq == 0 || slot >= g_total_slots) continue;
    if (g_head_index < 0 || seq_is_later(seq, best_seq)) {
      best_seq = seq;
      g_latest_slot = static_cast<int16_t>(slot);
      g_head_index = static_cast<int16_t>(i);
    }
  }
  g_latest_seq = best_seq;
  if (g_latest_slot >= 0) g_current_slot = g_latest_slot;
}

/*
 * load_recent_logs
 * Loads the most recent logs into the in-memory list.
 * Example:
 *   load_recent_logs();
 */
static void load_recent_logs() {
  clear_ram_logs();
  if (g_latest_slot < 0) return;

  PersistedLogEntry entry = {};
  if (!read_slot(g_latest_slot, &entry)) return;
  if (slot_is_empty(entry.seq)) return;

  push_ram_log(entry.entry);
  uint16_t prev_seq = entry.seq;
  int16_t slot = g_latest_slot;

  while (g_log_count < kMaxLogs) {
    int16_t next_slot = static_cast<int16_t>(slot - 1);
    if (next_slot < 0) next_slot = static_cast<int16_t>(g_total_slots - 1);

    if (!read_slot(next_slot, &entry)) break;
    if (slot_is_empty(entry.seq)) break;
    if (!seq_is_later(prev_seq, entry.seq)) break;

    push_ram_log(entry.entry);
    prev_seq = entry.seq;
    slot = next_slot;
  }
}

/*
 * append_persisted_log
 * Writes a new log entry to the flash ring buffer.
 * Example:
 *   append_persisted_log(entry);
 */
static void append_persisted_log(const LogEntry &entry) {
  if (!g_logs_ready || g_total_slots == 0) return;

  uint16_t new_seq = static_cast<uint16_t>(g_latest_seq + 1);
  if (new_seq == 0) {
    new_seq = 1;
    g_epoch++;
    write_epoch_to_flash(g_epoch);
  }

  int16_t next_slot = 0;
  if (g_latest_slot >= 0) {
    next_slot = static_cast<int16_t>(g_latest_slot + 1);
    if (next_slot >= static_cast<int16_t>(g_total_slots)) next_slot = 0;
  }

  PersistedLogEntry record = {};
  record.seq = new_seq;
  record.entry = entry;
  if (!write_slot(next_slot, record)) return;

  write_head_record(new_seq, static_cast<uint16_t>(next_slot));
  g_latest_slot = next_slot;
  g_latest_seq = new_seq;
  g_current_slot = next_slot;
  g_browse_epoch = g_epoch;
}

} // namespace

/*
 * logs_init
 * Initializes flash-backed log storage and loads recent logs into RAM.
 * Example:
 *   logs_init();
 */
void logs_init() {
  g_logs_ready = false;
  g_total_slots = 0;
  g_head_record_count = 0;
  g_log_start_offset = 0;
  g_current_slot = -1;
  g_latest_slot = -1;
  g_head_index = -1;
  g_latest_seq = 0;
  g_epoch = 0;
  g_browse_epoch = 0;
  clear_ram_logs();

  if (!ensure_fs()) return;
  if (!ensure_log_file_size(kLogStoreBytes)) return;

  size_t log_area = kLogStoreBytes - kLogMetaSize;
  size_t ring_bytes = log_area / kHeadRingDivisor;
  ring_bytes &= ~static_cast<size_t>(3);
  if (ring_bytes < kHeadRecordSize) ring_bytes = kHeadRecordSize;

  g_head_record_count = static_cast<uint16_t>(ring_bytes / kHeadRecordSize);
  g_log_start_offset = static_cast<uint32_t>(kLogMetaSize + ring_bytes);
  g_total_slots = static_cast<uint16_t>((kLogStoreBytes - g_log_start_offset) / sizeof(PersistedLogEntry));

  if (g_total_slots == 0) return;

  uint8_t stored_version = read_version_from_flash();
  if (stored_version != kLogFormatVersion) {
    g_logs_ready = true;
    wipe_storage();
    return;
  }

  g_epoch = read_epoch_from_flash();
  g_browse_epoch = g_epoch;
  find_latest_from_head();
  load_recent_logs();
  g_logs_ready = true;
}

/*
 * logs_wipe
 * Clears all persisted logs and resets in-memory log state.
 * Example:
 *   logs_wipe();
 */
void logs_wipe() {
  if (!ensure_fs()) return;
  if (!ensure_log_file_size(kLogStoreBytes)) return;
  if (g_total_slots == 0) {
    size_t log_area = kLogStoreBytes - kLogMetaSize;
    size_t ring_bytes = log_area / kHeadRingDivisor;
    ring_bytes &= ~static_cast<size_t>(3);
    if (ring_bytes < kHeadRecordSize) ring_bytes = kHeadRecordSize;
    g_head_record_count = static_cast<uint16_t>(ring_bytes / kHeadRecordSize);
    g_log_start_offset = static_cast<uint32_t>(kLogMetaSize + ring_bytes);
    g_total_slots = static_cast<uint16_t>((kLogStoreBytes - g_log_start_offset) / sizeof(PersistedLogEntry));
  }
  g_logs_ready = true;
  wipe_storage();
}

/*
 * add_log
 * Adds a log entry to RAM and persists it to flash storage.
 * Example:
 *   add_log(build_boot_log());
 */
void add_log(const LogEntry &entry) {
  append_persisted_log(entry);
  push_ram_log(entry);
}

/*
 * build_value_log
 * Builds a type-2 values log from the current simulated state.
 * Example:
 *   LogEntry entry = build_value_log();
 */
LogEntry build_value_log() {
  LogEntry entry = {};
  entry.type = 2;
  entry.dt = g_sim.now;
  entry.soil_pct = g_sim.moisture;
  entry.dryback = g_sim.dryback;
  return entry;
}

/*
 * build_boot_log
 * Builds a type-0 boot/marker log from the current simulated state.
 * Example:
 *   add_log(build_boot_log());
 */
LogEntry build_boot_log() {
  LogEntry entry = {};
  entry.type = 0;
  entry.dt = g_sim.now;
  entry.soil_pct = g_sim.moisture;
  entry.dryback = g_sim.dryback;
  return entry;
}

/*
 * build_feed_log
 * Builds a type-1 feed log from the current feeding session.
 * Example:
 *   LogEntry entry = build_feed_log();
 */
LogEntry build_feed_log() {
  LogEntry entry = {};
  entry.type = 1;
  entry.dt = g_sim.now;
  entry.slot = g_sim.feeding_slot + 1;
  entry.start_moisture = clamp_int(g_sim.moisture - 6, 0, 100);
  entry.end_moisture = g_sim.moisture;
  entry.volume_ml = g_sim.feeding_water_ml;
  entry.daily_total_ml = g_sim.daily_total_ml;
  entry.dryback = g_sim.dryback;
  entry.runoff = g_sim.runoff;
  strncpy(entry.stop_reason, g_sim.runoff ? "Runoff" : "Target", sizeof(entry.stop_reason) - 1);
  entry.stop_reason[sizeof(entry.stop_reason) - 1] = '\0';
  return entry;
}
