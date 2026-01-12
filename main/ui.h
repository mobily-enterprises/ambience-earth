#ifndef UI_H
#define UI_H

#include "messages.h"
#include <stdint.h>

#define DISPLAY_COLUMNS 20
#define LABEL_LENGTH 20

#define BUTTONS_PIN A7
#define BUTTONS_PIN_MODE INPUT_PULLUP  // Use internal pull-up for the analog ladder
#define BUTTONS_DEBOUNCE_MULTIPLIER 3  // Set debounce frequency multiplier
#define BUTTONS_ANALOG_MARGIN 20       // Set analog value margin
#define BUTTONS_HOLD_DURATION_MS 450   // Delay before auto-repeat starts
#define BUTTONS_HOLD_INTERVAL_MS 300   // Auto-repeat interval while held

struct Button {
  uint16_t threshold;
  bool repeat;
};

void initializeButtons();

// ****************************************
// **         USER INPUT                 **
// ****************************************
struct LabelRef {
  const char *ptr;
  MsgId id;
  uint8_t is_ram;
};

struct Choice {
  LabelRef label;
  int value;
};

void initLcd();
int8_t yesOrNo_P(MsgId question, bool initialUserInput = true);
int8_t yesOrNo_P_NoAbort(MsgId question, bool initialUserInput = true);
int8_t promptYesNoWithHeader(MsgId header, MsgId question, bool initialYes = true);
bool alert_P(MsgId question = MSG_LITTLE);
int8_t giveOk_P(MsgId top, MsgId promptText = MSG_LITTLE, MsgId line2 = MSG_LITTLE, MsgId line3 = MSG_LITTLE);
int8_t selectChoice(int howManyChoices, int initialUserInput, bool doNotClear = false);
void setChoices_P(MsgId label0=MSG_LITTLE,int value0=0,MsgId label1=MSG_LITTLE,int value1=0,MsgId label2=MSG_LITTLE,int value2=0,MsgId label3=MSG_LITTLE,int value3=0,MsgId label4=MSG_LITTLE,int value4=0,MsgId label5=MSG_LITTLE,int value5=0,MsgId label6=MSG_LITTLE,int value6=0,MsgId label7=MSG_LITTLE,int value7=0,MsgId label8=MSG_LITTLE,int value8=0,MsgId label9=MSG_LITTLE,int value9=0);
void setChoicesHeader_P(MsgId header=MSG_LITTLE);
void setChoice_R(unsigned char index, const char *label, int value=0);


void setChoice_P(unsigned char index, MsgId label, int value=0);

int16_t inputNumber_P(MsgId prompt, int16_t initialUserInput, int stepSize, int16_t min = 0, int16_t max = 100, MsgId postFix = MSG_LITTLE, MsgId optionalHeader = MSG_LITTLE);
bool inputTime_P(MsgId header, MsgId prompt, uint16_t initialMinutes, uint16_t *outMinutes);
bool inputDate_P(MsgId header, MsgId prompt, uint8_t *day, uint8_t *month, uint8_t *year);
bool inputLightsOnOff_P(MsgId header, uint16_t *onMinutes, uint16_t *offMinutes);
bool inputString_P(MsgId prompt, char *initialUserInput, MsgId optionalHeader = MSG_LITTLE, bool asEdit = false, uint8_t maxLen = LABEL_LENGTH);


void resetChoicesAndHeader();

// ****************************************
// **         USER OUTPUT                **
// ****************************************

void lcdClear();
void lcdClearLine(uint8_t y);
void lcdPrintSpaces(uint8_t count);
void lcdPrint_P(MsgId message, int8_t y = -1);
void lcdPrintTwoDigits(uint8_t value);
void lcdPrintNumber(int number, int8_t y = -1);
void lcdPrintBool(bool b, int8_t y = -1);
void lcdSetCursor(uint8_t x, uint8_t y);
void lcdFlashMessage_P(MsgId message, MsgId message2=MSG_LITTLE, uint16_t time = 1000);

void analogButtonsCheck();


#endif UI_H
