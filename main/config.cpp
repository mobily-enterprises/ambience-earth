#include <string.h>
#include <Arduino.h>
#include <EEPROM.h>
#include "config.h"

Config config;

void saveConfig() {  
  setConfigChecksum();
  EEPROM.put(CONFIG_ADDRESS, config);
}

void loadConfig() {
  EEPROM.get(CONFIG_ADDRESS, config);
}

bool configChecksumCorrect() {
  // Serial.println("CHECKSUM CHECK:");
  // Serial.println(calculateConfigChecksum());
  // Serial.println(config.checksum);
  return calculateConfigChecksum() == config.checksum;
}

/*
Config& getConfig() {
  return config;
}
*/

void initEeprom() {
  EEPROM.begin();
}

uint8_t calculateConfigChecksum() {
  const uint64_t FNV_offset_basis = 14695981039346656037ULL;
  const uint64_t FNV_prime = 1099511628211ULL;

  uint64_t hash = FNV_offset_basis;
  uint8_t* ptr = reinterpret_cast<uint8_t*>(&config);

  // Skip the first 8 bytes
  for (size_t i = 8; i < sizeof(config); ++i) {
    hash ^= static_cast<uint64_t>(ptr[i]);
    hash *= FNV_prime;
  }

  return static_cast<uint8_t>(hash);
}

// Function to verify the checksum of a Config object
bool verifyConfigChecksum() {
  // Calculate checksum using helper function
  uint8_t checksum = calculateConfigChecksum();
  
  // Compare the calculated checksum with the checksum stored in the Config struct
  return checksum == config.checksum;
}

// Function to set the checksum of a Config object
void setConfigChecksum() {
  config.checksum = calculateConfigChecksum();
}


void restoreDefaultConfig() {
  config.checksum = 0;
  config.feedFrom = FeedFrom::FEED_FROM_TOP;
  config.mustRunInitialSetup = true;
  strncpy(config.deviceName, "YOUR FEEDER", sizeof(config.deviceName));
  config.deviceName[sizeof(config.deviceName) - 1] = '\0';
  
  config.soilLittleMoistPercentage = 20;
  config.soilMoistPercentage = 60;
  config.soilVeryMoistPercentage = 80;

  config.trayNeedsEmptying = false;
 
  config.moistSensorCalibrationSoaked = 400;
  config.moistSensorCalibrationDry = 780;

  config.trayWaterLevelSensorCalibrationEmpty;
  config.trayWaterLevelSensorCalibrationHalf;
  config.trayWaterLevelSensorCalibrationFull;

  // Set default checksum
  config.checksum = calculateConfigChecksum();

  config.actions[0] = Action {
    "BOTTOM FEED",
    { Conditions::TRAY_DRY, Conditions::SOIL_DRY, Conditions::TRAY_OR_SOIL },
    { Conditions::TRAY_PLENTY, Conditions::SOIL_IGNORED, Conditions::NO_LOGIC },
    FeedFrom::FEED_FROM_TRAY,
  };
  config.actions[1] = Action {
    "TOP FEED ROFF",
    { Conditions::TRAY_IGNORED, Conditions::SOIL_LITTLE_MOIST, Conditions::NO_LOGIC },
    { Conditions::TRAY_EMPTY, Conditions::SOIL_IGNORED, Conditions::NO_LOGIC },
    FeedFrom::FEED_FROM_TOP,
  };
  config.actions[2] = Action {
    "STACK FEED",
    { Conditions::TRAY_IGNORED, Conditions::SOIL_LITTLE_MOIST, Conditions::NO_LOGIC },
    { Conditions::TRAY_EMPTY, Conditions::SOIL_LITTLE_MOIST, Conditions::TRAY_OR_SOIL },
    FeedFrom::FEED_FROM_TOP,
  };
  config.actions[3] = Action {
    "STACK FEED ROFF",
    { Conditions::TRAY_IGNORED, Conditions::SOIL_LITTLE_MOIST, Conditions::NO_LOGIC },
    { Conditions::TRAY_EMPTY, Conditions::SOIL_MOIST, Conditions::TRAY_AND_SOIL },
    FeedFrom::FEED_FROM_TOP,
  };
  config.actions[4] = Action {
    "FEED RECIRCUL8",
    { Conditions::TRAY_IGNORED, Conditions::SOIL_LITTLE_MOIST, Conditions::NO_LOGIC },
    { Conditions::TRAY_IGNORED, Conditions::SOIL_IGNORED, Conditions::NO_LOGIC },
    FeedFrom::FEED_FROM_TOP,
  };
  config.actions[5] = Action {
    "Custom 1",
    { },
    { },
    FeedFrom::FEED_FROM_TOP,
  };
  config.activeActionsIndex0 = -1;
  config.activeActionsIndex1 = -1;
}

