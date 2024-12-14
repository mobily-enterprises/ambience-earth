
#include "config.h"

#include "hardwareConf.h"
#include "moistureSensor.h"

#include <Arduino.h>

extern Config config;

void initMoistureSensor() {
  pinMode(SOIL_MOISTURE_SENSOR_POWER, OUTPUT);

  pinMode(SOIL_MOISTURE_SENSOR, INPUT);
  digitalWrite(SOIL_MOISTURE_SENSOR_POWER, LOW);
}

uint16_t senseSoilMoisture(uint8_t mode = 0) {
  digitalWrite(SOIL_MOISTURE_SENSOR_POWER, HIGH);  // Ensure the sensor is on
  delay(200);
  analogRead(SOIL_MOISTURE_SENSOR);  // Read the sensor value
  delay(200);
  uint16_t res = analogRead(SOIL_MOISTURE_SENSOR);  // Read the sensor value
  digitalWrite(SOIL_MOISTURE_SENSOR_POWER, LOW);  // Ensure the sensor is on
  return res;
}

uint16_t senseSoilMoistureLazy(uint8_t mode = 0) {
  static uint8_t state = 0;                 // Initial state
  static unsigned long lastReadTime = 0;    // Last time the sensor was read
  static uint16_t lastSensorValue = 0;      // Last read sensor value
  static unsigned long stateEntryTime = 0;  // Time when the state was entered
  static uint8_t previousMode = 1;          // Default to idle mode if no mode is specified
  static bool firstRun = true;              // Flag to check if this is the first run

  const unsigned long readInterval = SENSOR_READ_INTERVAL;                  // 30 seconds interval between reads in idle mode
  const unsigned long sensorStabilizationTime = SENSOR_STABILIZATION_TIME;  // 300 ms stabilization time

  // If no mode is specified, use the previous mode
  if (mode == 0) {
    mode = previousMode;
  } else if (mode != previousMode) {
    // Reset the state machine if the mode has changed
    state = 0;
    previousMode = mode;
    // firstRun = true; // Ensure the first run flag is set if the mode changes
  }

  if (mode == 1) {  // Idle mode
    Serial.print("MODE 1 SOIL MOISTURE READ with state ");
    Serial.println(state);

    switch (state) {
      case 0:  // Idle state
        if (firstRun || millis() - lastReadTime >= readInterval) {

          Serial.println("Outside the interval! Turning it on...");
          Serial.print("Firstrun was...");
          Serial.println(firstRun);

          digitalWrite(SOIL_MOISTURE_SENSOR_POWER, HIGH);  // Turn on the sensor
          stateEntryTime = millis();                       // Record the entry time
          state = 1;                                       // Move to the next state
          firstRun = false;                                // Clear the first run flag
        } else {
          digitalWrite(SOIL_MOISTURE_SENSOR_POWER, LOW);
        }
        break;
      case 1:  // Wait for stabilization
        Serial.println("Stabilising...");

        if (millis() - stateEntryTime >= sensorStabilizationTime) {
         
          Serial.println("Making an ACTIAL read, since it's stable!");

          lastSensorValue = analogRead(SOIL_MOISTURE_SENSOR);  // Read the sensor value
          Serial.print("Reading is: ");
          Serial.println(lastSensorValue);

          stateEntryTime = millis();  // Record the entry time
          state = 2;                  // Move to the next state
        } else {
          Serial.println("Not yet stable...");
        }
        break;
      case 2:  // Turn off the sensor
        if (millis() - stateEntryTime >= sensorStabilizationTime) {
          Serial.println("Turning it off...");

          digitalWrite(SOIL_MOISTURE_SENSOR_POWER, LOW);  // Turn off the sensor
          lastReadTime = millis();                        // Update the last read time
          state = 0;                                      // Move back to idle state
        }
        break;
        // default:
        // Serial.println("WTF...");
    }
  } else if (mode == 2) {  // Feeding mode
    Serial.println("MODE 2 READ");
    switch (state) {
      case 0:                                            // Initial state
        digitalWrite(SOIL_MOISTURE_SENSOR_POWER, HIGH);  // Ensure the sensor is on
        state = 1;                                       // Move to the reading state
        break;
      case 1:                                                // Continuously read the sensor value
        lastSensorValue = analogRead(SOIL_MOISTURE_SENSOR);  // Read the sensor value
        break;
    }
  }

  return lastSensorValue;
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

