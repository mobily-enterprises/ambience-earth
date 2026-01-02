#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>
#include <Arduino.h>
#include "feedSlots.h"

#define CONFIG_ADDRESS 0
#define CONFIG_VERSION 4
#define CONFIG_FLAG_MUST_RUN_INITIAL_SETUP 0x01
#define CONFIG_FLAG_FEEDING_DISABLED 0x02

// ************************************************************
// ** TYPE DEFINITIONS
// ************************************************************

typedef struct {
    uint8_t checksum;
    uint8_t version;
    uint8_t flags;

    uint16_t moistSensorCalibrationSoaked;
    uint16_t moistSensorCalibrationDry;

    uint16_t kbdUp;
    uint16_t kbdDown;
    uint16_t kbdLeft;
    uint16_t kbdRight;
    uint16_t kbdOk;
    uint8_t feedSlotsPacked[FEED_SLOT_COUNT][FEED_SLOT_PACKED_SIZE];
} Config;

static_assert(sizeof(Config) <= 256, "Config exceeds 256-byte EEPROM region");

uint8_t calculateConfigChecksum();
bool configChecksumCorrect();
bool verifyConfigChecksum();
void setConfigChecksum();
void restoreDefaultConfig();
bool validateConfig();

void saveConfig();
// Config& getConfig();
void loadConfig();
#endif CONFIG_H
