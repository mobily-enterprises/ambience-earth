#pragma once

#include <stdbool.h>
#include <stdint.h>

constexpr uint8_t FEED_SLOT_COUNT = 8;
constexpr uint8_t FEED_SLOT_NAME_LENGTH = 6;
constexpr uint8_t FEED_SLOT_PACKED_SIZE = 12;

enum FeedSlotFlags : uint8_t {
  FEED_SLOT_ENABLED = 1u << 0,
  FEED_SLOT_HAS_TIME_WINDOW = 1u << 1,
  FEED_SLOT_HAS_MOISTURE_BELOW = 1u << 2,
  FEED_SLOT_HAS_MIN_SINCE = 1u << 3,
  FEED_SLOT_HAS_MOISTURE_TARGET = 1u << 4,
  FEED_SLOT_BASELINE_SETTER = 1u << 5,
  FEED_SLOT_RUNOFF_REQUIRED = 1u << 6,
  FEED_SLOT_RUNOFF_AVOID = 1u << 7
};

typedef struct {
  uint16_t windowStartMinutes;
  uint16_t windowDurationMinutes;
  uint8_t moistureBelow;
  uint8_t moistureTarget;
  uint16_t minGapMinutes;
  uint16_t maxVolumeMl;
  uint8_t runoffHold5s;
  uint8_t flags;
} FeedSlot;

/*
 * packFeedSlot
 * Packs a FeedSlot into a 12-byte buffer using the parent-project bit layout.
 * Example:
 *   uint8_t packed[FEED_SLOT_PACKED_SIZE] = {0};
 *   packFeedSlot(packed, &slot);
 */
void packFeedSlot(uint8_t out[FEED_SLOT_PACKED_SIZE], const FeedSlot *slot);

/*
 * unpackFeedSlot
 * Unpacks a FeedSlot from a 12-byte buffer produced by packFeedSlot().
 * Example:
 *   FeedSlot slot = {};
 *   unpackFeedSlot(&slot, packed);
 */
void unpackFeedSlot(FeedSlot *slot, const uint8_t in[FEED_SLOT_PACKED_SIZE]);
