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
#include "volume.h"

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
  FEED_STOP_UI_PAUSE,
  FEED_STOP_MAX_DAILY_FEED_REACHED,
  FEED_STOP_FEED_NOT_CALIBRATED
};

struct FeedSession {
  bool active;
  uint8_t slotIndex;
  FeedSlot slot;
  unsigned long startMillis;
  uint32_t minVolumeMs;
  uint32_t maxVolumeMs;
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
static bool rtcValid = false;
static uint16_t rtcMinutes = 0;
static uint16_t rtcDayKey = 0;
static unsigned long rtcLastReadAt = 0;
static uint16_t lastEvaluatedRtcMinutes = 0xFFFF;
static uint16_t lastOffsetMinutes = 0;
static bool lastOffsetValid = false;
static bool feedingDisabledCached = false;
static bool feedingPausedForUi = false;
static uint32_t lastRefusalLightKey = 0xFFFFFFFFUL;
static uint8_t lastRefusalReason = 0xFF;

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

static inline bool dripperCalibrated() {
  return (config.flags & CONFIG_FLAG_DRIPPER_CALIBRATED) != 0;
}

static bool slotFlag(const FeedSlot *slot, uint8_t flag) {
  return (slot->flags & flag) != 0;
}

static bool isWithinWindow(uint16_t nowOffset, uint16_t startOffset, uint16_t duration) {
  if (duration == 0) return false;
  if (duration > 1439) duration = 1439;
  if (startOffset > 1439) startOffset = 1439;
  uint16_t endOffset = startOffset + duration;
  if (endOffset >= 1440) endOffset -= 1440;
  if (startOffset <= endOffset) {
    return nowOffset >= startOffset && nowOffset < endOffset;
  }
  return nowOffset >= startOffset || nowOffset < endOffset;
}

static uint16_t minutesSinceLightsOn() {
  uint16_t lightsOn = config.lightsOnMinutes;
  if (lightsOn > 1439) lightsOn = 0;
  if (rtcMinutes >= lightsOn) return static_cast<uint16_t>(rtcMinutes - lightsOn);
  return static_cast<uint16_t>(rtcMinutes + 1440 - lightsOn);
}

static uint32_t ticksToMs(uint8_t ticks) {
  return static_cast<uint32_t>(ticks) * kTickSeconds * 1000UL;
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
  session.startMillis = millis();
  session.minVolumeMs = volumeMlToMs(slot->minVolumeMl, config.dripperMsPerLiter);
  session.maxVolumeMs = volumeMlToMs(slot->maxVolumeMl, config.dripperMsPerLiter);
  session.pumpOn = true;
  session.runoffStartAt = 0;
  session.soilBeforePercent = soilMoistureAsPercentage(getSoilMoisture());
  session.trayBefore = trayWaterLevelAsState();
  session.startReason = timeTriggered ? LOG_START_TIME : LOG_START_MOISTURE;
  session.flags = 0;

  rtcStamp(&session.startYear, &session.startMonth, &session.startDay,
           &session.startHour, &session.startMinute);

  setSoilSensorRealTime();
  openLineIn();

  (void)rtcValid;
}

static void logFeedRefusal(uint8_t slotIndex, bool timeTriggered, FeedStopReason reason) {
  uint32_t lightKey = getLightDayKeyNow();
  uint8_t reasonCode = static_cast<uint8_t>(reason);
  if (lightKey == lastRefusalLightKey && reasonCode == lastRefusalReason) return;
  lastRefusalLightKey = lightKey;
  lastRefusalReason = reasonCode;

  uint8_t trayState = trayWaterLevelAsState();
  uint8_t soilPercent = soilMoistureAsPercentage(getSoilMoisture());
  unsigned long now = millis();

  clearLogEntry((void *)&newLogEntry);
  newLogEntry.entryType = 1;
  newLogEntry.millisStart = now;
  newLogEntry.millisEnd = now;
  newLogEntry.stopReason = reasonCode;
  newLogEntry.startReason = timeTriggered ? LOG_START_TIME : LOG_START_MOISTURE;
  newLogEntry.slotIndex = slotIndex;
  newLogEntry.flags = 0;
  newLogEntry.trayWaterLevelBefore = trayState;
  newLogEntry.trayWaterLevelAfter = trayState;
  newLogEntry.soilMoistureBefore = soilPercent;
  newLogEntry.soilMoistureAfter = soilPercent;

  rtcStamp(&newLogEntry.startYear, &newLogEntry.startMonth, &newLogEntry.startDay,
           &newLogEntry.startHour, &newLogEntry.startMinute);
  newLogEntry.endYear = newLogEntry.startYear;
  newLogEntry.endMonth = newLogEntry.startMonth;
  newLogEntry.endDay = newLogEntry.startDay;
  newLogEntry.endHour = newLogEntry.startHour;
  newLogEntry.endMinute = newLogEntry.startMinute;

  uint16_t dailyTotal = 0;
  if (newLogEntry.startMonth && newLogEntry.startDay) {
    dailyTotal = getDailyFeedTotalMlAt(newLogEntry.startYear, newLogEntry.startMonth,
                                       newLogEntry.startDay, newLogEntry.startHour,
                                       newLogEntry.startMinute);
  }
  newLogEntry.feedMl = 0;
  newLogEntry.dailyTotalMl = dailyTotal;

  writeLogEntry((void *)&newLogEntry);
}

static void stopFeed(FeedStopReason reason, unsigned long now) {
  closeLineIn();
  session.pumpOn = false;

  uint8_t trayAfter = trayWaterLevelAsState();
  uint16_t realtimeAvg = soilSensorGetRealtimeAvg();
  uint8_t soilAfterPercent = soilMoistureAsPercentage(realtimeAvg);
  setSoilSensorLazySeed(realtimeAvg);
  unsigned long elapsedMs = now - session.startMillis;
  uint16_t feedMl = msToVolumeMl(elapsedMs, config.dripperMsPerLiter);

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

  rtcStamp(&newLogEntry.endYear, &newLogEntry.endMonth, &newLogEntry.endDay,
           &newLogEntry.endHour, &newLogEntry.endMinute);
  uint16_t dailyTotal = 0;
  if (newLogEntry.endMonth && newLogEntry.endDay) {
    uint16_t previousTotal = getDailyFeedTotalMlAt(newLogEntry.endYear, newLogEntry.endMonth,
                                                   newLogEntry.endDay, newLogEntry.endHour,
                                                   newLogEntry.endMinute);
    if (UINT16_MAX - previousTotal < feedMl) dailyTotal = UINT16_MAX;
    else dailyTotal = static_cast<uint16_t>(previousTotal + feedMl);
  }
  newLogEntry.feedMl = feedMl;
  newLogEntry.dailyTotalMl = dailyTotal;

  writeLogEntry((void *)&newLogEntry);

  if (millisAtEndOfLastFeed) {
    updateaverageMsBetweenFeeds(now - millisAtEndOfLastFeed);
  }
  millisAtEndOfLastFeed = now;

  session.active = false;
}

static void tickActiveFeed(unsigned long now) {
  uint8_t moisturePercent = soilMoistureAsPercentage(getSoilMoisture());
  bool moistureReady = soilSensorRealtimeReady();

  unsigned long elapsed = now - session.startMillis;

  bool maxReached = (session.maxVolumeMs > 0) && (elapsed >= session.maxVolumeMs);
  bool minReached = true;
  if (slotFlag(&session.slot, FEED_SLOT_HAS_MIN_RUNTIME) && session.minVolumeMs > 0) {
    minReached = elapsed >= session.minVolumeMs;
  }

  bool moistureStop = false;
  if (slotFlag(&session.slot, FEED_SLOT_HAS_MOISTURE_TARGET) && moistureReady) {
    moistureStop = moisturePercent >= session.slot.moistureTarget;
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

  if (minReached && (moistureStop || runoffStop)) {
    stopFeed(moistureStop ? FEED_STOP_MOISTURE : FEED_STOP_RUNOFF, now);
    return;
  }
}

static bool startConditionsMet(const FeedSlot *slot, bool *timeOkOut, bool *moistureOkOut) {
  if (!soilSensorReady()) return false;

  bool hasTime = slotFlag(slot, FEED_SLOT_HAS_TIME_WINDOW);
  bool hasMoisture = slotFlag(slot, FEED_SLOT_HAS_MOISTURE_BELOW);
  if (!hasTime && !hasMoisture) return false;

  bool timeOk = false;
  if (hasTime && updateRtcCache()) {
    if (rtcMinutes != lastEvaluatedRtcMinutes) {
      lastEvaluatedRtcMinutes = rtcMinutes;
      lastOffsetMinutes = minutesSinceLightsOn();
      lastOffsetValid = true;
    }
    if (lastOffsetValid) {
      timeOk = isWithinWindow(lastOffsetMinutes, slot->windowStartMinutes, slot->windowDurationMinutes);
    }
  } else if (hasTime) {
    lastOffsetValid = false;
  }

  bool moistureOk = false;
  if (hasMoisture) {
    uint8_t moisturePercent = soilMoistureAsPercentage(getSoilMoisture());
    moistureOk = moisturePercent <= slot->moistureBelow;
  }

  if (!timeOk && !moistureOk) return false;

  if (slot->minGapMinutes > 0 && millisAtEndOfLastFeed) {
    unsigned long minDelayMs = (unsigned long)slot->minGapMinutes * 60000UL;
    if ((millis() - millisAtEndOfLastFeed) < minDelayMs) return false;
  }

  if (timeOkOut) *timeOkOut = timeOk;
  if (moistureOkOut) *moistureOkOut = moistureOk;
  return true;
}

static void maybeStartFeed() {
  bool calibrated = dripperCalibrated();
  uint16_t maxDaily = config.maxDailyWaterMl;
  uint16_t dailyTotal = 0;
  bool dailyTotalValid = false;

  for (uint8_t i = 0; i < FEED_SLOT_COUNT; ++i) {
    FeedSlot slot;
    unpackFeedSlot(&slot, config.feedSlotsPacked[i]);
    if (!slotFlag(&slot, FEED_SLOT_ENABLED)) continue;
    bool timeOk = false;
    bool moistureOk = false;
    if (!startConditionsMet(&slot, &timeOk, &moistureOk)) continue;

    bool timeTriggered = timeOk && rtcValid;
    if (!calibrated) {
      logFeedRefusal(i, timeTriggered, FEED_STOP_FEED_NOT_CALIBRATED);
      return;
    }
    if (maxDaily > 0) {
      if (!dailyTotalValid) {
        dailyTotal = getDailyFeedTotalMlNow();
        dailyTotalValid = true;
      }
      if (dailyTotal >= maxDaily) {
        logFeedRefusal(i, timeTriggered, FEED_STOP_MAX_DAILY_FEED_REACHED);
        return;
      }
    }

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
  outStatus->moistureReady = soilSensorRealtimeReady();
  outStatus->moisturePercent = soilMoistureAsPercentage(getSoilMoisture());
  outStatus->hasMoistureTarget = slotFlag(&session.slot, FEED_SLOT_HAS_MOISTURE_TARGET);
  outStatus->moistureTarget = session.slot.moistureTarget;
  outStatus->runoffRequired = slotFlag(&session.slot, FEED_SLOT_RUNOFF_REQUIRED);
  outStatus->hasMinRuntime = slotFlag(&session.slot, FEED_SLOT_HAS_MIN_RUNTIME);
  outStatus->minVolumeMl = session.slot.minVolumeMl;
  outStatus->maxVolumeMl = session.slot.maxVolumeMl;
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
