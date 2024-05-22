
#include "config.h"
#include "sensors.h"

#include <Arduino.h>

extern Config config;

void initSensors() {
  pinMode(SOIL_LITTLE_MOISTURE_SENSOR, INPUT);
  pinMode(TRAY_WATER_LEVEL_SENSOR, INPUT);
  pinMode(TRAY_FULL_SENSOR, INPUT_PULLUP); 
}

uint16_t  senseSoilMosture() {
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
  
  if (soilMoistureState == Conditions::SOIL_VERY_MOIST) return "Very moist";
  else if(soilMoistureState == Conditions::SOIL_MOIST) return "Moist";
  else if(soilMoistureState == Conditions::SOIL_LITTLE_MOIST) return "Little moist";
  else if(soilMoistureState == Conditions::SOIL_DRY) return "Dry";
}

uint8_t trayWaterLevelAsPercentage(uint16_t waterLevel) {
  uint16_t empty = config.trayWaterLevelSensorCalibrationEmpty;
  uint16_t half = config.trayWaterLevelSensorCalibrationHalf;
  uint16_t full = config.trayWaterLevelSensorCalibrationFull;
  unsigned int delta;

  if (waterLevel <= 10) {
      return 0;
  } else if (waterLevel <= empty) {
      return 1;
  } else if (waterLevel < half) {
      return (float)(waterLevel - empty) / (half - empty) * 50;
  } else if (waterLevel <= full) {
      return 50 + (float)(waterLevel - half) / (full - half) * 50;
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
    if (trayWaterLevelAsState != Conditions::TRAY_PLENTY) return "Error 1";
    return "Full";
  }

  if (trayWaterLevelAsState == Conditions::TRAY_PLENTY) return "Plenty";
  else if (trayWaterLevelAsState == Conditions::TRAY_SOME) return "Some";
  else if (trayWaterLevelAsState == Conditions::TRAY_EMPTY) return "Empty";
  else if (trayWaterLevelAsState == Conditions::TRAY_DRY) return "Dry";
}