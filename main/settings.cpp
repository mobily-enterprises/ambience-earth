#include <Arduino.h>
#include "LiquidCrystal_I2C.h"
#include "config.h"
#include "logs.h"
#include "main.h"
#include "messages.h"
#include "moistureSensor.h"
#include "pumps.h"
#include "feeding.h"
#include "runoffSensor.h"
#include "rtc.h"
#include "settings.h"
#include "ui.h"

extern LiquidCrystal_I2C lcd;

extern Config config;
extern unsigned long int millisAtEndOfLastFeed;

extern Button upButton;
extern Button leftButton;
extern Button downButton;
extern Button rightButton;
extern Button okButton;
extern Button *pressedButton;

static void setTimeAndDate() {
  uint8_t hour = 0;
  uint8_t minute = 0;
  uint8_t day = 1;
  uint8_t month = 1;
  uint8_t year = 26;

  if (rtcReadDateTime(&hour, &minute, &day, &month, &year)) {
    if (hour > 23) hour = 0;
    if (minute > 59) minute = 0;
    bool dateUnset = (month == 0 || month > 12 || day == 0 || day > 31);
    if (!dateUnset && year == 0 && month == 1 && day == 1) dateUnset = true;
    if (dateUnset) {
      day = 1;
      month = 1;
      year = 26;
    }
  } else {
    day = 1;
    month = 1;
    year = 26;
  }

  uint16_t timeMinutes = static_cast<uint16_t>(hour) * 60u + minute;
  if (!inputTime_P(MSG_SET_TIME, MSG_LITTLE, timeMinutes, &timeMinutes)) {
    lcdFlashMessage_P(MSG_ABORTED);
    return;
  }

  if (!inputDate_P(MSG_SET_DATE, MSG_LITTLE, &day, &month, &year)) {
    lcdFlashMessage_P(MSG_ABORTED);
    return;
  }

  hour = static_cast<uint8_t>(timeMinutes / 60u);
  minute = static_cast<uint8_t>(timeMinutes % 60u);

  int8_t save = yesOrNo_P(MSG_SAVE_QUESTION);
  if (save != 1) return;

  // TODO: When external EEPROM is available, reinstate the RTC write check and surface errors.
  (void)rtcSetDateTime(hour, minute, 0, day, month, year);
  config.flags |= CONFIG_FLAG_TIME_SET;
  saveConfigDone();
}

static void drawHeaderLines(MsgId line0, MsgId line1, char line1Suffix) {
  lcdClear();
  lcdSetCursor(0, 0);
  lcdPrint_P(line0);
  lcdSetCursor(0, 1);
  lcdPrint_P(line1);
  if (line1Suffix) lcd.print(line1Suffix);
}

static void drawPrimePrompt(bool priming) {
  if (!priming) {
    drawHeaderLines(MSG_PRIME_PUMP, MSG_OK_START, '\0');
    lcdPrint_P(MSG_OK_STOP, 2);
    return;
  }
  drawHeaderLines(MSG_PRIME_PUMP, MSG_OK_STOP, '\0');
}

static void drawDripperPrompt(bool filling, unsigned long startMillis, bool fullRedraw) {
  if (!filling) {
    drawHeaderLines(MSG_FILL_ONE_L, MSG_OK_START, '\0');
    lcdPrint_P(MSG_OK_WHEN_FULL, 2);
    return;
  }

  if (fullRedraw) {
    drawHeaderLines(MSG_FILLING, MSG_OK_WHEN_FULL, '\0');
    lcdSetCursor(0, 2);
    lcd.print('t');
    lcd.print(':');
  }

  lcdSetCursor(2, 2);
  lcdPrintSpaces(5);
  lcdSetCursor(2, 2);
  unsigned long seconds = (millis() - startMillis) / 1000UL;
  if (seconds < 1000) {
    lcd.print(seconds);
    lcd.print('s');
  }
}

static void drawWiping() {
  lcdClear();
  lcdPrint_P(MSG_WIPING, 2);
}

void saveConfigDone() {
  saveConfig();
  lcdFlashMessage_P(MSG_DONE);
}

void calibrateDripperFlow() {
  setSoilSensorLazy(); // ensure not in realtime mode
  screenSaverSuspend();

  bool priming = true;
  bool primeActive = false;
  bool filling = false;
  unsigned long startMillis = 0;
  unsigned long lastUpdate = 0;

  drawPrimePrompt(false);

  while (true) {
    runSoilSensorLazyReadings();
    analogButtonsCheck();

    unsigned long now = millis();
    if (filling && now - lastUpdate >= 1000) {
      lastUpdate = now;
      drawDripperPrompt(true, startMillis, false);
    }

    if (pressedButton == &leftButton) {
      if (primeActive || filling) closeLineIn();
      pressedButton = nullptr;
      screenSaverResume();
      return;
    }

    if (pressedButton == &okButton) {
      pressedButton = nullptr;
      if (priming) {
        if (!primeActive) {
          primeActive = true;
          openLineIn();
          drawPrimePrompt(true);
        } else {
          primeActive = false;
          closeLineIn();
          priming = false;
          drawDripperPrompt(false, startMillis, true);
        }
      } else if (!filling) {
        filling = true;
        startMillis = now;
        lastUpdate = now;
        openLineIn();
        drawDripperPrompt(true, startMillis, true);
      } else {
        closeLineIn();
        unsigned long elapsed = now - startMillis;
        if (elapsed == 0) elapsed = 1;
        uint32_t perLiter = elapsed * 10UL;
        if (perLiter < elapsed) perLiter = 0xFFFFFFFFUL;
        uint16_t initialMl = 4000;
        uint8_t stored = config.pulseTargetUnits;
        if (stored >= 5 && stored <= 50) initialMl = static_cast<uint16_t>(stored) * 200u;
        int16_t mlPerHour = inputNumber_P(MSG_LITTLE, static_cast<int16_t>(initialMl), 200, 1000, 10000, MSG_ML_PER_HOUR,
                                           MSG_CAL_DRIPPER_FLOW);
        if (mlPerHour < 0) mlPerHour = initialMl;
        uint8_t targetUnits = static_cast<uint8_t>(mlPerHour / 200);
        uint8_t onSec = 0;
        uint8_t offSec = 0;
        if (targetUnits >= 5 && targetUnits <= 50 && perLiter) {
          uint32_t fullRate = 3600000000UL / perLiter;
          uint32_t targetRate = (uint32_t)targetUnits * 200UL;
          if (fullRate > targetRate) {
            uint32_t diff = fullRate - targetRate;
            onSec = 10;
            uint32_t offCalc = (uint32_t)onSec * diff / targetRate;
            if (offCalc > 60) {
              onSec = 5;
              offCalc = (uint32_t)onSec * diff / targetRate;
            }
            if (offCalc > 255) offCalc = 255;
            offSec = static_cast<uint8_t>(offCalc);
          }
        }
        config.dripperMsPerLiter = perLiter;
        config.pulseTargetUnits = targetUnits;
        config.pulseOnSeconds = onSec;
        config.pulseOffSeconds = offSec;
        config.flags |= CONFIG_FLAG_DRIPPER_CALIBRATED;
        config.flags |= CONFIG_FLAG_PULSE_SET;
        saveConfig();
        lcdClear();
        lcdPrint_P(MSG_PULSE_COLON);
        lcd.print(' ');
        lcd.print(config.pulseOnSeconds);
        lcd.print(' ');
        lcdPrint_P(MSG_ON);
        lcd.print(' ');
        lcd.print(config.pulseOffSeconds);
        lcd.print(' ');
        lcdPrint_P(MSG_OFF);
        delay(3000);
        lcdFlashMessage_P(MSG_DONE);
        screenSaverResume();
        return;
      }
    }
  }
}

static void calibrateSensorsMenu() {
  int8_t choice = 0;
  int8_t lastChoice = 1;

  lcdClear();
  while (choice != -1) {
    setChoices_P(MSG_CAL_MOISTURE_SENSOR, 1, MSG_CAL_DRIPPER_FLOW, 2);
    setChoicesHeader_P(MSG_CALIBRATE_SENSORS);
    choice = selectChoice(2, lastChoice);
    if (choice != -1) lastChoice = choice;

    if (choice == 1) calibrateMoistureSensor();
    else if (choice == 2) calibrateDripperFlow();
  }
}

void environmentMenu() {
  uint16_t onMinutes = config.lightsOnMinutes;
  uint16_t offMinutes = config.lightsOffMinutes;

  if ((config.flags & CONFIG_FLAG_LIGHTS_ON_SET) == 0) {
    onMinutes = static_cast<uint16_t>(6 * 60);
  }
  if ((config.flags & CONFIG_FLAG_LIGHTS_OFF_SET) == 0) {
    offMinutes = static_cast<uint16_t>(18 * 60);
  }
  if (onMinutes > 1439) onMinutes = 0;
  if (offMinutes > 1439) offMinutes = 0;

  if (inputLightsOnOff_P(MSG_LIGHTS_ON_OFF_TIMES, &onMinutes, &offMinutes)) {
    config.lightsOnMinutes = onMinutes;
    config.lightsOffMinutes = offMinutes;
    config.flags |= CONFIG_FLAG_LIGHTS_ON_SET;
    config.flags |= CONFIG_FLAG_LIGHTS_OFF_SET;
    saveConfigDone();
  }
}

static bool moistureCalibrationDone() {
  return config.moistSensorCalibrationDry != 1024 && config.moistSensorCalibrationSoaked != 0;
}

static bool dripperCalibrationDone() {
  return (config.flags & CONFIG_FLAG_DRIPPER_CALIBRATED) != 0;
}

static bool timeDateDone() {
  return (config.flags & CONFIG_FLAG_TIME_SET) != 0;
}

static bool lightsDone() {
  if ((config.flags & CONFIG_FLAG_LIGHTS_ON_SET) == 0) return false;
  if ((config.flags & CONFIG_FLAG_LIGHTS_OFF_SET) == 0) return false;
  return true;
}

static bool maxDailyDone() {
  if (config.flags & CONFIG_FLAG_MAX_DAILY_SET) return true;
  return config.maxDailyWaterMl != 0;
}

static bool pulseDone() {
  return (config.flags & CONFIG_FLAG_PULSE_SET) != 0;
}

static void testPeripheralsMenu() {
  int8_t choice = 0;
  int8_t lastChoice = 1;

  lcdClear();
  while (choice != -1) {
    setChoices_P(MSG_TEST_SENSORS, 1, MSG_TEST_PUMPS, 2);
    setChoicesHeader_P(MSG_TEST_PERIPHERALS);
    choice = selectChoice(2, lastChoice);
    if (choice != -1) lastChoice = choice;

    if (choice == 1) testSensors();
    else if (choice == 2) pumpTest();
  }
}
void settings() {
  feedingClearRunoffWarning();
  int8_t choice = 0;
  int8_t lastChoice = 1;

  lcdClear();
  while (choice != -1) {
    setChoices_P(
      MSG_SET_TIME_DATE, 1,
      MSG_CALIBRATE_SENSORS, 2,
      MSG_TEST_PERIPHERALS, 3,
      MSG_RESET, 4);
    choice = selectChoice(4, lastChoice);
    if (choice != -1) lastChoice = choice;

    if (choice == 1) setTimeAndDate();
    else if (choice == 2) calibrateSensorsMenu();
    else if (choice == 3) testPeripheralsMenu();
    else if (choice == 4) settingsReset();
  }
}

bool initialSetupComplete() {
  return maxDailyDone() && timeDateDone() && lightsDone()
    && moistureCalibrationDone() && dripperCalibrationDone() && pulseDone();
}

void initialSetupMenu() {
  feedingClearRunoffWarning();
  int8_t choice = 0;
  int8_t lastChoice = 1;

  while (choice != -1) {
    resetChoicesAndHeader();
    uint8_t count = 0;

    if (!timeDateDone()) setChoice_P(count++, MSG_SET_TIME_DATE, 1);
    if (!maxDailyDone()) setChoice_P(count++, MSG_MAX_DAILY_WATER, 2);
    if (!lightsDone()) setChoice_P(count++, MSG_LIGHTS_ON_OFF_TIMES, 3);

    if (!moistureCalibrationDone()) setChoice_P(count++, MSG_CAL_MOISTURE_SENSOR, 4);
    if (!dripperCalibrationDone() || !pulseDone()) {
      setChoice_P(count++, MSG_CAL_DRIPPER_FLOW, 5);
    }

    setChoicesHeader_P(MSG_SETTINGS);
    choice = selectChoice(count, lastChoice);
    if (choice != -1) lastChoice = choice;

    if (choice == 1) setTimeAndDate();
    else if (choice == 2) editMaxDailyWater();
    else if (choice == 3) environmentMenu();
    else if (choice == 4) calibrateMoistureSensor();
    else if (choice == 5) calibrateDripperFlow();

    if (initialSetupComplete()) return;
  }
}

static uint16_t calibrateOnePoint(bool isDry) {
  drawHeaderLines(isDry ? MSG_CAL_DRY_ONLY : MSG_CAL_WET_ONLY,
                  isDry ? MSG_DRY_COLUMN : MSG_WET_COLUMN, ' ');

  soilSensorWindowStart();
  uint16_t lastRaw = 0xFFFF;
  unsigned long lastPrint = 0;

  while (true) {
    SoilSensorWindowStats stats;
    bool done = soilSensorWindowTick(&stats);
    uint16_t sample = soilSensorWindowLastRaw();
    unsigned long now = millis();

    if (sample != lastRaw && now - lastPrint >= 300) {
      lcdSetCursor(5, 1);
      lcd.print(sample);
      lcd.print(' ');
      lastRaw = sample;
      lastPrint = now;
    }

    if (done) {
      setSoilSensorLazy();
      uint16_t avg = stats.avgRaw;
      if (isDry) config.moistSensorCalibrationDry = avg;
      else config.moistSensorCalibrationSoaked = avg;
      saveConfig();

      drawHeaderLines(isDry ? MSG_CAL_DRY_ONLY : MSG_CAL_WET_ONLY,
                      isDry ? MSG_DRY_COLUMN : MSG_WET_COLON, '\0');
      lcd.print(avg);
      lcdSetCursor(0, 2);
      lcdPrint_P(MSG_SAVE_QUESTION);
      lcdSetCursor(0, 3);
      lcdPrint_P(MSG_OK_EDIT_BACK);
      while (true) {
        analogButtonsCheck();
        if (pressedButton == &okButton) {
          pressedButton = nullptr;
          break;
        } else if (pressedButton == &leftButton) {
          pressedButton = nullptr;
          return 0;
        }
      }
      return avg;
    }

    analogButtonsCheck();
    if (pressedButton == &leftButton) {
      setSoilSensorLazy();
      return 0;
    }
  }
}

void calibrateMoistureSensor() {
  while (true) {
    setChoices_P(MSG_CAL_DRY_ONLY, 1, MSG_CAL_WET_ONLY, 2);
    setChoicesHeader_P(MSG_CAL_MOISTURE_SENSOR);
    int8_t choice = selectChoice(2, 1);
    if (choice == -1) return;
    screenSaverSuspend();
    if (choice == 1) {
      calibrateOnePoint(true);
      screenSaverResume();
      return;
    }
    if (choice == 2) {
      calibrateOnePoint(false);
      screenSaverResume();
      return;
    }
  }
}

void pumpTest() {
  lcdFlashMessage_P(MSG_DEVICE_WILL_BLINK, MSG_THREE_TIMES, 500);
  const uint8_t cycles = 3;
  for (uint8_t i = 0; i < cycles; ++i) {
    digitalWrite(PUMP_IN_DEVICE, LOW);
    delay(1000);
    digitalWrite(PUMP_IN_DEVICE, HIGH);
    delay(1000);
  }
  lcdFlashMessage_P(MSG_DONE);
}

void testSensors() {
  setSoilSensorRealTime();
  drawHeaderLines(MSG_SOIL_MOISTURE_COLUMN, MSG_RUNOFF, ':');

  while (true) {
    analogButtonsCheck();
    if (pressedButton != nullptr) {
      setSoilSensorLazy();
      break;
    }

    getSoilMoisture();
    uint16_t raw = soilSensorGetRealtimeRaw();
    lcdSetCursor(10, 0);
    lcdPrintSpaces(5);
    lcdSetCursor(10, 0);
    lcdPrintNumber(raw);
    lcdSetCursor(10, 1);
    lcd.print(runoffDetected() ? 1 : 0);
    delay(300);
  }
}

void wipeLogsAndResetVars() {
  millisAtEndOfLastFeed = 0;
  wipeLogs();
}

void resetOnlyLogs() {
  int8_t confirmWipe;

  confirmWipe = yesOrNo_P(MSG_SURE_QUESTION);
  if (confirmWipe == -1) return;

  if (confirmWipe) {
    drawWiping();
    wipeLogsAndResetVars();
    createBootLogEntry();
  };
}

void settingsReset() {
  int8_t lastChoice = 0;
  while (true) {
    setChoices_P(MSG_RESET_ONLY_LOGS, 0, MSG_RESET_DATA, 1);
    setChoicesHeader_P(MSG_RESET);
    int8_t choice = selectChoice(2, lastChoice);

    if (choice == -1) return;
    lastChoice = choice;
    if (choice == 0) resetOnlyLogs();
    else if (choice == 1) resetData();
  }
}

struct ButtonPrompt {
  MsgId prompt;
  uint16_t *target;
};

static bool calibrateSingleButton(const ButtonPrompt &bp) {
  uint16_t reading = 1024;
  lcd.setCursor(0, 0);
  lcdPrint_P(bp.prompt);
  lcdClearLine(1);

  // Require a clean release before accepting a press.
  while (true) {
    reading = analogRead(BUTTONS_PIN);
    if (reading > 1000) {
      delay(10);
      if (analogRead(BUTTONS_PIN) > 1000) break;
    } else {
      delay(5);
    }
  }

  while (true) {
    reading = analogRead(BUTTONS_PIN);
    if (reading < 1000) {
      const uint8_t samples = 6;
      uint16_t minVal = 1023;
      uint16_t maxVal = 0;
      uint32_t sum = 0;
      uint8_t count = 0;

      for (uint8_t i = 0; i < samples; ++i) {
        uint16_t sample = analogRead(BUTTONS_PIN);
        if (sample > 1000) {
          count = 0;
          break;
        }
        if (sample < minVal) minVal = sample;
        if (sample > maxVal) maxVal = sample;
        sum += sample;
        count++;
        delay(10);
      }

      if (count == samples && (maxVal - minVal) <= BUTTONS_ANALOG_MARGIN) {
        uint16_t avg = static_cast<uint16_t>(sum / samples);
        *bp.target = avg;
        lcdPrintNumber(avg, 1);
        return true;
      }
    }
  }
}

void runButtonsSetup() {
  const ButtonPrompt prompts[] = {
    {MSG_PRESS_UP, &config.kbdUp},
    {MSG_PRESS_DOWN, &config.kbdDown},
    {MSG_PRESS_LEFT, &config.kbdLeft},
    {MSG_PRESS_RIGHT, &config.kbdRight},
    {MSG_PRESS_OK, &config.kbdOk},
  };

  for (size_t i = 0; i < sizeof(prompts) / sizeof(prompts[0]); ++i) {
    calibrateSingleButton(prompts[i]);
    delay(200);
  }

  initializeButtons();
}

int runInitialSetup() {
  if (!calibrateSoilMoistureSensor()) return 0;

  // Save config
  config.flags &= static_cast<uint8_t>(~CONFIG_FLAG_MUST_RUN_INITIAL_SETUP);
  saveConfig();

  lcdClear();
  lcdPrint_P(MSG_DONE);

  return 1;
}

int calibrateSoilMoistureSensor() {
  calibrateMoistureSensor();
  return true;
}

void resetData() {
  int8_t reset = yesOrNo_P(MSG_SURE_QUESTION);

  if (reset == -1) return;

  if (reset) {
    drawWiping();
    restoreDefaultConfig();
    saveConfig();
    wipeLogsAndResetVars();
    createBootLogEntry();
    while (!runInitialSetup())
      ;
  }
}
