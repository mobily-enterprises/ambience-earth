
#include "config.h"
#include "sensors.h"

#include <Arduino.h>

extern Config config;

void initSensors() {
  pinMode(SOIL_LITTLE_MOISTURE_SENSOR, INPUT);
  pinMode(TRAY_WATER_LEVEL_SENSOR, INPUT);
  pinMode(TRAY_FULL_SENSOR, INPUT_PULLUP); 

  pinMode(PUMP_IN, OUTPUT);
  pinMode(SOLENOID_IN, OUTPUT);
  pinMode(PUMP_OUT_DEVICE, OUTPUT);
}

void openLineIn() {
   digitalWrite(config.feedLine, HIGH);
}

void closeLineIn() {
   digitalWrite(config.feedLine, LOW);
}

void openPumpOut() {
  digitalWrite(PUMP_OUT_DEVICE, HIGH);
}

void closePumpOut() {
  digitalWrite(PUMP_OUT_DEVICE, LOW);
}


uint16_t senseSoilMoisture() {
  return (uint16_t) analogRead(SOIL_LITTLE_MOISTURE_SENSOR);
}

uint16_t senseTrayWaterLevel() {
  return analogRead(TRAY_WATER_LEVEL_SENSOR);
}

bool senseTrayIsFull() {
  bool s = digitalRead(TRAY_FULL_SENSOR);
  return !s;
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


uint8_t trayWaterLevelAsState(uint8_t trayWaterLevelAsPercentage) {
  uint8_t v = trayWaterLevelAsPercentage;

  if (v > 50) return (uint8_t) Conditions::TRAY_PLENTY;
  else if(v > 2) return (uint8_t) Conditions::TRAY_SOME;
  else if(v > 0) return (uint8_t) Conditions::TRAY_EMPTY;
  else return (uint8_t) Conditions::TRAY_DRY;
}


char* trayWaterLevelInEnglish(uint8_t trayWaterLevelAsState, bool trayIsFull) { 
  if (trayIsFull) {
    if (trayWaterLevelAsState != Conditions::TRAY_PLENTY) return MSG_ERROR_1;
    return MSG_FULL;
  }

  if (trayWaterLevelAsState == Conditions::TRAY_PLENTY) return MSG_TRAY_PLENTY;
  else if (trayWaterLevelAsState == Conditions::TRAY_SOME) return MSG_TRAY_SOME;
  else if (trayWaterLevelAsState == Conditions::TRAY_EMPTY) return MSG_TRAY_EMPTY;
  else if (trayWaterLevelAsState == Conditions::TRAY_DRY) return MSG_TRAY_PLENTY;
}