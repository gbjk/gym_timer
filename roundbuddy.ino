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
#include <avr/EEPROM.h>

#define USE_BUZZER 1

#define IR_PIN     14
#define BUZZ_PIN   19

#define FLASH_RATE 500

// MODE constants
#define OFF             1
#define WAIT            2
#define EDIT            3
#define TIMER           4
#define PAUSE           5
#define EXPECT_PROGRAM  6
#define EXPECT_EDIT_PR  7
#define EDIT_PROGRAM    8

#define BUTTON_0        0xFD30CF
#define BUTTON_1        0xFD08F7
#define BUTTON_2        0xFD8877
#define BUTTON_3        0xFD48B7
#define BUTTON_4        0xFD28D7
#define BUTTON_5        0xFDA857
#define BUTTON_6        0xFD6897
#define BUTTON_7        0xFD18E7
#define BUTTON_8        0xFD9867
#define BUTTON_9        0xFD58A7

#define BUTTON_LEFT     0xFD10EF
#define BUTTON_RIGHT    0xFD50AF
#define BUTTON_PLAY     0xFD807F
#define BUTTON_STOP     0xFD609F

#define BUTTON_SETUP    0xFD20DF
#define BUTTON_UP       0xFDA05F
#define BUTTON_DOWN     0xFDB04F
#define BUTTON_ENTER    0xFD906F
#define BUTTON_BACK     0xFD708F

#define BUTTON_VOL_DOWN 0xFD00FF
#define BUTTON_VOL_UP   0xFD40BF

IRrecv irrecv(IR_PIN);
decode_results results;

int mode  = WAIT;

#define ALARM_PHASE     1
#define REST_PHASE      4
#define PHASES          5
#define SAVED_TIMER_COUNT  9

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
  { 0, 1, "",     6, 0 },     // Default to 6.00 timer
  { 3, 0, "d0nE", 0, 6 },
  { 0, 0, "rE5t", 0, 2 },
  { 0, 1, "",     1, 30 },     // Default to 1.30 rest
  };

struct saved_timer {
    int     mins;
    int     secs;
    int     rest_mins;
    int     rest_secs;
};
struct saved_timer saved_timers[SAVED_TIMER_COUNT] = {
  { 5,  0,   1, 30 },
  { 6,  0,   1, 30 },
  { 7,  0,   1, 30 },
  { 8,  0,   2, 0 },
  { 9,  0,   2, 0 },
  { 10, 0,   2, 0 },
  { 20, 0,   4, 0 },
  { 0,  30,  0, 10  },
  { 1,  0,   0, 10  },
  };

struct saved_timer eeprom_timers[SAVED_TIMER_COUNT] EEMEM;

int current_saved_index;
int current_phase_index = ALARM_PHASE;
timer_phase current_phase       = timer_phases[ current_phase_index ];

// Contexts: Beeping during TIMER, or Flashing during editting
bool toggle = 0;
byte edit_digit;

unsigned long time;
unsigned long last_tick = 0; // Whatever the relevant ticks are, MODE context specific

const unsigned int powers [4] = {1000,100,10,1};

SevSeg sevseg;

void setup(){

  // Buzzer
  if (USE_BUZZER){
    pinMode(BUZZ_PIN, OUTPUT);
    }

  mode = WAIT;

  if (DEBUG){
    Serial.begin(9600);
    }

  load_saved_timers();

  set_time();

  sevseg.begin();

  irrecv.enableIRIn();
  }

// Loop functions
void loop(){

  time = millis();

  if (time < last_tick){
    /* Time should never be before last_tick. We've wrapped the unsigned long at 50 days of millis
       Best thing to do is to assume this literally just happened, but the only thing we can do really
       is to set last_tick to 0
    */
    last_tick = 0;
    }

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
  switch (mode){
    case TIMER:             timer_loop();     break;
    case EDIT:
    case EDIT_PROGRAM:
    case PAUSE:
    case EXPECT_EDIT_PR:
    case EXPECT_PROGRAM:    flash_loop();     break;
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
      }
    }

  if (current_phase.show_time){
    set_time();
    }
  else {
    sevseg.NewNum(current_phase.display);
    }
  }

void flash_loop(){
  if (time - last_tick >= FLASH_RATE){

    last_tick = time;

    if (!toggle){
      switch (mode){
        case EDIT_PROGRAM:
        case EDIT:            flash_edit();       break;
        case PAUSE:           flash_paused();     break;
        case EXPECT_EDIT_PR:
        case EXPECT_PROGRAM:  flash_program();    break;
        }
      }
    else {
      sevseg.ShowAll();
      }

    toggle = toggle ? 0 : 1;
    }
  }

void flash_edit(){
  sevseg.HideNum(edit_digit);
  }

void flash_paused(){
  sevseg.HideAll();
  }

void flash_program(){
  sevseg.HideNum(3);
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
    sprintf(display_timer, "r%i%02i", current_phase.mins, current_phase.secs);
    }
  else {
    sprintf(display_timer, "%02i%02i", current_phase.mins, current_phase.secs);
    }
  sevseg.NewNum(display_timer, 2);

  }

// Key handlers
void handle_key(unsigned long code){
  int new_number = 10;

  if (mode == OFF && code != BUTTON_VOL_DOWN){
    return;
    }

  switch (code){
    // Control buttons, in order of rows
    case BUTTON_VOL_DOWN:   handle_vol_down();      break;
    case BUTTON_PLAY:       handle_play();          break;
    case BUTTON_VOL_UP:     handle_vol_up();        break;

    case BUTTON_SETUP:      handle_setup();         break;
    case BUTTON_STOP:       handle_stop_or_mode();  break;

    case BUTTON_LEFT:       handle_left();          break;
    case BUTTON_ENTER:      handle_enter();         break;
    case BUTTON_RIGHT:      handle_right();         break;

    case BUTTON_BACK:       handle_back();          break;

    // Numbers
    case BUTTON_0:          new_number = 0;         break;
    case BUTTON_1:          new_number = 1;         break;
    case BUTTON_2:          new_number = 2;         break;
    case BUTTON_3:          new_number = 3;         break;
    case BUTTON_4:          new_number = 4;         break;
    case BUTTON_5:          new_number = 5;         break;
    case BUTTON_6:          new_number = 6;         break;
    case BUTTON_7:          new_number = 7;         break;
    case BUTTON_8:          new_number = 8;         break;
    case BUTTON_9:          new_number = 9;         break;

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

void handle_vol_up(){
  if (mode == EXPECT_EDIT_PR){
    // Reset the prom
    reset_saved_timers();
    leave_current_mode();
    }
  }

/* This button is labelled STOP/MODE, so it has very different meanings depending on current context
   If the timer is running then it stops the timer
   If the system is in WAIT then it allows you to choose a program
   If the system is in EDIT then it'll switch to editing the saved programs
*/
void handle_stop_or_mode(){

  if (mode == TIMER || mode == PAUSE){
    leave_current_mode();
    enter_wait();
    }
  else if (mode == WAIT){
    enter_expect_program();
    }
  else if (mode == EDIT){
    enter_expect_program();
    mode = EXPECT_EDIT_PR;
    }
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

void handle_vol_down(){
  leave_current_mode();

  if (mode == OFF){
    enter_wait();
    }
  else {
    enter_off();
    }
  }

void handle_back(){
  leave_current_mode();
  switch (mode){
    case EXPECT_EDIT_PR:
      enter_edit();
      break;
    case EDIT_PROGRAM:
      enter_expect_program();
      break;
    case EDIT:
    case EXPECT_PROGRAM:
      enter_wait();
      break;
    }
}

void handle_enter(){
  switch (mode){
    case EDIT:
      save_edit();
      leave_edit();
      enter_wait();
      break;
    case EDIT_PROGRAM:
      save_current_timer();
      leave_current_mode();
      enter_wait();
      break;
    }
  }

void handle_setup(){
  switch (mode){
    case EDIT:
    case EXPECT_EDIT_PR:
    case EDIT_PROGRAM:
      leave_current_mode();
      enter_wait();
      break;
    case WAIT:
      enter_edit();
      break;
    }
  }

void handle_right(){
  if (mode == EDIT || mode == EDIT_PROGRAM){
    sevseg.ShowAll();
    edit_digit += 1;
    if (edit_digit > 3){
      switch_edit_phase();
      }
    }
  }

void handle_left(){
  if (mode == EDIT || mode == EDIT_PROGRAM){
    sevseg.ShowAll();
    if ((current_phase_index == ALARM_PHASE && edit_digit == 0) ||
        (current_phase_index == REST_PHASE && edit_digit == 1)){
      switch_edit_phase();
      edit_digit = 3;
      }
    else {
      edit_digit -= 1;
      }
    }
  }

void handle_number(int new_number){
  if (mode == EDIT || mode == EDIT_PROGRAM){
    edit_timer(new_number);
    }
  else if (mode == EXPECT_PROGRAM || mode == EXPECT_EDIT_PR){
    choose_timer(new_number);
    }
  }

// Change of state functions
void leave_current_mode(){
  switch (mode){
    case PAUSE:
    case TIMER:           leave_timer();          break;
    case EDIT_PROGRAM:
    case EDIT:            leave_edit();           break;
    case EXPECT_EDIT_PR:
    case EXPECT_PROGRAM:  leave_expect_program(); break;
    }
  }

void enter_off(){
  mode = OFF;
  sevseg.NewNum("    ");
  }

void enter_wait(){
  sevseg.ShowAll();
  mode = WAIT;
  Serial.println("Enter WAIT Mode");
  current_phase_index = ALARM_PHASE;
  current_phase = timer_phases[ current_phase_index ];
  set_time();
  toggle = 1;
  }

void save_edit(){
  timer_phases[ current_phase_index ] = current_phase;
  }

void leave_edit(){
  sevseg.ShowAll();
  }

void leave_expect_program(){
  sevseg.ShowAll();
  }

void enter_edit(){
  Serial.println("Enter EDIT Mode");
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

void leave_confirm_save(){
  timer_phases[ current_phase_index ] = current_phase;
  sevseg.ShowAll();

  enter_wait();
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

void enter_expect_program(){
  Serial.println("Enter EXPECT Mode");
  mode = EXPECT_PROGRAM;
  sevseg.NewNum("Pr -");
  }

void enter_edit_program() {
  Serial.println("Enter EDIT PROGRAM Mode");
  enter_edit();
  mode = EDIT_PROGRAM;
  }

// Edit functions
void edit_timer(int new_number){
  // Nothing greater than 6 in decimal minutes or seconds
  if (new_number > 5 && edit_digit % 2 == 0){
    new_number = 5;
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

  sevseg.ShowAll();

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
    edit_digit = 1;
    }
  else {
    edit_digit = 0;
    }

  sevseg.ShowAll();

  // Show the new time from the new phase
  set_time();
  }

// Saved timer functions
void load_saved_timers(){
  Serial.println("Loading saved timers");
  saved_timer timer;
  for (int p=0;p<=SAVED_TIMER_COUNT;p++){
    eeprom_read_block((void*)&timer, &eeprom_timers[p], sizeof(timer));
    if (timer.mins == 0xFFFF){
      // Memory hasn't been saved yet, move on
      continue;
      }
    saved_timers[p] = timer;
    if (Serial){
      Serial.print("Loaded timer ");
      Serial.print(p + 1);
      Serial.print(": ");
      Serial.print(timer.mins);
      Serial.print(":");
      Serial.print(timer.secs);
      Serial.print(", ");
      Serial.print(timer.rest_mins);
      Serial.print(":");
      Serial.println(timer.rest_secs);
      }
    }
  }

void choose_timer(int timer_index){
  if (! (timer_index >= 1 && timer_index <= SAVED_TIMER_COUNT)){
    return;
    }

  char display_pr[4];
  sprintf(display_pr, "Pr %1i", timer_index);

  sevseg.ShowAll();
  sevseg.NewNum( display_pr );
  sevseg.PrintOutput();
  delay(1000);

  load_saved_timer( timer_index );

  if (mode == EXPECT_EDIT_PR){
    current_saved_index = timer_index - 1;
    enter_edit_program();
    }
  else {
    enter_wait();
    }
  }

void load_saved_timer(int timer_index){
  saved_timer new_timer = saved_timers[ timer_index - 1 ];

  timer_phases[ ALARM_PHASE ].mins = new_timer.mins;
  timer_phases[ ALARM_PHASE ].secs = new_timer.secs;
  timer_phases[ REST_PHASE ].mins  = new_timer.rest_mins;
  timer_phases[ REST_PHASE ].secs  = new_timer.rest_secs;
  }

void save_current_timer(){
  if (! (current_saved_index >= 0 && current_saved_index <= SAVED_TIMER_COUNT)){
    return;
    }

  timer_phases[ current_phase_index ] = current_phase;

  saved_timer timer = {
    timer_phases[ ALARM_PHASE ].mins,
    timer_phases[ ALARM_PHASE ].secs,
    timer_phases[ REST_PHASE ].mins,
    timer_phases[ REST_PHASE ].secs,
    };

  saved_timers[ current_saved_index ] = timer;

  if (Serial){
    Serial.print("Saving timer ");
    Serial.print(current_saved_index);
    Serial.print(": ");
    Serial.print(timer.mins);
    Serial.print(":");
    Serial.print(timer.secs);
    Serial.print(", ");
    Serial.print(timer.rest_mins);
    Serial.print(":");
    Serial.print(timer.rest_secs);
    }

  eeprom_update_block((const void*)&timer, &eeprom_timers[current_saved_index], sizeof(timer));
  }

void reset_saved_timers(){
  Serial.println("Resetting saved timers");
  saved_timer timer;
  timer.mins = 0xFFFF;
  for (int p=0;p<=SAVED_TIMER_COUNT;p++){
    eeprom_update_block((const void*)&timer, &eeprom_timers[p], sizeof(timer));
    }
  }
