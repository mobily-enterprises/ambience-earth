# Use the Optiboot/UNO target for more flash headroom (matches sim build).
# Previous Nano target: arduino-cli compile --fqbn arduino:avr:nano:cpu=atmega328 -e main
arduino-cli compile --fqbn arduino:avr:uno -e main
