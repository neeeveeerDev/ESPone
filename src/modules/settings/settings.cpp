#include "settings.h"
#include "settings_data.h"
#include "settings_desktop.h"
#include "settings_conn.h"
#include "settings_datetime.h"
#include "settings_power.h"
#include "settings_system.h"
#include <math.h>

namespace {
static const char* kMenuItems[] = {
  "Display",
  "Connection",
  "DateTime",
  "Power",
  "System"
};
static const int kMenuCount = sizeof(kMenuItems) / sizeof(kMenuItems[0]);
static const int itemH = 12;
static const int startY = 16;

static void updateScroll(int sel, int count, float& scroll, float& targetScroll, float& barY) {
  const int maxLines = 4;
  int maxScroll = count - maxLines;
  if (maxScroll < 0) maxScroll = 0;

  int topIndex = (int)(targetScroll / itemH + 0.5f);
  if (sel < topIndex) {
    targetScroll = (float)(sel * itemH);
  } else if (sel >= topIndex + maxLines) {
    targetScroll = (float)((sel - maxLines + 1) * itemH);
  }

  if (targetScroll < 0.0f) targetScroll = 0.0f;
  if (targetScroll > (float)(maxScroll * itemH)) targetScroll = (float)(maxScroll * itemH);

  float ds = targetScroll - scroll;
  if (fabsf(ds) > 0.1f) {
    scroll += ds * 0.35f;
  } else {
    scroll = targetScroll;
  }

  float targetBarY = (float)(startY + sel * itemH) - scroll;
  float dy = targetBarY - barY;
  if (fabsf(dy) > 0.1f) {
    barY += dy * 0.4f;
  } else {
    barY = targetBarY;
  }
}
}

void SettingsApp::open() {
  settings_data::init();
  scene_ = SCENE_MENU;
  menuSel_ = 0;
  barY_ = (float)startY;
  scroll_ = 0.0f;
  targetScroll_ = 0.0f;
  Serial.println("[settings] open");
}

void SettingsApp::close() {
  if (scene_ == SCENE_DISPLAY) settings_desktop::close();
  else if (scene_ == SCENE_CONNECTIVITY) settings_conn::close();
  else if (scene_ == SCENE_DATETIME) settings_datetime::close();
  else if (scene_ == SCENE_POWER) settings_power::close();
  else if (scene_ == SCENE_SYSTEM) settings_system::close();
  scene_ = SCENE_MENU;
}

bool SettingsApp::onBack() {
  if (scene_ == SCENE_DISPLAY) {
    if (settings_desktop::onBack()) return true;
  } else if (scene_ == SCENE_CONNECTIVITY) {
    if (settings_conn::onBack()) return true;
  } else if (scene_ == SCENE_DATETIME) {
    if (settings_datetime::onBack()) return true;
  } else if (scene_ == SCENE_POWER) {
    if (settings_power::onBack()) return true;
  } else if (scene_ == SCENE_SYSTEM) {
    if (settings_system::onBack()) return true;
  }

  if (scene_ != SCENE_MENU) {
    close();
    scene_ = SCENE_MENU;
    return true;
  }
  return false;
}

void SettingsApp::tick(uint32_t now) {
  if (scene_ == SCENE_MENU) {
    updateScroll(menuSel_, kMenuCount, scroll_, targetScroll_, barY_);
  } else if (scene_ == SCENE_DISPLAY) {
    settings_desktop::tick(now);
  } else if (scene_ == SCENE_CONNECTIVITY) {
    settings_conn::tick(now);
  } else if (scene_ == SCENE_DATETIME) {
    settings_datetime::tick(now);
  } else if (scene_ == SCENE_POWER) {
    settings_power::tick(now);
  } else if (scene_ == SCENE_SYSTEM) {
    settings_system::tick(now);
  }
}

void SettingsApp::onKey(const KeyEvent& ev) {
  if (scene_ == SCENE_MENU) {
    if (ev.key == BTN_UP) {
      menuSel_ = (menuSel_ - 1 + kMenuCount) % kMenuCount;
    } else if (ev.key == BTN_DOWN) {
      menuSel_ = (menuSel_ + 1) % kMenuCount;
    } else if (ev.key == BTN_OK && !ev.repeat) {
      if (menuSel_ == 0) {
        scene_ = SCENE_DISPLAY;
        settings_desktop::open();
      } else if (menuSel_ == 1) {
        scene_ = SCENE_CONNECTIVITY;
        settings_conn::open();
      } else if (menuSel_ == 2) {
        scene_ = SCENE_DATETIME;
        settings_datetime::open();
      } else if (menuSel_ == 3) {
        scene_ = SCENE_POWER;
        settings_power::open();
      } else if (menuSel_ == 4) {
        scene_ = SCENE_SYSTEM;
        settings_system::open();
      }
    }
  } else if (scene_ == SCENE_DISPLAY) {
    settings_desktop::onKey(ev);
  } else if (scene_ == SCENE_CONNECTIVITY) {
    settings_conn::onKey(ev);
  } else if (scene_ == SCENE_DATETIME) {
    settings_datetime::onKey(ev);
  } else if (scene_ == SCENE_POWER) {
    settings_power::onKey(ev);
  } else if (scene_ == SCENE_SYSTEM) {
    settings_system::onKey(ev);
  }
}

void SettingsApp::draw(U8G2& u8g2, uint32_t now) {
  if (scene_ == SCENE_MENU) {
    u8g2.setClipWindow(0, 14, 128, 64);
    u8g2.setFont(u8g2_font_6x12_tr);
    u8g2.setDrawColor(1);
    for (int i = 0; i < kMenuCount; i++) {
      int y = startY + (int)(i * itemH - scroll_ + 0.5f);
      if (y < startY - itemH || y > 64) continue;
      u8g2.drawStr(6, y + 10, kMenuItems[i]);
    }

    // Selection bar
    int curY = (int)(barY_ + 0.5f);
    if (curY >= startY && curY <= 64 - itemH) {
      u8g2.setDrawColor(2);
      u8g2.drawBox(2, curY, 124, itemH);
      u8g2.setDrawColor(1);
    }
    u8g2.setMaxClipWindow();

    // Header
    u8g2.setDrawColor(1);
    u8g2.drawBox(0, 0, 128, 14);
    u8g2.setDrawColor(0);
    u8g2.setFont(u8g2_font_7x14B_tr);
    u8g2.drawStr(4, 11, "SETTINGS");
    u8g2.setDrawColor(1);
  } else if (scene_ == SCENE_DISPLAY) {
    settings_desktop::draw(u8g2, now);
  } else if (scene_ == SCENE_CONNECTIVITY) {
    settings_conn::draw(u8g2, now);
  } else if (scene_ == SCENE_DATETIME) {
    settings_datetime::draw(u8g2, now);
  } else if (scene_ == SCENE_POWER) {
    settings_power::draw(u8g2, now);
  } else if (scene_ == SCENE_SYSTEM) {
    settings_system::draw(u8g2, now);
  }
}
