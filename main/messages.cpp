#include "messages.h"
#include <Arduino.h>
#include <Wire.h>

uint16_t msgOffset(MsgId id) {
  if (id >= MSG_COUNT) return 0;
  return pgm_read_word(&kMsgOffsets[static_cast<uint8_t>(id)]);
}

char msgReadByte(uint16_t address) {
  Wire.beginTransmission(EXT_EEPROM_ADDR);
  Wire.write(static_cast<uint8_t>(address >> 8));
  Wire.write(static_cast<uint8_t>(address & 0xFF));
  if (Wire.endTransmission() != 0) return '\0';
  if (Wire.requestFrom(static_cast<uint8_t>(EXT_EEPROM_ADDR), static_cast<uint8_t>(1)) != 1) return '\0';
  if (!Wire.available()) return '\0';
  return static_cast<char>(Wire.read());
}
