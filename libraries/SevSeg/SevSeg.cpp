/*
 7 segment driver through spi with continuous power

 Segments Bytes are reversed in byte order, so going from right to left: B87654321
     --6--
    |     |
    7     5
    |     |
    |--8--|
    |     |
    1     3
    |     |
     --2--  .4

 */

#include "SevSeg.h"
#include <SPI.h>

/*
  SRCK    - Clock - SPI SCLK
  RCK     - Latch - SPI SS
  SER IN  - Data  - SPI MOSI
*/

#define data_pin  11
#define clock_pin 13
#define latch_pin 10

/*
// Some suggestions sainsmart might use these pins, but they don't
#define data_pin  5
#define clock_pin 4
#define latch_pin 6
*/

const byte blank = B00000000;
const byte digit_bytes [10] = {
  B01110111,
  B00010100,
  B10110011,
  B10110110,
  B11010100,
  B11100110,
  B11100111,
  B00110100,
  B11110111,
  B11110100
  };

SevSeg::SevSeg()
{
}

//Begin
//Set pin modes and turns all displays off
void SevSeg::begin(){

  pinMode(latch_pin, OUTPUT);
  pinMode(clock_pin, OUTPUT);
  pinMode(data_pin, OUTPUT);

  SPI.begin();

  // Set hideNum high
  hideNum = 9;

  digitalWrite( latch_pin, LOW );

  // Turn eeverything off
  for (int i=0;i<4;i++){
    SPI.transfer( blank );
    }

  digitalWrite( latch_pin, HIGH );
}

void SevSeg::PrintOutput(){
  // Shift registers connected first digit last

  digitalWrite( latch_pin, LOW );
  
  for (int i=0;i<4;i++) {
    byte display = digits[i];
    if (hideAll){
      display = B00000000;
      }
    else if (hideNum == i){
      display &= B00001000;
      }
    SPI.transfer( display );
    }

  digitalWrite( latch_pin, HIGH );
  }

char last_display[4];
// New Number
/*******************************************************************************************/
// Function to update the number displayed
void SevSeg::NewNum(char display[4], byte decimal_place)
{

  if (strcmp(display, last_display)) {
    Serial.print("Display: |");
    Serial.println( display );

    strcpy(last_display, display);
    }

  for (int i=0;i<4;i++) {
    char digit = display[i];

    byte disp = blank;

    switch (digit){
    case 'A':
      disp = B11110101;
      break;
    case 'd':
      disp = B10010111;
      break;
    case 'E':
      disp = B11100011;
      break;
    case 'g':
      disp = B11110110;
      break;
    case 'o': // Top segment o
      disp = B11110000;
      break;
    case 'n':
      disp = B10000101;
      break;
    case 'P':
      disp = B11110001;
      break;
    case 'r':
      disp = B10000001;
      break;
    case 't':
      disp = B11000011;
      break;
    case 'V':
      disp = B01010111;
      break;
    case '-':
      disp = B10000000;
      break;
    case ' ':
      break;
    default:
      int num = digit - '0';
      disp = digit_bytes[ num ];
      break;
    }

    // Set the decimal place lights
    if (decimal_place == i + 1){
      disp |= B00001000;
    }

    digits[i] = disp;
  }
}
void SevSeg::NewNum(const char* display){
  char* pass_display = const_cast<char*>(display);
  NewNum(pass_display, 5);
  }

void SevSeg::HideNum(byte digit){
  hideNum = digit;
  }

void SevSeg::HideAll(){
  hideAll = true;
  }

void SevSeg::ShowAll(){
  hideNum = 9;
  hideAll = false;
  }
