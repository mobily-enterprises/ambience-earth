
#include <string.h>
#include <EEPROM.h>
#include "config.h"

Config config;

bool validateConfig() {
  if (!configChecksumCorrect()) return false;
  if (config.version != CONFIG_VERSION) return false;
  return true;
}

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

  // Skip only the checksum byte itself.
  for (size_t i = 1; i < sizeof(config); ++i) {
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
  config.version = CONFIG_VERSION;
  config.flags = 0;
#ifdef WOKWI_SIM
  config.flags &= static_cast<uint8_t>(~CONFIG_FLAG_MUST_RUN_INITIAL_SETUP);
#else
  config.flags |= CONFIG_FLAG_MUST_RUN_INITIAL_SETUP;
#endif

  config.moistSensorCalibrationSoaked = 0;
  config.moistSensorCalibrationDry = 1024;
  config.weightFullKg10 = 0;

  config.kbdUp = 0;
  config.kbdDown = 0;
  config.kbdLeft = 0;
  config.kbdRight = 0;
  config.kbdOk = 0;

  memset(config.feedSlotsPacked, 0, sizeof(config.feedSlotsPacked));

  // Set default checksum
  config.checksum = calculateConfigChecksum();
}
