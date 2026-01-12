#include "LiquidCrystal_I2C.h"
#include "ui.h"
#include "messages.h"
#include "config.h"
#include "moistureSensor.h"
#include "rtc.h"

// Private functions (not declared in ui.h)
static void labelcpy_R(char *destination, const char *source, uint8_t maxLen);
extern Config config;

LiquidCrystal_I2C lcd = LiquidCrystal_I2C(0x27, 20, 4);

Button *pressedButton;
Choice choices[10];

LabelRef choicesHeader;

Button upButton;
Button leftButton;
Button downButton;
Button rightButton;
Button okButton;

namespace {
struct ButtonState {
  Button *current;
  Button *lastSample;
  uint8_t pressCount;
  uint8_t releaseCount;
  unsigned long pressedAt;
  unsigned long lastRepeatAt;
};

static ButtonState buttonState = {};
static const uint8_t kDebounceSamples = (BUTTONS_DEBOUNCE_MULTIPLIER < 1) ? 1 : BUTTONS_DEBOUNCE_MULTIPLIER;
static const uint8_t kReleaseSamples = kDebounceSamples;
static const uint16_t kNoPressThreshold = 1000;
static const uint16_t kReleaseMargin = BUTTONS_ANALOG_MARGIN + (BUTTONS_ANALOG_MARGIN / 2u);

static uint16_t absDiff(uint16_t a, uint16_t b) {
  return (a > b) ? (a - b) : (b - a);
}

static bool thresholdValid(uint16_t threshold) {
  return threshold < 1024;
}

static uint16_t readButtonsAdc() {
  return readMedian3FromPin(BUTTONS_PIN);
}

static bool readingWithin(Button *button, uint16_t reading, uint16_t margin) {
  uint16_t threshold = button->threshold;
  if (!thresholdValid(threshold)) return false;
  return absDiff(reading, threshold) <= margin;
}

static void considerButton(Button *button, uint16_t reading, Button **best, uint16_t *bestDiff) {
  uint16_t threshold = button->threshold;
  if (!thresholdValid(threshold)) return;
  uint16_t diff = absDiff(reading, threshold);
  if (diff <= BUTTONS_ANALOG_MARGIN && diff < *bestDiff) {
    *best = button;
    *bestDiff = diff;
  }
}

static Button *readButtonSample(Button *current) {
  uint16_t reading = readButtonsAdc();
  if (reading >= kNoPressThreshold) return nullptr;

  if (current && readingWithin(current, reading, kReleaseMargin)) {
    return current;
  }

  Button *best = nullptr;
  uint16_t bestDiff = BUTTONS_ANALOG_MARGIN + 1;
  considerButton(&upButton, reading, &best, &bestDiff);
  considerButton(&downButton, reading, &best, &bestDiff);
  considerButton(&leftButton, reading, &best, &bestDiff);
  considerButton(&rightButton, reading, &best, &bestDiff);
  considerButton(&okButton, reading, &best, &bestDiff);

  return best;
}

static void resetButtonState() {
  buttonState = {};
  pressedButton = nullptr;
}
}  // namespace

void initializeButtons() {
  upButton.threshold = config.kbdUp;
  upButton.repeat = true;
  downButton.threshold = config.kbdDown;
  downButton.repeat = true;
  leftButton.threshold = config.kbdLeft;
  leftButton.repeat = false;
  rightButton.threshold = config.kbdRight;
  rightButton.repeat = false;
  okButton.threshold = config.kbdOk;
  okButton.repeat = false;

  resetButtonState();
}

static void updateSensorIndicator() {
  static bool indicatorOn = false;
  static unsigned long lastToggleAt = 0;
  const unsigned long blinkInterval = 200;

  if (!soilSensorIsActive()) {
    if (indicatorOn) {
      indicatorOn = false;
      lcdSetCursor(19, 3);
      lcd.print(' ');
    }
    return;
  }

  unsigned long now = millis();
  if (now - lastToggleAt < blinkInterval) return;
  lastToggleAt = now;
  indicatorOn = !indicatorOn;

  lcdSetCursor(19, 3);
  if (indicatorOn) lcd.write((uint8_t)3);
  else lcd.print(' ');
}

void analogButtonsCheck() {
  pressedButton = nullptr;
  unsigned long now = millis();
  Button *sample = readButtonSample(buttonState.current);

  if (!buttonState.current) {
    if (!sample) {
      buttonState.lastSample = nullptr;
      buttonState.pressCount = 0;
    } else {
      if (sample != buttonState.lastSample) {
        buttonState.lastSample = sample;
        buttonState.pressCount = 1;
      } else if (buttonState.pressCount < kDebounceSamples) {
        buttonState.pressCount++;
      }

      if (buttonState.pressCount >= kDebounceSamples) {
        buttonState.current = sample;
        buttonState.pressedAt = now;
        buttonState.lastRepeatAt = now;
        buttonState.pressCount = 0;
        buttonState.releaseCount = 0;
        pressedButton = sample;
      }
    }
  } else {
    if (sample == buttonState.current) {
      buttonState.releaseCount = 0;
      if (buttonState.current->repeat &&
          now - buttonState.pressedAt >= BUTTONS_HOLD_DURATION_MS &&
          now - buttonState.lastRepeatAt >= BUTTONS_HOLD_INTERVAL_MS) {
        buttonState.lastRepeatAt = now;
        pressedButton = buttonState.current;
      }
    } else if (!sample) {
      if (buttonState.releaseCount < kReleaseSamples) {
        buttonState.releaseCount++;
      }
      if (buttonState.releaseCount >= kReleaseSamples) {
        buttonState.current = nullptr;
        buttonState.lastSample = nullptr;
        buttonState.pressCount = 0;
        buttonState.releaseCount = 0;
      }
    } else {
      buttonState.releaseCount = 0;
    }
  }

  updateSensorIndicator();
}

void lcdClear() {
  lcd.clear();
}

void lcdPrintSpaces(uint8_t count) {
  while (count--) lcd.print(' ');
}

void lcdClearLine(uint8_t y) {
  lcdSetCursor(0, y);
  lcdPrintSpaces(DISPLAY_COLUMNS);
}

void lcdPrint_P(MsgId message, int8_t y) {
  if (y != -1) {
    lcdClearLine(y);
    lcdSetCursor(0, y);
  }
  if (message == MSG_LITTLE) return;
  uint16_t addr = msgOffset(message);
  for (uint8_t i = 0; i < LABEL_LENGTH; ++i) {
    char c = msgReadByte(addr++);
    if (!c) break;
    lcd.print(c);
  }
}

void lcdPrintNumber(int number, int8_t y) {
  if (y != -1) {
    lcdClearLine(y);
    lcdSetCursor(0, y);
  }
  lcd.print(number);
}

void lcdSetCursor(uint8_t x, uint8_t y) {
  lcd.setCursor(x, y);
}

// --- String input support (cycling allowed characters) ---

static char userInputString[LABEL_LENGTH + 1] = {0};

static bool isAllowedChar(char c) {
  return c == ' ' || (c >= 'A' && c <= 'Z');
}

static char getNextCharacter(char currentChar) {
  if (currentChar == ' ') return 'A';
  if (currentChar >= 'A' && currentChar < 'Z') return static_cast<char>(currentChar + 1);
  return ' ';
}

static char getPreviousCharacter(char currentChar) {
  if (currentChar == ' ') return 'Z';
  if (currentChar >= 'A' && currentChar <= 'Z') {
    if (currentChar == 'A') return ' ';
    return static_cast<char>(currentChar - 1);
  }
  return 'Z';
}

static void sanitizeUserInput(char *str, uint8_t maxLen) {
  uint8_t len = strnlen(str, maxLen);
  for (uint8_t i = 0; i < len; i++) {
    if (!isAllowedChar(str[i])) str[i] = ' ';
  }
  str[len] = '\0';
}

static uint8_t actualLength(const char *str, uint8_t maxLen) {
  uint8_t len = strnlen(str, maxLen);
  while (len > 0 && str[len - 1] == ' ') len--;
  return len;
}

static void drawPromptAndHeader(MsgId prompt, MsgId optionalHeader) {
  lcd.clear();
  lcd.setCursor(0, 2);
  lcdPrint_P(prompt);
  lcd.setCursor(0, 0);
  lcdPrint_P(optionalHeader);
}

// Simple wrapper to copy from RAM; used by string input.
static bool inputStringCommon(MsgId prompt, MsgId optionalHeader, char *initialUserInput, bool asEdit, uint8_t maxLen) {
  uint8_t cursorPosition = 0;
  bool displayChanged = true;
  bool cursorVisible = true;
  unsigned long previousMillis = 0;
  uint8_t limit = maxLen > LABEL_LENGTH ? LABEL_LENGTH : maxLen;
  if (limit == 0) limit = 1;

  const uint8_t inputY = 3;

  if (strnlen(initialUserInput, limit) == 0) {
    memset(userInputString, ' ', limit);
    userInputString[limit] = '\0';
    userInputString[0] = 'A';
    cursorPosition = 1;
  } else {
    labelcpy_R(userInputString, initialUserInput, limit);
    sanitizeUserInput(userInputString, limit);
    cursorPosition = actualLength(userInputString, limit);
    if (cursorPosition == 0) cursorPosition = 1;
  }

  drawPromptAndHeader(prompt, optionalHeader);

  while (true) {
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= 500) {
      cursorVisible = !cursorVisible;
      previousMillis = currentMillis;
      displayChanged = true;
    }

    if (displayChanged) {
      lcd.setCursor(0, inputY);
      lcd.write((uint8_t)1);
      lcd.setCursor(1, inputY);
      lcd.print(userInputString);
      if (cursorPosition > 0) {
        lcd.setCursor(cursorPosition, inputY);
        lcd.print(cursorVisible ? userInputString[cursorPosition - 1] : (char)0);
      }
      displayChanged = false;
    }

    analogButtonsCheck();
    if (pressedButton) {
      cursorVisible = true;
      previousMillis = currentMillis;
      displayChanged = true;
    }

    if (pressedButton == &okButton) {
      if (cursorPosition == 0) userInputString[0] = '\0';
      if (asEdit) labelcpy_R(initialUserInput, userInputString, limit);
      return true;
    } else if (pressedButton == &upButton) {
      if (cursorPosition != 0) {
        userInputString[cursorPosition - 1] = getNextCharacter(userInputString[cursorPosition - 1]);
        displayChanged = true;
      }
    } else if (pressedButton == &downButton) {
      if (cursorPosition != 0) {
        userInputString[cursorPosition - 1] = getPreviousCharacter(userInputString[cursorPosition - 1]);
        displayChanged = true;
      }
    } else if (pressedButton == &rightButton) {
      cursorPosition++;
      if (cursorPosition >= limit || cursorPosition >= DISPLAY_COLUMNS) cursorPosition = limit;
      displayChanged = true;
    } else if (pressedButton == &leftButton) {
      if (cursorPosition <= 1) {
        return false;
      }
      cursorPosition--;
      displayChanged = true;
    }
  }
}

// Public wrappers with sensible defaults for header/asEdit.
bool inputString_P(MsgId prompt, char *initialUserInput, MsgId optionalHeader /*=MSG_LITTLE*/, bool asEdit /*=false*/, uint8_t maxLen /*=LABEL_LENGTH*/) {
  return inputStringCommon(prompt, optionalHeader, initialUserInput, asEdit, maxLen);
}

// R-variant not needed in current codepaths; keep PGM version for consistency.

const byte fullSquare[8] = {
  B11111,
  B11111,
  B11111,
  B11111,
  B11111,
  B11111,
  B11111,
  B11111
};

const byte leftArrow[8] = {
  B00010,
  B00100,
  B01100,
  B11100,
  B01100,
  B00100,
  B00010
};

const byte rightArrow[8] = {
  B00000,
  B10000,
  B01100,
  B01110,
  B01100,
  B10000,
  B00000,
  B00000
};

const byte sensorDot[8] = {
  B00001,
  B00001,
  B00000,
  B00000,
  B00000,
  B00000,
  B00000,
  B00000
};


static void labelcpy_R(char *destination, const char *source, uint8_t maxLen) {
  if (!source) {
    destination[0] = '\0';
    return;
  }
  strncpy(destination, source, maxLen);
  destination[strnlen(destination, maxLen)] = '\0';
}

void initLcd() {
  lcd.init();
  delay(100);
  lcd.backlight();

  lcd.createChar(0, fullSquare);
  lcd.createChar(1, leftArrow);
  lcd.createChar(2, rightArrow);
  lcd.createChar(3, sensorDot);

  resetChoicesAndHeader();
}

void lcdPrintTwoDigits(uint8_t value) {
  if (value < 10) lcd.print('0');
  lcd.print(value);
}

static void drawTwoDigitsAt(uint8_t col, uint8_t row, uint8_t value) {
  lcdSetCursor(col, row);
  lcdPrintTwoDigits(value);
}

static void drawBlinkAt(uint8_t col, uint8_t row, bool blinkOn) {
  if (blinkOn) return;
  lcdSetCursor(col, row);
  lcd.write((uint8_t)0);
  lcdSetCursor(col + 1, row);
  lcd.write((uint8_t)0);
}

static void updateBlink(bool *cursorVisible, unsigned long *lastBlinkAt,
                        bool *displayChanged, unsigned long now) {
  if (now - *lastBlinkAt < 500) return;
  *cursorVisible = !*cursorVisible;
  *lastBlinkAt = now;
  *displayChanged = true;
}

static void resetBlink(bool *cursorVisible, unsigned long *lastBlinkAt,
                       bool *displayChanged, unsigned long now) {
  *cursorVisible = true;
  *lastBlinkAt = now;
  *displayChanged = true;
}

static void drawHeaderFrame(MsgId header) {
  lcdClear();
  lcdPrint_P(header, 0);
  lcdClearLine(3);
}

static void drawTimeEntryFrame(MsgId header, MsgId prompt, uint8_t row) {
  drawHeaderFrame(header);
  lcdPrint_P(prompt, 1);
  lcdClearLine(row);
  lcdSetCursor(0, row);
  lcdPrint_P(MSG_TIME_LABEL);
  lcdSetCursor(7, row);
  lcd.print(':');
}

static void drawTimeEntryLine(uint8_t hour, uint8_t minute, uint8_t row, bool blinkOn, bool editHour) {
  drawTwoDigitsAt(5, row, hour);
  drawTwoDigitsAt(8, row, minute);
  drawBlinkAt(editHour ? 5 : 8, row, blinkOn);
}

static void drawDateEntryFrame(MsgId header, MsgId prompt, uint8_t row) {
  drawHeaderFrame(header);
  lcdPrint_P(prompt, 1);
  lcdClearLine(row);
  lcdSetCursor(0, row);
  lcdPrint_P(MSG_DATE_LABEL);
  lcdSetCursor(7, row);
  lcd.print('/');
  lcdSetCursor(10, row);
  lcd.print('/');
}

static void drawDateEntryLine(uint8_t day, uint8_t month, uint8_t year, uint8_t row, bool blinkOn, uint8_t field) {
  drawTwoDigitsAt(5, row, day);
  drawTwoDigitsAt(8, row, month);
  drawTwoDigitsAt(11, row, year);
  uint8_t col = (field == 0) ? 5 : (field == 1) ? 8 : 11;
  drawBlinkAt(col, row, blinkOn);
}

static void drawLightsOnOffFrame(MsgId header) {
  drawHeaderFrame(header);
  lcdClearLine(1);
  lcdSetCursor(0, 1);
  lcdPrint_P(MSG_ON);
  lcd.print(' ');
  lcdSetCursor(5, 1);
  lcd.print(':');
  lcdSetCursor(9, 1);
  lcdPrint_P(MSG_OFF);
  lcd.print(' ');
  lcdSetCursor(15, 1);
  lcd.print(':');
  lcdClearLine(2);
}

static void drawLightsOnOffValues(uint8_t onHour, uint8_t onMinute, uint8_t offHour, uint8_t offMinute,
                                  uint8_t field, bool blinkOn) {
  static const uint8_t kRow = 1;
  static const uint8_t kOnCol = 3;
  static const uint8_t kOffCol = 13;

  drawTwoDigitsAt(kOnCol, kRow, onHour);
  drawTwoDigitsAt(kOnCol + 3, kRow, onMinute);
  drawTwoDigitsAt(kOffCol, kRow, offHour);
  drawTwoDigitsAt(kOffCol + 3, kRow, offMinute);

  uint8_t col = kOnCol;
  if (field == 1) col = kOnCol + 3;
  else if (field == 2) col = kOffCol;
  else if (field == 3) col = kOffCol + 3;
  drawBlinkAt(col, kRow, blinkOn);
}

static bool label_is_empty(const LabelRef *label) {
  if (!label) return true;
  if (label->is_ram) return !label->ptr || label->ptr[0] == '\0';
  return label->id == MSG_LITTLE;
}

static void lcdPrintLabel(const LabelRef *label) {
  if (!label) return;
  if (label->is_ram) {
    if (!label->ptr || !label->ptr[0]) return;
    lcd.print(label->ptr);
  } else {
    if (label->id == MSG_LITTLE) return;
    lcdPrint_P(label->id);
  }
}

static void drawYesNoCursor(bool yesSelected) {
  lcdSetCursor(0, 2);
  lcd.write((uint8_t)(yesSelected ? 2 : ' '));
  lcdSetCursor(0, 3);
  lcd.write((uint8_t)(yesSelected ? ' ' : 2));
}

void setChoices_P(MsgId label0, int value0, MsgId label1, int value1, MsgId label2, int value2, MsgId label3, int value3, MsgId label4, int value4, MsgId label5, int value5, MsgId label6, int value6, MsgId label7, int value7, MsgId label8, int value8, MsgId label9, int value9) {
  choices[0].label.ptr = nullptr;
  choices[0].label.id = label0;
  choices[0].label.is_ram = 0;
  choices[1].label.ptr = nullptr;
  choices[1].label.id = label1;
  choices[1].label.is_ram = 0;
  choices[2].label.ptr = nullptr;
  choices[2].label.id = label2;
  choices[2].label.is_ram = 0;
  choices[3].label.ptr = nullptr;
  choices[3].label.id = label3;
  choices[3].label.is_ram = 0;
  choices[4].label.ptr = nullptr;
  choices[4].label.id = label4;
  choices[4].label.is_ram = 0;
  choices[5].label.ptr = nullptr;
  choices[5].label.id = label5;
  choices[5].label.is_ram = 0;
  choices[6].label.ptr = nullptr;
  choices[6].label.id = label6;
  choices[6].label.is_ram = 0;
  choices[7].label.ptr = nullptr;
  choices[7].label.id = label7;
  choices[7].label.is_ram = 0;
  choices[8].label.ptr = nullptr;
  choices[8].label.id = label8;
  choices[8].label.is_ram = 0;
  choices[9].label.ptr = nullptr;
  choices[9].label.id = label9;
  choices[9].label.is_ram = 0;

  choices[0].value = value0;
  choices[1].value = value1;
  choices[2].value = value2;
  choices[3].value = value3;
  choices[4].value = value4;
  choices[5].value = value5;
  choices[6].value = value6;
  choices[7].value = value7;
  choices[8].value = value8;
  choices[9].value = value9;
}

void setChoicesHeader_P(MsgId header) {
  choicesHeader.ptr = nullptr;
  choicesHeader.id = header;
  choicesHeader.is_ram = 0;
}

void setChoice_P(unsigned char index, MsgId label, int value) {
  if (index >= (sizeof(choices) / sizeof(choices[0]))) return;
  choices[index].label.ptr = nullptr;
  choices[index].label.id = label;
  choices[index].label.is_ram = 0;
  choices[index].value = value;
}

void setChoice_R(unsigned char index, const char *label, int value) {
  if (index >= (sizeof(choices) / sizeof(choices[0]))) return;
  choices[index].label.ptr = label;
  choices[index].label.id = MSG_LITTLE;
  choices[index].label.is_ram = 1;
  choices[index].value = value;
}


/* ***************************************** */
/* *     LOW LEVEL UI FUNCTIONS              */
/* ***************************************** */


int16_t inputNumber_P(MsgId prompt, int16_t initialUserInput, int stepSize, int16_t min, int16_t max, MsgId postFix, MsgId optionalHeader) {
  int16_t userInput;
  bool displayChanged = true;
  const uint8_t inputY = 3;

  // Check if initialUserInput is not empty
  if (initialUserInput != -1) {
    userInput = initialUserInput;
  } else {
    userInput = 0;
  }

  drawPromptAndHeader(prompt, optionalHeader);

  while (true) {
    if (displayChanged) {
      lcd.setCursor(0, inputY);
      lcd.print(userInput);
      lcdPrint_P(postFix);    // Display postFix after userInput
      lcdPrintSpaces(3);      // Clear the remaining characters

      displayChanged = false;
    }

    analogButtonsCheck();  // This will set the global `pressedButton`

    if (pressedButton == &okButton || pressedButton == &rightButton) {
      // If OK button is pressed, return the input string
      return userInput;
    } else if (pressedButton == &upButton) {
      userInput = userInput + stepSize <= max ? userInput + stepSize : userInput;
      displayChanged = true;
    } else if (pressedButton == &downButton) {
      // If UP button is pressed, increment the character
      userInput = userInput - stepSize >= min ? userInput - stepSize : userInput;
      displayChanged = true;

    } else if (pressedButton == &leftButton) {
      return -1;
    } else {
      // ...
    }
  }
}

bool inputTime_P(MsgId header, MsgId prompt, uint16_t initialMinutes, uint16_t *outMinutes) {
  if (!outMinutes) return false;

  uint8_t hour = initialMinutes / 60;
  uint8_t minute = initialMinutes % 60;
  bool editHour = true;
  bool cursorVisible = true;
  bool displayChanged = true;
  unsigned long lastBlinkAt = 0;
  const uint8_t timeRow = 2;

  drawTimeEntryFrame(header, prompt, timeRow);

  while (true) {
    unsigned long now = millis();
    updateBlink(&cursorVisible, &lastBlinkAt, &displayChanged, now);

    if (displayChanged) {
      drawTimeEntryLine(hour, minute, timeRow, cursorVisible, editHour);
      displayChanged = false;
    }

    analogButtonsCheck();
    if (pressedButton) resetBlink(&cursorVisible, &lastBlinkAt, &displayChanged, now);

    if (pressedButton == &upButton) {
      if (editHour) hour = static_cast<uint8_t>((hour + 1) % 24);
      else minute = static_cast<uint8_t>((minute + 1) % 60);
      displayChanged = true;
    } else if (pressedButton == &downButton) {
      if (editHour) hour = static_cast<uint8_t>((hour + 23) % 24);
      else minute = static_cast<uint8_t>((minute + 59) % 60);
      displayChanged = true;
    } else if (pressedButton == &rightButton || pressedButton == &okButton) {
      if (editHour) {
        editHour = false;
        displayChanged = true;
      } else {
        *outMinutes = static_cast<uint16_t>(hour) * 60u + minute;
        return true;
      }
    } else if (pressedButton == &leftButton) {
      if (editHour) return false;
      editHour = true;
      displayChanged = true;
    }
  }
}

bool inputDate_P(MsgId header, MsgId prompt, uint8_t *day, uint8_t *month, uint8_t *year) {
  if (!day || !month || !year) return false;

  uint8_t d = *day;
  uint8_t m = *month;
  uint8_t y = *year;
  if (m == 0 || m > 12) m = 1;
  if (d == 0 || d > 31) d = 1;
  uint8_t maxDay = daysInMonth(m, y);
  if (d > maxDay) d = maxDay;

  uint8_t field = 0;
  bool cursorVisible = true;
  bool displayChanged = true;
  unsigned long lastBlinkAt = 0;
  const uint8_t dateRow = 2;

  drawDateEntryFrame(header, prompt, dateRow);

  while (true) {
    unsigned long now = millis();
    updateBlink(&cursorVisible, &lastBlinkAt, &displayChanged, now);

    if (displayChanged) {
      drawDateEntryLine(d, m, y, dateRow, cursorVisible, field);
      displayChanged = false;
    }

    analogButtonsCheck();
    if (pressedButton) resetBlink(&cursorVisible, &lastBlinkAt, &displayChanged, now);

    if (pressedButton == &upButton) {
      if (field == 0) {
        uint8_t limit = daysInMonth(m, y);
        d = (d >= limit) ? 1 : static_cast<uint8_t>(d + 1);
      } else if (field == 1) {
        m = (m >= 12) ? 1 : static_cast<uint8_t>(m + 1);
        maxDay = daysInMonth(m, y);
        if (d > maxDay) d = maxDay;
      } else {
        y = static_cast<uint8_t>(y + 1);
        maxDay = daysInMonth(m, y);
        if (d > maxDay) d = maxDay;
      }
      displayChanged = true;
    } else if (pressedButton == &downButton) {
      if (field == 0) {
        uint8_t limit = daysInMonth(m, y);
        d = (d <= 1) ? limit : static_cast<uint8_t>(d - 1);
      } else if (field == 1) {
        m = (m <= 1) ? 12 : static_cast<uint8_t>(m - 1);
        maxDay = daysInMonth(m, y);
        if (d > maxDay) d = maxDay;
      } else {
        y = static_cast<uint8_t>(y - 1);
        maxDay = daysInMonth(m, y);
        if (d > maxDay) d = maxDay;
      }
      displayChanged = true;
    } else if (pressedButton == &rightButton || pressedButton == &okButton) {
      if (field < 2) {
        field++;
        displayChanged = true;
      } else {
        *day = d;
        *month = m;
        *year = y;
        return true;
      }
    } else if (pressedButton == &leftButton) {
      if (field == 0) return false;
      field--;
      displayChanged = true;
    }
  }
}

bool inputLightsOnOff_P(MsgId header, uint16_t *onMinutes, uint16_t *offMinutes) {
  if (!onMinutes || !offMinutes) return false;

  uint16_t on = *onMinutes;
  uint16_t off = *offMinutes;
  if (on > 1439) on = 0;
  if (off > 1439) off = 0;

  uint8_t onHour = on / 60;
  uint8_t onMinute = on % 60;
  uint8_t offHour = off / 60;
  uint8_t offMinute = off % 60;
  uint8_t field = 0;
  bool cursorVisible = true;
  bool displayChanged = true;
  unsigned long lastBlinkAt = 0;

  drawLightsOnOffFrame(header);

  while (true) {
    unsigned long now = millis();
    updateBlink(&cursorVisible, &lastBlinkAt, &displayChanged, now);

    if (displayChanged) {
      drawLightsOnOffValues(onHour, onMinute, offHour, offMinute, field, cursorVisible);
      displayChanged = false;
    }

    analogButtonsCheck();
    if (pressedButton) resetBlink(&cursorVisible, &lastBlinkAt, &displayChanged, now);

    if (pressedButton == &upButton) {
      switch (field) {
        case 0: onHour = static_cast<uint8_t>((onHour + 1) % 24); break;
        case 1: onMinute = static_cast<uint8_t>((onMinute + 1) % 60); break;
        case 2: offHour = static_cast<uint8_t>((offHour + 1) % 24); break;
        case 3: offMinute = static_cast<uint8_t>((offMinute + 1) % 60); break;
      }
      displayChanged = true;
    } else if (pressedButton == &downButton) {
      switch (field) {
        case 0: onHour = static_cast<uint8_t>((onHour + 23) % 24); break;
        case 1: onMinute = static_cast<uint8_t>((onMinute + 59) % 60); break;
        case 2: offHour = static_cast<uint8_t>((offHour + 23) % 24); break;
        case 3: offMinute = static_cast<uint8_t>((offMinute + 59) % 60); break;
      }
      displayChanged = true;
    } else if (pressedButton == &rightButton || pressedButton == &okButton) {
      if (field < 3) {
        field++;
        displayChanged = true;
      } else {
        *onMinutes = static_cast<uint16_t>(onHour) * 60u + onMinute;
        *offMinutes = static_cast<uint16_t>(offHour) * 60u + offMinute;
        return true;
      }
    } else if (pressedButton == &leftButton) {
      if (field == 0) return false;
      field--;
      displayChanged = true;
    }
  }
}

void resetChoicesAndHeader() {
  for (uint8_t i = 0; i < 10; ++i) {
    choices[i].label.ptr = nullptr;
    choices[i].label.id = MSG_LITTLE;
    choices[i].label.is_ram = 0;
    choices[i].value = 0;
  }
  setChoicesHeader_P(MSG_LITTLE);
}

int8_t selectChoice(int howManyChoices, int initialUserInput, bool doNotClear) {
  int selectedIndex = 0;      // Default selection is the first choice
  int firstVisibleIndex = 0;  // Index of the first choice visible on the screen
  bool displayChanged = true;
  const uint8_t headerY = 0;
  const bool hasHeader = !label_is_empty(&choicesHeader);
  const uint8_t maxVisibleChoices = hasHeader ? 3 : 4;
  const uint8_t choicesStartY = hasHeader ? 1 : 0;

  if (!doNotClear) lcd.clear();

  // Find the index of the choice matching the initialUserInput
  for (int i = 0; i < howManyChoices; i++) {
    if (choices[i].value == initialUserInput) {
      selectedIndex = i;
      break;
    }
  }

  // Adjust the starting visible index if the selected index is outside the first page's range
  if (selectedIndex >= maxVisibleChoices) {
    firstVisibleIndex = selectedIndex - maxVisibleChoices + 1;
  }


  if (hasHeader) {
    lcd.setCursor(0, headerY);
    lcdPrintLabel(&choicesHeader);
  }

  int lastSelectedIndex = -1;
  int lastFirstVisibleIndex = -1;

  while (true) {
    if (displayChanged) {
      if (firstVisibleIndex != lastFirstVisibleIndex) {
        for (int i = 0; i < maxVisibleChoices; i++) {
          int choiceIndex = firstVisibleIndex + i;
          if (choiceIndex < howManyChoices) {
            lcd.setCursor(0, choicesStartY + i);
            lcdPrintSpaces(DISPLAY_COLUMNS);
            lcd.setCursor(0, choicesStartY + i);
            if (choiceIndex == selectedIndex) {
              lcd.write((uint8_t)2);  // Print cursor
            } else {
              lcd.print(' ');
            }

            lcdPrintLabel(&choices[choiceIndex].label);
          }
        }
      } else if (lastSelectedIndex != -1 && lastSelectedIndex != selectedIndex) {
        int prevRow = lastSelectedIndex - firstVisibleIndex;
        int nextRow = selectedIndex - firstVisibleIndex;
        if (prevRow >= 0 && prevRow < maxVisibleChoices) {
          lcd.setCursor(0, choicesStartY + prevRow);
          lcd.print(' ');
        }
        if (nextRow >= 0 && nextRow < maxVisibleChoices) {
          lcd.setCursor(0, choicesStartY + nextRow);
          lcd.write((uint8_t)2);
        }
      } else if (lastSelectedIndex == -1) {
        int row = selectedIndex - firstVisibleIndex;
        if (row >= 0 && row < maxVisibleChoices) {
          lcd.setCursor(0, choicesStartY + row);
          lcd.write((uint8_t)2);
        }
      }

      lastSelectedIndex = selectedIndex;
      lastFirstVisibleIndex = firstVisibleIndex;
      displayChanged = false;
    }

    // Keep background sensor tasks alive even while in blocking menus
    runSoilSensorLazyReadings();

    analogButtonsCheck();  // This will set the global `pressedButton`

    if (pressedButton == &okButton || pressedButton == &rightButton) {
      // If OK button is pressed, return the code of the selected choice
      // Serial.print('AAA');
      // Serial.print(selectedIndex);
      int8_t v = choices[selectedIndex].value;
      resetChoicesAndHeader();
      return v;
    } else if (pressedButton == &downButton) {
      // If DOWN button is pressed, move cursor to the next choice
      selectedIndex++;
      if (selectedIndex >= howManyChoices) {
        selectedIndex = 0;  // Wrap around to the first option
        firstVisibleIndex = 0;
      } else if (selectedIndex >= firstVisibleIndex + maxVisibleChoices) {
        firstVisibleIndex++;
      }
      displayChanged = true;
    } else if (pressedButton == &upButton) {
      // If UP button is pressed, move cursor to the previous choice
      selectedIndex--;
      if (selectedIndex < 0) {
        selectedIndex = howManyChoices - 1;  // Wrap around to the last option
        firstVisibleIndex = max(0, howManyChoices - maxVisibleChoices);
      } else if (selectedIndex < firstVisibleIndex) {
        firstVisibleIndex--;
      }
      displayChanged = true;
    } else if (pressedButton == &leftButton) {
      // If LEFT button is pressed, move cursor to the previous choice
      resetChoicesAndHeader();
      return -1;
    }
  }
}

void lcdFlashMessage_P(MsgId message, MsgId message2, uint16_t time) {
  lcdClear();
  lcdSetCursor(0, 1);
  lcdPrint_P(message);
  lcdSetCursor(0, 2);
  lcdPrint_P(message2);

  delay(time);
}

bool alert_P(MsgId warning) {
  setChoices_P(MSG_OK, 1);
  setChoicesHeader_P(warning);

  selectChoice(1, 1);
  return true;
}

int8_t promptYesNoWithHeader(MsgId header, MsgId question, bool initialYes) {
  bool yesSelected = initialYes;

  lcdClear();
  lcdPrint_P(header, 0);
  lcdPrint_P(question, 1);

  lcdClearLine(2);
  lcdSetCursor(1, 2);
  lcdPrint_P(MSG_YES);

  lcdClearLine(3);
  lcdSetCursor(1, 3);
  lcdPrint_P(MSG_NO);

  drawYesNoCursor(yesSelected);

  while (true) {
    analogButtonsCheck();

    if (pressedButton == &okButton || pressedButton == &rightButton) {
      return yesSelected ? 1 : 0;
    }
    if (pressedButton == &leftButton) {
      return -1;
    }
    if (pressedButton == &upButton || pressedButton == &downButton) {
      yesSelected = !yesSelected;
      drawYesNoCursor(yesSelected);
    }
  }
}

static int8_t yesOrNoCommon(MsgId question, bool initialUserInput, bool showAbort) {
  setChoices_P(MSG_YES, 1, MSG_NO, 0);
  setChoicesHeader_P(question);

  int8_t response = selectChoice(2, initialUserInput ? 1 : 0);
  if (response == -1) {
    if (showAbort) lcdFlashMessage_P(MSG_ABORTED);
    return -1;
  }
  return response == 1 ? 1 : 0;
}

int8_t yesOrNo_P(MsgId question, bool initialUserInput) {
  return yesOrNoCommon(question, initialUserInput, true);
}

int8_t yesOrNo_P_NoAbort(MsgId question, bool initialUserInput) {
  return yesOrNoCommon(question, initialUserInput, false);
}

int8_t giveOk_P(MsgId top, MsgId promptText, MsgId line2, MsgId line3) {
  setChoices_P(promptText, 1);
  setChoicesHeader_P(top);
  lcdPrint_P(line2, 2);
  lcdPrint_P(line3, 3);
  int8_t response = selectChoice(1, 1, true);
  if (response == -1) {
    lcdFlashMessage_P(MSG_ABORTED);
    return -1;
  }

  return 1;
}
