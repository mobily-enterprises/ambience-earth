#ifndef PUMPS_H
#define PUMPS_H

#include <Arduino.h>

#define PUMP_IN_DEVICE 2
#define PUMP_OUT_DEVICE 3

void initPumps();

void openLineIn();
void closeLineIn();
void openLineOut();
void closeLineOut();
#endif PUMPS_H