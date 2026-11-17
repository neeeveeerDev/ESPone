#include "cc1101_driver.h"
#include "config.h"
#include <SPI.h>
#include <RadioLib.h>
#include <math.h>

namespace cc1101 {

static bool s_hardwareDetected = false;
static bool s_inited = false;
static bool s_receiving = false;
static uint32_t s_frequency = 433920000;
static RadioPreset s_preset = PRESET_OOK_650;
static int s_noiseThreshold = -85; // Default squelch / noise cutoff: -85 dBm

static Module* s_radioMod = nullptr;
static CC1101* s_radio = nullptr;

static void applyPreset(RadioPreset preset);
static void applyFrequency(uint32_t freqHz);

void init() {
  if (s_inited) return;
  s_inited = true;

  pinMode(PIN_CC1101_CS, OUTPUT);
  digitalWrite(PIN_CC1101_CS, HIGH);

  pinMode(PIN_CC1101_GDO0, INPUT);
  pinMode(PIN_CC1101_GDO2, INPUT);

  SPI.begin(PIN_CC1101_SCK, PIN_CC1101_MISO, PIN_CC1101_MOSI, PIN_CC1101_CS);

  s_radioMod = new Module(PIN_CC1101_CS, PIN_CC1101_GDO0, RADIOLIB_NC, PIN_CC1101_GDO2, SPI);
  s_radio = new CC1101(s_radioMod);

  float freqMhz = (float)s_frequency / 1000000.0f;
  int16_t state = s_radio->begin(freqMhz, 4.8, 5.0, 135.0, 10, 16);

  if (state == RADIOLIB_ERR_NONE) {
    s_hardwareDetected = true;
    Serial.println("[cc1101] RadioLib: CC1101 initialized successfully!");
  } else {
    s_hardwareDetected = false;
    Serial.printf("[cc1101] RadioLib: CC1101 init failed, code %d. Hardware not detected.\n", state);
  }

  applyPreset(s_preset);
  applyFrequency(s_frequency);
}

bool isHardwareDetected() {
  return s_hardwareDetected;
}

static void applyFrequency(uint32_t freqHz) {
  s_frequency = freqHz;
  if (!s_hardwareDetected || !s_radio) return;
  float freqMhz = (float)freqHz / 1000000.0f;
  s_radio->setFrequency(freqMhz);
  if (s_receiving) {
    s_radio->receiveDirectAsync();
  }
}

void setFrequency(uint32_t freqHz) {
  applyFrequency(freqHz);
}

uint32_t getFrequency() {
  return s_frequency;
}

static void applyPreset(RadioPreset preset) {
  s_preset = preset;
  if (!s_hardwareDetected || !s_radio) return;

  s_radio->standby();

  switch (preset) {
    case PRESET_OOK_270:
      s_radio->setOOK(true);
      s_radio->setRxBandwidth(270.0);
      s_radio->setBitRate(4.8);
      break;

    case PRESET_OOK_650:
    default:
      s_radio->setOOK(true);
      s_radio->setRxBandwidth(650.0);
      s_radio->setBitRate(4.8);
      break;

    case PRESET_2FSK_238:
      s_radio->setOOK(false);
      s_radio->setFrequencyDeviation(15.0);
      s_radio->setRxBandwidth(270.0);
      s_radio->setBitRate(2.38);
      break;

    case PRESET_2FSK_476:
      s_radio->setOOK(false);
      s_radio->setFrequencyDeviation(47.0);
      s_radio->setRxBandwidth(650.0);
      s_radio->setBitRate(47.6);
      break;
  }

  // Optimize CC1101 AGC registers via RadioLib Module SPI helper
  if (s_radioMod) {
    s_radioMod->SPIwriteRegister(0x1B, 0x43); // AGCCTRL2
    s_radioMod->SPIwriteRegister(0x1C, 0x40); // AGCCTRL1
    s_radioMod->SPIwriteRegister(0x1D, 0x91); // AGCCTRL0
  }

  if (s_receiving) {
    s_radio->receiveDirectAsync();
  }
}

void setPreset(RadioPreset preset) {
  applyPreset(preset);
}

RadioPreset getPreset() {
  return s_preset;
}

int getRssi() {
  if (s_hardwareDetected && s_radio) {
    float r = s_radio->getRSSI();
    return (int)roundf(r);
  }
  return -120;
}

void setNoiseThreshold(int thresholdDbm) {
  s_noiseThreshold = thresholdDbm;
}

int getNoiseThreshold() {
  return s_noiseThreshold;
}

void startReceive() {
  s_receiving = true;
  if (s_hardwareDetected && s_radio) {
    pinMode(PIN_CC1101_GDO0, INPUT);
    s_radio->receiveDirectAsync();
  }
}

void stopReceive() {
  s_receiving = false;
  if (s_hardwareDetected && s_radio) {
    s_radio->standby();
  }
}

bool isReceiving() {
  return s_receiving;
}

void sendRaw(const int32_t* timings, size_t count, int repeat) {
  if (count == 0 || !timings) return;

  if (s_hardwareDetected && s_radio) {
    s_radio->transmitDirectAsync();
    pinMode(PIN_CC1101_GDO0, OUTPUT);
    digitalWrite(PIN_CC1101_GDO0, LOW);
    delayMicroseconds(200);

    for (int r = 0; r < repeat; r++) {
      for (size_t i = 0; i < count; i++) {
        int32_t t = timings[i];
        if (t > 0) {
          digitalWrite(PIN_CC1101_GDO0, HIGH);
          delayMicroseconds((uint32_t)t);
        } else if (t < 0) {
          digitalWrite(PIN_CC1101_GDO0, LOW);
          delayMicroseconds((uint32_t)(-t));
        }
      }
    }
    digitalWrite(PIN_CC1101_GDO0, LOW);
    pinMode(PIN_CC1101_GDO0, INPUT);
    s_radio->standby();
    if (s_receiving) {
      s_radio->receiveDirectAsync();
    }
  } else {
    // Hardware not connected: simulate timing for UI
    uint32_t totalUs = 0;
    for (size_t i = 0; i < count && i < 100; i++) {
      totalUs += (timings[i] > 0 ? timings[i] : -timings[i]);
    }
    uint32_t delayMs = (totalUs / 1000) * repeat;
    if (delayMs > 300) delayMs = 300;
    if (delayMs < 100) delayMs = 100;
    delay(delayMs);
  }
}

bool pollSignal(uint32_t now, int32_t* outTimings, size_t maxCount, size_t& outCount) {
  outCount = 0;
  if (!s_receiving) return false;

  if (s_hardwareDetected && s_radio) {
    if (digitalRead(PIN_CC1101_GDO0) == HIGH) {
      // Noise cutoff by RSSI squelch threshold
      if (s_noiseThreshold > -120) {
        int curRssi = getRssi();
        if (curRssi < s_noiseThreshold) {
          return false;
        }
      }

      uint32_t startWait = micros();
      size_t idx = 0;
      int lastState = HIGH;
      uint32_t lastChange = micros();
      int glitchCount = 0;

      while (idx < maxCount && (micros() - startWait < 80000)) {
        int currState = digitalRead(PIN_CC1101_GDO0);
        if (currState != lastState) {
          uint32_t nowUs = micros();
          uint32_t dur = nowUs - lastChange;

          // Pulse duration noise filter: ignore high-frequency noise spikes < 100 us
          if (dur < 100) {
            glitchCount++;
            if (glitchCount > 25 && idx < 4) {
              return false;
            }
          } else if (dur <= 25000) {
            outTimings[idx++] = (lastState == HIGH) ? (int32_t)dur : -(int32_t)dur;
          }
          lastState = currState;
          lastChange = nowUs;
        }
      }

      // Valid packet: at least 16 transitions and not dominated by noise glitches
      if (idx >= 16 && glitchCount <= (int)idx) {
        outCount = idx;
        return true;
      }
    }
  }
  return false;
}

} // namespace cc1101
