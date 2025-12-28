
#include "config.h"
#include "hardwareConf.h"
#include "messages.h"
#include "traySensors.h"

#include <Arduino.h>

extern Config config;

void initTraySensors() {
  pinMode(TRAY_SENSOR_LOW, INPUT_PULLUP);
}

bool senseTrayWaterLevelLow() {
  return !digitalRead(TRAY_SENSOR_LOW);
}

uint8_t trayWaterLevelAsState() {
  bool low = senseTrayWaterLevelLow();
  if (low) return TRAY_LITTLE;
  return TRAY_DRY;
}

PGM_P trayWaterLevelInEnglish(uint8_t trayState) {
  if (trayState == TRAY_LITTLE) return MSG_TRAY_LITTLE;
  else if (trayState == TRAY_DRY) return MSG_TRAY_DRY;
  else return MSG_TRAY_UNSTATED;
}

PGM_P trayWaterLevelInEnglishShort(uint8_t trayState) {
  if (trayState == TRAY_LITTLE) return MSG_TRAY_LITTLE_SHORT;
  else if (trayState == TRAY_DRY) return MSG_TRAY_DRY_SHORT;
  else return MSG_TRAY_UNSTATED_SHORT;
}
