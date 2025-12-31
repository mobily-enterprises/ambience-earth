#include <AnalogButtons.h>
#include <Arduino.h>
#include "LiquidCrystal_I2C.h"
#include "config.h"
#include "logs.h"
#include "main.h"
#include "messages.h"
#include "moistureSensor.h"
#include "pumps.h"
#include "rtc.h"
#include "settings.h"
#include "traySensors.h"
#include "ui.h"

extern LiquidCrystal_I2C lcd;

extern Config config;
extern unsigned long averageMsBetweenFeeds;
extern unsigned long int millisAtEndOfLastFeed;

extern Button upButton;
extern Button leftButton;
extern Button downButton;
extern Button rightButton;
extern Button okButton;
extern Button *pressedButton;

static void setTimeAndDate() {
  uint8_t hour = 0;
  uint8_t minute = 0;
  uint8_t day = 1;
  uint8_t month = 1;
  uint8_t year = 26;

  if (rtcReadDateTime(&hour, &minute, &day, &month, &year)) {
    if (hour > 23) hour = 0;
    if (minute > 59) minute = 0;
    bool dateUnset = (month == 0 || month > 12 || day == 0 || day > 31);
    if (!dateUnset && year == 0 && month == 1 && day == 1) dateUnset = true;
    if (dateUnset) {
      day = 1;
      month = 1;
      year = 26;
    }
  } else {
    day = 1;
    month = 1;
    year = 26;
  }

  if (!inputDateTime_P(MSG_SET_TIME_DATE, &hour, &minute, &day, &month, &year)) {
    lcdFlashMessage_P(MSG_ABORTED);
    return;
  }

  int8_t save = yesOrNo_P(MSG_SAVE_QUESTION);
  if (save != 1) return;

  if (rtcSetDateTime(hour, minute, 0, day, month, year)) {
    lcdFlashMessage_P(MSG_DONE);
  }
}

static void drawOkPrompt(PGM_P header, PGM_P prompt, PGM_P line2, PGM_P line3) {
  lcdClear();
  lcdPrint_P(header, 0);
  lcdClearLine(1);
  lcdSetCursor(0, 1);
  lcd.write((uint8_t)2);
  lcdPrint_P(prompt);
  lcdPrint_P(line2, 2);
  lcdPrint_P(line3, 3);
}

static void drawYesNoPrompt(bool yesSelected) {
  lcdClear();
  lcdPrint_P(MSG_SAVE_QUESTION, 0);
  lcdClearLine(1);
  lcdSetCursor(0, 1);
  if (yesSelected) lcd.write((uint8_t)2);
  else lcdPrint_P(MSG_SPACE);
  lcdPrint_P(MSG_YES);
  lcdClearLine(2);
  lcdSetCursor(0, 2);
  if (!yesSelected) lcd.write((uint8_t)2);
  else lcdPrint_P(MSG_SPACE);
  lcdPrint_P(MSG_NO);
}
void settings() {
  int8_t choice = 0;

  lcdClear();
  while (choice != -1) {
    setChoices_P(
      MSG_SET_TIME_DATE, 1,
      MSG_CAL_MOISTURE_SENSOR, 2,
      MSG_TEST_PUMPS, 3,
      MSG_TEST_SENSORS, 4,
      MSG_RESET, 5);
    choice = selectChoice(5, 1);

    if (choice == 1) setTimeAndDate();
    else if (choice == 2) calibrateMoistureSensor();
    else if (choice == 3) pumpTest();
    else if (choice == 4) testSensors();
    else if (choice == 5) settingsReset();
  }
}

static uint16_t calibrateOnePoint(bool isDry) {
  lcdClear();
  lcdSetCursor(0, 0);
  lcdPrint_P(isDry ? MSG_CAL_DRY_ONLY : MSG_CAL_WET_ONLY);
  lcdSetCursor(0, 1);
  lcdPrint_P(isDry ? MSG_DRY_COLUMN : MSG_WET_COLUMN);
  lcdPrint_P(MSG_SPACE);

  soilSensorWindowStart();
  uint16_t lastRaw = 0xFFFF;
  unsigned long lastPrint = 0;

  while (true) {
    SoilSensorWindowStats stats;
    bool done = soilSensorWindowTick(&stats);
    uint16_t sample = soilSensorWindowLastRaw();
    unsigned long now = millis();

    if (sample != lastRaw && now - lastPrint >= 300) {
      lcdSetCursor(5, 1);
      lcd.print(sample);
      lcdPrint_P(MSG_SPACE);
      lastRaw = sample;
      lastPrint = now;
    }

    if (done) {
      setSoilSensorLazy();
      uint16_t avg = stats.avgRaw;
      if (isDry) config.moistSensorCalibrationDry = avg;
      else config.moistSensorCalibrationSoaked = avg;
      saveConfig();

      lcdClear();
      lcdSetCursor(0, 0);
      lcdPrint_P(isDry ? MSG_CAL_DRY_ONLY : MSG_CAL_WET_ONLY);
      lcdSetCursor(0, 1);
      lcd.print(isDry ? F("Dry: ") : F("Wet: "));
      lcd.print(avg);
      lcdSetCursor(0, 2);
      lcdPrint_P(MSG_SAVE_QUESTION);
      lcdSetCursor(0, 3);
      lcdPrint_P(MSG_OK_EDIT_BACK);
      while (true) {
        analogButtonsCheck();
        if (pressedButton == &okButton) {
          pressedButton = nullptr;
          break;
        } else if (pressedButton == &leftButton) {
          pressedButton = nullptr;
          return 0;
        }
      }
      return avg;
    }

    analogButtonsCheck();
    if (pressedButton == &leftButton) {
      setSoilSensorLazy();
      return 0;
    }
  }
}

void calibrateMoistureSensor() {
  while (true) {
    setChoices_P(MSG_CAL_DRY_ONLY, 1, MSG_CAL_WET_ONLY, 2);
    setChoicesHeader_P(MSG_CAL_MOISTURE_SENSOR);
    int8_t choice = selectChoice(2, 1);
    if (choice == -1) return;
    if (choice == 1) {
      calibrateOnePoint(true);
      return;
    }
    if (choice == 2) {
      calibrateOnePoint(false);
      return;
    }
  }
}

void pumpTest() {
  lcdClear();
  lcdPrint_P(MSG_DEVICE_WILL_BLINK, 1);
  lcdPrint_P(MSG_THREE_TIMES, 2);
  delay(500);
  const uint8_t cycles = 3;
  for (uint8_t i = 0; i < cycles; ++i) {
    digitalWrite(PUMP_IN_DEVICE, LOW);
    delay(1000);
    digitalWrite(PUMP_IN_DEVICE, HIGH);
    delay(1000);
  }
  lcdClear();
  lcdPrint_P(MSG_DONE, 1);
  delay(1000);
}

void testSensors() {
  static const unsigned long interval = 300;  // interval at which to run (milliseconds)
  static unsigned long previousMillis = 0;   // will store last time the loop ran
  static int soilMoistureRawPercentage = 0;
  static int soilMoistureAvgPercentage = 0;
  static bool trayWaterLevelLow = false;
  static int soilMoistureRawReading;
  static int soilMoistureAvgReading;


  lcdClear();

  lcdSetCursor(0, 0);
  lcdPrint_P(MSG_TRAY_LOW_COLUMN);
  lcdSetCursor(0, 1);
  lcdPrint_P(MSG_MOISTURE_SENSOR_HEADING);
  lcdClearLine(2);
  lcdClearLine(3);

  setSoilSensorRealTime();

  while (1) {
    unsigned long currentMillis = millis();

    if (currentMillis - previousMillis >= interval) {
      previousMillis = currentMillis;

      soilMoistureAvgReading = getSoilMoisture();
      soilMoistureRawReading = soilSensorGetRealtimeRaw();
      
      soilMoistureAvgPercentage = soilMoistureAsPercentage(soilMoistureAvgReading);
      soilMoistureRawPercentage = soilMoistureAsPercentage(soilMoistureRawReading);
      
      trayWaterLevelLow = senseTrayWaterLevelLow();

      
      analogButtonsCheck();
 
      if (pressedButton != nullptr) {
        setSoilSensorLazy();
        break;
      }

      lcdSetCursor(15, 0);
      lcdPrintNumber(trayWaterLevelLow);
      lcdPrint_P(MSG_SPACE);

      lcdClearLine(2);
      lcdSetCursor(0, 2);
      lcdPrint_P(MSG_RAW_COLUMN);
      lcdSetCursor(5, 2);
      lcdPrintNumber(soilMoistureRawPercentage);
      lcdPrint_P(MSG_PERCENT);
      lcdPrint_P(MSG_SPACE);
      lcdPrintNumber(soilMoistureRawReading);

      lcdClearLine(3);
      lcdSetCursor(0, 3);
      lcdPrint_P(MSG_AVG_SHORT);
      lcdSetCursor(5, 3);
      lcdPrintNumber(soilMoistureAvgPercentage);
      lcdPrint_P(MSG_PERCENT);
      lcdPrint_P(MSG_SPACE);
      lcdPrintNumber(soilMoistureAvgReading);
    }
  }
}

void wipeLogsAndResetVars() {
  averageMsBetweenFeeds = 0.0;
  millisAtEndOfLastFeed = 0;
  wipeLogs();
  initialaverageMsBetweenFeeds();
}

void resetOnlyLogs() {
  int8_t confirmWipe;

  confirmWipe = yesOrNo_P(MSG_SURE_QUESTION);
  if (confirmWipe == -1) return;

  if (confirmWipe) {
    lcdClear();
    lcdPrint_P(MSG_WIPING, 2);
    wipeLogsAndResetVars();
    createBootLogEntry();
  };
}

void settingsReset() {
  while (true) {
    setChoices_P(MSG_RESET_ONLY_LOGS, 0, MSG_RESET_DATA, 1);
    setChoicesHeader_P(MSG_RESET);
    int8_t choice = selectChoice(2, 0);

    if (choice == -1) return;
    if (choice == 0) resetOnlyLogs();
    else if (choice == 1) resetData();
  }
}

void runButtonsSetup() {
  uint16_t read = 1024;

  lcd.setCursor(0, 0);
  lcdPrint_P(MSG_PRESS_UP);
  while (read > 1000) {
    read = analogRead(BUTTONS_PIN);
    if (read < 1000) {
      delay(100);
      read = analogRead(BUTTONS_PIN);
      config.kbdUp = read;
      lcdPrintNumber(read, 1);
      break;
    }
  }

  delay(1000);

  lcd.setCursor(0, 0);
  lcdPrint_P(MSG_PRESS_DOWN);
  read = 1024;
  while (read > 1000) {
    read = analogRead(BUTTONS_PIN);
    if (read < 1000) {
      delay(100);
      read = analogRead(BUTTONS_PIN);
      config.kbdDown = read;
      lcdPrintNumber(read, 1);
      break;
    }
  }
  delay(1000);

  lcd.setCursor(0, 0);
  lcdPrint_P(MSG_PRESS_LEFT);
  read = 1024;
  while (read > 1000) {
    read = analogRead(BUTTONS_PIN);
    if (read < 1000) {
      delay(100);
      read = analogRead(BUTTONS_PIN);
      config.kbdLeft = read;
      lcdPrintNumber(read, 1);
      break;
    }
  }

  delay(1000);

  lcd.setCursor(0, 0);
  lcdPrint_P(MSG_PRESS_RIGHT);
  read = 1024;
  while (read > 1000) {
    read = analogRead(BUTTONS_PIN);
    if (read < 1000) {
      delay(100);
      read = analogRead(BUTTONS_PIN);
      config.kbdRight = read;
      lcdPrintNumber(read, 1);
      break;
    }
  }

  delay(1000);

  lcd.setCursor(0, 0);
  lcdPrint_P(MSG_PRESS_OK);
  read = 1024;
  while (read > 1000) {
    read = analogRead(BUTTONS_PIN);
    if (read < 1000) {
      delay(100);
      read = analogRead(BUTTONS_PIN);
      config.kbdOk = read;
      lcdPrintNumber(read, 1);
      break;
    }
  }

  delay(1000);

  initializeButtons();
}

int runInitialSetup() {
  if (!calibrateSoilMoistureSensor()) return 0;

  // Save config
  config.flags &= static_cast<uint8_t>(~CONFIG_FLAG_MUST_RUN_INITIAL_SETUP);
  saveConfig();

  lcdClear();
  lcdPrint_P(MSG_DONE);

  return 1;
}

int calibrateSoilMoistureSensor() {
  calibrateMoistureSensor();
  return true;
}

void resetData() {
  int8_t reset = yesOrNo_P(MSG_SURE_QUESTION);

  if (reset == -1) return;

  if (reset) {
    lcdClear();
    lcdPrint_P(MSG_WIPING, 2);
    restoreDefaultConfig();
    saveConfig();
    wipeLogsAndResetVars();
    createBootLogEntry();
    while (!runInitialSetup())
      ;
  }
}

void activatePumps() {
  pumpTest();
}
