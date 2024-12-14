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

extern LiquidCrystal_I2C lcd;

extern Config config;
extern LogEntry currentLogEntry;
extern LogEntry newLogEntry;

extern double averageMsBetweenFeeds;
extern unsigned long int millisAtEndOfLastFeed;
extern Button *pressedButton;

void settings() {
  int8_t choice = 0;

  lcdClear();
  while (choice != -1) {
    setChoices(
      MSG_EDIT_ACTIONS, 1,
      MSG_FEEDING_FROM, 2,
      MSG_AUTO_DRAIN, 3,
      MSG_CALIBRATE, 5,
      MSG_MAINTENANCE, 10);
    choice = selectChoice(5, 1);

    if (choice == 1) settingsEditActions();
    else if (choice == 2) setFeedFrom();
    else if (choice == 3) setAutoDrain();
    else if (choice == 5) settingsCalibrate();
    else if (choice == 10) maintenance();
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
  static bool trayWaterLevelMid = false;
  static bool trayWaterLevelHigh = false;
  static int soilMoistureReading;


  lcdClear();

  lcdSetCursor(0, 0);
  lcdPrint(MSG_TRAY_LOW_COLUMN);
  lcdSetCursor(0, 1);
  lcdPrint(MSG_TRAY_MID_COLUMN);
  lcdSetCursor(0, 2);
  lcdPrint(MSG_TRAY_HIGH_COLUMN);
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
      trayWaterLevelMid = senseTrayWaterLevelMid();
      trayWaterLevelHigh = senseTrayWaterLevelHigh();

      
      analogButtonsCheck();
 
      if (pressedButton != nullptr) {
        setSoilSensorLazy();
        break;
      }

      // Serial.print("AH"); Serial.println(digitalRead(TRAY_SENSOR_LOW)); 
      lcdSetCursor(11, 0);
      lcdPrintNumber(trayWaterLevelLow);
      lcdPrint(MSG_SPACE);
      lcdSetCursor(11, 1);
      lcdPrintNumber(trayWaterLevelMid);
      lcdPrint(MSG_SPACE);
      lcdSetCursor(11, 2);
      lcdPrintNumber(trayWaterLevelHigh);

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

bool isActionActive(int8_t actionId) {
  return config.activeActionsIndex0 == actionId || config.activeActionsIndex1 == actionId;
  /*
  for (size_t i = 0; i < 4; ++i) {
    if (config.activeActionsIndexes[i] == actionId) {
      return true;
    }
  }
  return false;
  */
}

Conditions inputConditions(Conditions *initialConditions, char verb, int8_t choiceId) {

  setChoices(
    MSG_TRAY_SOIL_IGNORE, Conditions::TRAY_IGNORED,
    MSG_TRAY_DRY, Conditions::TRAY_DRY,
    MSG_TRAY_LITTLE, Conditions::TRAY_LITTLE,
    MSG_TRAY_MIDDLE, Conditions::TRAY_MIDDLE,
    MSG_TRAY_FULL, Conditions::TRAY_FULL
  );
  if (verb == 'F') {
    setChoicesHeader(MSG_START_TRAY);
  } else {
    setChoicesHeader(MSG_STOP_TRAY);
  }
  int8_t tray = (int8_t)selectChoice(5, (int8_t)initialConditions->tray);
  initialConditions->tray = tray;
  if (tray == -1) return;

  
  setChoices(
    MSG_TRAY_SOIL_IGNORE, 0,
    MSG_60_PERCENT, 60,
    MSG_65_PERCENT, 65,
    MSG_70_PERCENT, 60,
    MSG_75_PERCENT, 75,
    MSG_80_PERCENT, 80,
    MSG_85_PERCENT, 85,
    MSG_90_PERCENT, 60,
    MSG_95_PERCENT, 95,
    MSG_100_PERCENT, 100
  );


  if (verb == 'F') {
    setChoicesHeader(MSG_START_SOIL);
  } else {
    setChoicesHeader(MSG_STOP_SOIL);
  }

  int8_t soil = (int8_t)selectChoice(10, (int8_t)initialConditions->soil);
  initialConditions->soil = soil;
  if (soil == -1) return;

  int8_t logic;

  if (tray && soil) {
    setChoices(
      MSG_BOTH, Conditions::TRAY_AND_SOIL,
      MSG_EITHER, Conditions::TRAY_AND_SOIL);
    setChoicesHeader(MSG_LOGIC);
    logic = (int8_t)selectChoice(2, (int8_t)initialConditions->logic);
    initialConditions->logic = logic;
    if (logic == -1) return;
  } else {
    logic = Conditions::NO_LOGIC;
    initialConditions->logic = logic;
  }

  // Serial.println((int)initialConditions->tray); // Print the value of tray
  // Serial.println((int)initialConditions->soil); // Print the value of tray
}

void settingsEditActions() {
  lcdClear();
  uint8_t i = 0;
  while (true) {

    // Set the choices
    for (i = 0; i <= ACTIONS_ARRAY_SIZE; i++) setChoiceFromString(i, config.actions[i].name, i);

    // Pick one. If -1, then get out of this menu with the "break"
    int8_t choiceId = selectChoice(i, 0);
    if (choiceId == -1) break;

    // Editing of the choice's name
    // Note: until saved, the value will stay in userInputString
    // I don't want to waste 20 bytes to store something that might
    // not get used
    inputString(MSG_NAME_COLUMN, config.actions[choiceId].name, MSG_ACTION_NAME);
    if (getUserInputString()[0] == '*') continue;

    Conditions triggerConditions;
    triggerConditions.tray = config.actions[choiceId].triggerConditions.tray;
    triggerConditions.soil = config.actions[choiceId].triggerConditions.soil;
    triggerConditions.logic = config.actions[choiceId].triggerConditions.logic;
    inputConditions(&triggerConditions, 'F', choiceId);
    if (triggerConditions.tray == -1 || triggerConditions.soil == -1 || triggerConditions.logic == -1) continue;

    Conditions stopConditions;
    stopConditions.tray = config.actions[choiceId].stopConditions.tray;
    stopConditions.soil = config.actions[choiceId].stopConditions.soil;
    stopConditions.logic = config.actions[choiceId].stopConditions.logic;
    inputConditions(&stopConditions, 'S', choiceId);
    if (stopConditions.tray == -1 || stopConditions.soil == -1 || stopConditions.logic == -1) continue;

    // If it's editing a live one, won't be able to change FeedFrom, which will default to
    // whatever it's on.
    int8_t feedFrom;
    if (isActionActive(choiceId)) {
      feedFrom = config.actions[choiceId].feedFrom;
    } else {

      setChoices(
        MSG_TOP, FeedFrom::FEED_FROM_TOP,
        MSG_TRAY, FeedFrom::FEED_FROM_TRAY);
      setChoicesHeader(MSG_FEED_FROM);
      feedFrom = (int8_t)selectChoice(2, config.actions[choiceId].feedFrom);

      if (feedFrom == -1) return;
    }

    uint8_t confirmSave = yesOrNo(MSG_SAVE_QUESTION);
    if (confirmSave == -1) {
      return;
    }

    if (confirmSave) {
      // Main "action" fields, only overwritten once saving
      labelcpyFromString(config.actions[choiceId].name, getUserInputString());
      config.actions[choiceId].feedFrom = feedFrom;

      // Apply changes back to config.actions[choiceId].triggerConditions
      config.actions[choiceId].triggerConditions.tray = triggerConditions.tray;
      config.actions[choiceId].triggerConditions.soil = triggerConditions.soil;
      config.actions[choiceId].triggerConditions.logic = triggerConditions.logic;

      // Apply changes back to config.actions[choiceId].stopConditions
      config.actions[choiceId].stopConditions.tray = stopConditions.tray;
      config.actions[choiceId].stopConditions.soil = stopConditions.soil;
      config.actions[choiceId].stopConditions.logic = stopConditions.logic;

      saveConfig();
    }

    break;
  }
}

void settingsSafetyLimits() {
  uint32_t minFeedInterval;
  uint32_t maxFeedTime;
  uint32_t maxPumpOutTime;
  uint32_t pumpOutRestTime;
  int8_t goAhead;

  minFeedInterval = inputNumber(MSG_MINUTES, config.minFeedInterval/1000/60, 30, 0, 600, MSG_LITTLE, MSG_MIN_FEED_INTERVAL);
  if (minFeedInterval == -1) return;

  maxFeedTime = inputNumber(MSG_SECONDS, config.maxFeedTime/1000, 10, 0, 600, MSG_LITTLE, MSG_MAX_FEED_TIME);
  if (maxFeedTime == -1) return;

  maxPumpOutTime = inputNumber(MSG_SECONDS, config.maxPumpOutTime/1000, 10, 0, 600, MSG_LITTLE, MSG_MAX_PUMP_OUT_TIME);
  if (maxPumpOutTime == -1) return;

  pumpOutRestTime = inputNumber(MSG_SECONDS, config.pumpOutRestTime/1000, 5, 0, 600, MSG_LITTLE, MSG_PUMP_OUT_REST_TIME);
  if (pumpOutRestTime == -1) return;
  
  // Normalise to ms
  minFeedInterval = minFeedInterval * 1000L * 60;
  maxFeedTime = maxFeedTime * 1000L;
  maxPumpOutTime = maxPumpOutTime * 1000L;
  pumpOutRestTime = pumpOutRestTime * 1000L;

  lcdClear();

  goAhead = yesOrNo(MSG_SAVE_QUESTION);
  if (goAhead == -1) return;

  if (goAhead) {
    config.minFeedInterval = minFeedInterval;
    config.maxFeedTime = maxFeedTime;
    config.maxPumpOutTime = maxPumpOutTime;
    config.pumpOutRestTime = pumpOutRestTime;

    saveConfig();
  }
}




void settingsCalibrate() {
  bool calibrated;

  while (true) {
    setChoices(MSG_MOISTURE_SENSOR, 0, MSG_WATER_LEVELS, 1);
    setChoicesHeader(MSG_CALIBRATE);
    int8_t choice = selectChoice(1, 0);

    if (choice == -1) return;
    if (choice == 0) calibrated = calibrateSoilMoistureSensor();

    // If "calibrated" is set, the functions above will have
    // changed "config" which will need saving
    if (calibrated) saveConfig();
  }
}

int runInitialSetup() {
  setChoices(MSG_TOP, FeedFrom::FEED_FROM_TOP, MSG_TRAY, FeedFrom::FEED_FROM_TRAY);
  setChoicesHeader(MSG_FEED_FROM);
  config.feedFrom = selectChoice(2, FeedFrom::FEED_FROM_TOP);
  if (config.feedFrom == -1) return false;

  if (config.feedFrom == FeedFrom::FEED_FROM_TOP) {
    config.trayNeedsEmptying = yesOrNo(MSG_AUTO_DRAIN, false);
    if (config.trayNeedsEmptying == -1) return 0;
  } else {
    config.trayNeedsEmptying = false;
  }
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

void setAutoDrain() {
  if (config.feedFrom != FeedFrom::FEED_FROM_TOP) {
    lcdFlashMessage(MSG_ONLY_TOP_FEEDING, MSG_LITTLE, 2000);
    return;
  }
  int8_t trayNeedsEmptying = yesOrNo(MSG_AUTO_DRAIN, config.trayNeedsEmptying);
  if (trayNeedsEmptying == -1) return;

  int8_t confirmation = yesOrNo(MSG_SAVE_QUESTION);
  if (confirmation == -1) return;

  if (confirmation) {
    // Only overwritten once saving
    config.trayNeedsEmptying = trayNeedsEmptying;
    saveConfig();
  }
}

void setFeedFrom() {
  int8_t goAhead;
  //
  setChoices(MSG_TOP, FeedFrom::FEED_FROM_TOP, MSG_TRAY, FeedFrom::FEED_FROM_TRAY);
  setChoicesHeader(MSG_FEEDING_FROM);
  int8_t feedFrom = selectChoice(2, config.feedFrom);
  if (feedFrom == -1) return false;

  if (feedFrom == config.feedFrom) return false;

  goAhead = yesOrNo(MSG_SAVE_QUESTION);
  if (goAhead == -1) return false;

  if (goAhead) {

    // Only overwritten once saving
    config.feedFrom = feedFrom;

    // Zap autofeed for anything but TOP feeding
    if (config.feedFrom != FeedFrom::FEED_FROM_TOP) {
      config.trayNeedsEmptying = false;
    }

    // Zap action field if went this far
    config.activeActionsIndex0 = -1;
    config.activeActionsIndex1 = -1;
    return true;
  } else {
    return false;
  }
}

void activatePumps() {
  int8_t choice;
  while (true) {
    setChoices(
      MSG_PUMP_IN, PUMP_IN_DEVICE,
      MSG_PUMP_OUT, PUMP_OUT_DEVICE
    );
    choice = selectChoice(2, 0);
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