#include <Arduino.h>
#include <TM1637Display.h>
#include "IPA_BuzzerToneNotes.h"
#include <DS3231.h>
#include <Wire.h>
#include <EEPROM.h>

bool DEBUG_MODE = true;

// Module connection pins (Digital Pins)
#define DISPLAY_CLK 2
#define DISPLAY_DIO 3

byte time_h = 0;
byte time_m = 0;

uint8_t data[] = { 0x00, 0x00, 0x00, 0x00 };

// Display vars
#define TIMER_DELAY   500
#define TIMER_DOTS_TICK 1000
#define TIMER_BLINK_TICK 500

#define MASK_DOTS 0b01000000
#define MASK_NO_DOTS 0b00000000

TM1637Display display(DISPLAY_CLK, DISPLAY_DIO);

uint8_t mask; // to display dots

unsigned long digit_timer; // digit timer
unsigned long dot_timer; // digit timer
unsigned long blink_timer; // blink timer
unsigned long alarm_check_timer; // alarm check timer 

uint8_t current_brightness = 5;
bool blink_show_state = true;

// clock vars
DS3231 rtc;

bool h12Flag;
bool pmFlag;

bool normal_clock_mode = true;

bool edit_time_mode = false;
byte edit_time_mode_num = 0;

bool edit_time_hours_mode = false;
bool edit_time_minutes_mode = false;

bool edit_alarm_mode = false;
byte edit_alarm_mode_num = 0;

bool alarm_state = false; // on/off state for playing alarm

byte alarm_hours = 0;
byte alarm_minutes = 0;
bool alarm_repeat = false;
byte alarm_time_repeat = 0;

#define ALARM_MAX_TIME_REPEAT 3


// Button vars
#define BTN_BRIGHTNESS_PIN 8
#define BTN_ALARM_PIN 7
#define BTN_TIME_PIN 6
#define BTN_ACTION_PIN 5

bool btn_brightness_last_state = LOW;
bool btn_brightness_current_state = LOW;

bool btn_alarm_last_state = LOW;
bool btn_alarm_current_state = LOW;

bool btn_time_last_state = LOW;
bool btn_time_current_state = LOW;

bool btn_action_last_state = LOW;
bool btn_action_current_state = LOW;

// Buzzer vars
#define BUZZER_PIN 4

IPA_BuzzerToneNotes IPA_BuzzerToneNotes(BUZZER_PIN);

int melody_tempo = 140;

// Copied from https://github.com/robsoncouto/arduino-songs/
int melody[] = {

  //Based on the arrangement at https://www.flutetunes.com/tunes.php?id=192
  
  NOTE_E5, 4,  NOTE_B4,8,  NOTE_C5,8,  NOTE_D5,4,  NOTE_C5,8,  NOTE_B4,8,
  NOTE_A4, 4,  NOTE_A4,8,  NOTE_C5,8,  NOTE_E5,4,  NOTE_D5,8,  NOTE_C5,8,
  NOTE_B4, -4,  NOTE_C5,8,  NOTE_D5,4,  NOTE_E5,4,
  NOTE_C5, 4,  NOTE_A4,4,  NOTE_A4,8,  NOTE_A4,4,  NOTE_B4,8,  NOTE_C5,8,

  NOTE_D5, -4,  NOTE_F5,8,  NOTE_A5,4,  NOTE_G5,8,  NOTE_F5,8,
  NOTE_E5, -4,  NOTE_C5,8,  NOTE_E5,4,  NOTE_D5,8,  NOTE_C5,8,
  NOTE_B4, 4,  NOTE_B4,8,  NOTE_C5,8,  NOTE_D5,4,  NOTE_E5,4,
  NOTE_C5, 4,  NOTE_A4,4,  NOTE_A4,4, REST, 4,

  NOTE_E5, 4,  NOTE_B4,8,  NOTE_C5,8,  NOTE_D5,4,  NOTE_C5,8,  NOTE_B4,8,
  NOTE_A4, 4,  NOTE_A4,8,  NOTE_C5,8,  NOTE_E5,4,  NOTE_D5,8,  NOTE_C5,8,
  NOTE_B4, -4,  NOTE_C5,8,  NOTE_D5,4,  NOTE_E5,4,
  NOTE_C5, 4,  NOTE_A4,4,  NOTE_A4,8,  NOTE_A4,4,  NOTE_B4,8,  NOTE_C5,8,

  NOTE_D5, -4,  NOTE_F5,8,  NOTE_A5,4,  NOTE_G5,8,  NOTE_F5,8,
  NOTE_E5, -4,  NOTE_C5,8,  NOTE_E5,4,  NOTE_D5,8,  NOTE_C5,8,
  NOTE_B4, 4,  NOTE_B4,8,  NOTE_C5,8,  NOTE_D5,4,  NOTE_E5,4,
  NOTE_C5, 4,  NOTE_A4,4,  NOTE_A4,4, REST, 4,
  

  NOTE_E5,2,  NOTE_C5,2,
  NOTE_D5,2,   NOTE_B4,2,
  NOTE_C5,2,   NOTE_A4,2,
  NOTE_GS4,2,  NOTE_B4,4,  REST,8, 
  NOTE_E5,2,   NOTE_C5,2,
  NOTE_D5,2,   NOTE_B4,2,
  NOTE_C5,4,   NOTE_E5,4,  NOTE_A5,2,
  NOTE_GS5,2,

};

int current_melody_note = 0;
unsigned long melody_timer; // melody_timer timer

// sizeof gives the number of bytes, each int value is composed of two bytes (16 bits)
// there are two values per note (pitch and duration), so for each note there are four bytes
int notes = sizeof(melody) / sizeof(melody[0]);

// this calculates the duration of a whole note in ms
int wholenote = (60000 * 4) / melody_tempo;

int divider = 0, noteDuration = 0;

// update device settings
void updateEEPROM() {
  EEPROM.write(1, current_brightness);
  EEPROM.write(2, alarm_hours);
  EEPROM.write(3, alarm_minutes);
  EEPROM.write(4, alarm_repeat);
}

// read device settings
void readEEPROM() {
  current_brightness = EEPROM.read(1);
  alarm_hours = EEPROM.read(2);
  alarm_minutes = EEPROM.read(3);
  alarm_repeat = EEPROM.read(4);

  if (current_brightness > 7) {
    current_brightness = 3;
    updateEEPROM();
  }

  if (alarm_hours >= 24) {
    alarm_hours = 0;
    updateEEPROM();
  }

  if (alarm_minutes >= 60) {
    alarm_minutes = 0;
    updateEEPROM();
  }
}

// for debounce buttons
bool debounce(uint8_t btn_pin, bool last) {
  bool current = digitalRead(btn_pin);

  if (last != current)
  {
    delay(5);
    current = digitalRead(btn_pin);
  }

  return current;
}

void setup() {
  // load saved settings
  readEEPROM();
  
  if (DEBUG_MODE) {
    Serial.begin(9600);
  
    Serial.print("Brightness: ");
    Serial.println(current_brightness);

    Serial.print("Alarm Hours: ");
    Serial.println(alarm_hours);

    Serial.print("Alarm Minutes: ");
    Serial.println(alarm_minutes);

    Serial.print("Alarm Repeat: ");
    Serial.println(alarm_repeat);

    // rtc.setHour(12);
    // rtc.setMinute(34);
  }

  // setup display
  display.setBrightness(current_brightness);

  mask = MASK_NO_DOTS;

  // setup clock
  Wire.begin();

  // setup buzzer
  //pinMode(BUZZER_PIN, OUTPUT);

  // setup buttons
  pinMode(BTN_BRIGHTNESS_PIN, INPUT_PULLUP);
  pinMode(BTN_ALARM_PIN, INPUT_PULLUP);
  pinMode(BTN_TIME_PIN, INPUT_PULLUP);
  pinMode(BTN_ACTION_PIN, INPUT_PULLUP);
}

void loop() {
  // check if change BRIGHTNESS BTN was pressed
  btn_brightness_current_state = debounce(BTN_BRIGHTNESS_PIN, btn_brightness_last_state);

  // if last state was low and curret low change mode. Current state low as pressed because used INPUT_PULLUP pin mode
  if (btn_brightness_last_state == HIGH && btn_brightness_current_state == LOW)
  {
    if (++current_brightness > 7) {
      current_brightness = 0;
    }

    display.setBrightness(current_brightness);
    
    if (DEBUG_MODE) {
      Serial.print("Brightness: ");
      Serial.println(current_brightness);
    }

    updateEEPROM();
  }

  btn_brightness_last_state = btn_brightness_current_state;
  
  
  // check if setup TIME BTN was pressed
  btn_time_current_state = debounce(BTN_TIME_PIN, btn_time_last_state);

  if (btn_time_last_state == HIGH && btn_time_current_state == LOW)
  {
    if (edit_alarm_mode)
      return;

    edit_time_mode_num++;

    //edit_alarm_mode
    switch (edit_time_mode_num) {
      case 1: // time mode hours
      case 2: // time mode minutes
      case 3: // time mode seconds
          edit_time_mode = true;
          normal_clock_mode = false;
        break;
    }

    if (edit_time_mode_num > 3)  {
      edit_time_mode = false; // exit time edit mode
      normal_clock_mode = true; // return normal mode for clock
      edit_time_mode_num = 0; // reset mode num
    }

    display.clear();
  }

  btn_time_last_state = btn_time_current_state;

  
  // check if setup ACTION BTN was pressed
  btn_action_current_state = debounce(BTN_ACTION_PIN, btn_action_last_state);

  if (btn_action_last_state == HIGH && btn_action_current_state == LOW)
  {
    if (edit_time_mode && edit_time_mode_num == 1) {
      // increase hour
      time_h = rtc.getHour(h12Flag, pmFlag) + 1;

      if (time_h >= 24)
        time_h = 0;

      rtc.setHour(time_h);

      display.showNumberDecEx(rtc.getHour(h12Flag, pmFlag) * 100 + rtc.getMinute(), MASK_NO_DOTS, true);
    }

    if (edit_time_mode && edit_time_mode_num == 2) {
      // increase minutes
      time_m = rtc.getMinute() + 1;

      if (time_m >= 60)
        time_m = 0;

      rtc.setMinute(time_m);

      display.showNumberDecEx(rtc.getHour(h12Flag, pmFlag) * 100 + rtc.getMinute(), MASK_NO_DOTS, true);
    }

    if (edit_time_mode && edit_time_mode_num == 3) {
      // reset seconds

      rtc.setSecond(0);

      display.clear();
      display.showNumberDecEx(rtc.getSecond(), MASK_DOTS, true, 2, 2);
    }

    if (edit_alarm_mode && edit_alarm_mode_num == 1) {
      // increase hour
      alarm_hours += 1;

      if (alarm_hours >= 24)
        alarm_hours = 0;

      display.showNumberDecEx(alarm_hours * 100 + alarm_minutes, MASK_NO_DOTS, true);
      updateEEPROM();
    }

    if (edit_alarm_mode && edit_alarm_mode_num == 2) {
      // increase minutes
      alarm_minutes += 1;

      if (alarm_minutes >= 60)
        alarm_minutes = 0;

      display.showNumberDecEx(alarm_hours * 100 + alarm_minutes, MASK_NO_DOTS, true);
      updateEEPROM();
    }

    if (edit_alarm_mode && edit_alarm_mode_num == 3) {
      alarm_repeat = !alarm_repeat;
      updateEEPROM();
    }

    if (alarm_state) {
      // stop alarm
      alarm_state = false;
      current_melody_note = 0;
      alarm_time_repeat = 0;
    }
  }

  btn_action_last_state = btn_action_current_state;

  // check if setup ALARM BTN was pressed
  btn_alarm_current_state = debounce(BTN_ALARM_PIN, btn_alarm_last_state);

  if (btn_alarm_last_state == HIGH && btn_alarm_current_state == LOW)
  {
    if (edit_time_hours_mode || edit_time_minutes_mode)
      return;
    
    edit_alarm_mode_num++;

    //edit_alarm_mode
    switch (edit_alarm_mode_num) {
      case 1: // alarm mode hours
      case 2: // alarm mode minutes
      case 3: // alarm mode repeat
          edit_alarm_mode = true;
          normal_clock_mode = false;
        break;
    }

    if (edit_alarm_mode_num > 3)  {
      edit_alarm_mode = false; // exit alarm mode
      normal_clock_mode = true; // return normal mode for clock
      edit_alarm_mode_num = 0; // reset mode num
    }
  }

  btn_alarm_last_state = btn_alarm_current_state;

  // Edit Hours Mode
  if (edit_time_mode && edit_time_mode_num == 1) {
    if (millis() - blink_timer > TIMER_BLINK_TICK) {
      blink_timer = millis();  // reset digit timer
      blink_show_state = !blink_show_state;

      if (blink_show_state) {
        display.showNumberDecEx(rtc.getHour(h12Flag, pmFlag) * 100 + rtc.getMinute(), MASK_NO_DOTS, true);
      } else {
        display.clear();
        display.showNumberDecEx(rtc.getMinute(), MASK_NO_DOTS, true, 2, 2);
      }
    }
  }

  // Edit Minutes Mode
  if (edit_time_mode && edit_time_mode_num == 2) {
    if (millis() - blink_timer > TIMER_BLINK_TICK) {
      blink_timer = millis();  // reset digit timer
      blink_show_state = !blink_show_state;

      if (blink_show_state) {
        display.showNumberDecEx(rtc.getHour(h12Flag, pmFlag) * 100 + rtc.getMinute(), MASK_NO_DOTS, true);
      } else {
        display.clear();
        display.showNumberDecEx(rtc.getHour(h12Flag, pmFlag), MASK_NO_DOTS, true, 2, 0);
      }
    }
  }

  // Edit Seconds Mode
  if (edit_time_mode && edit_time_mode_num == 3) {
    if (millis() - blink_timer > TIMER_BLINK_TICK) {
      blink_timer = millis();  // reset digit timer
      blink_show_state = !blink_show_state;
      
      display.showNumberDecEx(rtc.getSecond(), MASK_DOTS, true, 2, 2);
    }
  }

  // Edit Alarm Hours Mode
  if (edit_alarm_mode && edit_alarm_mode_num == 1) {
    if (millis() - blink_timer > TIMER_BLINK_TICK) {
      blink_timer = millis();  // reset digit timer
      blink_show_state = !blink_show_state;

      if (blink_show_state) {
        display.showNumberDecEx(alarm_hours * 100 + alarm_minutes, MASK_NO_DOTS, true);
      } else {
        display.clear();
        display.showNumberDecEx(alarm_minutes, MASK_NO_DOTS, true, 2, 2);
      }
    }
  }
  
  // Edit Alarm Minutes Mode
  if (edit_alarm_mode && edit_alarm_mode_num == 2) {
    if (millis() - blink_timer > TIMER_BLINK_TICK) {
      blink_timer = millis();  // reset digit timer
      blink_show_state = !blink_show_state;

      if (blink_show_state) {
        display.showNumberDecEx(alarm_hours * 100 + alarm_minutes, MASK_NO_DOTS, true);
      } else {
        display.clear();
        display.showNumberDecEx(alarm_hours, MASK_NO_DOTS, true, 2, 0);
      }
    }
  }

  if (edit_alarm_mode && edit_alarm_mode_num == 3) {
    if (millis() - blink_timer > TIMER_BLINK_TICK) {
      blink_timer = millis();  // reset digit timer
      blink_show_state = !blink_show_state;

      if (blink_show_state) {
        data[0] = display.encodeDigit(10);
        display.setSegments(data);
        display.showNumberDecEx(alarm_repeat, MASK_NO_DOTS, true, 1, 3);
      } else {
        display.clear();
        data[0] = display.encodeDigit(10);
        display.setSegments(data);
      }
    }
  }

  if (normal_clock_mode) {
    if (millis() - digit_timer > TIMER_DELAY) {
      digit_timer = millis();  // reset digit timer

      display.showNumberDecEx(rtc.getHour(h12Flag, pmFlag) * 100 + rtc.getMinute(), mask, true);
    }

    if (millis() - dot_timer > TIMER_DOTS_TICK) {
      dot_timer = millis();  // reset dot timer

      if (mask == MASK_DOTS) {
        mask = MASK_NO_DOTS;
      } else {
        mask = MASK_DOTS;
      }
    }
  }

  // check alarm
  //if (normal_clock_mode) { // TODO: think about alarm in settings modes
    if (!alarm_state) {
      if (millis() - alarm_check_timer > TIMER_DOTS_TICK) {
        if ((alarm_hours == rtc.getHour(h12Flag, pmFlag)) && (alarm_minutes == rtc.getMinute()) && (rtc.getSecond() == 0)) {
          alarm_state = true;
        }
      }
    }
  //}
  
  // alarm play melody
  if (alarm_state) {
    if (millis() - melody_timer > noteDuration) {
      melody_timer = millis();  // reset melody stimer

      IPA_BuzzerToneNotes.stop();

      divider = melody[current_melody_note + 1];
      if (divider > 0) {
        // regular note, just proceed
        noteDuration = (wholenote) / divider;
      } else if (divider < 0) {
        // dotted notes are represented with negative durations!!
        noteDuration = (wholenote) / abs(divider);
        noteDuration *= 1.5;  // increases the duration in half for dotted notes
      }

      IPA_BuzzerToneNotes.play(melody[current_melody_note], noteDuration * 0.9);

      current_melody_note++;

      if (current_melody_note >= notes) {
        current_melody_note = 0;

        alarm_time_repeat++;

        if (alarm_time_repeat >= ALARM_MAX_TIME_REPEAT) {
          alarm_state = false;
          alarm_time_repeat = 0;
        }
      }
    }
  }
}
