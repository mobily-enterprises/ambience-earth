#ifndef MOISTURE_SENSORS_H
#define MOISTURE_SENSORS_H

#include <Arduino.h>

#define SOIL_MOISTURE_SENSOR A0
#define SOIL_MOISTURE_SENSOR_POWER 12

#define SENSOR_READ_INTERVAL 10000
#define SENSOR_STABILIZATION_TIME 500


void setSoilSensorLazy();
void setSoilSensorRealTime();
uint16_t getSoilMoisture();
uint16_t runSoilSensorLazyReadings();

uint16_t soilSensorOp(uint8_t op);

void initMoistureSensor();
uint8_t soilMoistureAsPercentage(uint16_t soilMosture);
#endif MOISTURE_SENSORS_H
