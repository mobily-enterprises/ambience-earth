#include <Wire.h>
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
extern Button* pressedButton;

const unsigned long interval = 1000;  // Interval in milliseconds (1000 milliseconds = 1 second)
unsigned long previousMillis = 0;

/*
  TODO: 
    Set active actions (up to 4)
    Test that editing action doesn't allow "feedFrom" if active
    Write calibration procedures for water level
    Force calibration procedures on setup
    Implement "reset"
    Print stats on home screen
    Add "View logs" function to browse through logs
    Implement running of actions, with timeout to prevent floods
    Implement built-in action "emptyTray", config to enable it
    Write infrastructure for stats, logging waterings with date/event, cycling EEPROM  
    Main screen: show stats, current settings (scrolling)
*/

void emptyTrayIfNecessary() {
  if (config.trayNeedsEmptying) {
    // TODO: EMPTY TRAY
  } 
}

void setup() {
  
  initLcdAndButtons();

  extern LiquidCrystal_I2C lcd;
  pinMode(BUTTONS_PIN, INPUT_PULLUP);

  Serial.begin(9600);  // Initialize serial communication at 9600 baud rate
  // pinMode(LED_PIN, OUTPUT); // Set LED pin as output

  initLcdAndButtons();
  void initSensors();

  loadConfig();
 
  if (!configChecksumCorrect()) {
    // Serial.println("CHECKSUM INCORRECT?!?");
    restoreDefaultConfig();
    saveConfig();
  }
 
  // runInitialSetup();
   
  if (config.mustRunInitialSetup) {
    while(!runInitialSetup());
  }


}
#include <LiquidCrystal_I2C.h>

extern LiquidCrystal_I2C lcd;

void loop() {
  
  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    emptyTrayIfNecessary();
    displayInfo();
  }


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
      "Run action", 1, 
      "Settings", 2,
      "Return to op", 3
    );
    choice = selectChoice(3, 1);
    
    if (choice == 1) pickAndRunAction();
    else if (choice == 2) settings();
  } while(choice != 3 && choice != -1);
}

void displayInfo() {
  uint16_t soilMoisture;
  uint8_t soilMoisturePercent;
  uint16_t trayWaterLevel;
  bool trayIsFull;
  
  soilMoisture = senseSoilMosture();
  soilMoisturePercent = soilMoistureAsPercentage(soilMoisture);
  trayWaterLevel = senseTrayWaterLevel();
  trayIsFull = senseTrayIsFull();

  lcdClear();
  lcdPrint("Soil", 0);
  lcdPrint(MSG_SPACE);

  lcdPrintNumber(soilMoisturePercent);
  lcdPrint("%");
  lcdPrint(MSG_SPACE);
  lcdPrint(soilMoistureInEnglish(soilMoisturePercent));
  lcdPrint(MSG_SPACE);

  lcdPrint("Tray", 1);
  lcdPrint(MSG_SPACE);
  lcdPrint(trayWaterLevelInEnglish(trayWaterLevel, trayIsFull));

  lcdPrint("On when:", 2);
  lcdPrint(MSG_SPACE);
}

void pickAndRunAction() {
  uint8_t i = 0, c = 0;
  for (i = 0; i <= 5; i++){      
    if (config.feedFrom == config.actions[i].feedFrom) {
      setChoice(c, config.actions[i].name, i);
      c++;
    }
  }

  int8_t choice = selectChoice(c, 0);
  
  // List appropriate actions
  // Pick one
  // run runAction(int) on the picked action
}




void settings () {
  int8_t choice = 0;

  while(choice != 4 && choice != -1) {
    lcdClear();
    setChoices(
      "Edit actions", 1, 
      "Moist levls", 2,
      "Calibrate", 3,
      "Reset", 10,
      "Go back", 11
    );
    choice = selectChoice(4, 1);

    if (choice == 1) settingsEditActions();
    else if (choice == 2) settingsDefaultMoistLevels();
    else if (choice == 3) settingsCalibrate();
  } 
}


bool isActionActive(int8_t actionId) {
  for (size_t i = 0; i < 4; ++i) {
    if (config.activeActionsIndexes[i] == actionId) {
      return true;
    }
  }
  return false;
}

Conditions inputConditions(Conditions *initialConditions, char *verb, int8_t choiceId) {

  char message[20];
  strcpy(message, "XXXX when tray is:");
  memcpy(message, verb, 4);
  
  setChoices(
    MSG_TRAY_SOIL_IGNORE, Conditions::TRAY_IGNORED,
    MSG_TRAY_EMPTY, Conditions::TRAY_EMPTY,
    MSG_TRAY_WET, Conditions::TRAY_WET,
    MSG_TRAY_HALF, Conditions::TRAY_HALF_FULL,
    MSG_TRAY_FULL, Conditions::TRAY_FULL
  );
  int8_t tray = (int8_t) selectChoice( 5, (int8_t)initialConditions->tray, message);
  initialConditions->tray = tray;
  Serial.println(initialConditions->tray);
  if (tray == -1) return;

  setChoices(
    MSG_TRAY_SOIL_IGNORE, Conditions::SOIL_IGNORED,
    MSG_SOIL_DRY, Conditions::SOIL_DRY,
    MSG_SOIL_DAMP, Conditions::SOIL_DAMP,
    MSG_SOIL_WET, Conditions::SOIL_WET,
    MSG_SOIL_VERY_WET, Conditions::SOIL_VERY_WET
  );
  int8_t soil = (int8_t) selectChoice( 5, (int8_t)initialConditions->soil, message);
  initialConditions->soil = soil;
  if (soil == -1) return;

  int8_t logic;
  
  if (tray != Conditions::TRAY_IGNORED && soil != Conditions::SOIL_IGNORED) {
    setChoices(
      "Tray AND soil", Conditions::SOIL_IGNORED,
      "Tray OR soil", Conditions::SOIL_DRY
    );
    logic = (int8_t) selectChoice( 5, (int8_t)initialConditions->logic, "Logic");
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
  while(true) {

    // Set the choices
    for (i = 0; i <= 5; i++) setChoice(i, config.actions[i].name, i);

    // Pick one. If -1, then get out of this menu with the "break"
    int8_t choiceId = selectChoice(i, 0);
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
      
      setChoices(
        MSG_TOP, FeedFrom::FEED_FROM_TOP,
        MSG_TRAY, FeedFrom::FEED_FROM_TRAY
      );
      feedFrom = (int8_t) selectChoice( 2, config.actions[choiceId].feedFrom, MSG_FEED_FROM);
      
      if (feedFrom == -1) return;
    }

    if (confirm(MSG_SAVE_QUESTION)) {

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
  lcdPrint("% Little moist");

  lcdPrintNumber(soilMoistPercentage, 2);
  lcdPrint("%-");
  lcdPrintNumber(soilVeryMoistPercentage - 1);
  lcdPrint("% Moist");

  lcdPrintNumber(soilVeryMoistPercentage, 3);
  lcdPrint("%-100% Very moist");
  
  delay(3000);
  if (confirm("Save?")) {
     config.soilLittleMoistPercentage = soilLittleMoistPercentage;
     config.soilMoistPercentage = soilMoistPercentage;
     config.soilVeryMoistPercentage = soilVeryMoistPercentage;

    saveConfig();
  }
}

void settingsCalibrate() {
}

int runInitialSetup() {
  setChoices("TOP", FeedFrom::FEED_FROM_TOP, "BOTTOM (TRAY)", FeedFrom::FEED_FROM_TRAY);
  config.feedFrom = selectChoice(2, FeedFrom::FEED_FROM_TOP, "Feeding from...");
  
  if (config.feedFrom == FeedFrom::FEED_FROM_TOP) {
    config.trayNeedsEmptying = confirm(MSG_AUTO_DRAIN, false);
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
  return true;  
}



int readSensor(int sensor) {
  
}

int calibrateSoilMoistureSensor() {
  int goAhead;
  int soaked;
  int dry;

  goAhead = confirm("DRY moist sensor", 1);
  if (!goAhead) {
    lcdAbortedMessage();
    return;
  }
  lcdClear();
  lcdPrint("Dry: ", 1);
  for(int i = 0;i < 4;i++){
    dry = senseSoilMosture();
    lcdClear();
    lcdPrintNumber(dry);
    lcdPrint(" ");
    delay(900);
  }

  delay(2000);
  
  goAhead = confirm("SOAKED moist sensor", 1);
  if (!goAhead) {
    lcdAbortedMessage();
    return;
  }

  lcdClear();
  lcdPrint("Soaked: ", 1);
  for(int i = 0;i < 4;i++){
    soaked = senseSoilMosture();
    lcdClear();
    lcdPrintNumber(soaked);
    lcdPrint(" ");
    delay(900);
  }
  
  config.moistSensorCalibrationSoaked = soaked;
  config.moistSensorCalibrationDry = dry;
  
  return true;
}