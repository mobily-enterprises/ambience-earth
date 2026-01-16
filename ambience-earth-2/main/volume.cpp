#include "volume.h"

/*
 * volumeMlToMs
 * Converts milliliters to milliseconds using dripper calibration.
 * Example:
 *   uint32_t ms = volumeMlToMs(250, 600000);
 */
uint32_t volumeMlToMs(uint16_t volumeMl, uint32_t dripperMsPerLiter) {
  if (volumeMl == 0 || dripperMsPerLiter == 0) return 0;
  if (dripperMsPerLiter > 4294967UL) dripperMsPerLiter = 4294967UL;
  uint32_t perMl = dripperMsPerLiter / 1000UL;
  uint32_t rem = dripperMsPerLiter % 1000UL;
  uint32_t ms = 0;

  if (perMl) {
    if (volumeMl > (uint32_t)(0xFFFFFFFFUL / perMl)) return 0xFFFFFFFFUL;
    ms = static_cast<uint32_t>(volumeMl) * perMl;
  }

  if (rem) {
    uint32_t extra = static_cast<uint32_t>(volumeMl) * rem;
    extra /= 1000UL;
    if (ms > 0xFFFFFFFFUL - extra) return 0xFFFFFFFFUL;
    ms += extra;
  }

  if (ms == 0) ms = 1;
  return ms;
}

/*
 * msToVolumeMl
 * Converts milliseconds to milliliters using dripper calibration.
 * Example:
 *   uint16_t ml = msToVolumeMl(10000, 600000);
 */
uint16_t msToVolumeMl(uint32_t millis, uint32_t dripperMsPerLiter) {
  if (millis == 0 || dripperMsPerLiter == 0) return 0;
  if (dripperMsPerLiter > 4294967UL) dripperMsPerLiter = 4294967UL;
  uint32_t q = millis / dripperMsPerLiter;
  if (q >= 66) return 0xFFFF;
  uint32_t r = millis % dripperMsPerLiter;
  uint32_t ml = q * 1000UL;
  if (r) ml += (r * 1000UL) / dripperMsPerLiter;
  if (ml > 0xFFFFUL) ml = 0xFFFFUL;
  return static_cast<uint16_t>(ml);
}
