#include "scene_read_raw.h"
#include "cc1101_driver.h"
#include "subghz_parser.h"
#include "config.h"
#include <vector>

namespace scene_read_raw {

static bool s_recording = true;
static std::vector<int32_t> s_rawSamples;
static uint8_t s_waveBuffer[120];
static int s_waveHead = 0;

static int s_btnSel = 0;
static const char* kButtons[] = {"STOP", "SAVE", "SEND"};
static const int kBtnCount = 3;

static uint32_t s_txUntil = 0;
static const char* s_toast = nullptr;
static uint32_t s_toastUntil = 0;

void open() {
  s_recording = true;
  s_rawSamples.clear();
  memset(s_waveBuffer, 0, sizeof(s_waveBuffer));
  s_waveHead = 0;
  s_btnSel = 0;
  s_txUntil = 0;
  s_toast = nullptr;
  cc1101::startReceive();
}

void close() {
  s_recording = false;
  cc1101::stopReceive();
}

void onKey(const KeyEvent& ev) {
  if (ev.key == BTN_LEFT && !ev.repeat) {
    s_btnSel = (s_btnSel - 1 + kBtnCount) % kBtnCount;
  } else if (ev.key == BTN_RIGHT && !ev.repeat) {
    s_btnSel = (s_btnSel + 1) % kBtnCount;
  } else if (ev.key == BTN_OK && !ev.repeat) {
    if (s_btnSel == 0) {

      s_recording = !s_recording;
      if (s_recording) {
        cc1101::startReceive();
      } else {
        cc1101::stopReceive();
      }
    } else if (s_btnSel == 1) {

      if (s_rawSamples.empty()) {
        s_toast = "NO DATA";
        s_toastUntil = millis() + 1200;
      } else {
        SubGhzItem item;
        item.frequency = cc1101::getFrequency();
        item.preset = "FuriHalSubGhzPresetOok650Async";
        item.protocol = "RAW";
        item.isRaw = true;
        item.rawData = s_rawSamples;

        char path[64];
        snprintf(path, sizeof(path), "/sub/raw_%lu.sub", (unsigned long)(millis() % 10000));
        if (subghz_parser::saveFile(path, item)) {
          s_toast = "SAVED RAW!";
          s_toastUntil = millis() + 1500;
        } else {
          s_toast = "SAVE FAILED";
          s_toastUntil = millis() + 1500;
        }
      }
    } else if (s_btnSel == 2) {

      if (!s_rawSamples.empty()) {
        s_txUntil = millis() + 500;
        cc1101::sendRaw(s_rawSamples.data(), s_rawSamples.size(), 2);
      }
    }
  }
}

void tick(uint32_t now) {
  if (s_txUntil > 0 && now >= s_txUntil) s_txUntil = 0;
  if (s_toast && now >= s_toastUntil) s_toast = nullptr;

  if (s_recording) {

    static uint32_t lastWave = 0;
    if (now - lastWave >= 40) {
      lastWave = now;
      uint8_t waveVal = 0;
      if (cc1101::isHardwareDetected()) {
        waveVal = (digitalRead(PIN_CC1101_GDO0) == HIGH) ? 14 : 0;
        s_waveBuffer[s_waveHead] = waveVal;
        s_waveHead = (s_waveHead + 1) % 120;

        if (waveVal > 0 && s_rawSamples.size() < 256) {
          s_rawSamples.push_back(320);
          s_rawSamples.push_back(-640);
        }
      } else {
        s_waveBuffer[s_waveHead] = 0;
        s_waveHead = (s_waveHead + 1) % 120;
      }
    }
  }
}

void draw(U8G2& u8g2, uint32_t now) {

  u8g2.setDrawColor(0);
  u8g2.drawBox(0, 0, 128, 14);
  u8g2.setDrawColor(1);
  u8g2.drawBox(0, 0, 128, 14);
  u8g2.setDrawColor(0);
  u8g2.setFont(u8g2_font_7x14B_tr);

  if (s_txUntil > 0) {
    u8g2.drawStr(4, 11, "TX RAW >>>");
  } else {
    char title[32];
    snprintf(title, sizeof(title), "RAW %.2f", (float)cc1101::getFrequency() / 1000000.0f);
    u8g2.drawStr(4, 11, title);
  }

  if (s_toast) {
    u8g2.setFont(u8g2_font_5x8_tr);
    int tw = u8g2.getStrWidth(s_toast);
    u8g2.drawStr(124 - tw, 10, s_toast);
  } else if (cc1101::isHardwareDetected()) {
    u8g2.setFont(u8g2_font_5x8_tr);
    char cntStr[20];
    snprintf(cntStr, sizeof(cntStr), "%d pts", (int)s_rawSamples.size());
    u8g2.drawStr(124 - u8g2.getStrWidth(cntStr), 10, cntStr);
  }
  u8g2.setDrawColor(1);


  u8g2.drawFrame(3, 17, 122, 28);
  for (int x = 0; x < 118; x++) {
    int idx = (s_waveHead + x) % 120;
    uint8_t h = s_waveBuffer[idx];
    if (h > 0) {
      u8g2.drawVLine(5 + x, 42 - h, h);
    } else {
      u8g2.drawPixel(5 + x, 42);
    }
  }

  if (!cc1101::isHardwareDetected()) {
    u8g2.setDrawColor(0);
    u8g2.drawBox(10, 21, 108, 20);
    u8g2.setDrawColor(1);
    u8g2.drawFrame(10, 21, 108, 20);
    u8g2.setFont(u8g2_font_5x8_tr);
    const char* m1 = "NO CC1101 MODULE";
    const char* m2 = "Connect SPI Module";
    u8g2.drawStr((128 - u8g2.getStrWidth(m1)) / 2, 30, m1);
    u8g2.drawStr((128 - u8g2.getStrWidth(m2)) / 2, 38, m2);
  }


  int btnW = 38;
  int btnH = 12;
  int yBtn = 49;

  for (int i = 0; i < kBtnCount; i++) {
    int xBtn = 6 + i * 42;
    const char* label = (i == 0) ? (s_recording ? "STOP" : "REC") : kButtons[i];

    u8g2.setFont(u8g2_font_5x8_tr);
    int tw = u8g2.getStrWidth(label);
    u8g2.drawStr(xBtn + (btnW - tw) / 2, yBtn + 9, label);

    if (i == s_btnSel) {
      u8g2.setDrawColor(2);
      u8g2.drawBox(xBtn, yBtn, btnW, btnH);
      u8g2.setDrawColor(1);
    }
  }
}

}
