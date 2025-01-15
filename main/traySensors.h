#ifndef TRAY_SENSORS_H
#define TRAY_SENSORS_H

#include <Arduino.h>

#define TRAY_SENSOR_HIGH 7
#define TRAY_SENSOR_MID 7  // CHANGE IT TO PIN WHEN SENSOR IS THERE. TILL THEN, SAME AS HIGH
#define TRAY_SENSOR_LOW 5

void initTraySensors();

bool senseTrayWaterLevelLow();
bool senseTrayWaterLevelMid();
bool senseTrayWaterLevelHigh();

char* trayWaterLevelInEnglish(uint8_t trayState);
char* trayWaterLevelInEnglishShort(uint8_t trayState);
uint8_t trayWaterLevelAsState();
#endif TRAY_SENSORS_H