#pragma once
#include <stdint.h>


enum BtnKey {
  BTN_UP, BTN_DOWN, BTN_LEFT, BTN_RIGHT, BTN_OK, BTN_BACK
};

struct KeyEvent {
  BtnKey key;
  bool repeat;
};
