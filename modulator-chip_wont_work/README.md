# Modulator Chip

Custom [Wokwi](https://wokwi.com/) chip that applies a slow oscillation to an
analog input signal and outputs the modulated voltage.

The source code lives in [src/main.c](src/main.c), and the pin list is defined
in [chip.json](chip.json).

Behavior:

- Sine wave oscillation
- Period: random per cycle between 10-20 seconds
- Depth: random per cycle between 10%-15% of the current input value
- Output clamped to 0-5V
- VCC low forces output to 0V

## Building

Open the dev container and run `make`.

## Testing

Use the included [diagram.json](diagram.json) and [wokwi.toml](wokwi.toml).
The test sketch [test/modulator_test/modulator_test.ino](test/modulator_test/modulator_test.ino) prints the raw and
modulated values over Serial. Build the test firmware with `make test`.

## License

This project is licensed under the MIT license. See [LICENSE](LICENSE).
