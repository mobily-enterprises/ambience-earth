#include <Arduino.h>
#include <string.h>
#include <avr/pgmspace.h>
#include "feeding.h"
#include "config.h"
#include "feedSlots.h"
#include "messages.h"
#include "ui.h"
#include <LiquidCrystal_I2C.h>
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
      lcdFlashMessage_P(MSG_ABORTED);
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

  if (!hasTime && !hasMoist) {
    p = append_P(p, PSTR("N/A"));
  } else {
    if (hasTime) {
      char start[5];
      formatTimeCompact(start, slot->startMinute);
      *p++ = '@';
      p = append_R(p, start);
      if (hasMoist) *p++ = ' ';
    }
    if (hasMoist) {
      p = append_P(p, PSTR("M<"));
      p = appendNumber(p, slot->moistureBelow);
      p = append_P(p, MSG_PERCENT);
    }
  }
  *p = '\0';
}

static void buildStopSummaryLine(char *out, const FeedSlot *slot) {
  char *p = out;
  p = append_P(p, PSTR("Stop: "));
  bool hasTarget = slotFlag(slot, FEED_SLOT_HAS_MOISTURE_TARGET);
  bool hasRunoff = slotFlag(slot, FEED_SLOT_RUNOFF_REQUIRED);

  if (!hasTarget && !hasRunoff) {
    p = append_P(p, PSTR("N/A"));
  } else {
    if (hasTarget) {
      p = append_P(p, PSTR("M>"));
      p = appendNumber(p, slot->moistureTarget);
      p = append_P(p, MSG_PERCENT);
      if (hasRunoff) *p++ = ' ';
    }
    if (hasRunoff) {
      p = append_P(p, PSTR("Runoff"));
    }
  }
  *p = '\0';
}

static void buildFeedSummaryLine(char *out, const FeedSlot *slot) {
  char *p = out;
  bool hasMinRuntime = slotFlag(slot, FEED_SLOT_HAS_MIN_RUNTIME);
  bool hasMaxRuntime = slot->maxRuntime5s > 0;
  bool hasGap = slotFlag(slot, FEED_SLOT_HAS_MIN_SINCE);

  if (hasMaxRuntime || hasMinRuntime) {
    uint16_t minSeconds = hasMinRuntime ? ticksToSeconds(slot->minRuntime5s) : 0;
    uint16_t maxSeconds = hasMaxRuntime ? ticksToSeconds(slot->maxRuntime5s) : 0;
    p = append_P(p, PSTR("Feed:"));
    p = appendNumber(p, minSeconds);
    *p++ = '-';
    p = appendNumber(p, maxSeconds);
    *p++ = 's';
  }

  if (hasGap) {
    if (p != out) *p++ = ' ';
    p = append_P(p, PSTR("Gap:"));
    p = appendNumber(p, slot->minSinceLastMinutes);
    *p++ = 'm';
  }
  *p = '\0';
}

static void drawTimeEntryLine(uint8_t hour, uint8_t minute, bool blinkOn, bool editHour) {
  char timeStr[6];
  formatTime(timeStr, static_cast<uint16_t>(hour) * 60u + minute);

  lcdClearLine(2);
  lcdSetCursor(0, 2);
  lcdPrint_P(PSTR("Time "));
  lcd.print(timeStr);

  if (!blinkOn) {
    uint8_t col = editHour ? 5 : 8;
    lcdSetCursor(col, 2);
    lcd.write((uint8_t)0);
    lcdSetCursor(col + 1, 2);
    lcd.write((uint8_t)0);
  }
}

static bool inputTimeWithHeader(PGM_P header, PGM_P prompt, uint16_t initialMinutes, uint16_t *outMinutes) {
  if (!outMinutes) return false;

  uint8_t hour = initialMinutes / 60;
  uint8_t minute = initialMinutes % 60;
  bool editHour = true;
  bool cursorVisible = true;
  bool displayChanged = true;
  unsigned long lastBlinkAt = 0;

  lcdClear();
  lcdPrint_P(header, 0);
  lcdPrint_P(prompt, 1);
  lcdClearLine(3);

  while (true) {
    unsigned long now = millis();
    if (now - lastBlinkAt >= 500) {
      cursorVisible = !cursorVisible;
      lastBlinkAt = now;
      displayChanged = true;
    }

    if (displayChanged) {
      drawTimeEntryLine(hour, minute, cursorVisible, editHour);
      displayChanged = false;
    }

    analogButtonsCheck();

    if (pressedButton == &upButton) {
      if (editHour) hour = static_cast<uint8_t>((hour + 1) % 24);
      else minute = static_cast<uint8_t>((minute + 1) % 60);
      displayChanged = true;
    } else if (pressedButton == &downButton) {
      if (editHour) hour = static_cast<uint8_t>((hour + 23) % 24);
      else minute = static_cast<uint8_t>((minute + 59) % 60);
      displayChanged = true;
    } else if (pressedButton == &rightButton || pressedButton == &okButton) {
      if (editHour) {
        editHour = false;
        displayChanged = true;
      } else {
        *outMinutes = static_cast<uint16_t>(hour) * 60u + minute;
        return true;
      }
    } else if (pressedButton == &leftButton) {
      if (editHour) return false;
      editHour = true;
      displayChanged = true;
    }
  }
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

  uint16_t newStartMinute = slot->startMinute;
  if (!inputTimeWithHeader(MSG_START_CONDITIONS, MSG_WINDOW, slot->startMinute, &newStartMinute)) {
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

  long int percent = inputNumber_P(MSG_MOIST_BELOW, slot->moistureBelow, 1, 0, 100, MSG_PERCENT, MSG_START_CONDITIONS);
  if (percent < 0) return false;

  slot->moistureBelow = static_cast<uint8_t>(percent);
  setSlotFlag(slot, FEED_SLOT_HAS_MOISTURE_BELOW, true);
  return true;
}

static bool editMinBetweenFeeds(FeedSlot *slot) {
  int8_t enable = promptYesNoWithHeader(MSG_FEED_OVERRIDES, MSG_MIN_SINCE, slotFlag(slot, FEED_SLOT_HAS_MIN_SINCE));
  if (enable == -1) return false;
  if (!enable) {
    setSlotFlag(slot, FEED_SLOT_HAS_MIN_SINCE, false);
    return true;
  }

  long int minutes = inputNumber_P(MSG_MIN_SINCE, slot->minSinceLastMinutes, 1, 0, 4095, MSG_MINUTES, MSG_FEED_OVERRIDES);
  if (minutes < 0) return false;

  slot->minSinceLastMinutes = static_cast<uint16_t>(minutes);
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

  long int percent = inputNumber_P(MSG_MOIST_TARGET, slot->moistureTarget, 1, 0, 100, MSG_PERCENT, MSG_END_CONDITIONS);
  if (percent < 0) return false;

  slot->moistureTarget = static_cast<uint8_t>(percent);
  setSlotFlag(slot, FEED_SLOT_HAS_MOISTURE_TARGET, true);
  return true;
}

static bool editMinRuntime(FeedSlot *slot) {
  int8_t enable = promptYesNoWithHeader(MSG_FEED_OVERRIDES, MSG_MIN_RUNTIME, slotFlag(slot, FEED_SLOT_HAS_MIN_RUNTIME));
  if (enable == -1) return false;
  if (!enable) {
    setSlotFlag(slot, FEED_SLOT_HAS_MIN_RUNTIME, false);
    return true;
  }

  long int seconds = inputNumber_P(MSG_MIN_RUNTIME, ticksToSeconds(slot->minRuntime5s), kTickSeconds,
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
  long int seconds = inputNumber_P(MSG_MAX_RUNTIME, initialSeconds, kTickSeconds, kTickSeconds,
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

static bool editSlotSequence(FeedSlot *slot) {
  int8_t enable = yesOrNo_P(MSG_ENABLE_SLOT, slotFlag(slot, FEED_SLOT_ENABLED));
  if (enable == -1) return false;
  setSlotFlag(slot, FEED_SLOT_ENABLED, enable);

  setChoices_P(MSG_CONTINUOUS, 0, MSG_PULSED, 1);
  setChoicesHeader_P(MSG_STYLE);
  int8_t style = selectChoice(2, slotFlag(slot, FEED_SLOT_PULSED) ? 1 : 0);
  if (style == -1) return false;
  setSlotFlag(slot, FEED_SLOT_PULSED, style == 1);

  if (slotFlag(slot, FEED_SLOT_PULSED)) {
    long int onSeconds = ticksToSeconds(slot->pulseOn5s);
    long int offSeconds = ticksToSeconds(slot->pulseOff5s);
    if (onSeconds <= 0) onSeconds = 20;
    if (offSeconds <= 0) offSeconds = 60;

    long int newOn = inputNumber_P(MSG_PULSE_ON, onSeconds, kTickSeconds, kTickSeconds,
                                   ticksToSeconds(kMaxPulseOnTicks), MSG_SECONDS, MSG_STYLE);
    if (newOn < 0) return false;

    long int newOff = inputNumber_P(MSG_PULSE_OFF, offSeconds, kTickSeconds, kTickSeconds,
                                    ticksToSeconds(kMaxPulseOffTicks), MSG_SECONDS, MSG_STYLE);
    if (newOff < 0) return false;

    uint16_t clampedOn = clampSeconds(newOn, kTickSeconds, ticksToSeconds(kMaxPulseOnTicks));
    uint16_t clampedOff = clampSeconds(newOff, kTickSeconds, ticksToSeconds(kMaxPulseOffTicks));
    slot->pulseOn5s = secondsToTicks(clampedOn, kMaxPulseOnTicks);
    slot->pulseOff5s = secondsToTicks(clampedOff, kMaxPulseOffTicks);
  }

  if (!editTimeWindow(slot)) return false;
  if (!editMoistureBelow(slot)) return false;

  if (!editMoistureTarget(slot)) return false;
  if (!editRunoffRequired(slot)) return false;

  if (!editMinRuntime(slot)) return false;
  if (!editMaxRuntime(slot)) return false;
  if (!editMinBetweenFeeds(slot)) return false;

  int8_t confirmSave = yesOrNo_P(MSG_SAVE_QUESTION);
  if (confirmSave == -1) return false;
  return confirmSave == 1;
}

static void viewFeedSlot(uint8_t slotIndex) {
  FeedSlot slot;
  loadFeedSlot(slotIndex, &slot);

  bool edit = showSlotSummary(slotIndex, &slot);
  if (!edit) return;

  FeedSlot updated = slot;
  if (!editSlotSequence(&updated)) return;
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

    setChoicesHeader_P(startIndex == 0 ? MSG_SLOTS_1_8 : MSG_SLOTS_9_16);
    choice = selectChoice(count, startIndex + 1);
    if (choice != -1) viewFeedSlot(static_cast<uint8_t>(choice - 1));
  }
}
}

void feedingMenu() {
  int8_t choice = 0;
  while (choice != -1) {
    setChoices_P(MSG_SLOTS_1_8, 1, MSG_SLOTS_9_16, 2);
    setChoicesHeader_P(MSG_FEEDING_MENU);
    choice = selectChoice(2, 1);
    if (choice == 1) feedingSlotsPage(0);
    else if (choice == 2) feedingSlotsPage(8);
  }
}
