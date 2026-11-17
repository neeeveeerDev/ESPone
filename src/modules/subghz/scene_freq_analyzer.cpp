#include "scene_freq_analyzer.h"
#include "cc1101_driver.h"

namespace scene_freq_analyzer {

static const uint32_t kScanFreqs[] = {
  315000000,
  433920000,
  868350000,
  915000000
};
static const int kScanCount = 4;

static int s_scanIndex = 0;
static uint32_t s_lockedFreq = 433920000;
static int s_maxRssi = -110;
static bool s_isHold = false;
static uint32_t s_lastSwitch = 0;

void open() {
  s_scanIndex = 0;
  s_lockedFreq = 433920000;
  s_maxRssi = -110;
  s_isHold = false;
  s_lastSwitch = 0;
  cc1101::startReceive();
}

void close() {
  cc1101::stopReceive();
}

void onKey(const KeyEvent& ev) {
  if (ev.key == BTN_OK && !ev.repeat) {
    s_isHold = !s_isHold;
  }
}

void tick(uint32_t now) {
  if (s_isHold || !cc1101::isHardwareDetected()) return;

  if (now - s_lastSwitch >= 60) {
    s_lastSwitch = now;
    s_scanIndex = (s_scanIndex + 1) % kScanCount;
    cc1101::setFrequency(kScanFreqs[s_scanIndex]);

    int rssi = cc1101::getRssi();
    if (rssi > s_maxRssi) {
      s_maxRssi = rssi;
      s_lockedFreq = kScanFreqs[s_scanIndex];
    } else if (s_maxRssi > -95) {
      s_maxRssi--;
    }
  }
}

void draw(U8G2& u8g2, uint32_t now) {
  (void)now;
  bool hw = cc1101::isHardwareDetected();


  u8g2.setDrawColor(0);
  u8g2.drawBox(0, 0, 128, 14);
  u8g2.setDrawColor(1);
  u8g2.drawBox(0, 0, 128, 14);
  u8g2.setDrawColor(0);
  u8g2.setFont(u8g2_font_7x14B_tr);
  u8g2.drawStr(4, 11, "FREQ ANALYZER");

  if (s_isHold || hw) {
    u8g2.setFont(u8g2_font_5x8_tr);
    const char* badge = s_isHold ? "HOLD" : "SCAN";
    u8g2.drawStr(124 - u8g2.getStrWidth(badge), 10, badge);
  }
  u8g2.setDrawColor(1);

  if (!hw) {
    u8g2.setDrawColor(0);
    u8g2.drawBox(10, 24, 108, 24);
    u8g2.setDrawColor(1);
    u8g2.drawFrame(10, 24, 108, 24);
    u8g2.setFont(u8g2_font_6x12_tr);
    const char* t1 = "NO CC1101 MODULE";
    u8g2.drawStr((128 - u8g2.getStrWidth(t1)) / 2, 36, t1);
    u8g2.setFont(u8g2_font_5x8_tr);
    const char* t2 = "Connect SPI Module";
    u8g2.drawStr((128 - u8g2.getStrWidth(t2)) / 2, 45, t2);
    return;
  }


  u8g2.setFont(u8g2_font_9x15B_tr);
  char fStr[32];
  snprintf(fStr, sizeof(fStr), "%.2f MHz", (float)s_lockedFreq / 1000000.0f);
  int fw = u8g2.getStrWidth(fStr);
  u8g2.drawStr((128 - fw) / 2, 33, fStr);


  u8g2.setFont(u8g2_font_5x8_tr);
  char rStr[32];
  int curRssi = cc1101::getRssi();
  snprintf(rStr, sizeof(rStr), "RSSI: %d dBm  Peak: %d", curRssi, s_maxRssi);
  int rw = u8g2.getStrWidth(rStr);
  u8g2.drawStr((128 - rw) / 2, 45, rStr);


  int barW = map(constrain(curRssi, -110, -40), -110, -40, 0, 116);
  u8g2.drawFrame(6, 51, 116, 8);
  if (barW > 0) {
    u8g2.drawBox(8, 53, barW, 4);
  }
}

}
