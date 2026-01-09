#!/bin/bash

FQBN="arduino:avr:nano:cpu=atmega328"
PROGRAMMER="arduinoasisp"

for i in 0 1 2 3 4; do
  PORT="/dev/ttyUSB$i"
  echo "Trying $PORT..." >&2
  if arduino-cli burn-bootloader -b "$FQBN" -P "$PROGRAMMER" -p "$PORT" -v; then
    echo "Success on $PORT" >&2
    exit 0
  fi
done

echo "Failed on /dev/ttyUSB0-4" >&2
exit 1
