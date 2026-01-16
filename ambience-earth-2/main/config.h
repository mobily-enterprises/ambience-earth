#pragma once

#include <stdint.h>
#include "feedSlots.h"

#define CONFIG_VERSION 17
#define CONFIG_FLAG_MUST_RUN_INITIAL_SETUP 0x01
#define CONFIG_FLAG_FEEDING_DISABLED 0x02
#define CONFIG_FLAG_DRIPPER_CALIBRATED 0x04
#define CONFIG_FLAG_TIME_SET 0x08
#define CONFIG_FLAG_LIGHTS_ON_SET 0x10
#define CONFIG_FLAG_LIGHTS_OFF_SET 0x20
#define CONFIG_FLAG_MAX_DAILY_SET 0x40
#define CONFIG_FLAG_PULSE_SET 0x80

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
  uint8_t runoffExpectation[FEED_SLOT_COUNT];

  uint16_t kbdUp;
  uint16_t kbdDown;
  uint16_t kbdLeft;
  uint16_t kbdRight;
  uint16_t kbdOk;
  char feedSlotNames[FEED_SLOT_COUNT][FEED_SLOT_NAME_LENGTH + 1];
  uint8_t feedSlotsPacked[FEED_SLOT_COUNT][FEED_SLOT_PACKED_SIZE];
} Config;

extern Config config;

/*
 * calculateConfigChecksum
 * Computes the checksum used to validate persisted config data.
 * Example:
 *   uint8_t sum = calculateConfigChecksum();
 */
uint8_t calculateConfigChecksum();

/*
 * setConfigChecksum
 * Updates the checksum field in the global config struct.
 * Example:
 *   setConfigChecksum();
 */
void setConfigChecksum();

/*
 * restoreDefaultConfig
 * Resets the global config struct to factory defaults.
 * Example:
 *   restoreDefaultConfig();
 */
void restoreDefaultConfig();

/*
 * validateConfig
 * Returns true when the current config passes checksum/version checks.
 * Example:
 *   if (!validateConfig()) restoreDefaultConfig();
 */
bool validateConfig();

/*
 * saveConfig
 * Persists the current config to flash storage.
 * Example:
 *   saveConfig();
 */
void saveConfig();

/*
 * loadConfig
 * Loads the config from flash storage into the global struct.
 * Example:
 *   if (!loadConfig()) restoreDefaultConfig();
 */
bool loadConfig();

/*
 * initConfig
 * Loads config from storage or restores defaults when invalid.
 * Example:
 *   initConfig();
 */
void initConfig();

/*
 * applySimDefaults
 * Applies Wokwi-friendly defaults so feeding logic is usable in simulation.
 * Example:
 *   applySimDefaults();
 */
void applySimDefaults();
