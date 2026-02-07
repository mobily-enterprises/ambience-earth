# Ambience Earth 2 Install + Build

These steps install the Arduino CLI dependencies needed to compile the
Ambience Earth 2 firmware (ESP32-S3 + LVGL), then build the sim firmware.

## 1) Install Arduino CLI (if needed)

If you don’t already have `arduino-cli`, install it via your preferred method.

## 2) Configure the ESP32 core

Run these once to add the ESP32 board manager URL and update the index:

```bash
arduino-cli config init
arduino-cli config set board_manager.additional_urls https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
arduino-cli core update-index
```

## 3) Install the ESP32 core

If your download tends to time out, set a longer connection timeout first:

```bash
arduino-cli config set network.connection_timeout 600s
```

```bash
arduino-cli core install esp32:esp32
```

## 4) Install required libraries

```bash
arduino-cli lib install "Adafruit ILI9341" "Adafruit FT6206 Library" "lvgl"
```

## 5) Build the sim firmware

Run from the `ambience-earth-2` directory so paths resolve correctly:

```bash
cd /home/claw/Development/ambience-earth/ambience-earth-2
./tools/compile-sim.sh
```

If the build fails, re-run the failing command and capture the error output.
