#ifndef FEED_SLOTS_H
#define FEED_SLOTS_H

#include <stdint.h>
#include <stdbool.h>

#define FEED_SLOT_COUNT 16
#define FEED_SLOT_PACKED_SIZE 10

enum FeedSlotFlags : uint8_t {
  FEED_SLOT_ENABLED = 1u << 0,
  FEED_SLOT_PULSED = 1u << 1,
  FEED_SLOT_HAS_TIME_WINDOW = 1u << 2,
  FEED_SLOT_HAS_MOISTURE_BELOW = 1u << 3,
  FEED_SLOT_HAS_MIN_SINCE = 1u << 4,
  FEED_SLOT_HAS_MOISTURE_TARGET = 1u << 5,
  FEED_SLOT_HAS_MIN_RUNTIME = 1u << 6,
  FEED_SLOT_RUNOFF_REQUIRED = 1u << 7
};

typedef struct {
  uint16_t startMinute;
  uint16_t endMinute;
  uint8_t moistureBelow;
  uint8_t moistureTarget;
  uint16_t minSinceLastMinutes;
  uint8_t minRuntime5s;
  uint8_t maxRuntime5s;
  uint8_t pulseOn5s;
  uint8_t pulseOff5s;
  uint8_t flags;
} FeedSlot;

/*
 * Pack a FeedSlot into a 10-byte buffer.
 *
 * Bit layout (LSB-first, byte 0 bit 0 is bit 0):
 *   Bits  0-10 : startMinute (0-1439)
 *   Bits 11-21 : endMinute (0-1439)
 *   Bits 22-28 : moistureBelow (0-100)
 *   Bits 29-35 : moistureTarget (0-100)
 *   Bits 36-47 : minSinceLastMinutes (0-4095)
 *   Bits 48-53 : minRuntime5s (0-48)        [5-second ticks]
 *   Bits 54-60 : maxRuntime5s (0-120)       [5-second ticks]
 *   Bits 61-64 : pulseOn5s (0-12)           [5-second ticks]
 *   Bits 65-71 : pulseOff5s (0-120)         [5-second ticks]
 *   Bits 72-79 : flags (see FeedSlotFlags)
 *
 * Fields can span byte boundaries; packing/unpacking writes/reads individual
 * bits so split fields are handled automatically.
 */
void packFeedSlot(uint8_t out[FEED_SLOT_PACKED_SIZE], const FeedSlot *slot);
void unpackFeedSlot(FeedSlot *slot, const uint8_t in[FEED_SLOT_PACKED_SIZE]);

#endif FEED_SLOTS_H
