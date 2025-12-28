// For information and examples see:
// https://link.wokwi.com/custom-chips-alpha

#include "wokwi-api.h"
#include <stdio.h>
#include <stdlib.h>

typedef struct {
  pin_t pin_a;
  pin_t pin_f;
  pin_t pin_e;
  pin_t pin_x;
  pin_t pin_d;
  pin_t pin_out;
  int last_adc;
} chip_state_t;

static float adc_to_volts(int adc) {
  return (adc / 1023.0f) * 5.0f;
}

static bool is_pressed(pin_t pin) {
  return pin_read(pin) == LOW;
}

static void update_output(chip_state_t *chip) {
  int adc = 1023;

  if (is_pressed(chip->pin_a)) adc = 900;
  else if (is_pressed(chip->pin_f)) adc = 700;
  else if (is_pressed(chip->pin_e)) adc = 500;
  else if (is_pressed(chip->pin_x)) adc = 300;
  else if (is_pressed(chip->pin_d)) adc = 100;

  if (adc != chip->last_adc) {
    printf("adc=%d\n", adc);
    chip->last_adc = adc;
  }

  pin_dac_write(chip->pin_out, adc_to_volts(adc));
}

static void chip_pin_change(void *user_data, pin_t pin, uint32_t value) {
  chip_state_t *chip = (chip_state_t*)user_data;
  printf("Pin change: %d %d\n", pin, value);
  update_output(chip);
}

void chip_init(void) {
  chip_state_t *chip = malloc(sizeof(chip_state_t));
  chip->pin_a = pin_init("A", INPUT_PULLUP);
  chip->pin_f = pin_init("F", INPUT_PULLUP);
  chip->pin_e = pin_init("E", INPUT_PULLUP);
  chip->pin_x = pin_init("X", INPUT_PULLUP);
  chip->pin_d = pin_init("D", INPUT_PULLUP);
  chip->pin_out = pin_init("OUT", ANALOG);
  chip->last_adc = -1;

  const pin_watch_config_t config = {
    .edge = BOTH,
    .pin_change = chip_pin_change,
    .user_data = chip,
  };
  pin_watch(chip->pin_a, &config);
  pin_watch(chip->pin_f, &config);
  pin_watch(chip->pin_e, &config);
  pin_watch(chip->pin_x, &config);
  pin_watch(chip->pin_d, &config);

  update_output(chip);
}
