#include "battery.h"
#include "config.h"

namespace battery {

static uint16_t s_filteredMv = 4200;
static uint8_t s_bars = 4;
static uint32_t s_lastSample = 0;
static bool s_usbMode = true;
static bool s_charging = false;
static int16_t s_testPct = -1;
static int8_t s_testCharging = -1;

void init() {
  analogSetPinAttenuation(PIN_BATTERY_ADC, ADC_11db);
#if defined(PIN_BATTERY_CHRG) && (PIN_BATTERY_CHRG >= 0)
  pinMode(PIN_BATTERY_CHRG, INPUT_PULLUP);
#endif
  // Perform immediate initial measurement
  tick(millis() + 1000);
}

void tick(uint32_t now) {
#if defined(PIN_BATTERY_CHRG) && (PIN_BATTERY_CHRG >= 0)
  // Diode cathode to TP4056 CHRG pin pulls GPIO LOW during active charge cycle
  s_charging = (digitalRead(PIN_BATTERY_CHRG) == LOW);
#else
  s_charging = false;
#endif

  if (s_lastSample != 0 && (now - s_lastSample < 1000)) return;
  s_lastSample = now;

  // 8-sample oversampling for ADC noise rejection
  uint32_t sum = 0;
  for (int i = 0; i < 8; i++) {
    sum += analogReadMilliVolts(PIN_BATTERY_ADC);
    delayMicroseconds(40);
  }
  uint32_t rawMv = sum / 8;

  // 1:2 divider calculation (4.2V max battery -> 2.1V ADC)
  uint32_t batMv = rawMv * 2;

  // If pin is disconnected or below 500mV, assume USB powered / no battery attached
  if (batMv < 500) {
    s_usbMode = true;
    s_filteredMv = 4200;
    s_bars = 4;
    return;
  }

  s_usbMode = false;
  if (s_filteredMv == 0) {
    s_filteredMv = batMv;
  } else {
    // Low-pass exponential smoothing filter: 75% history + 25% new sample
    s_filteredMv = (s_filteredMv * 3 + batMv) / 4;
  }

  // 4 divisions calculation for 1S LiPo:
  // 4 bars: >= 4000 mV (4.0V - 4.2V+)
  // 3 bars: >= 3750 mV (3.75V - 3.99V)
  // 2 bars: >= 3550 mV (3.55V - 3.74V)
  // 1 bar:  >= 3350 mV (3.35V - 3.54V)
  // 0 bars: < 3350 mV  (Critical / empty)
  if (s_filteredMv >= 4000) {
    s_bars = 4;
  } else if (s_filteredMv >= 3750) {
    s_bars = 3;
  } else if (s_filteredMv >= 3550) {
    s_bars = 2;
  } else if (s_filteredMv >= 3350) {
    s_bars = 1;
  } else {
    s_bars = 0;
  }
}

uint8_t getBars() {
  return s_bars;
}

uint16_t getMillivolts() {
  return s_filteredMv;
}

uint8_t getPercentage() {
  if (s_testPct >= 0) {
    return (uint8_t)(s_testPct > 100 ? 100 : s_testPct);
  }
  if (s_usbMode) return 100;
  if (s_filteredMv >= 4200) return 100;
  if (s_filteredMv <= 3300) return 0;
  return (uint8_t)(((s_filteredMv - 3300) * 100) / 900);
}

bool isUsbPowered() {
  return s_usbMode || isCharging();
}

bool isCharging() {
  if (s_testCharging >= 0) {
    return s_testCharging != 0;
  }
  return s_charging;
}

bool isLow() {
  return !s_usbMode && !isCharging() && (s_bars == 0);
}

void setTestPercentage(int16_t pct) {
  s_testPct = pct;
}

void setTestCharging(int8_t state) {
  s_testCharging = state;
}

} // namespace battery
