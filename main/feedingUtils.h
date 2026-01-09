#ifndef FEEDING_UTILS_H
#define FEEDING_UTILS_H

#include <stdint.h>
#include "feedSlots.h"

bool slotFlag(const FeedSlot *slot, uint8_t flag);
uint8_t clampBaselineOffset(uint8_t value);
uint8_t baselineMinus(uint8_t baseline, uint8_t offset);

#endif  // FEEDING_UTILS_H
