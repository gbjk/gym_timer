/*
 7 segment driver through spi with continuous power

 Segments Bytes are reversed in byte order, so going from right to left: B87654321
   4" digits              6.5" digits
     --6--                   --6--
    |     |                 |     |
    7     5                 7     5
    |     |                 |     |
    |--8--|                 |--4--|
    |     |                 |     |
    1     3                 1     3
    |     |                 |     |
     --2--  .4               --2--  .8

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

// Handling for different 7 segment/board layouts could be more graceful, really
// 6.5" digit byte order
const byte digit_bytes [10] = {
  B01110111,
  B00010100,
  B00111011,
  B00111110,
  B01011100,
  B01101110,
  B01101111,
  B00110100,
  B01111111,
  B01111100
  };

/* 4" digit byte order
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
*/

SevSeg::SevSeg(){ }

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
      display &= B10000000;
      }
    SPI.transfer( display );
    }

  digitalWrite( latch_pin, HIGH );
  }

// New Number
/*******************************************************************************************/
// Function to update the number displayed
void SevSeg::NewNum(char display[4], byte decimal_place)
{

  for (int i=0;i<4;i++) {
    char digit = display[i];

    byte disp = blank;

    switch (digit){
    case 'A':
      disp = B01111101;
      break;
    case 'd':
      disp = B00011111;
      break;
    case 'E':
      disp = B01101011;
      break;
    case 'g':
      disp = B01111110;
      break;
    case 'o': // Top segment o
      disp = B01111000;
      break;
    case 'n':
      disp = B00001101;
      break;
    case 'P':
      disp = B01111001;
      break;
    case 'r':
      disp = B00001001;
      break;
    case 't':
      disp = B01001011;
      break;
    case 'V':
      disp = B01010111;
      break;
    case '-':
      disp = B00001000;
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
      disp |= B10000000;
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
