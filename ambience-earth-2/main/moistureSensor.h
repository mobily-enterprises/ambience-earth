#pragma once

#include <Arduino.h>

#define SOIL_MOISTURE_SENSOR_PIN 1
#define SOIL_MOISTURE_SENSOR_POWER 3

#define SENSOR_SLEEP_INTERVAL 30000UL
#define SENSOR_WINDOW_DURATION 30000UL
#define SENSOR_CAL_WINDOW_DURATION 15000UL
#define SENSOR_SAMPLE_INTERVAL 250UL
#define SENSOR_STABILIZATION_TIME 2000UL
#define SENSOR_FEED_TAU_FAST_MS 700UL
#define SENSOR_FEED_TAU_SLOW_MS 7000UL

struct SoilSensorWindowStats {
  uint16_t minRaw;
  uint16_t maxRaw;
  uint16_t avgRaw;
  uint16_t count;
};

/*
 * readMedian3FromPin
 * Reads three samples and returns the median value.
 * Example:
 *   uint16_t raw = readMedian3FromPin(SOIL_MOISTURE_SENSOR_PIN);
 */
uint16_t readMedian3FromPin(uint8_t pin);

/*
 * setSoilSensorLazy
 * Switches the soil sensor to low-power windowed sampling mode.
 * Example:
 *   setSoilSensorLazy();
 */
void setSoilSensorLazy();

/*
 * setSoilSensorLazySeed
 * Seeds the lazy mode with an initial raw value.
 * Example:
 *   setSoilSensorLazySeed(lastRaw);
 */
void setSoilSensorLazySeed(uint16_t seed);

/*
 * setSoilSensorRealTime
 * Switches the soil sensor to real-time sampling mode.
 * Example:
 *   setSoilSensorRealTime();
 */
void setSoilSensorRealTime();

/*
 * getSoilMoisture
 * Returns the current soil moisture raw value.
 * Example:
 *   uint16_t raw = getSoilMoisture();
 */
uint16_t getSoilMoisture();

/*
 * runSoilSensorLazyReadings
 * Advances the lazy sampling state machine and returns the latest value.
 * Example:
 *   runSoilSensorLazyReadings();
 */
uint16_t runSoilSensorLazyReadings();

/*
 * soilSensorIsActive
 * Returns true when the sensor power pin is enabled.
 * Example:
 *   if (soilSensorIsActive()) { ... }
 */
bool soilSensorIsActive();

/*
 * soilSensorRealtimeReady
 * Returns true once the real-time sensor warmup has completed.
 * Example:
 *   if (soilSensorRealtimeReady()) { ... }
 */
bool soilSensorRealtimeReady();

/*
 * soilSensorReady
 * Returns true if any valid soil reading has been captured.
 * Example:
 *   if (soilSensorReady()) { ... }
 */
bool soilSensorReady();

/*
 * soilSensorGetRealtimeAvg
 * Returns the filtered real-time average.
 * Example:
 *   uint16_t avg = soilSensorGetRealtimeAvg();
 */
uint16_t soilSensorGetRealtimeAvg();

/*
 * soilSensorGetRealtimeRaw
 * Returns the last raw real-time sample.
 * Example:
 *   uint16_t raw = soilSensorGetRealtimeRaw();
 */
uint16_t soilSensorGetRealtimeRaw();

/*
 * soilSensorOp
 * Internal multiplexer for sensor operations used by other helpers.
 * Example:
 *   uint16_t value = soilSensorOp(1);
 */
uint16_t soilSensorOp(uint8_t op);

/*
 * soilSensorWindowStart
 * Starts a calibration sampling window.
 * Example:
 *   soilSensorWindowStart();
 */
void soilSensorWindowStart();

/*
 * soilSensorWindowTick
 * Updates the calibration window and fills stats when complete.
 * Example:
 *   SoilSensorWindowStats stats = {};
 *   if (soilSensorWindowTick(&stats)) { ... }
 */
bool soilSensorWindowTick(SoilSensorWindowStats *out);

/*
 * soilSensorWindowLastRaw
 * Returns the most recent raw sample from the window.
 * Example:
 *   uint16_t raw = soilSensorWindowLastRaw();
 */
uint16_t soilSensorWindowLastRaw();

/*
 * soilSensorLastWindowEndAt
 * Returns the millis timestamp when the last window finished.
 * Example:
 *   unsigned long t = soilSensorLastWindowEndAt();
 */
unsigned long soilSensorLastWindowEndAt();

/*
 * soilSensorCalWindowRemainingMs
 * Returns remaining milliseconds in the calibration averaging window.
 * Example:
 *   uint32_t remaining = soilSensorCalWindowRemainingMs();
 */
uint32_t soilSensorCalWindowRemainingMs();

/*
 * initMoistureSensor
 * Configures the moisture sensor pins and state machine.
 * Example:
 *   initMoistureSensor();
 */
void initMoistureSensor();

/*
 * soilMoistureAsPercentage
 * Converts a raw soil reading into a percentage using calibration.
 * Example:
 *   uint8_t pct = soilMoistureAsPercentage(raw);
 */
uint8_t soilMoistureAsPercentage(uint16_t soilMosture);
