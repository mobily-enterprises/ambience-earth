#include <Arduino.h>
#include <Wire.h>
#include <avr/pgmspace.h>
#include "LiquidCrystal_I2C.h"

#define EEPROM_ADDR 0x57
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

static void eepromWriteByte(uint16_t addr, uint8_t value) {
  Wire.beginTransmission(EEPROM_ADDR);
  Wire.write(static_cast<uint8_t>(addr >> 8));
  Wire.write(static_cast<uint8_t>(addr & 0xFF));
  Wire.write(value);
  Wire.endTransmission();
  delay(WRITE_DELAY_MS);
}

static void writeMessages() {
  uint16_t addr = 0;
  for (uint8_t i = 0; i < MSG_COUNT; ++i) {
    const char *p = reinterpret_cast<const char *>(pgm_read_ptr(&kMessages[i]));
    while (true) {
      char c = pgm_read_byte(p++);
      eepromWriteByte(addr++, static_cast<uint8_t>(c));
      if (!c) break;
    }
  }

  Serial.print("Bytes written: ");
  Serial.println(addr);
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
  writeMessages();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Done!");
  Serial.println("Done.");
}

void loop() {}
