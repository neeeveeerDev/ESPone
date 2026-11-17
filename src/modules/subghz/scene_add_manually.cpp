#include "scene_add_manually.h"
#include "subghz_parser.h"
#include "subghz_protocols.h"
#include "cc1101_driver.h"

namespace scene_add_manually {

static const char* kProtocols[] = {"Came", "Nice FLO", "Princeton", "Linear"};
static const int kProtCount = 4;
static int s_protIdx = 0;

static const uint32_t kFreqs[] = {433920000, 315000000, 868350000};
static const int kFreqCount = 3;
static int s_freqIdx = 0;

static uint32_t s_keyVal = 0x0E12;
static int s_selRow = 0;

static uint32_t s_txUntil = 0;
static const char* s_toast = nullptr;
static uint32_t s_toastUntil = 0;

void open() {
  s_protIdx = 0;
  s_freqIdx = 0;
  s_keyVal = 0x0E12;
  s_selRow = 0;
  s_txUntil = 0;
  s_toast = nullptr;
}

void close() {}

void onKey(const KeyEvent& ev) {
  if (ev.key == BTN_UP) {
    if (s_selRow > 0) s_selRow--;
    else s_selRow = 4;
  } else if (ev.key == BTN_DOWN) {
    if (s_selRow < 4) s_selRow++;
    else s_selRow = 0;
  } else if (ev.key == BTN_LEFT) {
    if (s_selRow == 0) {
      s_protIdx = (s_protIdx - 1 + kProtCount) % kProtCount;
    } else if (s_selRow == 1) {
      s_freqIdx = (s_freqIdx - 1 + kFreqCount) % kFreqCount;
    } else if (s_selRow == 2) {
      if (s_keyVal > 0) s_keyVal--;
    } else if (s_selRow == 4) {
      s_selRow = 3;
    }
  } else if (ev.key == BTN_RIGHT) {
    if (s_selRow == 0) {
      s_protIdx = (s_protIdx + 1) % kProtCount;
    } else if (s_selRow == 1) {
      s_freqIdx = (s_freqIdx + 1) % kFreqCount;
    } else if (s_selRow == 2) {
      s_keyVal++;
    } else if (s_selRow == 3) {
      s_selRow = 4;
    }
  } else if (ev.key == BTN_OK && !ev.repeat) {
    if (s_selRow == 0) {
      s_protIdx = (s_protIdx + 1) % kProtCount;
    } else if (s_selRow == 1) {
      s_freqIdx = (s_freqIdx + 1) % kFreqCount;
    } else if (s_selRow == 2) {
      s_keyVal += 0x10;
    } else if (s_selRow == 3) {

      s_txUntil = millis() + 450;
      std::vector<int32_t> timings;
      uint8_t bits = (s_protIdx == 2) ? 24 : (s_protIdx == 3 ? 10 : 12);
      uint32_t te = (s_protIdx == 1) ? 700 : (s_protIdx == 2 ? 400 : 320);
      cc1101::setFrequency(kFreqs[s_freqIdx]);
      if (subghz_protocols::encode(kProtocols[s_protIdx], s_keyVal, bits, te, timings)) {
        cc1101::sendRaw(timings.data(), timings.size(), 3);
      }
    } else if (s_selRow == 4) {

      SubGhzItem item;
      item.frequency = kFreqs[s_freqIdx];
      item.preset = "FuriHalSubGhzPresetOok650Async";
      item.protocol = kProtocols[s_protIdx];
      item.bits = (s_protIdx == 2) ? 24 : (s_protIdx == 3 ? 10 : 12);
      item.key = s_keyVal;
      item.te = (s_protIdx == 1) ? 700 : (s_protIdx == 2 ? 400 : 320);
      item.repeat = 3;

      char path[64];
      snprintf(path, sizeof(path), "/sub/%s_%04lX.sub", kProtocols[s_protIdx], (unsigned long)(s_keyVal & 0xFFFF));
      if (subghz_parser::saveFile(path, item)) {
        s_toast = "SAVED TO /sub/";
        s_toastUntil = millis() + 1500;
      } else {
        s_toast = "SAVE FAILED";
        s_toastUntil = millis() + 1500;
      }
    }
  }
}

void tick(uint32_t now) {
  if (s_txUntil > 0 && now >= s_txUntil) s_txUntil = 0;
  if (s_toast && now >= s_toastUntil) s_toast = nullptr;
}

void draw(U8G2& u8g2, uint32_t now) {
  (void)now;

  u8g2.setDrawColor(0);
  u8g2.drawBox(0, 0, 128, 14);
  u8g2.setDrawColor(1);
  u8g2.drawBox(0, 0, 128, 14);
  u8g2.setDrawColor(0);
  u8g2.setFont(u8g2_font_7x14B_tr);

  if (s_txUntil > 0) {
    u8g2.drawStr(4, 11, "TX >>>");
  } else {
    u8g2.drawStr(4, 11, "ADD MANUALLY");
  }

  if (s_toast) {
    u8g2.setFont(u8g2_font_5x8_tr);
    int tw = u8g2.getStrWidth(s_toast);
    u8g2.drawStr(124 - tw, 10, s_toast);
  }
  u8g2.setDrawColor(1);


  u8g2.setFont(u8g2_font_5x8_tr);


  char protStr[32];
  snprintf(protStr, sizeof(protStr), "Prot: %s", kProtocols[s_protIdx]);
  u8g2.drawStr(6, 24, protStr);
  if (s_selRow == 0) {
    u8g2.setDrawColor(2);
    u8g2.drawBox(4, 16, 120, 10);
    u8g2.setDrawColor(1);
  }


  char freqStr[32];
  snprintf(freqStr, sizeof(freqStr), "Freq: %.2f MHz", (float)kFreqs[s_freqIdx] / 1000000.0f);
  u8g2.drawStr(6, 35, freqStr);
  if (s_selRow == 1) {
    u8g2.setDrawColor(2);
    u8g2.drawBox(4, 27, 120, 10);
    u8g2.setDrawColor(1);
  }


  char keyStr[32];
  snprintf(keyStr, sizeof(keyStr), "Key: 0x%04lX", (unsigned long)(s_keyVal & 0xFFFF));
  u8g2.drawStr(6, 46, keyStr);
  if (s_selRow == 2) {
    u8g2.setDrawColor(2);
    u8g2.drawBox(4, 38, 120, 10);
    u8g2.setDrawColor(1);
  }


  int btnW = 54;
  int btnH = 11;
  int yBtn = 51;

  u8g2.drawStr(24, yBtn + 8, "SEND");
  if (s_selRow == 3) {
    u8g2.setDrawColor(2);
    u8g2.drawBox(8, yBtn, btnW, btnH);
    u8g2.setDrawColor(1);
  }

  u8g2.drawStr(84, yBtn + 8, "SAVE");
  if (s_selRow == 4) {
    u8g2.setDrawColor(2);
    u8g2.drawBox(68, yBtn, btnW, btnH);
    u8g2.setDrawColor(1);
  }
}

}
