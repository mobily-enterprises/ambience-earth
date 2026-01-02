# arduino-cli compile --fqbn arduino:avr:nano:cpu=atmega328 -e main --build-property build.extra_flags=-DWOKWI_SIM
# Use Optiboot/UNO target in sim to get ~2 KB extra flash for testing headroom.
arduino-cli compile --fqbn arduino:avr:uno -e main --build-property build.extra_flags=-DWOKWI_SIM
