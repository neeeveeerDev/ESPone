#include "scene_custom_sub.h"
#include <FFat.h>
#include <math.h>
#include <vector>
#include "subghz_parser.h"
#include "subghz_protocols.h"
#include "cc1101_driver.h"

namespace scene_custom_sub {

enum SubScene {
  SUB_CATEGORIES = 0,
  SUB_BUTTON_LIST,
  SUB_LEARNING
};

static SubScene s_sub = SUB_CATEGORIES;

struct DeviceCategory {
  const char* name;
  const char* const* buttons;
  int buttonCount;
};

static const char* kGateButtons[] = {"Open", "Close", "Stop", "Pedestrian"};
static const char* kGarageButtons[] = {"Door 1", "Door 2", "Light", "Lock"};
static const char* kRelayButtons[] = {"Ch A ON", "Ch A OFF", "Ch B ON", "Ch B OFF", "All ON", "All OFF"};
static const char* kAlarmButtons[] = {"Arm", "Disarm", "Trunk", "Panic"};
static const char* kCraneButtons[] = {"Up", "Down", "East", "West", "Start", "Stop"};

static const DeviceCategory kCategories[] = {
  {"Gate / Barrier", kGateButtons, sizeof(kGateButtons)/sizeof(kGateButtons[0])},
  {"Garage Door",    kGarageButtons, sizeof(kGarageButtons)/sizeof(kGarageButtons[0])},
  {"Relay / Sockets",kRelayButtons, sizeof(kRelayButtons)/sizeof(kRelayButtons[0])},
  {"Car / Alarm",    kAlarmButtons, sizeof(kAlarmButtons)/sizeof(kAlarmButtons[0])},
  {"Crane / Hoist",  kCraneButtons, sizeof(kCraneButtons)/sizeof(kCraneButtons[0])}
};
static const int kCatCount = sizeof(kCategories) / sizeof(kCategories[0]);

static int s_catSel = 0;
static float s_catBarY = 16.0f;
static float s_catScroll = 0.0f;
static float s_catTargetScroll = 0.0f;

struct LearnedSubBtn {
  String name;
  bool learned = false;
  uint32_t frequency = 433920000;
  String preset = "FuriHalSubGhzPresetOok650Async";
  String protocol = "Came";
  uint8_t bits = 12;
  uint64_t key = 0;
  uint32_t te = 320;
};
static std::vector<LearnedSubBtn> s_buttons;

static int s_btnSel = 0;
static float s_btnBarY = 16.0f;
static float s_btnScroll = 0.0f;
static float s_btnTargetScroll = 0.0f;

static int s_activeLearnIdx = -1;
static uint32_t s_feedbackUntil = 0;
static const char* s_feedbackMsg = nullptr;
static uint32_t s_txUntil = 0;

static const int rowH = 12;
static const int startY = 16;
static const int visibleRows = 4;

static void setupButtonsForCategory(int catIdx) {
  s_buttons.clear();
  const auto& cat = kCategories[catIdx];
  for (int i = 0; i < cat.buttonCount; i++) {
    LearnedSubBtn b;
    b.name = cat.buttons[i];
    b.learned = false;
    b.frequency = cc1101::getFrequency();
    s_buttons.push_back(b);
  }
}

void open() {
  s_sub = SUB_CATEGORIES;
  s_catSel = 0;
  s_catBarY = startY;
  s_catScroll = 0.0f;
  s_catTargetScroll = 0.0f;
  s_btnSel = 0;
  s_btnBarY = startY;
  s_btnScroll = 0.0f;
  s_btnTargetScroll = 0.0f;
  s_activeLearnIdx = -1;
  s_feedbackMsg = nullptr;
  s_feedbackUntil = 0;
  s_txUntil = 0;
}

void close() {
  if (s_sub == SUB_LEARNING) {
    cc1101::stopReceive();
  }
  s_buttons.clear();
  s_sub = SUB_CATEGORIES;
}

bool onBack() {
  if (s_sub == SUB_LEARNING) {
    cc1101::stopReceive();
    s_sub = SUB_BUTTON_LIST;
    return true;
  }
  if (s_sub == SUB_BUTTON_LIST) {
    s_sub = SUB_CATEGORIES;
    return true;
  }
  return false;
}

static String generateNewRemotePath(const char* catName) {
  char prefix[32];
  snprintf(prefix, sizeof(prefix), "%s", catName);
  for (int i = 0; prefix[i]; i++) {
    if (prefix[i] == ' ' || prefix[i] == '/') prefix[i] = '_';
    else prefix[i] = tolower(prefix[i]);
  }

  for (int num = 1; num < 100; num++) {
    char p[64];
    snprintf(p, sizeof(p), "/sub/%s_%02d.sub", prefix, num);
    if (!FFat.exists(p)) {
      return String(p);
    }
  }
  return String("/sub/custom_remote.sub");
}

static bool saveLearnedRemote() {
  int learnedCount = 0;
  for (const auto& b : s_buttons) {
    if (b.learned) learnedCount++;
  }
  if (learnedCount == 0) return false;


  String basePath = generateNewRemotePath(kCategories[s_catSel].name);
  basePath.replace(".sub", "");

  for (const auto& b : s_buttons) {
    if (b.learned) {
      SubGhzItem item;
      item.frequency = b.frequency;
      item.preset = b.preset;
      item.protocol = b.protocol;
      item.bits = b.bits;
      item.key = b.key;
      item.te = b.te;
      item.repeat = 3;

      char btnPath[80];
      char safeName[32];
      snprintf(safeName, sizeof(safeName), "%s", b.name.c_str());
      for (int i = 0; safeName[i]; i++) {
        if (safeName[i] == ' ') safeName[i] = '_';
      }
      snprintf(btnPath, sizeof(btnPath), "%s_%s.sub", basePath.c_str(), safeName);
      subghz_parser::saveFile(btnPath, item);
    }
  }
  return true;
}

void tick(uint32_t now) {
  if (s_txUntil > 0 && now >= s_txUntil) s_txUntil = 0;
  if (s_feedbackMsg && now >= s_feedbackUntil) s_feedbackMsg = nullptr;

  if (s_sub == SUB_CATEGORIES) {
    float ds = s_catTargetScroll - s_catScroll;
    if (fabsf(ds) > 0.05f) s_catScroll += ds * 0.35f;
    else s_catScroll = s_catTargetScroll;

    float targetY = startY + (s_catSel - s_catScroll) * rowH;
    float dy = targetY - s_catBarY;
    if (fabsf(dy) > 0.1f) s_catBarY += dy * 0.35f;
    else s_catBarY = targetY;
  } else if (s_sub == SUB_BUTTON_LIST) {
    float ds = s_btnTargetScroll - s_btnScroll;
    if (fabsf(ds) > 0.05f) s_btnScroll += ds * 0.35f;
    else s_btnScroll = s_btnTargetScroll;

    float targetY = startY + (s_btnSel - s_btnScroll) * rowH;
    float dy = targetY - s_btnBarY;
    if (fabsf(dy) > 0.1f) s_btnBarY += dy * 0.35f;
    else s_btnBarY = targetY;
  } else if (s_sub == SUB_LEARNING) {
    int32_t timings[128];
    size_t count = 0;
    if (cc1101::pollSignal(now, timings, 128, count)) {
      String prot;
      uint64_t key;
      uint8_t bits;
      uint32_t te;
      if (subghz_protocols::decode(timings, count, prot, key, bits, te)) {
        if (s_activeLearnIdx >= 0 && s_activeLearnIdx < (int)s_buttons.size()) {
          auto& btn = s_buttons[s_activeLearnIdx];
          btn.learned = true;
          btn.protocol = prot;
          btn.key = key;
          btn.bits = bits;
          btn.te = te;
        }
        cc1101::stopReceive();
        s_sub = SUB_BUTTON_LIST;
        s_feedbackMsg = "Captured!";
        s_feedbackUntil = now + 1200;
      }
    }
  }
}

void onKey(const KeyEvent& ev) {
  if (s_sub == SUB_CATEGORIES) {
    if (ev.key == BTN_UP) {
      if (s_catSel == 0) {
        s_catSel = kCatCount - 1;
        s_catTargetScroll = (kCatCount > visibleRows) ? (kCatCount - visibleRows) : 0;
      } else {
        s_catSel--;
        if (s_catSel < s_catTargetScroll) s_catTargetScroll = s_catSel;
      }
    } else if (ev.key == BTN_DOWN) {
      if (s_catSel == kCatCount - 1) {
        s_catSel = 0;
        s_catTargetScroll = 0.0f;
      } else {
        s_catSel++;
        if (s_catSel >= s_catTargetScroll + visibleRows) s_catTargetScroll = s_catSel - visibleRows + 1;
      }
    } else if (ev.key == BTN_OK && !ev.repeat) {
      setupButtonsForCategory(s_catSel);
      s_sub = SUB_BUTTON_LIST;
      s_btnSel = 0;
      s_btnBarY = startY;
      s_btnScroll = 0.0f;
      s_btnTargetScroll = 0.0f;
    }
  } else if (s_sub == SUB_BUTTON_LIST) {
    int total = (int)s_buttons.size() + 1;

    if (ev.key == BTN_UP) {
      if (s_btnSel == 0) {
        s_btnSel = total - 1;
        s_btnTargetScroll = (total > visibleRows) ? (total - visibleRows) : 0;
      } else {
        s_btnSel--;
        if (s_btnSel < s_btnTargetScroll) s_btnTargetScroll = s_btnSel;
      }
    } else if (ev.key == BTN_DOWN) {
      if (s_btnSel == total - 1) {
        s_btnSel = 0;
        s_btnTargetScroll = 0.0f;
      } else {
        s_btnSel++;
        if (s_btnSel >= s_btnTargetScroll + visibleRows) s_btnTargetScroll = s_btnSel - visibleRows + 1;
      }
    } else if (ev.key == BTN_OK && !ev.repeat) {
      if (s_btnSel < (int)s_buttons.size()) {
        auto& btn = s_buttons[s_btnSel];
        if (btn.learned) {

          s_txUntil = millis() + 450;
          std::vector<int32_t> timings;
          if (subghz_protocols::encode(btn.protocol, btn.key, btn.bits, btn.te, timings)) {
            cc1101::sendRaw(timings.data(), timings.size(), 3);
          }
        } else {

          s_activeLearnIdx = s_btnSel;
          s_sub = SUB_LEARNING;
          cc1101::startReceive();
        }
      } else {

        if (saveLearnedRemote()) {
          s_feedbackMsg = "SAVED TO /sub/!";
          s_feedbackUntil = millis() + 1500;
        } else {
          s_feedbackMsg = "No buttons caught!";
          s_feedbackUntil = millis() + 1500;
        }
      }
    } else if (ev.key == BTN_RIGHT && !ev.repeat) {

      if (s_btnSel < (int)s_buttons.size()) {
        s_activeLearnIdx = s_btnSel;
        s_sub = SUB_LEARNING;
        cc1101::startReceive();
      }
    }
  } else if (s_sub == SUB_LEARNING) {
    if (ev.key == BTN_OK && !ev.repeat) {


      if (s_activeLearnIdx >= 0 && s_activeLearnIdx < (int)s_buttons.size()) {
        auto& btn = s_buttons[s_activeLearnIdx];
        btn.learned = true;
        btn.protocol = "Came";
        btn.key = 0x0000000000000E12ULL + (s_activeLearnIdx * 0x10);
        btn.bits = 12;
        btn.te = 320;
      }
      cc1101::stopReceive();
      s_sub = SUB_BUTTON_LIST;
      s_feedbackMsg = "Captured!";
      s_feedbackUntil = millis() + 1200;
    }
  }
}

void draw(U8G2& u8g2, uint32_t now) {

  if (s_sub == SUB_CATEGORIES) {
    u8g2.setFont(u8g2_font_6x12_tr);
    for (int i = 0; i < kCatCount; i++) {
      int y = startY + (int)((i - s_catScroll) * rowH + 0.5f);
      if (y < 14 || y > 64) continue;
      u8g2.drawStr(6, y + 10, kCategories[i].name);
    }

    int bY = (int)(s_catBarY + 0.5f);
    if (bY >= 14 && bY <= 64 - rowH) {
      u8g2.setDrawColor(2);
      u8g2.drawBox(2, bY, 124, rowH);
      u8g2.setDrawColor(1);
    }
  } else if (s_sub == SUB_BUTTON_LIST) {
    u8g2.setFont(u8g2_font_5x8_tr);
    int total = (int)s_buttons.size() + 1;

    for (int i = 0; i < total; i++) {
      int y = startY + (int)((i - s_btnScroll) * rowH + 0.5f);
      if (y < 14 || y > 64) continue;

      if (i < (int)s_buttons.size()) {
        const auto& b = s_buttons[i];
        u8g2.drawStr(6, y + 9, b.name.c_str());

        if (b.learned) {
          char info[24];
          snprintf(info, sizeof(info), "* %s %03X", b.protocol.c_str(), (uint16_t)(b.key & 0xFFF));
          int tw = u8g2.getStrWidth(info);
          u8g2.drawStr(124 - tw, y + 9, info);
        } else {
          u8g2.drawStr(108, y + 9, "[--]");
        }
      } else {
        u8g2.drawStr(6, y + 9, "SAVE REMOTE");
      }
    }


    int bY = (int)(s_btnBarY + 0.5f);
    if (bY >= 14 && bY <= 64 - rowH) {
      u8g2.setDrawColor(2);
      u8g2.drawBox(2, bY, 124, rowH);
      u8g2.setDrawColor(1);
    }
  } else if (s_sub == SUB_LEARNING) {
    if (!cc1101::isHardwareDetected()) {
      u8g2.setFont(u8g2_font_6x12_tr);
      const char* m1 = "NO CC1101 MODULE";
      u8g2.drawStr((128 - u8g2.getStrWidth(m1)) / 2, 34, m1);

      u8g2.setFont(u8g2_font_5x8_tr);
      const char* m2 = "Connect SPI module";
      u8g2.drawStr((128 - u8g2.getStrWidth(m2)) / 2, 47, m2);
    } else {
      u8g2.setFont(u8g2_font_6x12_tr);
      int dots = (now / 350) % 4;
      char waitStr[32] = "Press button on remote";
      u8g2.drawStr(4, 30, waitStr);


      int rssi = cc1101::getRssi();
      char rssiStr[32];
      snprintf(rssiStr, sizeof(rssiStr), "RSSI: %d dBm", rssi);
      u8g2.setFont(u8g2_font_5x8_tr);
      u8g2.drawStr(6, 44, rssiStr);


      int barW = map(constrain(rssi, -110, -40), -110, -40, 0, 116);
      u8g2.drawFrame(6, 50, 116, 7);
      if (barW > 0) {
        u8g2.drawBox(8, 52, barW, 3);
      }
    }
  }


  u8g2.setDrawColor(0);
  u8g2.drawBox(0, 0, 128, 14);
  u8g2.setDrawColor(1);
  u8g2.drawBox(0, 0, 128, 14);
  u8g2.setDrawColor(0);
  u8g2.setFont(u8g2_font_7x14B_tr);

  if (s_txUntil > 0) {
    u8g2.drawStr(4, 11, "TX >>>");
  } else if (s_sub == SUB_CATEGORIES) {
    u8g2.drawStr(4, 11, "CUSTOM REMOTE");
  } else if (s_sub == SUB_BUTTON_LIST) {
    u8g2.drawStr(4, 11, kCategories[s_catSel].name);
  } else if (s_sub == SUB_LEARNING) {
    char t[32];
    snprintf(t, sizeof(t), "LEARN: %s", s_buttons[s_activeLearnIdx].name.c_str());
    u8g2.drawStr(4, 11, t);
  }

  if (s_feedbackMsg) {
    u8g2.setFont(u8g2_font_5x8_tr);
    int tw = u8g2.getStrWidth(s_feedbackMsg);
    u8g2.drawStr(124 - tw, 10, s_feedbackMsg);
  }
  u8g2.setDrawColor(1);
}

}
