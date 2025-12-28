#include <string.h>
#include "feedSlots.h"

namespace {
constexpr uint16_t kBitStartMinute = 0;
constexpr uint16_t kBitEndMinute = kBitStartMinute + 11;
constexpr uint16_t kBitMoistureBelow = kBitEndMinute + 11;
constexpr uint16_t kBitMoistureTarget = kBitMoistureBelow + 7;
constexpr uint16_t kBitMinSince = kBitMoistureTarget + 7;
constexpr uint16_t kBitMinRuntime = kBitMinSince + 12;
constexpr uint16_t kBitMaxRuntime = kBitMinRuntime + 6;
constexpr uint16_t kBitPulseOn = kBitMaxRuntime + 7;
constexpr uint16_t kBitPulseOff = kBitPulseOn + 4;
constexpr uint16_t kBitFlags = kBitPulseOff + 7;

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
  writeBits(out, kBitStartMinute, 11, slot->startMinute);
  writeBits(out, kBitEndMinute, 11, slot->endMinute);
  writeBits(out, kBitMoistureBelow, 7, slot->moistureBelow);
  writeBits(out, kBitMoistureTarget, 7, slot->moistureTarget);
  writeBits(out, kBitMinSince, 12, slot->minSinceLastMinutes);
  writeBits(out, kBitMinRuntime, 6, slot->minRuntime5s);
  writeBits(out, kBitMaxRuntime, 7, slot->maxRuntime5s);
  writeBits(out, kBitPulseOn, 4, slot->pulseOn5s);
  writeBits(out, kBitPulseOff, 7, slot->pulseOff5s);
  writeBits(out, kBitFlags, 8, slot->flags);
}

void unpackFeedSlot(FeedSlot *slot, const uint8_t in[FEED_SLOT_PACKED_SIZE]) {
  if (!slot || !in) return;

  slot->startMinute = static_cast<uint16_t>(readBits(in, kBitStartMinute, 11));
  slot->endMinute = static_cast<uint16_t>(readBits(in, kBitEndMinute, 11));
  slot->moistureBelow = static_cast<uint8_t>(readBits(in, kBitMoistureBelow, 7));
  slot->moistureTarget = static_cast<uint8_t>(readBits(in, kBitMoistureTarget, 7));
  slot->minSinceLastMinutes = static_cast<uint16_t>(readBits(in, kBitMinSince, 12));
  slot->minRuntime5s = static_cast<uint8_t>(readBits(in, kBitMinRuntime, 6));
  slot->maxRuntime5s = static_cast<uint8_t>(readBits(in, kBitMaxRuntime, 7));
  slot->pulseOn5s = static_cast<uint8_t>(readBits(in, kBitPulseOn, 4));
  slot->pulseOff5s = static_cast<uint8_t>(readBits(in, kBitPulseOff, 7));
  slot->flags = static_cast<uint8_t>(readBits(in, kBitFlags, 8));
}
