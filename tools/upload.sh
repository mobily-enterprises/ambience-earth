#!/bin/bash

for i in 0 1 2 3 4; do
  PORT="/dev/ttyUSB$i"
  echo "Trying $PORT..." >&2
  if arduino-cli upload -b arduino:avr:nano -p "$PORT" main; then
    echo "Success on $PORT" >&2
    exit 0
  fi
done

echo "Failed on /dev/ttyUSB0-4" >&2
exit 1
