#include "scene_read.h"
#include "cc1101_driver.h"
#include "subghz_parser.h"
#include "subghz_protocols.h"

namespace scene_read {

enum ReadState {
  STATE_LISTENING = 0,
  STATE_DETECTED
};

static ReadState s_state = STATE_LISTENING;
static String s_protocol = "";
static uint64_t s_key = 0;
static uint8_t s_bits = 0;
static uint32_t s_te = 320;
static int s_actionSel = 0;
static const char* kActions[] = {"SEND", "SAVE", "SCAN"};
static const int kActionCount = 3;

static uint32_t s_txUntil = 0;
static const char* s_toast = nullptr;
static uint32_t s_toastUntil = 0;

void open() {
  s_state = STATE_LISTENING;
  s_protocol = "";
  s_key = 0;
  s_bits = 0;
  s_actionSel = 0;
  s_txUntil = 0;
  s_toast = nullptr;
  cc1101::startReceive();
}

void close() {
  cc1101::stopReceive();
}

void onKey(const KeyEvent& ev) {
  if (s_state == STATE_LISTENING) {
    if (ev.key == BTN_OK && !ev.repeat) {


      s_protocol = "Came";
      s_key = 0x0000000000000E12ULL;
      s_bits = 12;
      s_te = 320;
      s_state = STATE_DETECTED;
      s_actionSel = 0;
    }
  } else if (s_state == STATE_DETECTED) {
    if (ev.key == BTN_LEFT && !ev.repeat) {
      s_actionSel = (s_actionSel - 1 + kActionCount) % kActionCount;
    } else if (ev.key == BTN_RIGHT && !ev.repeat) {
      s_actionSel = (s_actionSel + 1) % kActionCount;
    } else if (ev.key == BTN_OK && !ev.repeat) {
      if (s_actionSel == 0) {

        std::vector<int32_t> timings;
        if (subghz_protocols::encode(s_protocol, s_key, s_bits, s_te, timings)) {
          s_txUntil = millis() + 400;
          cc1101::sendRaw(timings.data(), timings.size(), 3);
        }
      } else if (s_actionSel == 1) {

        SubGhzItem item;
        item.frequency = cc1101::getFrequency();
        item.preset = "FuriHalSubGhzPresetOok650Async";
        item.protocol = s_protocol;
        item.bits = s_bits;
        item.key = s_key;
        item.te = s_te;
        item.repeat = 3;

        char path[64];
        snprintf(path, sizeof(path), "/sub/%s_%04X.sub", s_protocol.c_str(), (uint16_t)(s_key & 0xFFFF));
        if (subghz_parser::saveFile(path, item)) {
          s_toast = "SAVED TO /sub/";
          s_toastUntil = millis() + 1500;
        } else {
          s_toast = "SAVE FAILED";
          s_toastUntil = millis() + 1500;
        }
      } else if (s_actionSel == 2) {

        s_state = STATE_LISTENING;
        s_protocol = "";
        s_key = 0;
        s_bits = 0;
      }
    }
  }
}

void tick(uint32_t now) {
  if (s_txUntil > 0 && now >= s_txUntil) {
    s_txUntil = 0;
  }
  if (s_toast && now >= s_toastUntil) {
    s_toast = nullptr;
  }

  if (s_state == STATE_LISTENING) {
    int32_t timings[128];
    size_t count = 0;
    if (cc1101::pollSignal(now, timings, 128, count)) {
      String prot;
      uint64_t key;
      uint8_t bits;
      uint32_t te;
      if (subghz_protocols::decode(timings, count, prot, key, bits, te)) {
        s_protocol = prot;
        s_key = key;
        s_bits = bits;
        s_te = te;
        s_state = STATE_DETECTED;
        s_actionSel = 0;
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
    u8g2.drawStr(4, 11, "TX >>>");
  } else if (s_state == STATE_DETECTED) {
    u8g2.drawStr(4, 11, "SIGNAL CAUGHT");
  } else {
    char title[32];
    snprintf(title, sizeof(title), "READ %.2f", (float)cc1101::getFrequency() / 1000000.0f);
    u8g2.drawStr(4, 11, title);
  }

  if (s_toast) {
    u8g2.setFont(u8g2_font_5x8_tr);
    int tw = u8g2.getStrWidth(s_toast);
    u8g2.drawStr(124 - tw, 10, s_toast);
  }

  u8g2.setDrawColor(1);

  if (s_state == STATE_LISTENING) {
    if (!cc1101::isHardwareDetected()) {
      u8g2.setFont(u8g2_font_6x12_tr);
      const char* m1 = "NO CC1101 MODULE";
      u8g2.drawStr((128 - u8g2.getStrWidth(m1)) / 2, 34, m1);

      u8g2.setFont(u8g2_font_5x8_tr);
      const char* m2 = "Connect SPI Module";
      u8g2.drawStr((128 - u8g2.getStrWidth(m2)) / 2, 47, m2);
    } else {

      u8g2.setFont(u8g2_font_6x12_tr);
      int dots = (now / 350) % 4;
      char waitStr[24] = "Listening";
      for (int i = 0; i < dots; i++) strcat(waitStr, ".");
      u8g2.drawStr(6, 32, waitStr);


      int rssi = cc1101::getRssi();
      char rssiStr[32];
      snprintf(rssiStr, sizeof(rssiStr), "RSSI: %d dBm", rssi);
      u8g2.setFont(u8g2_font_5x8_tr);
      u8g2.drawStr(6, 46, rssiStr);


      int barW = map(constrain(rssi, -110, -40), -110, -40, 0, 116);
      u8g2.drawFrame(6, 52, 116, 7);
      if (barW > 0) {
        u8g2.drawBox(8, 54, barW, 3);
      }
    }
  } else {

    u8g2.setFont(u8g2_font_6x12_tr);
    char line1[32];
    snprintf(line1, sizeof(line1), "%s (%db)", s_protocol.c_str(), s_bits);
    u8g2.drawStr(6, 28, line1);

    char line2[32];
    snprintf(line2, sizeof(line2), "Key: 0x%06lX", (unsigned long)(s_key & 0xFFFFFF));
    u8g2.drawStr(6, 42, line2);


    int btnW = 38;
    int btnH = 12;
    int yBtn = 50;

    for (int i = 0; i < kActionCount; i++) {
      int xBtn = 6 + i * 42;
      u8g2.setFont(u8g2_font_5x8_tr);
      int tw = u8g2.getStrWidth(kActions[i]);
      u8g2.drawStr(xBtn + (btnW - tw) / 2, yBtn + 9, kActions[i]);

      if (i == s_actionSel) {
        u8g2.setDrawColor(2);
        u8g2.drawBox(xBtn, yBtn, btnW, btnH);
        u8g2.setDrawColor(1);
      }
    }
  }
}

}
