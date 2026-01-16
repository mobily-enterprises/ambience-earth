#include "config.h"

#include <Arduino.h>
#include <LittleFS.h>
#include <string.h>

namespace {
const char *kConfigPath = "/config.bin";

/*
 * ensure_fs
 * Mounts LittleFS so config data can be read and written.
 * Example:
 *   if (!ensure_fs()) return;
 */
static bool ensure_fs() {
  if (LittleFS.begin()) return true;
  return LittleFS.begin(true);
}

/*
 * computePulseSettings
 * Derives pulse on/off seconds from calibration values.
 * Example:
 *   computePulseSettings(&onSec, &offSec, targetUnits, dripperMsPerLiter);
 */
static void computePulseSettings(uint8_t *outOn, uint8_t *outOff, uint8_t targetUnits, uint32_t dripperMsPerLiter) {
  if (!outOn || !outOff) return;
  *outOn = 0;
  *outOff = 0;
  if (targetUnits < 5 || targetUnits > 50 || dripperMsPerLiter == 0) return;

  uint32_t fullRate = 3600000000UL / dripperMsPerLiter;
  uint32_t targetRate = static_cast<uint32_t>(targetUnits) * 200UL;
  if (fullRate <= targetRate) return;

  uint32_t diff = fullRate - targetRate;
  uint8_t onSec = 10;
  uint32_t offCalc = static_cast<uint32_t>(onSec) * diff / targetRate;
  if (offCalc > 60) {
    onSec = 5;
    offCalc = static_cast<uint32_t>(onSec) * diff / targetRate;
  }
  if (offCalc > 255) offCalc = 255;
  *outOn = onSec;
  *outOff = static_cast<uint8_t>(offCalc);
}
} // namespace

Config config;

uint8_t calculateConfigChecksum() {
  uint8_t hash = 0xA5;
  const uint8_t *ptr = reinterpret_cast<const uint8_t *>(&config);
  for (size_t i = 1; i < sizeof(config); ++i) {
    hash ^= ptr[i];
  }
  return hash;
}

void setConfigChecksum() {
  config.checksum = calculateConfigChecksum();
}

bool validateConfig() {
  if (calculateConfigChecksum() != config.checksum) return false;
  if (config.version != CONFIG_VERSION) return false;
  return true;
}

void restoreDefaultConfig() {
  memset(&config, 0, sizeof(config));
  config.version = CONFIG_VERSION;
  config.flags = 0;
  config.moistSensorCalibrationSoaked = 0;
  config.moistSensorCalibrationDry = 1024;
  config.dripperMsPerLiter = 600000;
  config.lightsOnMinutes = 0;
  config.lightsOffMinutes = 0;
  config.maxDailyWaterMl = 1000;
  config.pulseOnSeconds = 10;
  config.pulseOffSeconds = 5;
  config.pulseTargetUnits = 20;
  config.baselineX = 10;
  config.baselineY = 5;
  config.baselineDelayMinutes = 1;
  memset(config.runoffExpectation, 0, sizeof(config.runoffExpectation));
  memset(config.feedSlotNames, 0, sizeof(config.feedSlotNames));
  memset(config.feedSlotsPacked, 0, sizeof(config.feedSlotsPacked));
  setConfigChecksum();
}

void saveConfig() {
  if (!ensure_fs()) return;
  setConfigChecksum();
  File f = LittleFS.open(kConfigPath, "w");
  if (!f) return;
  f.write(reinterpret_cast<const uint8_t *>(&config), sizeof(config));
  f.flush();
  f.close();
}

bool loadConfig() {
  if (!ensure_fs()) return false;
  File f = LittleFS.open(kConfigPath, "r");
  if (!f) return false;
  size_t read_len = f.read(reinterpret_cast<uint8_t *>(&config), sizeof(config));
  f.close();
  return read_len == sizeof(config);
}

void initConfig() {
  if (!loadConfig() || !validateConfig()) {
    restoreDefaultConfig();
    saveConfig();
  }
  applySimDefaults();
}

void applySimDefaults() {
  bool changed = false;

  if (config.flags & CONFIG_FLAG_MUST_RUN_INITIAL_SETUP) {
    config.flags &= static_cast<uint8_t>(~CONFIG_FLAG_MUST_RUN_INITIAL_SETUP);
    changed = true;
  }

  if ((config.flags & CONFIG_FLAG_TIME_SET) == 0) {
    config.flags |= CONFIG_FLAG_TIME_SET;
    changed = true;
  }
  if ((config.flags & CONFIG_FLAG_LIGHTS_ON_SET) == 0) {
    config.flags |= CONFIG_FLAG_LIGHTS_ON_SET;
    changed = true;
  }
  if ((config.flags & CONFIG_FLAG_LIGHTS_OFF_SET) == 0) {
    config.flags |= CONFIG_FLAG_LIGHTS_OFF_SET;
    changed = true;
  }
  if ((config.flags & CONFIG_FLAG_DRIPPER_CALIBRATED) == 0) {
    config.flags |= CONFIG_FLAG_DRIPPER_CALIBRATED;
    changed = true;
  }
  if ((config.flags & CONFIG_FLAG_MAX_DAILY_SET) == 0) {
    config.flags |= CONFIG_FLAG_MAX_DAILY_SET;
    changed = true;
  }
  if ((config.flags & CONFIG_FLAG_PULSE_SET) == 0) {
    config.flags |= CONFIG_FLAG_PULSE_SET;
    changed = true;
  }

  if (config.lightsOnMinutes > 1439 || config.lightsOnMinutes == 0) {
    config.lightsOnMinutes = 6 * 60;
    changed = true;
  }
  if (config.lightsOffMinutes > 1439 || config.lightsOffMinutes == 0) {
    config.lightsOffMinutes = 18 * 60;
    changed = true;
  }
  if (config.maxDailyWaterMl == 0) {
    config.maxDailyWaterMl = 1000;
    changed = true;
  }
  if (config.moistSensorCalibrationDry == 1024) {
    config.moistSensorCalibrationDry = 800;
    changed = true;
  }
  if (config.moistSensorCalibrationSoaked == 0) {
    config.moistSensorCalibrationSoaked = 300;
    changed = true;
  }
  if (config.dripperMsPerLiter == 0) {
    config.dripperMsPerLiter = 600000;
    changed = true;
  }
  if (config.pulseTargetUnits < 5 || config.pulseTargetUnits > 50) {
    config.pulseTargetUnits = 20;
    changed = true;
  }
  if (config.pulseOnSeconds == 0 && config.pulseOffSeconds == 0) {
    uint8_t onSec = 0;
    uint8_t offSec = 0;
    computePulseSettings(&onSec, &offSec, config.pulseTargetUnits, config.dripperMsPerLiter);
    if (config.pulseOnSeconds != onSec || config.pulseOffSeconds != offSec) {
      config.pulseOnSeconds = onSec;
      config.pulseOffSeconds = offSec;
      changed = true;
    }
  }

  if (changed) saveConfig();
}
