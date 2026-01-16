# Ambience 2.0 (ESP32-S3 + ILI9341 cap-touch LVGL demo)

This sketch is a minimal LVGL touch test using the FT6206 controller on
Wokwi's ESP32-S3 + ILI9341 capacitive touch board.

Files:
- main/main.ino: LVGL touch test using Adafruit ILI9341 + FT6206
- diagram.json: Wokwi wiring for ESP32-S3 DevKitC-1 + ILI9341 cap-touch board
- libraries.txt: Wokwi library dependencies
- main/: all source files (C/C++ + headers) and build output in main/build

Notes:
- Display SPI pins: SCK=12, MOSI=11, MISO=13, CS=15, DC=2, RST=4
- Touch uses I2C: SDA=10, SCL=8
- Backlight is on GPIO 6
