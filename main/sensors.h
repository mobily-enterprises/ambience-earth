#include <Arduino.h>

#define SOIL_MOISTURE_SENSOR A0
#define TRAY_WATER_LEVEL_SENSOR A1
#define TRAY_FULL_SENSOR 2

void initSensors();
uint16_t senseSoilMosture();
uint8_t soilMoistureAsPercentage(uint16_t soilMosture);
char* soilMoistureInEnglish(uint8_t soilMostureAsPercentage);
uint16_t senseTrayWaterLevel();
bool senseTrayIsFull();
char* trayWaterLevelInEnglish(uint16_t trayWaterLevel, bool trayIsFull);
      