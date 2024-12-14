#ifndef MAIN_H
#define MAIN_H

#include "config.h"

enum PumpState { IDLE, PUMPING, COMPLETED };

#define SCREENSAVER_TRIGGER_TIME 60000
#define LOG_VALUES_INTERVAL 3600000
// #define LOG_VALUES_INTERVAL 10000


void maybeLogValues();

void runAction(Action *action, uint8_t index, bool force = 0);
void initialPinSetup();
void mainMenu();
int runInitialSetup();
void displayInfo(uint8_t screen);
void runAction(Action *action, uint8_t index, bool force = 0);
void updateAverageTimeBetweenFeed();
void initialaverageMsBetweenFeeds();
void lcdPrintTime(unsigned long milliseconds);
void screenSaverModeOff();
void screenSaverModeOn();
#endif MAIN_H