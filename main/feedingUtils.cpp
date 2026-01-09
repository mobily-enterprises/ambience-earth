#include "feedingUtils.h"

bool slotFlag(const FeedSlot *slot, uint8_t flag) {
  return (slot->flags & flag) != 0;
}

uint8_t clampBaselineOffset(uint8_t value) {
  if (value < 2) return 2;
  if (value > 20) return 20;
  return static_cast<uint8_t>(value & ~1u);
}

uint8_t baselineMinus(uint8_t baseline, uint8_t offset) {
  offset = clampBaselineOffset(offset);
  if (baseline <= offset) return 0;
  return static_cast<uint8_t>(baseline - offset);
}
