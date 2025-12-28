#include <AnalogButtons.h>
#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include "config.h"
#include "logs.h"
#include "main.h"
#include "messages.h"
#include "moistureSensor.h"
#include "pumps.h"
#include "settings.h"
#include "traySensors.h"
#include "ui.h"

extern LiquidCrystal_I2C lcd;

extern Config config;
extern double averageMsBetweenFeeds;
extern unsigned long int millisAtEndOfLastFeed;

extern Button upButton;
extern Button leftButton;
extern Button downButton;
extern Button rightButton;
extern Button okButton;
extern Button *pressedButton;

enum UiTaskKind : uint8_t {
  UI_TASK_NONE = 0,
  UI_TASK_CALIBRATION,
  UI_TASK_PUMP_TEST
};

static UiTaskKind uiTask = UI_TASK_NONE;
static const uint8_t kCalibrationSamples = 8;

enum CalState : uint8_t {
  CAL_STATE_IDLE = 0,
  CAL_STATE_PROMPT_DRY,
  CAL_STATE_SAMPLE_DRY,
  CAL_STATE_WAIT_AFTER_DRY,
  CAL_STATE_PROMPT_SOAK,
  CAL_STATE_SAMPLE_SOAK,
  CAL_STATE_CONFIRM_SAVE
};

enum PumpState : uint8_t {
  PUMP_STATE_IDLE = 0,
  PUMP_STATE_MESSAGE,
  PUMP_STATE_ON,
  PUMP_STATE_OFF
};

struct CalTask {
  CalState state;
  CalState lastState;
  unsigned long nextSampleAt;
  unsigned long stateStartAt;
  uint8_t samplesTaken;
  int32_t sampleSum;
  int dry;
  int soaked;
  bool saveChoice;
  bool saved;
};

struct PumpTask {
  PumpState state;
  PumpState lastState;
  unsigned long nextActionAt;
  uint8_t cyclesRemaining;
};

static CalTask calTask = {};
static PumpTask pumpTask = {};

static void drawOkPrompt(PGM_P header, PGM_P prompt, PGM_P line2, PGM_P line3) {
  lcdClear();
  lcdPrint_P(header, 0);
  lcdClearLine(1);
  lcdSetCursor(0, 1);
  lcd.write((uint8_t)2);
  lcdPrint_P(prompt);
  lcdPrint_P(line2, 2);
  lcdPrint_P(line3, 3);
}

static void drawYesNoPrompt(bool yesSelected) {
  lcdClear();
  lcdPrint_P(MSG_SAVE_QUESTION, 0);
  lcdClearLine(1);
  lcdSetCursor(0, 1);
  if (yesSelected) lcd.write((uint8_t)2);
  else lcdPrint_P(MSG_SPACE);
  lcdPrint_P(MSG_YES);
  lcdClearLine(2);
  lcdSetCursor(0, 2);
  if (!yesSelected) lcd.write((uint8_t)2);
  else lcdPrint_P(MSG_SPACE);
  lcdPrint_P(MSG_NO);
}

bool uiTaskActive() {
  return uiTask != UI_TASK_NONE;
}

static void finishCalibrationTask() {
  setSoilSensorLazy();
  uiTask = UI_TASK_NONE;
}

static void finishPumpTestTask() {
  digitalWrite(PUMP_IN_DEVICE, HIGH);
  uiTask = UI_TASK_NONE;
}

void startCalibrationTask() {
  if (uiTaskActive()) return;
  uiTask = UI_TASK_CALIBRATION;
  calTask.state = CAL_STATE_PROMPT_DRY;
  calTask.lastState = CAL_STATE_IDLE;
  calTask.nextSampleAt = 0;
  calTask.samplesTaken = 0;
  calTask.sampleSum = 0;
  calTask.dry = 0;
  calTask.soaked = 0;
  calTask.saveChoice = true;
  calTask.saved = false;
  setSoilSensorRealTime();
  screenSaverModeOff();
}

void startPumpTestTask() {
  if (uiTaskActive()) return;
  uiTask = UI_TASK_PUMP_TEST;
  pumpTask.state = PUMP_STATE_MESSAGE;
  pumpTask.lastState = PUMP_STATE_IDLE;
  pumpTask.nextActionAt = 0;
  pumpTask.cyclesRemaining = 3;
  screenSaverModeOff();
}

static void runCalibrationTask() {
  analogButtonsCheck();

  bool entered = calTask.state != calTask.lastState;
  if (entered) calTask.stateStartAt = millis();

  switch (calTask.state) {
    case CAL_STATE_PROMPT_DRY:
      if (entered) {
        drawOkPrompt(MSG_MOIST_SENSOR, MSG_YES_ITS_DRY, MSG_ENSURE_SENSOR_IS, MSG_VERY_DRY);
      }
      if (pressedButton == &okButton) {
        calTask.state = CAL_STATE_SAMPLE_DRY;
        return;
      }
      if (pressedButton == &leftButton) {
        finishCalibrationTask();
        return;
      }
      break;

    case CAL_STATE_SAMPLE_DRY:
      if (entered) {
        calTask.samplesTaken = 0;
        calTask.sampleSum = 0;
        calTask.nextSampleAt = 0;
        lcdClear();
        lcdPrint_P(MSG_DRY_COLUMN, 1);
      }
      if (!soilSensorRealtimeReady()) break;
      if (millis() >= calTask.nextSampleAt) {
        (void)getSoilMoisture();
        uint16_t sample = soilSensorGetRealtimeRaw();
        calTask.sampleSum += sample;
        lcdPrint_P(MSG_DRY_COLUMN, 1);
        lcdPrintNumber(sample);
        lcdPrint_P(MSG_SPACE);
        calTask.samplesTaken++;
        calTask.nextSampleAt = millis() + 900;
        if (calTask.samplesTaken >= kCalibrationSamples) {
          calTask.dry = (int)(calTask.sampleSum / calTask.samplesTaken);
          calTask.state = CAL_STATE_WAIT_AFTER_DRY;
          return;
        }
      }
      break;

    case CAL_STATE_WAIT_AFTER_DRY:
      if (millis() - calTask.stateStartAt >= 2000) {
        calTask.state = CAL_STATE_PROMPT_SOAK;
        return;
      }
      break;

    case CAL_STATE_PROMPT_SOAK:
      if (entered) {
        drawOkPrompt(MSG_MOIST_SENSOR, MSG_YES_ITS_SOAKED, MSG_ENSURE_SENSOR_IS, MSG_VERY_SOAKED);
      }
      if (pressedButton == &okButton) {
        calTask.state = CAL_STATE_SAMPLE_SOAK;
        return;
      }
      if (pressedButton == &leftButton) {
        finishCalibrationTask();
        return;
      }
      break;

    case CAL_STATE_SAMPLE_SOAK:
      if (entered) {
        calTask.samplesTaken = 0;
        calTask.sampleSum = 0;
        calTask.nextSampleAt = 0;
        lcdClear();
        lcdPrint_P(MSG_SOAKED_COLUMN, 1);
      }
      if (!soilSensorRealtimeReady()) break;
      if (millis() >= calTask.nextSampleAt) {
        (void)getSoilMoisture();
        uint16_t sample = soilSensorGetRealtimeRaw();
        calTask.sampleSum += sample;
        lcdPrint_P(MSG_SOAKED_COLUMN, 1);
        lcdPrintNumber(sample);
        lcdPrint_P(MSG_SPACE);
        calTask.samplesTaken++;
        calTask.nextSampleAt = millis() + 900;
        if (calTask.samplesTaken >= kCalibrationSamples) {
          calTask.soaked = (int)(calTask.sampleSum / calTask.samplesTaken);
          setSoilSensorLazy();
          calTask.state = CAL_STATE_CONFIRM_SAVE;
          return;
        }
      }
      break;

    case CAL_STATE_CONFIRM_SAVE:
      if (entered) {
        calTask.saveChoice = true;
        drawYesNoPrompt(calTask.saveChoice);
      }
      if (pressedButton == &upButton || pressedButton == &downButton) {
        calTask.saveChoice = !calTask.saveChoice;
        drawYesNoPrompt(calTask.saveChoice);
      }
      if (pressedButton == &okButton) {
        if (calTask.saveChoice) {
          config.moistSensorCalibrationSoaked = calTask.soaked;
          config.moistSensorCalibrationDry = calTask.dry;
          saveConfig();
          calTask.saved = true;
        }
        finishCalibrationTask();
        return;
      }
      if (pressedButton == &leftButton) {
        finishCalibrationTask();
        return;
      }
      break;

    default:
      finishCalibrationTask();
      return;
  }

  calTask.lastState = calTask.state;
}

static void runPumpTestTask() {
  analogButtonsCheck();

  bool entered = pumpTask.state != pumpTask.lastState;

  switch (pumpTask.state) {
    case PUMP_STATE_MESSAGE:
      if (entered) {
        lcdClear();
        lcdPrint_P(MSG_DEVICE_WILL_BLINK, 1);
        lcdPrint_P(MSG_THREE_TIMES, 2);
        pumpTask.nextActionAt = millis() + 100;
      }
      if (millis() >= pumpTask.nextActionAt) {
        pumpTask.state = PUMP_STATE_ON;
        return;
      }
      break;

    case PUMP_STATE_ON:
      if (entered) {
        digitalWrite(PUMP_IN_DEVICE, LOW);
        pumpTask.nextActionAt = millis() + 1000;
      }
      if (millis() >= pumpTask.nextActionAt) {
        pumpTask.state = PUMP_STATE_OFF;
        return;
      }
      break;

    case PUMP_STATE_OFF:
      if (entered) {
        digitalWrite(PUMP_IN_DEVICE, HIGH);
        pumpTask.nextActionAt = millis() + 1000;
      }
      if (millis() >= pumpTask.nextActionAt) {
        if (pumpTask.cyclesRemaining > 0) pumpTask.cyclesRemaining--;
        if (pumpTask.cyclesRemaining == 0) {
          finishPumpTestTask();
          return;
        }
        pumpTask.state = PUMP_STATE_ON;
        return;
      }
      break;

    default:
      finishPumpTestTask();
      return;
  }

  pumpTask.lastState = pumpTask.state;
}

void runUiTask() {
  if (uiTask == UI_TASK_CALIBRATION) {
    runCalibrationTask();
  } else if (uiTask == UI_TASK_PUMP_TEST) {
    runPumpTestTask();
  }
}

void settings() {
  int8_t choice = 0;

  lcdClear();
  while (choice != -1) {
    setChoices_P(
      MSG_CAL_MOISTURE_SENSOR, 1,
      MSG_MAINTENANCE, 2);
    choice = selectChoice(2, 1);

    if (choice == 1) {
      startCalibrationTask();
      return;
    } else if (choice == 2) {
      maintenance();
      if (uiTaskActive()) return;
    }
  }
}

void maintenance() {
  int8_t choice = 0;
  while (choice != -1) {
    lcdClear();
    setChoices_P(
      MSG_TEST_PUMPS, 1,
      MSG_TEST_SENSORS, 2,
      MSG_RESET, 3

    );
    choice = selectChoice(3, 1);

    if (choice == 1) {
      startPumpTestTask();
      return;
    }
    else if (choice == 2) testSensors();
    else if (choice == 3) settingsReset();
  }
}

void testSensors() {
  static const unsigned long interval = 300;  // interval at which to run (milliseconds)
  static unsigned long previousMillis = 0;   // will store last time the loop ran
  static int soilMoistureRawPercentage = 0;
  static int soilMoistureAvgPercentage = 0;
  static bool trayWaterLevelLow = false;
  static int soilMoistureRawReading;
  static int soilMoistureAvgReading;


  lcdClear();

  lcdSetCursor(0, 0);
  lcdPrint_P(MSG_TRAY_LOW_COLUMN);
  lcdClearLine(1);
  lcdClearLine(2);
  lcdClearLine(3);

  setSoilSensorRealTime();

  while (1) {
    unsigned long currentMillis = millis();

    if (currentMillis - previousMillis >= interval) {
      previousMillis = currentMillis;

      soilMoistureAvgReading = getSoilMoisture();
      soilMoistureRawReading = soilSensorGetRealtimeRaw();
      
      soilMoistureAvgPercentage = soilMoistureAsPercentage(soilMoistureAvgReading);
      soilMoistureRawPercentage = soilMoistureAsPercentage(soilMoistureRawReading);
      
      trayWaterLevelLow = senseTrayWaterLevelLow();

      
      analogButtonsCheck();
 
      if (pressedButton != nullptr) {
        setSoilSensorLazy();
        break;
      }

      // Serial.print("AH"); Serial.println(digitalRead(TRAY_SENSOR_LOW)); 
      lcdSetCursor(15, 0);
      lcdPrintNumber(trayWaterLevelLow);
      lcdPrint_P(MSG_SPACE);

      lcdClearLine(1);
      lcdSetCursor(0, 1);
      lcdPrint_P(MSG_RAW_COLUMN);
      lcdSetCursor(5, 1);
      lcdPrintNumber(soilMoistureRawPercentage);
      lcdPrint_P(MSG_PERCENT);
      lcdPrint_P(MSG_SPACE);
      lcdPrintNumber(soilMoistureRawReading);

      lcdClearLine(2);
      lcdSetCursor(0, 2);
      lcdPrint_P(MSG_AVG_COLUMN);
      lcdSetCursor(5, 2);
      lcdPrintNumber(soilMoistureAvgPercentage);
      lcdPrint_P(MSG_PERCENT);
      lcdPrint_P(MSG_SPACE);
      lcdPrintNumber(soilMoistureAvgReading);
    }
  }
}



void wipeLogsAndResetVars() {
  averageMsBetweenFeeds = 0.0;
  millisAtEndOfLastFeed = 0;
  wipeLogs();
  initialaverageMsBetweenFeeds();
}

void resetOnlyLogs() {
  int8_t confirmWipe;

  confirmWipe = yesOrNo_P(MSG_SURE_QUESTION);
  if (confirmWipe == -1) return;

  if (confirmWipe) {
    lcdClear();
    lcdPrint_P(MSG_WIPING, 2);
    wipeLogsAndResetVars();
    createBootLogEntry();
  };
}

void settingsReset() {
  while (true) {
    setChoices_P(MSG_RESET_ONLY_LOGS, 0, MSG_RESET_DATA, 1);
    setChoicesHeader_P(MSG_RESET);
    int8_t choice = selectChoice(2, 0);

    if (choice == -1) return;
    if (choice == 0) resetOnlyLogs();
    else if (choice == 1) resetData();
  }
}

static void debugButtonRead(uint16_t readValue) {
  static unsigned long lastDebugMillis = 0;
  const unsigned long interval = 250;
  const unsigned long now = millis();

  if (now - lastDebugMillis < interval) return;
  lastDebugMillis = now;

  (void)readValue;
}

void runButtonsSetup() {
  uint16_t read = 1024;

  lcd.setCursor(0, 0);
  lcdPrint_P(MSG_PRESS_UP);
  while (read > 1000) {
    read = analogRead(BUTTONS_PIN);
    debugButtonRead(read);
    if (read < 1000) {
      delay(100);
      read = analogRead(BUTTONS_PIN);
      config.kbdUp = read;
      break;
    }
  }

  delay(1000);

  lcd.setCursor(0, 0);
  lcdPrint_P(MSG_PRESS_DOWN);
  read = 1024;
  while (read > 1000) {
    read = analogRead(BUTTONS_PIN);
    debugButtonRead(read);
    if (read < 1000) {
      delay(100);
      read = analogRead(BUTTONS_PIN);
      config.kbdDown = read;
      break;
    }
  }
  delay(1000);

  lcd.setCursor(0, 0);
  lcdPrint_P(MSG_PRESS_LEFT);
  read = 1024;
  while (read > 1000) {
    read = analogRead(BUTTONS_PIN);
    debugButtonRead(read);
    if (read < 1000) {
      delay(100);
      read = analogRead(BUTTONS_PIN);
      config.kbdLeft = read;
      break;
    }
  }

  delay(1000);

  lcd.setCursor(0, 0);
  lcdPrint_P(MSG_PRESS_RIGHT);
  read = 1024;
  while (read > 1000) {
    read = analogRead(BUTTONS_PIN);
    debugButtonRead(read);
    if (read < 1000) {
      delay(100);
      read = analogRead(BUTTONS_PIN);
      config.kbdRight = read;
      break;
    }
  }

  delay(1000);

  lcd.setCursor(0, 0);
  lcdPrint_P(MSG_PRESS_OK);
  read = 1024;
  while (read > 1000) {
    read = analogRead(BUTTONS_PIN);
    debugButtonRead(read);
    if (read < 1000) {
      delay(100);
      read = analogRead(BUTTONS_PIN);
      config.kbdOk = read;
      break;
    }
  }

  delay(1000);

  initializeButtons();
}

int runInitialSetup() {
  if (!calibrateSoilMoistureSensor()) return 0;

  // Save config
  config.mustRunInitialSetup = false;
  saveConfig();

  lcdClear();
  lcdPrint_P(MSG_DONE);

  return 1;
}

int calibrateSoilMoistureSensor() {
  startCalibrationTask();
  while (uiTaskActive()) {
    runUiTask();
  }
  return calTask.saved ? true : false;
}

void resetData() {
  int8_t reset = yesOrNo_P(MSG_SURE_QUESTION);

  if (reset == -1) return;

  if (reset) {
    lcdClear();
    lcdPrint_P(MSG_WIPING, 2);
    restoreDefaultConfig();
    saveConfig();
    wipeLogsAndResetVars();
    createBootLogEntry();
    while (!runInitialSetup())
      ;
  }
}

void activatePumps() {
  startPumpTestTask();
}
