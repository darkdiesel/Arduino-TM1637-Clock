#include "IPA_BuzzerToneNotes.h"
#include "Arduino.h"

IPA_BuzzerToneNotes::IPA_BuzzerToneNotes(uint8_t pin) {
  _pin = pin;
  init(pin);
}

void IPA_BuzzerToneNotes::init(uint8_t pin) {
  pinMode(pin, OUTPUT);
}

void IPA_BuzzerToneNotes::play(unsigned int frequency, unsigned long duration = 0){
  tone(_pin, frequency, duration);
}

void IPA_BuzzerToneNotes::stop(){
  noTone(_pin);
}

void IPA_BuzzerToneNotes::pause(){
  noTone(_pin);
}