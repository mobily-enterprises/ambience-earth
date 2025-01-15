#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>
#include <Arduino.h>
#include "ui.h"

#define CONFIG_ADDRESS 0

#define ACTIONS_ARRAY_SIZE 5
// ************************************************************
// ** TYPE DEFINITIONS
// ************************************************************

typedef struct {
  enum : int8_t { P1, S1 } what;
} WaterSource;

enum FeedFrom : int8_t {
  FEED_FROM_TOP,
  FEED_FROM_TRAY
};

typedef struct {
  enum : int8_t { TRAY_IGNORED, TRAY_DRY, TRAY_LITTLE, TRAY_MIDDLE, TRAY_FULL } tray;
  int8_t soil;
  enum : int8_t { NO_LOGIC, TRAY_OR_SOIL, TRAY_AND_SOIL } logic;
} Conditions;

typedef struct {
  char name[LABEL_LENGTH + 1];
  Conditions triggerConditions;
  Conditions stopConditions;
  FeedFrom feedFrom;
} Action; 

typedef struct {
    uint8_t checksum;
    FeedFrom feedFrom;
    bool mustRunInitialSetup;
    char deviceName[10];

    bool trayNeedsEmptying;

    uint16_t moistSensorCalibrationSoaked;
    uint16_t moistSensorCalibrationDry;
    

    // Less than these will make it qualify
    int16_t trayWaterLevelSensorCalibrationQuarter;
    int16_t trayWaterLevelSensorCalibrationHalf;
    int16_t trayWaterLevelSensorCalibrationThreeQuarters;

    uint32_t minFeedInterval;
    uint32_t maxFeedTime;
    uint32_t maxPumpOutTime;
    uint32_t pumpOutRestTime;

    Action actions[ACTIONS_ARRAY_SIZE];

    int8_t activeActionsIndex0;
    int8_t activeActionsIndex1;
    uint16_t kbdUp;
    uint16_t kbdDown;
    uint16_t kbdLeft;
    uint16_t kbdRight;
    uint16_t kbdOk;
} Config;

uint8_t calculateConfigChecksum();
bool configChecksumCorrect();
bool verifyConfigChecksum();
void setConfigChecksum();
void restoreDefaultConfig();

void saveConfig();
// Config& getConfig();
void loadConfig();
#endif CONFIG_H
