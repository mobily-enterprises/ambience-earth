#ifndef LIQUIDCRYSTAL_I2C_H
#define LIQUIDCRYSTAL_I2C_H

#include <Arduino.h>
#include <Print.h>

class LiquidCrystal_I2C : public Print {
public:
  LiquidCrystal_I2C(uint8_t address, uint8_t cols, uint8_t rows);

  void init();
  void begin(uint8_t cols, uint8_t rows);
  void clear();
  void home();
  void setCursor(uint8_t col, uint8_t row);
  void backlight();
  void noBacklight();
  void createChar(uint8_t location, const uint8_t charmap[]);
  bool isOk() const { return _ok; }
  virtual size_t write(uint8_t value);
  using Print::write;

private:
  void command(uint8_t value);
  void send(uint8_t value, uint8_t mode);
  void write4bits(uint8_t value);
  bool expanderWrite(uint8_t data);
  bool pulseEnable(uint8_t data);
  void setRowOffsets(uint8_t row0, uint8_t row1, uint8_t row2, uint8_t row3);

  uint8_t _addr;
  uint8_t _cols;
  uint8_t _rows;
  uint8_t _backlightMask;
  uint8_t _displayControl;
  uint8_t _displayMode;
  uint8_t _displayFunction;
  uint8_t _rowOffsets[4];
  bool _ok;
};

#endif
