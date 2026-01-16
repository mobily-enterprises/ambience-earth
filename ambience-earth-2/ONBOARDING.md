# Onboarding

This guide is for junior developers. It walks you through the codebase in a
specific order so each piece makes sense before you move on.

## 1) Start here: the Arduino entry point

Open `main/main.ino`. This is the smallest file and shows the runtime
shape of the project:

- `setup()` does three things: initialize the display, initialize the
  simulation, and build the UI.
- `loop()` runs the LVGL task handler continuously (this is how LVGL updates the
  screen and handles input).

You now know the top-level lifecycle: init -> build UI -> LVGL loop.

## 2) Understand the global state model

Open `main/app_state.h` and `main/app_state.cpp`. These define the data the UI is built
from and how it is stored.

What to focus on:

- Core constants (screen size, buffer lines, slot/log counts).
- App structs: `SimState`, `Slot`, `LogEntry`, and various `*Refs` for UI
  handles.
- Global instances: `g_sim`, `g_slots`, `g_logs`, `g_screen_stack`, and others.
- Pool helpers (`reset_binding_pool`, `alloc_binding`, etc.), which are used to
  bind UI controls to values without dynamic allocation.

This gives you the vocabulary used across the whole codebase.

## 3) See how the display and touch are wired

Open `main/platform_display.h` and `main/platform_display.cpp`.

Key ideas:

- `platform_display_init()` sets up SPI, I2C, the TFT, and the touch controller.
- `disp_flush()` is LVGL’s render callback; it pushes pixels to the ILI9341.
- `touch_read()` turns FT6206 touch points into LVGL input events.
- `lv_tick_set_cb(millis)` is required for LVGL v9 to process timers and touch.

This file is “hardware glue” — it connects LVGL to the display and touch.

## 4) Learn the simulation model

Open `main/sim.h` and `main/sim.cpp`.

What it does:

- `sim_init()` seeds fake data for the UI (time, moisture, slots, etc.).
- `sim_tick()` advances the simulation once per second.
- `sim_start_feed()` begins a simulated feeding cycle.

This is a **simulation only** layer. Nothing here touches real sensors.

## 5) Learn how logs are created

Open `main/logs.h` and `main/logs.cpp`.

What it does:

- `add_log()` inserts a log at the head of a fixed-size log list.
- `build_*_log()` creates specific log shapes for boot/values/feed.

The log viewer pulls from these entries; it does not create logs itself.

## 6) Understand reusable UI building blocks

Open `main/ui_components.h`.

This file contains reusable LVGL helpers such as:

- Menu list rows
- Number selector widgets
- Date/time input blocks
- Modal prompts

Each function has a doc comment with a practical usage example. Use this file
when you want to build a new screen without repeating LVGL boilerplate.

## 7) Learn the screen building layer

Open `main/ui_screens.h` and `main/ui_screens.cpp`. This file **builds and updates all
screens**, but it does not decide when to show them.

How it is structured:

- `build_*_screen()` functions build the LVGL object tree for a screen.
- `update_*()` functions refresh dynamic text (moisture, logs, calibration
  values).
- `build_screen(ScreenId)` is a simple screen factory.
- The slot wizard and time/date wizard live here because they are mostly UI.

Read screens in this order (from simplest to most complex):

1. `build_info_screen()` + `update_info_screen()`
2. `build_menu_screen()`
3. `build_logs_screen()` + `update_logs_screen()`
4. Feeding screens (`build_feeding_menu_screen`, slots list, slot summary)
5. Settings screens (time/date, calibrations, tests, reset)
6. Wizard screens (`wizard_render_step`, `time_date_render_step`)

This gives you the visual structure and the per-screen update logic.

## 8) Learn the navigation and event flow

Open `main/ui_flow.h` and `main/ui_flow.cpp`. This is the **navigation and event hub**.

What to focus on:

- `push_screen()` / `pop_screen()` implement a simple screen stack.
- `build_ui()` sets the LVGL theme, creates the first screen, and starts the
  UI timer.
- All button events live here (`open_*_event`, `toggle_pause_event`, etc.).
- `ui_timer_cb()` ticks the simulation and refreshes the UI.

Think of `ui_flow.cpp` as “what happens when you tap something.”

## 9) Understand how timers update the UI

Follow the data path:

1. `ui_timer_cb()` in `ui_flow.cpp` runs every ~200 ms.
2. It calls `sim_tick()` once per second.
3. It calls `update_screensaver()` and `update_active_screen()`.
4. Those functions in `ui_screens.cpp` update labels based on `g_sim`.

This is the main runtime loop for UI updates.

## 10) Where to add new features

Use these guidelines:

- **New screen layout**: add a `build_*_screen()` function in `main/ui_screens.cpp`.
- **Navigation to it**: add an `open_*_event()` in `main/ui_flow.cpp` and wire it to a
  button.
- **Dynamic data**: store it in `main/app_state.h` and update in `main/sim.cpp` (simulation
  only) or in real sensor code later.
- **New reusable UI pattern**: add a helper in `main/ui_components.h` with a usage
  example.

## 11) Build and run (simulation)

Use the provided script:

```
tools/compile-sim.sh
```

This uses the ESP32-S3 toolchain and produces a Wokwi-compatible binary.

## 12) Common gotchas

- LVGL v9 does **not** honor `LV_TICK_CUSTOM`. Always call
  `lv_tick_set_cb(millis)` in `platform_display_init()`.
- The UI uses fixed-size pools (`kBindingPoolSize`, `kOptionGroupSize`). If you
  add many new widgets, you may need to increase those sizes in `app_state.h`.
- `g_*` globals are shared across modules. Look in `app_state.h` first when
  you want to know where data lives.

## Quick recap: the reading order

1. `main/main.ino`
2. `main/app_state.h` and `main/app_state.cpp`
3. `main/platform_display.cpp`
4. `main/sim.cpp`
5. `main/logs.cpp`
6. `main/ui_components.h`
7. `main/ui_screens.cpp`
8. `main/ui_flow.cpp`

If you follow this order, each file will feel like a natural step from the
previous one.
