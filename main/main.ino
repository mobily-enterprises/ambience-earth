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
// const unsigned long actionInterval = 2000;  // Interval in milliseconds (1000 milliseconds = 1 second)
unsigned long actionPreviousMillis = 0;

double averageMsBetweenFeeds = 0.0;
unsigned long int millisAtEndOfLastFeed = 0;
unsigned long lastButtonPressTime = 0;
bool screenSaverMode = false;

void setup() {
  Serial.begin(9600);  // Initialize serial communication at 9600 baud rate

  initLcdAndButtons();

  extern LiquidCrystal_I2C lcd;

  initSensors();


  // 0: TRAY WATER LEVEL
  pinMode(A1, OUTPUT);
  digitalWrite(A1, LOW);
  // 2: MOISTURE SENSOR
  // pinMode(A3, OUTPUT); digitalWrite(A3, LOW);
  // 4: I2C
  // 5: I2C
  pinMode(A6, OUTPUT);
  digitalWrite(A6, LOW);
  pinMode(A7, OUTPUT);
  digitalWrite(A7, LOW);


  // TEMP();

  // Init logs memory
  initLogs(&currentLogEntry, 1024, 256, 768, sizeof(currentLogEntry));

  loadConfig();

  if (!configChecksumCorrect()) {
    // Serial.println("CHECKSUM INCORRECT?!?");
    restoreDefaultConfig();
    saveConfig();
    wipeLogs();
  }

  if (config.mustRunInitialSetup) {
    while (!runInitialSetup())
      ;
  }

  initialaverageMsBetweenFeeds();
  createBootLogEntry();

  // Serial.println("GOING TO LATEST SLOT FOR DEBUGGING REASONS:");
  goToLatestSlot();
}

/*
void TEMP() {
  lcd.init();
  // delay(1000);
  lcd.backlight(); 

  // Set up the LCD's number of columns and rows
  lcd.setCursor(0, 0);

  // Print a message to the LCD
  lcd.print("A0 M: ");
  lcd.setCursor(0, 2);
  lcd.print("A2 W: ");

  pinMode(4, OUTPUT);
  digitalWrite(4,  HIGH); // Ensure the sensor is on

  pinMode(A2, INPUT);
  pinMode(A0, INPUT);
  while(true) {
    int sensorA0 = analogRead(A0);
    delay(100);
    analogRead(A2);
    int sensorA2 = analogRead(A2);

    // pinMode(A0, OUTPUT);
    // digitalWrite(A0, LOW);
    // pinMode(A2, OUTPUT);
    // // digitalWrite(A2, LOW);


    // Display the readings on the LCD
    lcd.setCursor(6, 0);
    lcd.print(sensorA0);
    lcd.print("   ");  // Clear any trailing characters
    lcd.setCursor(6, 2);
    lcd.print(sensorA2);
    lcd.print("   ");  // Clear any trailing characters

    // Wait for 50 milliseconds
    delay(200);
  }  

}

*/


void createBootLogEntry() {
  senseSoilMoisture(1);
  senseTrayWaterLevel(1);
  delay(500);

  uint8_t soilMoistureBefore = soilMoistureAsPercentage(senseSoilMoisture(2));
  uint8_t trayWaterLevelBefore = trayWaterLevelAsPercentage(senseTrayWaterLevel(2));

  clearLogEntry((void *)&newLogEntry);
  newLogEntry.millisStart = 0;
  newLogEntry.millisEnd = millis();
  newLogEntry.entryType = 0;  // BOOTED UP
  newLogEntry.actionId = 7;   // NOT RELEVANT
  newLogEntry.soilMoistureBefore = soilMoistureBefore;
  newLogEntry.trayWaterLevelBefore = trayWaterLevelBefore;
  newLogEntry.soilMoistureAfter = 0;
  newLogEntry.trayWaterLevelAfter = 0;
  newLogEntry.topFeed = 0;
  newLogEntry.outcome = 0;

  // Serial.println("Writing initial boot entry...");
  writeLogEntry((void *)&newLogEntry);

  senseTrayWaterLevel(1);
}


void initialaverageMsBetweenFeeds() {
  int8_t currentLogSlotBefore = getCurrentLogSlot();
  int8_t count = 0;
  int8_t totalLogSlots = getTotalLogSlots();

  unsigned long int found = 0;

  while (true) {

    // currentLogEntry
    // Serial.print("Seq: ");// Serial.print(currentLogEntry.seq);
    // Serial.print(" Type: ");// Serial.print(currentLogEntry.entryType);
    // Serial.print(" Outcome: ");// Serial.print(currentLogEntry.outcome);
    // Serial.print(" Started at: ");// Serial.print(currentLogEntry.millisStart);
    // Serial.print(" Ended at: ");// Serial.println(currentLogEntry.millisEnd);

    if (currentLogEntry.entryType == 1) {
      if (!found) found = currentLogEntry.millisEnd;
      else {
        updateaverageMsBetweenFeeds(abs(found - currentLogEntry.millisStart));
        found = currentLogEntry.millisEnd;
      }
    } else {
      found = 0;
    }

    if (count++ >= totalLogSlots) break;
    goToPreviousLogSlot(true);
    if (!currentLogEntry.seq) break;
  }
  goToLogSlot(currentLogSlotBefore);
}

void updateaverageMsBetweenFeeds(unsigned long durationMillis) {
  static unsigned long callCount = 0;
  // Serial.print("Called updateaverageMsBetweenFeeds() with param: ");
  // Serial.println(durationMillis);

  // Serial.print("Count: ");// Serial.println(callCount);
  // Serial.print("Previous average: ");// Serial.println(averageMsBetweenFeeds);

  callCount++;
  averageMsBetweenFeeds += (durationMillis - averageMsBetweenFeeds) / callCount;

  // Serial.print("New average: ");// Serial.println(averageMsBetweenFeeds);
}

void loop() {

  unsigned long currentMillis = millis();


  /*
  pinMode(A7, INPUT_PULLUP);
  while(true) {
    // Read the value from analog pin A7
    int sensorValue = analogRead(A7);
    // Print the value to the Serial Monitor
    Serial.println("STOCAZZO");
    Serial.println(sensorValue);
    // Wait for 500 milliseconds (0.5 seconds)
    delay(1500);
  }
  */


  if (millis() - lastButtonPressTime > SCREENSAVER_TRIGGER_TIME) screenSaverModeOn();

  if (currentMillis - actionPreviousMillis >= actionInterval) {
    actionPreviousMillis = currentMillis;
    screenCounter = (screenCounter + 1) % 8;
    maybeRunActions();
    emptyTrayIfNecessary();
    displayInfo(screenCounter);
  }

// delay(3000);


/*
while(1) {
    openLineIn();
    delay(20);
    closeLineIn();
    delay(20);
  }
*/


  // To change the state (esp. turning it off after reading) if necessary
  // senseTrayWaterLevel(1);

  
  // Exit the main "action" loop, and go to the system's menu,
  // if a button is pressed
  analogButtonsCheck();
  if (pressedButton != nullptr) {
    lastButtonPressTime = millis();

    if (pressedButton == &okButton) mainMenu();
    else screenSaverModeOff();
    // pressedButton = nullptr;
  }
}

void screenSaverModeOff() {
  screenSaverMode = false;
  lcd.backlight();
}

void screenSaverModeOn() {
  screenSaverMode = true;
  lcd.noBacklight();
}

void mainMenu() {
  int8_t choice;

  lcd.backlight();
  do {
    setChoices(
      MSG_ACTIVE_ACTIONS, 1,
      MSG_LOGS, 2,
      MSG_FORCE_ACTION, 3,
      MSG_SETTINGS, 4);

    choice = selectChoice(4, 1);

    if (choice == 1) setActiveActions();
    else if (choice == 2) viewLogs();
    else if (choice == 3) forceAction();
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
      setChoice(0, MSG_LITTLE_SLOT, 0);
    } else {
      setChoiceFromString(0, config.actions[config.activeActionsIndex0].name, 0);
    }

    if (config.activeActionsIndex1 == -1) {
      setChoice(1, MSG_LITTLE_SLOT, 1);
    } else {
      setChoiceFromString(1, config.actions[config.activeActionsIndex1].name, 1);
    }

    setChoicesHeader(MSG_PICK_A_SLOT);
    slot = selectChoice(2, 0);
    if (slot == -1) return;

    setChoice(0, MSG_LITTLE_SLOT, 127);
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


void printSoilAndWaterTrayStatus() {
  uint16_t soilMoisture = senseSoilMoisture();
  uint16_t soilMoisturePercent = soilMoistureAsPercentage(soilMoisture);
  uint16_t trayWaterLevel = senseTrayWaterLevel();
  uint16_t trayWaterLevelPercent = trayWaterLevelAsPercentage(trayWaterLevel);

  bool trayIsFull = senseTrayIsFull();


  lcdPrint(MSG_SOIL_NOW, 0);
  lcdPrint(MSG_SPACE);
  lcdPrintNumber(soilMoisturePercent);
  lcdPrint(MSG_PERCENT);
  lcdPrint(MSG_SPACE);
  lcdPrint(soilMoistureInEnglish(soilMoistureAsState(soilMoisturePercent)));
  lcdPrint(MSG_SPACE);

  lcdPrint(MSG_TRAY_NOW, 1);
  lcdPrint(MSG_SPACE);
  lcdPrintNumber(trayWaterLevelPercent);
  lcdPrint(MSG_PERCENT);
  lcdPrint(MSG_SPACE);
  lcdPrint(trayWaterLevelInEnglish(trayWaterLevelAsState(trayWaterLevelPercent, trayIsFull), trayIsFull));
}



void runAction(Action *action, uint8_t index, bool force = 0) {

  // Serial.println("INDEX:");
  // Serial.println(index);


  // Quit if not enough time has lapsed OR if the conditions
  // are not satisfied
  if (!force) {
    if (millis() < 10000) return;
    if (millisAtEndOfLastFeed && millis() - millisAtEndOfLastFeed < config.minFeedInterval) return;
    if (!actionShouldStartOrStop(action, 1)) return;
  }


  bool trayIsFull = senseTrayIsFull();

  lcdPrint(MSG_SOIL_NOW, 0);
  lcdPrint(MSG_SPACE);



  lcdClear();

  lcdPrint(MSG_FEEDING, 2);


  // Serial.println("3");

  // Let water in
  openLineIn();

  senseTrayWaterLevel(2);
  senseSoilMoisture(2);
  

  unsigned long feedStartMillis = millis();
  // Serial.print("feedStartMillis: "); Serial.println(feedStartMillis);


  uint8_t soilMoistureBefore = soilMoistureAsPercentage(senseSoilMoisture());
  uint8_t trayWaterLevelBefore = trayWaterLevelAsPercentage(senseTrayWaterLevel());

  uint8_t shouldStop = 0;
  uint8_t retCode = 0;

  // Serial.print("millisAtEndOfLastFeed: "); Serial.println(millisAtEndOfLastFeed);

  unsigned long lastUpdateMillis = 0;        // Store the time when we last updated the status
  const unsigned long updateInterval = 500;  // Update interval in milliseconds (500 ms = 0.5 second)

  while (true) {
    // Serial.println("4");

    unsigned long currentMillis = millis();
    if (currentMillis - lastUpdateMillis >= updateInterval) {
      // Update the last update time
      lastUpdateMillis = currentMillis;

      // Update the status
      printSoilAndWaterTrayStatus();
    }

    if (actionShouldStartOrStop(action, 0)) {
      shouldStop = true;
      retCode = 0;  // OK
    }
    if (millis() - feedStartMillis >= config.maxFeedTime) {
      shouldStop = true;
      retCode = 1;  // INTERRUPTED
    }

    // Serial.println("5");

    // Keep emptying the tray if necessary
    emptyTrayIfNecessary();
    // Serial.println("6");

    if (shouldStop) {
      // Serial.println("7");
      // Serial.println(shouldStop);

      if (millisAtEndOfLastFeed) updateaverageMsBetweenFeeds(millis() - millisAtEndOfLastFeed);
      millisAtEndOfLastFeed = millis();

      closeLineIn();
      senseTrayWaterLevel(1);
      senseSoilMoisture(1);

      // Serial.print("MS: "); Serial.println(millis());

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
      if (millis() - lastPumpOutExecutionTime >= config.pumpOutRestTime && trayState >= Conditions::TRAY_SOME) {
        lastPumpOutExecutionTime = millis();
        openPumpOut();
        pumpOutStartMillis = millis();
        pumpState = PUMPING;
      }
      break;

    case PUMPING:
      if (millis() - pumpOutStartMillis >= config.maxPumpOutTime || trayState <= Conditions::TRAY_LITTLE) {
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


void lcdPrintTime(unsigned long milliseconds) {

  // Calculate the total number of seconds, minutes, and hours
  unsigned long totalSeconds = milliseconds / 1000;
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

/*
void displayInfo(uint8_t screen) {
  switch (screen) {
    case 0:
     displayInfo4(screen);
      break;
    case 1:
    displayInfo4(screen);
      break;
    case 2:
     displayInfo4(screen);
      break;
    case 3:
     displayInfo4(screen);
      break;
    case 4:  
      displayInfo4(screen);
      break;
    case 5:
      //displayInfo3(screen);
      displayInfo4(screen);
      break;
    case 6:
         displayInfo4(screen);
    case 7:
      displayInfo1(screen);
      // displayInfo2(screen);
    break;
  }
}

*/

void displayInfo(uint8_t screen) {
  if (screenSaverMode) {
    uint8_t x = random(0, 20); // Random column (0-15)
    uint8_t y = random(0, 4);  // Random row (0-3)
  
    // Set the cursor to the random position and print the star character
    lcd.clear();
    lcd.setCursor(x, y);
    // lcd.setCursor(1, 1);
    
    lcd.write('*');

    return;
  }

  switch (screen) {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
      displayInfo1(screen);
      break;
    case 5:
      displayInfo2(screen);
      // displayInfo1(screen);
      break;
    case 6:
      displayInfo3(screen);
      break;
    case 7:
      // displayInfo1(screen);
      displayInfo1(screen);
      break;
  }
}


void displayInfo1(uint8_t screen) {
  uint16_t soilMoisture;
  uint8_t soilMoisturePercent;
  uint8_t trayWaterLevelPercent;

  uint16_t trayWaterLevel;
  bool trayIsFull;

  printSoilAndWaterTrayStatus();

  lcdPrint(MSG_LAST_FEED, 2);
  if (!millisAtEndOfLastFeed) lcdPrint(MSG_NOT_YET);
  else lcdPrintTimeSince(millisAtEndOfLastFeed);
  // lcdPrintNumber(actionPreviousMillis);

  lcdPrint(MSG_AVG_COLUMN, 3);
  if (!averageMsBetweenFeeds) lcdPrint(MSG_NA);
  // else lcdPrintNumber(averageMsBetweenFeeds);
  else lcdPrintTime(averageMsBetweenFeeds);
}


void displayInfo4(uint8_t screen) {
  // uint16_t soilMoisture = senseSoilMoisture();
  // uint16_t trayWaterLevel = senseTrayWaterLevel(2);

  lcdClear();

  digitalWrite(TRAY_WATER_LEVEL_SENSOR_POWER, HIGH);

  printSoilAndWaterTrayStatus();
  pinMode(A2, INPUT);
  lcdPrint(MSG_SOIL_NOW, 2);
  lcdPrint(MSG_SPACE);
  // lcdPrintNumber(soilMoisture);
  lcdPrint(MSG_SPACE);
  lcd.print(analogRead(SOIL_MOISTURE_SENSOR));

  lcdPrint(MSG_TRAY_NOW, 3);
  lcdPrint(MSG_SPACE);
  // lcdPrintNumber(trayWaterLevel);
  lcdPrint(MSG_SPACE);
  lcd.print(analogRead(TRAY_WATER_LEVEL_SENSOR));
}


void displayInfo3(uint8_t screen) {
  lcdClear();
  lcdPrint(MSG_NEXT_FEED, 1);


  uint32_t millisSinceLastFeed = millis() - millisAtEndOfLastFeed;
  if (millisAtEndOfLastFeed && millisSinceLastFeed < config.minFeedInterval) {
    lcdPrintTime(config.minFeedInterval - millisSinceLastFeed);
    lcdPrint(MSG_AT_THE_EARLIEST, 2);
  } else {
    lcdPrint(MSG_WHEN_CONDITIONS_ALLOW, 2);
  }
}

char *trayConditionToEnglish(uint8_t condition) {
  if (condition == Conditions::TRAY_DRY) return MSG_TRAY_DRY;
  else if (condition == Conditions::TRAY_LITTLE) return MSG_TRAY_LITTLE;
  else if (condition == Conditions::TRAY_SOME) return MSG_TRAY_SOME;
  else if (condition == Conditions::TRAY_PLENTY) return MSG_TRAY_PLENTY;
  else if (condition == Conditions::TRAY_FULL) return MSG_TRAY_FULL;

  else return MSG_LITTLE;
}


char *soilConditionToEnglish(uint8_t condition) {
  if (condition == Conditions::SOIL_DRY) return MSG_SOIL_DRY;
  else if (condition == Conditions::SOIL_LITTLE_MOIST) return MSG_SOIL_LITTLE_MOIST;
  else if (condition == Conditions::SOIL_MOIST) return MSG_SOIL_MOIST;
  else if (condition == Conditions::SOIL_VERY_MOIST) return MSG_SOIL_VERY_MOIST;
  else return MSG_LITTLE;
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
