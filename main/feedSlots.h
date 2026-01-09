#ifndef FEED_SLOTS_H
#define FEED_SLOTS_H

#include <stdint.h>
#include <stdbool.h>

#define FEED_SLOT_COUNT 8
#define FEED_SLOT_NAME_LENGTH 6
#define FEED_SLOT_PACKED_SIZE 12

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
  uint16_t windowStartMinutes;    // minutes after lights-on
  uint16_t windowDurationMinutes; // window length in minutes
  uint8_t moistureBelow;
  uint8_t moistureTarget;
  uint16_t minGapMinutes;
  uint16_t maxVolumeMl;
  uint8_t runoffHold5s;
  uint8_t flags;
} FeedSlot;

/*
 * Pack a FeedSlot into a 12-byte buffer.
 *
 * Bit layout (LSB-first, byte 0 bit 0 is bit 0):
 *   Bits  0-10 : windowStartMinutes (0-1439) [offset from lights-on]
 *   Bits 11-21 : windowDurationMinutes (0-1439)
 *   Bits 22-28 : moistureBelow (0-100)
 *   Bits 29-35 : moistureTarget (0-100)
 *   Bits 36-47 : minGapMinutes (0-4095)
 *   Bits 48-60 : reserved
 *   Bits 61-73 : maxVolumeMl (0-8191 ml)
 *   Bits 74-80 : runoffHold5s (0-120)       [5-second ticks]
 *   Bits 81-88 : flags (see FeedSlotFlags)
 *
 * Fields can span byte boundaries; packing/unpacking writes/reads individual
 * bits so split fields are handled automatically.
 */
void packFeedSlot(uint8_t out[FEED_SLOT_PACKED_SIZE], const FeedSlot *slot);
void unpackFeedSlot(FeedSlot *slot, const uint8_t in[FEED_SLOT_PACKED_SIZE]);

#endif FEED_SLOTS_H
