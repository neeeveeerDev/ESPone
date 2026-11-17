#pragma once
#include <Arduino.h>

namespace battery {

void init();
void tick(uint32_t now);

uint8_t  getBars();          // 0 to 4 bars
uint16_t getMillivolts();    // Battery voltage in mV (after 2x divider)
uint8_t  getPercentage();    // 0 to 100%
bool     isUsbPowered();     // True if running on USB or no battery
bool     isCharging();       // True if hardware charging pin active (TP4056 CHRG)
bool     isLow();            // True if critically low (< 3350 mV)

// Developer simulation / testing (pass < 0 to reset to automatic hardware sensing)
void     setTestPercentage(int16_t pct);
void     setTestCharging(int8_t state);

}
