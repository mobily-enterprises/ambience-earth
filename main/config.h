#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>
#include <Arduino.h>
#include "feedSlots.h"

#define CONFIG_ADDRESS 0
#define CONFIG_VERSION 17
#define CONFIG_FLAG_MUST_RUN_INITIAL_SETUP 0x01
#define CONFIG_FLAG_FEEDING_DISABLED 0x02
#define CONFIG_FLAG_DRIPPER_CALIBRATED 0x04
#define CONFIG_FLAG_TIME_SET 0x08
#define CONFIG_FLAG_LIGHTS_ON_SET 0x10
#define CONFIG_FLAG_LIGHTS_OFF_SET 0x20
#define CONFIG_FLAG_MAX_DAILY_SET 0x40
#define CONFIG_FLAG_PULSE_SET 0x80

// ************************************************************
// ** TYPE DEFINITIONS
// ************************************************************

typedef struct {
    uint8_t checksum;
    uint8_t version;
    uint8_t flags;

    uint16_t moistSensorCalibrationSoaked;
    uint16_t moistSensorCalibrationDry;

    uint32_t dripperMsPerLiter;
    uint16_t lightsOnMinutes;
    uint16_t lightsOffMinutes;
    uint16_t maxDailyWaterMl;
    uint8_t pulseOnSeconds;
    uint8_t pulseOffSeconds;
    uint8_t pulseTargetUnits;
    uint8_t baselineX;
    uint8_t baselineY;
    uint8_t baselineDelayMinutes;
    uint8_t runoffExpectation[FEED_SLOT_COUNT]; // per-slot expectation: 0=none,1=must,2=avoid

    uint16_t kbdUp;
    uint16_t kbdDown;
    uint16_t kbdLeft;
    uint16_t kbdRight;
    uint16_t kbdOk;
    char feedSlotNames[FEED_SLOT_COUNT][FEED_SLOT_NAME_LENGTH + 1];
    uint8_t feedSlotsPacked[FEED_SLOT_COUNT][FEED_SLOT_PACKED_SIZE];
} Config;

static_assert(sizeof(Config) <= 256, "Config exceeds 256-byte EEPROM region");

uint8_t calculateConfigChecksum();
void setConfigChecksum();
void restoreDefaultConfig();
bool validateConfig();

void saveConfig();
// Config& getConfig();
void loadConfig();
#endif CONFIG_H
