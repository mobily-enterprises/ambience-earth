#ifndef RUNOFF_SENSOR_H
#define RUNOFF_SENSOR_H

#include <Arduino.h>

#define RUNOFF_SENSOR_PIN 5

void initRunoffSensor();
bool runoffDetected();

#endif RUNOFF_SENSOR_H
