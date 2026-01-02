#include "feeding.h"

#include <Arduino.h>
#include <string.h>

#include "config.h"
#include "feedSlots.h"
#include "logs.h"
#include "moistureSensor.h"
#include "pumps.h"
#include "rtc.h"
#include "settings.h"
#include "traySensors.h"
#include "weightSensor.h"

extern Config config;
extern LogEntry newLogEntry;
extern unsigned long int millisAtEndOfLastFeed;
void updateaverageMsBetweenFeeds(unsigned long durationMillis);

namespace {
const uint8_t kTickSeconds = 5;
const unsigned long kRtcReadIntervalMs = 1000;

enum FeedStopReason : uint8_t {
  FEED_STOP_NONE = 0,
  FEED_STOP_MOISTURE,
  FEED_STOP_RUNOFF,
  FEED_STOP_MAX_RUNTIME,
  FEED_STOP_DISABLED,
  FEED_STOP_UI_PAUSE
};

struct FeedSession {
  bool active;
  uint8_t slotIndex;
  FeedSlot slot;
  unsigned long startMillis;
  unsigned long lastPulseToggleAt;
  bool pumpOn;
  unsigned long runoffStartAt;
  uint8_t soilBeforePercent;
  uint8_t trayBefore;
  uint8_t startYear;
  uint8_t startMonth;
  uint8_t startDay;
  uint8_t startHour;
  uint8_t startMinute;
  uint8_t startReason;
  uint8_t flags;
};

static FeedSession session = {};
static uint16_t lastTimeTriggerDay[FEED_SLOT_COUNT] = {};
static bool rtcValid = false;
static uint16_t rtcMinutes = 0;
static uint16_t rtcDayKey = 0;
static unsigned long rtcLastReadAt = 0;
static bool feedingDisabledCached = false;
static bool feedingPausedForUi = false;

static inline bool feedingDisabledFlag() {
  return (config.flags & CONFIG_FLAG_FEEDING_DISABLED) != 0;
}

static inline void setFeedingDisabledFlag(bool disabled) {
  if (disabled) config.flags |= CONFIG_FLAG_FEEDING_DISABLED;
  else config.flags &= static_cast<uint8_t>(~CONFIG_FLAG_FEEDING_DISABLED);
  feedingDisabledCached = disabled;
}

static inline bool feedingPausedFlag() {
  return feedingPausedForUi;
}

static inline void setFeedingPausedFlag(bool paused) {
  feedingPausedForUi = paused;
}

static bool slotFlag(const FeedSlot *slot, uint8_t flag) {
  return (slot->flags & flag) != 0;
}

static uint32_t ticksToMs(uint8_t ticks) {
  return static_cast<uint32_t>(ticks) * kTickSeconds * 1000UL;
}

static uint16_t ticksToSeconds(uint8_t ticks) {
  return static_cast<uint16_t>(ticks) * kTickSeconds;
}

static uint16_t weightPercentOfFull(uint16_t weightKg10, uint16_t fullKg10) {
  if (fullKg10 == 0) return 0;
  return static_cast<uint16_t>((weightKg10 * 100UL) / fullKg10);
}

static void loadFeedSlotRuntime(uint8_t index, FeedSlot *slot) {
  unpackFeedSlot(slot, config.feedSlotsPacked[index]);
}

static void clampSlotDurations(FeedSlot *slot) {
  if (slot->maxRuntime5s == 0) slot->maxRuntime5s = 1;
  if (slotFlag(slot, FEED_SLOT_PULSED)) {
    if (slot->pulseOn5s == 0) slot->pulseOn5s = 1;
    if (slot->pulseOff5s == 0) slot->pulseOff5s = 1;
  }
}

static bool updateRtcCache() {
  unsigned long now = millis();
  if (now - rtcLastReadAt < kRtcReadIntervalMs) return rtcValid;

  rtcLastReadAt = now;
  rtcValid = rtcReadMinutesAndDay(&rtcMinutes, &rtcDayKey);
  return rtcValid;
}

static void startFeed(uint8_t slotIndex, const FeedSlot *slot, bool timeTriggered) {
  if (feedingDisabledCached || feedingPausedFlag()) return;

  session.active = true;
  session.slotIndex = slotIndex;
  session.slot = *slot;
  clampSlotDurations(&session.slot);
  session.startMillis = millis();
  session.lastPulseToggleAt = session.startMillis;
  session.pumpOn = true;
  session.runoffStartAt = 0;
  session.soilBeforePercent = soilMoistureAsPercentage(getSoilMoisture());
  session.trayBefore = trayWaterLevelAsState();
  session.startReason = timeTriggered ? LOG_START_TIME : LOG_START_MOISTURE;
  session.flags = slotFlag(slot, FEED_SLOT_PULSED) ? LOG_FLAG_PULSED : 0;

  uint8_t hour = 0, minute = 0, day = 0, month = 0, year = 0;
  if (rtcReadDateTime(&hour, &minute, &day, &month, &year)) {
    session.startYear = year;
    session.startMonth = month;
    session.startDay = day;
    session.startHour = hour;
    session.startMinute = minute;
  } else {
    session.startYear = 0;
    session.startMonth = 0;
    session.startDay = 0;
    session.startHour = 0;
    session.startMinute = 0;
  }

  setSoilSensorRealTime();
  openLineIn();

  if (timeTriggered && rtcValid) {
    lastTimeTriggerDay[slotIndex] = rtcDayKey;
  }
}

static void stopFeed(FeedStopReason reason, unsigned long now) {
  closeLineIn();
  session.pumpOn = false;

  uint8_t trayAfter = trayWaterLevelAsState();
  uint16_t realtimeAvg = soilSensorGetRealtimeAvg();
  uint8_t soilAfterPercent = soilMoistureAsPercentage(realtimeAvg);
  setSoilSensorLazySeed(realtimeAvg);

  clearLogEntry((void *)&newLogEntry);
  newLogEntry.entryType = 1;
  newLogEntry.millisStart = session.startMillis;
  newLogEntry.millisEnd = now;
  newLogEntry.stopReason = static_cast<uint8_t>(reason);
  newLogEntry.startReason = session.startReason;
  newLogEntry.slotIndex = session.slotIndex;
  newLogEntry.flags = session.flags;
  newLogEntry.trayWaterLevelBefore = session.trayBefore;
  newLogEntry.trayWaterLevelAfter = trayAfter;
  newLogEntry.soilMoistureBefore = session.soilBeforePercent;
  newLogEntry.soilMoistureAfter = soilAfterPercent;
  newLogEntry.startYear = session.startYear;
  newLogEntry.startMonth = session.startMonth;
  newLogEntry.startDay = session.startDay;
  newLogEntry.startHour = session.startHour;
  newLogEntry.startMinute = session.startMinute;

  uint8_t hour = 0, minute = 0, day = 0, month = 0, year = 0;
  if (rtcReadDateTime(&hour, &minute, &day, &month, &year)) {
    newLogEntry.endYear = year;
    newLogEntry.endMonth = month;
    newLogEntry.endDay = day;
    newLogEntry.endHour = hour;
    newLogEntry.endMinute = minute;
  } else {
    newLogEntry.endYear = 0;
    newLogEntry.endMonth = 0;
    newLogEntry.endDay = 0;
    newLogEntry.endHour = 0;
    newLogEntry.endMinute = 0;
  }

  writeLogEntry((void *)&newLogEntry);

  if (millisAtEndOfLastFeed) {
    updateaverageMsBetweenFeeds(now - millisAtEndOfLastFeed);
  }
  millisAtEndOfLastFeed = now;

  session.active = false;
}

static void updatePumpState(unsigned long now) {
  if (!slotFlag(&session.slot, FEED_SLOT_PULSED)) {
    if (!session.pumpOn) {
      session.pumpOn = true;
      openLineIn();
    }
    return;
  }

  unsigned long onMs = ticksToMs(session.slot.pulseOn5s);
  unsigned long offMs = ticksToMs(session.slot.pulseOff5s);
  if (onMs == 0) onMs = kTickSeconds * 1000UL;
  if (offMs == 0) offMs = kTickSeconds * 1000UL;

  if (session.pumpOn) {
    if (now - session.lastPulseToggleAt >= onMs) {
      session.pumpOn = false;
      session.lastPulseToggleAt = now;
      closeLineIn();
    }
  } else {
    if (now - session.lastPulseToggleAt >= offMs) {
      session.pumpOn = true;
      session.lastPulseToggleAt = now;
      openLineIn();
    }
  }
}

static void tickActiveFeed(unsigned long now) {
  uint8_t moisturePercent = soilMoistureAsPercentage(getSoilMoisture());
  bool moistureReady = soilSensorRealtimeReady();
  bool weightReady = weightSensorReady();
  float weightGrams = weightSensorLastValue();
  uint16_t weightKg10 = static_cast<uint16_t>((weightGrams + 50.0f) / 100.0f); // deci-kg

  bool maxReached = (now - session.startMillis) >= ticksToMs(session.slot.maxRuntime5s);
  bool minReached = true;
  if (slotFlag(&session.slot, FEED_SLOT_HAS_MIN_RUNTIME)) {
    minReached = (now - session.startMillis) >= ticksToMs(session.slot.minRuntime5s);
  }

  bool moistureStop = false;
  if (slotFlag(&session.slot, FEED_SLOT_HAS_MOISTURE_TARGET) && moistureReady) {
    moistureStop = moisturePercent >= session.slot.moistureTarget;
  }

  bool weightStop = false;
  uint16_t fullKg10 = config.weightFullKg10;
  if (session.slot.weightAboveKg10 && weightReady && fullKg10) {
    uint16_t pct = weightPercentOfFull(weightKg10, fullKg10);
    weightStop = pct >= session.slot.weightAboveKg10;
  }

  bool runoffStop = false;
  if (slotFlag(&session.slot, FEED_SLOT_RUNOFF_REQUIRED)) {
    bool runoffNow = trayWaterLevelAsState() == TRAY_LITTLE;
    if (runoffNow) {
      if (session.runoffStartAt == 0) session.runoffStartAt = now;
      unsigned long holdMs = ticksToMs(session.slot.runoffHold5s);
      if (session.runoffStartAt && (now - session.runoffStartAt) >= holdMs) {
        runoffStop = true;
      }
    } else {
      session.runoffStartAt = 0;
    }
  }

  if (maxReached) {
    stopFeed(FEED_STOP_MAX_RUNTIME, now);
    return;
  }

  if (minReached && (moistureStop || runoffStop || weightStop)) {
    FeedStopReason reason = FEED_STOP_RUNOFF;
    if (moistureStop) reason = FEED_STOP_MOISTURE;
    else if (weightStop) reason = FEED_STOP_MAX_RUNTIME; // reuse Max as weight cap
    stopFeed(reason, now);
    return;
  }

  updatePumpState(now);
}

static bool startConditionsMet(uint8_t slotIndex, const FeedSlot *slot) {
  bool hasTime = slotFlag(slot, FEED_SLOT_HAS_TIME_WINDOW);
  bool hasMoisture = slotFlag(slot, FEED_SLOT_HAS_MOISTURE_BELOW);
  bool hasWeight = slot->weightBelowKg10 != 0;
  uint16_t fullKg10 = config.weightFullKg10;
  if (!hasTime && !hasMoisture && !hasWeight) return false;

  bool timeOk = false;
  if (hasTime && updateRtcCache()) {
    if (rtcMinutes == slot->startMinute && lastTimeTriggerDay[slotIndex] != rtcDayKey) {
      timeOk = true;
    }
  }

  bool moistureOk = false;
  if (hasMoisture) {
    if (!soilSensorReady()) return false;
    uint8_t moisturePercent = soilMoistureAsPercentage(getSoilMoisture());
    moistureOk = moisturePercent <= slot->moistureBelow;
  }

  bool weightOk = false;
  if (hasWeight) {
    if (fullKg10 == 0) return false;
    if (!weightSensorReady()) return false;
    float weightGrams = weightSensorLastValue();
    uint16_t weightKg10 = static_cast<uint16_t>((weightGrams + 50.0f) / 100.0f);
    uint16_t pct = weightPercentOfFull(weightKg10, fullKg10);
    weightOk = pct <= slot->weightBelowKg10;
  }

  if (!timeOk && !moistureOk && !weightOk) return false;

  if (slot->minGapMinutes > 0 && millisAtEndOfLastFeed) {
    unsigned long minDelayMs = (unsigned long)slot->minGapMinutes * 60000UL;
    if ((millis() - millisAtEndOfLastFeed) < minDelayMs) return false;
  }

  return true;
}

static void maybeStartFeed() {
  for (uint8_t i = 0; i < FEED_SLOT_COUNT; ++i) {
    FeedSlot slot;
    unpackFeedSlot(&slot, config.feedSlotsPacked[i]);
    if (!slotFlag(&slot, FEED_SLOT_ENABLED)) continue;
    if (!startConditionsMet(i, &slot)) continue;

    bool timeTriggered = slotFlag(&slot, FEED_SLOT_HAS_TIME_WINDOW) && rtcValid && (rtcMinutes == slot.startMinute);
    startFeed(i, &slot, timeTriggered);
    break;
  }
}
}

void feedingTick() {
  unsigned long now = millis();

  feedingDisabledCached = feedingDisabledFlag();

  if (feedingPausedFlag()) {
    if (session.active) {
      stopFeed(FEED_STOP_UI_PAUSE, now);
    }
    return;
  }

  feedingDisabledCached = feedingDisabledFlag();
  if (feedingDisabledCached) {
    if (session.active) {
      stopFeed(FEED_STOP_DISABLED, now);
    }
    return;
  }

  if (session.active) {
    tickActiveFeed(now);
    return;
  }

  maybeStartFeed();
}

bool feedingIsActive() {
  return session.active;
}

bool feedingGetStatus(FeedStatus *outStatus) {
  if (!outStatus) return false;

  if (!session.active) {
    memset(outStatus, 0, sizeof(*outStatus));
    return false;
  }

  outStatus->active = true;
  outStatus->slotIndex = session.slotIndex;
  outStatus->pumpOn = session.pumpOn;
  outStatus->pulsed = slotFlag(&session.slot, FEED_SLOT_PULSED);
  outStatus->moistureReady = soilSensorRealtimeReady();
  outStatus->moisturePercent = soilMoistureAsPercentage(getSoilMoisture());
  outStatus->hasMoistureTarget = slotFlag(&session.slot, FEED_SLOT_HAS_MOISTURE_TARGET);
  outStatus->moistureTarget = session.slot.moistureTarget;
  outStatus->runoffRequired = slotFlag(&session.slot, FEED_SLOT_RUNOFF_REQUIRED);
  outStatus->hasMinRuntime = slotFlag(&session.slot, FEED_SLOT_HAS_MIN_RUNTIME);
  outStatus->minRuntimeSeconds = ticksToSeconds(session.slot.minRuntime5s);
  outStatus->maxRuntimeSeconds = ticksToSeconds(session.slot.maxRuntime5s);
  outStatus->elapsedSeconds = (uint16_t)((millis() - session.startMillis) / 1000UL);
  return true;
}

bool feedingIsEnabled() {
  return !feedingDisabledFlag();
}

void feedingSetEnabled(bool enabled) {
  bool disable = !enabled;
  bool wasDisabled = feedingDisabledFlag();
  if (disable == wasDisabled && (!disable || !session.active)) return;

  if (disable && session.active) {
    stopFeed(FEED_STOP_DISABLED, millis());
  }

  setFeedingDisabledFlag(disable);
  saveConfig();
}

bool feedingIsPausedForUi() {
  return feedingPausedFlag();
}

void feedingPauseForUi() {
  setFeedingPausedFlag(true);
  if (session.active) {
    stopFeed(FEED_STOP_UI_PAUSE, millis());
  }
}

void feedingResumeAfterUi() {
  setFeedingPausedFlag(false);
}

void feedingForceStartSlot(uint8_t slotIndex) {
  if (session.active || slotIndex >= FEED_SLOT_COUNT) return;

  FeedSlot slot;
  loadFeedSlotRuntime(slotIndex, &slot);
  // Treat as moisture-triggered for logging purposes.
  startFeed(slotIndex, &slot, false);
}
