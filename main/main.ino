
#include <AnalogButtons.h>
#include <Arduino.h>
#include "ui.h"
#include "sensors.h"
#include "config.h"
#include "messages.h"
#include "sensors.h"

#include <LiquidCrystal_I2C.h>

// Define the address where the config will be stored in EEPROM

unsigned int feedNumber = 0;

// Config& gConfig = getConfig();
extern Config config;
extern Button *pressedButton;

const unsigned long interval = 2000;  // Interval in milliseconds (1000 milliseconds = 1 second)
unsigned long previousMillis = 0;

void runAction(Action *action, bool force = 0);

/*
datetime start (4)
Event code (1)

Outcome (1)
*/

/*
  TODO: 
    X Set active actions (up to 2)
    X Test that editing action doesn't allow "feedFrom" if active
    X Allow changing of where it's feeding from
    Write calibration procedures for water level
    X Force calibration procedures on setup    
    X Implement "reset"

    X Main screen: show stats, current settings (scrolling), info
      X Make function that works out state of tray and soil uniformly
      X Resolve the knot of how to deal with empty in tray, passing percentage or numbers?

    X Implement built-in action "emptyTray", config to enable it, add it to initial setup for "TOP", UNSET when changing feedFrom
    
    Implement ACTUAL running of actions, with timeout to prevent floods 
    
    LOGS:
    Write infrastructure for stats/logs, logging waterings with date/event, cycling EEPROM 
    Add "View logs" function to browse through logs     

    
    */

void maybeRunActions() {
  if (config.activeActionsIndex0 == -1) runAction(&config.actions[config.activeActionsIndex0]);
  if (config.activeActionsIndex1 == -1) runAction(&config.actions[config.activeActionsIndex1]);
}

bool actionShouldStart(Action *action) {
  return true;
}

bool actionShouldStop(Action *action) {
  return true;
}

void runAction(Action *action, bool force = 0) {
  return;
  static unsigned long lastExecutionTime = 0;

  if (millis() - lastExecutionTime >= FEED_MIN_INTERVAL) {
    lastExecutionTime = millis();
  }

  if (millis() - lastExecutionTime < MAX_FEED_TIME || force) {  // Run for at most 10 seconds


    if (!actionShouldStart(action)) return;

    // LOG: Starting feed. Time from last feed: X
    digitalWrite(PUMP_OUT_DEVICE, HIGH);

    unsigned long feedStartTime = 0;
    bool feedRunning = false;

    feedStartTime = millis();

    while (true) {

      uint8_t shouldStop = 0;
      if (actionShouldStop(action)) shouldStop = 1;
      if (millis() - feedStartTime >= MAX_FEED_TIME) shouldStop = 2;


      if (shouldStop) {
        digitalWrite(PUMP_OUT_DEVICE, LOW);
        // LOG: Feed finished. Timestamp. Duration.
        return;
      }
    }
  }
}


void emptyTrayIfNecessary() {
  static unsigned long lastExecutionTime = 0;
  static bool isRunning = false;

  // Ensure at most 10 seconds of execution per minute
  if (millis() - lastExecutionTime >= PUMP_REST_TIME && !isRunning) {
    // If a minute has passed and the function is not already running, reset last execution time
    lastExecutionTime = millis();
  }

  if (millis() - lastExecutionTime < MAX_PUMP_TIME) {  // Run for at most 10 seconds
    uint8_t trayState;
    unsigned long pumpStartTime = 0;
    bool pumpRunning = false;

    if (config.trayNeedsEmptying) {
      digitalWrite(PUMP_OUT_DEVICE, LOW);
      while (true) {
        trayState = trayWaterLevelAsState(trayWaterLevelAsPercentage(senseTrayWaterLevel()));

        // Tray is empty: it's all done
        if (trayState == Conditions::TRAY_DRY || trayState == Conditions::TRAY_EMPTY) {
          digitalWrite(PUMP_OUT_DEVICE, LOW);
          return;
        }
        // Check if the pump is not already running
        if (!pumpRunning) {
          // Start the pump and record the start time
          lcdClear();
          lcdPrint(MSG_EXTRACTING);
          digitalWrite(PUMP_OUT_DEVICE, HIGH);  // Activate the pump
          pumpStartTime = millis();
          pumpRunning = true;
        } else {
          // Check if the pump has been running for more than 10 seconds
          if (millis() - pumpStartTime >= MAX_PUMP_TIME) {  // 10 seconds
            // Deactivate the pump and reset pump state
            digitalWrite(PUMP_OUT_DEVICE, LOW);  // Deactivate the pump
            lcdClear();
            return;
          }
        }
      }
    }
  }
}


void setup() {
  Serial.begin(9600);  // Initialize serial communication at 9600 baud rate

  initLcdAndButtons();

  extern LiquidCrystal_I2C lcd;

  initSensors();

  loadConfig();

  if (!configChecksumCorrect()) {
    // Serial.println("CHECKSUM INCORRECT?!?");
    restoreDefaultConfig();
    saveConfig();
  }

  // runInitialSetup();

  if (config.mustRunInitialSetup) {
    while (!runInitialSetup())
      ;
  }
}
#include <LiquidCrystal_I2C.h>

extern LiquidCrystal_I2C lcd;

uint8_t screenCounter = 0;

void loop() {

  // Serial.println("HERE 2");
  unsigned long currentMillis = millis();


  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    screenCounter = (screenCounter + 1) % 8;
    maybeRunActions();
    emptyTrayIfNecessary();
    displayInfo(screenCounter);
  }


  analogButtonsCheck();
  if (pressedButton != nullptr) {
    pressedButton = nullptr;
    mainMenu();
  }
}

void viewLogs() {
}

void activatePumps() {
  int8_t choice;
  while (true) {
    setChoices2(
      MSG_PUMP_IN2, 0,
      MSG_PUMP_OUT2, 1,
      MSG_SOLENOID_IN2, 2);
    choice = selectChoice2(3, 0);
    if (choice == -1) return;

    // TODO: FINISH HERE
  }
}

void mainMenu() {
  int8_t choice;
  do {
    setChoices2(
      MSG_ACTIVE_ACTIONS2, 1,
      MSG_LOGS2, 2,
      MSG_FORCE_ACTION2, 3,
      MSG_SETTINGS2, 4);

    choice = selectChoice2(4, 1);

    if (choice == 1) setActiveActions();
    if (choice == 2) viewLogs();
    if (choice == 3) forceAction();
    else if (choice == 4) settings();
  } while (choice != -1);
}

void forceAction() {
  uint8_t i = 0;
  int8_t index = 0;
  if (config.activeActionsIndex0 == -1 && config.activeActionsIndex1 == -1) {
    lcdFlashMessage("No action set!");
  }

  if (config.activeActionsIndex0 != -1) {
    setChoice(i, config.actions[config.activeActionsIndex0].name, i);
    i++;
  }

  if (config.activeActionsIndex1 != -1) {
    setChoice(i, config.actions[config.activeActionsIndex1].name, i);
    i++;
  }

  index = selectChoice2(2, 0, MSG_PICK_ACTION2);
  if (index == -1) return;
  runAction(&config.actions[index], true);
}

void displayInfo(uint8_t screen) {
  switch (screen) {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
      displayInfo1(screen);
      break;
    case 6:
      displayInfo2(screen);
      break;
    case 7:
      displayInfo2(screen);
      break;
  }
}

void displayInfo1(uint8_t screen) {
  uint16_t soilMoisture;
  uint8_t soilMoisturePercent;
  uint8_t trayWaterLevelPercent;

  uint16_t trayWaterLevel;
  bool trayIsFull;

  soilMoisture = senseSoilMosture();
  soilMoisturePercent = soilMoistureAsPercentage(soilMoisture);
  trayWaterLevel = senseTrayWaterLevel();
  trayWaterLevelPercent = trayWaterLevelAsPercentage(trayWaterLevel);
  trayIsFull = senseTrayIsFull();

  lcdClear();
  lcdPrint(MSG_SOIL, 0);
  lcdPrint(MSG_SPACE);

  lcdPrintNumber(soilMoisturePercent);
  lcdPrint(MSG_PERCENT);
  lcdPrint(MSG_SPACE);
  lcdPrint(soilMoistureInEnglish(soilMoistureAsState(soilMoisturePercent)));
  lcdPrint(MSG_SPACE);

  lcdPrint(MSG_TRAY, 1);
  lcdPrint(MSG_SPACE);
  lcdPrintNumber(trayWaterLevelPercent);
  lcdPrint(MSG_PERCENT);
  lcdPrint(MSG_SPACE);

  lcdPrint(trayWaterLevelInEnglish(trayWaterLevelAsState(trayWaterLevelPercent), trayIsFull));
}

char *trayConditionToEnglish(uint8_t condition) {
  if (condition == Conditions::TRAY_DRY) return MSG_TRAY_DRY;
  else if (condition == Conditions::TRAY_EMPTY) return MSG_TRAY_EMPTY;
  else if (condition == Conditions::TRAY_SOME) return MSG_TRAY_SOME;
  else if (condition == Conditions::TRAY_PLENTY) return MSG_TRAY_PLENTY;
  else return MSG_EMPTY;
}


char *soilConditionToEnglish(uint8_t condition) {
  if (condition == Conditions::SOIL_DRY) return MSG_SOIL_DRY;
  else if (condition == Conditions::SOIL_LITTLE_MOIST) return MSG_SOIL_LITTLE_MOIST;
  else if (condition == Conditions::SOIL_MOIST) return MSG_SOIL_MOIST;
  else if (condition == Conditions::SOIL_VERY_MOIST) return MSG_SOIL_VERY_MOIST;
  else return MSG_EMPTY;
}

void displayTriggerConditions(Conditions conditions) {
  if (conditions.tray != Conditions::TRAY_IGNORED) {
    lcdPrint(MSG_TRAY);
    lcdPrint(MSG_SPACE);
    lcdPrint(trayConditionToEnglish(conditions.tray));
  }

  if (conditions.logic != Conditions::NO_LOGIC) {
    if (conditions.logic == Conditions::TRAY_OR_SOIL) {
      lcdPrint(MSG_OR);
    } else if (conditions.logic == Conditions::TRAY_AND_SOIL) {
      lcdPrint(MSG_AND);
    }
  }
  if (conditions.soil != Conditions::SOIL_IGNORED) {
    lcdPrint(MSG_SOIL);
    lcdPrint(MSG_SPACE);
    lcdPrint(soilConditionToEnglish(conditions.soil));
  }
}

void displayInfo2(uint8_t screen) {
  lcdClear();
  int8_t active;

  if (config.activeActionsIndex0 == -1 && config.activeActionsIndex1 == -1) {
    active = -1;
  } else {
    if (config.activeActionsIndex0 == -1) {
      active = config.activeActionsIndex1;
    } else if (config.activeActionsIndex1 == -1) {
      active = config.activeActionsIndex0;
    } else {
      if (screen % 2 == 0) {
        active = config.activeActionsIndex0;
      } else {
        active = config.activeActionsIndex1;
      }
    }
  }

  if (active == -1) {
    lcdPrint(MSG_INACTIVE);
  } else {
    lcdPrint(MSG_ON_WHEN);
    lcdPrint(MSG_SPACE);
    lcdPrint(MSG_BR_OPEN);
    lcdPrint(config.actions[active].name);
    lcdPrint(MSG_BR_CLOSED);
    lcd.setCursor(0, 1);

    displayTriggerConditions(config.actions[active].triggerConditions);

    lcd.setCursor(0, 2);
    lcdPrint(MSG_OFF_WHEN);
    lcd.setCursor(0, 3);
    displayTriggerConditions(config.actions[active].stopConditions);
  }
}

void displayInfo3(uint8_t screen) {
  lcdClear();
}

void displayInfo4(uint8_t screen) {
  lcdClear();
}

int8_t pickAction() {
  uint8_t i = 0, c = 0;
  for (i = 0; i <= 5; i++) {
    if (config.feedFrom == config.actions[i].feedFrom) {
      setChoice(c, config.actions[i].name, i);
      c++;
    }
  }

  int8_t choice = selectChoice2(c, 0);

  return choice;
}



void pickAndRunAction() {
  int8_t choice = pickAction();

  // List appropriate actions
  // Pick one
  // run runAction(int) on the picked action
}

void setActiveActions() {
  int8_t slot;
  int8_t choice;

  while (true) {
    setChoice(0, config.activeActionsIndex0 == -1 ? MSG_EMPTY_SLOT : config.actions[config.activeActionsIndex0].name, 0);
    setChoice(1, config.activeActionsIndex1 == -1 ? MSG_EMPTY_SLOT : config.actions[config.activeActionsIndex1].name, 1);

    slot = selectChoice2(2, 0, MSG_PICK_A_SLOT2);
    if (slot == -1) return;

    setChoice(0, MSG_EMPTY_SLOT, 127);
    uint8_t i = 1, c = 1;
    for (i = 0; i <= 5; i++) {
      if (config.feedFrom == config.actions[i].feedFrom) {
        setChoice(c, config.actions[i].name, i);
        c++;
      }
    }

    choice = selectChoice2(c, 127);
    if (choice == -1) continue;

    if (choice == 127) choice = -1;
    if (slot == 0) config.activeActionsIndex0 = choice;
    if (slot == 1) config.activeActionsIndex1 = choice;
    saveConfig();
  }
}


void resetData() {
  if (confirm2(MSG_SURE_QUESTION2)) {
    restoreDefaultConfig();
    saveConfig();
    while (!runInitialSetup())
      ;
  }
}

void setAutoDrain() {
  if (config.feedFrom != FeedFrom::FEED_FROM_TOP) {
    lcdFlashMessage(MSG_ONLY_TOP_FEEDING, MSG_EMPTY, 2000);
  }
  bool trayNeedsEmptying = confirm2(MSG_AUTO_DRAIN2, config.trayNeedsEmptying);

  if (confirm2(MSG_SAVE_QUESTION2)) {
    // Only overwritten once saving
    config.trayNeedsEmptying = trayNeedsEmptying;
  }
}

void setFeedFrom() {
  //
  setChoices2(MSG_TOP2, FeedFrom::FEED_FROM_TOP, MSG_TRAY2, FeedFrom::FEED_FROM_TRAY);
  int8_t feedFrom = selectChoice2(2, config.feedFrom, MSG_FEEDING_FROM);
  if (feedFrom == -1) return;

  setChoices2(MSG_PUMP_IN2, FeedLine::PUMP_IN, MSG_SOLENOID_IN2, FeedLine::SOLENOID_IN);
  int8_t feedLine = selectChoice2(2, config.feedLine, MSG_LINE_IN2);
  if (feedLine == -1) return;


  if (feedFrom == config.feedFrom && feedLine == config.feedLine) return;

  if (confirm2(MSG_SAVE_QUESTION2)) {

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

void settings() {
  int8_t choice = 0;

  while (choice != -1) {
    lcdClear();
    setChoices2(
      MSG_EDIT_ACTIONS2, 1,
      MSG_FEEDING_FROM, 2,
      MSG_AUTO_DRAIN2, 3,
      MSG_MOISTURE_LEVELS, 4,
      MSG_CALIBRATE, 5,
      MSG_MAINTENANCE2, 10);
    choice = selectChoice2(6, 1);

    /*
    "Test pumps/sols", 2,
    if (choice == 2) activatePumps();
*/
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
    setChoices2(
      MSG_RESET_DATA2, 1,
      MSG_TEST_PUMPS2, 2,
      MSG_RUN_ANY_ACTIONS2, 3);
    choice = selectChoice2(3, 1);

    if (choice == 1) resetData();
    else if (choice == 2) activatePumps();
    else if (choice == 3) pickAndRunAction();
  }
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

Conditions inputConditions(Conditions *initialConditions, char *verb, int8_t choiceId) {

  char message[20];
  strcpy(message, "XXXX when YYYY is:");
  memcpy(message, verb, 4);

  setChoices2(
    MSG_TRAY_SOIL_IGNORE2, Conditions::TRAY_IGNORED,
    MSG_TRAY_DRY2, Conditions::TRAY_DRY,
    MSG_TRAY_EMPTY2, Conditions::TRAY_EMPTY,
    MSG_TRAY_SOME2, Conditions::TRAY_SOME,
    MSG_TRAY_PLENTY2, Conditions::TRAY_PLENTY);
  memcpy(message + 10, MSG_TRAY, 4);
  int8_t tray = (int8_t)selectChoice(5, (int8_t)initialConditions->tray, message);
  initialConditions->tray = tray;
  if (tray == -1) return;

  setChoices2(
    MSG_TRAY_SOIL_IGNORE2, Conditions::SOIL_IGNORED,
    MSG_SOIL_DRY2, Conditions::SOIL_DRY,
    MSG_SOIL_LITTLE_MOIST2, Conditions::SOIL_LITTLE_MOIST,
    MSG_SOIL_MOIST2, Conditions::SOIL_MOIST,
    MSG_SOIL_VERY_MOIST2, Conditions::SOIL_VERY_MOIST);
  memcpy(message + 10, MSG_SOIL, 4);
  int8_t soil = (int8_t)selectChoice(5, (int8_t)initialConditions->soil, message);
  initialConditions->soil = soil;
  if (soil == -1) return;

  int8_t logic;

  if (tray != Conditions::TRAY_IGNORED && soil != Conditions::SOIL_IGNORED) {
    setChoices2(
      MSG_BOTH2, Conditions::TRAY_AND_SOIL,
      MSG_EITHER2, Conditions::TRAY_AND_SOIL);
    logic = (int8_t)selectChoice2(2, (int8_t)initialConditions->logic, MSG_LOGIC2);
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
    for (i = 0; i <= 5; i++) setChoice(i, config.actions[i].name, i);

    // Pick one. If -1, then get out of this menu with the "break"
    int8_t choiceId = selectChoice2(i, 0);
    if (choiceId == -1) break;

    // Editing of the choice's name
    // Note: until saved, the value will stay in userInputString
    // I don't want to waste 20 bytes to store something that might
    // not get used
    inputString("Name:", config.actions[choiceId].name, "Action name");
    if (getUserInputString()[0] == '*') continue;

    Conditions triggerConditions;
    triggerConditions.tray = config.actions[choiceId].triggerConditions.tray;
    triggerConditions.soil = config.actions[choiceId].triggerConditions.soil;
    triggerConditions.logic = config.actions[choiceId].triggerConditions.logic;
    inputConditions(&triggerConditions, "Feed", choiceId);
    if (triggerConditions.tray == -1 || triggerConditions.soil == -1 || triggerConditions.logic == -1) continue;

    Conditions stopConditions;
    stopConditions.tray = config.actions[choiceId].stopConditions.tray;
    stopConditions.soil = config.actions[choiceId].stopConditions.soil;
    stopConditions.logic = config.actions[choiceId].stopConditions.logic;
    inputConditions(&stopConditions, "Stop", choiceId);
    if (stopConditions.tray == -1 || stopConditions.soil == -1 || stopConditions.logic == -1) continue;

    // If it's editing a live one, won't be able to change FeedFrom, which will default to
    // whatever it's on.
    int8_t feedFrom;
    if (isActionActive(choiceId)) {
      feedFrom = config.actions[choiceId].feedFrom;
    } else {

      setChoices2(
        MSG_TOP2, FeedFrom::FEED_FROM_TOP,
        MSG_TRAY2, FeedFrom::FEED_FROM_TRAY);
      feedFrom = (int8_t)selectChoice2(2, config.actions[choiceId].feedFrom, MSG_FEED_FROM2);

      if (feedFrom == -1) return;
    }

    if (confirm2(MSG_SAVE_QUESTION2)) {

      // Main "action" fields, only overwritten once saving
      labelcpy(config.actions[choiceId].name, getUserInputString());
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

  soilVeryMoistPercentage = inputNumber("Over ", config.soilVeryMoistPercentage, 5, 0, 95, "%", "Very moist");
  if (soilVeryMoistPercentage == -1) return;

  soilMoistPercentage = inputNumber("Over ", config.soilMoistPercentage, 5, 0, config.soilVeryMoistPercentage - 5, "%", "Moist");
  if (soilVeryMoistPercentage == -1) return;

  soilLittleMoistPercentage = inputNumber("Over ", config.soilLittleMoistPercentage, 5, 0, config.soilMoistPercentage - 5, "%", "Little moist");
  if (soilVeryMoistPercentage == -1) return;

  lcdClear();

  lcdPrint("0\%-", 0);
  lcdPrintNumber(soilLittleMoistPercentage - 1);
  lcdPrint("% Dry");

  lcdPrintNumber(soilLittleMoistPercentage, 1);
  lcdPrint("%-");
  lcdPrintNumber(soilMoistPercentage - 1);
  lcdPrint("% ");
  lcdPrint(MSG_SOIL_LITTLE_MOIST);

  lcdPrintNumber(soilMoistPercentage, 2);
  lcdPrint("%-");
  lcdPrintNumber(soilVeryMoistPercentage - 1);
  lcdPrint("% ");
  lcdPrint(MSG_SOIL_MOIST);

  lcdPrintNumber(soilVeryMoistPercentage, 3);
  lcdPrint("%-100% ");
  lcdPrint(MSG_SOIL_VERY_MOIST);

  delay(2000);
  if (confirm2(MSG_SAVE_QUESTION2)) {
    config.soilLittleMoistPercentage = soilLittleMoistPercentage;
    config.soilMoistPercentage = soilMoistPercentage;
    config.soilVeryMoistPercentage = soilVeryMoistPercentage;

    saveConfig();
  }
}

void settingsCalibrate() {
  bool calibrated;

  while (true) {
    setChoices2(MSG_MOISTURE_SENSOR2, 0, MSG_WATER_LEVELS2, 1);
    int8_t choice = selectChoice2(2, 0, MSG_CALIBRATE);

    if (choice == -1) return;
    if (choice == 0) calibrated = calibrateSoilMoistureSensor();
    if (choice == 1) calibrated = calibrateTrayWaterLevelSensors();

    // If "calibrated" is set, the functions above will have
    // changed "config" which will need saving
    if (calibrated) saveConfig();
  }
}

int runInitialSetup() {
  setChoices2(MSG_TOP2, FeedFrom::FEED_FROM_TOP, MSG_TRAY2, FeedFrom::FEED_FROM_TRAY);
  config.feedFrom = selectChoice2(2, FeedFrom::FEED_FROM_TOP, MSG_FEED_FROM2);
  if (config.feedFrom == -1) return false;

  setChoices2(MSG_PUMP_IN2, FeedLine::PUMP_IN, MSG_SOLENOID_IN2, FeedLine::SOLENOID_IN);
  config.feedLine = selectChoice2(2, config.feedLine, MSG_LINE_IN2);
  if (config.feedLine == -1) return false;

  if (config.feedFrom == FeedFrom::FEED_FROM_TOP) {
    config.trayNeedsEmptying = confirm2(MSG_AUTO_DRAIN2, false);
  } else {
    config.trayNeedsEmptying = false;
  }
  if (!calibrateSoilMoistureSensor()) return 0;
  if (!calibrateTrayWaterLevelSensors()) return 0;

  // Save config
  config.mustRunInitialSetup = false;
  saveConfig();

  lcdClear();
  lcdPrint("Done!");

  return 1;
}

int calibrateTrayWaterLevelSensors() {

  uint8_t goAhead;

  int16_t empty;
  int16_t half;
  int16_t full;
  bool trayFull;

  lcdFlashMessage(MSG_SENSOR_2_MM_WATER, MSG_EMPTY, 2000);


  goAhead = confirm2(MSG_TINY_BIT_OF_WATER2, 1);
  if (!goAhead) {
    lcdFlashMessage(MSG_ABORTED);
    return false;
  }
  lcdClear();
  lcdPrint("Empty: ", 1);
  for (int i = 0; i < 8; i++) {
    empty = senseTrayWaterLevel();
    // lcdClear();
    lcdPrintNumber(empty);
    lcdPrint(MSG_SPACE);
    delay(900);
  }

  delay(2000);

  goAhead = confirm2(MSG_HALF_TRAY2, 1);
  if (!goAhead) {
    lcdFlashMessage(MSG_ABORTED);
    return false;
  }

  lcdClear();
  lcdPrint("Half: ", 1);
  for (int i = 0; i < 8; i++) {
    half = senseTrayWaterLevel();
    // lcdClear();
    lcdPrintNumber(half);
    lcdPrint(MSG_SPACE);
    delay(900);
  }

  goAhead = confirm2(MSG_FULL_TRAY2, 1);
  if (!goAhead) {
    lcdFlashMessage(MSG_ABORTED);
    return false;
  }
  lcdClear();
  lcdPrint("Full: ", 1);
  for (int i = 0; i < 8; i++) {
    full = senseTrayWaterLevel();
    // lcdClear();
    lcdPrintNumber(full);
    lcdPrint(MSG_SPACE);
    delay(900);
  }

  lcdFlashMessage("Attach white sensor", "where LED turns on", 2000);

  while (true) {
    goAhead = confirm2(MSG_SENSOR_ATTACHED2, 1);
    if (!goAhead) {
      lcdFlashMessage(MSG_ABORTED);
      return false;
    }

    trayFull = senseTrayIsFull();
    if (!trayFull) {
      lcdFlashMessage("Sensor not sensing", "water");
    } else {
      break;
    }
  }

  // Confirm to save
  if (confirm2(MSG_SAVE_QUESTION2)) {
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

  goAhead = confirm2(MSG_DRY_MOIST_SENSOR2, 1);
  if (!goAhead) {
    lcdFlashMessage(MSG_ABORTED);
    return;
  }
  lcdClear();
  lcdPrint("Dry: ", 1);
  for (int i = 0; i < 4; i++) {
    dry = senseSoilMosture();
    // lcdClear();
    lcdPrintNumber(dry);
    lcdPrint(" ");
    delay(900);
  }

  delay(2000);

  goAhead = confirm2(MSG_SOAKED_MOIST_SENSOR2, 1);
  if (!goAhead) {
    lcdFlashMessage(MSG_ABORTED);
    return;
  }

  lcdClear();
  lcdPrint("Soaked: ", 1);
  for (int i = 0; i < 4; i++) {
    soaked = senseSoilMosture();
    // lcdClear();
    lcdPrintNumber(soaked);
    lcdPrint(" ");
    delay(900);
  }

  // Confirm to save
  if (confirm2(MSG_SAVE_QUESTION2)) {
    config.moistSensorCalibrationSoaked = soaked;
    config.moistSensorCalibrationDry = dry;
    return true;
  } else {
    return false;
  }
}