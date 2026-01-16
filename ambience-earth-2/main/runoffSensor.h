#pragma once

#include <Arduino.h>

#define RUNOFF_SENSOR_PIN 7

/*
 * initRunoffSensor
 * Initializes the runoff sensor input with pull-up.
 * Example:
 *   initRunoffSensor();
 */
void initRunoffSensor();

/*
 * runoffDetected
 * Returns true when the runoff sensor is active (active-low).
 * Example:
 *   if (runoffDetected()) { ... }
 */
bool runoffDetected();
