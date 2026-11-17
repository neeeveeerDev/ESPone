#include "scene_ble_spam.h"
#include "ble_spam_engine.h"
#include <math.h>

namespace scene_ble_spam {

enum SpamSubScene : uint8_t {
  SUB_CATEGORIES = 0,
  SUB_APPLE_ACTIONS,
  SUB_APPLE_DEVICES,
  SUB_SPAM_RUN
};

static const char* kCategories[] = {
  "Apple actions",
  "Apple not your device",
  "Samsung pair",
  "Google fast pair",
  "Windows Swift Pair"
};
static const int kCategoryCount = sizeof(kCategories) / sizeof(kCategories[0]);

static const char* kAppleActions[] = {
  "Apple TV Setup",
  "Setup New Phone",
  "Transfer Number",
  "TV Color Balance",
  "Apple Vision Pro",
  "Apple TV Connecting",
  "Apple TV Audio Sync",
  "Setup New Apple TV",
  "HomePod Setup",
  "HomeKit Apple TV",
  "Pair Apple TV",
  "Setup New iPad"
};
static const int kAppleActionsCount = sizeof(kAppleActions) / sizeof(kAppleActions[0]);

static const char* kAppleDevices[] = {
  "AirPods Pro",
  "AirPods Pro 2nd Gen",
  "AirPods Pro 2nd Gen USB-C",
  "AirPods 4 ANC",
  "AirPods 4",
  "AirPods Max USB-C",
  "Beats Powerbeats Pro 2",
  "Beats Solo 3",
  "AirPods Max",
  "Beats Flex",
  "AirTag",
  "Hermes AirTag",
  "AirPods",
  "AirPods 2nd Gen",
  "AirPods 3rd Gen",
  "Powerbeats 3",
  "Powerbeats Pro",
  "Beats Solo Pro",
  "Beats Studio Buds",
  "Beats X",
  "Beats Studio 3",
  "Beats Studio Pro",
  "Beats Fit Pro",
  "Beats Studio Buds+",
  "Beats Solo 4",
  "Beats Solo Buds",
  "Powerbeats Fit"
};
static const int kAppleDevicesCount = sizeof(kAppleDevices) / sizeof(kAppleDevices[0]);

static const int kRowH = 12;
static const int kStartY = 16;

static SpamSubScene s_subScene = SUB_CATEGORIES;
static SpamSubScene s_prevSubScene = SUB_CATEGORIES;

static int s_catSel = 0;
static float s_catBarY = (float)kStartY;
static float s_catScroll = 0.0f;
static float s_catTargetScroll = 0.0f;

static int s_actionSel = 0;
static float s_actionBarY = (float)kStartY;
static float s_actionScroll = 0.0f;
static float s_actionTargetScroll = 0.0f;

static int s_deviceSel = 0;
static float s_deviceBarY = (float)kStartY;
static float s_deviceScroll = 0.0f;
static float s_deviceTargetScroll = 0.0f;

static const char* s_currentTargetName = "";
static bool s_isSpamming = false;
static uint32_t s_spamPacketCount = 0;
static uint32_t s_lastSpamTick = 0;


static ble_spam_engine::AttackType s_engineAttack = ble_spam_engine::GOOGLE_FASTPAIR;
static int s_engineDeviceIndex = -1;

static const uint32_t kSpamIntervalMs = 70;


static void updateScroll(int sel, int count, float& scroll, float& targetScroll, float& barY) {
  const int maxLines = 4;
  int maxScroll = count - maxLines;
  if (maxScroll < 0) maxScroll = 0;

  int topIndex = (int)(targetScroll / kRowH + 0.5f);
  if (sel < topIndex) {
    targetScroll = (float)(sel * kRowH);
  } else if (sel >= topIndex + maxLines) {
    targetScroll = (float)((sel - maxLines + 1) * kRowH);
  }

  if (targetScroll < 0.0f) targetScroll = 0.0f;
  if (targetScroll > (float)(maxScroll * kRowH)) targetScroll = (float)(maxScroll * kRowH);

  float ds = targetScroll - scroll;
  if (fabsf(ds) > 0.1f) {
    scroll += ds * 0.35f;
  } else {
    scroll = targetScroll;
  }

  float targetBarY = (float)(kStartY + sel * kRowH) - scroll;
  float dy = targetBarY - barY;
  if (fabsf(dy) > 0.1f) {
    barY += dy * 0.4f;
  } else {
    barY = targetBarY;
  }
}

static void drawHeader(U8G2& u8g2, const char* title) {
  u8g2.setDrawColor(1);
  u8g2.drawBox(0, 0, 128, 14);
  u8g2.setDrawColor(0);
  u8g2.setFont(u8g2_font_7x14B_tr);
  u8g2.drawStr(4, 11, title);
  u8g2.setDrawColor(1);
}

static void drawList(U8G2& u8g2, const char* const* items, int count, float scroll, float barY) {
  u8g2.setFont(u8g2_font_6x12_tr);
  for (int i = 0; i < count; i++) {
    int y = kStartY + (int)(i * kRowH - scroll + 0.5f);
    if (y < kStartY - kRowH || y > 64) continue;
    u8g2.drawStr(6, y + 10, items[i]);
  }


  u8g2.setDrawColor(2);
  int curY = (int)(barY + 0.5f);
  if (curY < kStartY) curY = kStartY;
  if (curY > 64 - kRowH) curY = 64 - kRowH;
  u8g2.drawBox(2, curY, 124, kRowH);
  u8g2.setDrawColor(1);
}

static void drawSpamRun(U8G2& u8g2, uint32_t now) {
  (void)now;
  drawHeader(u8g2, "BLE SPAM");


  u8g2.setFont(u8g2_font_6x12_tr);
  u8g2.setDrawColor(1);
  u8g2.drawStr(6, 25, s_currentTargetName);
  u8g2.drawHLine(4, 28, 120);


  u8g2.setFont(u8g2_font_5x8_tr);
  if (s_isSpamming) {
    u8g2.drawStr(6, 38, "Status: SPAMMING...");
    char buf[32];
    snprintf(buf, sizeof(buf), "Packets: %lu", (unsigned long)s_spamPacketCount);
    u8g2.drawStr(6, 47, buf);
  } else {
    u8g2.drawStr(6, 38, "Status: STANDBY");
    u8g2.drawStr(6, 47, "Press OK to spam");
  }


  const int btnW = 96;
  const int btnH = 13;
  const int btnX = (128 - btnW) / 2;
  const int btnY = 50;

  u8g2.drawRBox(btnX, btnY, btnW, btnH, 2);
  u8g2.setDrawColor(0);
  u8g2.setFont(u8g2_font_6x12_tr);
  const char* label = s_isSpamming ? "SPAMMING..." : "SPAM";
  int tw = u8g2.getStrWidth(label);
  u8g2.drawStr(btnX + (btnW - tw) / 2, btnY + 10, label);
  u8g2.setDrawColor(1);
}

void open() {
  s_subScene = SUB_CATEGORIES;
  s_prevSubScene = SUB_CATEGORIES;

  s_catSel = 0;
  s_catBarY = (float)kStartY;
  s_catScroll = 0.0f;
  s_catTargetScroll = 0.0f;

  s_actionSel = 0;
  s_actionBarY = (float)kStartY;
  s_actionScroll = 0.0f;
  s_actionTargetScroll = 0.0f;

  s_deviceSel = 0;
  s_deviceBarY = (float)kStartY;
  s_deviceScroll = 0.0f;
  s_deviceTargetScroll = 0.0f;

  s_currentTargetName = "";
  s_isSpamming = false;
  s_spamPacketCount = 0;
  s_engineDeviceIndex = -1;


  ble_spam_engine::init();
}

void close() {
  s_isSpamming = false;
  s_subScene = SUB_CATEGORIES;

  ble_spam_engine::deinit();
}

bool onBack() {
  if (s_subScene == SUB_SPAM_RUN) {
    if (s_isSpamming) {
      s_isSpamming = false;
      ble_spam_engine::stopAdvertising();
      return true;
    }
    s_subScene = s_prevSubScene;
    return true;
  }
  if (s_subScene == SUB_APPLE_ACTIONS || s_subScene == SUB_APPLE_DEVICES) {
    s_subScene = SUB_CATEGORIES;
    return true;
  }

  return false;
}

void tick(uint32_t now) {
  if (s_subScene == SUB_CATEGORIES) {
    updateScroll(s_catSel, kCategoryCount, s_catScroll, s_catTargetScroll, s_catBarY);
  } else if (s_subScene == SUB_APPLE_ACTIONS) {
    updateScroll(s_actionSel, kAppleActionsCount, s_actionScroll, s_actionTargetScroll, s_actionBarY);
  } else if (s_subScene == SUB_APPLE_DEVICES) {
    updateScroll(s_deviceSel, kAppleDevicesCount, s_deviceScroll, s_deviceTargetScroll, s_deviceBarY);
  } else if (s_subScene == SUB_SPAM_RUN) {
    if (s_isSpamming) {
      if (now - s_lastSpamTick >= kSpamIntervalMs) {
        s_lastSpamTick = now;

        ble_spam_engine::sendPacket(s_engineAttack, s_engineDeviceIndex);
        s_spamPacketCount = ble_spam_engine::getPacketCount();
      }
    }
  }
}

void onKey(const KeyEvent& ev) {
  if (s_subScene == SUB_CATEGORIES) {
    if (ev.key == BTN_UP) {
      s_catSel = (s_catSel - 1 + kCategoryCount) % kCategoryCount;
    } else if (ev.key == BTN_DOWN) {
      s_catSel = (s_catSel + 1) % kCategoryCount;
    } else if (ev.key == BTN_OK && !ev.repeat) {
      if (s_catSel == 0) {
        s_subScene = SUB_APPLE_ACTIONS;
      } else if (s_catSel == 1) {
        s_subScene = SUB_APPLE_DEVICES;
      } else {
        s_currentTargetName = kCategories[s_catSel];
        s_prevSubScene = SUB_CATEGORIES;
        s_subScene = SUB_SPAM_RUN;
        s_isSpamming = false;
        s_spamPacketCount = 0;
        ble_spam_engine::resetPacketCount();


        if (s_catSel == 2)      { s_engineAttack = ble_spam_engine::SAMSUNG;           s_engineDeviceIndex = 2; }
        else if (s_catSel == 3) { s_engineAttack = ble_spam_engine::GOOGLE_FASTPAIR;   s_engineDeviceIndex = -1; }
        else if (s_catSel == 4) { s_engineAttack = ble_spam_engine::WINDOWS_SWIFT_PAIR; s_engineDeviceIndex = -1; }
      }
    }
  } else if (s_subScene == SUB_APPLE_ACTIONS) {
    if (ev.key == BTN_UP) {
      s_actionSel = (s_actionSel - 1 + kAppleActionsCount) % kAppleActionsCount;
    } else if (ev.key == BTN_DOWN) {
      s_actionSel = (s_actionSel + 1) % kAppleActionsCount;
    } else if (ev.key == BTN_OK && !ev.repeat) {
      s_currentTargetName = kAppleActions[s_actionSel];
      s_prevSubScene = SUB_APPLE_ACTIONS;
      s_subScene = SUB_SPAM_RUN;
      s_isSpamming = false;
      s_spamPacketCount = 0;
      ble_spam_engine::resetPacketCount();
      s_engineAttack      = ble_spam_engine::APPLE_ACTIONS;
      s_engineDeviceIndex = s_actionSel;
    }
  } else if (s_subScene == SUB_APPLE_DEVICES) {
    if (ev.key == BTN_UP) {
      s_deviceSel = (s_deviceSel - 1 + kAppleDevicesCount) % kAppleDevicesCount;
    } else if (ev.key == BTN_DOWN) {
      s_deviceSel = (s_deviceSel + 1) % kAppleDevicesCount;
    } else if (ev.key == BTN_OK && !ev.repeat) {
      s_currentTargetName = kAppleDevices[s_deviceSel];
      s_prevSubScene = SUB_APPLE_DEVICES;
      s_subScene = SUB_SPAM_RUN;
      s_isSpamming = false;
      s_spamPacketCount = 0;
      ble_spam_engine::resetPacketCount();
      s_engineAttack      = ble_spam_engine::APPLE_DEVICES;
      s_engineDeviceIndex = s_deviceSel;
    }
  } else if (s_subScene == SUB_SPAM_RUN) {
    if (ev.key == BTN_OK && !ev.repeat) {
      s_isSpamming = !s_isSpamming;
      if (!s_isSpamming) {
        ble_spam_engine::stopAdvertising();
      }
      s_lastSpamTick = millis();
    }
  }
}

void draw(U8G2& u8g2, uint32_t now) {
  switch (s_subScene) {
    case SUB_CATEGORIES:
      drawHeader(u8g2, "BLE SPAM");
      drawList(u8g2, kCategories, kCategoryCount, s_catScroll, s_catBarY);
      break;

    case SUB_APPLE_ACTIONS:
      drawHeader(u8g2, "APPLE ACTIONS");
      drawList(u8g2, kAppleActions, kAppleActionsCount, s_actionScroll, s_actionBarY);
      break;

    case SUB_APPLE_DEVICES:
      drawHeader(u8g2, "NOT YOUR DEV");
      drawList(u8g2, kAppleDevices, kAppleDevicesCount, s_deviceScroll, s_deviceBarY);
      break;

    case SUB_SPAM_RUN:
      drawSpamRun(u8g2, now);
      break;
  }
}

}
