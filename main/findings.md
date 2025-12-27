findings.md

• Overview

  - Arduino irrigation controller with LCD/analog-button UI. Manages soil-
    moisture sensor and tray water-level sensors to drive two pumps/valves, logs
    events to EEPROM, and offers menus for setup/calibration.

  Runtime Flow (main/main.ino)

  - setup() powers LCD, sets pins, initializes sensors/pumps/log storage/config;
    runs checksum guard + initial setup (button calibration + feed mode +
    moisture calibration) if needed; seeds average feed interval from logs,
    writes boot log, and switches soil sensor to lazy mode.
  - loop() handles screensaver + button navigation/menu, keeps soil sensor lazy
    readings updated, logs periodic values hourly (LOG_VALUES_INTERVAL), and
    every ~2s runs actions, tray-emptying state machine, and refreshes display.
  - Actions: maybeRunActions() evaluates up to two active actions; runAction()
    opens line-in, shows status, stops on stop conditions or max time, logs
    before/after readings, updates average time between feeds, and restores lazy
    sensor mode.
  - Tray emptying: state machine emptyTrayIfNecessary() pumps out when tray
    level hits mid+ and respects rest/max timing.

  Configuration & Persistence (config.*)

  - Config stored at EEPROM address 0 with checksum; includes feed mode (top vs
    tray), auto-drain flag, moisture calibration, tray sensor calibration
    thresholds, safety timers, button ADC values, two active-action slots, and a
    5-action array (name/trigger/stop conditions/feedFrom).
  - Defaults set in restoreDefaultConfig() (mustRunInitialSetup=true,
    feedFrom=top, tray emptying off, calibration and safety timers, sample
    actions). Checksum uses FNV-like hash over config (skipping first 8 bytes).

  Sensors & Actuators

  - Moisture sensor (main/moistureSensor.*): analog A0 powered by D12. Supports
    real-time and lazy modes; lazy mode powers sensor only on scheduled reads
    (runSoilSensorLazyReadings() every loop). soilMoistureAsPercentage() maps
    ADC to % using stored dry/soaked calibration.
  - Tray sensors (main/traySensors.*): active-low digital inputs; map to dry/
    little/middle/full states. Note TRAY_SENSOR_HIGH and TRAY_SENSOR_MID
    currently share pin 7 (mid placeholder).
  - Pumps/valves (main/pumps.*): PUMP_IN (2) active-low open, PUMP_OUT (3) uses
    HIGH to open, LOW to close (init sets both off).

  UI & Interaction (main/ui.*, messages.h)

  - 16x4 I2C LCD at 0x27 with custom arrows. AnalogButtons library on A7 with
    per-button ADC thresholds stored in config (runButtonsSetup() captures
    them).
  - Choice/number/string input helpers render menus, yes/no prompts, etc.
    Screens cycle with up/down; OK opens main menu (active actions, logs, force
    action, settings). Screensaver blanks backlight after 60s.

  Logging (main/logs.*)

  - EEPROM ring buffer starting at address 256 with epoch metadata; ~47 slots of
    LogEntry (seq, timestamps, entryType: boot/feed/values, actionId, soil/tray
    before/after, topFeed flag, outcome).
  - Maintains logEpoch for wrap-around, browsable epoch for UI. maybeLogValues()
    writes hourly soil/tray snapshot entries; feeds and boot are logged via
    runAction() and createBootLogEntry().

  Settings & Calibration (main/settings.*)

  - Menus for editing actions (name, trigger/stop conditions, feed direction),
    choosing feed mode, enabling “auto drain”, calibrating moisture sensor, tray
    sensor calibration prompts (TODO), safety timers, testing pumps/sensors,
    wiping logs or full reset. Initial setup forces feed mode selection,
    optional auto drain, and moisture calibration.

  Things to clarify/verify

  - trayNeedsEmptying semantics: setAutoDrain() stores user’s “Auto drain?”
    answer, but emptyTrayIfNecessary() exits early when the flag is true
    (appears inverted).
  - Tray sensor defaults for 1/4/1/2/3/4 full calibrations are declared in
    config but never set in restoreDefaultConfig().
  - soilMoistureAsState() is declared but not implemented/used.
  - TRAY_SENSOR_HIGH and TRAY_SENSOR_MID use the same pin (placeholder until
    hardware exists?).
  - Pump-out control uses HIGH to open, LOW to close—confirm matches hardware.

  Let me know if you want diagrams, edge-case notes, or proposed fixes for the
  above ambiguities.

- `config.trayNeedsEmptying` is set when the user enables “Auto drain?”, yet `emptyTrayIfNecessary()` bails out when the flag is true, so the semantics appear inverted. Clarify whether auto-drain should run or skip when this flag is set.
- Tray calibration fields (`trayWaterLevelSensorCalibrationQuarter/Half/ThreeQuarters`) are declared but never initialised in `restoreDefaultConfig()` or used elsewhere; calibration flow seems incomplete.
- `soilMoistureAsState()` is declared in `moistureSensor.h` but not implemented or referenced.
- `TRAY_SENSOR_HIGH` and `TRAY_SENSOR_MID` both map to pin 7 (mid is noted as placeholder); add the real pin when the mid-level sensor is wired.
- `openLineOut()/closeLineOut()` drive PUMP_OUT HIGH to open and LOW to close; confirm this matches the hardware wiring to avoid reversed behavior.
