#include "runoffSensor.h"

void initRunoffSensor() {
  pinMode(RUNOFF_SENSOR_PIN, INPUT_PULLUP);
}

bool runoffDetected() {
  return !digitalRead(RUNOFF_SENSOR_PIN);
}
