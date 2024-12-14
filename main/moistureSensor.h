#ifndef MOISTURE_SENSORS_H
#define MOISTURE_SENSORS_H

#include <Arduino.h>

#define SOIL_MOISTURE_SENSOR A0
#define SOIL_MOISTURE_SENSOR_POWER 3

#define SENSOR_READ_INTERVAL 10000
#define SENSOR_STABILIZATION_TIME 300

void initMoistureSensor();
uint16_t senseSoilMoisture(uint8_t mode = 0);
uint8_t soilMoistureAsPercentage(uint16_t soilMosture);
uint8_t soilMoistureAsState(uint8_t soilMoistureAsPercentage);
#endif MOISTURE_SENSORS_H