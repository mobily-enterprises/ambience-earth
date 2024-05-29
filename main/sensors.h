#ifndef SENSORS_H
#define SENSORS_H

#include <Arduino.h>

#define SOIL_MOISTURE_SENSOR A0
#define TRAY_WATER_LEVEL_SENSOR A2
#define TRAY_FULL_SENSOR 12

#define SOLENOID_IN_DEVICE 10
#define PUMP_IN_DEVICE 7
#define PUMP_OUT_DEVICE 2


void initSensors();
uint16_t senseSoilMoisture();
uint8_t soilMoistureAsPercentage(uint16_t soilMosture);
uint8_t trayWaterLevelAsPercentage(uint16_t trayWaterLevel);
char* soilMoistureInEnglish(uint8_t soilMostureAsPercentage);
uint16_t senseTrayWaterLevel();
bool senseTrayIsFull();
char* trayWaterLevelInEnglish(uint8_t trayWaterLevelAsPercentage, bool trayIsFull);
uint8_t soilMoistureAsState(uint8_t soilMoistureAsPercentage);
uint8_t trayWaterLevelAsState(uint8_t percentage, bool isFull);
void openLineIn();
void closeLineIn();
void openPumpOut();
void closePumpOut();
#endif SENSORS_H