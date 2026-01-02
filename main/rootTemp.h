#ifndef ROOT_TEMP_H
#define ROOT_TEMP_H

#include <Arduino.h>

// Data pin for the root temperature probe (DS18B20).
// Adjust if you wire the sensor elsewhere.
#define ROOT_TEMP_PIN 6

// Initialize the bus pin.
void initRootTempSensor();

// Blocking read of the DS18B20. Returns temperature in °C * 10 (e.g., 231 = 23.1°C).
// Returns false if the sensor does not respond.
bool rootTempReadC10(int16_t *outC10);

#endif // ROOT_TEMP_H
