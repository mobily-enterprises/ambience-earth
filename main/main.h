#ifndef MAIN_H
#define MAIN_H

#include "config.h"

#define SCREENSAVER_TRIGGER_TIME 60000
#define LOG_VALUES_INTERVAL 3600000 // Every hour
// #define LOG_VALUES_INTERVAL 10000


void maybeLogValues();

void initialPinSetup();
void mainMenu();
int runInitialSetup();
void displayInfo(uint8_t screen);
void initialaverageMsBetweenFeeds();
void lcdPrintTime(unsigned long milliseconds);
void screenSaverModeOff();
void screenSaverModeOn();
#endif MAIN_H
