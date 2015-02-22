/* Arduino gym timer
 * 
 * Specifically targetted BJJ round timer.
 *
 * See LICENSE.txt for license information.
 *
 * Copyright Gareth Kirwan <gbjk@thermeon.com>.
*/

#define DEBUG 1

#include <IRsensor.h>
#include <SevSeg.h>
#include <SPI.h>

#define USE_BUZZER 1

#define IR_PIN     14
#define BUZZ_PIN   19

// MODE constants
#define OFF             1
#define WAIT            2
#define EDIT            3
#define TIMER           4
#define PAUSE           5

#define EXPECT_PROGRAM  7

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

int mode  = WAIT;

#define ALARM_PHASE 1
#define REST_PHASE  4
#define PHASES      5

struct timer_phase {
  int             beeps;          // How many beeps
  bool            show_time;      // Whether to show the time, or the display char
  char            display[5];
  int             mins;
  int             secs;
  unsigned long   end_time;
  unsigned long   end_beep;
};

struct timer_phase timer_phases[PHASES] = {
  { 1, 0, " go ", 0, 2 },
  { 0, 1, "",     0, 8 },     // Default to 0.08 timer
  { 3, 0, "d0nE", 0, 6 },
  { 0, 0, "rE5t", 0, 3 },
  { 0, 1, "",     0, 5 },     // Default to 0.05 rest
  };

int current_phase_index = ALARM_PHASE;
timer_phase current_phase       = timer_phases[ current_phase_index ];

// Contexts: Beeping during TIMER, or Flashing during editting
bool toggle = 0;
byte edit_digit;

unsigned long time;
unsigned long last_tick; // Whatever the relevant ticks are, MODE context specific

const unsigned int powers [4] = {1000,100,10,1};

SevSeg sevseg;

void setup(){

  // Buzzer
  if (USE_BUZZER){
    pinMode(BUZZ_PIN, OUTPUT);
    }

  mode = WAIT;

  set_time();

  if (DEBUG){
    Serial.begin(9600);
    }

  sevseg.begin();

  irrecv.enableIRIn(); 
  }

// Loop functions
void loop(){

  // Will need this for handle_key events
  time = millis();

  loop_for_mode();

  sevseg.PrintOutput();

  if (irrecv.decode(&results)) {
    if (results.value != REPEAT){
      handle_key(results.value);
      }
    irrecv.resume(); // Receive the next value
    }
  }

// See DOCUMENTATION.md
void loop_for_mode(){
  if (mode == TIMER){
    timer_loop();
    }
  else if (mode == EDIT){
    edit_loop();
    }
}

void timer_loop(){
  if (time > current_phase.end_time){ 
    switch_phase();
    }
  else {
    if (time - last_tick >= 1000){
      current_phase.secs -= 1;
      if (current_phase.secs < 0){
        current_phase.mins -= 1;
        current_phase.secs = 59;
        }
      last_tick = time;

      if (current_phase.beeps){
        if (toggle){
          // We've done the beep, so decrease count
          current_phase.beeps--;
          digitalWrite(BUZZ_PIN, LOW);
          }
        else {
          // Don't decrease the beep count until we're done beeping, so we stop lhe last beep
          digitalWrite(BUZZ_PIN, HIGH);
          }
        toggle = toggle ? 0 : 1;
        }

      if (Serial && current_phase.show_time){
          Serial.print(">  ");
          Serial.print(current_phase.mins);
          Serial.print(".");
          Serial.println(current_phase.secs);
          }
      }
    }

  if (current_phase.show_time){
    set_time();
    }
  else {
    sevseg.NewNum(current_phase.display);
    }
  }

void edit_loop(){
  // Edit flash rate
  if (time - last_tick >= 500){

    last_tick = time;

    if (!toggle){
      // Flash digit we're editting
      sevseg.HideNum(edit_digit);
      }
    else {
      sevseg.ShowAll();
      }

    toggle = toggle ? 0 : 1;
    }
  }

// Timer functions
void switch_phase(){ 
  end_phase();

  current_phase_index += 1;
  if (current_phase_index >= PHASES){
    current_phase_index = 0;
    }

  start_phase();
  }

void start_phase(){
  // Copy the phase config over
  current_phase = timer_phases[ current_phase_index ];

  continue_phase();
  }

void continue_phase(){
  current_phase.end_time = time + (((current_phase.mins * 60ul) + current_phase.secs) * 1000ul);

  if (current_phase.beeps){
    // Start the beep up
    digitalWrite(BUZZ_PIN, HIGH);
    }

  last_tick = time;

  if (Serial){
    Serial.print("Starting phase: ");
    Serial.println(current_phase_index);

    if (!current_phase.show_time){
      Serial.print(">  ");
      Serial.println(current_phase.display);
      }

    Serial.print("Current time: ");
    Serial.println(time);

    Serial.print("End at: ");
    Serial.println(current_phase.end_time);
    }

  toggle = 1;
  }

void end_phase(){
  // Make sure we're not flash down, buzzering, etc
  digitalWrite(BUZZ_PIN, LOW);
  toggle = 0;
  }

void set_time(){
  char display_timer[4];

  if (current_phase_index == REST_PHASE){
    sprintf(display_timer, "r %02i", current_phase.secs);
    sevseg.NewNum(display_timer, 5);
    }
  else {
    sprintf(display_timer, "%02i%02i", current_phase.mins, current_phase.secs);
    sevseg.NewNum(display_timer, 2);
    }

  }

// Key handlers
void handle_key(unsigned long code){
  int new_number = 10;

  if (mode == OFF && code != BUTTON_PWR){
    return;
    }

  switch (code){
    case BUTTON_PWR:  handle_power();   break;
    case BUTTON_PLAY: handle_play();    break;
    case BUTTON_MODE: handle_mode();    break;
    case BUTTON_SWAP: handle_swap();    break;
    case BUTTON_MUTE: handle_mute();    break;

    // Currently unused
    case BUTTON_FF:   handle_ff();      break;
    case BUTTON_RW:   handle_rw();      break;

    // Numbers
    case BUTTON_0:     new_number = 0;  break;
    case BUTTON_1:     new_number = 1;  break;
    case BUTTON_2:     new_number = 2;  break;
    case BUTTON_3:     new_number = 3;  break;
    case BUTTON_4:     new_number = 4;  break;
    case BUTTON_5:     new_number = 5;  break;
    case BUTTON_6:     new_number = 6;  break;
    case BUTTON_7:     new_number = 7;  break;
    case BUTTON_8:     new_number = 8;  break;
    case BUTTON_9:     new_number = 9;  break;

    default:
      if (Serial){
        Serial.print("Other button: ");
        Serial.println(code, HEX);
        }
  }

  if (new_number < 10){
    handle_number(new_number);
  }
}

void handle_mute(){
  // No current handling for mute
  }

void handle_swap(){
  // Programming not supported yet
  // mode = EXPECT_PROGRAM;
  }

void handle_play(){
  leave_current_mode();

  if (mode == TIMER){
    enter_pause();
    }
  else if (mode == PAUSE){
    resume_timer();
    }
  else {
    enter_timer();
    }
  }

void handle_power(){
  leave_current_mode();

  if (mode == OFF){
    enter_wait();
    }
  else {
    enter_off();
    }
  }

void handle_mode(){
  leave_current_mode();

  if (mode == WAIT){
    enter_edit();
    }
  else {
    enter_wait();
    }
  }

void handle_ff(){
  if (mode == EDIT){
    edit_digit += 1;
    if (edit_digit > 3){
      switch_edit_phase();
      }
    }
  }

void handle_rw(){
  if (mode == EDIT){
    if ((current_phase_index == ALARM_PHASE && edit_digit == 0) ||
        (current_phase_index == REST_PHASE && edit_digit == 2)){
      switch_edit_phase();
      edit_digit = 3;
      }
    else {
      edit_digit -= 1;
      }
    }
  }

void handle_number(int new_number){
  if (mode == EDIT){
    edit_number(new_number);
    }
/*
  else if (mode == EXPECT_PROGRAM){
    char display_pr[4];
    sprintf(display_pr, "Pr %1i", new_number);

    sevseg.NewNum(display_pr, 5);
    mode = WAIT;
  }
*/
  }

// Change of state functions
void leave_current_mode(){
  switch (mode){
    case PAUSE:
    case TIMER:       leave_timer();    break;
    case EDIT:        leave_edit();     break;
    }
  }

void enter_off(){
  mode = OFF;
  sevseg.NewNum("    ");
  }

void enter_wait(){
  mode = WAIT;
  current_phase_index = ALARM_PHASE;
  current_phase = timer_phases[ current_phase_index ];
  set_time();
  toggle = 1;
  }

void leave_edit(){
  timer_phases[ current_phase_index ] = current_phase;
  sevseg.ShowAll();
  }

void enter_edit(){
  current_phase_index = ALARM_PHASE;
  current_phase = timer_phases[ current_phase_index ];

  // Need to run set_time anyway, to make sure we switch to a leading 0
  set_time();

  // Make sure we'll immediately flash down
  last_tick = time - 1000;

  edit_digit = 0;
  toggle    = 1;

  mode = EDIT;
  }

void resume_timer(){
  mode = TIMER;

  sevseg.ShowAll();

  continue_phase();
  }

void enter_timer(){
  mode = TIMER;

  sevseg.ShowAll();

  current_phase_index = 0;
  start_phase();
  }

void leave_timer(){
  end_phase();
  }

void enter_pause(){
  mode = PAUSE;
  end_phase();
  }

// Edit functions
void edit_number(int new_number){
  // Nothing greater than 6 in decimal minutes or seconds
  if (new_number > 6 && edit_digit % 2 == 0){
    new_number = 6;
    }

  if (edit_digit == 0){
    int remainder = current_phase.mins % 10;
    current_phase.mins = (10 * new_number) + remainder;
    }
  else if (edit_digit == 1){
    int decimal = current_phase.mins / 10;
    current_phase.mins = (10 * decimal ) + new_number;
    }
  else if (edit_digit == 2){
    int remainder = current_phase.secs % 10;
    current_phase.secs = (10 * new_number) + remainder;
    }
  else {
    int decimal = current_phase.secs / 10;
    current_phase.secs = (10 * decimal) + new_number;
    }

  edit_digit += 1;

  set_time();

  if (edit_digit > 3){
    // Show the user what they just did before switching modes
    sevseg.ShowAll();
    sevseg.PrintOutput();
    delay(1000);

    switch_edit_phase();
    }
  }

void switch_edit_phase(){
  // Copy whatever we've just editted over
  timer_phases[ current_phase_index ] = current_phase;

  // Switch between editting alarm and rest
  current_phase_index = current_phase_index == ALARM_PHASE ? REST_PHASE : ALARM_PHASE;
  current_phase = timer_phases[ current_phase_index ];

  if (current_phase_index == REST_PHASE){
    edit_digit = 2;
    }
  else {
    edit_digit = 0;
    }

  // Show the new time from the new phase
  set_time();
  }
