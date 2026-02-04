#include <Arduino.h>

#include "logs.h"
#include "platform_display.h"
#include "sim.h"
#include "ui_flow.h"

// Reduce LVGL tick rate in Wokwi to avoid slow webview refresh.
#ifdef WOKWI_SIM
static const uint32_t kMinLoopDelayMs = 16;
static const uint32_t kMaxLoopDelayMs = 50;
#else
static const uint32_t kMinLoopDelayMs = 5;
static const uint32_t kMaxLoopDelayMs = 20;
#endif

/*
 * setup
 * Arduino entry point that initializes display, simulation, and UI.
 * Example:
 *   // Called automatically by the Arduino core on boot.
 */
void setup() {
  Serial.begin(115200);
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
  if (delayMs < kMinLoopDelayMs) delayMs = kMinLoopDelayMs;
  else if (delayMs > kMaxLoopDelayMs) delayMs = kMaxLoopDelayMs;
  delay(delayMs);
}
