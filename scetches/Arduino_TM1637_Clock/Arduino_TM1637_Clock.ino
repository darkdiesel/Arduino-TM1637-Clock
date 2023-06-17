#include <Arduino.h>
#include <TM1637Display.h>
#include "IPA_BuzzerToneNotes.h"
#include <DS3231.h>
#include <Wire.h>
#include <EEPROM.h>

// Module connection pins (Digital Pins)
#define DISPLAY_CLK 2
#define DISPLAY_DIO 3

byte time_h = 0;
byte time_m = 0;

uint8_t data[] = { 0xff, 0xff, 0xff, 0xff };

// Display vars
#define TIMER_DELAY   500
#define TIMER_DOTS_TICK 1000

#define MASK_DOTS 0b01000000
#define MASK_NO_DOTS 0b00000000

TM1637Display display(DISPLAY_CLK, DISPLAY_DIO);

uint8_t mask; // to display dots

unsigned long digit_timer; // digit timer
unsigned long dot_timer; // digit timer

uint8_t current_brightness = 5;

// clock vars
DS3231 rtc;

bool h12Flag;
bool pmFlag;

bool edit_time_hours_mode = false;
bool edit_time_minutes_mode = false;

bool edit_alarm_hours_mode = false;
bool edit_alarm_minutes_mode = false;
bool edit_alarm_repeat_mode = false;

bool alarm_state = false; // on/off state for device

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
}

// read device settings
void readEEPROM() {
  current_brightness = EEPROM.read(1);

  if (current_brightness > 7) {
    current_brightness = 3;
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
  Serial.begin(9600);

  // load saved settings
  readEEPROM();
  Serial.print("Brightness: ");
  Serial.println(current_brightness);

  // setup display
  display.setBrightness(current_brightness);

  mask = MASK_NO_DOTS;

  // setup clock
  Wire.begin();
  rtc.setHour(2);
  rtc.setMinute(27);

  // setup buzzer
  //pinMode(BUZZER_PIN, OUTPUT);

  // setup buttons
  pinMode(BTN_BRIGHTNESS_PIN, INPUT_PULLUP);
  pinMode(BTN_ALARM_PIN, INPUT_PULLUP);
  pinMode(BTN_TIME_PIN, INPUT_PULLUP);
  pinMode(BTN_ACTION_PIN, INPUT_PULLUP);
}

void loop() {
    // check if change brightness button was pressed
  btn_brightness_current_state = debounce(BTN_BRIGHTNESS_PIN, btn_brightness_last_state);

  // if last state was low and curret low change mode. Current state low as pressed because used INPUT_PULLUP pin mode
  if (btn_brightness_last_state == HIGH && btn_brightness_current_state == LOW)
  {
    if (++current_brightness > 7) {
      current_brightness = 0;
    }

    display.setBrightness(current_brightness);
    
    Serial.print("Brightness: ");
    Serial.println(current_brightness);
    
    updateEEPROM();
  }

  btn_brightness_last_state = btn_brightness_current_state;


  if (millis() - digit_timer > TIMER_DELAY) {
    digit_timer = millis();  // reset digit timer

    // data[0] = display.encodeDigit(h1);
    // data[1] = display.encodeDigit(h2);
    // data[2] = display.encodeDigit(m1);
    // data[3] = display.encodeDigit(m2);
    // display.setSegments(data);

    display.showNumberDecEx(rtc.getHour(h12Flag, pmFlag) * 100 + rtc.getMinute(), mask, true);

    // Serial.print(rtc.getHour(h12Flag, pmFlag), DEC); //24-hr
    // Serial.print(":");
    // Serial.print(rtc.getMinute(), DEC);
    // Serial.print(":");
    // Serial.println(rtc.getSecond(), DEC);
  }

  if (millis() - dot_timer > TIMER_DOTS_TICK) {
    dot_timer = millis();  // reset dot timer

    if (mask == MASK_DOTS) {
      mask = MASK_NO_DOTS;
    } else {
      mask = MASK_DOTS;
    }
  }

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
      }
    }
  }
}
