#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>
#include <Arduino.h>

#define CONFIG_ADDRESS 0
#define CONFIG_VERSION 1

// ************************************************************
// ** TYPE DEFINITIONS
// ************************************************************

typedef struct {
  enum : int8_t { P1, S1 } what;
} WaterSource;

typedef struct {
    uint8_t checksum;
    uint8_t version;
    bool mustRunInitialSetup;
    char deviceName[10];

    uint16_t moistSensorCalibrationSoaked;
    uint16_t moistSensorCalibrationDry;

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
bool validateAndMigrateConfig();

void saveConfig();
// Config& getConfig();
void loadConfig();
#endif CONFIG_H
