
#include <AnalogButtons.h>
#include <Arduino.h>
#include "ui.h"
#include "sensors.h"
#include "config.h"
#include "messages.h"
#include "sensors.h"
#include "logs.h"

#include <LiquidCrystal_I2C.h>

/*
  Make it log properly
  Make it possible to view logs
  Have extra screen with extra info:
    Average time between feed
    Time since last feed
*/

// Define the address where the config will be stored in EEPROM

unsigned int feedNumber = 0;

// Config& gConfig = getConfig();
extern Config config;
extern Button *pressedButton;

typedef struct {
  unsigned int seq : 8;
  unsigned long millisStart;
  unsigned long millisEnd;
  unsigned int actionId : 3;
  unsigned int trayWaterLevelBefore : 7;
  unsigned int soilMoistureBefore : 7;
  bool topFeed : 1;
  unsigned int outcome : 4;
  unsigned int trayWaterLevelAfter : 7;
  unsigned int soilMoistureAfter : 7;
  unsigned int padding1 : 14;
} LogEntry;

LogEntry currentLogEntry;
LogEntry newLogEntry;

extern Button upButton;
extern Button leftButton;
extern Button downButton;
extern Button rightButton;
extern Button okButton;


const unsigned long interval = 2000;  // Interval in milliseconds (1000 milliseconds = 1 second)
unsigned long previousMillis = 0;

enum PumpState { IDLE, PUMPING, COMPLETED };


void runAction(Action *action, bool force = 0);
void mainMenu();
int runInitialSetup();
void displayInfo(uint8_t screen);

void maybeRunActions() {
  if (config.activeActionsIndex0 != -1) runAction(&config.actions[config.activeActionsIndex0]);
  if (config.activeActionsIndex1 != -1) runAction(&config.actions[config.activeActionsIndex1]);
}

bool actionShouldStartOrStop(Action *action, bool start = true) {
  Conditions* c;
    c = start ? &action->triggerConditions : &action->stopConditions;

  uint8_t trayState = trayWaterLevelAsState(trayWaterLevelAsPercentage(senseTrayWaterLevel()), senseTrayIsFull());
  uint8_t soilState = soilMoistureAsState(soilMoistureAsPercentage(senseSoilMoisture()));

  /*
  Serial.println("B");
  Serial.println(trayState);
  Serial.println(soilState);
  */

  uint8_t actionTrayState = c->tray;
  uint8_t actionSoilState = c->soil;
  uint8_t actionLogic = c->logic;

  /*
  Serial.println("C");
  Serial.println(actionTrayState);
  Serial.println(actionSoilState);
  Serial.println(actionLogic);
  */

  bool satisfied = false;

  bool traySatisfied;
  bool soilSatisfied;
  if (start) {
    traySatisfied = trayState <= actionTrayState;
    soilSatisfied = soilState <= actionSoilState;
  } else {
    traySatisfied = trayState >= actionTrayState;
    soilSatisfied = soilState >= actionSoilState;
  }

  /*
  Serial.println("D");
  Serial.println(traySatisfied);
  Serial.println(soilSatisfied);
  */

  bool r;
  if (!actionTrayState && !actionSoilState) r = false;
  else if (!actionTrayState && actionSoilState) r = soilSatisfied;
  else if (actionTrayState && !actionSoilState) r = traySatisfied;
  else if (actionLogic == Conditions::TRAY_OR_SOIL) r = soilSatisfied || traySatisfied;
  else /* if (actionLogic == Conditions::TRAY_AND_SOIL)*/ r = soilSatisfied && traySatisfied;  

  /*
  Serial.println("E");
  Serial.println(r);
  */

  return r;
}


void runAction(Action *action, bool force = 0) { 
  static unsigned long lastExecutionTime = 0;

  // Serial.println("1");

  // Quit if not enough time has lapsed OR if the conditions
  // are not satisfied
  if (!force) {
    if (lastExecutionTime && millis() - lastExecutionTime < FEED_MIN_INTERVAL) return;
    if (!actionShouldStartOrStop(action, 1)) return;
  }

  // Serial.println("2");
  
  // We are in!
  // Set a new execution time
  lastExecutionTime = millis();

  lcdClear();
  lcdPrint(MSG_FEEDING, 2);
 
  // Serial.println("3");

  // Let water in
  openLineIn();

  unsigned long feedStartMillis = millis();
  uint8_t soilMoistureBefore = soilMoistureAsPercentage(senseSoilMoisture());
  uint8_t trayWaterLevelBefore = trayWaterLevelAsPercentage(senseTrayWaterLevel());

  while (true) {
    // Serial.println("4");
  
    uint8_t shouldStop = 0;
    if (actionShouldStartOrStop(action, 0)) shouldStop = 1;
    if (millis() - feedStartMillis >= MAX_FEED_TIME) shouldStop = 2;
    // Serial.println("5");
    
    // Keep emptying the tray if necessary
    emptyTrayIfNecessary();
    // Serial.println("6");

    if (shouldStop) {
      // Serial.println("7");
      // Serial.println(shouldStop);

      closeLineIn();

      clearLogEntry((void *) &newLogEntry);
      newLogEntry.millisStart = feedStartMillis;
      newLogEntry.millisEnd = 12; // millis();
      newLogEntry.actionId = 1; 
      newLogEntry.soilMoistureBefore = soilMoistureBefore;
      newLogEntry.trayWaterLevelBefore = trayWaterLevelBefore;
      newLogEntry.soilMoistureAfter = soilMoistureAsPercentage(senseSoilMoisture());
      newLogEntry.trayWaterLevelAfter = trayWaterLevelAsPercentage(senseTrayWaterLevel());
      newLogEntry.topFeed = action->feedFrom == FeedFrom::FEED_FROM_TOP;
      newLogEntry.outcome = shouldStop == 1 ? 0 : shouldStop;

      writeLogEntry((void *) &newLogEntry);

      return;
    }
    delay(100);
  }
}

PumpState pumpState = IDLE;
unsigned long lastPumpOutExecutionTime = 0;
unsigned long pumpOutStartMillis = 0;

void emptyTrayIfNecessary() {

  // No need
  if (config.trayNeedsEmptying) return;

  const uint8_t trayState = trayWaterLevelAsState(trayWaterLevelAsPercentage(senseTrayWaterLevel()), senseTrayIsFull());
  switch (pumpState) {
    case IDLE:
      if (millis() - lastPumpOutExecutionTime >= PUMP_REST_TIME && trayState >= Conditions::TRAY_SOME ) {
        lastPumpOutExecutionTime = millis();
        openPumpOut();
        pumpOutStartMillis = millis();
        pumpState = PUMPING;
      }
      break;

    case PUMPING:
      if (millis() - pumpOutStartMillis >= MAX_PUMP_TIME || trayState <= Conditions::TRAY_EMPTY) {
        closePumpOut();
        pumpState = COMPLETED;
      }
      break;

    case COMPLETED:
      // Any post-pumping actions can be handled here
      pumpState = IDLE;
      break;
  }
}

void setup() {
  Serial.begin(9600);  // Initialize serial communication at 9600 baud rate

  initLcdAndButtons();

  extern LiquidCrystal_I2C lcd;

  initSensors();

  // Init logs memory
  initLogs(&currentLogEntry, 1024, 256, 768, sizeof(currentLogEntry));

  loadConfig();

  if (!configChecksumCorrect()) {
    // Serial.println("CHECKSUM INCORRECT?!?");
    restoreDefaultConfig();
    saveConfig();
    wipeLogs();
  }

  // runInitialSetup();

  if (config.mustRunInitialSetup) {
    while (!runInitialSetup());
  }
}
#include <LiquidCrystal_I2C.h>

extern LiquidCrystal_I2C lcd;

uint8_t screenCounter = 0;

void loop() {

  unsigned long currentMillis = millis();


  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    screenCounter = (screenCounter + 1) % 8;
    maybeRunActions();
    emptyTrayIfNecessary();
    displayInfo(screenCounter);
  }

  // Exit the main "action" loop, and go to the system's menu,
  // if a button is pressed
  analogButtonsCheck();
  if (pressedButton != nullptr) {
    pressedButton = nullptr;
    mainMenu();
  }
}

void viewLogs() {
  lcdClear();

  if (noLogs()) {
    lcdFlashMessage(MSG_NO_LOG_ENTRIES);
    return;
  }
    
  bool dataChanged = true;
  while(1) {
    if (dataChanged) {
      lcdClearLine(1);
      lcd.setCursor(0, 1);
      lcd.print(currentLogEntry.millisStart);
      lcdPrint(MSG_SPACE);
      lcd.print(currentLogEntry.millisEnd);

      lcdClearLine(2);
      lcd.setCursor(0, 2);
      lcd.print(currentLogEntry.seq);
      
      dataChanged = false;
    }
 
    analogButtonsCheck();
    if (pressedButton == &leftButton) {
      break;
    } else if (pressedButton == &upButton) {
      getPreviousSlot();
      dataChanged = true;
    } else if (pressedButton == &downButton) {
      goToNextSlot();
      dataChanged = true;
    }
  }
}
void activatePumps() {
  int8_t choice;
  while (true) {
    setChoices(
      MSG_PUMP_IN, 0,
      MSG_PUMP_OUT, 1,
      MSG_SOLENOID_IN, 2);
    choice = selectChoice(3, 0);
    if (choice == -1) return;

    // TODO: FINISH HERE
  }
}

void mainMenu() {
  int8_t choice;
  do {
    setChoices(
      MSG_ACTIVE_ACTIONS, 1,
      MSG_LOGS, 2,
      MSG_FORCE_ACTION, 3,
      MSG_SETTINGS, 4);

    choice = selectChoice(4, 1);

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
    lcdFlashMessage(MSG_NO_ACTION_SET);
    return;
  }

  if (config.activeActionsIndex0 != -1) {
    setChoiceFromString(i, config.actions[config.activeActionsIndex0].name, i);
    i++;
  }

  if (config.activeActionsIndex1 != -1) {
    setChoiceFromString(i, config.actions[config.activeActionsIndex1].name, i);
    i++;
  }

  setChoicesHeader(MSG_PICK_ACTION);
  index = selectChoice(i, 0);
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

  soilMoisture = senseSoilMoisture();
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

  lcdPrint(trayWaterLevelInEnglish(trayWaterLevelAsState(trayWaterLevelPercent, trayIsFull), trayIsFull));
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
    lcd.print(config.actions[active].name);
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
  for (i = 0; i <= ACTIONS_ARRAY_SIZE; i++) {
    if (config.feedFrom == config.actions[i].feedFrom) {
      setChoiceFromString(c, config.actions[i].name, i);
      c++;
    }
  }

  int8_t choice = selectChoice(c, 0);

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
    if (config.activeActionsIndex0 == -1) {
      setChoice(0, MSG_EMPTY_SLOT, 0);
    } else {
      setChoiceFromString(0, config.actions[config.activeActionsIndex0].name, 0);
    }

    if (config.activeActionsIndex1 == -1) {
      setChoice(1,MSG_EMPTY_SLOT, 1);
    } else {
      setChoiceFromString(1, config.actions[config.activeActionsIndex1].name, 1);
    }

    setChoicesHeader(MSG_PICK_A_SLOT);
    slot = selectChoice(2, 0);
    if (slot == -1) return;

    setChoice(0, MSG_EMPTY_SLOT, 127);
    uint8_t i = 1, c = 1;
    for (i = 0; i < ACTIONS_ARRAY_SIZE; i++) {
      if (config.feedFrom == config.actions[i].feedFrom) {
        setChoiceFromString(c, config.actions[i].name, i);
        c++;
      }
    }

    choice = selectChoice(c, 127);
    if (choice == -1) continue;

    if (choice == 127) choice = -1;
    if (slot == 0) config.activeActionsIndex0 = choice;
    if (slot == 1) config.activeActionsIndex1 = choice;
    
    saveConfig();
  }
}


void resetData() {
  int8_t reset = confirm(MSG_SURE_QUESTION);

  if (reset == -1) return;

  if (reset) {
    restoreDefaultConfig();
    saveConfig();
    wipeLogs();
    while (!runInitialSetup());
  }
}

void setAutoDrain() {
  if (config.feedFrom != FeedFrom::FEED_FROM_TOP) {
    lcdFlashMessage(MSG_ONLY_TOP_FEEDING, MSG_EMPTY, 2000);
  }
  int8_t trayNeedsEmptying = confirm(MSG_AUTO_DRAIN, config.trayNeedsEmptying);
  if (trayNeedsEmptying == -1) return;

  int8_t confirmation = confirm(MSG_SAVE_QUESTION);
  if (confirmation == -1) return;

  if (confirmation) {
    // Only overwritten once saving
    config.trayNeedsEmptying = trayNeedsEmptying;
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
      MSG_TEST_PUMPS, 3,
      MSG_RUN_ANY_ACTIONS, 4
      
      );
    choice = selectChoice(4, 1);

    if (choice == 1) resetData();
    else if (choice == 2) resetOnlyLogs();
    else if (choice == 3) activatePumps();
    else if (choice == 4) pickAndRunAction();
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
        MSG_TRAY, FeedFrom::FEED_FROM_TRAY
      );
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

  soilVeryMoistPercentage = inputNumber(MSG_OVER, config.soilVeryMoistPercentage, 5, 0, 95, MSG_PERCENT,  MSG_SOIL_VERY_MOIST);
  if (soilVeryMoistPercentage == -1) return;

  soilMoistPercentage = inputNumber(MSG_OVER, config.soilMoistPercentage, 5, 0, config.soilMoistPercentage - 5, MSG_PERCENT,  MSG_SOIL_MOIST);
  if (soilVeryMoistPercentage == -1) return;

  soilLittleMoistPercentage = inputNumber(MSG_OVER, config.soilLittleMoistPercentage, 5, 0, config.soilMoistPercentage - 5, MSG_PERCENT,  MSG_SOIL_LITTLE_MOIST);
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