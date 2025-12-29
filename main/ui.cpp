#include <LiquidCrystal_I2C.h>
#include <AnalogButtons.h>
#include "ui.h"
#include "messages.h"
#include "config.h"
#include "moistureSensor.h"

// Private functions (not declared in ui.h)
static void labelcpy_P(char *destination, PGM_P source);
static void labelcpy_R(char *destination, const char *source);
void upClick();
void downClick();
void leftClick();
void rightClick();
void okClick();
extern Config config;

LiquidCrystal_I2C lcd = LiquidCrystal_I2C(0x27, 20, 4);


AnalogButtons analogButtons = AnalogButtons(BUTTONS_PIN, BUTTONS_PIN_MODE, BUTTONS_DEBOUNCE_MULTIPLIER, BUTTONS_ANALOG_MARGIN);

Button *pressedButton;
Choice choices[10];

LabelRef choicesHeader;

Button upButton;
Button leftButton;
Button downButton;
Button rightButton;
Button okButton;

void initializeButtons() {
    upButton = Button(config.kbdUp, &upClick, nullptr, BUTTONS_HOLD_DURATION_MS, BUTTONS_HOLD_INTERVAL_MS);
    leftButton = Button(config.kbdLeft, &leftClick);
    rightButton = Button(config.kbdRight, &rightClick);
    downButton = Button(config.kbdDown, &downClick, nullptr, BUTTONS_HOLD_DURATION_MS, BUTTONS_HOLD_INTERVAL_MS);
    okButton = Button(config.kbdOk, &okClick);

    analogButtons.add(upButton);
    analogButtons.add(leftButton);
    analogButtons.add(rightButton);
    analogButtons.add(downButton);
    analogButtons.add(okButton);

}

void upClick() {
  pressedButton = &upButton;
}
void downClick() {
  pressedButton = &downButton;
}
void leftClick() {
  pressedButton = &leftButton;
}
void rightClick() {
  pressedButton = &rightButton;
}
void okClick() {
  pressedButton = &okButton;
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

  analogButtons.check();
  updateSensorIndicator();
}

void lcdClear() {
  lcd.clear();
}

void lcdClearLine(uint8_t y) {
  lcdSetCursor(0, y);
  lcdPrint_P(MSG_SPACES);
}

void lcdPrintBool(bool b, int8_t y) {
  if (y != -1) {
    lcdClearLine(y);
    lcdSetCursor(0, y);
  }
  lcd.print(b);
}


void lcdPrint_P(PGM_P message, int8_t y = -1) {
  char buffer[LABEL_LENGTH + 1];

  if (y != -1) {
    lcdClearLine(y);
    lcdSetCursor(0, y);
  }
  labelcpy_P(buffer, message);
  lcd.print(buffer);
}

void lcdPrint_R(const char *message, int8_t y = -1) {
  char buffer[LABEL_LENGTH + 1];

  if (y != -1) {
    lcdClearLine(y);
    lcdSetCursor(0, y);
  }
  labelcpy_R(buffer, message);
  lcd.print(buffer);
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

const byte barLevel1[8] = {
  B00000,
  B00000,
  B00000,
  B00000,
  B00000,
  B00000,
  B11111,
  B11111
};

const byte barLevel2[8] = {
  B00000,
  B00000,
  B00000,
  B00000,
  B11111,
  B11111,
  B11111,
  B11111
};

const byte barLevel3[8] = {
  B00000,
  B00000,
  B11111,
  B11111,
  B11111,
  B11111,
  B11111,
  B11111
};

const byte barLevel4[8] = {
  B11111,
  B11111,
  B11111,
  B11111,
  B11111,
  B11111,
  B11111,
  B11111
};

static void labelcpy_R(char *destination, const char *source) {
  if (!source) {
    destination[0] = '\0';
    return;
  }
  strncpy(destination, source, LABEL_LENGTH);
  destination[strnlen(destination, LABEL_LENGTH)] = '\0';
}

void initLcd() {
  lcd.init();
  delay(1000);
  lcd.backlight();

  lcd.createChar(0, fullSquare);
  lcd.createChar(1, leftArrow);
  lcd.createChar(2, rightArrow);
  lcd.createChar(3, sensorDot);
  lcd.createChar(4, barLevel1);
  lcd.createChar(5, barLevel2);
  lcd.createChar(6, barLevel3);
  lcd.createChar(7, barLevel4);

  resetChoicesAndHeader();
}

static void printTwoDigits(uint8_t value) {
  if (value < 10) lcd.print('0');
  lcd.print(value);
}

static void drawTwoDigitsAt(uint8_t col, uint8_t row, uint8_t value) {
  lcdSetCursor(col, row);
  printTwoDigits(value);
}

static void drawBlinkAt(uint8_t col, uint8_t row, bool blinkOn) {
  if (blinkOn) return;
  lcdSetCursor(col, row);
  lcd.write((uint8_t)0);
  lcdSetCursor(col + 1, row);
  lcd.write((uint8_t)0);
}

static void drawTimeEntryFrame(PGM_P header, PGM_P prompt, uint8_t row) {
  lcdClear();
  lcdPrint_P(header, 0);
  lcdPrint_P(prompt, 1);
  lcdClearLine(row);
  lcdSetCursor(0, row);
  lcdPrint_P(PSTR("Time "));
  lcdSetCursor(7, row);
  lcd.print(':');
  lcdClearLine(3);
}

static void drawTimeEntryLine(uint8_t hour, uint8_t minute, uint8_t row, bool blinkOn, bool editHour) {
  drawTwoDigitsAt(5, row, hour);
  drawTwoDigitsAt(8, row, minute);
  drawBlinkAt(editHour ? 5 : 8, row, blinkOn);
}

static void labelcpy_P(char *destination, PGM_P source) {
  if (!source) {
    destination[0] = '\0';
    return;
  }
  strncpy_P(destination, source, LABEL_LENGTH);
  destination[strnlen_P(source, LABEL_LENGTH)] = '\0';
}

static bool label_is_empty(const LabelRef *label) {
  if (!label || !label->ptr) return true;
  if (label->is_progmem) return pgm_read_byte(label->ptr) == '\0';
  return label->ptr[0] == '\0';
}

static void lcdPrintLabel(const LabelRef *label) {
  if (!label || !label->ptr) return;
  if (label->is_progmem) {
    lcdPrint_P((PGM_P)label->ptr);
  } else {
    lcdPrint_R(label->ptr);
  }
}

void setChoices_P(PGM_P label0 = MSG_LITTLE, int value0 = 0, PGM_P label1 = MSG_LITTLE, int value1 = 0, PGM_P label2 = MSG_LITTLE, int value2 = 0, PGM_P label3 = MSG_LITTLE, int value3 = 0, PGM_P label4 = MSG_LITTLE, int value4 = 0, PGM_P label5 = MSG_LITTLE, int value5 = 0, PGM_P label6 = MSG_LITTLE, int value6 = 0, PGM_P label7 = MSG_LITTLE, int value7 = 0, PGM_P label8 = MSG_LITTLE, int value8 = 0, PGM_P label9 = MSG_LITTLE, int value9 = 0) {
  choices[0].label.ptr = (const char *)label0;
  choices[0].label.is_progmem = 1;
  choices[1].label.ptr = (const char *)label1;
  choices[1].label.is_progmem = 1;
  choices[2].label.ptr = (const char *)label2;
  choices[2].label.is_progmem = 1;
  choices[3].label.ptr = (const char *)label3;
  choices[3].label.is_progmem = 1;
  choices[4].label.ptr = (const char *)label4;
  choices[4].label.is_progmem = 1;
  choices[5].label.ptr = (const char *)label5;
  choices[5].label.is_progmem = 1;
  choices[6].label.ptr = (const char *)label6;
  choices[6].label.is_progmem = 1;
  choices[7].label.ptr = (const char *)label7;
  choices[7].label.is_progmem = 1;
  choices[8].label.ptr = (const char *)label8;
  choices[8].label.is_progmem = 1;
  choices[9].label.ptr = (const char *)label9;
  choices[9].label.is_progmem = 1;

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

void setChoices_R(const char *label0 = "", int value0 = 0, const char *label1 = "", int value1 = 0, const char *label2 = "", int value2 = 0, const char *label3 = "", int value3 = 0, const char *label4 = "", int value4 = 0, const char *label5 = "", int value5 = 0, const char *label6 = "", int value6 = 0, const char *label7 = "", int value7 = 0, const char *label8 = "", int value8 = 0, const char *label9 = "", int value9 = 0) {
  choices[0].label.ptr = label0;
  choices[0].label.is_progmem = 0;
  choices[1].label.ptr = label1;
  choices[1].label.is_progmem = 0;
  choices[2].label.ptr = label2;
  choices[2].label.is_progmem = 0;
  choices[3].label.ptr = label3;
  choices[3].label.is_progmem = 0;
  choices[4].label.ptr = label4;
  choices[4].label.is_progmem = 0;
  choices[5].label.ptr = label5;
  choices[5].label.is_progmem = 0;
  choices[6].label.ptr = label6;
  choices[6].label.is_progmem = 0;
  choices[7].label.ptr = label7;
  choices[7].label.is_progmem = 0;
  choices[8].label.ptr = label8;
  choices[8].label.is_progmem = 0;
  choices[9].label.ptr = label9;
  choices[9].label.is_progmem = 0;

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

void setChoicesHeader_P(PGM_P header = MSG_LITTLE) {
  choicesHeader.ptr = (const char *)header;
  choicesHeader.is_progmem = 1;
}

void setChoicesHeader_R(const char *header = "") {
  choicesHeader.ptr = header;
  choicesHeader.is_progmem = 0;
}

void setChoice_P(unsigned char index, PGM_P label, int value = 0) {
  if (index >= (sizeof(choices) / sizeof(choices[0]))) return;
  choices[index].label.ptr = (const char *)label;
  choices[index].label.is_progmem = 1;
  choices[index].value = value;
}

void setChoice_R(unsigned char index, const char *label, int value = 0) {
  if (index >= (sizeof(choices) / sizeof(choices[0]))) return;
  choices[index].label.ptr = label;
  choices[index].label.is_progmem = 0;
  choices[index].value = value;
}


/* ***************************************** */
/* *     LOW LEVEL UI FUNCTIONS              */
/* ***************************************** */


long int inputNumber_P(PGM_P prompt, long int initialUserInput, int stepSize = 1, long int min = 0, long int max = 100, PGM_P postFix = MSG_LITTLE, PGM_P optionalHeader = MSG_LITTLE) {
  long int userInput;
  bool displayChanged = true;
  const uint8_t headerY = 0;
  const uint8_t promptY = 2;
  const uint8_t inputY = 3;

  // Check if initialUserInput is not empty
  if (initialUserInput != -1) {
    userInput = initialUserInput;
  } else {
    userInput = 0;
  }

  lcd.clear();

  lcd.setCursor(0, promptY);
  lcdPrint_P(prompt);

  lcd.setCursor(0, headerY);
  lcdPrint_P(optionalHeader);

  while (true) {
    if (displayChanged) {
      lcd.setCursor(0, inputY);
      lcd.print(userInput);
      lcdPrint_P(postFix);    // Display postFix after userInput
      lcdPrint_P(MSG_SPACE);  // Clear the remaining characters
      lcdPrint_P(MSG_SPACE);  // Clear the remaining characters
      lcdPrint_P(MSG_SPACE);  // Clear the remaining characters

      displayChanged = false;
    }

    analogButtonsCheck();  // This will set the global `pressedButton`

    if (pressedButton == &okButton) {
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
      displayChanged = true;
    } else {
      // ...
    }
  }
}

long int inputNumber_R(const char *prompt, long int initialUserInput, int stepSize = 1, long int min = 0, long int max = 100, const char *postFix = "", const char *optionalHeader = "") {
  long int userInput;
  bool displayChanged = true;
  const uint8_t headerY = 0;
  const uint8_t promptY = 2;
  const uint8_t inputY = 3;

  // Check if initialUserInput is not empty
  if (initialUserInput != -1) {
    userInput = initialUserInput;
  } else {
    userInput = 0;
  }

  lcd.clear();

  lcd.setCursor(0, promptY);
  lcdPrint_R(prompt);

  lcd.setCursor(0, headerY);
  lcdPrint_R(optionalHeader);

  while (true) {
    if (displayChanged) {
      lcd.setCursor(0, inputY);
      lcd.print(userInput);
      lcdPrint_R(postFix);    // Display postFix after userInput
      lcdPrint_P(MSG_SPACE);  // Clear the remaining characters
      lcdPrint_P(MSG_SPACE);  // Clear the remaining characters
      lcdPrint_P(MSG_SPACE);  // Clear the remaining characters

      displayChanged = false;
    }

    analogButtonsCheck();  // This will set the global `pressedButton`

    if (pressedButton == &okButton) {
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
      displayChanged = true;
    } else {
      // ...
    }
  }
}

bool inputTime_P(PGM_P header, PGM_P prompt, uint16_t initialMinutes, uint16_t *outMinutes) {
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
    if (now - lastBlinkAt >= 500) {
      cursorVisible = !cursorVisible;
      lastBlinkAt = now;
      displayChanged = true;
    }

    if (displayChanged) {
      drawTimeEntryLine(hour, minute, timeRow, cursorVisible, editHour);
      displayChanged = false;
    }

    analogButtonsCheck();

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

static void drawDateTimeFrame(PGM_P header) {
  lcdClear();
  lcdPrint_P(header, 0);
  lcdSetCursor(0, 1);
  lcdPrint_P(PSTR("Time "));
  lcdSetCursor(7, 1);
  lcd.print(':');
  lcdSetCursor(0, 2);
  lcdPrint_P(PSTR("Date "));
  lcdSetCursor(7, 2);
  lcd.print('/');
  lcdSetCursor(10, 2);
  lcd.print('/');
  lcdClearLine(3);
}

static void drawDateTimeValues(uint8_t hour, uint8_t minute, uint8_t day, uint8_t month, uint8_t year,
                               uint8_t field, bool blinkOn) {
  drawTwoDigitsAt(5, 1, hour);
  drawTwoDigitsAt(8, 1, minute);
  drawTwoDigitsAt(5, 2, day);
  drawTwoDigitsAt(8, 2, month);
  drawTwoDigitsAt(11, 2, year);

  uint8_t row = (field < 2) ? 1 : 2;
  uint8_t col = 5;
  switch (field) {
    case 0: col = 5; break;
    case 1: col = 8; break;
    case 2: col = 5; break;
    case 3: col = 8; break;
    case 4: col = 11; break;
    default: col = 5; break;
  }
  drawBlinkAt(col, row, blinkOn);
}

bool inputDateTime_P(PGM_P header, uint8_t *hour, uint8_t *minute, uint8_t *day, uint8_t *month, uint8_t *year) {
  if (!hour || !minute || !day || !month || !year) return false;

  uint8_t field = 0;
  bool cursorVisible = true;
  bool displayChanged = true;
  unsigned long lastBlinkAt = 0;

  drawDateTimeFrame(header);

  while (true) {
    unsigned long now = millis();
    if (now - lastBlinkAt >= 500) {
      cursorVisible = !cursorVisible;
      lastBlinkAt = now;
      displayChanged = true;
    }

    if (displayChanged) {
      drawDateTimeValues(*hour, *minute, *day, *month, *year, field, cursorVisible);
      displayChanged = false;
    }

    analogButtonsCheck();

    if (pressedButton == &upButton) {
      switch (field) {
        case 0: *hour = static_cast<uint8_t>((*hour + 1) % 24); break;
        case 1: *minute = static_cast<uint8_t>((*minute + 1) % 60); break;
        case 2: *day = (*day >= 31) ? 1 : static_cast<uint8_t>(*day + 1); break;
        case 3: *month = (*month >= 12) ? 1 : static_cast<uint8_t>(*month + 1); break;
        case 4: *year = (*year >= 99) ? 0 : static_cast<uint8_t>(*year + 1); break;
      }
      displayChanged = true;
    } else if (pressedButton == &downButton) {
      switch (field) {
        case 0: *hour = (*hour == 0) ? 23 : static_cast<uint8_t>(*hour - 1); break;
        case 1: *minute = (*minute == 0) ? 59 : static_cast<uint8_t>(*minute - 1); break;
        case 2: *day = (*day <= 1) ? 31 : static_cast<uint8_t>(*day - 1); break;
        case 3: *month = (*month <= 1) ? 12 : static_cast<uint8_t>(*month - 1); break;
        case 4: *year = (*year == 0) ? 99 : static_cast<uint8_t>(*year - 1); break;
      }
      displayChanged = true;
    } else if (pressedButton == &rightButton || pressedButton == &okButton) {
      if (field < 4) {
        field++;
        displayChanged = true;
      } else {
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
  for (int i = 0; i < 10; i++) {
    choices[i].label.ptr = nullptr;
    choices[i].label.is_progmem = 0;
    choices[i].value = 0;
  }

  choicesHeader.ptr = nullptr;
  choicesHeader.is_progmem = 0;
}

int8_t selectChoice(int howManyChoices, int initialUserInput, bool doNotClear = false) {
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
            lcdPrint_P(MSG_SPACES);
            lcd.setCursor(0, choicesStartY + i);
            if (choiceIndex == selectedIndex) {
              lcd.write((uint8_t)2);  // Print cursor
            } else {
              lcdPrint_P(MSG_SPACE);
            }

            lcdPrintLabel(&choices[choiceIndex].label);
          }
        }
      } else if (lastSelectedIndex != -1 && lastSelectedIndex != selectedIndex) {
        int prevRow = lastSelectedIndex - firstVisibleIndex;
        int nextRow = selectedIndex - firstVisibleIndex;
        if (prevRow >= 0 && prevRow < maxVisibleChoices) {
          lcd.setCursor(0, choicesStartY + prevRow);
          lcdPrint_P(MSG_SPACE);
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

void lcdFlashMessage_P(PGM_P message, PGM_P message2 = MSG_LITTLE, uint16_t time = 1000) {
  lcdClear();
  lcdSetCursor(0, 1);
  lcdPrint_P(message);
  lcdSetCursor(0, 2);
  lcdPrint_P(message2);

  delay(time);
}

void lcdFlashMessage_R(const char *message, const char *message2 = "", uint16_t time = 1000) {
  lcdClear();
  lcdSetCursor(0, 1);
  lcdPrint_R(message);
  lcdSetCursor(0, 2);
  lcdPrint_R(message2);

  delay(time);
}


bool alert_P(PGM_P warning = MSG_LITTLE) {
  setChoices_P(MSG_OK, 1);
  setChoicesHeader_P(warning);

  int8_t userInput = selectChoice(1, 1);
  return 1;
}

bool alert_R(const char *warning = "") {
  setChoices_P(MSG_OK, 1);
  setChoicesHeader_R(warning);

  int8_t userInput = selectChoice(1, 1);
  return 1;
}

int8_t yesOrNo_P(PGM_P question, bool initialUserInput = true) {
  setChoices_P(MSG_YES, 1, MSG_NO, 0);
  setChoicesHeader_P(question);

  int8_t response = selectChoice(2, initialUserInput ? 1 : 0);
  if (response == -1) {
    lcdFlashMessage_P(MSG_ABORTED);
    return response;
  }
  return response == 1 ? true : false;
}

int8_t yesOrNo_R(const char *question, bool initialUserInput = true) {
  setChoices_P(MSG_YES, 1, MSG_NO, 0);
  setChoicesHeader_R(question);

  int8_t response = selectChoice(2, initialUserInput ? 1 : 0);
  if (response == -1) {
    lcdFlashMessage_P(MSG_ABORTED);
    return response;
  }
  return response == 1 ? true : false;
}

int8_t yesOrNo_P_NoAbort(PGM_P question, bool initialUserInput = true) {
  setChoices_P(MSG_YES, 1, MSG_NO, 0);
  setChoicesHeader_P(question);

  int8_t response = selectChoice(2, initialUserInput ? 1 : 0);
  return response == 1 ? true : (response == 0 ? 0 : -1);
}

int8_t giveOk_P(PGM_P top, PGM_P promptText = MSG_LITTLE, PGM_P line2 = MSG_LITTLE, PGM_P line3 = MSG_LITTLE) {
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

int8_t giveOk_R(const char *top, const char *promptText = "", const char *line2 = "", const char *line3 = "") {
  setChoices_R(promptText, 1);
  setChoicesHeader_R(top);
  lcdPrint_R(line2, 2);
  lcdPrint_R(line3, 3);
  int8_t response = selectChoice(1, 1, true);
  if (response == -1) {
    lcdFlashMessage_P(MSG_ABORTED);
    return -1;
  }

  return 1;
}
