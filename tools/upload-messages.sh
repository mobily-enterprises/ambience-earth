#!/bin/bash
set -e

echo "Writing..." >&2

for i in 0 1 2 3 4; do
  PORT="/dev/ttyUSB$i"
  echo "Trying $PORT..." >&2
  if arduino-cli upload -b arduino:avr:nano -p "$PORT" tools/strings_writer; then
    echo "Done!" >&2
    exit 0
  fi
done

echo "Failed on /dev/ttyUSB0-4" >&2
exit 1
