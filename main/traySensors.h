#ifndef TRAY_SENSORS_H
#define TRAY_SENSORS_H

#include <Arduino.h>
#include <avr/pgmspace.h>

#define TRAY_SENSOR_LOW 5

enum TrayState : uint8_t {
  TRAY_DRY = 0,
  TRAY_LITTLE = 1,
};

void initTraySensors();

bool senseTrayWaterLevelLow();

PGM_P trayWaterLevelInEnglish(uint8_t trayState);
PGM_P trayWaterLevelInEnglishShort(uint8_t trayState);
uint8_t trayWaterLevelAsState();
#endif TRAY_SENSORS_H
