#ifndef MOISTURE_SENSORS_H
#define MOISTURE_SENSORS_H

#include <Arduino.h>

#define SOIL_MOISTURE_SENSOR A0
#define SOIL_MOISTURE_SENSOR_POWER 12

#define SENSOR_SLEEP_INTERVAL 60000UL
#define SENSOR_WINDOW_DURATION 30000UL
#define SENSOR_SAMPLE_INTERVAL 500UL
#define SENSOR_STABILIZATION_TIME 2000UL
#define SENSOR_WINDOW_MAX_SAMPLES ((SENSOR_WINDOW_DURATION / SENSOR_SAMPLE_INTERVAL) + 1)


void setSoilSensorLazy();
void setSoilSensorRealTime();
uint16_t getSoilMoisture();
uint16_t runSoilSensorLazyReadings();
bool soilSensorIsActive();
uint8_t soilSensorGetLastWindowSampleCount();
uint8_t soilSensorGetLastWindowSample(uint8_t index);
uint8_t soilSensorGetLastWindowMinPercent();
uint8_t soilSensorGetLastWindowMaxPercent();
uint8_t soilSensorGetLastWindowAvgPercent();

uint16_t soilSensorOp(uint8_t op);

void initMoistureSensor();
uint8_t soilMoistureAsPercentage(uint16_t soilMosture);
#endif MOISTURE_SENSORS_H
