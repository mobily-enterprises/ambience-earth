#ifndef VOLUME_H
#define VOLUME_H

#include <stdint.h>

// Convert milliliters to milliseconds using dripper calibration (ms per liter).
// Returns 0 if calibration is missing or volume is zero.
uint32_t volumeMlToMs(uint16_t volumeMl, uint32_t dripperMsPerLiter);

// Convert milliseconds to milliliters using dripper calibration.
// Returns 0 if calibration is missing or duration is zero.
uint16_t msToVolumeMl(uint32_t millis, uint32_t dripperMsPerLiter);

#endif // VOLUME_H
