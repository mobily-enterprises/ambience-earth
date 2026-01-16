#include "feedingUtils.h"

/*
 * slotFlag
 * Returns true if the given feed slot has the requested flag set.
 * Example:
 *   bool enabled = slotFlag(&slot, FEED_SLOT_ENABLED);
 */
bool slotFlag(const FeedSlot *slot, uint8_t flag) {
  if (!slot) return false;
  return (slot->flags & flag) != 0;
}

/*
 * clampBaselineOffset
 * Clamps baseline offsets to the even 2-20 range used by the parent logic.
 * Example:
 *   uint8_t offset = clampBaselineOffset(9);
 */
uint8_t clampBaselineOffset(uint8_t value) {
  if (value < 2) return 2;
  if (value > 20) return 20;
  return static_cast<uint8_t>(value & ~1u);
}

/*
 * baselineMinus
 * Computes a baseline target minus an offset while staying within bounds.
 * Example:
 *   uint8_t target = baselineMinus(baseline, 6);
 */
uint8_t baselineMinus(uint8_t baseline, uint8_t offset) {
  offset = clampBaselineOffset(offset);
  if (baseline <= offset) return 0;
  return static_cast<uint8_t>(baseline - offset);
}
