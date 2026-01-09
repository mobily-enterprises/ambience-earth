# KeyLadder Chip

Custom [Wokwi](https://wokwi.com/) chip that converts five momentary buttons into
a single analog ladder output.

The source code lives in [src/main.c](src/main.c), and the pin list is defined
in [chip.json](chip.json).

Output mapping (ADC @ 5V):

- A = 900
- F = 700
- E = 500
- X = 300
- D = 100
- none = 1023

When multiple buttons are held, priority is A -> F -> E -> X -> D.

## Building

Open the dev container and run `make`.

## Testing

Use the included [diagram.json](diagram.json) and [wokwi.toml](wokwi.toml).
The test sketch [test/keyladder_test/keyladder_test.ino](test/keyladder_test/keyladder_test.ino) prints the analog
value and detected button over Serial. Build the test firmware with `make test`.

## License

This project is licensed under the MIT license. See [LICENSE](LICENSE).
