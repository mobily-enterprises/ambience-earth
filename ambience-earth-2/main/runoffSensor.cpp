#include "runoffSensor.h"

/*
 * initRunoffSensor
 * Initializes the runoff sensor input with pull-up.
 * Example:
 *   initRunoffSensor();
 */
void initRunoffSensor() {
  pinMode(RUNOFF_SENSOR_PIN, INPUT_PULLUP);
}

/*
 * runoffDetected
 * Returns true when the runoff sensor is active (active-low).
 * Example:
 *   if (runoffDetected()) { ... }
 */
bool runoffDetected() {
  return !digitalRead(RUNOFF_SENSOR_PIN);
}
