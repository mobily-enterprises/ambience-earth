#ifndef MOISTURE_SENSORS_H
#define MOISTURE_SENSORS_H

#include <Arduino.h>

#define SOIL_MOISTURE_SENSOR A0
#define SOIL_MOISTURE_SENSOR_POWER 12

#define SENSOR_SLEEP_INTERVAL 60000UL
#define SENSOR_WINDOW_DURATION 30000UL
#define SENSOR_SAMPLE_INTERVAL 250UL
#define SENSOR_STABILIZATION_TIME 2000UL
#define SENSOR_FEED_TAU_FAST_MS 700UL
#define SENSOR_FEED_TAU_SLOW_MS 7000UL
#define SENSOR_FEED_DEADBAND_PERCENT 7
#define SENSOR_FEED_RISE_MS 2000UL
#define SENSOR_FEED_DROP_MS 1000UL
#define SENSOR_WINDOW_MIN_DURATION 10000UL
#define SENSOR_EXTREME_RANGE_MIN 4
#define SENSOR_EXTREME_HYSTERESIS 1
#define SENSOR_EXTREME_HOLD_SAMPLES 4


/*
  FEEDING MODE API (USE THIS IN FEED LOGIC):
  - On feed start: call setSoilSensorRealTime() to power the sensor and seed the EMA.
  - During feeding: call getSoilMoisture() for the weighted average (EMA).
  - To ignore warm-up: call soilSensorRealtimeReady() before trusting values.
  - Optional raw reads: soilSensorGetRealtimeRaw().
  - On feed end: call setSoilSensorLazy() to resume windowed long-term sampling.
*/
void setSoilSensorLazy();
void setSoilSensorLazySeed(uint16_t seed);
void setSoilSensorRealTime();
uint16_t getSoilMoisture();
uint16_t runSoilSensorLazyReadings();
bool soilSensorIsActive();
bool soilSensorRealtimeReady();
uint16_t soilSensorGetRealtimeAvg();
uint16_t soilSensorGetRealtimeRaw();
uint16_t soilSensorOp(uint8_t op);

void initMoistureSensor();
uint8_t soilMoistureAsPercentage(uint16_t soilMosture);
#endif MOISTURE_SENSORS_H
