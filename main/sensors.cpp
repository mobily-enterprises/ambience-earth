
#include "config.h"
#include "sensors.h"

#include <Arduino.h>

extern Config config;

void initSensors() {
  pinMode(SOIL_MOISTURE_SENSOR, INPUT);
  pinMode(TRAY_WATER_LEVEL_SENSOR, INPUT);
  pinMode(TRAY_FULL_SENSOR, INPUT_PULLUP); 


  pinMode(PUMP_IN_DEVICE, OUTPUT);
  digitalWrite(PUMP_IN_DEVICE, HIGH);

  pinMode(SOLENOID_IN_DEVICE, OUTPUT);
  digitalWrite(SOLENOID_IN_DEVICE, HIGH);

  pinMode(PUMP_OUT_DEVICE, OUTPUT);
  digitalWrite(PUMP_OUT_DEVICE, HIGH);
}

void openLineIn(){
  if (config.feedLine == FeedLine::PUMP_IN) {
    digitalWrite(PUMP_IN_DEVICE, LOW);
  } else {
    digitalWrite(SOLENOID_IN_DEVICE, LOW);
  }
}

void closeLineIn(){
  if (config.feedLine == FeedLine::PUMP_IN) {
    digitalWrite(PUMP_IN_DEVICE, HIGH);
  } else {
    digitalWrite(SOLENOID_IN_DEVICE, HIGH);
  }
}

void openPumpOut() {
  digitalWrite(PUMP_OUT_DEVICE, HIGH);
}

void closePumpOut() {
  digitalWrite(PUMP_OUT_DEVICE, LOW);
}

uint16_t senseSoilMoisture() {
  const uint8_t NUM_READINGS = 10;  // Number of readings to average
  static uint16_t readings[NUM_READINGS];  // Array to store readings
  static uint8_t readIndex = 0;            // Index of the current reading
  static uint32_t total = 0;               // Sum of the readings
  static uint16_t average = 0;             // Average of the readings

  // Subtract the last reading from the total
  total -= readings[readIndex];
  
  // Read a new value
  readings[readIndex] = (uint16_t) analogRead(SOIL_MOISTURE_SENSOR);
  
  // Add the new reading to the total
  total += readings[readIndex];
  
  // Advance to the next position in the array
  readIndex = (readIndex + 1) % NUM_READINGS;
  
  // Calculate the average
  average = total / NUM_READINGS;
  
  return average;
}


uint16_t senseTrayWaterLevel() {
  const uint8_t NUM_READINGS = 10;  // Number of readings to average
  static uint16_t readings[NUM_READINGS];  // Array to store readings
  static uint8_t readIndex = 0;            // Index of the current reading
  static uint32_t total = 0;               // Sum of the readings
  static uint16_t average = 0;             // Average of the readings

  // Subtract the last reading from the total
  total -= readings[readIndex];
  
  // Read a new value
  readings[readIndex] = (uint16_t) analogRead(TRAY_WATER_LEVEL_SENSOR);
  
  // Add the new reading to the total
  total += readings[readIndex];
  
  // Advance to the next position in the array
  readIndex = (readIndex + 1) % NUM_READINGS;
  
  // Calculate the average
  average = total / NUM_READINGS;
  
  return average;
}

bool senseTrayIsFull() {
    // Static variables retain their values between function calls
    static const int NUM_READINGS = 5;
    static bool readings[NUM_READINGS] = {0}; // Array to store recent readings
    static int currentIndex = 0; // Index to keep track of the current reading
    
    bool currentReading = !digitalRead(TRAY_FULL_SENSOR); // Read the current state of the sensor
    
    readings[currentIndex] = currentReading; // Store the current reading in the array
    currentIndex = (currentIndex + 1) % NUM_READINGS; // Move to the next index, wrapping around if necessary
    
    // Check if there are 5 consecutive readings of 1
    int countOnes = 0;
    for (int i = 0; i < NUM_READINGS; ++i) {
        if (readings[i]) {
            countOnes++;
            if (countOnes == NUM_READINGS)
                return true;
        } else {
            countOnes = 0; // Reset the count if a zero is encountered
        }
    }
    
    return false; // Return false if not enough consecutive readings of 1
}



uint8_t soilMoistureAsPercentage(uint16_t soilMoisture) {
  uint16_t soaked = config.moistSensorCalibrationSoaked;
  uint16_t dry = config.moistSensorCalibrationDry;
  unsigned int delta = dry - soaked;
  uint16_t percentage;
    
  unsigned int shifted = config.moistSensorCalibrationDry - soilMoisture;
 
//   Serial.println(shifted);   
  if (shifted == 0 || shifted > 60000) shifted = 1;
  else if (shifted > delta) shifted = delta;

  percentage = (shifted * 100) / delta;
  // Serial.println(shifted);
  // Serial.println(percentage);
  
  
  return percentage;
}

uint8_t soilMoistureAsState(uint8_t soilMoistureAsPercentage) {
  uint8_t v = soilMoistureAsPercentage;

  if (v > config.soilVeryMoistPercentage) return (uint8_t) Conditions::SOIL_VERY_MOIST;
  else if(v > config.soilMoistPercentage) return (uint8_t) Conditions::SOIL_MOIST;
  else if(v > config.soilLittleMoistPercentage) return (uint8_t) Conditions::SOIL_LITTLE_MOIST;
  else return (uint8_t) Conditions::SOIL_DRY;
}

char* soilMoistureInEnglish(uint8_t soilMoistureState) {
  if (soilMoistureState == Conditions::SOIL_VERY_MOIST) return MSG_SOIL_VERY_MOIST;
  else if(soilMoistureState == Conditions::SOIL_MOIST) return MSG_SOIL_MOIST;
  else if(soilMoistureState == Conditions::SOIL_LITTLE_MOIST) return MSG_SOIL_LITTLE_MOIST;
  else if(soilMoistureState == Conditions::SOIL_DRY) return MSG_SOIL_DRY;
}

uint8_t trayWaterLevelAsPercentage(uint16_t waterLevel) {
  uint16_t empty = config.trayWaterLevelSensorCalibrationEmpty;
  uint16_t half = config.trayWaterLevelSensorCalibrationHalf;
  uint16_t full = config.trayWaterLevelSensorCalibrationFull;

  if (waterLevel <= 200 ) {
    return 0;
  } else if (waterLevel <= empty ) {
    return 1;
  } else if (waterLevel <= half) {
    float emptyLog = log(empty);
    float halfLog = log(half);
    float waterLevelLog = log(waterLevel);

    float percentage = ((waterLevelLog - emptyLog) / (halfLog - emptyLog)) * 50;
    return round(percentage);
  } else if (waterLevel <= full) {
    float halfLog = log(half);
    float fullLog = log(full);
    float waterLevelLog = log(waterLevel);

    float percentage = 50 + ((waterLevelLog - halfLog) / (fullLog - halfLog)) * 50;
    return round(percentage);
  } else {
    return 100;
  }
}


uint8_t trayWaterLevelAsState(uint8_t percentage, bool isFull) {
  uint8_t v = percentage;

  // The "isFull" sensor will force a "plenty" state
  if (isFull) return (uint8_t) Conditions::TRAY_FULL;;

  if (v > 50) return (uint8_t) Conditions::TRAY_PLENTY;
  else if(v > 2) return (uint8_t) Conditions::TRAY_SOME;
  else if(v > 0) return (uint8_t) Conditions::TRAY_EMPTY;
  else return (uint8_t) Conditions::TRAY_DRY;
}


char* trayWaterLevelInEnglish(uint8_t trayState, bool trayIsFull) { 

  /*if (trayIsFull) {
    if (trayState != Conditions::TRAY_PLENTY) return MSG_ERROR_1;
    return MSG_FULL;
  }*/

  if (trayState == Conditions::TRAY_FULL) return MSG_TRAY_FULL;
  if (trayState == Conditions::TRAY_PLENTY) return MSG_TRAY_PLENTY;
  else if (trayState == Conditions::TRAY_SOME) return MSG_TRAY_SOME;
  else if (trayState == Conditions::TRAY_EMPTY) return MSG_TRAY_EMPTY;
  else if (trayState == Conditions::TRAY_DRY) return MSG_TRAY_DRY;
}