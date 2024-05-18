#ifndef UI_H
#define UI_H

void initLcdAndButtons();

#define DISPLAY_COLUMNS 20

#define BUTTONS_PIN A6
#define BUTTONS_PIN_MODE INPUT         // Set to INPUT to disable internal pull-up resistor
#define BUTTONS_DEBOUNCE_MULTIPLIER 2  // Set debounce frequency multiplier
#define BUTTONS_ANALOG_MARGIN 20       // Set analog value margin

// ****************************************
// **         USER INPUT                 **
// ****************************************
struct Choice {
  char label[DISPLAY_COLUMNS + 1];
  int value;
};

bool confirm(char *question, bool initialUserInput = 1);

bool alert(char *question);

int selectChoice(int howManyChoices, int initialUserInput, char *optionalHeader = "");
void setChoices(const char *label0="",int value0=0,const char *label1="",int value1=0,const char *label2="",int value2=0,const char *label3="",int value3=0,const char *label4="",int value4=0,const char *label5="",int value5=0);
void setChoice(unsigned char index, const char *label="",int value=0);
Choice* getChoices();

int inputNumber(char *prompt, int initialUserInput, int stepSize, int min = 0, int max = 100, char *postFix = "", char *optionalHeader = "");

void inputString(char *prompt, char *initialUserInput, char *optionalHeader = "");

char* getUserInputString();

// ****************************************
// **         USER OUTPUT                **
// ****************************************

void lcdClear();
void lcdClearLine(uint8_t y);
void lcdPrint(char *message, uint8_t y = 0);
void lcdPrintNumber(int number, uint8_t y = 0);
void lcdSetCursor(uint8_t x, uint8_t y);
void lcdAbortedMessage();
void analogButtonsCheck();


#endif UI_H