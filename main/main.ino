#include <Arduino.h>
#include "main.h"
#include "ui.h"
#include "config.h"
#include "messages.h"
#include "runoffSensor.h"
#include "pumps.h"
#include "moistureSensor.h"
#include "settings.h"
#include "feeding.h"
#include "logs.h"
#include "rtc.h"
#include "LiquidCrystal_I2C.h"
#include "volume.h"

extern LiquidCrystal_I2C lcd;
uint8_t screenCounter = 0;
const uint8_t kScreenCount = 1;

extern Config config;

LogEntry currentLogEntry;
LogEntry newLogEntry;

extern Button *pressedButton;
extern Button upButton;
extern Button leftButton;
extern Button downButton;
extern Button rightButton;
extern Button okButton;

const unsigned long idleDisplayIntervalMs = 2000;  // Periodic refresh when idle
unsigned long lastDisplayAt = 0;

unsigned long int millisAtEndOfLastFeed = 0;
uint16_t lastFeedMl = 0;
unsigned long lastButtonPressTime = 0;
bool screenSaverMode = false;
static bool forceDisplayRedraw = false;
static uint8_t screenSaverHold = 0;

static void displayFeedingStatus(bool fullRedraw);
static bool handleUiInput(unsigned long now);


static uint8_t screenSaverRand = 0xA5;

static uint8_t nextScreenSaverRand() {
  screenSaverRand = (screenSaverRand >> 1) ^ (uint8_t)(-(screenSaverRand & 1u) & 0xB8u);
  return screenSaverRand;
}

static char idleStatusChar() {
  if (feedingRunoffWarning()) return '!';
  if (!feedingIsEnabled()) return 'z';
  return '*';
}

void setup() {
  initLcd();
  initRtc();

  extern LiquidCrystal_I2C lcd;

  initialPinSetup();
 
  initRunoffSensor();
  initPumps();
  initMoistureSensor();

  // Init logs memory
  initLogs(&currentLogEntry, 4096, kMsgDataSize, 0, sizeof(currentLogEntry));

  loadConfig();

  if (!validateConfig()) {
    restoreDefaultConfig();
    saveConfig();
    wipeLogs();
  }

#ifdef WOKWI_SIM
  if (!config.kbdUp && !config.kbdDown && !config.kbdLeft && !config.kbdRight && !config.kbdOk) {
    config.kbdUp = 500;
    config.kbdDown = 300;
    config.kbdLeft = 900;
    config.kbdRight = 100;
    config.kbdOk = 700;
  }

  if (config.flags & CONFIG_FLAG_MUST_RUN_INITIAL_SETUP) {
    config.flags &= static_cast<uint8_t>(~CONFIG_FLAG_MUST_RUN_INITIAL_SETUP);
  }

  bool simConfigChanged = false;
  if ((config.flags & CONFIG_FLAG_TIME_SET) == 0) {
    config.flags |= CONFIG_FLAG_TIME_SET;
    simConfigChanged = true;
  }
  if ((config.flags & CONFIG_FLAG_LIGHTS_ON_SET) == 0) {
    config.flags |= CONFIG_FLAG_LIGHTS_ON_SET;
    simConfigChanged = true;
  }
  if ((config.flags & CONFIG_FLAG_LIGHTS_OFF_SET) == 0) {
    config.flags |= CONFIG_FLAG_LIGHTS_OFF_SET;
    simConfigChanged = true;
  }
  if ((config.flags & CONFIG_FLAG_DRIPPER_CALIBRATED) == 0) {
    config.flags |= CONFIG_FLAG_DRIPPER_CALIBRATED;
    simConfigChanged = true;
  }
  if ((config.flags & CONFIG_FLAG_MAX_DAILY_SET) == 0) {
    config.flags |= CONFIG_FLAG_MAX_DAILY_SET;
    simConfigChanged = true;
  }
  if ((config.flags & CONFIG_FLAG_PULSE_SET) == 0) {
    config.flags |= CONFIG_FLAG_PULSE_SET;
    simConfigChanged = true;
  }

  if (config.lightsOnMinutes > 1439 || config.lightsOnMinutes == 0) {
    config.lightsOnMinutes = 6 * 60;
    simConfigChanged = true;
  }
  if (config.lightsOffMinutes > 1439 || config.lightsOffMinutes == 0) {
    config.lightsOffMinutes = 18 * 60;
    simConfigChanged = true;
  }
  if (config.maxDailyWaterMl == 0) {
    config.maxDailyWaterMl = 1000;
    simConfigChanged = true;
  }
  if (config.moistSensorCalibrationDry == 1024) {
    config.moistSensorCalibrationDry = 800;
    simConfigChanged = true;
  }
  if (config.moistSensorCalibrationSoaked == 0) {
    config.moistSensorCalibrationSoaked = 300;
    simConfigChanged = true;
  }
  if (config.dripperMsPerLiter == 0) {
    config.dripperMsPerLiter = 600000;
    simConfigChanged = true;
  }
  if (config.pulseTargetUnits < 5 || config.pulseTargetUnits > 50) {
    config.pulseTargetUnits = 20;
    simConfigChanged = true;
  }
  if (config.pulseOnSeconds == 0 && config.pulseOffSeconds == 0) {
    uint8_t onSec = 0;
    uint8_t offSec = 0;
    if (config.pulseTargetUnits >= 5 && config.pulseTargetUnits <= 50 && config.dripperMsPerLiter) {
      uint32_t fullRate = 3600000000UL / config.dripperMsPerLiter;
      uint32_t targetRate = (uint32_t)config.pulseTargetUnits * 200UL;
      if (fullRate > targetRate) {
        uint32_t diff = fullRate - targetRate;
        onSec = 10;
        uint32_t offCalc = (uint32_t)onSec * diff / targetRate;
        if (offCalc > 60) {
          onSec = 5;
          offCalc = (uint32_t)onSec * diff / targetRate;
        }
        if (offCalc > 255) offCalc = 255;
        offSec = static_cast<uint8_t>(offCalc);
      }
    }
    if (config.pulseOnSeconds != onSec || config.pulseOffSeconds != offSec) {
      config.pulseOnSeconds = onSec;
      config.pulseOffSeconds = offSec;
      simConfigChanged = true;
    }
  }
  if (simConfigChanged) {
    saveConfig();
  }
#else
  bool forceButtonsSetup = analogRead(BUTTONS_PIN) <= 10;

  if (forceButtonsSetup ||
      (config.flags & CONFIG_FLAG_MUST_RUN_INITIAL_SETUP) ||
      (!config.kbdUp && !config.kbdDown && !config.kbdLeft &&
       !config.kbdRight && !config.kbdOk)) {
    runButtonsSetup();
    config.flags &= static_cast<uint8_t>(~CONFIG_FLAG_MUST_RUN_INITIAL_SETUP);
    saveConfig();
  }
#endif

  // runButtonsSetup();

  initializeButtons();

  createBootLogEntry();
  goToLatestSlot();

  feedingBaselineInit();

  setSoilSensorLazy();
}


void loop() {
  // Main loop phases: feed tick -> ui task (if any) -> handle input -> periodic display -> background sensors/logging.
  if (!initialSetupComplete()) {
    feedingPauseForUi();
    initialSetupMenu();
    feedingResumeAfterUi();
    forceDisplayRedraw = true;
    return;
  }
  feedingTick();
  unsigned long currentMillis = millis();

  bool inputRendered = handleUiInput(currentMillis);

  if (!screenSaverHold && currentMillis - lastButtonPressTime > SCREENSAVER_TRIGGER_TIME) screenSaverModeOn();

  // Necessary to run this consistently so that
  // lazy reads work
  runSoilSensorLazyReadings();
  feedingBaselineTick();

  maybeLogValues();

  unsigned long displayInterval = feedingIsActive() ? 500UL : idleDisplayIntervalMs;
  if (!inputRendered && currentMillis - lastDisplayAt >= displayInterval) {
    lastDisplayAt = currentMillis;
    displayInfo(screenCounter);
    // int pinValue = digitalRead(12);
    // Serial.print(digitalRead(12));
  }
  delay(10); // Let the CPU breathe
}

static bool handleUiInput(unsigned long now) {
  // Centralized button handling: wake screensaver, navigate screens, open menu, and render immediately.
  analogButtonsCheck();
  if (pressedButton == nullptr) return false;

  lastButtonPressTime = now;

  if (screenSaverMode) {
    screenSaverModeOff();
    forceDisplayRedraw = true;
    displayInfo(screenCounter); // immediate redraw on wake
    pressedButton = nullptr;    // swallow wake press
    return true;
  }

  if (pressedButton == &okButton) {
    feedingPauseForUi();
    mainMenu();
    feedingResumeAfterUi();
    pressedButton = nullptr;
    return true;
  }

  if (pressedButton == &upButton) {
    screenCounter = (screenCounter - 1 + kScreenCount) % kScreenCount;
  } else if (pressedButton == &downButton) {
    screenCounter = (screenCounter + 1) % kScreenCount;
  }

  if (forceDisplayRedraw || pressedButton == &upButton || pressedButton == &downButton) {
    displayInfo(screenCounter);
    lastDisplayAt = now;
    pressedButton = nullptr;
    return true;
  }

  pressedButton = nullptr;
  return false;
}

void maybeLogValues() {
    static unsigned long lastRunTime = 0; // Stores the last time the function was executed
    unsigned long currentMillis = millis(); // Get the current time in milliseconds

    if (!soilSensorReady()) return;

    // Check if LOG_VALUES_INTERVAL has passed since the last run
    if (currentMillis - lastRunTime >= LOG_VALUES_INTERVAL) {  
      lastRunTime = currentMillis; // Update the last run time

      uint8_t soilMoisture = soilMoistureAsPercentage(getSoilMoisture());
      clearLogEntry((void *)&newLogEntry);
      newLogEntry.entryType = 2;  // VALUES
      newLogEntry.millisStart = millis();
      newLogEntry.stopReason = LOG_STOP_NONE;
      newLogEntry.startReason = LOG_START_NONE;
      newLogEntry.slotIndex = 0;
      newLogEntry.flags = 0;
      newLogEntry.soilMoistureBefore = soilMoisture;
      newLogEntry.soilMoistureAfter = 0;
      rtcStamp(&newLogEntry.startYear, &newLogEntry.startMonth, &newLogEntry.startDay,
               &newLogEntry.startHour, &newLogEntry.startMinute);
      newLogEntry.endYear = 0;
      newLogEntry.endMonth = 0;
      newLogEntry.endDay = 0;
      newLogEntry.endHour = 0;
      newLogEntry.endMinute = 0;
      writeLogEntry((void *)&newLogEntry);
  }
}

void initialPinSetup() {
  // Set only the pins we own; avoid blanket driving analog pins that may connect to sensors.
  //pinMode(0, OUTPUT); digitalWrite(0, LOW); // D0 (RX) left alone, serial comms
  //pinMode(1, OUTPUT); digitalWrite(1, LOW); // D1 (TX) left alone, serial comms

  static const uint8_t kOwnedPins[] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13};
  for (uint8_t pin : kOwnedPins) {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
  }

  // Leave A0-A3 alone until their respective modules configure them.
  // A4/A5 are SDA/SCL for I2C (LCD), leave them alone.

  pinMode(A6, INPUT_PULLUP); // Input only, set as pullup
  pinMode(BUTTONS_PIN, BUTTONS_PIN_MODE); // Analog buttons ladder input
}

void createBootLogEntry() {
  uint8_t soilMoistureBefore = soilMoistureAsPercentage(getSoilMoisture());

  clearLogEntry((void *)&newLogEntry);
  newLogEntry.millisStart = 0;
  newLogEntry.millisEnd = millis();
  newLogEntry.entryType = 0;  // BOOTED UP
  newLogEntry.stopReason = LOG_STOP_NONE;
  newLogEntry.startReason = LOG_START_NONE;
  newLogEntry.slotIndex = 0;
  newLogEntry.flags = 0;
  newLogEntry.soilMoistureBefore = soilMoistureBefore;
  newLogEntry.soilMoistureAfter = 0;
  rtcStamp(&newLogEntry.startYear, &newLogEntry.startMonth, &newLogEntry.startDay,
           &newLogEntry.startHour, &newLogEntry.startMinute);
  newLogEntry.endYear = 0;
  newLogEntry.endMonth = 0;
  newLogEntry.endDay = 0;
  newLogEntry.endHour = 0;
  newLogEntry.endMinute = 0;

  // Serial.println("Writing initial boot entry...");
  writeLogEntry((void *)&newLogEntry);
}



void screenSaverModeOff() {
  screenSaverMode = false;
  lcd.backlight();
}

void screenSaverModeOn() {
  screenSaverMode = true;
  lcd.noBacklight();
}

void screenSaverSuspend() {
  if (screenSaverHold < 255) screenSaverHold++;
  if (screenSaverMode) screenSaverModeOff();
}

void screenSaverResume() {
  if (screenSaverHold) screenSaverHold--;
  lastButtonPressTime = millis();
}

void mainMenu() {
  int8_t choice;
  int8_t lastChoice = 1;

  lcd.backlight();
  do {
    MsgId feedingToggle = feedingIsEnabled() ? MSG_PAUSE_FEEDING : MSG_UNPAUSE_FEEDING;
    setChoices_P(
      MSG_LOGS, 1,
      MSG_FEEDING_MENU, 2,
      MSG_FORCE_FEED, 3,
      feedingToggle, 4,
      MSG_SETTINGS, 5);

    choice = selectChoice(5, lastChoice);
    if (choice != -1) lastChoice = choice;

    if (choice == 1) viewLogs();
    else if (choice == 2) feedingMenu();
    else if (choice == 3) {
      if (feedingForceFeedMenu()) return;
    }
    else if (choice == 4) {
      feedingSetEnabled(!feedingIsEnabled());
      // Immediate feedback; loop redraws menu with new label.
    }
    else if (choice == 5) settings();
  } while (choice != -1);
  forceDisplayRedraw = true;
  displayInfo(screenCounter);
}

static void lcdPrintSpaces(uint8_t count) {
  while (count--) lcd.print(' ');
}

static void lcdPrintPercent3(uint8_t value) {
  if (value < 100) lcd.print(' ');
  if (value < 10) lcd.print(' ');
  lcd.print(value);
  lcd.print('%');
}

static uint8_t msgLength(MsgId message, uint8_t limit) {
  if (message == MSG_LITTLE || limit == 0) return 0;
  uint16_t addr = msgOffset(message);
  uint8_t len = 0;
  while (len < limit) {
    if (!msgReadByte(addr++)) break;
    len++;
  }
  return len;
}

static void lcdPrintPadded_P(MsgId message, uint8_t width) {
  lcdPrint_P(message);
  uint8_t len = msgLength(message, width);
  if (len < width) lcdPrintSpaces(width - len);
}

static bool getRtcTimeString(char *out) {
  if (!rtcIsOk()) return false;

  static unsigned long lastReadAt = 0;
  static uint16_t cachedMinutes = 0;
  static bool cachedValid = false;

  unsigned long now = millis();
  if (!cachedValid || now - lastReadAt >= 1000) {
    uint16_t minutes = 0;
    uint16_t dayKey = 0;
    cachedValid = rtcReadMinutesAndDay(&minutes, &dayKey);
    if (cachedValid) {
      cachedMinutes = minutes;
      lastReadAt = now;
    }
  }

  if (!cachedValid) return false;

  uint8_t hour = cachedMinutes / 60;
  uint8_t minute = cachedMinutes % 60;
  out[0] = static_cast<char>('0' + (hour / 10));
  out[1] = static_cast<char>('0' + (hour % 10));
  out[2] = ':';
  out[3] = static_cast<char>('0' + (minute / 10));
  out[4] = static_cast<char>('0' + (minute % 10));
  out[5] = '\0';
  return true;
}

void printSoilAndWaterTrayStatus(bool fullRedraw) {
  static const uint8_t kMoistValueCol = 7;
  static const uint8_t kDrybackLabelCol = 12;
  static const uint8_t kDrybackValueCol = 16;
  static const uint8_t kPercentFieldWidth = 4;
  static const uint8_t kTimeFieldWidth = 7;
  static const uint8_t kTimeCol = DISPLAY_COLUMNS - kTimeFieldWidth;
  static const unsigned long kDrybackRefreshMs = 60000UL;

  uint16_t soilMoisture = getSoilMoisture();
  uint16_t soilMoisturePercent = soilMoistureAsPercentage(soilMoisture);
  static unsigned long lastDrybackUpdate = 0;
  static uint8_t drybackPercent = 0;
  static bool drybackValid = false;

  if (fullRedraw) {
    lcdSetCursor(0, 0);
    lcdPrint_P(MSG_SOIL_NOW);
    lcdPrint_P(MSG_SPACE);

    lcdSetCursor(kDrybackLabelCol, 0);
    lcdPrint_P(MSG_DB_COLON);
    lcdPrint_P(MSG_SPACE);
    lcdClearLine(1);
  }

  if (fullRedraw || millis() - lastDrybackUpdate >= kDrybackRefreshMs) {
    drybackValid = getDrybackPercent(&drybackPercent);
    lastDrybackUpdate = millis();
  }

  lcdSetCursor(kMoistValueCol, 0);
  lcdPrintSpaces(kPercentFieldWidth);
  lcdSetCursor(kMoistValueCol, 0);
  if (!soilSensorReady()) {
    lcdPrint_P(MSG_PERCENT_UNKNOWN); // not ready yet
  } else {
    lcd.print(soilMoisturePercent);
    lcd.print('%');
  }

  lcdSetCursor(kDrybackValueCol, 0);
  lcdPrintSpaces(kPercentFieldWidth);
  lcdSetCursor(kDrybackValueCol, 0);
  if (!drybackValid) {
    lcdPrint_P(MSG_PERCENT_UNKNOWN);
  } else {
    lcd.print(drybackPercent);
    lcd.print('%');
  }

  char timeStr[6];
  lcdSetCursor(kTimeCol, 1);
  if (getRtcTimeString(timeStr)) {
    lcd.print(timeStr);
    lcdPrintSpaces(kTimeFieldWidth - 5);
  } else {
    lcdPrint_P(MSG_RTC_ERR);
  }
}

/*  
    **********************************************************************
    * LOGS VIEWING CODE
    * This allows users to go through the logs
    ***********************************************************************
*/


static void lcdPrintHoursTenths(unsigned long totalMinutes) {
  unsigned long tenths = (totalMinutes + 3) / 6; // round to nearest 0.1h
  unsigned long hours = tenths / 10UL;
  uint8_t frac = tenths % 10UL;
  lcd.print(hours);
  lcd.print('.');
  lcd.print(frac);
  lcd.print('h');
}

void lcdPrintTimeSince(unsigned long milliseconds) {
  unsigned long elapsedMinutes = (millis() - milliseconds) / 60000UL;
  lcdPrintHoursTenths(elapsedMinutes);
}


static void lcdPrintDateTime(uint8_t day, uint8_t month, uint8_t year, uint8_t hour, uint8_t minute) {
  if (!day || !month) {
    lcdPrint_P(MSG_DATETIME_UNKNOWN);
    return;
  }
  lcdPrintTwoDigits(day);
  lcd.print('/');
  lcdPrintTwoDigits(month);
  lcd.print('/');
  lcdPrintTwoDigits(year);
  lcd.print(' ');
  lcdPrintTwoDigits(hour);
  lcd.print(':');
  lcdPrintTwoDigits(minute);
}


void lcdPrintTimeDuration(unsigned long start, unsigned long finish) {
  unsigned long elapsedMinutes = (finish - start) / 60000UL;
  lcdPrintHoursTenths(elapsedMinutes);
}

void showLogType0() {
  lcdClear();
  // Print absolute log number (epoch-aware)
  lcd.print((unsigned long)getAbsoluteLogNumber());
  lcdPrint_P(MSG_SPACE);
  lcdPrint_P(MSG_LOG_TYPE_0);
  lcd.setCursor(0, 1);
  lcdPrint_P(MSG_AT);
  lcdPrintDateTime(currentLogEntry.startDay, currentLogEntry.startMonth, currentLogEntry.startYear,
                   currentLogEntry.startHour, currentLogEntry.startMinute);
  lcd.setCursor(0, 2);
  lcdPrint_P(MSG_SOIL_COLON);
  lcd.print(currentLogEntry.soilMoistureBefore);
  lcd.print('%');
}

void showLogType1() {
  lcdClear();
  // Print absolute log number (epoch-aware)
  lcd.print((unsigned long)getAbsoluteLogNumber());
  lcdPrint_P(MSG_SPACE);
  lcdPrint_P(MSG_LOG_TYPE_1);
  lcdPrint_P(MSG_SPACE);
  lcd.print('S');
  lcd.print(currentLogEntry.slotIndex + 1);

  lcd.setCursor(0, 1);
  lcdPrint_P(MSG_START_LABEL);
  lcdPrintDateTime(currentLogEntry.startDay, currentLogEntry.startMonth, currentLogEntry.startYear,
                   currentLogEntry.startHour, currentLogEntry.startMinute);

  lcd.setCursor(0, 2);
  lcdPrint_P(MSG_V_COLON);
  uint16_t volMl = currentLogEntry.feedMl;
  if (!volMl && currentLogEntry.millisEnd > currentLogEntry.millisStart) {
    unsigned long elapsedMs = currentLogEntry.millisEnd - currentLogEntry.millisStart;
    volMl = msToVolumeMl(elapsedMs, config.dripperMsPerLiter);
  }
  if (volMl) lcd.print(volMl);
  else lcdPrint_P(MSG_NA_LOWER);
  lcdPrint_P(MSG_T_COLON);
  lcd.print(currentLogEntry.dailyTotalMl);
  lcdPrint_P(MSG_S_COLON);
  switch (currentLogEntry.stopReason) {
    case LOG_STOP_MOISTURE: lcdPrint_P(MSG_MST_SHORT); break;
    case LOG_STOP_RUNOFF: lcdPrint_P(MSG_RUN_SHORT); break;
    case LOG_STOP_MAX_RUNTIME: lcdPrint_P(MSG_MAX_SHORT); break;
    case LOG_STOP_DISABLED: lcdPrint_P(MSG_OFF); break;
    case LOG_STOP_UI_PAUSE: lcdPrint_P(MSG_CFG_SHORT); break;
    case LOG_STOP_MAX_DAILY_FEED_REACHED: lcdPrint_P(MSG_DAY_SHORT); break;
    case LOG_STOP_FEED_NOT_CALIBRATED: lcdPrint_P(MSG_CAL_SHORT); break;
    default: lcdPrint_P(MSG_DASHES_3); break;
  }
  if (currentLogEntry.flags & LOG_FLAG_RUNOFF_ANY) lcd.print('!');

  lcd.setCursor(0, 3);
  lcdPrint_P(MSG_SOIL_MOISTURE_COLUMN);
  lcdPrintNumber(currentLogEntry.soilMoistureBefore);
  lcd.print('-');
  lcdPrintNumber(currentLogEntry.soilMoistureAfter);
  lcd.print('%');
  if (currentLogEntry.flags & LOG_FLAG_RUNOFF_SEEN) {
    lcd.print(' ');
    lcd.print('R');
  }
}

void showLogType2() {
  lcdClear();
  // Print absolute log number (epoch-aware)
  lcd.print((unsigned long)getAbsoluteLogNumber());
  lcdPrint_P(MSG_SPACE);
  lcdPrint_P(MSG_LOG_TYPE_2);
  lcd.setCursor(0, 1);
  lcdPrint_P(MSG_AT);
  lcdPrintDateTime(currentLogEntry.startDay, currentLogEntry.startMonth, currentLogEntry.startYear,
                   currentLogEntry.startHour, currentLogEntry.startMinute);

  lcd.setCursor(0, 2);
  lcdPrint_P(MSG_SOIL_MOISTURE_COLUMN);
  lcdPrintNumber(currentLogEntry.soilMoistureBefore);
  lcdPrint_P(MSG_PERCENT);

  lcd.setCursor(0, 3);
  // lcdPrintLogParamsSoil(currentLogEntry.soilMoistureBefore);
  // lcdPrintLogParamsSoil(currentLogEntry.soilMoistureAfter);
}


void viewLogs() {
  feedingClearRunoffWarning();
  lcdClear();

  // Always start from the latest available log entry
  goToLatestSlot();

  if (noLogs()) {
    lcdFlashMessage_P(MSG_NO_LOG_ENTRIES);
    return;
  }

  bool dataChanged = true;
  while (1) {
    if (dataChanged) {

      if (currentLogEntry.entryType == 0) showLogType0();
      if (currentLogEntry.entryType == 1) showLogType1();
      if (currentLogEntry.entryType == 2) showLogType2();
      dataChanged = false;
    }

    analogButtonsCheck();
    if (pressedButton == &leftButton) {
      break;
    } else if (pressedButton == &upButton) { // && currentLogEntry.entryType != 0) {
      if (goToPreviousLogSlot()) {
        dataChanged = true;
      }
    } else if (pressedButton == &downButton) {
      if (goToNextLogSlot()) {
        dataChanged = true;
      }
    }
  }
}


void displayInfo(uint8_t screen) {
  static uint8_t lastScreen = 255;
  static bool lastScreenSaver = false;
  bool fullRedraw = false;

  if (screenSaverMode) {
    lastScreenSaver = true;

    uint8_t r = nextScreenSaverRand();
    uint8_t x = r % 20;
    uint8_t y = (r >> 5) & 0x03;
  
    // Set the cursor to the random position and print the star character
    lcd.clear();
    lcd.setCursor(x, y);
    // lcd.setCursor(1, 1);
    
    lcd.write(idleStatusChar());

    return;
  }

  if (lastScreenSaver) {
    lastScreenSaver = false;
    lastScreen = 255;
  }

  if (forceDisplayRedraw) {
    lcd.clear();
    lastScreen = screen;
    fullRedraw = true;
    forceDisplayRedraw = false;
  } else if (screen != lastScreen) {
    lcd.clear();
    lastScreen = screen;
    fullRedraw = true;
  }

  switch (screen) {
    case 0: displayInfo1(fullRedraw); break;
  }
}


void displayInfo1(bool fullRedraw) {
  static bool lastFeeding = false;
  bool feedingNow = feedingIsActive();
  if (feedingNow != lastFeeding) {
    lcd.clear();
    fullRedraw = true;
    lastFeeding = feedingNow;
  }
  if (feedingNow) {
    displayFeedingStatus(fullRedraw);
    return;
  }
  static const uint8_t kDailyValueCol = 3;
  static const uint8_t kDailyFieldWidth = 7;  // "65535ml" max
  static const uint8_t kDailyWarnCol = 10;
  static const uint8_t kBaselineLabelCol = 12;
  static const uint8_t kBaselineValueCol = 16;
  static const uint8_t kBaselineFieldWidth = 4;

  printSoilAndWaterTrayStatus(fullRedraw);
  bool showBaseline = feedingHasBaselineSetter();

  if (fullRedraw) {
    lcdClearLine(2);
    lcdSetCursor(0, 2);
    lcdPrint_P(MSG_LAST_FEED);
    lcdSetCursor(0, 3);
    lcdPrint_P(MSG_TODAY_COLON);
  }

  lcdSetCursor(kDailyWarnCol, 3);
  char statusChar = idleStatusChar();
  lcd.print(statusChar == '*' ? ' ' : statusChar);

  uint16_t minutes = 0;
  bool dayNow = false;
  if (rtcReadMinutesAndDay(&minutes, nullptr)) {
    uint16_t on = config.lightsOnMinutes;
    uint16_t off = config.lightsOffMinutes;
    if (on > 1439) on = 0;
    if (off > 1439) off = 0;
    if (on != off) {
      uint16_t duration = (off >= on) ? (off - on) : static_cast<uint16_t>(1440 - on + off);
      dayNow = rtcIsWithinWindow(minutes, on, duration);
    }
  }
  lcdSetCursor(0, 1);
  if (dayNow) lcdPrint_P(MSG_DAY_UPPER_PAD);
  else lcdPrint_P(MSG_NIGHT);

  lcdClearLine(2);
  lcdSetCursor(0, 2);
  lcdPrint_P(MSG_LAST_FEED);
  if (!millisAtEndOfLastFeed) lcdPrint_P(MSG_DASHES_2);
  else lcdPrintTimeSince(millisAtEndOfLastFeed);
  lcdPrint_P(MSG_AGO);
  if (!millisAtEndOfLastFeed) lcdPrint_P(MSG_DASHES_2);
  else lcd.print(lastFeedMl);
  lcdPrint_P(MSG_ML);
  // lcdPrintNumber(actionPreviousMillis);

  uint16_t dailyTotal = getDailyFeedTotalMlNow();
  lcdSetCursor(kDailyValueCol, 3);
  lcdPrintSpaces(kDailyFieldWidth);
  lcdSetCursor(kDailyValueCol, 3);
  lcd.print(dailyTotal);
  lcdPrint_P(MSG_ML);
  if (showBaseline) {
    lcdSetCursor(kBaselineLabelCol, 3);
    lcdPrint_P(MSG_BL_COLON);
  }
  lcdSetCursor(kBaselineValueCol, 3);
  lcdPrintSpaces(kBaselineFieldWidth);
  if (showBaseline) {
    lcdSetCursor(kBaselineValueCol, 3);
    uint8_t baseline = 0;
    if (feedingGetBaselinePercent(&baseline)) {
      lcd.print(baseline);
      lcd.print('%');
    } else {
      lcdPrint_P(MSG_NA);
    }
  }

}

static void displayFeedingStatus(bool fullRedraw) {
  FeedStatus status;
  if (!feedingGetStatus(&status)) return;

  if (fullRedraw) {
    lcdClearLine(0);
    lcdSetCursor(0, 0);
    lcdPrint_P(MSG_FEEDING_S);
    lcd.print(status.slotIndex + 1);
    lcd.print(' ');
    lcd.print(config.pulseOnSeconds);
    lcd.print('/');
    lcd.print(config.pulseOffSeconds);

    lcdClearLine(2);
    lcdSetCursor(0, 2);
    lcdPrint_P(MSG_STOP_COLON);
    bool anyStop = false;
    if (status.hasMoistureTarget) {
      lcdPrint_P(MSG_SPACE);
      lcdPrint_P(MSG_MOIST_GT);
      lcd.print(status.moistureTarget);
      lcd.print('%');
      anyStop = true;
    }
    if (status.runoffRequired) {
      if (anyStop) lcd.print(' ');
      lcdPrint_P(MSG_RUNOFF);
      anyStop = true;
    }
    if (!anyStop) {
      lcdPrint_P(MSG_SPACE);
      lcdPrint_P(MSG_NA);
    }
  }

  lcdClearLine(1);
  lcdSetCursor(0, 1);
  lcdPrint_P(MSG_PUMP_COLON);
  lcdPrint_P(status.pumpOn ? MSG_ON_UPPER : MSG_OFF_UPPER);
  lcdPrint_P(MSG_SPACE);
  lcdPrint_P(MSG_MST_COLON);
  if (status.moistureReady) {
    lcd.print(status.moisturePercent);
    lcd.print('%');
  } else {
    lcdPrint_P(MSG_PERCENT_UNKNOWN);
  }
  uint16_t elapsed = status.elapsedSeconds;
  if (elapsed > 999) elapsed = 999;
  lcdSetCursor(DISPLAY_COLUMNS - 3, 1);
  if (elapsed < 100) lcd.print(' ');
  if (elapsed < 10) lcd.print(' ');
  lcd.print(elapsed);

  lcdClearLine(3);
  lcdSetCursor(0, 3);
  lcdPrint_P(MSG_MAX_COLON);
  if (status.maxVolumeMl) lcd.print(status.maxVolumeMl);
  else lcd.print('-');
  lcdPrint_P(MSG_ML_W_COLON);
  uint16_t deliveredMl = msToVolumeMl((uint32_t)status.elapsedSeconds * 1000UL, config.dripperMsPerLiter);
  lcd.print(deliveredMl);
  lcd.print('m');
  lcd.print('l');
}
