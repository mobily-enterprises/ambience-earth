#ifndef UI_H
#define UI_H

#include "messages.h"
#include <avr/pgmspace.h>

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
struct LabelRef {
  const char *ptr;
  uint8_t is_progmem;
};

struct Choice {
  LabelRef label;
  int value;
};

void initLcd();
int8_t yesOrNo_P(PGM_P question, bool initialUserInput = true);
int8_t yesOrNo_R(const char* question, bool initialUserInput = true);
bool alert_P(PGM_P question = MSG_LITTLE);
bool alert_R(const char* question = "");
int8_t giveOk_P(PGM_P top, PGM_P promptText = MSG_LITTLE, PGM_P line2 = MSG_LITTLE, PGM_P line3 = MSG_LITTLE);
int8_t giveOk_R(const char* top, const char* promptText = "", const char* line2 = "", const char* line3 = "");
int8_t selectChoice(int howManyChoices, int initialUserInput, bool doNotClear = false);
void setChoices_P(PGM_P label0=MSG_LITTLE,int value0=0,PGM_P label1=MSG_LITTLE,int value1=0,PGM_P label2=MSG_LITTLE,int value2=0,PGM_P label3=MSG_LITTLE,int value3=0,PGM_P label4=MSG_LITTLE,int value4=0,PGM_P label5=MSG_LITTLE,int value5=0,PGM_P label6=MSG_LITTLE,int value6=0,PGM_P label7=MSG_LITTLE,int value7=0,PGM_P label8=MSG_LITTLE,int value8=0,PGM_P label9=MSG_LITTLE,int value9=0);
void setChoices_R(const char *label0="",int value0=0,const char *label1="",int value1=0,const char *label2="",int value2=0,const char *label3="",int value3=0,const char *label4="",int value4=0,const char *label5="",int value5=0,const char *label6="",int value6=0,const char *label7="",int value7=0,const char *label8="",int value8=0,const char *label9="",int value9=0);
void setChoicesHeader_P(PGM_P header=MSG_LITTLE);
void setChoicesHeader_R(const char *header="");


void setChoice_P(unsigned char index, PGM_P label, int value=0);
void setChoice_R(unsigned char index, const char *label, int value=0);

long int inputNumber_P(PGM_P prompt, long int initialUserInput, int stepSize, long int min = 0, long int max = 100, PGM_P postFix = MSG_LITTLE, PGM_P optionalHeader = MSG_LITTLE);
long int inputNumber_R(const char *prompt, long int initialUserInput, int stepSize, long int min = 0, long int max = 100, const char *postFix = "", const char *optionalHeader = "");

void inputString_P(PGM_P prompt, char *initialUserInput, PGM_P optionalHeader = MSG_LITTLE, bool asEdit = false);
void inputString_R(const char *prompt, char *initialUserInput, const char *optionalHeader = "", bool asEdit = false);

char* getUserInputString();

void resetChoicesAndHeader();

// ****************************************
// **         USER OUTPUT                **
// ****************************************

void lcdClear();
void lcdClearLine(uint8_t y);
void lcdPrint_P(PGM_P message, int8_t y = -1);
void lcdPrint_R(const char *message, int8_t y = -1);
void lcdPrintNumber(int number, int8_t y = -1);
void lcdPrintBool(bool b, int8_t y = -1);
void lcdSetCursor(uint8_t x, uint8_t y);
void lcdFlashMessage_P(PGM_P message, PGM_P message2=MSG_LITTLE, uint16_t time = 1000);
void lcdFlashMessage_R(const char *message, const char *message2="", uint16_t time = 1000);

void analogButtonsCheck();


#endif UI_H
