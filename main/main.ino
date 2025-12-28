#include <AnalogButtons.h>
#include <Arduino.h>
#include "main.h"
#include "ui.h"
#include "config.h"
#include "messages.h"
#include "traySensors.h"
#include "pumps.h"
#include "moistureSensor.h"
#include "settings.h"
#include "logs.h"
#include <LiquidCrystal_I2C.h>

/* YOU ARE HERE
  Adjust logging of tray and moisture, BOTH in the hourly one AND in the feed log
  Check why soil jumps to 100% after feeding and when turining it on
  Check if logs go properly 100%, cycle around,
*/


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
  Serial.begin(115200);  // Initialize serial communication at 9600 baud rate
  initLcd();

  extern LiquidCrystal_I2C lcd;

  initialPinSetup();
 
  initTraySensors();
  initPumps();
  initMoistureSensor();

  // Init logs memory
  initLogs(&currentLogEntry, 1024, 256, 768, sizeof(currentLogEntry));

  loadConfig();

  if (!configChecksumCorrect()) {
    restoreDefaultConfig();
    saveConfig();
    wipeLogs();
  }

  if (config.mustRunInitialSetup) {
    config.mustRunInitialSetup = false;
    saveConfig();
  }

  // runButtonsSetup();

  initializeButtons();

  initialaverageMsBetweenFeeds();

  createBootLogEntry();
  goToLatestSlot();

  setSoilSensorLazy();
}


void loop() {
  unsigned long currentMillis = millis();
  if (millis() - lastButtonPressTime > SCREENSAVER_TRIGGER_TIME) screenSaverModeOn();

  analogButtonsCheck();
  
  if (pressedButton != nullptr) {
    if (pressedButton == &upButton) screenCounter = (screenCounter - 1 + 3) % 3;
    if (pressedButton == &downButton) screenCounter = (screenCounter + 1) % 3;
    displayInfo(screenCounter);
  }

  if (pressedButton != nullptr) {
    lastButtonPressTime = millis();

    if (pressedButton == &okButton) mainMenu();

    else screenSaverModeOff();
    // pressedButton = nullptr;
  }

  // Necessary to run this consistently so that
  // lazy reads work
  runSoilSensorLazyReadings();

  maybeLogValues();

  if (currentMillis - actionPreviousMillis >= actionInterval) {
    actionPreviousMillis = currentMillis;
    displayInfo(screenCounter);
    // int pinValue = digitalRead(12);
    // Serial.print(digitalRead(12));
  }
  delay(50); // Let the CPU breathe
}

void maybeLogValues() {
    static unsigned long lastRunTime = 0; // Stores the last time the function was executed
    unsigned long currentMillis = millis(); // Get the current time in milliseconds

    // Check if LOG_VALUES_INTERVAL has passed since the last run
    if (currentMillis - lastRunTime >= LOG_VALUES_INTERVAL) {  
      lastRunTime = currentMillis; // Update the last run time

      uint8_t soilMoisture = soilMoistureAsPercentage(getSoilMoisture());
      uint8_t trayState = trayWaterLevelAsState();

      // Serial.println("VALUES:");
      // Serial.println(soilMoisture);
      // Serial.println(trayState);
      // Serial.println("");

      clearLogEntry((void *)&newLogEntry);
      newLogEntry.entryType = 2;  // VALUES
      newLogEntry.millisStart = millis();
      newLogEntry.actionId = 0;
      newLogEntry.soilMoistureBefore = soilMoisture;
      newLogEntry.trayWaterLevelBefore = trayState;
      newLogEntry.topFeed = 0;
      newLogEntry.outcome = 0;
      writeLogEntry((void *)&newLogEntry);
  }
}

void initialPinSetup() {
  // Initial set for all pins
  // This is a blanket-set, it will be overridden by
  // sensor functions as needed.
  //pinMode(0, OUTPUT); digitalWrite(0, LOW); // D0 (RX) left alone, serial comms
  //pinMode(1, OUTPUT); digitalWrite(1, LOW); // D1 (TX) left alone, serial comms
  
  pinMode(2, OUTPUT); digitalWrite(2, LOW); 
  pinMode(3, OUTPUT); digitalWrite(3, LOW); 
  pinMode(4, OUTPUT); digitalWrite(4, LOW);
  pinMode(5, OUTPUT); digitalWrite(5, LOW);
  pinMode(6, OUTPUT); digitalWrite(6, LOW);
  pinMode(7, OUTPUT); digitalWrite(7, LOW);
  pinMode(8, OUTPUT); digitalWrite(8, LOW);
  pinMode(9, OUTPUT); digitalWrite(9, LOW);
  pinMode(10, OUTPUT); digitalWrite(10, LOW);
  pinMode(11, OUTPUT); digitalWrite(11, LOW);
  pinMode(12, OUTPUT); digitalWrite(12, LOW);
  pinMode(13, OUTPUT); digitalWrite(13, LOW);

  // Set analog pins (A0 to A5) as OUTPUT and drive LOW
  pinMode(A0, OUTPUT); digitalWrite(A0, LOW);
  pinMode(A1, OUTPUT); digitalWrite(A1, LOW);
  pinMode(A2, OUTPUT); digitalWrite(A2, LOW);
  pinMode(A3, OUTPUT); digitalWrite(A3, LOW);
  pinMode(A4, OUTPUT); digitalWrite(A4, LOW);
  pinMode(A5, OUTPUT); digitalWrite(A5, LOW);

  pinMode(A6, INPUT_PULLUP); // Input only, set as pullup
  pinMode(A7, INPUT_PULLUP); // Analog buttons use external ladder/pull-up
}

void createBootLogEntry() {
  uint8_t soilMoistureBefore = soilMoistureAsPercentage(getSoilMoisture());

  clearLogEntry((void *)&newLogEntry);
  newLogEntry.millisStart = 0;
  newLogEntry.millisEnd = millis();
  newLogEntry.entryType = 0;  // BOOTED UP
  newLogEntry.actionId = 7;   // NOT RELEVANT
  newLogEntry.soilMoistureBefore = soilMoistureBefore;
  newLogEntry.soilMoistureAfter = 0;
  newLogEntry.trayWaterLevelAfter = 0;
  newLogEntry.topFeed = 0;
  newLogEntry.outcome = 0;

  // Serial.println("Writing initial boot entry...");
  writeLogEntry((void *)&newLogEntry);
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
      MSG_LOGS, 1,
      MSG_SETTINGS, 2);

    choice = selectChoice(2, 1);

    if (choice == 1) viewLogs();
    else if (choice == 2) settings();
  } while (choice != -1);
}


void printSoilAndWaterTrayStatus() {
  uint16_t soilMoisture = getSoilMoisture();
  uint16_t soilMoisturePercent = soilMoistureAsPercentage(soilMoisture);
 
  lcdPrint(MSG_SOIL_NOW, 0);
  lcdPrint(MSG_SPACE);
  lcdPrintNumber(soilMoisturePercent);
  lcdPrint(MSG_PERCENT);
 
  lcdPrint(MSG_TRAY_NOW, 1);
  lcdPrint(MSG_SPACE);
  lcdPrint(trayWaterLevelInEnglish(trayWaterLevelAsState()));
}

/*  
    **********************************************************************
    * LOGS VIEWING CODE
    * This allows users to go through the logs
    ***********************************************************************
*/


void lcdPrintTime(unsigned long milliseconds) {

  // Calculate the total number of minutes and hours
  unsigned long totalMinutes = milliseconds / 1000 / 60;
  unsigned long minutes = totalMinutes % 60;
  unsigned long hours = totalMinutes / 60;

  if (hours) {
    lcd.print(hours);
    lcd.print('h');
  }

  // Always show minutes (even 0m) so the time isn't blank
  lcd.print(minutes);
  lcd.print('m');
}


void lcdPrintTimeSince(unsigned long milliseconds) {
  // Calculate elapsed time in milliseconds
  unsigned long elapsedMillis = millis() - milliseconds;

  // Calculate the total number of minutes and hours
  unsigned long totalMinutes = elapsedMillis / 1000 / 60;
  unsigned long minutes = totalMinutes % 60;
  unsigned long hours = totalMinutes / 60;

  if (hours) {
    lcd.print(hours);
    lcd.print('h');
  }

  lcd.print(minutes);
  lcd.print('m');
}


void lcdPrintTimePretty(unsigned long milliseconds) {
  // Calculate elapsed time in milliseconds
  unsigned long elapsedMillis = milliseconds;

  // Calculate the total number of minutes and hours
  unsigned long totalMinutes = elapsedMillis / 1000 / 60;
  unsigned long minutes = totalMinutes % 60;
  unsigned long hours = totalMinutes / 60;

  if (hours) {
    lcd.print(hours);
    lcd.print('h');
  }

  lcd.print(minutes);
  lcd.print('m');
}


void lcdPrintTimeDuration(unsigned long start, unsigned long finish) {
  // Calculate elapsed time in milliseconds
  unsigned long elapsedMillis = finish - start;

  // Calculate the total number of minutes and hours
  unsigned long totalMinutes = elapsedMillis / 1000 / 60;
  unsigned long minutes = totalMinutes % 60;
  unsigned long hours = totalMinutes / 60;

  if (hours) {
    lcd.print(hours);
    lcd.print('h');
  }

  lcd.print(minutes);
  lcd.print('m');
}

void showLogType0() {
  lcdClear();
  // Print absolute log number (epoch-aware)
  lcd.print((unsigned long)getAbsoluteLogNumber());
  lcdPrint(MSG_SPACE);
  lcdPrint(MSG_LOG_TYPE_0);
  // lcdPrint(MSG_SPACE);
  // lcdPrintNumber(currentLogEntry.millisStart);
  // lcdPrint(MSG_SPACE);
  // lcdPrint(MSG_AGO);
}

void showLogType1() {
  lcdClear();
  lcdClear();
  // Print absolute log number (epoch-aware)
  lcd.print((unsigned long)getAbsoluteLogNumber());
  lcdPrint(MSG_SPACE);
  lcdPrint(MSG_LOG_TYPE_1);
  lcdPrint(MSG_SPACE);
  lcdPrintTimePretty(currentLogEntry.millisStart);
  lcdPrint(MSG_SPACE);
  lcdPrint(MSG_LATER);
  lcdPrint(MSG_SPACE);

  lcd.setCursor(0, 1);
  lcd.print("Dur:");
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
  lcdPrint(trayWaterLevelInEnglishShort(currentLogEntry.trayWaterLevelBefore));
  // lcdPrintNumber(currentLogEntry.trayWaterLevelBefore);
  lcdPrint(MSG_DASH);
  lcdPrint(trayWaterLevelInEnglishShort(currentLogEntry.trayWaterLevelAfter));
  // lcdPrintNumber(currentLogEntry.trayWaterLevelAfter);

  lcd.setCursor(0, 3);
  if (currentLogEntry.outcome == 0) lcdPrint(MSG_LOG_OUTCOME_0);
  if (currentLogEntry.outcome == 1) lcdPrint(MSG_LOG_OUTCOME_1);

  // lcdPrintLogParamsSoil(currentLogEntry.soilMoistureBefore);
  // lcdPrintLogParamsSoil(currentLogEntry.soilMoistureAfter);
}

void showLogType2() {
  lcdClear();
  // Print absolute log number (epoch-aware)
  lcd.print((unsigned long)getAbsoluteLogNumber());
  lcdPrint(MSG_SPACE);
  lcdPrint(MSG_LOG_TYPE_2);
  lcdPrint(MSG_SPACE);
  lcdPrintTimePretty(currentLogEntry.millisStart);
  lcdPrint(MSG_SPACE);
  lcdPrint(MSG_LATER);
  lcdPrint(MSG_SPACE);

  lcd.setCursor(0, 2);
  lcdPrint(MSG_SOIL_MOISTURE_COLUMN);
  // lcdPrintNumber(soilMoistureAsPercentage(currentLogEntry.soilMoistureBefore));
  lcdPrintNumber(currentLogEntry.soilMoistureBefore);
  lcdPrint(MSG_PERCENT);

  lcd.setCursor(0, 3);
  lcdPrint(MSG_TRAY);
  lcdPrint(MSG_COLUMN_SPACE);
  lcdPrint(trayWaterLevelInEnglish(currentLogEntry.trayWaterLevelBefore));
  
  // lcdPrintLogParamsSoil(currentLogEntry.soilMoistureBefore);
  // lcdPrintLogParamsSoil(currentLogEntry.soilMoistureAfter);
}


void viewLogs() {
  lcdClear();

  // Always start from the latest available log entry
  goToLatestSlot();

  if (noLogs()) {
    lcdFlashMessage(MSG_NO_LOG_ENTRIES);
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

  lcd.clear();
  switch (screen) {
    case 0: displayInfo1(); break;
    case 1: displayInfo3(); break;
    case 2: displayInfo4(); break;
  }
}


void displayInfo1() {
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


void displayInfo4() {
 
  lcdClear();

  printSoilAndWaterTrayStatus();
  pinMode(A2, INPUT);
  lcdPrint(MSG_SOIL_NOW, 2);
  lcdPrint(MSG_SPACE);
  // lcdPrintNumber(soilMoisture);
  lcdPrint(MSG_SPACE);
  lcd.print(getSoilMoisture());
}


void displayInfo3() {
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
