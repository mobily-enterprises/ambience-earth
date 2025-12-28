
#include <string.h>
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
#ifdef WOKWI_SIM
  config.mustRunInitialSetup = false;
#else
  config.mustRunInitialSetup = true;
#endif

  config.moistSensorCalibrationSoaked = 200;
  config.moistSensorCalibrationDry = 500;

  config.minFeedInterval = 1000L * 60 * 30;
  //config.minFeedIntervalMoistOverride = 65;


  config.maxFeedTime = 1000L * 60 * 3;

#ifdef WOKWI_SIM
  // Wokwi keyladder defaults: E=500, X=300, A=900, F=700, D=100
  config.kbdUp = 500;
  config.kbdDown = 300;
  config.kbdLeft = 900;
  config.kbdRight = 100;
  config.kbdOk = 700;
#else
  config.kbdUp = 0;
  config.kbdDown = 0;
  config.kbdLeft = 0;
  config.kbdRight = 0;
  config.kbdOk = 0;
#endif

  // Set default checksum
  config.checksum = calculateConfigChecksum();
}
