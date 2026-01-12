
#include <string.h>
#include <EEPROM.h>
#include "config.h"

Config config;

bool validateConfig() {
  if (calculateConfigChecksum() != config.checksum) return false;
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

/*
Config& getConfig() {
  return config;
}
*/

uint8_t calculateConfigChecksum() {
  uint8_t hash = 0xA5;
  uint8_t* ptr = reinterpret_cast<uint8_t*>(&config);

  // Skip only the checksum byte itself.
  for (size_t i = 1; i < sizeof(config); ++i) {
    hash ^= ptr[i];
  }

  return hash;
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
#ifdef WOKWI_SIM
  config.dripperMsPerLiter = 600000;
#else
  config.dripperMsPerLiter = 900000;
#endif
  config.lightsOnMinutes = 0;
  config.lightsOffMinutes = 0;
  config.maxDailyWaterMl = 1000;
#ifdef WOKWI_SIM
  config.pulseOnSeconds = 10;
  config.pulseOffSeconds = 5;
#else
  config.pulseOnSeconds = 0;
  config.pulseOffSeconds = 0;
#endif
  config.pulseTargetUnits = 20;
  config.baselineX = 10;
  config.baselineY = 5;
#ifdef WOKWI_SIM
  config.baselineDelayMinutes = 1;
#else
  config.baselineDelayMinutes = 30;
#endif
  memset(config.runoffExpectation, 0, sizeof(config.runoffExpectation));

  config.kbdUp = 0;
  config.kbdDown = 0;
  config.kbdLeft = 0;
  config.kbdRight = 0;
  config.kbdOk = 0;

  memset(config.feedSlotNames, 0, sizeof(config.feedSlotNames));
  memset(config.feedSlotsPacked, 0, sizeof(config.feedSlotsPacked));

  // Set default checksum
  config.checksum = calculateConfigChecksum();
}
