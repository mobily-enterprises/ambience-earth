#include <Arduino.h>

#define SOIL_LITTLE_MOISTURE_SENSOR A0
#define TRAY_WATER_LEVEL_SENSOR A1
#define TRAY_FULL_SENSOR 6

void initSensors();
uint16_t senseSoilMosture();
uint8_t soilMoistureAsPercentage(uint16_t soilMosture);
uint8_t trayWaterLevelAsPercentage(uint16_t trayWaterLevel);
char* soilMoistureInEnglish(uint8_t soilMostureAsPercentage);
uint16_t senseTrayWaterLevel();
bool senseTrayIsFull();
char* trayWaterLevelInEnglish(uint8_t trayWaterLevelAsPercentage, bool trayIsFull);
uint8_t soilMoistureAsState(uint8_t soilMoistureAsPercentage);
uint8_t trayWaterLevelAsState(uint8_t trayWaterLevelAsPercentage);