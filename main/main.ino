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
#include "feeding.h"
#include "logs.h"
#include <LiquidCrystal_I2C.h>
#include <avr/pgmspace.h>

/* YOU ARE HERE
  Adjust logging of tray and moisture, BOTH in the hourly one AND in the feed log
  Check why soil jumps to 100% after feeding and when turining it on
  Check if logs go properly 100%, cycle around,
*/


extern LiquidCrystal_I2C lcd;
uint8_t screenCounter = 0;
const uint8_t kScreenCount = 4;

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
static bool forceDisplayRedraw = false;

void setup() {
  Serial.begin(115200);  // Initialize serial communication at 9600 baud rate
  randomSeed(analogRead(A6));
  initLcd();

  extern LiquidCrystal_I2C lcd;

  initialPinSetup();
 
  initTraySensors();
  initPumps();
  initMoistureSensor();

  // Init logs memory
  initLogs(&currentLogEntry, 1024, 256, 768, sizeof(currentLogEntry));

  loadConfig();

  if (!validateConfig()) {
    restoreDefaultConfig();
    saveConfig();
    wipeLogs();
  }

#ifdef WOKWI_SIM
  if (!config.kbdUp && !config.kbdDown && !config.kbdLeft && !config.kbdRight && !config.kbdOk) {
    config.kbdUp = 500;
    config.kbdDown = 300;
    config.kbdLeft = 900;
    config.kbdRight = 100;
    config.kbdOk = 700;
    saveConfig();
  }

  if (config.flags & CONFIG_FLAG_MUST_RUN_INITIAL_SETUP) {
    config.flags &= static_cast<uint8_t>(~CONFIG_FLAG_MUST_RUN_INITIAL_SETUP);
    saveConfig();
  }
#else
  if (config.flags & CONFIG_FLAG_MUST_RUN_INITIAL_SETUP) {
    runButtonsSetup();
    while (!runInitialSetup());
  }
#endif

  // runButtonsSetup();

  initializeButtons();

  initialaverageMsBetweenFeeds();

  createBootLogEntry();
  goToLatestSlot();

  setSoilSensorLazy();
}


void loop() {
  unsigned long currentMillis = millis();
  if (!uiTaskActive() && millis() - lastButtonPressTime > SCREENSAVER_TRIGGER_TIME) screenSaverModeOn();

  if (uiTaskActive()) {
    runUiTask();
    runSoilSensorLazyReadings();
    maybeLogValues();
    delay(50); // Let the CPU breathe
    return;
  }

  analogButtonsCheck();
  
  if (pressedButton != nullptr) {
    if (pressedButton == &upButton) screenCounter = (screenCounter - 1 + kScreenCount) % kScreenCount;
    if (pressedButton == &downButton) screenCounter = (screenCounter + 1) % kScreenCount;
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
  // A4/A5 are SDA/SCL for I2C (LCD), leave them alone.

  pinMode(A6, INPUT_PULLUP); // Input only, set as pullup
  pinMode(BUTTONS_PIN, BUTTONS_PIN_MODE); // Analog buttons ladder input
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
    setChoices_P(
      MSG_LOGS, 1,
      MSG_FEEDING_MENU, 2,
      MSG_SETTINGS, 3);

    choice = selectChoice(3, 1);

    if (choice == 1) viewLogs();
    else if (choice == 2) feedingMenu();
    else if (choice == 3) settings();
    if (uiTaskActive()) return;
  } while (choice != -1);
  forceDisplayRedraw = true;
  displayInfo(screenCounter);
}

static void lcdPrintSpaces(uint8_t count) {
  while (count--) lcd.print(' ');
}

static void lcdPrintPercent3(uint8_t value) {
  if (value < 100) lcd.print(' ');
  if (value < 10) lcd.print(' ');
  lcd.print(value);
  lcd.print('%');
}

static void lcdPrintPadded_P(PGM_P message, uint8_t width) {
  lcdPrint_P(message);
  uint8_t len = strlen_P(message);
  if (len < width) lcdPrintSpaces(width - len);
}

void printSoilAndWaterTrayStatus(bool fullRedraw) {
  static const uint8_t kSoilValueCol = 10;
  static const uint8_t kTrayValueCol = 6;
  static const uint8_t kTrayFieldWidth = DISPLAY_COLUMNS - kTrayValueCol;

  uint16_t soilMoisture = getSoilMoisture();
  uint16_t soilMoisturePercent = soilMoistureAsPercentage(soilMoisture);
  uint8_t trayState = trayWaterLevelAsState();
 
  if (fullRedraw) {
    lcdSetCursor(0, 0);
    lcdPrint_P(MSG_SOIL_NOW);
    lcdPrint_P(MSG_SPACE);

    lcdSetCursor(0, 1);
    lcdPrint_P(MSG_TRAY_NOW);
    lcdPrint_P(MSG_SPACE);
  }

  lcdSetCursor(kSoilValueCol, 0);
  if (soilMoisturePercent < 100) lcd.print(' ');
  if (soilMoisturePercent < 10) lcd.print(' ');
  lcd.print(soilMoisturePercent);
  lcd.print('%');

  lcdSetCursor(kTrayValueCol, 1);
  PGM_P trayLabel = trayWaterLevelInEnglish(trayState);
  lcdPrintPadded_P(trayLabel, kTrayFieldWidth);
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
  lcdPrint_P(MSG_SPACE);
  lcdPrint_P(MSG_LOG_TYPE_0);
  // lcdPrint_P(MSG_SPACE);
  // lcdPrintNumber(currentLogEntry.millisStart);
  // lcdPrint_P(MSG_SPACE);
  // lcdPrint_P(MSG_AGO);
}

void showLogType1() {
  // Feed log entry (entryType = 1); shown when feeding logic is reintroduced.
  lcdClear();
  lcdClear();
  // Print absolute log number (epoch-aware)
  lcd.print((unsigned long)getAbsoluteLogNumber());
  lcdPrint_P(MSG_SPACE);
  lcdPrint_P(MSG_LOG_TYPE_1);
  lcdPrint_P(MSG_SPACE);
  lcdPrintTimePretty(currentLogEntry.millisStart);
  lcdPrint_P(MSG_SPACE);
  lcdPrint_P(MSG_LATER);
  lcdPrint_P(MSG_SPACE);

  lcd.setCursor(0, 1);
  lcd.print("Dur:");
  lcdPrintTimeDuration(currentLogEntry.millisStart, currentLogEntry.millisEnd);

  lcd.setCursor(0, 2);
  lcdPrint_P(MSG_S_COLUMN);
  lcdPrintNumber(currentLogEntry.soilMoistureBefore);
  lcdPrint_P(MSG_PERCENT);
  lcdPrint_P(MSG_DASH);
  lcdPrintNumber(currentLogEntry.soilMoistureAfter);
  lcdPrint_P(MSG_PERCENT);
  lcdPrint_P(MSG_SPACE);

  lcdPrint_P(MSG_W_COLUMN);
  lcdPrint_P(trayWaterLevelInEnglishShort(currentLogEntry.trayWaterLevelBefore));
  // lcdPrintNumber(currentLogEntry.trayWaterLevelBefore);
  lcdPrint_P(MSG_DASH);
  lcdPrint_P(trayWaterLevelInEnglishShort(currentLogEntry.trayWaterLevelAfter));
  // lcdPrintNumber(currentLogEntry.trayWaterLevelAfter);

  lcd.setCursor(0, 3);
  if (currentLogEntry.outcome == 0) lcdPrint_P(MSG_LOG_OUTCOME_0);
  if (currentLogEntry.outcome == 1) lcdPrint_P(MSG_LOG_OUTCOME_1);

  // lcdPrintLogParamsSoil(currentLogEntry.soilMoistureBefore);
  // lcdPrintLogParamsSoil(currentLogEntry.soilMoistureAfter);
}

void showLogType2() {
  lcdClear();
  // Print absolute log number (epoch-aware)
  lcd.print((unsigned long)getAbsoluteLogNumber());
  lcdPrint_P(MSG_SPACE);
  lcdPrint_P(MSG_LOG_TYPE_2);
  lcdPrint_P(MSG_SPACE);
  lcdPrintTimePretty(currentLogEntry.millisStart);
  lcdPrint_P(MSG_SPACE);
  lcdPrint_P(MSG_LATER);
  lcdPrint_P(MSG_SPACE);

  lcd.setCursor(0, 2);
  lcdPrint_P(MSG_SOIL_MOISTURE_COLUMN);
  // lcdPrintNumber(soilMoistureAsPercentage(currentLogEntry.soilMoistureBefore));
  lcdPrintNumber(currentLogEntry.soilMoistureBefore);
  lcdPrint_P(MSG_PERCENT);

  lcd.setCursor(0, 3);
  lcdPrint_P(MSG_TRAY);
  lcdPrint_P(MSG_COLUMN_SPACE);
  lcdPrint_P(trayWaterLevelInEnglish(currentLogEntry.trayWaterLevelBefore));
  
  // lcdPrintLogParamsSoil(currentLogEntry.soilMoistureBefore);
  // lcdPrintLogParamsSoil(currentLogEntry.soilMoistureAfter);
}


void viewLogs() {
  lcdClear();

  // Always start from the latest available log entry
  goToLatestSlot();

  if (noLogs()) {
    lcdFlashMessage_P(MSG_NO_LOG_ENTRIES);
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
  static uint8_t lastScreen = 255;
  static bool lastScreenSaver = false;
  bool fullRedraw = false;

  if (screenSaverMode) {
    lastScreenSaver = true;

    uint8_t x = random(0, 20); // Random column (0-15)
    uint8_t y = random(0, 4);  // Random row (0-3)
  
    // Set the cursor to the random position and print the star character
    lcd.clear();
    lcd.setCursor(x, y);
    // lcd.setCursor(1, 1);
    
    lcd.write('*');

    return;
  }

  if (lastScreenSaver) {
    lastScreenSaver = false;
    lastScreen = 255;
  }

  if (forceDisplayRedraw) {
    lcd.clear();
    lastScreen = screen;
    fullRedraw = true;
    forceDisplayRedraw = false;
  } else if (screen != lastScreen) {
    lcd.clear();
    lastScreen = screen;
    fullRedraw = true;
  }

  switch (screen) {
    case 0: displayInfo1(fullRedraw); break;
    case 1: displayInfo3(fullRedraw); break;
    case 2: displayInfo4(fullRedraw); break;
    case 3: displayInfoChart(fullRedraw); break;
  }
}


void displayInfo1(bool fullRedraw) {
  static const uint8_t kLastFeedValueCol = 11;
  static const uint8_t kAvgValueCol = 14;
  static const uint8_t kLastFeedFieldWidth = DISPLAY_COLUMNS - kLastFeedValueCol;
  static const uint8_t kAvgFieldWidth = DISPLAY_COLUMNS - kAvgValueCol;

  printSoilAndWaterTrayStatus(fullRedraw);

  if (fullRedraw) {
    lcdPrint_P(MSG_LAST_FEED, 2);
    lcdPrint_P(MSG_AVG_COLUMN, 3);
  }

  lcdSetCursor(kLastFeedValueCol, 2);
  lcdPrintSpaces(kLastFeedFieldWidth);
  lcdSetCursor(kLastFeedValueCol, 2);
  if (!millisAtEndOfLastFeed) lcdPrintPadded_P(MSG_NOT_YET, kLastFeedFieldWidth);
  else lcdPrintTimeSince(millisAtEndOfLastFeed);
  // lcdPrintNumber(actionPreviousMillis);

  lcdSetCursor(kAvgValueCol, 3);
  lcdPrintSpaces(kAvgFieldWidth);
  lcdSetCursor(kAvgValueCol, 3);
  if (!averageMsBetweenFeeds) lcdPrintPadded_P(MSG_NA, kAvgFieldWidth);
  // else lcdPrintNumber(averageMsBetweenFeeds);
  else lcdPrintTime(averageMsBetweenFeeds);
}


void displayInfo4(bool fullRedraw) {
  static const uint8_t kSoilValueCol = 10;
  static const uint8_t kRawFieldWidth = 4;
 
  printSoilAndWaterTrayStatus(fullRedraw);
  pinMode(A2, INPUT);

  if (fullRedraw) {
    lcdSetCursor(0, 2);
    lcdPrint_P(MSG_SOIL_NOW);
    lcdPrint_P(MSG_SPACE);
  }

  lcdSetCursor(kSoilValueCol, 2);
  lcdPrintSpaces(kRawFieldWidth);
  lcdSetCursor(kSoilValueCol, 2);
  lcd.print(getSoilMoisture());
}


void displayInfo3(bool fullRedraw) {
  if (!fullRedraw) return;
  lcdPrint_P(MSG_NEXT_FEED, 1);
  lcdPrint_P(MSG_WHEN_CONDITIONS_ALLOW, 2);
}

void displayInfoChart(bool fullRedraw) {
  static const char kSparkLevels[] = " .:-=+*#%@";
  static const uint8_t kSparkLevelCount = sizeof(kSparkLevels) - 1;

  uint8_t count = soilSensorGetLastWindowSampleCount();

  if (count == 0) {
    if (fullRedraw) {
      lcdSetCursor(0, 0);
      lcd.print(F("Moisture chart"));
      lcdClearLine(1);
      lcdClearLine(2);
      lcdClearLine(3);
      lcdSetCursor(0, 2);
      lcd.print(F("No data yet"));
    }
    return;
  }

  uint8_t minP = soilSensorGetLastWindowMinPercent();
  uint8_t maxP = soilSensorGetLastWindowMaxPercent();
  uint8_t avgP = soilSensorGetLastWindowAvgPercent();
  uint8_t nowP = soilMoistureAsPercentage(getSoilMoisture());

  lcdSetCursor(0, 0);
  lcd.print(F("Min:"));
  lcdPrintPercent3(minP);
  lcd.print(F(" Max:"));
  lcdPrintPercent3(maxP);
  lcdPrintSpaces(DISPLAY_COLUMNS - 17);

  char line[DISPLAY_COLUMNS + 1];
  uint8_t range = maxP - minP;
  if (range < 5) range = 5;

  for (uint8_t col = 0; col < DISPLAY_COLUMNS; col++) {
    uint8_t start = (uint32_t)col * count / DISPLAY_COLUMNS;
    uint8_t end = (uint32_t)(col + 1) * count / DISPLAY_COLUMNS;
    if (end <= start) end = start + 1;
    if (end > count) end = count;

    uint16_t sum = 0;
    uint8_t n = 0;
    for (uint8_t i = start; i < end; i++) {
      sum += soilSensorGetLastWindowSample(i);
      n++;
    }
    uint8_t avg = n ? (uint8_t)(sum / n) : 0;

    uint8_t level;
    if (avg <= minP) level = 0;
    else if (avg >= maxP) level = kSparkLevelCount - 1;
    else level = (uint8_t)((uint32_t)(avg - minP) * (kSparkLevelCount - 1) / range);

    line[col] = kSparkLevels[level];
  }
  line[DISPLAY_COLUMNS] = '\0';

  lcdSetCursor(0, 1);
  lcd.print(line);

  lcdSetCursor(0, 2);
  lcd.print(F("Avg:"));
  lcdPrintPercent3(avgP);
  lcd.print(F(" Now:"));
  lcdPrintPercent3(nowP);
  lcdPrintSpaces(DISPLAY_COLUMNS - 17);

  if (fullRedraw) lcdClearLine(3);
}
