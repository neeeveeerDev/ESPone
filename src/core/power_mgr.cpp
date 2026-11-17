#include "power_mgr.h"
#include <esp32-hal-cpu.h>

namespace power_mgr {

static CpuProfile s_currentProfile = CpuProfile::BALANCED;
static bool s_manualLock = false;

void init() {
  // Start in Balanced mode (160 MHz) to save power right from boot
  setProfile(CpuProfile::BALANCED);
}

void setProfile(CpuProfile profile) {
  if (s_currentProfile == profile) return;
  s_currentProfile = profile;
  setCpuFrequencyMhz((uint32_t)profile);
  Serial.printf("[power_mgr] CPU Frequency scaled to %u MHz\n", (unsigned)profile);
}

CpuProfile getProfile() {
  return s_currentProfile;
}

uint32_t getFrequencyMhz() {
  return getCpuFrequencyMhz();
}

void notifyActivity() {
  if (s_currentProfile == CpuProfile::IDLE_ECO) {
    setProfile(CpuProfile::BALANCED);
  }
}

void tick(uint32_t now, uint32_t lastActivity) {
  // If user is idle for more than 3.5 seconds on desktop/menu, scale down to 80 MHz
  if (s_currentProfile != CpuProfile::IDLE_ECO && (now - lastActivity >= 3500)) {
    setProfile(CpuProfile::IDLE_ECO);
  }
}

} // namespace power_mgr
