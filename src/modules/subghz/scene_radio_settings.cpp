#include "scene_radio_settings.h"
#include "cc1101_driver.h"

namespace scene_radio_settings {

static const uint32_t kFreqs[] = {433920000, 315000000, 868350000, 915000000};
static const int kFreqCount = 4;
static int s_freqIdx = 0;

static const char* kPresets[] = {"OOK 650k", "OOK 270k", "2-FSK 2.38k", "2-FSK 47.6k"};
static const cc1101::RadioPreset kPresetVals[] = {
  cc1101::PRESET_OOK_650,
  cc1101::PRESET_OOK_270,
  cc1101::PRESET_2FSK_238,
  cc1101::PRESET_2FSK_476
};
static const int kPresetCount = 4;
static int s_presetIdx = 0;

static const int kNoiseVals[] = {-85, -80, -75, -70, -120};
static const char* kNoiseLabels[] = {"Auto (-85dBm)", "Med (-80dBm)", "High (-75dBm)", "Max (-70dBm)", "OFF"};
static const int kNoiseCount = 5;
static int s_noiseIdx = 0;

static int s_selRow = 0;

void open() {
  uint32_t curF = cc1101::getFrequency();
  s_freqIdx = 0;
  for (int i = 0; i < kFreqCount; i++) {
    if (kFreqs[i] == curF) { s_freqIdx = i; break; }
  }
  s_presetIdx = (int)cc1101::getPreset();
  
  int curThresh = cc1101::getNoiseThreshold();
  s_noiseIdx = 0;
  for (int i = 0; i < kNoiseCount; i++) {
    if (kNoiseVals[i] == curThresh) { s_noiseIdx = i; break; }
  }

  s_selRow = 0;
}

void close() {}

void onKey(const KeyEvent& ev) {
  if (ev.key == BTN_UP) {
    if (s_selRow > 0) s_selRow--;
    else s_selRow = 2;
  } else if (ev.key == BTN_DOWN) {
    if (s_selRow < 2) s_selRow++;
    else s_selRow = 0;
  } else if (ev.key == BTN_LEFT || ev.key == BTN_RIGHT || (ev.key == BTN_OK && !ev.repeat)) {
    int dir = (ev.key == BTN_LEFT) ? -1 : 1;
    if (s_selRow == 0) {
      s_freqIdx = (s_freqIdx + dir + kFreqCount) % kFreqCount;
      cc1101::setFrequency(kFreqs[s_freqIdx]);
    } else if (s_selRow == 1) {
      s_presetIdx = (s_presetIdx + dir + kPresetCount) % kPresetCount;
      cc1101::setPreset(kPresetVals[s_presetIdx]);
    } else if (s_selRow == 2) {
      s_noiseIdx = (s_noiseIdx + dir + kNoiseCount) % kNoiseCount;
      cc1101::setNoiseThreshold(kNoiseVals[s_noiseIdx]);
    }
  }
}

void tick(uint32_t now) {
  (void)now;
}

void draw(U8G2& u8g2, uint32_t now) {
  (void)now;

  u8g2.setDrawColor(0);
  u8g2.drawBox(0, 0, 128, 14);
  u8g2.setDrawColor(1);
  u8g2.drawBox(0, 0, 128, 14);
  u8g2.setDrawColor(0);
  u8g2.setFont(u8g2_font_7x14B_tr);
  u8g2.drawStr(4, 11, "RADIO SETTINGS");
  u8g2.setDrawColor(1);

  u8g2.setFont(u8g2_font_5x8_tr);

  // Row 0: Frequency
  char fStr[32];
  snprintf(fStr, sizeof(fStr), "Freq: %.2f MHz", (float)kFreqs[s_freqIdx] / 1000000.0f);
  u8g2.drawStr(6, 25, fStr);
  if (s_selRow == 0) {
    u8g2.setDrawColor(2);
    u8g2.drawBox(4, 17, 120, 10);
    u8g2.setDrawColor(1);
  }

  // Row 1: Preset
  char pStr[32];
  snprintf(pStr, sizeof(pStr), "Mod: %s", kPresets[s_presetIdx]);
  u8g2.drawStr(6, 36, pStr);
  if (s_selRow == 1) {
    u8g2.setDrawColor(2);
    u8g2.drawBox(4, 28, 120, 10);
    u8g2.setDrawColor(1);
  }

  // Row 2: Noise Cut / Squelch
  char nStr[32];
  snprintf(nStr, sizeof(nStr), "NoiseCut: %s", kNoiseLabels[s_noiseIdx]);
  u8g2.drawStr(6, 47, nStr);
  if (s_selRow == 2) {
    u8g2.setDrawColor(2);
    u8g2.drawBox(4, 39, 120, 10);
    u8g2.setDrawColor(1);
  }

  // Bottom Status
  char mStr[32];
  snprintf(mStr, sizeof(mStr), "CC1101: %s", cc1101::isHardwareDetected() ? "CONNECTED (OK)" : "NOT FOUND");
  u8g2.drawStr(6, 60, mStr);
}

}
