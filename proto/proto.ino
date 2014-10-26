// Prototype gym timer

#include <IRremote.h>
#include <SevSeg.h>
#include <SPI.h>

#define ALARM_DURATION 4000
#define WARN_DURATION  2000

#define USE_BUZZER 0

#define DEFAULT_MINS 0
#define DEFAULT_SECS 3

#define DEFAULT_REST_MINS 0
#define DEFAULT_REST_SECS 5

#define IR_PIN     14
#define BUZZ_PIN   19

#define OFF             1
#define WAIT            2
#define ALARM_COUNTDOWN 3
#define ALARM           4
#define EDIT            5
#define EXPECT_PROGRAM  6
#define REST            7
#define REST_COUNTDOWN  8
#define WARN            9

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

IRrecv irrecv(IR_PIN);
decode_results results;

int mode = 1;
bool flash_state;
byte edit_digit;

unsigned long last_tick;
unsigned long last_sec;
unsigned long alarm_time;
unsigned long warn_start;

int expect = 0;

int mins = DEFAULT_MINS;
int secs = DEFAULT_SECS;
int last_mins;
int last_secs;

int rest_mins = DEFAULT_REST_MINS;
int rest_secs = DEFAULT_REST_SECS;

const unsigned int powers [4] = {1000,100,10,1};

SevSeg sevseg;

void setup()
{

  // Buzzer
  pinMode(BUZZ_PIN, OUTPUT);

  mode = WAIT;

  set_time();

  Serial.begin(9600);

  sevseg.begin();

  irrecv.enableIRIn(); 
}

void loop() {
  bool show = mode != OFF;

  unsigned long mils = tick_clock();

  if (mode == WARN){
    if (USE_BUZZER){
      digitalWrite(BUZZ_PIN, HIGH);
    }
  }

  if (mode == ALARM_COUNTDOWN || mode == REST_COUNTDOWN){
    do_countdown(mils);
  }
  else if (mode == EDIT){
    if (!flash_state){
      // Flash digit we're editting
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
      if (USE_BUZZER){
        digitalWrite(BUZZ_PIN, HIGH);
      }
    }
    else {
      digitalWrite(BUZZ_PIN, LOW);
      show = false;
    }
  }

  if (!show){
    sevseg.NewNum("   ");
  }

  sevseg.PrintOutput();

  if (irrecv.decode(&results)) {
    if (results.value != REPEAT){
      handle_key(results.value);
    }
    irrecv.resume(); // Receive the next value
  }

  sevseg.ShowAll();
}

unsigned long tick_clock(){
  unsigned long mils = millis();

  int tick_rate = mode == EDIT ? 400 : 1000;

  if (mils - last_tick > tick_rate){

    if ((mode == REST || mode == WARN) && (mils - warn_start > WARN_DURATION)){
      if (mode == REST){
        mode = REST_COUNTDOWN;
      }
      else if (mode == WARN){
        mode = ALARM_COUNTDOWN;
        digitalWrite(BUZZ_PIN, LOW);
      }
    }

    flash_state = flash_state ? 0 : 1;
    last_tick = mils;

    if (!flash_state){

      // Only end an alarm as flash state goes down
      if (mode == ALARM && mils - alarm_time > ALARM_DURATION){
        end_alarm();
      }
    }
  }
  return mils;
}

void set_time(){
  char display_timer[4];
  // Switch back to allowing decimal minutes
  sprintf(display_timer, "%02i%02i", mins, secs);
  sevseg.NewNum(display_timer , 2);
}

void do_countdown(unsigned long mils){
  if (mils - last_sec >= 1000){
    if (mins == 0 && secs == 0){
      if (mode == ALARM_COUNTDOWN){
        mode = ALARM;

        alarm_time = mils;

        sevseg.NewNum("0555");

        // Ensure we're flashing up
        flash_state = 1;
      }
      else if (mode == REST_COUNTDOWN){
        mins = last_mins;
        secs = last_secs;
        start_alarm_countdown();
      }

      last_tick = last_sec = millis();

      return;
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
        start_alarm_countdown();
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

void start_alarm_countdown(){
  mode = mode == ALARM_COUNTDOWN ? WAIT : WARN;

  last_secs = secs;
  last_mins = mins;

  last_tick = last_sec = millis();

  sevseg.NewNum(" 9o ");

  // Make sure we're starting up, for the buzzer
  flash_state = 1;
  }

void end_alarm(){
  digitalWrite(BUZZ_PIN, LOW);
  // Reset the default display
  mode = REST;
  warn_start = millis();

  // TODO, real values
  mins = DEFAULT_REST_MINS;
  secs = DEFAULT_REST_SECS;

  last_tick = last_sec = millis();

  sevseg.NewNum("rESt");
  }

void handle_mode(){
  if (mode == ALARM){
    end_alarm();
  }
  else {
    int old_mode = mode;
    mode = mode == EDIT ? WAIT : EDIT;
    if (mode == EDIT){
      if (old_mode == ALARM_COUNTDOWN || old_mode == ALARM || old_mode == REST || old_mode == REST_COUNTDOWN){
        secs = last_secs;
        mins = last_mins;
        set_time();
      }

      edit_digit = 0;
    }
  }
}

void handle_number(int new_number){
  if (mode == EDIT){
    if (edit_digit == 0){
      if (new_number > 6){
        new_number = 6;
        }
      int decimal = mins % 10;
      mins = (10 * new_number) + decimal;
    }
    else if (edit_digit == 1){
      int decimal = mins / 10;
      mins = (10 * decimal ) + new_number;
    }
    else if (edit_digit == 2){
      if (new_number > 6 && edit_digit % 2 == 0){
        new_number = 6;
        }
      int decimal = secs % 10;
      secs = (10 * new_number) + decimal;
    }
    else {
      int decimal = secs / 10;
      secs = (10 * decimal) + new_number;
    }
    edit_digit += 1;
    if (edit_digit > 3){
      edit_digit = 0;
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

/*
Controls:
 Mode: Start or Stop editting the time
 Swap: User Programs
 Play/Pause: Start countdown or end alarm
*/
