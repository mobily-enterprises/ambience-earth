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

- Sine-wave modulation around the input value.
- Depth: random per cycle, 5% to 7% of the current input value.
- Period: random per cycle, 20 to 35 seconds for a full up/down cycle.
- Hold time: 1.5 to 2.5 seconds at the top and bottom of the oscillation.
- Output is clamped to 0-5V; VCC low forces output to 0V.

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
