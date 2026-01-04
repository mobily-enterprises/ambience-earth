#include "volume.h"

#include <stdint.h>

uint32_t volumeMlToMs(uint16_t volumeMl, uint32_t dripperMsPerLiter) {
  if (volumeMl == 0 || dripperMsPerLiter == 0) return 0;
  uint64_t ms = (uint64_t)volumeMl * (uint64_t)dripperMsPerLiter;
  ms /= 1000ULL; // ms per ml
  if (ms == 0) ms = 1;
  if (ms > 0xFFFFFFFFULL) ms = 0xFFFFFFFFULL;
  return static_cast<uint32_t>(ms);
}

uint16_t msToVolumeMl(uint32_t millis, uint32_t dripperMsPerLiter) {
  if (millis == 0 || dripperMsPerLiter == 0) return 0;
  uint64_t ml = (uint64_t)millis * 1000ULL;
  ml /= (uint64_t)dripperMsPerLiter;
  if (ml > 0xFFFFULL) ml = 0xFFFFULL;
  return static_cast<uint16_t>(ml);
}
