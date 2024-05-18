#ifndef DATA_H
#define DATA_H

#include <stdint.h>
#include <Arduino.h>
#include "ui.h"

#define LABEL_STRING_LENGTH 20
#define CONFIG_ADDRESS 0

// ************************************************************
// ** TYPE DEFINITIONS
// ************************************************************

typedef struct {
  enum : uint8_t { P1, S1 } what;
} WaterSource;

enum FeedFrom : uint8_t {
  FEED_FROM_TOP,
  FEED_FROM_TRAY
};

typedef struct {
  enum : uint8_t { TRAY_IGNORED, TRAY_EMPTY, TRAY_AT_MOST_WET, TRAY_AT_LEAST_WET, TRAY_AT_MOST_HALF_FULL, TRAY_AT_LEAST_HALF_FULL, TRAY_FULL } tray;
  enum : uint8_t { SOIL_IGNORED, SOIL_DRY, SOIL_AT_MOST_DAMP, SOIL_AT_LEAST_DAMP, SOIL_AT_MOST_WET, SOIL_AT_LEAST_WET, SOIL_VERY_WET } soil;
  enum : uint8_t { NO_LOGIC, TRAY_OR_SOIL, TRAY_AND_SOIL } logic;
} Conditions;

typedef struct {
  char name[DISPLAY_COLUMNS + 1];
  Conditions triggerConditions;
  Conditions endConditions;
  FeedFrom feedFrom;
  bool active;
} Action; 

typedef struct {
  uint8_t actionIndex;
  uint8_t everyNFeeds; // 0 for every time
  uint8_t maxNTimesPerHour; // 0 for every time
} ActiveAction;

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
    ActiveAction activeAction;
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
