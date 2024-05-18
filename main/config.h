#ifndef DATA_H
#define DATA_H

#include <stdint.h>
#include <Arduino.h>
#include "ui.h"

#define CONFIG_ADDRESS 0

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
//   enum : uint8_t { TRAY_IGNORED, TRAY_EMPTY, TRAY_AT_MOST_WET, TRAY_AT_LEAST_WET, TRAY_AT_MOST_HALF_FULL, TRAY_AT_LEAST_HALF_FULL, TRAY_FULL } tray;
//  enum : uint8_t { SOIL_IGNORED, SOIL_DRY, SOIL_AT_MOST_DAMP, SOIL_AT_LEAST_DAMP, SOIL_AT_MOST_WET, SOIL_AT_LEAST_WET, SOIL_VERY_WET } soil;
  enum : int8_t { TRAY_IGNORED, TRAY_EMPTY, TRAY_WET, TRAY_HALF_FULL, TRAY_FULL } tray;
  enum : int8_t { SOIL_IGNORED, SOIL_DRY, SOIL_DAMP, SOIL_WET, SOIL_VERY_WET } soil;
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

    // Less than these will make it qualify
    int8_t soilLittleMoistPercentage;
    int8_t soilMoistPercentage;
    int8_t soilVeryMoistPercentage;

    bool trayNeedsEmptying;

    uint16_t moistSensorCalibrationSoaked;
    uint16_t moistSensorCalibrationDry;
    

    // Less than these will make it qualify
    int16_t trayWaterLevelSensorCalibrationEmpty;
    int16_t trayWaterLevelSensorCalibrationWet;
    int16_t trayWaterLevelSensorCalibrationHalfFull;

    Action actions[6];

    int8_t activeActionsIndexes[4];
    int a;
} Config;

uint8_t calculateConfigChecksum();
bool configChecksumCorrect();
bool verifyConfigChecksum();
void setConfigChecksum();
void restoreDefaultConfig();

void saveConfig();
// Config& getConfig();
void loadConfig();
#endif DATA_H
