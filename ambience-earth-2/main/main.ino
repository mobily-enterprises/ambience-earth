#include <Arduino.h>

#include "logs.h"
#include "platform_display.h"
#include "sim.h"
#include "ui_flow.h"

/*
 * setup
 * Arduino entry point that initializes display, simulation, and UI.
 * Example:
 *   // Called automatically by the Arduino core on boot.
 */
void setup() {
  lv_display_t *disp = platform_display_init();
  randomSeed(micros());
  logs_init();
  sim_init();
  build_ui();
  lv_refr_now(disp);
}

/*
 * loop
 * Arduino main loop that runs the LVGL task handler.
 * Example:
 *   // Called automatically by the Arduino core after setup().
 */
void loop() {
  uint32_t delayMs = lv_timer_handler();
  if (delayMs < 5) delayMs = 5;
  else if (delayMs > 20) delayMs = 20;
  delay(delayMs);
}
