#ifndef UI_H
#define UI_H

#include "messages.h"
#include <avr/pgmspace.h>

#define DISPLAY_COLUMNS 20
#define LABEL_LENGTH 20

#define BUTTONS_PIN A7
#define BUTTONS_PIN_MODE INPUT_PULLUP  // Use internal pull-up for the analog ladder
#define BUTTONS_DEBOUNCE_MULTIPLIER 2  // Set debounce frequency multiplier
#define BUTTONS_ANALOG_MARGIN 20       // Set analog value margin
#define BUTTONS_HOLD_DURATION_MS 300   // Delay before auto-repeat starts
#define BUTTONS_HOLD_INTERVAL_MS 80    // Auto-repeat interval while held

void initializeButtons();

// ****************************************
// **         USER INPUT                 **
// ****************************************
struct LabelRef {
  PGM_P ptr;
};

struct Choice {
  LabelRef label;
  int value;
};

void initLcd();
int8_t yesOrNo_P(PGM_P question, bool initialUserInput = true);
int8_t yesOrNo_P_NoAbort(PGM_P question, bool initialUserInput = true);
bool alert_P(PGM_P question = MSG_LITTLE);
int8_t giveOk_P(PGM_P top, PGM_P promptText = MSG_LITTLE, PGM_P line2 = MSG_LITTLE, PGM_P line3 = MSG_LITTLE);
int8_t selectChoice(int howManyChoices, int initialUserInput, bool doNotClear = false);
void setChoices_P(PGM_P label0=MSG_LITTLE,int value0=0,PGM_P label1=MSG_LITTLE,int value1=0,PGM_P label2=MSG_LITTLE,int value2=0,PGM_P label3=MSG_LITTLE,int value3=0,PGM_P label4=MSG_LITTLE,int value4=0,PGM_P label5=MSG_LITTLE,int value5=0,PGM_P label6=MSG_LITTLE,int value6=0,PGM_P label7=MSG_LITTLE,int value7=0,PGM_P label8=MSG_LITTLE,int value8=0,PGM_P label9=MSG_LITTLE,int value9=0);
void setChoicesHeader_P(PGM_P header=MSG_LITTLE);


void setChoice_P(unsigned char index, PGM_P label, int value=0);

long int inputNumber_P(PGM_P prompt, long int initialUserInput, int stepSize, long int min = 0, long int max = 100, PGM_P postFix = MSG_LITTLE, PGM_P optionalHeader = MSG_LITTLE);
bool inputTime_P(PGM_P header, PGM_P prompt, uint16_t initialMinutes, uint16_t *outMinutes);
bool inputDateTime_P(PGM_P header, uint8_t *hour, uint8_t *minute, uint8_t *day, uint8_t *month, uint8_t *year);
void inputString_P(PGM_P prompt, char *initialUserInput, PGM_P optionalHeader = MSG_LITTLE, bool asEdit = false, uint8_t maxLen = LABEL_LENGTH);


void resetChoicesAndHeader();

// ****************************************
// **         USER OUTPUT                **
// ****************************************

void lcdClear();
void lcdClearLine(uint8_t y);
void lcdPrint_P(PGM_P message, int8_t y = -1);
void lcdPrintNumber(int number, int8_t y = -1);
void lcdPrintBool(bool b, int8_t y = -1);
void lcdSetCursor(uint8_t x, uint8_t y);
void lcdFlashMessage_P(PGM_P message, PGM_P message2=MSG_LITTLE, uint16_t time = 1000);

void analogButtonsCheck();


#endif UI_H
