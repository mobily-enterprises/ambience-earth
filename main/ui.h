#ifndef UI_H
#define UI_H

#include "messages.h"

#define DISPLAY_COLUMNS 20
#define LABEL_LENGTH 20

#define BUTTONS_PIN A7
#define BUTTONS_PIN_MODE INPUT         // Set to INPUT to disable internal pull-up resistor
#define BUTTONS_DEBOUNCE_MULTIPLIER 2  // Set debounce frequency multiplier
#define BUTTONS_ANALOG_MARGIN 20       // Set analog value margin

void initializeButtons();

// ****************************************
// **         USER INPUT                 **
// ****************************************
struct Choice {
  char label[LABEL_LENGTH + 1];
  int value;
};

void initLcd();
int8_t yesOrNo(char* question, bool initialUserInput = true);
bool alert(char *question=MSG_LITTLE);
int8_t giveOk(char* top, const char *promptText=MSG_LITTLE, const char *line2=MSG_LITTLE, const char *line3=MSG_LITTLE);
int8_t selectChoice(int howManyChoices, int initialUserInput, bool doNotClear = false);
void setChoices(const char *label0=MSG_LITTLE,int value0=0,const char *label1=MSG_LITTLE,int value1=0,const char *label2=MSG_LITTLE,int value2=0,const char *label3=MSG_LITTLE,int value3=0,const char *label4=MSG_LITTLE,int value4=0,const char *label5=MSG_LITTLE,int value5=0,const char *label6=MSG_LITTLE,int value6=0,const char *label7=MSG_LITTLE,int value7=0,const char *label8=MSG_LITTLE,int value8=0,const char *label9=MSG_LITTLE,int value9=0);
void setChoicesHeader(const char *header=MSG_LITTLE);


void setChoiceFromString(unsigned char index, const char *label="",int value=0);
void setChoice(unsigned char index, const char *label,int value=0);

long int inputNumber(char *prompt, long int initialUserInput, int stepSize, long int min = 0, long int max = 100, char *postFix = MSG_LITTLE, char *optionalHeader = MSG_LITTLE);

void inputString(char *prompt, char *initialUserInput, char *optionalHeader, bool asEdit = false);

char* getUserInputString();

void labelcpyFromString(char* destination, const char* source);
void labelcpy(char* destination, const char *source);

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
void lcdFlashMessage(char *message, char *message2=MSG_LITTLE, uint16_t time = 1000);

void analogButtonsCheck();


#endif UI_H