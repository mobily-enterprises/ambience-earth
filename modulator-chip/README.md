# Modulator Chip

Custom [Wokwi](https://wokwi.com/) chip that applies a slow oscillation to an
analog input signal and outputs the modulated voltage.

The source code lives in [src/main.c](src/main.c), and the pin list is defined
in [chip.json](chip.json).

Behavior:

- Sine-wave modulation around the input value.
- Depth: random per cycle, ~2% to 3% peak-to-peak (1% to 1.5% each side).
- Period: random per cycle, 16 to 28 seconds for a full up/down cycle.
- Hold time: 0.75 to 1.25 seconds at the top and bottom of the oscillation.
- Output is clamped to 0-5V; VCC low forces output to 0V.

Wire VCC to a defined HIGH (5V or an enable pin). Leaving VCC unconnected can
float and disable the output. Tie VCC to 5V if you do not need enable control.

## Building

Open the dev container and run `make`.

## Testing

Use the included [diagram.json](diagram.json) and [wokwi.toml](wokwi.toml).
The test sketch [test/modulator_test/modulator_test.ino](test/modulator_test/modulator_test.ino) prints the raw input
value (A1) and modulated output (A0) over Serial. Build the test firmware with `make test`.

## License

This project is licensed under the MIT license. See [LICENSE](LICENSE).
