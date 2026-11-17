#include "gpio_pins.h"
#include <Preferences.h>
#include "config.h"

namespace gpio_pins {

static const char* kLabels[PIN_COUNT] = {
  "A", "B", "C", "D", "E", "F", "G"
};

static const int8_t kDefaults[PIN_COUNT] = {
  DEFAULT_PIN_A,
  DEFAULT_PIN_B,
  DEFAULT_PIN_C,
  DEFAULT_PIN_D,
  DEFAULT_PIN_E,
  DEFAULT_PIN_F,
  DEFAULT_PIN_G
};

static int8_t s_pins[PIN_COUNT];
static bool s_inited = false;

void init() {
  if (s_inited) return;
  s_inited = true;

  Preferences prefs;
  prefs.begin("espone_gpio", true);

  for (int i = 0; i < PIN_COUNT; i++) {
    char key[12];
    snprintf(key, sizeof(key), "p_%s", kLabels[i]);
    s_pins[i] = prefs.getChar(key, kDefaults[i]);
  }
  prefs.end();

  Serial.println("[gpio] Custom letter pins loaded:");
  for (int i = 0; i < PIN_COUNT; i++) {
    Serial.printf("  Pin %s -> GPIO %d\n", kLabels[i], s_pins[i]);
  }
}

int8_t getPin(CasePinIndex idx) {
  if (!s_inited) init();
  if (idx >= 0 && idx < PIN_COUNT) {
    return s_pins[idx];
  }
  return -1;
}

void setPin(CasePinIndex idx, int8_t gpio) {
  if (!s_inited) init();
  if (idx >= 0 && idx < PIN_COUNT) {
    s_pins[idx] = gpio;
  }
}

const char* getLabel(CasePinIndex idx) {
  if (idx >= 0 && idx < PIN_COUNT) {
    return kLabels[idx];
  }
  return "?";
}

void save() {
  Preferences prefs;
  prefs.begin("espone_gpio", false);
  for (int i = 0; i < PIN_COUNT; i++) {
    char key[12];
    snprintf(key, sizeof(key), "p_%s", kLabels[i]);
    prefs.putChar(key, s_pins[i]);
  }
  prefs.end();
  Serial.println("[gpio] Pin mappings saved to NVS!");
}

void resetDefaults() {
  for (int i = 0; i < PIN_COUNT; i++) {
    s_pins[i] = kDefaults[i];
  }
  save();
  Serial.println("[gpio] Pin mappings reset to defaults");
}

}
