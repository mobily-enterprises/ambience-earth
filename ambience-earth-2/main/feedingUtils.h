#pragma once

#include <stdint.h>
#include "feedSlots.h"

/*
 * slotFlag
 * Returns true if the given feed slot has the requested flag set.
 * Example:
 *   if (slotFlag(&slot, FEED_SLOT_ENABLED)) { ... }
 */
bool slotFlag(const FeedSlot *slot, uint8_t flag);

/*
 * clampBaselineOffset
 * Clamps baseline offsets to the even 2-20 range used by the parent logic.
 * Example:
 *   uint8_t offset = clampBaselineOffset(input);
 */
uint8_t clampBaselineOffset(uint8_t value);

/*
 * baselineMinus
 * Computes a baseline target minus an offset while staying within bounds.
 * Example:
 *   uint8_t target = baselineMinus(baseline, config.baselineX);
 */
uint8_t baselineMinus(uint8_t baseline, uint8_t offset);
