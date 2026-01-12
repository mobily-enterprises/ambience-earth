#include "feeding.h"

#include <Arduino.h>
#include <string.h>

#include "config.h"
#include "feedSlots.h"
#include "feedingUtils.h"
#include "logs.h"
#include "moistureSensor.h"
#include "pumps.h"
#include "rtc.h"
#include "settings.h"
#include "runoffSensor.h"
#include "volume.h"

extern Config config;
extern LogEntry newLogEntry;
extern unsigned long int millisAtEndOfLastFeed;
extern uint16_t lastFeedMl;

namespace {
const uint8_t kTickSeconds = 5;
const uint8_t kMoistureBaselineSentinel = 127;
const unsigned long kRtcReadIntervalMs = 1000;
const uint8_t kSessionFlagRunoffSeen = 1u << 0;

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
  uint32_t maxVolumeMs;
  uint16_t dailyTotalAtStart;
  bool pumpOn;
  bool pulseEnabled;
  uint32_t pulseOnMs;
  uint32_t pulseOffMs;
  uint32_t pulseRemainingMs;
  unsigned long lastPulseUpdateAt;
  uint32_t onElapsedMs;
  unsigned long runoffStartAt;
  uint8_t soilBeforePercent;
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
static bool sessionForceFeed = false;
static uint8_t runoffWarning = 0;
static uint32_t lastRefusalLightKey = 0xFFFFFFFFUL;
static uint8_t lastRefusalReason = 0xFF;
static uint8_t baselinePercent = 0;
static bool baselineValid = false;
static bool baselineFromLatest = false;
static int16_t baselineCandidateSlot = -1;
static uint32_t baselineCandidateEndMinutes = 0;
static bool baselineCandidateValid = false;
static unsigned long lastBaselineWindowAt = 0;

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

static uint16_t minutesSinceLightsOn() {
  uint16_t lightsOn = config.lightsOnMinutes;
  if (lightsOn > 1439) lightsOn = 0;
  if (rtcMinutes >= lightsOn) return static_cast<uint16_t>(rtcMinutes - lightsOn);
  return static_cast<uint16_t>(rtcMinutes + 1440 - lightsOn);
}

static uint32_t ticksToMs(uint8_t ticks) {
  return static_cast<uint32_t>(ticks) * kTickSeconds * 1000UL;
}

static inline bool baselineGetPercent(uint8_t *outPercent) {
  if (!baselineValid) return false;
  if (outPercent) *outPercent = baselinePercent;
  return true;
}

static bool isLeapYear(uint8_t year) {
  return (year % 4u) == 0;
}

static bool dateTimeToMinutes(uint8_t year, uint8_t month, uint8_t day,
                              uint8_t hour, uint8_t minute, uint32_t *outMinutes) {
  if (!outMinutes) return false;
  if (month == 0 || month > 12 || day == 0 || day > 31) return false;
  if (hour > 23 || minute > 59) return false;
  static const uint8_t kDaysInMonth[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  uint8_t dim = kDaysInMonth[month - 1];
  if (month == 2 && isLeapYear(year)) dim = 29;
  if (day > dim) return false;
  uint32_t days = static_cast<uint32_t>(year) * 365u + static_cast<uint32_t>((year + 3u) / 4u);
  for (uint8_t m = 1; m < month; ++m) {
    days += kDaysInMonth[m - 1];
    if (m == 2 && isLeapYear(year)) days += 1;
  }
  days += static_cast<uint32_t>(day - 1);
  *outMinutes = days * 1440u + static_cast<uint32_t>(hour) * 60u + minute;
  return true;
}

static bool getNowMinutes(uint32_t *outMinutes) {
  uint8_t hour = 0;
  uint8_t minute = 0;
  uint8_t day = 0;
  uint8_t month = 0;
  uint8_t year = 0;
  if (!rtcReadDateTime(&hour, &minute, &day, &month, &year)) return false;
  return dateTimeToMinutes(year, month, day, hour, minute, outMinutes);
}

static bool logEntryEndMinutes(const LogEntry *entry, uint32_t *outMinutes) {
  if (!entry) return false;
  return dateTimeToMinutes(entry->endYear, entry->endMonth, entry->endDay,
                           entry->endHour, entry->endMinute, outMinutes);
}

static void baselineSetCandidate(int16_t slot, const LogEntry *entry) {
  baselineCandidateSlot = slot;
  baselineCandidateEndMinutes = 0;
  baselineCandidateValid = logEntryEndMinutes(entry, &baselineCandidateEndMinutes);
  baselineFromLatest = false;
}

static void updatePulse(unsigned long now) {
  unsigned long delta = now - session.lastPulseUpdateAt;
  session.lastPulseUpdateAt = now;

  if (!session.pulseEnabled) {
    if (!session.pumpOn) {
      session.pumpOn = true;
      openLineIn();
    }
    session.onElapsedMs += delta;
    return;
  }

  while (delta > 0) {
    uint32_t step = static_cast<uint32_t>(delta);
    if (step > session.pulseRemainingMs) step = session.pulseRemainingMs;
    if (session.pumpOn) session.onElapsedMs += step;
    session.pulseRemainingMs -= step;
    delta -= step;
    if (session.pulseRemainingMs == 0) {
      session.pumpOn = !session.pumpOn;
      session.pulseRemainingMs = session.pumpOn ? session.pulseOnMs : session.pulseOffMs;
      if (session.pumpOn) openLineIn();
      else closeLineIn();
    }
  }
}

static bool updateRtcCache() {
  unsigned long now = millis();
  if (now - rtcLastReadAt < kRtcReadIntervalMs) return rtcValid;

  rtcLastReadAt = now;
  rtcValid = rtcReadMinutesAndDay(&rtcMinutes, &rtcDayKey);
  return rtcValid;
}

static void startFeed(uint8_t slotIndex, const FeedSlot *slot, bool timeTriggered, bool allowWhileDisabled) {
  if ((!allowWhileDisabled && feedingDisabledCached) || feedingPausedFlag()) return;

  sessionForceFeed = false;
  session.active = true;
  session.slotIndex = slotIndex;
  session.slot = *slot;
  if (slotFlag(&session.slot, FEED_SLOT_HAS_MOISTURE_TARGET) &&
      session.slot.moistureTarget == kMoistureBaselineSentinel) {
    uint8_t baseline = 0;
    if (baselineGetPercent(&baseline)) {
      session.slot.moistureTarget = baselineMinus(baseline, config.baselineY);
    } else {
      session.slot.flags &= static_cast<uint8_t>(~FEED_SLOT_HAS_MOISTURE_TARGET);
      session.slot.moistureTarget = 0;
    }
  }
  session.startMillis = millis();
  session.maxVolumeMs = volumeMlToMs(slot->maxVolumeMl, config.dripperMsPerLiter);
  session.dailyTotalAtStart = (config.maxDailyWaterMl > 0) ? getDailyFeedTotalMlNow() : 0;
  session.pumpOn = true;
  session.pulseOnMs = static_cast<uint32_t>(config.pulseOnSeconds) * 1000UL;
  session.pulseOffMs = static_cast<uint32_t>(config.pulseOffSeconds) * 1000UL;
  session.pulseEnabled = session.pulseOnMs > 0 && session.pulseOffMs > 0;
  session.pulseRemainingMs = session.pulseEnabled ? session.pulseOnMs : 0;
  session.lastPulseUpdateAt = session.startMillis;
  session.onElapsedMs = 0;
  session.runoffStartAt = 0;
  session.soilBeforePercent = soilMoistureAsPercentage(getSoilMoisture());
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

  uint8_t soilPercent = soilMoistureAsPercentage(getSoilMoisture());
  unsigned long now = millis();

  initLogEntryCommon(&newLogEntry, 1, reasonCode,
                     timeTriggered ? LOG_START_TIME : LOG_START_MOISTURE,
                     slotIndex, 0, soilPercent, soilPercent);
  newLogEntry.millisStart = now;
  newLogEntry.millisEnd = now;

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
  updatePulse(now);
  closeLineIn();
  session.pumpOn = false;

  uint16_t realtimeAvg = soilSensorGetRealtimeAvg();
  uint8_t soilAfterPercent = soilMoistureAsPercentage(realtimeAvg);
  setSoilSensorLazySeed(realtimeAvg);
  uint32_t deliveredMs = session.onElapsedMs;
  uint16_t feedMl = msToVolumeMl(deliveredMs, config.dripperMsPerLiter);
  lastFeedMl = feedMl;

  uint8_t logFlags = 0;
  if (slotFlag(&session.slot, FEED_SLOT_RUNOFF_REQUIRED) &&
      !(session.flags & kSessionFlagRunoffSeen)) {
    logFlags |= LOG_FLAG_RUNOFF_MISSING;
  }
  if (slotFlag(&session.slot, FEED_SLOT_RUNOFF_AVOID) &&
      (session.flags & kSessionFlagRunoffSeen)) {
    logFlags |= LOG_FLAG_RUNOFF_UNEXPECTED;
  }
  if (slotFlag(&session.slot, FEED_SLOT_BASELINE_SETTER)) {
    logFlags |= LOG_FLAG_BASELINE_SETTER;
  }
  if (session.flags & kSessionFlagRunoffSeen) {
    logFlags |= LOG_FLAG_RUNOFF_SEEN;
  }
  initLogEntryCommon(&newLogEntry, 1, static_cast<uint8_t>(reason), session.startReason,
                     session.slotIndex, logFlags, session.soilBeforePercent, soilAfterPercent);
  newLogEntry.millisStart = session.startMillis;
  newLogEntry.millisEnd = now;
  if (logFlags & LOG_FLAG_RUNOFF_ANY) runoffWarning = 1;
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
  if ((logFlags & LOG_FLAG_BASELINE_SETTER) && (logFlags & LOG_FLAG_RUNOFF_SEEN)) {
    baselineSetCandidate(getCurrentLogSlot(), &newLogEntry);
  }

  millisAtEndOfLastFeed = now;

  session.active = false;
  sessionForceFeed = false;
}

static void tickActiveFeed(unsigned long now) {
  updatePulse(now);
  uint8_t moisturePercent = soilMoistureAsPercentage(getSoilMoisture());
  bool moistureReady = soilSensorRealtimeReady();

  uint32_t deliveredMs = session.onElapsedMs;
  bool dailyStop = false;
  if (!sessionForceFeed) {
    uint16_t maxDaily = config.maxDailyWaterMl;
    if (maxDaily > 0) {
      if (session.dailyTotalAtStart >= maxDaily) {
        dailyStop = true;
      } else {
        uint16_t deliveredMl = msToVolumeMl(deliveredMs, config.dripperMsPerLiter);
        if (deliveredMl >= static_cast<uint16_t>(maxDaily - session.dailyTotalAtStart)) dailyStop = true;
      }
    }
  }
  bool maxReached = (session.maxVolumeMs > 0) && (deliveredMs >= session.maxVolumeMs);

  bool moistureStop = false;
  if (slotFlag(&session.slot, FEED_SLOT_HAS_MOISTURE_TARGET) && moistureReady) {
    moistureStop = moisturePercent >= session.slot.moistureTarget;
  }

  bool runoffNow = false;
  if (slotFlag(&session.slot, FEED_SLOT_RUNOFF_REQUIRED) ||
      slotFlag(&session.slot, FEED_SLOT_RUNOFF_AVOID)) {
    runoffNow = runoffDetected();
    if (runoffNow) session.flags |= kSessionFlagRunoffSeen;
  }

  bool runoffStop = false;
  if (slotFlag(&session.slot, FEED_SLOT_RUNOFF_REQUIRED)) {
      if (runoffNow) {
        if (session.runoffStartAt == 0) session.runoffStartAt = now;
        unsigned long holdMs = ticksToMs(session.slot.runoffHold5s);
        if (holdMs < 1000UL) holdMs = 1000UL;
        if (session.runoffStartAt && (now - session.runoffStartAt) >= holdMs) {
          runoffStop = true;
        }
      } else {
      session.runoffStartAt = 0;
    }
  }

  if (dailyStop) {
    stopFeed(FEED_STOP_MAX_DAILY_FEED_REACHED, now);
    return;
  }

  if (maxReached) {
    stopFeed(FEED_STOP_MAX_RUNTIME, now);
    return;
  }

  if (moistureStop || runoffStop) {
    stopFeed(moistureStop ? FEED_STOP_MOISTURE : FEED_STOP_RUNOFF, now);
    return;
  }
}

static bool startConditionsMet(const FeedSlot *slot, bool *timeOkOut, bool *moistureOkOut) {
  bool hasTime = slotFlag(slot, FEED_SLOT_HAS_TIME_WINDOW);
  bool hasMoisture = slotFlag(slot, FEED_SLOT_HAS_MOISTURE_BELOW);
  if (hasMoisture && !soilSensorReady()) return false;

  bool timeOk = false;
  if (hasTime && updateRtcCache()) {
    if (rtcMinutes != lastEvaluatedRtcMinutes) {
      lastEvaluatedRtcMinutes = rtcMinutes;
      lastOffsetMinutes = minutesSinceLightsOn();
      lastOffsetValid = true;
    }
    if (lastOffsetValid) {
      if (slot->windowDurationMinutes == 0) {
        timeOk = (lastOffsetMinutes == slot->windowStartMinutes);
      } else {
        timeOk = rtcIsWithinWindow(lastOffsetMinutes, slot->windowStartMinutes, slot->windowDurationMinutes);
      }
    }
  } else if (hasTime) {
    lastOffsetValid = false;
  }

  bool moistureOk = false;
  if (hasMoisture) {
    uint8_t moisturePercent = soilMoistureAsPercentage(getSoilMoisture());
    uint8_t threshold = slot->moistureBelow;
    if (threshold == kMoistureBaselineSentinel) {
      uint8_t baseline = 0;
      if (!baselineGetPercent(&baseline)) return false;
      threshold = baselineMinus(baseline, config.baselineX);
    }
    moistureOk = moisturePercent <= threshold;
  }

  if ((hasTime && !timeOk) || (hasMoisture && !moistureOk)) return false;

  if (slot->minGapMinutes > 0 && millisAtEndOfLastFeed) {
    unsigned long minDelayMs = (unsigned long)slot->minGapMinutes * 60000UL;
    if ((millis() - millisAtEndOfLastFeed) < minDelayMs) return false;
  }

  if (timeOkOut) *timeOkOut = timeOk;
  if (moistureOkOut) *moistureOkOut = moistureOk;
  return true;
}

static void maybeStartFeed() {
  bool rtcOk = updateRtcCache();
  uint16_t on = config.lightsOnMinutes;
  uint16_t off = config.lightsOffMinutes;
  if (on > 1439) on = 0;
  if (off > 1439) off = 0;
  if (on == off) return;
  if (rtcOk) {
    uint16_t duration = (off >= on) ? (off - on) : static_cast<uint16_t>(1440 - on + off);
    if (!rtcIsWithinWindow(rtcMinutes, on, duration)) return;
  }

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

    startFeed(i, &slot, timeTriggered, false);
    break;
  }
  }
}

void feedingForceFeed(uint8_t slotIndex) {
  if (slotIndex >= FEED_SLOT_COUNT) return;
  if (session.active) return;

  FeedSlot slot;
  unpackFeedSlot(&slot, config.feedSlotsPacked[slotIndex]);
  startFeed(slotIndex, &slot, false, true);
  if (session.active) {
    sessionForceFeed = true;
    session.startReason = LOG_START_USER;
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
      if (!sessionForceFeed) {
        stopFeed(FEED_STOP_DISABLED, now);
        return;
      }
    } else {
      return;
    }
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

bool feedingRunoffWarning() {
  return runoffWarning != 0;
}

void feedingClearRunoffWarning() {
  runoffWarning = 0;
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
  outStatus->maxVolumeMl = session.slot.maxVolumeMl;
  uint32_t onSeconds = session.onElapsedMs / 1000UL;
  if (onSeconds > UINT16_MAX) onSeconds = UINT16_MAX;
  outStatus->elapsedSeconds = static_cast<uint16_t>(onSeconds);
  return true;
}

void feedingBaselineInit() {
  LogEntry latestSetter = {};
  LogEntry latestWithBaseline = {};
  LogEntry latestFeed = {};
  int16_t latestSetterSlot = -1;
  int16_t latestBaselineSlot = -1;

  baselineValid = false;
  baselineFromLatest = true;
  baselinePercent = 0;
  baselineCandidateSlot = -1;
  baselineCandidateEndMinutes = 0;
  baselineCandidateValid = false;
  lastBaselineWindowAt = 0;

  findLatestBaselineEntries(&latestSetter, &latestSetterSlot,
                            &latestWithBaseline, &latestBaselineSlot,
                            &latestFeed);

  if (latestBaselineSlot >= 0 && latestWithBaseline.baselinePercent != LOG_BASELINE_UNSET) {
    baselinePercent = latestWithBaseline.baselinePercent;
    baselineValid = true;
  }

  if (latestSetterSlot >= 0) {
    baselineCandidateSlot = latestSetterSlot;
    baselineCandidateValid = logEntryEndMinutes(&latestSetter, &baselineCandidateEndMinutes);
    baselineFromLatest = (latestSetter.baselinePercent != LOG_BASELINE_UNSET);
  }

  if (latestFeed.entryType == 1 && latestFeed.feedMl > 0) {
    uint32_t endMinutes = 0;
    uint32_t nowMinutes = 0;
    lastFeedMl = latestFeed.feedMl;
    if (logEntryEndMinutes(&latestFeed, &endMinutes) && getNowMinutes(&nowMinutes) && nowMinutes >= endMinutes) {
      uint32_t delta = nowMinutes - endMinutes;
      if (delta <= 4095) {
        millisAtEndOfLastFeed = millis() - (unsigned long)delta * 60000UL;
      }
    }
  }
}

void feedingBaselineTick() {
  if (!baselineCandidateValid || baselineFromLatest) return;
  if (baselineCandidateSlot < 0) return;
  unsigned long windowEndAt = soilSensorLastWindowEndAt();
  if (!windowEndAt || windowEndAt == lastBaselineWindowAt) return;
  lastBaselineWindowAt = windowEndAt;
  if (!soilSensorReady()) return;

  uint32_t nowMinutes = 0;
  if (!getNowMinutes(&nowMinutes)) return;
  if (nowMinutes < baselineCandidateEndMinutes) return;
  uint32_t delta = nowMinutes - baselineCandidateEndMinutes;
  if (delta < 30 || delta > 60) return;

  uint8_t percent = soilMoistureAsPercentage(getSoilMoisture());
  if (!patchLogBaselinePercent(baselineCandidateSlot, percent)) return;
  baselinePercent = percent;
  baselineValid = true;
  baselineFromLatest = true;
}

bool feedingGetBaselinePercent(uint8_t *outPercent) {
  return baselineGetPercent(outPercent);
}

bool feedingHasBaselineSetter() {
  return baselineCandidateSlot >= 0;
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
