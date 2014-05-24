// Prototype gym timer

#include <IRremote.h>
#include <SevSeg.h>

SevSeg sevseg;

#define DEFAULT_MINS 6
#define DEFAULT_SECS 00

#define IR_PIN     14
#define BUZZ_PIN   19

#define OFF             1
#define WAIT            2
#define COUNTDOWN       3
#define ALARM           4
#define EDIT            5
#define EXPECT_PROGRAM  6
#define REST            6

#define BUTTON_0     0xFF6897
#define BUTTON_1     0xFF30CF
#define BUTTON_2     0xFF18E7
#define BUTTON_3     0xFF7A85
#define BUTTON_4     0xFF10EF 
#define BUTTON_5     0xFF38C7
#define BUTTON_6     0xFF5AA5
#define BUTTON_7     0xFF42BD
#define BUTTON_8     0xFF4AB5
#define BUTTON_9     0xFF52AD
#define BUTTON_FF    0xFFC23D 
#define BUTTON_RW    0xFF02FD
#define BUTTON_PLAY  0xFF22DD
#define BUTTON_MODE  0xFF629D
#define BUTTON_PWR   0xFFA25D
#define BUTTON_MUTE  0xFFE21D
#define BUTTON_SWAP  0xFF9867
#define BUTTON_PLUS  0xFF906F
#define BUTTON_MINUS 0xFFA857

#define ALARM_DURATION 6000

IRrecv irrecv(IR_PIN);
decode_results results;

int mode = 1;
bool flash_state;
byte edit_digit;

unsigned long last_flash;
unsigned long last_sec;
unsigned long alarm_time;
unsigned long rest_start;

int expect = 0;
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

  set_time();

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
    // Only end an alarm as flash state goes down
    if (!flash_state){
      if (mode == ALARM && mils - alarm_time > ALARM_DURATION){
        end_alarm();
      }
    }
  }

  if (mode == COUNTDOWN){
    do_countdown(mils);
  }
  else if (mode == EDIT){
    if (!flash_state){
      // Flash digit 2
      sevseg.HideNum(edit_digit);
      }
    }
  else if (mode == EXPECT_PROGRAM){
    if (!flash_state){
      show = false;
    }
  }

  if (mode == ALARM){
    if (flash_state){
      // TODO BUZZER OFF
      //digitalWrite(BUZZ_PIN, HIGH);
    }
    else {
      digitalWrite(BUZZ_PIN, LOW);
      show = false;
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
    sevseg.PrintOutput();
  }

  sevseg.ShowAll();
}

void set_time(){
  char display_timer[4];
  sprintf(display_timer, "%2i%02i", mins, secs);
  sevseg.NewNum(display_timer, 2);
  }

void do_countdown(unsigned long mils){
  if (mils - last_sec >= 1000){
    if (mins == 0 && secs == 0){
      mode = ALARM;
      mins = 5;
      secs = 55;
      alarm_time = mils;
      // Ensure we're flashing up
      flash_state = 1;
    }
    else {
      secs -= 1;
      if (secs < 0){
        mins -= 1;
        secs = 59;
      }
      last_sec = mils;
    }
  set_time();
  }
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
    case BUTTON_PLAY:
      if (mode == ALARM){
        end_alarm();
        }
      else {
        mode = mode == COUNTDOWN ? WAIT : COUNTDOWN;
        }
      break;
    case BUTTON_MODE:
      handle_mode();
      break;

    case BUTTON_SWAP:
      mode = EXPECT_PROGRAM;
      break;

    // Currently unused
    case BUTTON_FF:
    case BUTTON_RW:
    case BUTTON_MUTE:
    case BUTTON_PWR:
      break;

    default:
      Serial.print("Other button: ");
      Serial.println(code, HEX);
  }

  if (new_number < 10){
    handle_number(new_number);
  }
}

void end_alarm(){
  digitalWrite(BUZZ_PIN, LOW);
  // Reset the default display
  secs = DEFAULT_SECS;
  mins = DEFAULT_MINS;
  mode = REST;
  rest_start = millis();

  char display_timer[4];
  sprintf(display_timer, "rESt");
  sevseg.NewNum(display_timer, 5);
  }

void handle_mode(){
  if (mode == ALARM){
    end_alarm();
  }
  else {
    mode = mode == EDIT ? WAIT : EDIT;
    if (mode == EDIT){
      edit_digit = 1;
    }
  }
}

void handle_number(int new_number){
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
  set_time();
  }
  else if (mode == EXPECT_PROGRAM){
    char display_pr[4];
    sprintf(display_pr, "Pr %1i", new_number);

    sevseg.NewNum(display_pr, 5);
    mode = WAIT;
  }
}

