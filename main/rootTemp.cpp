#include "rootTemp.h"

// Minimal one-wire bit-bang for a single DS18B20 on ROOT_TEMP_PIN.
// This keeps footprint small and avoids pulling in an external library.

static void oneWireLow() {
  pinMode(ROOT_TEMP_PIN, OUTPUT);
  digitalWrite(ROOT_TEMP_PIN, LOW);
}

static void oneWireRelease() {
  pinMode(ROOT_TEMP_PIN, INPUT_PULLUP);
}

static void oneWireWriteBit(uint8_t bit) {
  oneWireLow();
  if (bit) {
    delayMicroseconds(6);
    oneWireRelease();
    delayMicroseconds(64);
  } else {
    delayMicroseconds(60);
    oneWireRelease();
    delayMicroseconds(10);
  }
}

static uint8_t oneWireReadBit() {
  uint8_t bit;
  oneWireLow();
  delayMicroseconds(6);
  oneWireRelease();
  delayMicroseconds(9);
  bit = digitalRead(ROOT_TEMP_PIN);
  delayMicroseconds(55);
  return bit;
}

static void oneWireWriteByte(uint8_t data) {
  for (uint8_t i = 0; i < 8; i++) {
    oneWireWriteBit(data & 0x01);
    data >>= 1;
  }
}

static uint8_t oneWireReadByte() {
  uint8_t data = 0;
  for (uint8_t i = 0; i < 8; i++) {
    data >>= 1;
    if (oneWireReadBit()) data |= 0x80;
  }
  return data;
}

static bool oneWireReset() {
  oneWireLow();
  delayMicroseconds(480);
  oneWireRelease();
  delayMicroseconds(70);
  bool presence = (digitalRead(ROOT_TEMP_PIN) == LOW);
  delayMicroseconds(410);
  return presence;
}

void initRootTempSensor() {
  oneWireRelease();
}

bool rootTempReadC10(int16_t *outC10) {
  if (!outC10) return false;

  if (!oneWireReset()) return false;       // no presence pulse
  oneWireWriteByte(0xCC);                  // Skip ROM (single sensor assumed)
  oneWireWriteByte(0x44);                  // Convert T
  delay(750);                              // Max conversion time at 12-bit

  if (!oneWireReset()) return false;
  oneWireWriteByte(0xCC);                  // Skip ROM
  oneWireWriteByte(0xBE);                  // Read scratchpad

  uint8_t tempL = oneWireReadByte();
  uint8_t tempH = oneWireReadByte();
  // Skip remaining scratchpad bytes to leave bus idle
  for (uint8_t i = 0; i < 7; i++) oneWireReadByte();

  int16_t raw = (int16_t)((tempH << 8) | tempL); // DS18B20 is 1/16 °C units
  int32_t c10 = ((int32_t)raw * 10 + 8) / 16;    // °C * 10 with rounding
  *outC10 = (int16_t)c10;
  return true;
}
