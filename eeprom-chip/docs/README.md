# I2C EEPROM (Simulation)

Simple I2C EEPROM used by the simulator to serve message strings at address `0x57`.

## Build

- Run `tools/compile-messages.sh` after changing strings.
- Run `tools/compile-sim.sh` from the repo root; it rebuilds this chip (via devcontainer/Docker) before compiling the simulator firmware.
