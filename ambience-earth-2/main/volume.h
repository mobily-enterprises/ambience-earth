#pragma once

#include <stdint.h>

/*
 * volumeMlToMs
 * Converts milliliters to milliseconds using dripper calibration.
 * Example:
 *   uint32_t ms = volumeMlToMs(250, config.dripperMsPerLiter);
 */
uint32_t volumeMlToMs(uint16_t volumeMl, uint32_t dripperMsPerLiter);

/*
 * msToVolumeMl
 * Converts milliseconds to milliliters using dripper calibration.
 * Example:
 *   uint16_t ml = msToVolumeMl(10000, config.dripperMsPerLiter);
 */
uint16_t msToVolumeMl(uint32_t millis, uint32_t dripperMsPerLiter);
