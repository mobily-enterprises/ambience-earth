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
#include <LiquidCrystal_I2C.h>

extern LiquidCrystal_I2C lcd;

extern Config config;
extern LogEntry currentLogEntry;
extern LogEntry newLogEntry;


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
      MSG_RESET_DATA, 1,
      MSG_RESET_ONLY_LOGS, 2,
      MSG_TEST_PUMPS, 3

    );
    choice = selectChoice(3, 1);

    if (choice == 1) resetData();
    else if (choice == 2) resetOnlyLogs();
    else if (choice == 3) activatePumps();
  }
}

void resetOnlyLogs() {
  uint8_t confirmWipe;

  confirmWipe = confirm(MSG_SURE_QUESTION);
  if (confirmWipe == -1) return;

  if (confirmWipe) {
    lcdClear();
    lcdPrint(MSG_WIPING, 2);
    wipeLogs();
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
    MSG_TRAY_PLENTY, Conditions::TRAY_PLENTY);
  if (verb == 'F') {
    setChoicesHeader(MSG_START_TRAY);
  } else {
    setChoicesHeader(MSG_STOP_TRAY);
  }
  int8_t tray = (int8_t)selectChoice(5, (int8_t)initialConditions->tray);
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

    uint8_t confirmSave = confirm(MSG_SAVE_QUESTION);
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

void settingsDefaultMoistLevels() {
  int8_t soilLittleMoistPercentage;
  int8_t soilMoistPercentage;
  int8_t soilVeryMoistPercentage;

  soilVeryMoistPercentage = inputNumber(MSG_OVER, config.soilVeryMoistPercentage, 5, 0, 95, MSG_PERCENT, MSG_SOIL_VERY_MOIST);
  if (soilVeryMoistPercentage == -1) return;

  soilMoistPercentage = inputNumber(MSG_OVER, config.soilMoistPercentage, 5, 0, config.soilMoistPercentage - 5, MSG_PERCENT, MSG_SOIL_MOIST);
  if (soilVeryMoistPercentage == -1) return;

  soilLittleMoistPercentage = inputNumber(MSG_OVER, config.soilLittleMoistPercentage, 5, 0, config.soilMoistPercentage - 5, MSG_PERCENT, MSG_SOIL_LITTLE_MOIST);
  if (soilVeryMoistPercentage == -1) return;

  lcdClear();

  lcd.setCursor(0, 0);
  lcdPrint(MSG_ZERO_PERCENT_DASH);
  lcdPrintNumber(soilLittleMoistPercentage - 1);
  lcdPrint(MSG_PERCENT_DRY);

  lcdPrintNumber(soilLittleMoistPercentage, 1);
  lcdPrint(MSG_PERCENT_DASH);
  lcdPrintNumber(soilMoistPercentage - 1);
  lcdPrint(MSG_PERCENT_SPACE);
  lcdPrint(MSG_SOIL_LITTLE_MOIST);

  lcdPrintNumber(soilMoistPercentage, 2);
  lcd.print(MSG_PERCENT_DASH);
  lcdPrintNumber(soilVeryMoistPercentage - 1);
  lcd.print(MSG_PERCENT_SPACE);
  lcdPrint(MSG_SOIL_MOIST);

  lcdPrintNumber(soilVeryMoistPercentage, 3);
  lcdPrint(MSG_PERCENT_DASH_ONEHUNDRED_PERCENT_SPACE);
  lcdPrint(MSG_SOIL_VERY_MOIST);

  delay(2000);
  if (confirm(MSG_SAVE_QUESTION)) {
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
    config.trayNeedsEmptying = confirm(MSG_AUTO_DRAIN, false);
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

  lcdFlashMessage(MSG_SENSOR__MM_WATER2, MSG_EMPTY, 2000);


  goAhead = confirm(MSG_TINY_BIT_OF_WATER, 1);
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

  goAhead = confirm(MSG_HALF_TRAY, 1);
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

  goAhead = confirm(MSG_FULL_TRAY, 1);
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
    goAhead = confirm(MSG_SENSOR_ATTACHED, 1);
    if (goAhead == -1) return false;


    trayFull = senseTrayIsFull();
    if (!trayFull) {
      lcdFlashMessage(MSG_SENSOR_NOT_SENSING_WATER_1, MSG_SENSOR_NOT_SENSING_WATER_2);
    } else {
      break;
    }
  }

  // Confirm to save
  if (confirm(MSG_SAVE_QUESTION)) {
    config.trayWaterLevelSensorCalibrationEmpty = empty;
    config.trayWaterLevelSensorCalibrationHalf = half;
    config.trayWaterLevelSensorCalibrationFull = full;
    return true;

  } else {
    return false;
  }
}



int readSensor(int sensor) {
}

int calibrateSoilMoistureSensor() {
  int goAhead;
  int soaked;
  int dry;


  goAhead = confirm(MSG_DRY_MOIST_SENSOR, 1);
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

  goAhead = confirm(MSG_SOAKED_MOIST_SENSOR, 1);
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

  // Confirm to save
  if (confirm(MSG_SAVE_QUESTION)) {
    config.moistSensorCalibrationSoaked = soaked;
    config.moistSensorCalibrationDry = dry;
    return true;
  } else {
    return false;
  }
}

void resetData() {
  int8_t reset = confirm(MSG_SURE_QUESTION);

  if (reset == -1) return;

  if (reset) {
    restoreDefaultConfig();
    saveConfig();
    wipeLogs();
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
  int8_t trayNeedsEmptying = confirm(MSG_AUTO_DRAIN, config.trayNeedsEmptying);
  if (trayNeedsEmptying == -1) return;

  int8_t confirmation = confirm(MSG_SAVE_QUESTION);
  if (confirmation == -1) return;

  if (confirmation) {
    // Only overwritten once saving
    config.trayNeedsEmptying = trayNeedsEmptying;
    saveConfig();
  }
}

void setFeedFrom() {
  //
  setChoices(MSG_TOP, FeedFrom::FEED_FROM_TOP, MSG_TRAY, FeedFrom::FEED_FROM_TRAY);
  setChoicesHeader(MSG_FEEDING_FROM);
  int8_t feedFrom = selectChoice(2, config.feedFrom);
  if (feedFrom == -1) return;

  setChoices(MSG_PUMP_IN, FeedLine::PUMP_IN, MSG_SOLENOID_IN, FeedLine::SOLENOID_IN);
  setChoicesHeader(MSG_LINE_IN);
  int8_t feedLine = selectChoice(2, config.feedLine);
  if (feedLine == -1) return;


  if (feedFrom == config.feedFrom && feedLine == config.feedLine) return;

  if (confirm(MSG_SAVE_QUESTION)) {

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

    digitalWrite(choice, HIGH);
    alert(MSG_PRESS_OK_TO_STOP);
    digitalWrite(choice, LOW);
    return;
  }
}

void createBootLogEntry() {
  unsigned long bootedUpMillis = millis();
  uint8_t soilMoistureBefore = soilMoistureAsPercentage(senseSoilMoisture());
  uint8_t trayWaterLevelBefore = trayWaterLevelAsPercentage(senseTrayWaterLevel());

  clearLogEntry((void *)&newLogEntry);
  newLogEntry.millisStart = bootedUpMillis;
  newLogEntry.millisEnd = millis();
  newLogEntry.entryType = 0;  // BOOTED UP
  newLogEntry.actionId = 7;   // NOT RELEVANT
  newLogEntry.soilMoistureBefore = soilMoistureBefore;
  newLogEntry.trayWaterLevelBefore = trayWaterLevelBefore;
  newLogEntry.soilMoistureAfter = 0;
  newLogEntry.trayWaterLevelAfter = 0;
  newLogEntry.topFeed = 0;
  newLogEntry.outcome = 0;

  writeLogEntry((void *)&newLogEntry);
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