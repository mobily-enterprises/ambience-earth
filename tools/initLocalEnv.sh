#!/bin/bash
set -euo pipefail

# ESP32 + TFT demo dependencies.
arduino-cli core update-index
arduino-cli core install esp32:esp32
arduino-cli lib install "Adafruit FT6206 Library" "TFT_eSPI"
arduino-cli lib install lvgl
