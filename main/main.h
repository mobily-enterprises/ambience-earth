#ifndef MAIN_H
#define MAIN_H

#include "config.h"

typedef struct {
  unsigned int seq : 8;
  unsigned long millisStart;
  unsigned long millisEnd;
  unsigned int entryType : 3;
  unsigned int actionId : 3;
  unsigned int trayWaterLevelBefore : 7;
  unsigned int soilMoistureBefore : 7;
  bool topFeed : 1;
  unsigned int outcome : 4;
  unsigned int trayWaterLevelAfter : 7;
  unsigned int soilMoistureAfter : 7;
  unsigned int padding1 : 11;
} LogEntry;

enum PumpState { IDLE, PUMPING, COMPLETED };

void runAction(Action *action, uint8_t index, bool force = 0);
void mainMenu();
int runInitialSetup();
void displayInfo(uint8_t screen);
void runAction(Action *action, uint8_t index, bool force = 0);
void updateAverageTimeBetweenFeed();
void initialAverageTimeBetweenFeeds();
#endif MAIN_H