#pragma once

#include <Arduino.h>

#define PUMP_IN_DEVICE 9

/*
 * initPumps
 * Configures the pump output pin.
 * Example:
 *   initPumps();
 */
void initPumps();

/*
 * openLineIn
 * Turns the pump output on (active-low).
 * Example:
 *   openLineIn();
 */
void openLineIn();

/*
 * closeLineIn
 * Turns the pump output off (active-low).
 * Example:
 *   closeLineIn();
 */
void closeLineIn();
