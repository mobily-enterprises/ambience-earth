#ifndef ROOT_TEMP_H
#define ROOT_TEMP_H

#include <Arduino.h>

// Data pin for the root temperature probe (DS18B20).
// Adjust if you wire the sensor elsewhere.
#define ROOT_TEMP_PIN 6

// Initialize the bus pin.
void initRootTempSensor();

// Non-blocking read of the DS18B20. Kick off convert/read in poll; use cache getter.
void rootTempPoll();
bool rootTempCachedC10(int16_t *outC10);
#define rootTempReadC10 rootTempCachedC10

#endif // ROOT_TEMP_H
