#include <Arduino.h>
#include <Wire.h>
#include <avr/pgmspace.h>
#include "LiquidCrystal_I2C.h"

#define EEPROM_ADDR 0x50
#define WRITE_DELAY_MS 5

enum MsgId : uint8_t {
#define X(name, str) name,
#include "messages_def.h"
#undef X
  MSG_COUNT
};

#define X(name, str) static const char msg_##name[] PROGMEM = str;
#include "messages_def.h"
#undef X

const char *const kMessages[] PROGMEM = {
#define X(name, str) msg_##name,
#include "messages_def.h"
#undef X
};

static bool eepromReadByte(uint16_t addr, uint8_t *out) {
  Wire.beginTransmission(EEPROM_ADDR);
  Wire.write(static_cast<uint8_t>(addr >> 8));
  Wire.write(static_cast<uint8_t>(addr & 0xFF));
  if (Wire.endTransmission() != 0) return false;
  if (Wire.requestFrom(static_cast<uint8_t>(EEPROM_ADDR), static_cast<uint8_t>(1)) != 1) return false;
  if (!Wire.available()) return false;
  *out = static_cast<uint8_t>(Wire.read());
  return true;
}

static bool eepromWriteByte(uint16_t addr, uint8_t value) {
  Wire.beginTransmission(EEPROM_ADDR);
  Wire.write(static_cast<uint8_t>(addr >> 8));
  Wire.write(static_cast<uint8_t>(addr & 0xFF));
  Wire.write(value);
  if (Wire.endTransmission() != 0) return false;
  delay(WRITE_DELAY_MS);
  uint8_t readBack = 0;
  if (!eepromReadByte(addr, &readBack)) return false;
  return readBack == value;
}

static bool writeMessages() {
  uint16_t addr = 0;
  for (uint8_t i = 0; i < MSG_COUNT; ++i) {
    const char *p = reinterpret_cast<const char *>(pgm_read_ptr(&kMessages[i]));
    while (true) {
      char c = pgm_read_byte(p++);
      if (!eepromWriteByte(addr++, static_cast<uint8_t>(c))) return false;
      if (!c) break;
    }
  }

  Serial.print("Bytes written: ");
  Serial.println(addr);
  return true;
}

LiquidCrystal_I2C lcd = LiquidCrystal_I2C(0x27, 20, 4);

void setup() {
  Serial.begin(9600);
  Wire.begin();
  delay(100);
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Writing...");
  bool ok = writeMessages();
  lcd.clear();
  lcd.setCursor(0, 0);
  if (ok) {
    lcd.print("Done!");
    Serial.println("Done.");
  } else {
    lcd.print("EEPROM ERROR");
    lcd.setCursor(0, 1);
    lcd.print("Check address");
    Serial.println("EEPROM write failed.");
  }
}

void loop() {}
