// Prototype gym timer

#include <IRremote.h>
#include <SevSeg.h>

SevSeg sevseg;

#define DEFAULT_MINS 6
#define DEFAULT_SECS 00

#define IR_PIN     14
#define BUZZ_PIN   19

#define OFF         1
#define WAIT        2
#define COUNTDOWN   3
#define ALARM       4
#define EDIT        5

#define BUTTON_0    0xFF6897
#define BUTTON_1    0xFF30CF
#define BUTTON_2    0xFF18E7
#define BUTTON_3    0xFF7A85
#define BUTTON_4    0xFF10EF 
#define BUTTON_5    0xFF38C7
#define BUTTON_6    0xFF5AA5
#define BUTTON_7    0xFF42BD
#define BUTTON_8    0xFF4AB5
#define BUTTON_9    0xFF52AD
#define BUTTON_FF   0xFFC23D 
#define BUTTON_RW   0xFF02FD
#define BUTTON_PLAY 0xFF22DD
#define BUTTON_MODE 0xFF629D
#define BUTTON_PWR  0xFFA25D

IRrecv irrecv(IR_PIN);
decode_results results;

int mode = 1;
bool flash_state;
byte edit_digit;

unsigned long last_flash;
unsigned long last_sec;

int secs = DEFAULT_SECS;
int mins = DEFAULT_MINS;
const unsigned int powers [4] = {1000,100,10,1};

void setup()
{

  // Buzzer
  pinMode(BUZZ_PIN, OUTPUT);

  // 7-segment display
  sevseg.Begin(
    0,                // Cathode mode
    10,11,12,13,      // Digit pins D1-D4
    2,3,4,5,6,7,8,9   // Segment pins 1-7 and Decimal Point
    );

  mode = WAIT;

  Serial.begin(9600);
    
  irrecv.enableIRIn(); 
}

void loop() {
  bool show = mode != OFF;

  unsigned long mils = millis();

  int flash_rate = mode == EDIT ? 400 : 1000;
  if (mils - last_flash > flash_rate){
    flash_state = flash_state ? 0 : 1;
    last_flash = mils;
    }

  if (mode == COUNTDOWN){
    if (mils - last_sec >= 1000){
      if (mins == 0 && secs == 0){
        mode = ALARM;
        mins = 5;
        secs = 55;
      }
      else {
        secs -= 1;
        if (secs < 0){
          mins -= 1;
          secs = 59;
        }
        last_sec = mils;
      }
    }
  }
  else if (mode == EDIT){
    if (flash_state){
      // Flash digit 2
      sevseg.HideNum(edit_digit);
      }
    }

  if (mode == ALARM){
    if (flash_state){
      digitalWrite(BUZZ_PIN, HIGH);
      show = false;
      }
    else {
      digitalWrite(BUZZ_PIN, LOW);
      }
    }

  if (show){
    sevseg.PrintOutput();
  }

  if (irrecv.decode(&results)) {
    if (results.value != REPEAT){
      handle_key(results.value);
    }
    irrecv.resume(); // Receive the next value
  }

  if (show){
    int display_timer = (mins * 100) + secs;
    // Shift decimal left to display 055
    byte decimal_point = mode == ALARM ? 3 : 2;

    sevseg.NewNum(display_timer, decimal_point);
    sevseg.PrintOutput();
  }

  sevseg.ShowAll();
}

void handle_key(unsigned long code){
  int new_number = 10;
  switch (code){
    case BUTTON_0:     new_number = 0; break;
    case BUTTON_1:     new_number = 1; break;
    case BUTTON_2:     new_number = 2; break;
    case BUTTON_3:     new_number = 3; break;
    case BUTTON_4:     new_number = 4; break;
    case BUTTON_5:     new_number = 5; break;
    case BUTTON_6:     new_number = 6; break;
    case BUTTON_7:     new_number = 7; break;
    case BUTTON_8:     new_number = 8; break;
    case BUTTON_9:     new_number = 9; break;
    case BUTTON_FF:    Serial.println("Got FF"); break;
    case BUTTON_RW:    Serial.println("Got RW"); break;
    case BUTTON_PLAY:
      mode = COUNTDOWN;
      break;
    case BUTTON_MODE:  
      if (mode == ALARM){
        // Reset the default display
        digitalWrite(BUZZ_PIN, LOW);
        secs = DEFAULT_SECS;
        mins = DEFAULT_MINS;
        }
      mode = mode == EDIT ? WAIT : EDIT;
      if (mode == EDIT){
        edit_digit = 1;
        }
      break;
    case BUTTON_PWR:   Serial.println("Got PWR"); break;

    default:    Serial.println("Other button");
  }

  if (new_number < 10){

    if (mode == EDIT){
      if (edit_digit < 2){
        mins = new_number;
      }
      else if (edit_digit == 2){
        int decimal = secs % 10;
        secs = (10 * new_number) + decimal;
      }
      else {
        int decimal = secs / 10;
        secs = (10 * decimal) + new_number;
      }
      edit_digit += 1;
      if (edit_digit > 3){
        edit_digit = 1;
      }
    }
  }
}
