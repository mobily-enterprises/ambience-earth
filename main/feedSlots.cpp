#include <string.h>
#include "feedSlots.h"

namespace {
constexpr uint16_t kBitWindowStart = 0;
constexpr uint16_t kBitWindowDuration = kBitWindowStart + 11;
constexpr uint16_t kBitMoistureBelow = kBitWindowDuration + 11;
constexpr uint16_t kBitMoistureTarget = kBitMoistureBelow + 7;
constexpr uint16_t kBitMinSince = kBitMoistureTarget + 7;
constexpr uint16_t kBitMinVolume = kBitMinSince + 12;
constexpr uint16_t kBitMaxVolume = kBitMinVolume + 13;
constexpr uint16_t kBitRunoffHold = kBitMaxVolume + 13;
constexpr uint16_t kBitFlags = kBitRunoffHold + 7;

static void writeBits(uint8_t *buffer, uint16_t bitOffset, uint8_t bitCount, uint32_t value) {
  for (uint8_t i = 0; i < bitCount; ++i) {
    uint16_t bitIndex = bitOffset + i;
    uint8_t byteIndex = bitIndex >> 3;
    uint8_t bitInByte = bitIndex & 0x7;
    uint8_t mask = static_cast<uint8_t>(1u << bitInByte);
    if (value & (1UL << i)) buffer[byteIndex] |= mask;
    else buffer[byteIndex] &= static_cast<uint8_t>(~mask);
  }
}

static uint32_t readBits(const uint8_t *buffer, uint16_t bitOffset, uint8_t bitCount) {
  uint32_t value = 0;
  for (uint8_t i = 0; i < bitCount; ++i) {
    uint16_t bitIndex = bitOffset + i;
    uint8_t byteIndex = bitIndex >> 3;
    uint8_t bitInByte = bitIndex & 0x7;
    if (buffer[byteIndex] & (1u << bitInByte)) value |= (1UL << i);
  }
  return value;
}
}

void packFeedSlot(uint8_t out[FEED_SLOT_PACKED_SIZE], const FeedSlot *slot) {
  if (!out || !slot) return;

  memset(out, 0, FEED_SLOT_PACKED_SIZE);
  writeBits(out, kBitWindowStart, 11, slot->windowStartMinutes);
  writeBits(out, kBitWindowDuration, 11, slot->windowDurationMinutes);
  writeBits(out, kBitMoistureBelow, 7, slot->moistureBelow);
  writeBits(out, kBitMoistureTarget, 7, slot->moistureTarget);
  writeBits(out, kBitMinSince, 12, slot->minGapMinutes);
  writeBits(out, kBitMaxVolume, 13, slot->maxVolumeMl);
  writeBits(out, kBitRunoffHold, 7, slot->runoffHold5s);
  writeBits(out, kBitFlags, 8, slot->flags);
}

void unpackFeedSlot(FeedSlot *slot, const uint8_t in[FEED_SLOT_PACKED_SIZE]) {
  if (!slot || !in) return;

  slot->windowStartMinutes = static_cast<uint16_t>(readBits(in, kBitWindowStart, 11));
  slot->windowDurationMinutes = static_cast<uint16_t>(readBits(in, kBitWindowDuration, 11));
  slot->moistureBelow = static_cast<uint8_t>(readBits(in, kBitMoistureBelow, 7));
  slot->moistureTarget = static_cast<uint8_t>(readBits(in, kBitMoistureTarget, 7));
  slot->minGapMinutes = static_cast<uint16_t>(readBits(in, kBitMinSince, 12));
  slot->maxVolumeMl = static_cast<uint16_t>(readBits(in, kBitMaxVolume, 13));
  slot->runoffHold5s = static_cast<uint8_t>(readBits(in, kBitRunoffHold, 7));
  slot->flags = static_cast<uint8_t>(readBits(in, kBitFlags, 8));
}
