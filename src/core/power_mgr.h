#pragma once
#include <Arduino.h>

namespace power_mgr {

enum class CpuProfile : uint8_t {
  IDLE_ECO  = 80,    // 80 MHz (Home screen, clock, screensaver, power save)
  BALANCED  = 160,   // 160 MHz (Menu, File Manager, UI navigation)
  HIGH_PERF = 240    // 240 MHz (WiFi attacks, Wardriving, SubGHz, JS engine)
};

void init();
void setProfile(CpuProfile profile);
CpuProfile getProfile();
uint32_t getFrequencyMhz();

// Automated idle scaling
void tick(uint32_t now, uint32_t lastActivity);
void notifyActivity();

} // namespace power_mgr
