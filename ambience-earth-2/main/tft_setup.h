#pragma once

// TFT_eSPI auto-includes this file via <tft_setup.h> to pick up the display
// wiring and driver for the ESP32-S3 + ILI9341 SPI panel.
#define USER_SETUP_LOADED
#define ILI9341_DRIVER

#define USE_FSPI_PORT

#define TFT_MOSI 11
#define TFT_MISO 13
#define TFT_SCLK 12
#define TFT_CS 15
#define TFT_DC 2
#define TFT_RST 4

#define TFT_BL 6
#define TFT_BACKLIGHT_ON HIGH

#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_FONT7
#define LOAD_FONT8
#define LOAD_GFXFF

#define SMOOTH_FONT

#define SPI_FREQUENCY 40000000
#define SPI_READ_FREQUENCY 20000000
