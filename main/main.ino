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
    X Allow for stack feeding: run action one time in 4, feeds, etc.
    X Write function to write settings to ROM
    X Write function to initialise ROM to factory defaults
    X Load config on startup: if not initialised, set factory defaults and force calibration

    X Reorganise WHOLE codebase so that it actually makes sense and doesn't waste memory

    Ask initial questions
     X Top/bottom
     X Tray or wastepipe (trayNeedsEmptying, which if false nullifies all of the "water out" business)
     - Calibrate

    Write calibration procedures 
    After initial questions, resave configuration

    Make main menu

    Run action... (only ones matching watering method)
    
    Settings
      Edit actions
        - (List of actions)
          - Edit
            -Name
            -Start conditions
            -Stop conditions
            -Active         
          - Delete
      Calibration
        X Cal. moisture sensor
        Cal. water levels
          
      Reset
      Set active actions (up to 4)


    Allow function to reset config and re-ask initial questions
    Have screen display current actions configured and current parameters

    Actually run the set programmes, displaying stats
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
    choice = selectChoice(2, 1);
    
    if (choice == 1) pickAndRunAction();
    else if (choice == 2) settings();
  } while(choice != 3);
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
  lcdPrint("Soil ", 0);

  lcdPrintNumber(soilMoisturePercent);
  lcdPrint("% ");
  lcdPrint(soilMoistureInEnglish(soilMoisturePercent));
  lcdPrint(" ");

  lcdPrint("Tray ", 1);
  lcdPrint(trayWaterLevelInEnglish(trayWaterLevel, trayIsFull));

  lcdPrint("On when: ", 2);
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

Conditions inputConditions(Conditions *initialConditions, char* prompt) {
  Serial.println(prompt);

  setChoices(
    "Ignore tray", Conditions::TRAY_IGNORED,
    "Tray is empty", Conditions::TRAY_EMPTY,
    "Tray is wet", Conditions::TRAY_WET,
    "Tray is half full", Conditions::TRAY_HALF_FULL,
    "Tray is full", Conditions::TRAY_FULL
  );
  int8_t tray = (int8_t) selectChoice( 5, (int8_t)initialConditions->tray, prompt);
  if (tray == -1) return;

  setChoices(
    "Ignore soil", Conditions::SOIL_IGNORED,
    "Soil is dry", Conditions::SOIL_DRY,
    "Soil is damp", Conditions::SOIL_DAMP,
    "Soil is wet", Conditions::SOIL_WET,
    "Soil is very wet", Conditions::SOIL_VERY_WET
  );
  int8_t soil = (int8_t) selectChoice( 5, (int8_t)initialConditions->soil, prompt);
  if (soil == -1) return;

  int8_t logic;

  if (tray != -1 && soil != -1) {
    setChoices(
      "BOTH tray and soil", Conditions::SOIL_IGNORED,
      "EITHER tray or soil", Conditions::SOIL_DRY
    );
    logic = (int8_t) selectChoice( 5, (int8_t)initialConditions->logic, prompt);
    if (logic == -1) return;
  } else {
    logic = Conditions::NO_LOGIC;
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
    inputString("Name:", config.actions[choiceId].name, nullptr);
    if (getUserInputString()[0] == '*') continue;

    Conditions triggerConditions;
    triggerConditions.tray = config.actions[choiceId].triggerConditions.tray;
    triggerConditions.soil = config.actions[choiceId].triggerConditions.soil;
    triggerConditions.logic = config.actions[choiceId].triggerConditions.logic;
    inputConditions(&triggerConditions, "Open water when...");
    if (triggerConditions.tray == -1 || triggerConditions.soil == -1 || triggerConditions.logic == -1) continue;

    // Apply changes back to config.actions[choiceId].triggerConditions
    config.actions[choiceId].triggerConditions.tray = triggerConditions.tray;
    config.actions[choiceId].triggerConditions.soil = triggerConditions.soil;
    config.actions[choiceId].triggerConditions.logic = triggerConditions.logic;

    break;
  }
}

void settingsDefaultMoistLevels() {
  int8_t soilLittleMoistPercentage;
  int8_t soilMoistPercentage;
  int8_t soilVeryMoistPercentage;

  Serial.println("D1");
  soilVeryMoistPercentage = inputNumber("Over ", config.soilVeryMoistPercentage, 5, 0, 95, "%", "Very moist");
  if (soilVeryMoistPercentage == -1) return
Serial.println("D2");
  soilMoistPercentage = inputNumber("Over ", config.soilMoistPercentage, 5, 0, config.soilVeryMoistPercentage - 5, "%", "Moist");
  if (soilVeryMoistPercentage == -1) return
  Serial.println("D3");
  soilLittleMoistPercentage = inputNumber("Over ", config.soilLittleMoistPercentage, 5, 0, config.soilMoistPercentage - 5, "%", "Little moist");
  if (soilVeryMoistPercentage == -1) return
Serial.println("D4");
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




  /*
  userInput = inputNumber("How moist?", 50, 5, 0, 100, "%", "Water level");

  setChoices("First", 1, "Second", 2)
  userInput = selectChoice(2, 1, "Insert here");
  // String userInput = inputString("Enter a string:", "STOcaZzO AND $T", "Configuring meaw");
  
  // Display the entered string on the LCD
  lcdClear();

  delay(1000);
  */

  /*
  lcd.setCursor(0, 0);
lcd.print("Michelinoooooo");
lcd.setCursor(0, 1);
lcd.print(millis() / 1000);

  Serial.print("LED from LOW to HIGH\n");
  digitalWrite(LED_PIN, HIGH);
  delay(1000); 
  lcd.clear();
  Serial.print("LED from HIGH to LOW\n");
  digitalWrite(LED_PIN, LOW);
  delay(1000); 

  // Read analog values from analog pins A0, A1, and A2
  int sensorValueA0 = analogRead(A0);
  int sensorValueA1 = analogRead(A1);
  int sensorValueA2 = analogRead(A2);
  int sensorValueA3 = analogRead(A3);

  // Print the sensor values to the serial monitor
  Serial.print("Sensor value at A0: ");
  Serial.println(sensorValueA0);
  
  Serial.print("Sensor value at A1: ");
  Serial.println(sensorValueA1);
  
  Serial.print("Sensor value at A2: ");
  Serial.println(sensorValueA2);

  Serial.print("Sensor value at A3: ");
  Serial.println(sensorValueA3);
*/




