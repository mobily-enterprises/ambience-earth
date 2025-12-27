#include <AnalogButtons.h>
#include <Arduino.h>
#include "main.h"
#include "ui.h"
#include "config.h"
#include "messages.h"
#include "traySensors.h"
#include "pumps.h"
#include "moistureSensor.h"
#include "logs.h"
#include "settings.h"
#include "main.h"
#include <LiquidCrystal_I2C.h>
#include <AnalogButtons.h>

extern LiquidCrystal_I2C lcd;

extern Config config;
extern double averageMsBetweenFeeds;
extern unsigned long int millisAtEndOfLastFeed;

extern Button upButton;
extern Button leftButton;
extern Button downButton;
extern Button rightButton;
extern Button okButton;
extern Button *pressedButton;

void settings() {
  int8_t choice = 0;

  lcdClear();
  while (choice != -1) {
    setChoices(
      MSG_CALIBRATE, 1,
      MSG_MAINTENANCE, 2);
    choice = selectChoice(2, 1);

    if (choice == 1) settingsCalibrate();
    else if (choice == 2) maintenance();
  }
}

void maintenance() {
  int8_t choice = 0;
  while (choice != -1) {
    lcdClear();
    setChoices(
      MSG_SAFETY_LIMITS, 1,
      MSG_TEST_PUMPS, 2,
      MSG_TEST_SENSORS, 3,
      MSG_RESET_ONLY_LOGS, 4,
      MSG_RESET_DATA, 5

    );
    choice = selectChoice(5, 1);

    if (choice == 1) settingsSafetyLimits();
    else if (choice == 2) activatePumps();
    else if (choice == 3) testSensors();
    else if (choice == 4) resetOnlyLogs();
    else if (choice == 5) resetData();
  }
}

void testSensors() {
  static const unsigned long interval = 300;  // interval at which to run (milliseconds)
  static unsigned long previousMillis = 0;   // will store last time the loop ran
  static int soilMoisturePercentage = 0;
  static bool trayWaterLevelLow = false;
  static int soilMoistureReading;


  lcdClear();

  lcdSetCursor(0, 0);
  lcdPrint(MSG_TRAY_LOW_COLUMN);
  lcdSetCursor(0, 3);
  lcdPrint(MSG_SOIL_MOISTURE_COLUMN);

  setSoilSensorRealTime();

  while (1) {
    unsigned long currentMillis = millis();

    if (currentMillis - previousMillis >= interval) {
      previousMillis = currentMillis;

      soilMoistureReading = getSoilMoisture();
      
      soilMoisturePercentage = soilMoistureAsPercentage(soilMoistureReading);
      
      trayWaterLevelLow = senseTrayWaterLevelLow();

      
      analogButtonsCheck();
 
      if (pressedButton != nullptr) {
        setSoilSensorLazy();
        break;
      }

      // Serial.print("AH"); Serial.println(digitalRead(TRAY_SENSOR_LOW)); 
      lcdSetCursor(11, 0);
      lcdPrintNumber(trayWaterLevelLow);
      lcdPrint(MSG_SPACE);

      lcdSetCursor(10, 3);
      lcdPrintNumber(soilMoisturePercentage);
      lcdPrint(MSG_PERCENT);
      lcdPrint(MSG_SPACE);
      lcdPrintNumber(soilMoistureReading);

      lcdPrint(MSG_SPACE);
      lcdPrint(MSG_SPACE);
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

  confirmWipe = yesOrNo(MSG_SURE_QUESTION);
  if (confirmWipe == -1) return;

  if (confirmWipe) {
    lcdClear();
    lcdPrint(MSG_WIPING, 2);
    wipeLogsAndResetVars();
    createBootLogEntry();
  };
}

void settingsSafetyLimits() {
  uint32_t minFeedInterval;
  uint32_t maxFeedTime;
  int8_t goAhead;

  minFeedInterval = inputNumber(MSG_MINUTES, config.minFeedInterval/1000/60, 30, 0, 600, MSG_LITTLE, MSG_MIN_FEED_INTERVAL);
  if (minFeedInterval == -1) return;

  maxFeedTime = inputNumber(MSG_SECONDS, config.maxFeedTime/1000, 10, 0, 600, MSG_LITTLE, MSG_MAX_FEED_TIME);
  if (maxFeedTime == -1) return;
  
  // Normalise to ms
  minFeedInterval = minFeedInterval * 1000L * 60;
  maxFeedTime = maxFeedTime * 1000L;

  lcdClear();

  goAhead = yesOrNo(MSG_SAVE_QUESTION);
  if (goAhead == -1) return;

  if (goAhead) {
    config.minFeedInterval = minFeedInterval;
    config.maxFeedTime = maxFeedTime;

    saveConfig();
  }
}

void settingsCalibrate() {
  while (true) {
    setChoices(MSG_MOISTURE_SENSOR, 0);
    setChoicesHeader(MSG_CALIBRATE);
    int8_t choice = selectChoice(1, 0);

    if (choice == -1) return;
    if (choice == 0 && calibrateSoilMoistureSensor()) saveConfig();
  }
}

static void debugButtonRead(uint16_t readValue) {
  static unsigned long lastDebugMillis = 0;
  const unsigned long interval = 250;
  const unsigned long now = millis();

  if (now - lastDebugMillis < interval) return;
  lastDebugMillis = now;

  (void)readValue;
}

void runButtonsSetup() {
  uint16_t read = 1024;

  lcd.setCursor(0, 0);
  lcdPrint(MSG_PRESS_UP);
  while (read > 1000) {
    read = analogRead(BUTTONS_PIN);
    debugButtonRead(read);
    if (read < 1000) {
      delay(100);
      read = analogRead(BUTTONS_PIN);
      config.kbdUp = read;
      break;
    }
  }

  delay(1000);

  lcd.setCursor(0, 0);
  lcdPrint(MSG_PRESS_DOWN);
  read = 1024;
  while (read > 1000) {
    read = analogRead(BUTTONS_PIN);
    debugButtonRead(read);
    if (read < 1000) {
      delay(100);
      read = analogRead(BUTTONS_PIN);
      config.kbdDown = read;
      break;
    }
  }
  delay(1000);

  lcd.setCursor(0, 0);
  lcdPrint(MSG_PRESS_LEFT);
  read = 1024;
  while (read > 1000) {
    read = analogRead(BUTTONS_PIN);
    debugButtonRead(read);
    if (read < 1000) {
      delay(100);
      read = analogRead(BUTTONS_PIN);
      config.kbdLeft = read;
      break;
    }
  }

  delay(1000);

  lcd.setCursor(0, 0);
  lcdPrint(MSG_PRESS_RIGHT);
  read = 1024;
  while (read > 1000) {
    read = analogRead(BUTTONS_PIN);
    debugButtonRead(read);
    if (read < 1000) {
      delay(100);
      read = analogRead(BUTTONS_PIN);
      config.kbdRight = read;
      break;
    }
  }

  delay(1000);

  lcd.setCursor(0, 0);
  lcdPrint(MSG_PRESS_OK);
  read = 1024;
  while (read > 1000) {
    read = analogRead(BUTTONS_PIN);
    debugButtonRead(read);
    if (read < 1000) {
      delay(100);
      read = analogRead(BUTTONS_PIN);
      config.kbdOk = read;
      break;
    }
  }

  delay(1000);

  initializeButtons();
}

int runInitialSetup() {
  if (!calibrateSoilMoistureSensor()) return 0;

  // Save config
  config.mustRunInitialSetup = false;
  saveConfig();

  lcdClear();
  lcdPrint(MSG_DONE);

  return 1;
}

int calibrateSoilMoistureSensor() {
  int8_t goAhead;
  int soaked;
  int dry;

  goAhead = giveOk(MSG_MOIST_SENSOR, MSG_YES_ITS_DRY, MSG_ENSURE_SENSOR_IS, MSG_VERY_DRY);
  if (goAhead == -1) return false;

  setSoilSensorRealTime();

  lcdClear();
  lcdPrint(MSG_DRY_COLUMN, 1);
  for (int i = 0; i < 4; i++) {
    dry = getSoilMoisture();
    // lcdClear();
    lcdPrintNumber(dry);
    lcdPrint(MSG_SPACE);
    delay(900);
  }

  delay(2000);

  goAhead = giveOk(MSG_MOIST_SENSOR, MSG_YES_ITS_SOAKED, MSG_ENSURE_SENSOR_IS, MSG_VERY_SOAKED);
  if (goAhead == -1) {
    setSoilSensorLazy();
    return false;
  }

  lcdClear();
  lcdPrint(MSG_SOAKED_COLUMN, 1);
  for (int i = 0; i < 4; i++) {
    soaked = getSoilMoisture();
    // lcdClear();
    lcdPrintNumber(soaked);
    lcdPrint(MSG_SPACE);
    delay(900);
  }

  setSoilSensorLazy();

  goAhead = yesOrNo(MSG_SAVE_QUESTION);
  if (goAhead == -1) return;

  // Confirm to save
  if (goAhead) {
    config.moistSensorCalibrationSoaked = soaked;
    config.moistSensorCalibrationDry = dry;
    return true;
  } else {
    return false;
  }
}

void resetData() {
  int8_t reset = yesOrNo(MSG_SURE_QUESTION);

  if (reset == -1) return;

  if (reset) {
    lcdClear();
    lcdPrint(MSG_WIPING, 2);
    restoreDefaultConfig();
    saveConfig();
    wipeLogsAndResetVars();
    createBootLogEntry();
    while (!runInitialSetup())
      ;
  }
}

void activatePumps() {
  int8_t choice;
  while (true) {
    setChoices(MSG_PUMP_IN, PUMP_IN_DEVICE);
    choice = selectChoice(1, 0);
    if (choice == -1) return;

  lcdFlashMessage(MSG_DEVICE_WILL_BLINK, MSG_THREE_TIMES, 100);
 
   // pinMode(choice, OUTPUT);
    for(int i = 0; i <= 3; i++) {
      digitalWrite(choice, LOW);
      delay(1000);
      digitalWrite(choice, HIGH);
      delay(1000);
    }
  }
}
