#!/usr/bin/env bash
set -euo pipefail

echo "[1/3] Installing Arduino AVR core..."
arduino-cli core update-index
arduino-cli core install arduino:avr

echo "[2/3] Building classic Ambience Earth simulation firmware..."
arduino-cli compile --fqbn arduino:avr:mega -e main --build-property build.extra_flags=-DWOKWI_SIM

echo "[3/3] Done. For ESP32-S3 variant run:"
echo "  arduino-cli core install esp32:esp32"
echo "  (cd ambience-earth-2 && ./tools/compile-sim.sh)"
