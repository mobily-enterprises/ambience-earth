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
        Cal. moisture sensor
        Cal. water levels
        Cal. empty tray sensor  
      Reset
    
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

  // Initialize EEPROM

  initLcdAndButtons();
  void initSensors();

  loadConfig();
 
  // inputString("Enter a string:", "STOcaZzO AND $T", "Configuring meaw");

  if (!configChecksumCorrect()) {
    Serial.println("CHECKSUM INCORRECT?!?");
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
  int choice;
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

  unsigned char choice = selectChoice(c, 0);
  
  // List appropriate actions
  // Pick one
  // run runAction(int) on the picked action
}

void settings () {
  int choice = 0;

  while(choice != 4 && choice != -1) {
    setChoices(
      "Edit actions", 1, 
      "Default moist levls", 2,
      "Calibrate", 3,
      "Factory reset", 10,
      "Go back", 11
    );
    choice = selectChoice(4, 2);
    lcdPrintNumber(choice, 4);
    delay(1000);


    if (choice == 1) settingsEditActions();
    else if (choice == 2) settingsDefaultMoistLevels();
    else if (choice == 3) settingsCalibrate();
  } 
}

// YOU ARE HERE
void settingsEditActions() {
    

/*
      -Action 1
      -Action 2
      -Action 3
      -Action 4
      -Action 5
      -Action 6
      -Action 7
      -Empty tray
*/
}

void settingsDefaultMoistLevels() {
  uint8_t soilLittleMoistPercentage;
  uint8_t soilMoistPercentage;
  uint8_t soilVeryMoistPercentage;

  soilVeryMoistPercentage = inputNumber("Over ", config.soilVeryMoistPercentage, 5, 0, 95, "%", "");
  if (soilVeryMoistPercentage == -1) return

  soilMoistPercentage = inputNumber("Over ", config.soilMoistPercentage, 5, 0, config.soilVeryMoistPercentage - 5, "%", "");
  if (soilVeryMoistPercentage == -1) return
  
  soilLittleMoistPercentage = inputNumber("Over ", config.soilLittleMoistPercentage, 5, 0, config.soilMoistPercentage - 5, "%", "");
  if (soilVeryMoistPercentage == -1) return

  lcdClear();
  
  lcdPrint("0\%-", 0);
  lcdPrint(soilLittleMoistPercentage - 1);
  lcdPrint("% Dry");

  lcdPrint(soilLittleMoistPercentage, 1);
  lcdPrint("%-");
  lcdPrint(soilMoistPercentage - 1);
  lcdPrint("% Little moist");

  lcdPrint(soilMoistPercentage, 1);
  lcdPrint("%-");
  lcdPrint(soilVeryMoistPercentage - 1);
  lcdPrint("% Moist");

  lcdPrint(soilVeryMoistPercentage, 1);
  lcdPrint("%-100%");
  lcdPrint("% Very moist");


  lcdPrint(soilLittleMoistPercentage + 1, 1);
  lcdPrint("%-");
  lcdPrint(soilMoistPercentage);
  lcdPrint("% moist");

  lcdPrint(soilMoistPercentage + 1, 1);
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
  lcdPrint("Unit ready!");

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




