#!/bin/bash
set -euo pipefail

# ESP32 + TFT demo dependencies.
arduino-cli core update-index
arduino-cli core install esp32:esp32
arduino-cli lib install "Adafruit GFX Library" "Adafruit ILI9341" "Adafruit FT6206 Library"
arduino-cli lib install lvgl
