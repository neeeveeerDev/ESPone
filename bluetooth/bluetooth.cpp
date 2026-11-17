#include "bluetooth.h"
#include "scene_ble_scan.h"
#include "scene_media_control.h"
#include "scene_ibeacon.h"
#include "scene_ble_spam.h"
#include "scene_badble.h"
#include <math.h>

namespace {
static const char* kMenuItems[] = {
  "BLE Scan",
  "Media Control",
  "iBeacon",
  "BLE Spam",
  "BadBLE"
};
static const int kMenuCount = sizeof(kMenuItems) / sizeof(kMenuItems[0]);
static const int kRowH = 12;
static const int kStartY = 16;

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
}

void BluetoothApp::open() {
  scene_ = SCENE_MENU;
  menuSel_ = 0;
  barY_ = (float)kStartY;
  scroll_ = 0.0f;
  targetScroll_ = 0.0f;
  Serial.println("[bluetooth] open");
}

void BluetoothApp::close() {
  if (scene_ == SCENE_BLE_SCAN) {
    scene_ble_scan::close();
  } else if (scene_ == SCENE_MEDIA_CONTROL) {
    scene_media_control::close();
  } else if (scene_ == SCENE_IBEACON) {
    scene_ibeacon::close();
  } else if (scene_ == SCENE_BLE_SPAM) {
    scene_ble_spam::close();
  } else if (scene_ == SCENE_BADBLE) {
    scene_badble::close();
  }
  scene_ = SCENE_MENU;
  Serial.println("[bluetooth] close");
}

bool BluetoothApp::onBack() {
  if (scene_ == SCENE_BLE_SCAN) {
    if (scene_ble_scan::onBack()) {
      return true;
    }
    scene_ble_scan::close();
    scene_ = SCENE_MENU;
    return true;
  } else if (scene_ == SCENE_MEDIA_CONTROL) {
    if (scene_media_control::onBack()) {
      return true;
    }
    scene_media_control::close();
    scene_ = SCENE_MENU;
    return true;
  } else if (scene_ == SCENE_IBEACON) {
    if (scene_ibeacon::onBack()) {
      return true;
    }
    scene_ibeacon::close();
    scene_ = SCENE_MENU;
    return true;
  } else if (scene_ == SCENE_BLE_SPAM) {
    if (scene_ble_spam::onBack()) {
      return true;
    }
    scene_ble_spam::close();
    scene_ = SCENE_MENU;
    return true;
  } else if (scene_ == SCENE_BADBLE) {
    if (scene_badble::onBack()) {
      return true;
    }
    scene_badble::close();
    scene_ = SCENE_MENU;
    return true;
  }
  return false;
}

void BluetoothApp::tick(uint32_t now) {
  if (scene_ == SCENE_MENU) {
    updateScroll(menuSel_, kMenuCount, scroll_, targetScroll_, barY_);
  } else if (scene_ == SCENE_BLE_SCAN) {
    scene_ble_scan::tick(now);
  } else if (scene_ == SCENE_MEDIA_CONTROL) {
    scene_media_control::tick(now);
  } else if (scene_ == SCENE_IBEACON) {
    scene_ibeacon::tick(now);
  } else if (scene_ == SCENE_BLE_SPAM) {
    scene_ble_spam::tick(now);
  } else if (scene_ == SCENE_BADBLE) {
    scene_badble::tick(now);
  }
}

void BluetoothApp::onKey(const KeyEvent& ev) {
  if (scene_ == SCENE_MENU) {
    if (ev.key == BTN_UP) {
      menuSel_ = (menuSel_ - 1 + kMenuCount) % kMenuCount;
    } else if (ev.key == BTN_DOWN) {
      menuSel_ = (menuSel_ + 1) % kMenuCount;
    } else if (ev.key == BTN_OK && !ev.repeat) {
      if (menuSel_ == 0) {
        scene_ = SCENE_BLE_SCAN;
        scene_ble_scan::open();
      } else if (menuSel_ == 1) {
        scene_ = SCENE_MEDIA_CONTROL;
        scene_media_control::open();
      } else if (menuSel_ == 2) {
        scene_ = SCENE_IBEACON;
        scene_ibeacon::open();
      } else if (menuSel_ == 3) {
        scene_ = SCENE_BLE_SPAM;
        scene_ble_spam::open();
      } else if (menuSel_ == 4) {
        scene_ = SCENE_BADBLE;
        scene_badble::open();
      }
    }
  } else if (scene_ == SCENE_BLE_SCAN) {
    scene_ble_scan::onKey(ev);
  } else if (scene_ == SCENE_MEDIA_CONTROL) {
    scene_media_control::onKey(ev);
  } else if (scene_ == SCENE_IBEACON) {
    scene_ibeacon::onKey(ev);
  } else if (scene_ == SCENE_BLE_SPAM) {
    scene_ble_spam::onKey(ev);
  } else if (scene_ == SCENE_BADBLE) {
    scene_badble::onKey(ev);
  }
}

void BluetoothApp::draw(U8G2& u8g2, uint32_t now) {
  if (scene_ == SCENE_MENU) {
    u8g2.setClipWindow(0, 14, 128, 64);
    u8g2.setFont(u8g2_font_6x12_tr);
    for (int i = 0; i < kMenuCount; i++) {
      int y = kStartY + (int)(i * kRowH - scroll_ + 0.5f);
      if (y < kStartY - kRowH || y > 64) continue;
      u8g2.drawStr(6, y + 10, kMenuItems[i]);
    }


    u8g2.setDrawColor(2);
    int curY = (int)(barY_ + 0.5f);
    if (curY < kStartY) curY = kStartY;
    if (curY > 64 - kRowH) curY = 64 - kRowH;
    u8g2.drawBox(2, curY, 124, kRowH);
    u8g2.setDrawColor(1);
    u8g2.setMaxClipWindow();


    u8g2.setDrawColor(1);
    u8g2.drawBox(0, 0, 128, 14);
    u8g2.setDrawColor(0);
    u8g2.setFont(u8g2_font_7x14B_tr);
    u8g2.drawStr(4, 11, "BLUETOOTH");
    u8g2.setDrawColor(1);
  } else if (scene_ == SCENE_BLE_SCAN) {
    scene_ble_scan::draw(u8g2, now);
  } else if (scene_ == SCENE_MEDIA_CONTROL) {
    scene_media_control::draw(u8g2, now);
  } else if (scene_ == SCENE_IBEACON) {
    scene_ibeacon::draw(u8g2, now);
  } else if (scene_ == SCENE_BLE_SPAM) {
    scene_ble_spam::draw(u8g2, now);
  } else if (scene_ == SCENE_BADBLE) {
    scene_badble::draw(u8g2, now);
  }
}
