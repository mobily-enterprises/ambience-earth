# KeyLadder Chip

Custom chip for [Wokwi](https://wokwi.com/) that turns five momentary buttons
into a single analog ladder output.

## Pin names

| Name | Description |
| ---- | ----------- |
| A    | Button input (active-low) |
| F    | Button input (active-low) |
| E    | Button input (active-low) |
| X    | Button input (active-low) |
| D    | Button input (active-low) |
| OUT  | Analog output (0-5V) |
| VCC  | Supply (optional) |
| GND  | Ground (optional) |

## Output values

ADC values at 5V:

- A = 900
- F = 700
- E = 500
- X = 300
- D = 100
- none = 1023

If multiple buttons are pressed, the first match in the order above wins.

## Usage

Prebuilt `dist/chip.wasm` and `dist/chip.json` are committed so Wokwi runs out
of the box. Run `make` in this folder if you change the chip sources.

Add the chip to your `wokwi.toml`:

```toml
[[chip]]
name = "keyladder"
binary = "dist/chip.wasm"
```

Then include it in your `diagram.json`:

```json
  { "type": "chip-keyladder", "id": "keys" }
```

For a complete example, see [diagram.json](../diagram.json) in this folder.
