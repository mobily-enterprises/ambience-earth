
#include "config.h"

#include "hardwareConf.h"
#include "traySensors.h"

#include <Arduino.h>

extern Config config;

void initTraySensors() {
  pinMode(TRAY_SENSOR_HIGH, INPUT_PULLUP);
  pinMode(TRAY_SENSOR_MID, INPUT_PULLUP);
  pinMode(TRAY_SENSOR_LOW, INPUT_PULLUP);
}

bool senseTrayWaterLevelLow() {
  return !digitalRead(TRAY_SENSOR_LOW);
}

bool senseTrayWaterLevelMid() {
  return !digitalRead(TRAY_SENSOR_MID);
}

bool senseTrayWaterLevelHigh() {
  return !digitalRead(TRAY_SENSOR_HIGH);
}

uint8_t trayWaterLevelAsState() {
  bool low = senseTrayWaterLevelLow();
  bool mid = senseTrayWaterLevelMid;
  bool high = senseTrayWaterLevelHigh();

  if (high) return Conditions::TRAY_FULL;
  else if (mid) return Conditions::TRAY_MIDDLE;
  else if (low) return Conditions::TRAY_LITTLE;
  else return Conditions::TRAY_DRY;
}

char* trayWaterLevelInEnglish(uint8_t trayState) {
  if (trayState == Conditions::TRAY_FULL) return MSG_TRAY_FULL;
  else if (trayState == Conditions::TRAY_MIDDLE) return MSG_TRAY_MIDDLE;
  else if (trayState == Conditions::TRAY_LITTLE) return MSG_TRAY_LITTLE;
  else if (trayState == Conditions::TRAY_DRY) return MSG_TRAY_DRY;
  else return MSG_TRAY_UNSTATED;
}

char* trayWaterLevelInEnglishShort(uint8_t trayState) {
  if (trayState == Conditions::TRAY_FULL) return MSG_TRAY_FULL_SHORT;
  else if (trayState == Conditions::TRAY_MIDDLE) return MSG_TRAY_MIDDLE_SHORT;
  else if (trayState == Conditions::TRAY_LITTLE) return MSG_TRAY_LITTLE_SHORT;
  else if (trayState == Conditions::TRAY_DRY) return MSG_TRAY_DRY_SHORT;
  else return MSG_TRAY_UNSTATED_SHORT;
}