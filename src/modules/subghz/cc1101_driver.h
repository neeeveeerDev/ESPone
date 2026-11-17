#pragma once
#include <Arduino.h>

namespace cc1101 {

enum RadioPreset : uint8_t {
  PRESET_OOK_270 = 0,
  PRESET_OOK_650,
  PRESET_2FSK_238,
  PRESET_2FSK_476
};

void init();
bool isHardwareDetected();

void setFrequency(uint32_t freqHz);
uint32_t getFrequency();

void setPreset(RadioPreset preset);
RadioPreset getPreset();

int getRssi();

void startReceive();
void stopReceive();
bool isReceiving();

void sendRaw(const int32_t* timings, size_t count, int repeat = 1);

bool pollSignal(uint32_t now, int32_t* outTimings, size_t maxCount, size_t& outCount);

// Noise cutoff / squelch control
void setNoiseThreshold(int thresholdDbm); // e.g. -85 dBm (or -120 for OFF)
int  getNoiseThreshold();

}
