#include "pumps.h"

/*
 * initPumps
 * Configures the pump output pin.
 * Example:
 *   initPumps();
 */
void initPumps() {
  pinMode(PUMP_IN_DEVICE, OUTPUT);
  digitalWrite(PUMP_IN_DEVICE, HIGH);
}

/*
 * openLineIn
 * Turns the pump output on (active-low).
 * Example:
 *   openLineIn();
 */
void openLineIn() {
  digitalWrite(PUMP_IN_DEVICE, LOW);
}

/*
 * closeLineIn
 * Turns the pump output off (active-low).
 * Example:
 *   closeLineIn();
 */
void closeLineIn() {
  digitalWrite(PUMP_IN_DEVICE, HIGH);
}
