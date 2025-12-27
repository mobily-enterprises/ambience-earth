
#include "config.h"

#include "hardwareConf.h"
#include "pumps.h"

#include <Arduino.h>

extern Config config;

void initPumps() {
  pinMode(PUMP_IN_DEVICE, OUTPUT);
  digitalWrite(PUMP_IN_DEVICE, HIGH);
}

void openLineIn() {
  digitalWrite(PUMP_IN_DEVICE, LOW);
}

void closeLineIn() {
  digitalWrite(PUMP_IN_DEVICE, HIGH);
}
