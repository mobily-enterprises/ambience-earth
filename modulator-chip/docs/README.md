# Modulator Chip

Custom chip for [Wokwi](https://wokwi.com/) that applies a slow oscillation to
an analog input signal and outputs the modulated voltage.

## Pin names

| Name | Description |
| ---- | ----------- |
| IN   | Analog input (0-5V) |
| OUT  | Analog output (0-5V) |
| VCC  | Enable (active-high) |
| GND  | Ground (optional) |

## Behavior

- Sine wave oscillation
- Period: random per cycle between 10-20 seconds
- Depth: random per cycle between 10%-15% of the current input value
- Output clamped to 0-5V
- VCC low forces output to 0V

## Usage

Add the chip to your `wokwi.toml`:

```toml
[[chip]]
name = "modulator"
binary = "dist/chip.wasm"
```

Then include it in your `diagram.json`:

```json
  { "type": "chip-modulator", "id": "mod1" }
```

For a complete example, see [diagram.json](../diagram.json) in this folder.
