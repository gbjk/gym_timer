#ifndef SevSeg_h
#define SevSeg_h
#include "Arduino.h"

class SevSeg
{

public:
  SevSeg();

  //Public Functions
  void begin();
  void NewNum(char display[4], byte decimal_place);
  void NewNum(const char* display);

  void PrintOutput();

  void HideNum(byte digit);
  void ShowAll();

  //Public Variables


private:
  //Private Functions

  //Private Variables
  boolean mode, DigitOn, DigitOff, SegOn, SegOff;
  byte digits[4];
  byte hideNum;
};

#endif
