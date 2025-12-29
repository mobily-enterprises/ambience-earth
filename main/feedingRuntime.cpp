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
  FEED_STOP_MAX_RUNTIME
};

struct FeedSession {
  bool active;
  uint8_t slotIndex;
  FeedSlot slot;
  unsigned long startMillis;
  unsigned long lastPulseToggleAt;
  bool pumpOn;
  uint8_t soilBeforePercent;
  uint8_t trayBefore;
};

static FeedSession session = {};
static uint16_t lastTimeTriggerDay[FEED_SLOT_COUNT] = {};
static bool rtcValid = false;
static uint16_t rtcMinutes = 0;
static uint16_t rtcDayKey = 0;
static unsigned long rtcLastReadAt = 0;

static bool slotFlag(const FeedSlot *slot, uint8_t flag) {
  return (slot->flags & flag) != 0;
}

static uint32_t ticksToMs(uint8_t ticks) {
  return static_cast<uint32_t>(ticks) * kTickSeconds * 1000UL;
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
  session.active = true;
  session.slotIndex = slotIndex;
  session.slot = *slot;
  clampSlotDurations(&session.slot);
  session.startMillis = millis();
  session.lastPulseToggleAt = session.startMillis;
  session.pumpOn = true;
  session.soilBeforePercent = soilMoistureAsPercentage(getSoilMoisture());
  session.trayBefore = trayWaterLevelAsState();

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
  uint8_t soilAfterPercent = soilMoistureAsPercentage(soilSensorGetRealtimeAvg());
  setSoilSensorLazy();

  clearLogEntry((void *)&newLogEntry);
  newLogEntry.entryType = 1;
  newLogEntry.millisStart = session.startMillis;
  newLogEntry.millisEnd = now;
  newLogEntry.actionId = session.slotIndex & 0x7;
  newLogEntry.trayWaterLevelBefore = session.trayBefore;
  newLogEntry.trayWaterLevelAfter = trayAfter;
  newLogEntry.soilMoistureBefore = session.soilBeforePercent;
  newLogEntry.soilMoistureAfter = soilAfterPercent;
  newLogEntry.topFeed = 0;
  newLogEntry.outcome = (reason == FEED_STOP_MAX_RUNTIME) ? 1 : 0;
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

  bool maxReached = (now - session.startMillis) >= ticksToMs(session.slot.maxRuntime5s);
  bool minReached = true;
  if (slotFlag(&session.slot, FEED_SLOT_HAS_MIN_RUNTIME)) {
    minReached = (now - session.startMillis) >= ticksToMs(session.slot.minRuntime5s);
  }

  bool moistureStop = false;
  if (slotFlag(&session.slot, FEED_SLOT_HAS_MOISTURE_TARGET) && moistureReady) {
    moistureStop = moisturePercent >= session.slot.moistureTarget;
  }

  bool runoffStop = false;
  if (slotFlag(&session.slot, FEED_SLOT_RUNOFF_REQUIRED)) {
    runoffStop = trayWaterLevelAsState() == TRAY_LITTLE;
  }

  if (maxReached) {
    stopFeed(FEED_STOP_MAX_RUNTIME, now);
    return;
  }

  if (minReached && (moistureStop || runoffStop)) {
    stopFeed(moistureStop ? FEED_STOP_MOISTURE : FEED_STOP_RUNOFF, now);
    return;
  }

  updatePumpState(now);
}

static bool startConditionsMet(uint8_t slotIndex, const FeedSlot *slot) {
  bool hasTime = slotFlag(slot, FEED_SLOT_HAS_TIME_WINDOW);
  bool hasMoisture = slotFlag(slot, FEED_SLOT_HAS_MOISTURE_BELOW);
  if (!hasTime && !hasMoisture) return false;

  bool timeOk = false;
  if (hasTime && updateRtcCache()) {
    if (rtcMinutes == slot->startMinute && lastTimeTriggerDay[slotIndex] != rtcDayKey) {
      timeOk = true;
    }
  }

  bool moistureOk = false;
  if (hasMoisture) {
    uint8_t moisturePercent = soilMoistureAsPercentage(getSoilMoisture());
    moistureOk = moisturePercent <= slot->moistureBelow;
  }

  if (!timeOk && !moistureOk) return false;

  if (slotFlag(slot, FEED_SLOT_HAS_MIN_SINCE) && millisAtEndOfLastFeed) {
    unsigned long minDelayMs = (unsigned long)slot->minSinceLastMinutes * 60000UL;
    if ((millis() - millisAtEndOfLastFeed) < minDelayMs) return false;
  }

  return true;
}

static void maybeStartFeed() {
  if (uiTaskActive()) return;

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

  if (session.active) {
    tickActiveFeed(now);
    return;
  }

  maybeStartFeed();
}

bool feedingIsActive() {
  return session.active;
}
