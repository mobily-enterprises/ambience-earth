// For information and examples see:
// https://link.wokwi.com/custom-chips-alpha

#include "wokwi-api.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

typedef struct {
  pin_t pin_in;
  pin_t pin_out;
  pin_t pin_vcc;
  timer_t timer;
  float phase;
  float depth;
  float period;
  float hold_remaining;
  uint32_t rng;
  int last_millivolts;
} chip_state_t;

static const float kPi = 3.1415927f;
static const float kTwoPi = 6.2831853f;
static const float kHalfPi = 1.5707963f;
static const float kThreeHalfPi = 4.7123890f;
static const float kTickSeconds = 0.05f;

static uint32_t lcg_next(uint32_t *state) {
  *state = (*state * 1664525u) + 1013904223u;
  return *state;
}

static float rand_unit(chip_state_t *chip) {
  return (lcg_next(&chip->rng) & 0x00FFFFFFu) / 16777216.0f;
}

static void choose_cycle(chip_state_t *chip) {
  chip->period = 16.0f + rand_unit(chip) * 12.0f;
  chip->depth = 0.01f + rand_unit(chip) * 0.005f;
}

static void choose_hold(chip_state_t *chip) {
  chip->hold_remaining = 0.75f + rand_unit(chip) * 0.5f;
}

static float wrap_pi(float value) {
  while (value > kPi) value -= kTwoPi;
  while (value < -kPi) value += kTwoPi;
  return value;
}

static float absf(float value) {
  return value < 0.0f ? -value : value;
}

static float sin_approx(float value) {
  float x = wrap_pi(value);
  const float B = 4.0f / kPi;
  const float C = -4.0f / (kPi * kPi);
  float y = B * x + C * x * absf(x);
  const float P = 0.225f;
  return P * (y * absf(y) - y) + y;
}

static void update_output(void *user_data) {
  chip_state_t *chip = (chip_state_t *)user_data;
  if (chip->pin_vcc != NO_PIN && pin_read(chip->pin_vcc) == LOW) {
    pin_dac_write(chip->pin_out, 0.0f);
    return;
  }
  float volts = pin_adc_read(chip->pin_in);
  if (volts < 0.0f) volts = 0.0f;
  if (volts > 5.0f) volts = 5.0f;

  float sine = sin_approx(chip->phase);
  if (sine > 1.0f) sine = 1.0f;
  if (sine < -1.0f) sine = -1.0f;
  float modulated = volts + (volts * chip->depth * sine);
  if (modulated < 0.0f) modulated = 0.0f;
  if (modulated > 5.0f) modulated = 5.0f;

  int millivolts = (int)(volts * 1000.0f + 0.5f);
  if (millivolts != chip->last_millivolts) {
    printf("in=%.3fV\n", volts);
    chip->last_millivolts = millivolts;
  }

  pin_dac_write(chip->pin_out, modulated);

  if (chip->hold_remaining > 0.0f) {
    chip->hold_remaining -= kTickSeconds;
    if (chip->hold_remaining < 0.0f) chip->hold_remaining = 0.0f;
    return;
  }

  float next_phase = chip->phase + (kTwoPi * kTickSeconds) / chip->period;
  if (next_phase >= kTwoPi) {
    next_phase -= kTwoPi;
    choose_cycle(chip);
  }

  if (chip->phase < kHalfPi && next_phase >= kHalfPi) {
    chip->phase = kHalfPi;
    choose_hold(chip);
    return;
  }
  if (chip->phase < kThreeHalfPi && next_phase >= kThreeHalfPi) {
    chip->phase = kThreeHalfPi;
    choose_hold(chip);
    return;
  }

  chip->phase = next_phase;
}

void chip_init(void) {
  chip_state_t *chip = malloc(sizeof(chip_state_t));
  if (!chip) return;
  chip->pin_in = pin_init("IN", ANALOG);
  chip->pin_out = pin_init("OUT", ANALOG);
  chip->pin_vcc = pin_init("VCC", INPUT);
  chip->phase = 0.0f;
  chip->hold_remaining = 0.0f;
  chip->rng = (uint32_t)get_sim_nanos();
  if (chip->rng == 0) chip->rng = 1;
  chip->last_millivolts = -1;
  choose_cycle(chip);

  const timer_config_t timer_config = {
    .callback = update_output,
    .user_data = chip,
  };
  chip->timer = timer_init(&timer_config);
  timer_start(chip->timer, 50000, true);

  update_output(chip);
}
