#include <Arduino.h>
#include <string.h>
#include <avr/pgmspace.h>
#include "feeding.h"
#include "config.h"
#include "feedSlots.h"
#include "messages.h"
#include "ui.h"
#include "LiquidCrystal_I2C.h"
#include <AnalogButtons.h>

extern Config config;
extern LiquidCrystal_I2C lcd;
extern Button *pressedButton;
extern Button upButton;
extern Button downButton;
extern Button leftButton;
extern Button rightButton;
extern Button okButton;

namespace {
const uint8_t kSlotsPerPage = 8;
const uint8_t kTickSeconds = 5;
const uint8_t kMaxMinRuntimeTicks = 48;   // 240s
const uint8_t kMaxMaxRuntimeTicks = 120;  // 600s
const uint8_t kMaxPulseOnTicks = 12;      // 60s
const uint8_t kMaxPulseOffTicks = 120;    // 600s
const uint8_t kListLabelSize = 14;
static char slotListLabels[kSlotsPerPage][kListLabelSize];

static int8_t promptYesNoWithHeader(PGM_P header, PGM_P question, bool initialYes) {
  bool yesSelected = initialYes;

  lcdClear();
  lcdPrint_P(header, 0);
  lcdPrint_P(question, 1);

  lcdClearLine(2);
  lcdSetCursor(1, 2);
  lcdPrint_P(MSG_YES);

  lcdClearLine(3);
  lcdSetCursor(1, 3);
  lcdPrint_P(MSG_NO);

  auto drawCursor = [&]() {
    lcdSetCursor(0, 2);
    lcd.write((uint8_t)(yesSelected ? 2 : ' '));
    lcdSetCursor(0, 3);
    lcd.write((uint8_t)(yesSelected ? ' ' : 2));
  };

  drawCursor();

  while (true) {
    analogButtonsCheck();

    if (pressedButton == &okButton || pressedButton == &rightButton) {
      return yesSelected ? 1 : 0;
    }
    if (pressedButton == &leftButton) {
      return -1;
    }
    if (pressedButton == &upButton || pressedButton == &downButton) {
      yesSelected = !yesSelected;
      drawCursor();
    }
  }
}

static char *append_P(char *dst, PGM_P src) {
  char c;
  while ((c = pgm_read_byte(src++))) {
    *dst++ = c;
  }
  *dst = '\0';
  return dst;
}

static char *append_R(char *dst, const char *src) {
  while (*src) {
    *dst++ = *src++;
  }
  *dst = '\0';
  return dst;
}

static char *appendNumber(char *dst, uint16_t value) {
  char buf[6];
  uint8_t len = 0;
  do {
    buf[len++] = static_cast<char>('0' + (value % 10));
    value /= 10;
  } while (value && len < sizeof(buf));

  while (len--) {
    *dst++ = buf[len];
  }
  *dst = '\0';
  return dst;
}

static void formatTime(char *out, uint16_t minutesOfDay) {
  if (minutesOfDay > 1439) minutesOfDay = 1439;
  uint8_t hour = minutesOfDay / 60;
  uint8_t minute = minutesOfDay % 60;

  out[0] = static_cast<char>('0' + (hour / 10));
  out[1] = static_cast<char>('0' + (hour % 10));
  out[2] = ':';
  out[3] = static_cast<char>('0' + (minute / 10));
  out[4] = static_cast<char>('0' + (minute % 10));
  out[5] = '\0';
}

static void formatTimeCompact(char *out, uint16_t minutesOfDay) {
  if (minutesOfDay > 1439) minutesOfDay = 1439;
  uint8_t hour = minutesOfDay / 60;
  uint8_t minute = minutesOfDay % 60;

  out[0] = static_cast<char>('0' + (hour / 10));
  out[1] = static_cast<char>('0' + (hour % 10));
  out[2] = static_cast<char>('0' + (minute / 10));
  out[3] = static_cast<char>('0' + (minute % 10));
  out[4] = '\0';
}

static uint16_t ticksToSeconds(uint8_t ticks) {
  return static_cast<uint16_t>(ticks) * kTickSeconds;
}

static uint8_t secondsToTicks(uint16_t seconds, uint8_t maxTicks) {
  uint16_t ticks = seconds / kTickSeconds;
  if (ticks > maxTicks) ticks = maxTicks;
  return static_cast<uint8_t>(ticks);
}

static uint16_t clampSeconds(long int seconds, uint16_t minSeconds, uint16_t maxSeconds) {
  if (seconds < 0) return minSeconds;
  uint16_t value = static_cast<uint16_t>(seconds);
  if (value < minSeconds) value = minSeconds;
  if (value > maxSeconds) value = maxSeconds;
  return value;
}

static bool slotFlag(const FeedSlot *slot, uint8_t flag) {
  return (slot->flags & flag) != 0;
}

static void setSlotFlag(FeedSlot *slot, uint8_t flag, bool enabled) {
  if (enabled) slot->flags |= flag;
  else slot->flags &= static_cast<uint8_t>(~flag);
}

static void loadFeedSlot(uint8_t index, FeedSlot *slot) {
  unpackFeedSlot(slot, config.feedSlotsPacked[index]);
}

static void saveFeedSlot(uint8_t index, const FeedSlot *slot) {
  uint8_t packed[FEED_SLOT_PACKED_SIZE];
  packFeedSlot(packed, slot);
  if (memcmp(packed, config.feedSlotsPacked[index], FEED_SLOT_PACKED_SIZE) != 0) {
    memcpy(config.feedSlotsPacked[index], packed, FEED_SLOT_PACKED_SIZE);
    saveConfig();
  }
}

static void buildSlotListLabel(char *out, uint8_t slotNumber, const FeedSlot *slot) {
  char *p = out;
  p = append_P(p, MSG_SLOT);
  *p++ = ' ';
  p = appendNumber(p, slotNumber);
  *p++ = ' ';
  p = append_P(p, slotFlag(slot, FEED_SLOT_ENABLED) ? MSG_ON : MSG_OFF);
  *p = '\0';
}

static void buildSummaryLine0(char *out, uint8_t slotNumber, const FeedSlot *slot) {
  char *p = out;
  p = append_P(p, MSG_SLOT);
  *p++ = ' ';
  p = appendNumber(p, slotNumber);
  *p++ = ' ';
  *p++ = '(';
  if (!slotFlag(slot, FEED_SLOT_ENABLED)) {
    p = append_P(p, MSG_OFF);
  } else if (slotFlag(slot, FEED_SLOT_PULSED)) {
    p = append_P(p, PSTR("pulse"));
  } else {
    p = append_P(p, PSTR("cont"));
  }
  *p++ = ')';
  *p = '\0';
}

static void buildStartSummaryLine(char *out, const FeedSlot *slot) {
  char *p = out;
  p = append_P(p, PSTR("Start: "));
  bool hasTime = slotFlag(slot, FEED_SLOT_HAS_TIME_WINDOW);
  bool hasMoist = slotFlag(slot, FEED_SLOT_HAS_MOISTURE_BELOW);
  bool hasWeight = slot->weightBelowKg10 != 0;

  if (!hasTime && !hasMoist && !hasWeight) {
    p = append_P(p, PSTR("N/A"));
  } else {
    if (hasTime) {
      char start[6];
      formatTime(start, slot->startMinute);
      p = append_R(p, start);
      if (hasMoist || hasWeight) *p++ = ' ';
    }
    if (hasMoist) {
      p = append_P(p, PSTR("M<"));
      p = appendNumber(p, slot->moistureBelow);
      p = append_P(p, MSG_PERCENT);
      if (hasWeight) *p++ = ' ';
    }
    if (hasWeight) {
      p = append_P(p, PSTR("W<"));
      uint16_t v = slot->weightBelowKg10;
      p = appendNumber(p, v / 10);
      *p++ = '.';
      p = appendNumber(p, v % 10);
      p = append_P(p, PSTR("kg"));
    }
  }
  *p = '\0';
}

static void buildStopSummaryLine(char *out, const FeedSlot *slot) {
  char *p = out;
  p = append_P(p, PSTR("Stop: "));
  bool hasTarget = slotFlag(slot, FEED_SLOT_HAS_MOISTURE_TARGET);
  bool hasRunoff = slotFlag(slot, FEED_SLOT_RUNOFF_REQUIRED);
  bool hasWeightStop = slot->weightAboveKg10 != 0;

  if (!hasTarget && !hasRunoff && !hasWeightStop) {
    p = append_P(p, PSTR("N/A"));
  } else {
    if (hasTarget) {
      p = append_P(p, PSTR("M>"));
      p = appendNumber(p, slot->moistureTarget);
      p = append_P(p, MSG_PERCENT);
      if (hasRunoff || hasWeightStop) *p++ = ' ';
    }
    if (hasRunoff) {
      p = append_P(p, PSTR("Runoff"));
      if (hasWeightStop) *p++ = ' ';
    }
    if (hasWeightStop) {
      p = append_P(p, PSTR("W>"));
      uint16_t v = slot->weightAboveKg10;
      p = appendNumber(p, v / 10);
      *p++ = '.';
      p = appendNumber(p, v % 10);
      p = append_P(p, PSTR("kg"));
    }
  }
  *p = '\0';
}

static void buildFeedSummaryLine(char *out, const FeedSlot *slot) {
  char *p = out;
  bool hasMinRuntime = slotFlag(slot, FEED_SLOT_HAS_MIN_RUNTIME);
  bool hasMaxRuntime = slot->maxRuntime5s > 0;
  bool tightGap = slot->minGapMinutes >= 1000;

  if (hasMaxRuntime || hasMinRuntime) {
    uint16_t minSeconds = hasMinRuntime ? ticksToSeconds(slot->minRuntime5s) : 0;
    uint16_t maxSeconds = hasMaxRuntime ? ticksToSeconds(slot->maxRuntime5s) : 0;
    p = append_P(p, PSTR("Feed:"));
    p = appendNumber(p, minSeconds);
    *p++ = '-';
    p = appendNumber(p, maxSeconds);
    *p++ = 's';
  }

  if (p != out && !tightGap) *p++ = ' ';
  p = append_P(p, PSTR("D:"));
  p = appendNumber(p, slot->minGapMinutes);
  *p++ = 'm';
  *p = '\0';
}

static bool inputTimeWithHeader(PGM_P header, PGM_P prompt, uint16_t initialMinutes, uint16_t *outMinutes) {
  return inputTime_P(header, prompt, initialMinutes, outMinutes);
}

static bool showSlotSummary(uint8_t slotIndex, const FeedSlot *slot) {
  char line[LABEL_LENGTH + 1];
  const uint8_t slotNumber = slotIndex + 1;

  lcdClear();

  buildSummaryLine0(line, slotNumber, slot);
  lcdClearLine(0);
  lcdSetCursor(0, 0);
  lcd.print(line);

  buildStartSummaryLine(line, slot);
  lcdClearLine(1);
  lcdSetCursor(0, 1);
  lcd.print(line);

  buildStopSummaryLine(line, slot);
  lcdClearLine(2);
  lcdSetCursor(0, 2);
  lcd.print(line);

  buildFeedSummaryLine(line, slot);
  lcdClearLine(3);
  lcdSetCursor(0, 3);
  lcd.print(line);

  while (true) {
    analogButtonsCheck();
    if (pressedButton == &okButton) return true;
    if (pressedButton == &leftButton) return false;
  }
}

static bool editTimeWindow(FeedSlot *slot) {
  int8_t enable = promptYesNoWithHeader(MSG_START_CONDITIONS, MSG_WINDOW, slotFlag(slot, FEED_SLOT_HAS_TIME_WINDOW));
  if (enable == -1) return false;
  if (!enable) {
    setSlotFlag(slot, FEED_SLOT_HAS_TIME_WINDOW, false);
    return true;
  }

  uint16_t initialStart = slot->startMinute;
  if (initialStart > 1439) initialStart = 1439;
  uint16_t newStartMinute = initialStart;
  if (!inputTimeWithHeader(MSG_START_CONDITIONS, MSG_WINDOW, initialStart, &newStartMinute)) {
    return false;
  }

  slot->startMinute = newStartMinute;
  setSlotFlag(slot, FEED_SLOT_HAS_TIME_WINDOW, true);
  return true;
}

static bool editMoistureBelow(FeedSlot *slot) {
  int8_t enable = promptYesNoWithHeader(MSG_START_CONDITIONS, MSG_MOIST_BELOW, slotFlag(slot, FEED_SLOT_HAS_MOISTURE_BELOW));
  if (enable == -1) return false;
  if (!enable) {
    setSlotFlag(slot, FEED_SLOT_HAS_MOISTURE_BELOW, false);
    return true;
  }

  uint8_t initial = slot->moistureBelow;
  if (!slotFlag(slot, FEED_SLOT_HAS_MOISTURE_BELOW)) initial = 50;
  if (initial > 100) initial = 100;
  long int percent = inputNumber_P(MSG_MOIST_BELOW, initial, 5, 0, 100, MSG_PERCENT, MSG_START_CONDITIONS);
  if (percent < 0) return false;
  if (percent > 100) percent = 100;

  slot->moistureBelow = static_cast<uint8_t>(percent);
  setSlotFlag(slot, FEED_SLOT_HAS_MOISTURE_BELOW, true);
  return true;
}

static bool editMinBetweenFeeds(FeedSlot *slot) {
  uint16_t initial = slot->minGapMinutes;
  if (initial > 4095) initial = 4095;
  long int minutes = inputNumber_P(MSG_MIN_SINCE, initial, 20, 0, 4095, MSG_MINUTES, MSG_FEED_OVERRIDES);
  if (minutes < 0) return false;
  if (minutes > 4095) minutes = 4095;

  slot->minGapMinutes = static_cast<uint16_t>(minutes);
  setSlotFlag(slot, FEED_SLOT_HAS_MIN_SINCE, true);
  return true;
}

static bool editMoistureTarget(FeedSlot *slot) {
  int8_t enable = promptYesNoWithHeader(MSG_END_CONDITIONS, MSG_MOIST_TARGET, slotFlag(slot, FEED_SLOT_HAS_MOISTURE_TARGET));
  if (enable == -1) return false;
  if (!enable) {
    setSlotFlag(slot, FEED_SLOT_HAS_MOISTURE_TARGET, false);
    return true;
  }

  uint8_t initial = slot->moistureTarget;
  if (!slotFlag(slot, FEED_SLOT_HAS_MOISTURE_TARGET)) initial = 50;
  if (initial > 100) initial = 100;
  long int percent = inputNumber_P(MSG_MOIST_TARGET, initial, 5, 0, 100, MSG_PERCENT, MSG_END_CONDITIONS);
  if (percent < 0) return false;
  if (percent > 100) percent = 100;

  slot->moistureTarget = static_cast<uint8_t>(percent);
  setSlotFlag(slot, FEED_SLOT_HAS_MOISTURE_TARGET, true);
  return true;
}

static bool editWeightBelow(FeedSlot *slot) {
  int8_t enable = promptYesNoWithHeader(MSG_START_CONDITIONS, MSG_WEIGHT_BELOW, slot->weightBelowKg10 != 0);
  if (enable == -1) return false;
  if (!enable) {
    slot->weightBelowKg10 = 0;
    return true;
  }

  uint8_t initial = slot->weightBelowKg10 ? slot->weightBelowKg10 : 150; // 15.0 kg default
  if (initial > 250) initial = 250;
  long int val = inputNumber_P(MSG_WEIGHT_BELOW, initial, 5, 0, 250, MSG_LITTLE, MSG_START_CONDITIONS);
  if (val < 0) return false;
  if (val > 250) val = 250;
  slot->weightBelowKg10 = static_cast<uint8_t>(val);
  return true;
}

static bool editWeightTarget(FeedSlot *slot) {
  int8_t enable = promptYesNoWithHeader(MSG_END_CONDITIONS, MSG_WEIGHT_TARGET, slot->weightAboveKg10 != 0);
  if (enable == -1) return false;
  if (!enable) {
    slot->weightAboveKg10 = 0;
    return true;
  }

  uint8_t initial = slot->weightAboveKg10 ? slot->weightAboveKg10 : 180; // 18.0 kg default
  if (initial > 250) initial = 250;
  long int val = inputNumber_P(MSG_WEIGHT_TARGET, initial, 5, 0, 250, MSG_LITTLE, MSG_END_CONDITIONS);
  if (val < 0) return false;
  if (val > 250) val = 250;
  slot->weightAboveKg10 = static_cast<uint8_t>(val);
  return true;
}

static bool editMinRuntime(FeedSlot *slot) {
  int8_t enable = promptYesNoWithHeader(MSG_FEED_OVERRIDES, MSG_MIN_RUNTIME, slotFlag(slot, FEED_SLOT_HAS_MIN_RUNTIME));
  if (enable == -1) return false;
  if (!enable) {
    setSlotFlag(slot, FEED_SLOT_HAS_MIN_RUNTIME, false);
    return true;
  }

  long int initialSeconds = ticksToSeconds(slot->minRuntime5s);
  if (initialSeconds < kTickSeconds) initialSeconds = kTickSeconds;
  if (initialSeconds > ticksToSeconds(kMaxMinRuntimeTicks)) {
    initialSeconds = ticksToSeconds(kMaxMinRuntimeTicks);
  }
  long int seconds = inputNumber_P(MSG_MIN_RUNTIME, initialSeconds, kTickSeconds,
                                   kTickSeconds, ticksToSeconds(kMaxMinRuntimeTicks), MSG_SECONDS, MSG_FEED_OVERRIDES);
  if (seconds < 0) return false;

  uint16_t clamped = clampSeconds(seconds, kTickSeconds, ticksToSeconds(kMaxMinRuntimeTicks));
  slot->minRuntime5s = secondsToTicks(clamped, kMaxMinRuntimeTicks);
  setSlotFlag(slot, FEED_SLOT_HAS_MIN_RUNTIME, true);
  return true;
}

static bool editMaxRuntime(FeedSlot *slot) {
  long int initialSeconds = ticksToSeconds(slot->maxRuntime5s);
  if (initialSeconds <= 0) initialSeconds = 60;
  if (initialSeconds < kTickSeconds) initialSeconds = kTickSeconds;
  if (initialSeconds > ticksToSeconds(kMaxMaxRuntimeTicks)) {
    initialSeconds = ticksToSeconds(kMaxMaxRuntimeTicks);
  }
  uint16_t minBound = kTickSeconds;
  if (slotFlag(slot, FEED_SLOT_HAS_MIN_RUNTIME)) {
    minBound = ticksToSeconds(slot->minRuntime5s);
    if (minBound < kTickSeconds) minBound = kTickSeconds;
  }
  if (initialSeconds < minBound) initialSeconds = minBound;

  long int seconds = inputNumber_P(MSG_MAX_RUNTIME, initialSeconds, 15, minBound,
                                   ticksToSeconds(kMaxMaxRuntimeTicks), MSG_SECONDS, MSG_FEED_OVERRIDES);
  if (seconds < 0) return false;

  uint16_t clamped = clampSeconds(seconds, kTickSeconds, ticksToSeconds(kMaxMaxRuntimeTicks));
  slot->maxRuntime5s = secondsToTicks(clamped, kMaxMaxRuntimeTicks);
  return true;
}

static bool editRunoffRequired(FeedSlot *slot) {
  int8_t enable = promptYesNoWithHeader(MSG_END_CONDITIONS, MSG_RUNOFF_REQUIRED, slotFlag(slot, FEED_SLOT_RUNOFF_REQUIRED));
  if (enable == -1) return false;
  setSlotFlag(slot, FEED_SLOT_RUNOFF_REQUIRED, enable);
  return true;
}

static bool editRunoffHold(FeedSlot *slot) {
  if (!slotFlag(slot, FEED_SLOT_RUNOFF_REQUIRED)) return true;
  uint16_t initialSeconds = ticksToSeconds(slot->runoffHold5s);
  if (initialSeconds > 600) initialSeconds = 600;
  long int seconds = inputNumber_P(MSG_RUNOFF_HOLD, initialSeconds, kTickSeconds, 0, 600, MSG_SECONDS, MSG_END_CONDITIONS);
  if (seconds < 0) return false;
  if (seconds > 600) seconds = 600;
  slot->runoffHold5s = secondsToTicks(seconds, kMaxMaxRuntimeTicks);
  return true;
}

enum SlotStepAction : uint8_t {
  STEP_NEXT = 0,
  STEP_BACK,
  STEP_ABORT,
  STEP_SAVE
};

static SlotStepAction editPulseSettings(FeedSlot *slot) {
  if (!slotFlag(slot, FEED_SLOT_PULSED)) return STEP_NEXT;

  long int onSeconds = ticksToSeconds(slot->pulseOn5s);
  long int offSeconds = ticksToSeconds(slot->pulseOff5s);
  if (onSeconds <= 0) onSeconds = 20;
  if (offSeconds <= 0) offSeconds = 60;
  if (onSeconds > ticksToSeconds(kMaxPulseOnTicks)) onSeconds = ticksToSeconds(kMaxPulseOnTicks);
  if (offSeconds > ticksToSeconds(kMaxPulseOffTicks)) offSeconds = ticksToSeconds(kMaxPulseOffTicks);

  long int newOn = inputNumber_P(MSG_PULSE_ON, onSeconds, kTickSeconds, kTickSeconds,
                                 ticksToSeconds(kMaxPulseOnTicks), MSG_SECONDS, MSG_STYLE);
  if (newOn < 0) return STEP_BACK;

  long int newOff = inputNumber_P(MSG_PULSE_OFF, offSeconds, kTickSeconds, kTickSeconds,
                                  ticksToSeconds(kMaxPulseOffTicks), MSG_SECONDS, MSG_STYLE);
  if (newOff < 0) return STEP_BACK;

  uint16_t clampedOn = clampSeconds(newOn, kTickSeconds, ticksToSeconds(kMaxPulseOnTicks));
  uint16_t clampedOff = clampSeconds(newOff, kTickSeconds, ticksToSeconds(kMaxPulseOffTicks));
  slot->pulseOn5s = secondsToTicks(clampedOn, kMaxPulseOnTicks);
  slot->pulseOff5s = secondsToTicks(clampedOff, kMaxPulseOffTicks);
  return STEP_NEXT;
}

static bool editSlotSequence(uint8_t /*slotIndex*/, FeedSlot *slot) {
  enum Step : uint8_t {
    STEP_ENABLE = 0,
    STEP_STYLE,
    STEP_PULSE,
    STEP_START_TIME,
    STEP_START_MOIST,
    STEP_START_WEIGHT,
    STEP_STOP_MOIST,
    STEP_STOP_WEIGHT,
    STEP_STOP_RUNOFF,
    STEP_RUNOFF_HOLD,
    STEP_MIN_RUNTIME,
    STEP_MAX_RUNTIME,
    STEP_MIN_BETWEEN,
    STEP_CONFIRM
  };

  uint8_t step = STEP_ENABLE;

  while (true) {
    SlotStepAction action = STEP_NEXT;

    switch (step) {
      case STEP_ENABLE: {
        int8_t enable = yesOrNo_P(MSG_ENABLE_SLOT, slotFlag(slot, FEED_SLOT_ENABLED));
        if (enable == -1) action = STEP_BACK;
        else {
          setSlotFlag(slot, FEED_SLOT_ENABLED, enable);
          action = STEP_NEXT;
        }
        break;
      }
      case STEP_STYLE: {
        setChoices_P(MSG_CONTINUOUS, 0, MSG_PULSED, 1);
        setChoicesHeader_P(MSG_STYLE);
        int8_t style = selectChoice(2, slotFlag(slot, FEED_SLOT_PULSED) ? 1 : 0);
        if (style == -1) action = STEP_BACK;
        else {
          setSlotFlag(slot, FEED_SLOT_PULSED, style == 1);
          action = STEP_NEXT;
        }
        break;
      }
      case STEP_PULSE:
        action = editPulseSettings(slot);
        break;
      case STEP_START_TIME:
        action = editTimeWindow(slot) ? STEP_NEXT : STEP_BACK;
        break;
      case STEP_START_MOIST:
        action = editMoistureBelow(slot) ? STEP_NEXT : STEP_BACK;
        break;
      case STEP_START_WEIGHT:
        action = editWeightBelow(slot) ? STEP_NEXT : STEP_BACK;
        break;
      case STEP_STOP_MOIST:
        action = editMoistureTarget(slot) ? STEP_NEXT : STEP_BACK;
        break;
      case STEP_STOP_WEIGHT:
        action = editWeightTarget(slot) ? STEP_NEXT : STEP_BACK;
        break;
      case STEP_STOP_RUNOFF:
        action = editRunoffRequired(slot) ? STEP_NEXT : STEP_BACK;
        break;
      case STEP_RUNOFF_HOLD:
        action = editRunoffHold(slot) ? STEP_NEXT : STEP_BACK;
        break;
      case STEP_MIN_RUNTIME:
        action = editMinRuntime(slot) ? STEP_NEXT : STEP_BACK;
        break;
      case STEP_MAX_RUNTIME:
        action = editMaxRuntime(slot) ? STEP_NEXT : STEP_BACK;
        break;
      case STEP_MIN_BETWEEN:
        action = editMinBetweenFeeds(slot) ? STEP_NEXT : STEP_BACK;
        break;
      case STEP_CONFIRM: {
        int8_t confirmSave = yesOrNo_P_NoAbort(MSG_SAVE_QUESTION);
        if (confirmSave == -1) action = STEP_BACK;
        else if (confirmSave == 0) action = STEP_ABORT;
        else action = STEP_SAVE;
        break;
      }
      default:
        action = STEP_ABORT;
        break;
    }

    if (action == STEP_NEXT) {
      if (step == STEP_CONFIRM) return true;
      uint8_t next = static_cast<uint8_t>(step + 1);
      while (next == STEP_PULSE && !slotFlag(slot, FEED_SLOT_PULSED)) {
        next = static_cast<uint8_t>(next + 1);
      }
      if (next == STEP_RUNOFF_HOLD && !slotFlag(slot, FEED_SLOT_RUNOFF_REQUIRED)) {
        next = static_cast<uint8_t>(next + 1);
      }
      // Skip start/stop steps if weight/moisture disabled? (keep sequential)
      step = next;
      continue;
    }
    if (action == STEP_BACK) {
      if (step == STEP_ENABLE) return false;
      uint8_t prev = static_cast<uint8_t>(step - 1);
      while (prev == STEP_PULSE && !slotFlag(slot, FEED_SLOT_PULSED)) {
        if (prev == STEP_ENABLE) break;
        prev = static_cast<uint8_t>(prev - 1);
      }
      step = prev;
      continue;
    }
    if (action == STEP_SAVE) return true;
    if (action == STEP_ABORT) return false;
  }
}

static void viewFeedSlot(uint8_t slotIndex) {
  FeedSlot slot;
  loadFeedSlot(slotIndex, &slot);

  bool edit = showSlotSummary(slotIndex, &slot);
  if (!edit) return;

  FeedSlot updated = slot;
  if (!editSlotSequence(slotIndex, &updated)) return;
  saveFeedSlot(slotIndex, &updated);
}

static void feedingSlotsPage(uint8_t startIndex) {
  int8_t choice = 0;
  uint8_t count = FEED_SLOT_COUNT - startIndex;
  if (count > kSlotsPerPage) count = kSlotsPerPage;

  while (choice != -1) {
    for (uint8_t i = 0; i < count; ++i) {
      FeedSlot slot;
      uint8_t slotNumber = startIndex + i + 1;
      loadFeedSlot(startIndex + i, &slot);
      buildSlotListLabel(slotListLabels[i], slotNumber, &slot);
    }

    setChoices_R(slotListLabels[0], startIndex + 1,
                 slotListLabels[1], startIndex + 2,
                 slotListLabels[2], startIndex + 3,
                 slotListLabels[3], startIndex + 4,
                 slotListLabels[4], startIndex + 5,
                 slotListLabels[5], startIndex + 6,
                 slotListLabels[6], startIndex + 7,
                 slotListLabels[7], startIndex + 8);

    setChoicesHeader_P(MSG_SLOTS_1_8);
    choice = selectChoice(count, startIndex + 1);
    if (choice != -1) viewFeedSlot(static_cast<uint8_t>(choice - 1));
  }
}

} /* End of namespace */

void feedingMenu() {
  feedingSlotsPage(0);
}
