#ifndef UI_H
#define UI_H

#include "messages.h"

void initLcdAndButtons();

#define DISPLAY_COLUMNS 20
#define LABEL_LENGTH 20

#define BUTTONS_PIN A6
#define BUTTONS_PIN_MODE INPUT         // Set to INPUT to disable internal pull-up resistor
#define BUTTONS_DEBOUNCE_MULTIPLIER 2  // Set debounce frequency multiplier
#define BUTTONS_ANALOG_MARGIN 20       // Set analog value margin

// ****************************************
// **         USER INPUT                 **
// ****************************************
struct Choice {
  char label[LABEL_LENGTH + 1];
  int value;
};

bool confirm2(char* question, bool initialUserInput = true);
bool alert2(char *question=MSG_EMPTY2);

int8_t selectChoice2(int howManyChoices, int initialUserInput);
void setChoices2(const char *label0=MSG_EMPTY2,int value0=0,const char *label1=MSG_EMPTY2,int value1=0,const char *label2=MSG_EMPTY2,int value2=0,const char *label3=MSG_EMPTY2,int value3=0,const char *label4=MSG_EMPTY2,int value4=0,const char *label5=MSG_EMPTY2,int value5=0);
void setChoicesHeader(const char *header="");


void setChoiceFromString(unsigned char index, const char *label="",int value=0);
void setChoice(unsigned char index, const char *label,int value=0);

int inputNumber(char *prompt, int initialUserInput, int stepSize, int min = 0, int max = 100, char *postFix = "", char *optionalHeader = "");

void inputString(char *prompt, char *initialUserInput, char *optionalHeader, bool asEdit = false);

char* getUserInputString();

void labelcpy(char* destination, const char* source);
void labelcpy2(char* destination, const char *source);

void resetChoicesAndHeader();

// ****************************************
// **         USER OUTPUT                **
// ****************************************

void lcdClear();
void lcdClearLine(uint8_t y);
void lcdPrint(const char *message, int8_t y = -1);
void lcdPrintNumber(int number, uint8_t y = 0);
void lcdPrintBool(bool b, uint8_t y = 0);
void lcdSetCursor(uint8_t x, uint8_t y);
void lcdFlashMessage2(char *message, char *message2=MSG_EMPTY2, uint16_t time = 1000);

void analogButtonsCheck();


#endif UI_H