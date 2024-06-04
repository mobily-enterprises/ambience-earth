
#include "config.h"
#include "sensors.h"

#include <Arduino.h>

extern Config config;

void initSensors() {
  pinMode(TRAY_WATER_LEVEL_SENSOR_POWER, OUTPUT);
  pinMode(SOIL_MOISTURE_SENSOR_POWER, OUTPUT);
  
  pinMode(SOIL_MOISTURE_SENSOR, INPUT);
  pinMode(TRAY_WATER_LEVEL_SENSOR, INPUT);
  
  digitalWrite(TRAY_WATER_LEVEL_SENSOR_POWER, HIGH);
  digitalWrite(SOIL_MOISTURE_SENSOR_POWER, HIGH);

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

/*
uint16_t senseSoilMoisture(bool immediate = false) {
    const uint8_t NUM_READINGS = 5;    // Number of readings to average
    static uint32_t total = 0;         // Sum of the readings
    static uint16_t average = 0;       // Average of the readings

    if (immediate) return analogRead(SOIL_MOISTURE_SENSOR);

    // Subtract the oldest reading from the total
    total -= total / NUM_READINGS;

    // Read a new value
    // analogRead(SOIL_MOISTURE_SENSOR);
    // delay(100);
    total += analogRead(SOIL_MOISTURE_SENSOR);

    // Calculate the average
    average = total / NUM_READINGS;

    return average;
}
*/

uint16_t senseSoilMoisture(uint8_t mode = 0) {
  static uint8_t state = 0; // Initial state
  static unsigned long lastReadTime = 0; // Last time the sensor was read
  static uint16_t lastSensorValue = 0; // Last read sensor value
  static unsigned long stateEntryTime = 0; // Time when the state was entered
  static uint8_t previousMode = 1; // Default to idle mode if no mode is specified
  static bool firstRun = true; // Flag to check if this is the first run

  const unsigned long readInterval = SENSOR_READ_INTERVAL; // 30 seconds interval between reads in idle mode
  const unsigned long sensorStabilizationTime = SENSOR_STABILIZATION_TIME; // 300 ms stabilization time


  // If no mode is specified, use the previous mode
  if (mode == 0) {
    mode = previousMode;
  } else if (mode != previousMode) {
    // Reset the state machine if the mode has changed
    state = 0;
    previousMode = mode;
    // firstRun = true; // Ensure the first run flag is set if the mode changes
  }

  if (mode == 1) { // Idle mode
    Serial.print("MODE 1 SOIL MOISTURE READ with state ");
    Serial.println(state);
    
    switch (state) {
      case 0: // Idle state
        if (firstRun || millis() - lastReadTime >= readInterval) {
          
          // Serial.println("Outside the interval! Turning it on...");
          // Serial.println(firstRun);
   
          digitalWrite(SOIL_MOISTURE_SENSOR_POWER, HIGH); // Turn on the sensor
          stateEntryTime = millis(); // Record the entry time
          state = 1; // Move to the next state
          firstRun = false; // Clear the first run flag
        } else {
          digitalWrite(SOIL_MOISTURE_SENSOR_POWER, LOW);
        }
        break;
      case 1: // Wait for stabilization
        // Serial.println("Stabilising...");

        if (millis() - stateEntryTime >= sensorStabilizationTime) {
          // analogRead(TRAY_WATER_LEVEL_SENSOR);
          // delay(100);
          
          // Serial.println("Making an ACTIAL read, since it's stable!");
   
          lastSensorValue = analogRead(SOIL_MOISTURE_SENSOR); // Read the sensor value
          stateEntryTime = millis(); // Record the entry time
          state = 2; // Move to the next state
        }
        break;
      case 2: // Turn off the sensor
        if (millis() - stateEntryTime >= sensorStabilizationTime) {
          // Serial.println("Turning it off...");
   
          digitalWrite(SOIL_MOISTURE_SENSOR_POWER, LOW); // Turn off the sensor
          lastReadTime = millis(); // Update the last read time
          state = 0; // Move back to idle state
        }
        break;
      // default:
        // Serial.println("WTF...");

    }
  } else if (mode == 2) { // Feeding mode
    // Serial.println("MODE 2 READ");
    switch (state) {
      case 0: // Initial state
        digitalWrite(SOIL_MOISTURE_SENSOR_POWER, HIGH); // Ensure the sensor is on
        state = 1; // Move to the reading state
        break;
      case 1: // Continuously read the sensor value
        lastSensorValue = analogRead(SOIL_MOISTURE_SENSOR); // Read the sensor value
        break;
    }
  }

  return lastSensorValue;
}




uint16_t senseTrayWaterLevel(uint8_t mode = 0) {
  static uint8_t state = 0; // Initial state
  static unsigned long lastReadTime = 0; // Last time the sensor was read
  static uint16_t lastSensorValue = 0; // Last read sensor value
  static unsigned long stateEntryTime = 0; // Time when the state was entered
  static uint8_t previousMode = 1; // Default to idle mode if no mode is specified
  static bool firstRun = true; // Flag to check if this is the first run

  const unsigned long readInterval = SENSOR_READ_INTERVAL; // 30 seconds interval between reads in idle mode
  const unsigned long sensorStabilizationTime = SENSOR_STABILIZATION_TIME; // 300 ms stabilization time


  // If no mode is specified, use the previous mode
  if (mode == 0) {
    mode = previousMode;
  } else if (mode != previousMode) {
    // Reset the state machine if the mode has changed
    state = 0;
    previousMode = mode;
    // firstRun = true; // Ensure the first run flag is set if the mode changes
  }

  if (mode == 1) { // Idle mode
    Serial.print("MODE 1 TRAY WATER SENSOR READ with state ");
    Serial.println(state);
    
    switch (state) {
      case 0: // Idle state
        if (firstRun || millis() - lastReadTime >= readInterval) {
          
          // Serial.println("Outside the interval! Turning it on...");
          // Serial.println(firstRun);
   
          digitalWrite(TRAY_WATER_LEVEL_SENSOR_POWER, HIGH); // Turn on the sensor
          stateEntryTime = millis(); // Record the entry time
          state = 1; // Move to the next state
          firstRun = false; // Clear the first run flag
        } else {
          digitalWrite(TRAY_WATER_LEVEL_SENSOR_POWER, LOW);
        }
        break;
      case 1: // Wait for stabilization
        // Serial.println("Stabilising...");

        if (millis() - stateEntryTime >= sensorStabilizationTime) {
          // analogRead(TRAY_WATER_LEVEL_SENSOR);
          // delay(100);
          
          // Serial.println("Making an ACTIAL read, since it's stable!");
   
          lastSensorValue = analogRead(TRAY_WATER_LEVEL_SENSOR); // Read the sensor value
          stateEntryTime = millis(); // Record the entry time
          state = 2; // Move to the next state
        }
        break;
      case 2: // Turn off the sensor
        if (millis() - stateEntryTime >= sensorStabilizationTime) {
          // Serial.println("Turning it off...");
   
          digitalWrite(TRAY_WATER_LEVEL_SENSOR_POWER, LOW); // Turn off the sensor
          lastReadTime = millis(); // Update the last read time
          state = 0; // Move back to idle state
        }
        break;
      // default:
        // Serial.println("WTF...");

    }
  } else if (mode == 2) { // Feeding mode
    // Serial.println("MODE 2 READ");
    switch (state) {
      case 0: // Initial state
        digitalWrite(TRAY_WATER_LEVEL_SENSOR_POWER, HIGH); // Ensure the sensor is on
        state = 1; // Move to the reading state
        break;
      case 1: // Continuously read the sensor value
        lastSensorValue = analogRead(TRAY_WATER_LEVEL_SENSOR); // Read the sensor value
        break;
    }
  }

  return lastSensorValue;
}


bool senseTrayIsFull() {
  return !digitalRead(TRAY_FULL_SENSOR);
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
  uint16_t quarter = config.trayWaterLevelSensorCalibrationQuarter;
  uint16_t half = config.trayWaterLevelSensorCalibrationHalf;
  uint16_t threeQuarters = config.trayWaterLevelSensorCalibrationThreeQuarters;

  if (waterLevel <= quarter) {
    // Interpolate between 0% and 25%
    return (waterLevel * 25) / quarter;
  } else if (waterLevel <= half) {
    // Interpolate between 25% and 50%
    return 25 + ((waterLevel - quarter) * 25) / (half - quarter);
  } else if (waterLevel <= threeQuarters) {
    // Interpolate between 50% and 75%
    return 50 + ((waterLevel - half) * 25) / (threeQuarters - half);
  } else {
    // Interpolate between 75% and 100%
    // Assuming the range from threeQuarters to full is the same as the range from half to threeQuarters
    uint16_t fullEstimate = threeQuarters + (threeQuarters - half);
    if (waterLevel >= fullEstimate) {
      return 100;  // Cap the value at 100%
    } else {
      return 75 + ((waterLevel - threeQuarters) * 25) / (fullEstimate - threeQuarters);
    }
  }
}


uint8_t trayWaterLevelAsState(uint8_t percentage, bool isFull) {
  uint8_t v = percentage;

  // The "isFull" sensor will force a "plenty" state
  if (isFull) return (uint8_t) Conditions::TRAY_FULL;;

  if (v > 50) return (uint8_t) Conditions::TRAY_PLENTY;
  else if(v > 2) return (uint8_t) Conditions::TRAY_SOME;
  else if(v > 0) return (uint8_t) Conditions::TRAY_LITTLE;
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
  else if (trayState == Conditions::TRAY_LITTLE) return MSG_TRAY_LITTLE;
  else if (trayState == Conditions::TRAY_DRY) return MSG_TRAY_DRY;
}