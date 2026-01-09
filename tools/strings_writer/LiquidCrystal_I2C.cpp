#include "LiquidCrystal_I2C.h"

#include <Wire.h>

static const uint8_t kMaskRs = 0x01;
static const uint8_t kMaskEn = 0x04;
static const uint8_t kMaskBacklight = 0x08;

static const uint8_t LCD_CLEARDISPLAY = 0x01;
static const uint8_t LCD_RETURNHOME = 0x02;
static const uint8_t LCD_ENTRYMODESET = 0x04;
static const uint8_t LCD_DISPLAYCONTROL = 0x08;
static const uint8_t LCD_FUNCTIONSET = 0x20;
static const uint8_t LCD_SETCGRAMADDR = 0x40;
static const uint8_t LCD_SETDDRAMADDR = 0x80;

static const uint8_t LCD_ENTRYLEFT = 0x02;
static const uint8_t LCD_ENTRYSHIFTDECREMENT = 0x00;
static const uint8_t LCD_DISPLAYON = 0x04;
static const uint8_t LCD_4BITMODE = 0x00;
static const uint8_t LCD_1LINE = 0x00;
static const uint8_t LCD_2LINE = 0x08;
static const uint8_t LCD_5x8DOTS = 0x00;

LiquidCrystal_I2C::LiquidCrystal_I2C(uint8_t address, uint8_t cols, uint8_t rows)
    : _addr(address),
      _cols(cols),
      _rows(rows),
      _backlightMask(kMaskBacklight),
      _displayControl(0),
      _displayMode(0),
      _displayFunction(0),
      _ok(true) {
  setRowOffsets(0x00, 0x40, 0x14, 0x54);
}

void LiquidCrystal_I2C::init() {
  begin(_cols, _rows);
}

void LiquidCrystal_I2C::begin(uint8_t cols, uint8_t rows) {
  _cols = cols;
  _rows = rows;

  Wire.begin();

  _displayFunction = LCD_4BITMODE | LCD_1LINE | LCD_5x8DOTS;
  if (rows > 1) _displayFunction |= LCD_2LINE;

  setRowOffsets(0x00, 0x40, 0x14, 0x54);

  delay(50);

  expanderWrite(_backlightMask);
  delay(100);

  // 4-bit initialization sequence.
  write4bits(0x30);
  delayMicroseconds(4500);
  write4bits(0x30);
  delayMicroseconds(4500);
  write4bits(0x30);
  delayMicroseconds(150);
  write4bits(0x20);

  command(LCD_FUNCTIONSET | _displayFunction);

  _displayControl = LCD_DISPLAYON;
  command(LCD_DISPLAYCONTROL | _displayControl);

  clear();

  _displayMode = LCD_ENTRYLEFT | LCD_ENTRYSHIFTDECREMENT;
  command(LCD_ENTRYMODESET | _displayMode);
}

void LiquidCrystal_I2C::clear() {
  command(LCD_CLEARDISPLAY);
  delayMicroseconds(2000);
}

void LiquidCrystal_I2C::home() {
  command(LCD_RETURNHOME);
  delayMicroseconds(2000);
}

void LiquidCrystal_I2C::setCursor(uint8_t col, uint8_t row) {
  if (row >= _rows) row = _rows ? (_rows - 1) : 0;
  command(LCD_SETDDRAMADDR | (col + _rowOffsets[row]));
}

void LiquidCrystal_I2C::backlight() {
  _backlightMask = kMaskBacklight;
  expanderWrite(0);
}

void LiquidCrystal_I2C::noBacklight() {
  _backlightMask = 0;
  expanderWrite(0);
}

void LiquidCrystal_I2C::createChar(uint8_t location, const uint8_t charmap[]) {
  location &= 0x7;
  command(LCD_SETCGRAMADDR | (location << 3));
  for (uint8_t i = 0; i < 8; i++) {
    write(charmap[i]);
  }
}

size_t LiquidCrystal_I2C::write(uint8_t value) {
  send(value, kMaskRs);
  return 1;
}

void LiquidCrystal_I2C::command(uint8_t value) {
  send(value, 0);
}

void LiquidCrystal_I2C::send(uint8_t value, uint8_t mode) {
  uint8_t high = (value & 0xF0) | mode;
  uint8_t low = ((value << 4) & 0xF0) | mode;
  write4bits(high);
  write4bits(low);
}

void LiquidCrystal_I2C::write4bits(uint8_t value) {
  if (!expanderWrite(value)) return;
  pulseEnable(value);
}

bool LiquidCrystal_I2C::expanderWrite(uint8_t data) {
  for (uint8_t attempt = 0; attempt < 2; ++attempt) {
    Wire.beginTransmission(_addr);
    Wire.write(data | _backlightMask);
    if (Wire.endTransmission() == 0) {
      _ok = true;
      return true;
    }
    delay(1);
  }
  _ok = false;
  return false;
}

bool LiquidCrystal_I2C::pulseEnable(uint8_t data) {
  if (!expanderWrite(data | kMaskEn)) return false;
  delayMicroseconds(1);
  if (!expanderWrite(data & (uint8_t)~kMaskEn)) return false;
  delayMicroseconds(50);
  return true;
}

void LiquidCrystal_I2C::setRowOffsets(uint8_t row0, uint8_t row1, uint8_t row2, uint8_t row3) {
  _rowOffsets[0] = row0;
  _rowOffsets[1] = row1;
  _rowOffsets[2] = row2;
  _rowOffsets[3] = row3;
}
