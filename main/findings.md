findings.md

• Overview

  - Arduino controller with LCD/analog-button UI. Reads soil-moisture and a
    single tray low-level sensor, logs values to EEPROM, and provides
    calibration/maintenance tools (pump test, sensor test).

  Runtime Flow (main/main.ino)

  - setup() powers LCD, sets pins, initializes sensors/pumps/log storage/config;
    runs checksum guard + initial setup (button calibration + moisture
    calibration) if needed; seeds average feed interval from logs, writes boot
    log, and switches soil sensor to lazy mode.
  - loop() handles screensaver + button navigation/menu, keeps soil sensor lazy
    readings updated, logs periodic values hourly (LOG_VALUES_INTERVAL), and
    refreshes the display.

  Configuration & Persistence (config.*)

  - Config stored at EEPROM address 0 with checksum; includes moisture
    calibration, safety timers, and button ADC values.

  Sensors & Actuators

  - Moisture sensor (main/moistureSensor.*): analog A0 powered by D12. Supports
    real-time and lazy modes; lazy mode powers sensor only on scheduled reads
    (runSoilSensorLazyReadings() every loop). soilMoistureAsPercentage() maps
    ADC to % using stored dry/soaked calibration.
  - Tray sensor (main/traySensors.*): active-low digital input for dry/little.
  - Pump (main/pumps.*): PUMP_IN (2) active-low open.

  UI & Interaction (main/ui.*, messages.h)

  - 16x4 I2C LCD at 0x27 with custom arrows. AnalogButtons library on A7 with
    per-button ADC thresholds stored in config (runButtonsSetup() captures
    them).
  - Screens cycle with up/down; OK opens main menu (logs, settings). Settings
    provide calibration and maintenance (safety limits, test pump, test sensors,
    reset logs/data).

  Logging (main/logs.*)

  - EEPROM ring buffer starting at address 256 with epoch metadata; ~47 slots of
    LogEntry (seq, timestamps, entryType: boot/values, soil/tray readings).
  - maybeLogValues() writes hourly soil/tray snapshot entries; boot is logged
    via createBootLogEntry().

  Things to clarify/verify

  - soilMoistureAsState() is declared in moistureSensor.h but not implemented or
    used.
