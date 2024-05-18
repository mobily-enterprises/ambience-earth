#include <LiquidCrystal_I2C.h>
#include <AnalogButtons.h>
#include "ui.h"
#include "messages.h"


// Private functions (not declared in ui.h)
void labelcpy(char* destination, const char* source);
void upClick();
void downClick();
void leftClick();
void rightClick();
void okClick();
void sanitizeUserInput(char *userInput);
int actualLength(const char* str);
char getNextCharacter(char currentChar);
char getPreviousCharacter(char currentChar);

LiquidCrystal_I2C lcd = LiquidCrystal_I2C(0x27, 16, 4);

AnalogButtons analogButtons = AnalogButtons(BUTTONS_PIN, BUTTONS_PIN_MODE, BUTTONS_DEBOUNCE_MULTIPLIER, BUTTONS_ANALOG_MARGIN);

Button* pressedButton;
Choice choices[6];
char userInputString[DISPLAY_COLUMNS + 1];
char displayedLine[DISPLAY_COLUMNS + 1];

 
// ACTUAL keypad
Button upButton = Button(35, &upClick);
Button leftButton = Button(2, &leftClick);
Button downButton = Button(91, &downClick);
Button rightButton = Button(170, &rightClick);
Button okButton = Button(352, &okClick);

/*
// Tony's keypad
Button upButton = Button(340, &upClick);
Button leftButton = Button(512, &leftClick);
Button downButton = Button(682, &downClick);
Button rightButton = Button(170, &rightClick);
Button okButton = Button(853, &okClick);
*/


void upClick() { pressedButton = &upButton; }
void downClick() { pressedButton = &downButton; }
void leftClick() { pressedButton = &leftButton; }
void rightClick() { pressedButton = &rightButton; }
void okClick() { pressedButton = &okButton; }

void analogButtonsCheck() {
  pressedButton = nullptr;
  analogButtons.check();
}

void lcdClear() {
  lcd.clear();
}

void lcdClearLine(uint8_t y) {
  lcdSetCursor(0, y);
  labelcpy(displayedLine, "                    ");
  lcd.print(displayedLine);
}

void lcdPrint(char *message, uint8_t y) {
  if (y) {
    lcdClearLine(y);
    lcdSetCursor(0, y);
  }
  // int y = lcd.cursorY();
  // labelCpy(displayedLine, message);
  // displayedLine[DISPLAY_COLUMS - y] = '\0';  
  lcd.print(message);
}

void lcdPrintNumber(int number, uint8_t y = 0) {
  if (y) {
    lcdClearLine(y);
    lcdSetCursor(0, y);
  }
  // int y = lcd.cursorY();
  // labelCpy(displayedLine, message);
  // displayedLine[DISPLAY_COLUMS - y] = '\0';  
  lcd.print(number);
}

void lcdSetCursor(uint8_t x, uint8_t y) {
  lcd.setCursor(x, y);
}

char* getUserInputString() {
  return userInputString;
}

Choice* getChoices() {
  return choices;
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

void labelcpy(char* destination, const char* source) {
  strncpy(destination, source, DISPLAY_COLUMNS);
  destination[strnlen(destination, DISPLAY_COLUMNS)] = '\0';
}

void initLcdAndButtons() {
  lcd.init();
  // delay(1000);
  lcd.backlight(); 
  // delay(1000);

  analogButtons.add(upButton);
  analogButtons.add(leftButton);
  analogButtons.add(downButton);
  analogButtons.add(rightButton);
  analogButtons.add(okButton);

  lcd.createChar(0, fullSquare);
  lcd.createChar(1, leftArrow);
  lcd.createChar(2, rightArrow);
}

void setChoices(const char *label0="",int value0=0,const char *label1="",int value1=0,const char *label2="",int value2=0,const char *label3="",int value3=0,const char *label4="",int value4=0,const char *label5="",int value5=0) {
  labelcpy(choices[0].label, label0);
  labelcpy(choices[1].label, label1);
  labelcpy(choices[2].label, label2);
  labelcpy(choices[3].label, label3);
  labelcpy(choices[4].label, label4);
  labelcpy(choices[5].label, label5);
  choices[0].value = value0;
  choices[1].value = value1;
  choices[2].value = value2;
  choices[3].value = value3;
  choices[4].value = value4;
  choices[5].value = value5;
}

void setChoice(unsigned char index, const char *label="",int value=0) {
  labelcpy(choices[index].label, label);
  choices[index].value = value;
}


char* allowedStringCharacters = " ABCDEFGHIJKLMNOPQRSTUVWXYZ";
int initialCharacterPosition = 1;

int isCharacterAllowed(char character) {
  for (int i = 0; allowedStringCharacters[i]; ++i) {
    if (allowedStringCharacters[i] == character) {
      return i; // Return the index if character is found
    }
  }
  return -1; // Return -1 if character is not found
}

/* ***************************************** */
/* *     LOW LEVEL UI FUNCTIONS              */
/* ***************************************** */

int inputNumber(char *prompt, int initialUserInput, int stepSize = 1, int min = 0, int max = 100, char * postFix = "", char * optionalHeader = "") {
  int userInput;
  bool displayChanged = true;
#define HEADER_Y 0
#define PROMPT_Y 2
#define INPUT_Y 3

  // Check if initialUserInput is not empty
  if (initialUserInput != -1) {
    userInput = initialUserInput;
  } else {
    userInput = 0;
  }

  lcd.clear();

  lcd.setCursor(0, PROMPT_Y);
  lcd.print(prompt);

  lcd.setCursor(0, HEADER_Y);
  lcd.print(optionalHeader);

  while (true) {
    if (displayChanged) {
      lcd.setCursor(0, INPUT_Y);
      lcd.print(userInput);
      lcd.print(postFix);  // Display postFix after userInput
      lcd.print("   ");    // Clear the remaining characters
      displayChanged = false;
    }

    analogButtonsCheck(); // This will set the global `pressedButton`

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
      // Serial.println("MEH");
    }
  }
}


int selectChoice(int howManyChoices, int initialUserInput, char *optionalHeader = "") {
  int selectedIndex = 0;      // Default selection is the first choice
  int firstVisibleIndex = 0;  // Index of the first choice visible on the screen
  bool displayChanged = true;
  unsigned long previousMillis = 0;
  #define HEADER_Y 0
  #define MAX_VISIBLE_CHOICES (optionalHeader != "" ? 3 : 4)  // Maximum number of choices visible on the screen at a time
  #define CHOICES_START_Y (optionalHeader != "" ? 1 : 0)      // Adjust CHOICES_START_Y based on whether optionalHeader is provided or not

  // Find the index of the choice matching the initialUserInput
  for (int i = 0; i < howManyChoices; i++) {
    if (choices[i].value == initialUserInput) {
      selectedIndex = i;
      break;
    }
  }

  // Adjust the starting visible index if the selected index is outside the first page's range
  if (selectedIndex >= MAX_VISIBLE_CHOICES) {
    firstVisibleIndex = selectedIndex - MAX_VISIBLE_CHOICES + 1;
  }

  lcd.clear();

  if (optionalHeader != "") {
    lcd.setCursor(0, HEADER_Y);
    lcd.print(optionalHeader);
  }

  while (true) {
    if (displayChanged) {
      for (int i = 0; i < MAX_VISIBLE_CHOICES; i++) {
        int choiceIndex = firstVisibleIndex + i;
        if (choiceIndex < howManyChoices) {
          lcd.setCursor(0, CHOICES_START_Y + i);
          lcd.print("                    ");
          lcd.setCursor(0, CHOICES_START_Y + i);
          if (choiceIndex == selectedIndex) {
            lcd.write((uint8_t)2);  // Print cursor
            lcd.print("");          // Space between cursor and choice
          } else {
            lcd.print(" ");  // Double space if no cursor
          }

          lcd.print(choices[choiceIndex].label);
        }
      }
      displayChanged = false;
    }

    analogButtonsCheck(); // This will set the global `pressedButton`

    if (pressedButton == &okButton || pressedButton == &rightButton) {
      // If OK button is pressed, return the code of the selected choice
      // Serial.print('AAA');
      // Serial.print(selectedIndex);
      return choices[selectedIndex].value;
    } else if (pressedButton == &downButton) {
      // If DOWN button is pressed, move cursor to the next choice
      selectedIndex++;
      if (selectedIndex >= howManyChoices) {
        selectedIndex = 0;  // Wrap around to the first option
        firstVisibleIndex = 0;
      } else if (selectedIndex >= firstVisibleIndex + MAX_VISIBLE_CHOICES) {
        firstVisibleIndex++;
      }
      displayChanged = true;
    } else if (pressedButton == &upButton) {
      // If UP button is pressed, move cursor to the previous choice
      selectedIndex--;
      if (selectedIndex < 0) {
        selectedIndex = howManyChoices - 1;  // Wrap around to the last option
        firstVisibleIndex = max(0, howManyChoices - MAX_VISIBLE_CHOICES);
      } else if (selectedIndex < firstVisibleIndex) {
        firstVisibleIndex--;
      }
      displayChanged = true;
    } else if (pressedButton == &leftButton) {
      // If LEFT button is pressed, move cursor to the previous choice
      return -1;
    }
  }
}

void lcdAbortedMessage() {
  lcdClear();
  lcdSetCursor(1,0);
  lcdPrint("Aborted!");
  delay(2000);
}

bool alert(char *optionalHeader = "") {
  setChoices("OK", 1);
  
  int userInput = selectChoice(1, 1, optionalHeader);
  return 1;
}

bool confirm(char *question, bool initialUserInput = 1) {
  setChoices("YES", 1, "NO", 0);
  
  int userInput = selectChoice(2, initialUserInput ? 1 : 0, question);
  return userInput == 1 ? true : false;
}

void inputString(char *prompt, char *initialUserInput, char *optionalHeader = MSG_EMPTY) {
  int cursorPosition = 0;
  bool displayChanged = true;
  bool cursorVisible = true;
  unsigned long previousMillis = 0;
#define HEADER_Y 0
#define PROMPT_Y 2
#define INPUT_Y 3

  // Check if initialUserInput is not empty
  if (!strlen(initialUserInput) > 0) {
    labelcpy(userInputString, MSG_SPACES);
    userInputString[0] = allowedStringCharacters[initialCharacterPosition];
  } else {
    labelcpy(userInputString, initialUserInput);
    sanitizeUserInput(userInputString);
    cursorPosition = actualLength(userInputString);
  }

  lcd.clear();

  lcd.setCursor(0, PROMPT_Y);
  lcd.print(prompt);

  lcd.setCursor(0, HEADER_Y);
  lcd.print(optionalHeader);

  while (true) {
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= 500) {  // Cursor blinking interval (500 milliseconds)
      cursorVisible = !cursorVisible;             // Toggle cursor visibility
      previousMillis = currentMillis;             // Update previousMillis
      displayChanged = true;                      // Update display flag for cursor change
    }

    if (displayChanged) {
      if (cursorVisible) {
        lcd.setCursor(0, INPUT_Y);
        lcd.write((uint8_t)1);
        lcd.setCursor(1, INPUT_Y);
        lcd.print(userInputString);
        lcd.setCursor(cursorPosition, INPUT_Y);
        lcd.print(userInputString[cursorPosition - 1]);
      } else {
        lcd.setCursor(0, INPUT_Y);
        lcd.write((uint8_t)1);
        lcd.setCursor(1, INPUT_Y);
        lcd.print(userInputString);
        // Print the full square instead of the character to simulate blinking cursor
        lcd.setCursor(cursorPosition, INPUT_Y);
        lcd.write((uint8_t)0);  // Print custom character fullSquare
      }
      displayChanged = false;
    }
    analogButtonsCheck(); // This will set the global `pressedButton`

    if (pressedButton == &okButton) {
      if (cursorPosition == 0) labelcpy(userInputString, MSG_STAR);
      // If OK button is pressed, return the input string
      return;
    } else if (pressedButton == &downButton) {
      if (cursorPosition != 0) {
        // If UP button is pressed, increment the character
        userInputString[cursorPosition - 1] = getNextCharacter(userInputString[cursorPosition - 1]);
        displayChanged = true;
      }
    } else if (pressedButton == &upButton) {
      if (cursorPosition != 0) {
        // If DOWN button is pressed, decrement the character
        userInputString[cursorPosition - 1] = getPreviousCharacter(userInputString[cursorPosition - 1]);
        displayChanged = true;
      }
    } else if (pressedButton == &rightButton) {
      // If RIGHT button is pressed, move cursor to the next position
      cursorPosition++;
      // If cursor reaches beyond 15, wrap around to 0
      if (cursorPosition > 15) {
        cursorPosition = 0;
      }
      displayChanged = true;
    } else if (pressedButton == &leftButton) {
      // if (cursorPosition == 0) return "*";
      // If LEFT button is pressed, move cursor to the previous position
      cursorPosition--;
      // If cursor goes below 0, wrap around to 15
      if (cursorPosition < 0) {
        // This will count as "go back", the initial value will be considered
        return initialUserInput;
        cursorPosition = 15;
      }
      displayChanged = true;
    } else {
      // Serial.println("MEH");
    }
  }
}

void sanitizeUserInput(char *userInput) {

  // Convert all characters to uppercase
  for (char* ptr = userInput; *ptr; ++ptr) *ptr = toupper(*ptr);

  // Replace characters not in allowedStringCharacters with spaces
  for (size_t i = 0; i < strlen(userInput); ++i) {
    if (isCharacterAllowed(userInput[i]) == -1) {
      userInput[i] = ' ';
    }
  }
}

char getNextCharacter(char currentChar) {
  int index = isCharacterAllowed(currentChar);
  if (index == -1) {
    // Character not found in the list
    return allowedStringCharacters[initialCharacterPosition];
  }
  // Increment index with wrap-around
  index = (index + 1) % strlen(allowedStringCharacters);
  return allowedStringCharacters[index];
}

char getPreviousCharacter(char currentChar) {
  int index = isCharacterAllowed(currentChar);
  if (index == -1) {
    // Character not found in the list
    return allowedStringCharacters[initialCharacterPosition];
  }
  // Decrement index with wrap-around
  index = (index - 1 + strlen(allowedStringCharacters)) % strlen(allowedStringCharacters);
  return allowedStringCharacters[index];
}

int actualLength(const char* str) {
  int length = strlen(str);
  for (int i = length - 1; i >= 0; i--) {
    if (str[i] != ' ') return i + 1;
  }
  return 0;
}

