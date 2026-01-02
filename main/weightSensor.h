#ifndef WEIGHT_SENSOR_H
#define WEIGHT_SENSOR_H

#include <Arduino.h>

// HX711 pins (adjust if needed)
#define WEIGHT_DOUT_PIN 3
#define WEIGHT_SCK_PIN 4

// Initialize the HX711 pins.
void initWeightSensor();

// Tare the scale (averages a few raw readings).
void weightSensorTare(uint8_t samples = 8);

// Set scale factor: units per raw count (e.g., grams per count).
void weightSensorSetScale(float unitsPerCount);

// Read weight into outUnits (same units as scale factor). Returns false if not ready.
bool weightSensorRead(float *outUnits, uint8_t samples = 4);

// Cached read helper for background polling.
void weightSensorPoll();
bool weightSensorReady();
float weightSensorLastValue();

#endif // WEIGHT_SENSOR_H
