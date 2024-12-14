
#include "config.h"

#include "hardwareConf.h"
#include "moistureSensor.h"

#include <Arduino.h>

extern Config config;

void initMoistureSensor() {
  pinMode(SOIL_MOISTURE_SENSOR_POWER, OUTPUT);

  pinMode(SOIL_MOISTURE_SENSOR, INPUT);

  digitalWrite(SOIL_MOISTURE_SENSOR_POWER, HIGH); // Power on the sensor
  delay(SENSOR_STABILIZATION_TIME); // Stabilize the sensor    
}

uint16_t runSoilSensorLazyReadings() {
  return soilSensorOp(0);
}

uint16_t getSoilMoisture() {
  return soilSensorOp(1);
}

void setSoilSensorLazy() {
  soilSensorOp(2);
}

void setSoilSensorRealTime() {
  soilSensorOp(3);
}

uint16_t soilSensorOp(uint8_t op) {
  static uint16_t lazyValue = 0;
  static uint8_t readMode = 1; // Real time mode by default
  static uint8_t state = 0;    // Initial state: everything is off

  static unsigned long lastReadTime = 0;    // Last lazy read time
  static unsigned long sensorOnTime = 0;   // Time when the sensor was powered on

  const unsigned long readInterval = SENSOR_READ_INTERVAL;
  const unsigned long stabilizationTime = SENSOR_STABILIZATION_TIME;

  switch (op) {
    case 0: { // Update laze value after stabilising
      if (readMode == 0) {
        switch (state) {
          case 0: // Idle, check if it's time to read
            if (millis() - lastReadTime >= readInterval) {
              digitalWrite(SOIL_MOISTURE_SENSOR_POWER, HIGH); // Power on the sensor
              sensorOnTime = millis(); // Record when the sensor was turned on
              state = 1;
            }
            break;

          case 1: // Stabilizing and reading
            if (millis() - sensorOnTime >= stabilizationTime) {
              lazyValue = analogRead(SOIL_MOISTURE_SENSOR); // Read the sensor value
              lastReadTime = millis(); // Update the last read time
              digitalWrite(SOIL_MOISTURE_SENSOR_POWER, LOW); // Power off the sensor
              state = 0; // Return to idle state
            }
            break;
        }
      }
      return lazyValue; // Always return the last lazy value
    }

    case 1: { // Immediate read
      if (readMode == 1) {
        uint16_t realTimeValue = analogRead(SOIL_MOISTURE_SENSOR); // Read the sensor value
        return realTimeValue; // Return the real-time value
      } else {
        return lazyValue; // Default to lazy value if in lazy mode
      }
    }

    case 2: { // Set lazy mode
      digitalWrite(SOIL_MOISTURE_SENSOR_POWER, LOW); // Power off the sensor
      readMode = 0; // Switch to lazy mode
      lazyValue = 0; // Reset lazy value to avoid stale data
      state = 0; // Reset to idle state
      break;
    }

    case 3: { // Set real-time mode
      readMode = 1; // Switch to real-time mode
      digitalWrite(SOIL_MOISTURE_SENSOR_POWER, HIGH); // Power on the sensor
      delay(stabilizationTime); // Stabilize the sensor
      break;
    }

    default: // Handle invalid operation codes
      return 0xFFFF; // Return a special value for invalid operations
  }

  return 0; // Default return for non-reading operations
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

