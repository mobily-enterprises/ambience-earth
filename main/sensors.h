#ifndef SENSORS_H
#define SENSORS_H

#include <Arduino.h>

#define SOIL_MOISTURE_SENSOR A0
#define SOIL_MOISTURE_SENSOR_POWER 3

#define TRAY_WATER_LEVEL_SENSOR A2
#define TRAY_WATER_LEVEL_SENSOR_POWER 5
#define TRAY_FULL_SENSOR 12

#define SOLENOID_IN_DEVICE 10
#define PUMP_IN_DEVICE 7
#define PUMP_OUT_DEVICE 2

#define SENSOR_READ_INTERVAL 30000
#define SENSOR_STABILIZATION_TIME 300

const unsigned long readInterval = 5000; // 30 seconds interval between reads in idle mode
  const unsigned long sensorStabilizationTime = 300; // 30


void initSensors();
uint16_t senseSoilMoisture(uint8_t mode = 0);
uint8_t soilMoistureAsPercentage(uint16_t soilMosture);
uint8_t trayWaterLevelAsPercentage(uint16_t trayWaterLevel);
char* soilMoistureInEnglish(uint8_t soilMostureAsPercentage);
uint16_t senseTrayWaterLevel(uint8_t mode = 0);
bool senseTrayIsFull();
char* trayWaterLevelInEnglish(uint8_t trayWaterLevelAsPercentage, bool trayIsFull);
uint8_t soilMoistureAsState(uint8_t soilMoistureAsPercentage);
uint8_t trayWaterLevelAsState(uint8_t percentage, bool isFull);
void openLineIn();
void closeLineIn();
void openPumpOut();
void closePumpOut();
#endif SENSORS_H