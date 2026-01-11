#include <Arduino.h>
#include <string.h>
#include "feeding.h"
#include "config.h"
#include "feedSlots.h"
#include "feedingUtils.h"
#include "messages.h"
#include "ui.h"
#include "LiquidCrystal_I2C.h"

extern Config config;
extern LiquidCrystal_I2C lcd;
extern Button *pressedButton;
extern Button upButton;
extern Button downButton;
extern Button leftButton;
extern Button rightButton;
extern Button okButton;

namespace {
const uint16_t kMinVolumeMl = 50;   // minimum adjustable volume per feed
const uint16_t kMaxVolumeMl = 5000; // upper bound for volume
const uint8_t kMoistureBaselineSentinel = 127;
const uint8_t kBaselineMin = 2;
const uint8_t kBaselineMax = 20;
const uint8_t kBaselineStep = 2;
static char slotListLabels[FEED_SLOT_COUNT][LABEL_LENGTH + 1];

static char *append_P(char *dst, MsgId src) {
  if (src == MSG_LITTLE) {
    *dst = '\0';
    return dst;
  }
  uint16_t addr = msgOffset(src);
  while (true) {
    char c = msgReadByte(addr++);
    if (!c) break;
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

static void formatOffset(char *out, uint16_t minutes) {
  if (minutes > 1439) minutes = 1439;
  uint8_t hour = minutes / 60;
  uint8_t minute = minutes % 60;

  out[0] = '+';
  out[1] = static_cast<char>('0' + (hour / 10));
  out[2] = static_cast<char>('0' + (hour % 10));
  out[3] = ':';
  out[4] = static_cast<char>('0' + (minute / 10));
  out[5] = static_cast<char>('0' + (minute % 10));
  out[6] = '\0';
}

static uint16_t clampVolume(int16_t ml) {
  if (ml < 0) return kMinVolumeMl;
  uint16_t value = static_cast<uint16_t>(ml);
  if (value < kMinVolumeMl) value = kMinVolumeMl;
  if (value > kMaxVolumeMl) value = kMaxVolumeMl;
  return value;
}

static void setSlotFlag(FeedSlot *slot, uint8_t flag, bool enabled) {
  if (enabled) slot->flags |= flag;
  else slot->flags &= static_cast<uint8_t>(~flag);
}

static void loadFeedSlot(uint8_t index, FeedSlot *slot) {
  unpackFeedSlot(slot, config.feedSlotsPacked[index]);
}

static bool saveFeedSlot(uint8_t index, const FeedSlot *slot) {
  uint8_t packed[FEED_SLOT_PACKED_SIZE];
  packFeedSlot(packed, slot);
  if (memcmp(packed, config.feedSlotsPacked[index], FEED_SLOT_PACKED_SIZE) != 0) {
    memcpy(config.feedSlotsPacked[index], packed, FEED_SLOT_PACKED_SIZE);
    saveConfig();
    return true;
  }
  return false;
}

enum MoistureMode : uint8_t {
  MOIST_MODE_OFF = 0,
  MOIST_MODE_PERCENT = 1,
  MOIST_MODE_BASELINE = 2
};

static int8_t selectMoistureMode(MsgId header, uint8_t initialMode, MsgId baselineLabel) {
  setChoices_P(baselineLabel, MOIST_MODE_BASELINE, MSG_PERCENT, MOIST_MODE_PERCENT, MSG_NO, MOIST_MODE_OFF);
  setChoicesHeader_P(header);
  return selectChoice(3, initialMode);
}

static void buildSummaryLine0(char *out, const FeedSlot *slot, const char *slotName) {
  char *p = out;
  if (slotName && slotName[0]) p = append_R(p, slotName);
  else p = append_P(p, MSG_SLOT_EMPTY);
  *p++ = ' ';
  *p++ = '(';
  p = append_P(p, slotFlag(slot, FEED_SLOT_ENABLED) ? MSG_ON : MSG_OFF);
  *p++ = ')';
  if (slotFlag(slot, FEED_SLOT_BASELINE_SETTER)) {
    *p++ = ' ';
    p = append_P(p, MSG_SETTR);
  }
  *p = '\0';
}

static void buildSlotListEntry(uint8_t index, const FeedSlot *slot, char out[LABEL_LENGTH + 1]) {
  char *p = out;
  const char *name = config.feedSlotNames[index];
  if (name && name[0]) {
    p = append_R(p, name);
  } else {
    p = append_P(p, MSG_SLOT_EMPTY);
  }

  bool enabled = slotFlag(slot, FEED_SLOT_ENABLED);
  if ((p - out) < LABEL_LENGTH - 5) {
    *p++ = ' ';
    if (enabled) *p++ = ' ';
    *p++ = '(';
    p = append_P(p, enabled ? MSG_ON_UPPER : MSG_OFF);
    *p++ = ')';
  }
  *p = '\0';
}

static void buildStartSummaryLine(char *out, const FeedSlot *slot) {
  char *p = out;
  bool hasTime = slotFlag(slot, FEED_SLOT_HAS_TIME_WINDOW);
  bool hasMoist = slotFlag(slot, FEED_SLOT_HAS_MOISTURE_BELOW);
  uint8_t baseline = 0;

  if (hasMoist && slot->moistureBelow == kMoistureBaselineSentinel) {
    feedingGetBaselinePercent(&baseline);
  }

  if (!hasTime && !hasMoist) {
    p = append_P(p, MSG_NA);
  } else {
    if (hasTime) {
      char start[7];
      char duration[6];
      formatOffset(start, slot->windowStartMinutes);
      *p++ = 'T';
      p = append_R(p, start);
      if (slot->windowDurationMinutes != 0) {
        formatTime(duration, slot->windowDurationMinutes);
        *p++ = '/';
        p = append_R(p, duration);
      }
      if (hasMoist) *p++ = ' ';
    }
    if (hasMoist) {
      p = append_P(p, MSG_MOIST_LT);
      if (slot->moistureBelow == kMoistureBaselineSentinel) {
        uint8_t value = baselineMinus(baseline, config.baselineX);
        if (value == 0) {
          p = append_P(p, MSG_DASHES_2);
        } else {
          p = appendNumber(p, value);
          p = append_P(p, MSG_PERCENT);
        }
      } else {
        p = appendNumber(p, slot->moistureBelow);
        p = append_P(p, MSG_PERCENT);
      }
    }
  }
  *p = '\0';
}

static void buildStopSummaryLine(char *out, const FeedSlot *slot, uint8_t slotIndex) {
  char *p = out;
  p = append_P(p, MSG_STOP_COLON);
  *p++ = ' ';
  bool hasTarget = slotFlag(slot, FEED_SLOT_HAS_MOISTURE_TARGET);
  bool mustRunoff = slotFlag(slot, FEED_SLOT_RUNOFF_REQUIRED);
  bool hasRunoff = mustRunoff;
  uint8_t pref = config.runoffExpectation[slotIndex];
  uint8_t baseline = 0;

  if (hasTarget && slot->moistureTarget == kMoistureBaselineSentinel) {
    feedingGetBaselinePercent(&baseline);
  }

  if (!hasTarget && !hasRunoff) {
    p = append_P(p, MSG_NA);
  } else {
    if (hasTarget) {
      p = append_P(p, MSG_MOIST_GT);
      if (slot->moistureTarget == kMoistureBaselineSentinel) {
        uint8_t value = baselineMinus(baseline, config.baselineY);
        if (value == 0) {
          p = append_P(p, MSG_DASHES_2);
        } else {
          p = appendNumber(p, value);
          p = append_P(p, MSG_PERCENT);
        }
      } else {
        p = appendNumber(p, slot->moistureTarget);
        p = append_P(p, MSG_PERCENT);
      }
      if (hasRunoff) *p++ = ' ';
    }
    if (hasRunoff) {
      p = append_P(p, MSG_RUNOFF_REQUIRED);
      if (pref) *p++ = (pref == 1) ? '+' : '-';
    }
  }
  if (!hasRunoff && pref) {
    *p++ = ' ';
    *p++ = 'R';
    *p++ = (pref == 1) ? '+' : '-';
  }
  *p = '\0';
}

static void buildFeedSummaryLine(char *out, const FeedSlot *slot) {
  char *p = out;
  p = append_P(p, MSG_MAX_COLON);
  if (slot->maxVolumeMl > 0) {
    p = appendNumber(p, slot->maxVolumeMl);
    p = append_P(p, MSG_ML);
  } else {
    p = append_P(p, MSG_NA);
  }

  while ((p - out) < 13) *p++ = ' ';

  p = append_P(p, MSG_D_COLON);
  p = appendNumber(p, slot->minGapMinutes);
  *p++ = 'm';
  *p = '\0';
}

static void drawSummaryLine(uint8_t row, const char *line) {
  lcdClearLine(row);
  lcdSetCursor(0, row);
  lcd.print(line);
}

static bool showSlotSummary(const FeedSlot *slot, const char *slotName, uint8_t slotIndex) {
  char line[LABEL_LENGTH + 1];

  lcdClear();

  buildSummaryLine0(line, slot, slotName);
  drawSummaryLine(0, line);

  buildStartSummaryLine(line, slot);
  drawSummaryLine(1, line);

  buildStopSummaryLine(line, slot, slotIndex);
  drawSummaryLine(2, line);

  buildFeedSummaryLine(line, slot);
  drawSummaryLine(3, line);

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

  uint16_t initialStart = slot->windowStartMinutes;
  if (initialStart > 1439) initialStart = 1439;
  uint16_t newStartMinute = initialStart;
  if (!inputTime_P(MSG_START_CONDITIONS, MSG_L_PLUS_START, initialStart, &newStartMinute)) {
    return false;
  }

  uint16_t initialDuration = slot->windowDurationMinutes;
  if (!slotFlag(slot, FEED_SLOT_HAS_TIME_WINDOW)) initialDuration = 0;
  if (initialDuration > 1439) initialDuration = 1439;
  uint16_t newDurationMinutes = initialDuration;
  if (!inputTime_P(MSG_START_CONDITIONS, MSG_CHECK_FOR, initialDuration, &newDurationMinutes)) {
    return false;
  }

  if (newDurationMinutes > 1439) newDurationMinutes = 1439;

  slot->windowStartMinutes = newStartMinute;
  slot->windowDurationMinutes = newDurationMinutes;
  setSlotFlag(slot, FEED_SLOT_HAS_TIME_WINDOW, true);
  return true;
}

static bool editMoistureBelow(FeedSlot *slot) {
  uint8_t initialMode = MOIST_MODE_OFF;
  if (slotFlag(slot, FEED_SLOT_HAS_MOISTURE_BELOW)) {
    initialMode = (slot->moistureBelow == kMoistureBaselineSentinel) ? MOIST_MODE_BASELINE : MOIST_MODE_PERCENT;
  }

  int8_t mode = selectMoistureMode(MSG_MOIST_BELOW, initialMode, MSG_BASELINE_X);
  if (mode == -1) return false;
  if (mode == MOIST_MODE_OFF) {
    setSlotFlag(slot, FEED_SLOT_HAS_MOISTURE_BELOW, false);
    return true;
  }
  if (mode == MOIST_MODE_BASELINE) {
    slot->moistureBelow = kMoistureBaselineSentinel;
    setSlotFlag(slot, FEED_SLOT_HAS_MOISTURE_BELOW, true);
    return true;
  }

  uint8_t initial = slot->moistureBelow;
  if (!slotFlag(slot, FEED_SLOT_HAS_MOISTURE_BELOW) || initial == kMoistureBaselineSentinel) initial = 50;
  if (initial > 100) initial = 100;
  int16_t percent = inputNumber_P(MSG_MOIST_BELOW, static_cast<int16_t>(initial), 5, 0, 100, MSG_PERCENT, MSG_START_CONDITIONS);
  if (percent < 0) return false;
  if (percent > 100) percent = 100;

  slot->moistureBelow = static_cast<uint8_t>(percent);
  setSlotFlag(slot, FEED_SLOT_HAS_MOISTURE_BELOW, true);
  return true;
}

static bool editMinBetweenFeeds(FeedSlot *slot) {
  uint16_t initial = slot->minGapMinutes;
  if (!slotFlag(slot, FEED_SLOT_HAS_MIN_SINCE)) initial = 60;
  if (initial > 4095) initial = 4095;
  int16_t minutes = inputNumber_P(MSG_MIN_SINCE, static_cast<int16_t>(initial), 20, 0, 4095, MSG_MINUTES, MSG_FEED_OVERRIDES);
  if (minutes < 0) return false;
  if (minutes > 4095) minutes = 4095;

  slot->minGapMinutes = static_cast<uint16_t>(minutes);
  setSlotFlag(slot, FEED_SLOT_HAS_MIN_SINCE, true);
  return true;
}

static bool editMoistureTarget(FeedSlot *slot) {
  uint8_t initialMode = MOIST_MODE_OFF;
  if (slotFlag(slot, FEED_SLOT_HAS_MOISTURE_TARGET)) {
    initialMode = (slot->moistureTarget == kMoistureBaselineSentinel) ? MOIST_MODE_BASELINE : MOIST_MODE_PERCENT;
  }

  int8_t mode = selectMoistureMode(MSG_MOIST_TARGET, initialMode, MSG_BASELINE_Y);
  if (mode == -1) return false;
  if (mode == MOIST_MODE_OFF) {
    setSlotFlag(slot, FEED_SLOT_HAS_MOISTURE_TARGET, false);
    return true;
  }
  if (mode == MOIST_MODE_BASELINE) {
    slot->moistureTarget = kMoistureBaselineSentinel;
    setSlotFlag(slot, FEED_SLOT_HAS_MOISTURE_TARGET, true);
    return true;
  }

  uint8_t initial = slot->moistureTarget;
  if (!slotFlag(slot, FEED_SLOT_HAS_MOISTURE_TARGET) || initial == kMoistureBaselineSentinel) initial = 50;
  if (initial > 100) initial = 100;
  int16_t percent = inputNumber_P(MSG_MOIST_TARGET, static_cast<int16_t>(initial), 5, 0, 100, MSG_PERCENT, MSG_END_CONDITIONS);
  if (percent < 0) return false;
  if (percent > 100) percent = 100;

  slot->moistureTarget = static_cast<uint8_t>(percent);
  setSlotFlag(slot, FEED_SLOT_HAS_MOISTURE_TARGET, true);
  return true;
}

static bool editMaxVolume(FeedSlot *slot) {
  int16_t initialMl = static_cast<int16_t>(slot->maxVolumeMl);
  if (initialMl <= 0) initialMl = 500;
  if (initialMl < kMinVolumeMl) initialMl = kMinVolumeMl;
  if (initialMl > kMaxVolumeMl) initialMl = kMaxVolumeMl;

  int16_t ml = inputNumber_P(MSG_MAX_RUNTIME, initialMl, 25, kMinVolumeMl, kMaxVolumeMl, MSG_ML, MSG_END_CONDITIONS);
  if (ml < 0) return false;

  slot->maxVolumeMl = clampVolume(ml);
  return true;
}

static bool editRunoffRequired(FeedSlot *slot, uint8_t slotIndex) {
  // Functional question: should this feed stop on runoff?
  int8_t stopOnRunoff = promptYesNoWithHeader(MSG_END_CONDITIONS, MSG_RUNOFF_PRESENT, slotFlag(slot, FEED_SLOT_RUNOFF_REQUIRED));
  if (stopOnRunoff == -1) return false;
  setSlotFlag(slot, FEED_SLOT_RUNOFF_REQUIRED, stopOnRunoff);

  // Expectation question: record intent for future warnings (does not affect control).
  enum Preference : uint8_t { PREF_NEITHER = 0, PREF_MUST = 1, PREF_AVOID = 2 };
  uint8_t currentPref = config.runoffExpectation[slotIndex];
  if (currentPref > PREF_AVOID) currentPref = PREF_NEITHER;
  if (stopOnRunoff && currentPref == PREF_NEITHER) currentPref = PREF_MUST; // default to Yes when stopping on runoff
redo_expectation:
  MsgId expectationQuestion = stopOnRunoff ? MSG_MUST_RUNOFF : MSG_AVOID_RUNOFF;
  if (stopOnRunoff) {
    setChoices_P(MSG_YES, PREF_MUST, MSG_NO, PREF_AVOID, MSG_NEITHER, PREF_NEITHER);
  } else {
    setChoices_P(MSG_YES, PREF_AVOID, MSG_NO, PREF_MUST, MSG_NEITHER, PREF_NEITHER);
  }
  setChoicesHeader_P(expectationQuestion);
  int8_t pref = selectChoice(3, currentPref);
  if (pref != -1) {
    currentPref = static_cast<uint8_t>(pref);
    config.runoffExpectation[slotIndex] = currentPref;
    setSlotFlag(slot, FEED_SLOT_RUNOFF_AVOID, pref == PREF_AVOID);
  }
  if (pref == -1) return true;
  bool baselineSetter = slotFlag(slot, FEED_SLOT_BASELINE_SETTER);
  if (stopOnRunoff && pref == PREF_MUST) {
    int8_t baseline = promptYesNoWithHeader(MSG_LITTLE, MSG_BASELINE_SETTER, baselineSetter);
    if (baseline == -1) goto redo_expectation;
    baselineSetter = baseline;
  } else {
    baselineSetter = false;
  }
  setSlotFlag(slot, FEED_SLOT_BASELINE_SETTER, baselineSetter);
  return true;
}

enum SlotStepAction : uint8_t {
  STEP_NEXT = 0,
  STEP_BACK,
  STEP_ABORT,
  STEP_SAVE
};

static bool editSlotSequence(uint8_t slotIndex, FeedSlot *slot, char *slotName) {
  enum Step : uint8_t {
  STEP_ENABLE = 0,
  STEP_NAME,
  STEP_START_TIME,
  STEP_START_MOIST,
  STEP_STOP_MOIST,
  STEP_MAX_VOLUME,
  STEP_STOP_RUNOFF,
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
      case STEP_NAME: {
        bool accepted = inputString_P(MSG_SLOT_NAME, slotName, MSG_FEEDING_MENU, true, FEED_SLOT_NAME_LENGTH);
        action = accepted ? STEP_NEXT : STEP_BACK;
        break;
      }
      case STEP_START_TIME:
        action = editTimeWindow(slot) ? STEP_NEXT : STEP_BACK;
        break;
      case STEP_START_MOIST:
        action = editMoistureBelow(slot) ? STEP_NEXT : STEP_BACK;
        break;
      case STEP_STOP_MOIST:
        action = editMoistureTarget(slot) ? STEP_NEXT : STEP_BACK;
        break;
      case STEP_MAX_VOLUME:
        action = editMaxVolume(slot) ? STEP_NEXT : STEP_BACK;
        break;
      case STEP_STOP_RUNOFF:
        action = editRunoffRequired(slot, slotIndex) ? STEP_NEXT : STEP_BACK;
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
      step = next;
      continue;
    }
    if (action == STEP_BACK) {
      if (step == STEP_ENABLE) return false;
      uint8_t prev = static_cast<uint8_t>(step - 1);
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
  char *slotName = config.feedSlotNames[slotIndex];
  uint8_t originalPref = config.runoffExpectation[slotIndex];
  char editedName[FEED_SLOT_NAME_LENGTH + 1];
  memcpy(editedName, slotName, FEED_SLOT_NAME_LENGTH + 1);

  bool edit = showSlotSummary(&slot, editedName, slotIndex);
  if (!edit) return;

  FeedSlot updated = slot;
  if (!editSlotSequence(slotIndex, &updated, editedName)) {
    config.runoffExpectation[slotIndex] = originalPref;
    return;
  }

  memcpy(slotName, editedName, FEED_SLOT_NAME_LENGTH + 1);
  bool slotChanged = saveFeedSlot(slotIndex, &updated);
  if (!slotChanged) saveConfig();
}

static bool feedingSlotsPage(bool forceFeed) {
  int8_t choice = 0;
  int8_t lastChoice = 1;

  while (choice != -1) {
    resetChoicesAndHeader();
    for (uint8_t i = 0; i < FEED_SLOT_COUNT; ++i) {
      FeedSlot slot;
      loadFeedSlot(i, &slot);
      buildSlotListEntry(i, &slot, slotListLabels[i]);
      setChoice_R(i, slotListLabels[i], static_cast<int>(i + 1));
    }

    choice = selectChoice(FEED_SLOT_COUNT, lastChoice);
    if (choice != -1) lastChoice = choice;
    if (choice != -1) {
      uint8_t slotIndex = static_cast<uint8_t>(choice - 1);
      if (forceFeed) {
        feedingResumeAfterUi();
        feedingForceFeed(slotIndex);
        return true;
      }
      viewFeedSlot(slotIndex);
    }
  }
  return false;
}

static void editBaselineSetting(uint8_t *value, MsgId label) {
  uint8_t initial = clampBaselineOffset(*value);
  int16_t input = inputNumber_P(label, static_cast<int16_t>(initial), kBaselineStep, kBaselineMin, kBaselineMax,
                                 MSG_PERCENT, MSG_FEEDING_MENU);
  if (input < 0) return;
  *value = clampBaselineOffset(static_cast<uint8_t>(input));
  saveConfig();
  lcdFlashMessage_P(MSG_DONE);
}

} /* End of namespace */

void feedingMenu() {
  int8_t choice = 0;
  int8_t lastChoice = 1;
  while (choice != -1) {
    setChoices_P(
      MSG_FEEDING_SCHEDULE, 1,
      MSG_MAX_DAILY_WATER, 2,
      MSG_BASELINE_X, 3,
      MSG_BASELINE_Y, 4);
    setChoicesHeader_P(MSG_FEEDING_MENU);
    choice = selectChoice(4, lastChoice);
    if (choice != -1) lastChoice = choice;

    if (choice == 1) {
      feedingSlotsPage(false);
    } else if (choice == 2) {
      editMaxDailyWater();
    } else if (choice == 3) {
      editBaselineSetting(&config.baselineX, MSG_BASELINE_X);
    } else if (choice == 4) {
      editBaselineSetting(&config.baselineY, MSG_BASELINE_Y);
    }
  }
}

bool feedingForceFeedMenu() {
  return feedingSlotsPage(true);
}

void editMaxDailyWater() {
  int16_t initial = static_cast<int16_t>(config.maxDailyWaterMl);
  if (initial < 100 || initial > 5000) initial = 1000;
  int16_t ml = inputNumber_P(MSG_MAX_DAILY_WATER, initial, 100, 100, 5000, MSG_ML, MSG_FEEDING_MENU);
  if (ml >= 0) {
    config.maxDailyWaterMl = static_cast<uint16_t>(ml);
    config.flags |= CONFIG_FLAG_MAX_DAILY_SET;
    saveConfig();
    lcdFlashMessage_P(MSG_DONE);
  }
}
