#include <AnalogButtons.h>
#include <Arduino.h>
#include "main.h"
#include "ui.h"
#include "sensors.h"
#include "config.h"
#include "messages.h"
#include "sensors.h"
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

void settings() {
  int8_t choice = 0;

  while (choice != -1) {
    lcdClear();
    setChoices(
      MSG_EDIT_ACTIONS, 1,
      MSG_FEEDING_FROM, 2,
      MSG_AUTO_DRAIN, 3,
      MSG_MOISTURE_LEVELS, 4,
      MSG_CALIBRATE, 5,
      MSG_MAINTENANCE, 10);
    choice = selectChoice(6, 1);

    if (choice == 1) settingsEditActions();
    else if (choice == 2) setFeedFrom();
    else if (choice == 3) setAutoDrain();
    else if (choice == 4) settingsDefaultMoistLevels();
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
      MSG_RESET_ONLY_LOGS, 3,
      MSG_RESET_DATA, 4

    );
    choice = selectChoice(4, 1);

    if (choice == 1) settingsSafetyLimits();
    else if (choice == 2) activatePumps();
    else if (choice == 3) resetOnlyLogs();
    else if (choice == 4) resetData();
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
    MSG_TRAY_EMPTY, Conditions::TRAY_EMPTY,
    MSG_TRAY_SOME, Conditions::TRAY_SOME,
    MSG_TRAY_PLENTY, Conditions::TRAY_PLENTY,
    MSG_TRAY_FULL, Conditions::TRAY_FULL
  );
  if (verb == 'F') {
    setChoicesHeader(MSG_START_TRAY);
  } else {
    setChoicesHeader(MSG_STOP_TRAY);
  }
  int8_t tray = (int8_t)selectChoice(6, (int8_t)initialConditions->tray);
  initialConditions->tray = tray;
  if (tray == -1) return;

  setChoices(
    MSG_TRAY_SOIL_IGNORE, Conditions::SOIL_IGNORED,
    MSG_SOIL_DRY, Conditions::SOIL_DRY,
    MSG_SOIL_LITTLE_MOIST, Conditions::SOIL_LITTLE_MOIST,
    MSG_SOIL_MOIST, Conditions::SOIL_MOIST,
    MSG_SOIL_VERY_MOIST, Conditions::SOIL_VERY_MOIST);

  if (verb == 'F') {
    setChoicesHeader(MSG_START_SOIL);
  } else {
    setChoicesHeader(MSG_STOP_SOIL);
  }

  int8_t soil = (int8_t)selectChoice(5, (int8_t)initialConditions->soil);
  initialConditions->soil = soil;
  if (soil == -1) return;

  int8_t logic;

  if (tray != Conditions::TRAY_IGNORED && soil != Conditions::SOIL_IGNORED) {
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

  minFeedInterval = inputNumber(MSG_MINUTES, config.minFeedInterval/1000/60, 30, 0, 600, MSG_EMPTY, MSG_MIN_FEED_INTERVAL);
  if (minFeedInterval == -1) return;

  maxFeedTime = inputNumber(MSG_SECONDS, config.maxFeedTime/1000, 10, 0, 600, MSG_EMPTY, MSG_MAX_FEED_TIME);
  if (maxFeedTime == -1) return;

  maxPumpOutTime = inputNumber(MSG_SECONDS, config.maxPumpOutTime/1000, 10, 0, 600, MSG_EMPTY, MSG_MAX_PUMP_OUT_TIME);
  if (maxPumpOutTime == -1) return;

  pumpOutRestTime = inputNumber(MSG_SECONDS, config.pumpOutRestTime/1000, 5, 0, 600, MSG_EMPTY, MSG_PUMP_OUT_REST_TIME);
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



void settingsDefaultMoistLevels() {
  int8_t soilLittleMoistPercentage;
  int8_t soilMoistPercentage;
  int8_t soilVeryMoistPercentage;
  int8_t goAhead;

  soilVeryMoistPercentage = inputNumber(MSG_OVER, config.soilVeryMoistPercentage, 5, 0, 95, MSG_PERCENT, MSG_SOIL_WHEN_VERY_MOIST);
  if (soilVeryMoistPercentage == -1) return;

  soilMoistPercentage = inputNumber(MSG_OVER, config.soilMoistPercentage, 5, 0, config.soilMoistPercentage - 5, MSG_PERCENT, MSG_SOIL_WHEN_MOIST);
  if (soilVeryMoistPercentage == -1) return;

  soilLittleMoistPercentage = inputNumber(MSG_OVER, config.soilLittleMoistPercentage, 5, 0, config.soilMoistPercentage - 5, MSG_PERCENT, MSG_SOIL_WHEN_LITTLE_MOIST);
  if (soilVeryMoistPercentage == -1) return;

  lcdClear();

  lcdPrint(MSG_ZERO_PERCENT_DASH);
  lcdPrintNumber(soilLittleMoistPercentage - 1);
  lcdPrint(MSG_PERCENT_DRY);

  lcdPrintNumber(soilLittleMoistPercentage, 1);
  lcdPrint(MSG_PERCENT_DASH);
  lcdPrintNumber(soilMoistPercentage - 1);
  lcdPrint(MSG_PERCENT_SPACE);
  lcdPrint(MSG_SOIL_LITTLE_MOIST);

  lcdPrintNumber(soilMoistPercentage, 2);
  lcdPrint(MSG_PERCENT_DASH);
  lcdPrintNumber(soilVeryMoistPercentage - 1);
  lcdPrint(MSG_PERCENT_SPACE);
  lcdPrint(MSG_SOIL_MOIST);

  lcdPrintNumber(soilVeryMoistPercentage, 3);
  lcdPrint(MSG_PERCENT_DASH_ONEHUNDRED_PERCENT_SPACE);
  lcdPrint(MSG_SOIL_VERY_MOIST);

  delay(5000);
  
  goAhead = yesOrNo(MSG_SAVE_QUESTION);
  if (goAhead == -1) return;

  if (goAhead) {
    config.soilLittleMoistPercentage = soilLittleMoistPercentage;
    config.soilMoistPercentage = soilMoistPercentage;
    config.soilVeryMoistPercentage = soilVeryMoistPercentage;

    saveConfig();
  }
}

void settingsCalibrate() {
  bool calibrated;

  while (true) {
    setChoices(MSG_MOISTURE_SENSOR, 0, MSG_WATER_LEVELS, 1);
    setChoicesHeader(MSG_CALIBRATE);
    int8_t choice = selectChoice(2, 0);

    if (choice == -1) return;
    if (choice == 0) calibrated = calibrateSoilMoistureSensor();
    if (choice == 1) calibrated = calibrateTrayWaterLevelSensors();

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

  setChoices(MSG_PUMP_IN, FeedLine::PUMP_IN, MSG_SOLENOID_IN, FeedLine::SOLENOID_IN);
  setChoicesHeader(MSG_LINE_IN);
  config.feedLine = selectChoice(2, config.feedLine);
  if (config.feedLine == -1) return false;

  if (config.feedFrom == FeedFrom::FEED_FROM_TOP) {
    config.trayNeedsEmptying = yesOrNo(MSG_AUTO_DRAIN, false);
    if (config.trayNeedsEmptying == -1) return 0;
  } else {
    config.trayNeedsEmptying = false;
  }
  if (!calibrateSoilMoistureSensor()) return 0;
  if (!calibrateTrayWaterLevelSensors()) return 0;

  // Save config
  config.mustRunInitialSetup = false;
  saveConfig();

  lcdClear();
  lcdPrint(MSG_DONE);

  return 1;
}

int calibrateTrayWaterLevelSensors() {

  int8_t goAhead;

  int16_t empty;
  int16_t half;
  int16_t full;
  bool trayFull;

  lcdFlashMessage(MSG_SENSOR_2_MM_WATER, MSG_EMPTY, 2000);


  goAhead = giveOk(MSG_WATER_TRAY_SENSOR, MSG_YES_2_MM, MSG_EMPTY, MSG_HOLD_IT_STILL);
  if (goAhead == -1) return false;

  lcdClear();
  lcdPrint(MSG_EMPTY_COLUMN, 1);
  for (int i = 0; i < 8; i++) {
    empty = senseTrayWaterLevel();
    // lcdClear();
    lcdPrintNumber(empty);
    lcdPrint(MSG_SPACE);
    delay(900);
  }

  delay(2000);

  goAhead = giveOk(MSG_WATER_TRAY_SENSOR, MSG_YES_HALF_WAY, MSG_NOW_HALF_WAY, MSG_HOLD_IT_STILL);
  if (goAhead == -1) return false;

  lcdClear();
  lcdPrint(MSG_HALF_COLUMN, 1);
  for (int i = 0; i < 8; i++) {
    half = senseTrayWaterLevel();
    // lcdClear();
    lcdPrintNumber(half);
    lcdPrint(MSG_SPACE);
    delay(900);
  }

  goAhead = giveOk(MSG_WATER_TRAY_SENSOR, MSG_YES_FULL_IN, MSG_NOW_FULL_IN, MSG_DO_NOT_SUBMERGE);
  if (goAhead == -1) return false;

  lcdClear();
  lcdPrint(MSG_FULL_COLUMN, 1);
  for (int i = 0; i < 8; i++) {
    full = senseTrayWaterLevel();
    // lcdClear();
    lcdPrintNumber(full);
    lcdPrint(MSG_SPACE);
    delay(900);
  }

  lcdFlashMessage(MSG_ATTACH_WHITE_SENSOR, MSG_WHERE_LED_TURNS_ON, 2000);

  while (true) {
    goAhead = yesOrNo(MSG_SENSOR_ATTACHED, 1);
    if (goAhead == -1 || !goAhead) return false;

    trayFull = senseTrayIsFull();
    if (!trayFull) {
      lcdFlashMessage(MSG_SENSOR_NOT_SENSING_WATER_1, MSG_SENSOR_NOT_SENSING_WATER_2);
    } else {
      break;
    }
  }

  goAhead = yesOrNo(MSG_SAVE_QUESTION, 1);
  if (goAhead == -1  || !goAhead) return false;

  config.trayWaterLevelSensorCalibrationEmpty = empty;
  config.trayWaterLevelSensorCalibrationHalf = half;
  config.trayWaterLevelSensorCalibrationFull = full;
  return true;
}



int readSensor(int sensor) {
}

int calibrateSoilMoistureSensor() {
  int8_t goAhead;
  int soaked;
  int dry;

  goAhead = giveOk(MSG_MOIST_SENSOR, MSG_YES_ITS_DRY, MSG_ENSURE_SENSOR_IS, MSG_VERY_DRY);
  if (goAhead == -1) return false;

  lcdClear();
  lcdPrint(MSG_DRY_COLUMN, 1);
  for (int i = 0; i < 4; i++) {
    dry = senseSoilMoisture();
    // lcdClear();
    lcdPrintNumber(dry);
    lcdPrint(MSG_SPACE);
    delay(900);
  }

  delay(2000);

  goAhead = giveOk(MSG_MOIST_SENSOR, MSG_YES_ITS_SOAKED, MSG_ENSURE_SENSOR_IS, MSG_VERY_SOAKED);
  if (goAhead == -1) return false;

  lcdClear();
  lcdPrint(MSG_SOAKED_COLUMN, 1);
  for (int i = 0; i < 4; i++) {
    soaked = senseSoilMoisture();
    // lcdClear();
    lcdPrintNumber(soaked);
    lcdPrint(MSG_SPACE);
    delay(900);
  }

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
    lcdFlashMessage(MSG_ONLY_TOP_FEEDING, MSG_EMPTY, 2000);
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

  setChoices(MSG_PUMP_IN, FeedLine::PUMP_IN, MSG_SOLENOID_IN, FeedLine::SOLENOID_IN);
  setChoicesHeader(MSG_LINE_IN);
  int8_t feedLine = selectChoice(2, config.feedLine);
  if (feedLine == -1) return false;


  if (feedFrom == config.feedFrom && feedLine == config.feedLine) return false;

  goAhead = yesOrNo(MSG_SAVE_QUESTION);
  if (goAhead == -1) return false;

  if (goAhead) {

    // Only overwritten once saving
    config.feedFrom = feedFrom;
    config.feedLine = feedLine;

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
      MSG_PUMP_OUT, PUMP_OUT_DEVICE,
      MSG_SOLENOID_IN, SOLENOID_IN_DEVICE);
    choice = selectChoice(3, 0);
    if (choice == -1) return;

  lcdFlashMessage(MSG_DEVICE_WILL_BLINK, MSG_THREE_TIMES, 100);
 
   // pinMode(choice, OUTPUT);
    for(int i = 0; i <= 3; i++) {
      digitalWrite(choice, HIGH);
      delay(1000);
      digitalWrite(choice, LOW);
      delay(1000);
    }
  }
}



/*
int8_t pickAction() {
  uint8_t i = 0, c = 0;
  for (i = 0; i <= ACTIONS_ARRAY_SIZE; i++) {
    if (config.feedFrom == config.actions[i].feedFrom) {
      setChoiceFromString(c, config.actions[i].name, i);
      c++;
    }
  }

  int8_t choice = selectChoice(c, 0);

  return choice;
}

*/