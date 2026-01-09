# ambience-earth

A feeder meant to be used for stack feeding based on humidity.

It will work with any moisture sensors, since the humidity baseline is reset every day. (The sensor needs to be reliable in terms of providing meaningful _relative_ readings).

It will fit in a Nano, although it's really pushing the space constraints (before the code becomes unreadable).

To save precious space, the program's strings are placed in the RTC's EEPROM and fetched when needed.

Arduino Nano is expected to have Optiboot.

![Wokwi wiring reference](docs/images/wokwi-schematic.png)

Manual: [docs/manual.md](docs/manual.md)

## Hardware overview (friendly, practical)

This project uses an Arduino Nano plus a few I2C modules and simple sensors. The links below are examples (not endorsements of specific vendors). Parts are interchangeable as long as they match the interface.

### Parts list (example links, not endorsements)

- **Arduino Nano** (main MCU): [AliExpress listing](https://www.aliexpress.com/item/1005008858765129.htm)
- **I2C hub** (fan-out the I2C bus): [AliExpress listing](https://www.aliexpress.com/item/1005002811407142.html)
- **IO shield** (optional but makes wiring easier): [AliExpress listing](https://www.aliexpress.com/item/1005006191550486.html)
- **LCD 20x4 (I2C)** (main display): [AliExpress listing](https://www.aliexpress.com/item/1005008824610341.html)
- **RTC + EEPROM module** (timekeeping + strings storage): [AliExpress listing](https://www.aliexpress.com/item/32666603579.html)
- **Moisture sensor** (analog signal): [AliExpress listing](https://www.aliexpress.com/item/1005007953078865.html)
- **Buttons** (analog ladder keypad): [AliExpress listing](https://www.aliexpress.com/item/32952276299.html)
- **Relay module** (switches pump/solenoid power): [AliExpress listing](https://www.aliexpress.com/item/10000334999223.html)
- **Flyback diode (1N4007)** (across pump/solenoid): [AliExpress listing](https://www.aliexpress.com/item/1005009562512822.html)
- **Pump** (water delivery): [AliExpress listing](https://www.aliexpress.com/item/1005006318276743.html)
- **Solenoid** (valve, wired in parallel with the pump): [AliExpress listing](https://www.aliexpress.com/item/1005005553905520.html)

### Cables

- Short F2F 4-pin I2C cables (hub <-> RTC/LCD): [AliExpress listing](https://www.aliexpress.com/item/1005006190567526.html)
- Longer F2F 4-pin I2C cables (LCD reach): [AliExpress listing](https://www.aliexpress.com/item/1005005996835448.html)

### Build guide (Nano pin map + wiring tips)

Friendly rule of thumb: **power everything off the Nano 5V/GND, and use A4/A5 for I2C**.

- **Power for pump/solenoid**
  - Use a dedicated 6V supply for the pump and solenoid.
  - Feed that 6V into **VIN** on the Nano so you have a common supply rail.
  - The IO shield does not expose VIN, so you'll need to solder a lead to the VIN hole on the pinout board.
  - Pick the **6V** version of the solenoid, and note that the Nano will take 6V on VIN and regulate it down to 5V with minimal thermal issues in this setup.

- **I2C hub -> Arduino Nano**
  - SDA -> A4
  - SCL -> A5
  - VCC -> 5V
  - GND -> GND

  Note that the pinout board above has a nearly laid out set of 4 pins so that you can just use the 4P dupont cable to connect it up.

- **LCD + RTC/EEPROM -> I2C hub**
  - Plug both into the I2C hub using the 4-pin cables.
  - Double-check the pin order on each module; some boards label pins in a different order.

- **Buttons (analog ladder board)**
  - OUT/SIG -> A7
  - VCC -> 5V
  - GND -> GND
  - Then run the on-device button calibration.

- **Moisture sensor**
  - Signal -> A0
  - VCC -> D12 (the firmware powers the sensor only when sampling)
  - GND -> GND

- **Runoff sensor (simple switch)**
  - Signal -> D5
  - Other side -> GND
  - The pin uses `INPUT_PULLUP`, so the switch pulls it to ground when active.

- **Pump + Solenoid**
  - Both are driven **in parallel** by the same output (D2).
  - Use the relay module to switch power:
    - Relay control side: connect the 3-pin cable to the Arduino (VCC, GND, IN to a digital pin).
    - Load side (switched power):
      - VIN -> Pump/Solenoid +
      - Pump/Solenoid - -> Relay -> any GND on the Arduino
  - Share grounds between the supply and the Nano.
  - Add a flyback diode directly across the pump/solenoid leads:
    - Stripe (cathode) -> +, other end (anode) -> -
    - This protects the relay contacts and the Arduino from voltage spikes.

### Also typically needed

- A suitable **pump/solenoid power supply** (voltage/current matched to your hardware).
- Flyback protection if your relay board does not include it (don't drive inductive loads directly from the Nano).
- **6mm tubing** to connect the solenoid to the pump (water input flows through the solenoid, output goes to the pump).
- Tubing fittings and a moisture probe extension cable if needed.

## Use in Simulation (Wokwi)

- Build message offsets + EEPROM image (required if `main/messages_def.h` changes): `tools/compile-messages.sh`
- Build the sim firmware (auto-rebuilds the custom EEPROM chip): `tools/compile-sim.sh`
- Open the Wokwi project using `wokwi.toml` + `diagram.json`

Notes:
- `tools/compile-sim.sh` calls `tools/build-eeprom-chip.sh`, which uses a local WASM toolchain if available, otherwise the devcontainer/Docker image.

## Use in a Real device (Arduino Nano)

- Bootloader (Optiboot):
  - Easiest: select the **new bootloader** Nano profile (Arduino IDE) or use FQBN `arduino:avr:nano:cpu=atmega328` (Arduino CLI).
  - If your core lacks that profile, edit `boards.txt` (typically `~/.arduino15/packages/arduino/hardware/avr/<version>/boards.txt`) to match Optiboot:
    - `nano.menu.cpu.atmega328.upload.maximum_size=32256`
    - `nano.menu.cpu.atmega328.upload.speed=115200`
    - `nano.menu.cpu.atmega328.bootloader.low_fuses=0xFF`
    - `nano.menu.cpu.atmega328.bootloader.high_fuses=0xDE`
    - `nano.menu.cpu.atmega328.bootloader.extended_fuses=0xFD`
    - `nano.menu.cpu.atmega328.bootloader.file=optiboot/optiboot_atmega328.hex`
    - Make sure any old `nano.menu.cpu.atmega328.bootloader.high_fuses=0xDA` line is commented out.
  - Then burn the bootloader (e.g. `tools/burnOptiBoot.sh`) before flashing firmware.
  - Wiring reference: [ArduinoISP wiring guide](https://docs.arduino.cc/built-in-examples/arduino-isp/ArduinoISP)

- If strings changed, rebuild + flash the external EEPROM:
  - `tools/compile-messages.sh`
  - `tools/upload-messages.sh`
- Build + upload firmware:
  - `tools/compile_upload.sh` (compiles `main` then uploads), or
  - `tools/compile.sh` and `tools/upload.sh` separately

`tools/upload.sh` and `tools/upload-messages.sh` will try `/dev/ttyUSB0-4` and stop on the first successful upload.
