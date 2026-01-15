#ifndef MESSAGES_H
#define MESSAGES_H

#include <stdint.h>
#include <avr/pgmspace.h>

enum MsgId : uint8_t {
#define X(name, str) name,
#include "messages_def.h"
#undef X
  MSG_COUNT
};

#define EXT_EEPROM_ADDR 0x50
#define EXT_EEPROM_SIZE 4096

extern const uint16_t kMsgOffsets[MSG_COUNT] PROGMEM;
extern const uint16_t kMsgDataSize;

uint16_t msgOffset(MsgId id);
char msgReadByte(uint16_t address);

#endif  // MESSAGES_H
