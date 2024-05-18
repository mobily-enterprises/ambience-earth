#include "config.h"
#include "sensors.h"

#include <Arduino.h>

extern Config config;

void initSensors() {
    // pinMode(LED_PIN, OUTPUT); // Set LED pin as output
    pinMode(SOIL_MOISTURE_SENSOR, INPUT);
    pinMode(TRAY_WATER_LEVEL_SENSOR, INPUT);
    pinMode(TRAY_FULL_SENSOR, INPUT);
}

uint16_t  senseSoilMosture() {
  return (uint16_t) analogRead(SOIL_MOISTURE_SENSOR);
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

/*
SOAKED 400 100%
DRY    700 0%

shifted : delta = X : 100

raw   shifted
-----------------
702   -2 
701   -1
700    0
699    1
698    2
697    3

403 297
402 298 
401 299
400 300 
401 301

*/

char* soilMoistureInEnglish(uint8_t soilMoistureAsPercentage) {
  uint8_t v = soilMoistureAsPercentage;

  if (v > config.soilVeryMoistPercentage) return "Very moist";
  else if(v > config.soilMoistPercentage) return "Moist";
  else if(v > config.soilLittleMoistPercentage) return "Little moist";
  else return "Dry";
}

uint16_t senseTrayWaterLevel() {
  return analogRead(TRAY_WATER_LEVEL_SENSOR);
}

bool senseTrayIsFull() {
  // TEMPORARY CODE
  int s = analogRead(TRAY_FULL_SENSOR);
  return s > 100;
}

char* trayWaterLevelInEnglish(uint16_t trayWaterLevel, bool trayIsFull) {
  if (trayIsFull) {
    if (trayWaterLevel < config.trayWaterLevelSensorCalibrationHalfFull) return "Error 1";
    return "Full";
  }

  if (trayWaterLevel <= config.trayWaterLevelSensorCalibrationEmpty) return "Empty";
  else if (trayWaterLevel <= config.trayWaterLevelSensorCalibrationWet) return "Wet";
  else if (trayWaterLevel <= config.trayWaterLevelSensorCalibrationHalfFull) return "< Half";
  else return "> Half";
}
