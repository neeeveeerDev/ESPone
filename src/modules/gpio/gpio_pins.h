#pragma once
#include <Arduino.h>

namespace gpio_pins {

enum CasePinIndex {
  PIN_IDX_A = 0,
  PIN_IDX_B,
  PIN_IDX_C,
  PIN_IDX_D,
  PIN_IDX_E,
  PIN_IDX_F,
  PIN_IDX_G,
  PIN_COUNT
};


void init();


int8_t getPin(CasePinIndex idx);
void setPin(CasePinIndex idx, int8_t gpio);


const char* getLabel(CasePinIndex idx);


void save();


void resetDefaults();

}
