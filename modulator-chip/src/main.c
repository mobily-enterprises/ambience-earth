// For information and examples see:
// https://link.wokwi.com/custom-chips-alpha

#include "wokwi-api.h"
#include <stdio.h>
#include <stdlib.h>

typedef struct {
  pin_t pin_in;
  pin_t pin_a;
  pin_t pin_f;
  pin_t pin_e;
  pin_t pin_x;
  pin_t pin_d;
  pin_t pin_out;
  timer_t timer;
} chip_state_t;

static void update_output(void *user_data) {
  chip_state_t *chip = (chip_state_t *)user_data;
  float volts = pin_adc_read(chip->pin_in);
  if (volts < 0.0f) volts = 0.0f;
  if (volts > 5.0f) volts = 5.0f;

  static int last_millivolts = -1;
  int millivolts = (int)(volts * 1000.0f + 0.5f);
  if (millivolts != last_millivolts) {
    printf("in=%.3fV\n", volts);
    last_millivolts = millivolts;
  }

  pin_dac_write(chip->pin_out, volts);
}

static void chip_pin_change(void *user_data, pin_t pin, uint32_t value) {
  chip_state_t *chip = (chip_state_t*)user_data;
  printf("Pin change: %d %d\n", pin, value);
  update_output(chip);
}

void chip_init(void) {
  chip_state_t *chip = malloc(sizeof(chip_state_t));
  chip->pin_in = pin_init("IN", ANALOG);
  chip->pin_a = pin_init("A", INPUT_PULLUP);
  chip->pin_f = pin_init("F", INPUT_PULLUP);
  chip->pin_e = pin_init("E", INPUT_PULLUP);
  chip->pin_x = pin_init("X", INPUT_PULLUP);
  chip->pin_d = pin_init("D", INPUT_PULLUP);
  chip->pin_out = pin_init("OUT", ANALOG);

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
  pin_watch(chip->pin_in, &config);

  const timer_config_t timer_config = {
    .callback = update_output,
    .user_data = chip,
  };
  chip->timer = timer_init(&timer_config);
  timer_start(chip->timer, 50000, true);

  update_output(chip);
}
