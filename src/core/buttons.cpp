#include "buttons.h"
#include <Arduino.h>
#include "config.h"

namespace {

struct Button {
  uint8_t pin = 0;
  bool stable = false, lastRaw = false;
  uint32_t tEdge = 0, tHeld = 0, tRep = 0;

  void begin(uint8_t p) { pin = p; pinMode(pin, INPUT_PULLUP); }


  bool poll(uint32_t now, bool& isRepeat) {
    bool raw = digitalRead(pin) == LOW;
    isRepeat = false;
    if (raw != lastRaw) { lastRaw = raw; tEdge = now; }
    if (raw != stable && now - tEdge >= BTN_DEBOUNCE_MS) {
      stable = raw;
      if (stable) { tHeld = now; tRep = now; return true; }
    }
    if (stable && raw && now - tHeld >= BTN_REPEAT_DELAY_MS &&
        now - tRep >= BTN_REPEAT_MS) {
      tRep = now;
      isRepeat = true;
      return true;
    }
    return false;
  }
};


Button kBtns[6];
const uint8_t kPins[6] = {
    PIN_BTN_UP, PIN_BTN_DOWN, PIN_BTN_LEFT,
    PIN_BTN_RIGHT, PIN_BTN_OK, PIN_BTN_BACK,
};

static const int kQueueCap = 8;
static KeyEvent kInjected[kQueueCap];
static volatile int kHead = 0, kTail = 0;

}

namespace buttons {

void init() {
  for (int i = 0; i < 6; i++) kBtns[i].begin(kPins[i]);
}

void inject(BtnKey key, bool repeat) {
  int next = (kHead + 1) % kQueueCap;
  if (next != kTail) {
    kInjected[kHead] = { key, repeat };
    kHead = next;
  }
}

bool poll(KeyEvent& ev) {
  if (kTail != kHead) {
    ev = kInjected[kTail];
    kTail = (kTail + 1) % kQueueCap;
    return true;
  }
  uint32_t now = millis();
  for (int i = 0; i < 6; i++) {
    bool rep;
    if (kBtns[i].poll(now, rep)) {
      ev.key = (BtnKey)i;
      ev.repeat = rep;
      return true;
    }
  }
  return false;
}

}
