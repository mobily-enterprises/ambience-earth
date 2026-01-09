#!/bin/bash
set -e

bash tools/build-eeprom-chip.sh
arduino-cli compile --fqbn arduino:avr:mega -e main --build-property build.extra_flags=-DWOKWI_SIM
