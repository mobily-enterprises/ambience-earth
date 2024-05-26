/*
  * Average feeding time
    * Add variables averageTimeBetweenFeed and timeSinceLastFeed
    * Write procedure that works out those variables reading the logs
    * Print this information in the home screen
  
  LATER:
  * Make MIN_FEED_INTERVAL a variable set at setup time and in settings 
  * Same for the max feed duration and max pump IN/OUT is on. Put them in maintenance

*/
#include <AnalogButtons.h>
#include <Arduino.h>
#include "main.h"
#include "ui.h"
#include "sensors.h"
#include "config.h"
#include "messages.h"
#include "sensors.h"
#include "settings.h"
#include "logs.h"
#include <LiquidCrystal_I2C.h>

extern LiquidCrystal_I2C lcd;
uint8_t screenCounter = 0;

extern Config config;

LogEntry currentLogEntry;
LogEntry newLogEntry;

extern Button *pressedButton;
extern Button upButton;
extern Button leftButton;
extern Button downButton;
extern Button rightButton;
extern Button okButton;

const unsigned long actionInterval = 2000;  // Interval in milliseconds (1000 milliseconds = 1 second)
unsigned long actionPreviousMillis = 0;

int averageTimeBetweenFeed = 0, timeSinceLastFeed = 0;

void setup() {
  Serial.begin(230400);  // Initialize serial communication at 9600 baud rate

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
    while (!runInitialSetup())
      ;
  }

  // Add a boot log entry IF needed
  // (avoid repeating them unnecessarily)
  createBootLogEntry();
}

void loop() {

  unsigned long currentMillis = millis();


  if (currentMillis - actionPreviousMillis >= actionInterval) {
    actionPreviousMillis = currentMillis;
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


/*  
    **********************************************************************
    * ACTIONS CODE
    * Both for menu items, and for maybeRunActions() from the main loop()
    ***********************************************************************
*/

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
      setChoice(1, MSG_EMPTY_SLOT, 1);
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
  // Serial.println("INDEX:");

  // Serial.println(index);
  runAction(&config.actions[index], index, true);
}


void maybeRunActions() {
  if (config.activeActionsIndex0 != -1) runAction(&config.actions[config.activeActionsIndex0], config.activeActionsIndex0);
  if (config.activeActionsIndex1 != -1) runAction(&config.actions[config.activeActionsIndex1], config.activeActionsIndex1);
}

bool actionShouldStartOrStop(Action *action, bool start = true) {
  Conditions *c;
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


void runAction(Action *action, uint8_t index, bool force = 0) {
  static unsigned long lastExecutionTime = 0;

  // Serial.println("INDEX:");
  // Serial.println(index);


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

  uint8_t shouldStop = 0;
  uint8_t retCode = 0;
  while (true) {
    // Serial.println("4");

    
    if (actionShouldStartOrStop(action, 0)) {
      shouldStop = true;
      retCode = 0; // OK
    }
    if (millis() - feedStartMillis >= MAX_FEED_TIME) {
      shouldStop = true;
      retCode = 1; // INTERRUPTED
    }
    // Serial.println("5");

    // Keep emptying the tray if necessary
    emptyTrayIfNecessary();
    // Serial.println("6");

    if (shouldStop) {
      // Serial.println("7");
      // Serial.println(shouldStop);

      closeLineIn();

      delay(3500);
      clearLogEntry((void *)&newLogEntry);
      newLogEntry.entryType = 1;  // FEED
      newLogEntry.millisStart = feedStartMillis;
      newLogEntry.millisEnd = millis();
      newLogEntry.actionId = index;
      newLogEntry.soilMoistureBefore = soilMoistureBefore;
      newLogEntry.trayWaterLevelBefore = trayWaterLevelBefore;
      newLogEntry.soilMoistureAfter = soilMoistureAsPercentage(senseSoilMoisture());
      newLogEntry.trayWaterLevelAfter = trayWaterLevelAsPercentage(senseTrayWaterLevel());
      newLogEntry.topFeed = action->feedFrom == FeedFrom::FEED_FROM_TOP;
      newLogEntry.outcome = retCode;

      writeLogEntry((void *)&newLogEntry);

      return;
    }
    delay(100);
  }
}

/*  
    **********************************************************************
    * TRAY EMPTYING CODE
    * Run both from the main loop() and when a feed (action) is running
    * It's a state machine, so it works in the "background"
    ***********************************************************************
*/

PumpState pumpState = IDLE;
unsigned long lastPumpOutExecutionTime = 0;
unsigned long pumpOutStartMillis = 0;

void emptyTrayIfNecessary() {

  // No need
  if (config.trayNeedsEmptying) return;

  const uint8_t trayState = trayWaterLevelAsState(trayWaterLevelAsPercentage(senseTrayWaterLevel()), senseTrayIsFull());
  switch (pumpState) {
    case IDLE:
      if (millis() - lastPumpOutExecutionTime >= PUMP_REST_TIME && trayState >= Conditions::TRAY_SOME) {
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

/*  
    **********************************************************************
    * LOGS VIEWING CODE
    * This allows users to go through the logs
    ***********************************************************************
*/

void lcdPrintTimeSince(unsigned long milliseconds) {
  // Calculate elapsed time in milliseconds
  unsigned long elapsedMillis = millis() - milliseconds;

  // Calculate the total number of seconds, minutes, and hours
  unsigned long totalSeconds = elapsedMillis / 1000;
  unsigned long seconds = totalSeconds % 60;
  unsigned long totalMinutes = totalSeconds / 60;
  unsigned long minutes = totalMinutes % 60;
  unsigned long hours = totalMinutes / 60;

  if (hours) {
    lcd.print(hours);
    lcd.print('h');
  }

  if (minutes) {
    lcd.print(minutes);
    lcd.print('m');
  }

  lcd.print(seconds);
  lcd.print('s');
}

void lcdPrintTimeDuration(unsigned long start, unsigned long finish) {
  // Calculate elapsed time in milliseconds
  unsigned long elapsedMillis = finish - start;

  // Calculate the total number of seconds, minutes, and hours
  unsigned long totalSeconds = elapsedMillis / 1000;
  unsigned long seconds = totalSeconds % 60;
  unsigned long totalMinutes = totalSeconds / 60;
  unsigned long minutes = totalMinutes % 60;
  unsigned long hours = totalMinutes / 60;

  if (hours) {
    lcd.print(hours);
    lcd.print('h');
  }

  if (minutes) {
    lcd.print(minutes);
    lcd.print('m');
  }

  lcd.print(seconds);
  lcd.print('s');
}


void showLogType0() {
  lcdClear();
  lcdPrint(MSG_LOG_TYPE_0, 0);
  lcdPrint(MSG_SPACE);
  lcdPrintTimeSince(currentLogEntry.millisStart);
  lcdPrint(MSG_SPACE);
  lcdPrint(MSG_AGO);
}

void showLogType1() {
  lcdClear();
  lcdPrint(MSG_LOG_TYPE_1, 0);
  lcdPrint(MSG_SPACE);
  lcdPrintTimeSince(currentLogEntry.millisStart);
  lcdPrint(MSG_SPACE);
  lcdPrint(MSG_AGO);
  lcdPrint(MSG_SPACE);

  lcd.setCursor(0, 1);
  lcd.print(config.actions[currentLogEntry.actionId].name);
  lcd.setCursor(15, 1);
  lcdPrint(MSG_SPACE);
  lcdPrintTimeDuration(currentLogEntry.millisStart, currentLogEntry.millisEnd);

  lcd.setCursor(0, 2);
  lcdPrint(MSG_S_COLUMN);
  lcdPrintNumber(currentLogEntry.soilMoistureBefore);
  lcdPrint(MSG_PERCENT);
  lcdPrint(MSG_DASH);
  lcdPrintNumber(currentLogEntry.soilMoistureAfter);
  lcdPrint(MSG_PERCENT);
  lcdPrint(MSG_SPACE);
  
  lcdPrint(MSG_W_COLUMN);
  lcdPrintNumber(currentLogEntry.trayWaterLevelBefore);
  lcdPrint(MSG_PERCENT);
  lcdPrint(MSG_DASH);
  lcdPrintNumber(currentLogEntry.trayWaterLevelAfter);
  lcdPrint(MSG_PERCENT);
  
  lcd.setCursor(0, 3);
  if (currentLogEntry.outcome == 0) lcdPrint(MSG_LOG_OUTCOME_0);
  if (currentLogEntry.outcome == 1) lcdPrint(MSG_LOG_OUTCOME_1);
  
  // lcdPrintLogParamsSoil(currentLogEntry.soilMoistureBefore);
  // lcdPrintLogParamsSoil(currentLogEntry.soilMoistureAfter);
}

void viewLogs() {
  lcdClear();

  if (noLogs()) {
    lcdFlashMessage(MSG_NO_LOG_ENTRIES);
    return;
  }

  bool dataChanged = true;
  while (1) {
    if (dataChanged) {

      if (currentLogEntry.entryType == 0) showLogType0();
      if (currentLogEntry.entryType == 1) showLogType1();
      dataChanged = false;
    }

    analogButtonsCheck();
    if (pressedButton == &leftButton) {
      break;
    } else if (pressedButton == &downButton && currentLogEntry.entryType != 0) {
      if (goToPreviousLogSlot()) {
        dataChanged = true;
      }
    } else if (pressedButton == &upButton) {
      if (goToNextLogSlot()) {
        dataChanged = true;
      }
    }
  }
}

/*  
    **********************************************************************
    * RUNNING SCREEN CODE
    * displayInfo() is run in the main program's loop()
    ***********************************************************************
*/
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
