#include "settings_desktop.h"
#include "settings_data.h"
#include "modules/modules.h"
#include <math.h>

namespace settings_desktop {

enum SubMode : uint8_t {
  MODE_DESKTOP_MENU = 0,
  MODE_MENU_LAYOUT
};

static SubMode s_subMode = MODE_DESKTOP_MENU;

static const int kDesktopCount = 5;
static const int kRowH = 12;
static const int kStartY = 16;

static int   s_sel = 0;
static float s_barY = 16.0f;
static float s_scroll = 0.0f;
static float s_targetScroll = 0.0f;

static const char* kSaverNames[] = { "OFF", "Clock", "Logo" };

static const uint16_t kScreenOffVals[] = { 0, 15, 30, 60, 120 };
static const char* kScreenOffNames[] = { "Never", "15s", "30s", "60s", "120s" };
static const int kScreenOffCount = sizeof(kScreenOffVals) / sizeof(kScreenOffVals[0]);

static int findScreenOffIdx(uint16_t sec) {
  for (int i = 0; i < kScreenOffCount; i++) {
    if (kScreenOffVals[i] == sec) return i;
  }
  return 2;
}

static const int kAppCount = 8;
static uint8_t s_order[8] = { 0, 1, 2, 3, 4, 5, 6, 7 };
static int   s_layoutSel = 0;
static float s_layoutBarY = 16.0f;
static float s_layoutScroll = 0.0f;
static float s_layoutTargetScroll = 0.0f;
static bool  s_moving = false;

static uint32_t s_okPressTime = 0;
static bool     s_okPressed = false;
static bool     s_holdTriggered = false;
static const uint32_t kHoldThresholdMs = 450;

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

void open() {
  s_subMode = MODE_DESKTOP_MENU;
  s_sel = 0;
  s_barY = (float)kStartY;
  s_scroll = 0.0f;
  s_targetScroll = 0.0f;
  s_moving = false;
  s_okPressed = false;
  s_holdTriggered = false;
  settings_data::getMenuOrder(s_order, kAppCount);
}

void close() {
  settings_data::save();
  modules::reloadMenu();
  s_subMode = MODE_DESKTOP_MENU;
}

bool onBack() {
  if (s_subMode == MODE_MENU_LAYOUT) {
    if (s_moving) {
      s_moving = false;
      return true;
    }
    settings_data::save();
    modules::reloadMenu();
    s_subMode = MODE_DESKTOP_MENU;
    return true;
  }
  return false;
}

void tick(uint32_t now) {
  if (s_subMode == MODE_DESKTOP_MENU) {
    updateScroll(s_sel, kDesktopCount, s_scroll, s_targetScroll, s_barY);
  } else if (s_subMode == MODE_MENU_LAYOUT) {
    if (s_okPressed && !s_holdTriggered && (now - s_okPressTime >= kHoldThresholdMs)) {
      s_holdTriggered = true;
      uint8_t appId = s_order[s_layoutSel];
      if (appId != 7) {
        settings_data::toggleAppHidden(appId);
        modules::reloadMenu();
      }
      s_moving = false;
    }
    updateScroll(s_layoutSel, kAppCount, s_layoutScroll, s_layoutTargetScroll, s_layoutBarY);
  }
}

void onKey(const KeyEvent& ev) {
  if (s_subMode == MODE_DESKTOP_MENU) {
    if (ev.key == BTN_UP) {
      s_sel = (s_sel - 1 + kDesktopCount) % kDesktopCount;
    } else if (ev.key == BTN_DOWN) {
      s_sel = (s_sel + 1) % kDesktopCount;
    } else if (ev.key == BTN_LEFT) {
      if (s_sel == 0) {
        uint8_t b = settings_data::getBrightness();
        if (b > 10) settings_data::setBrightness(b - 10);
      } else if (s_sel == 1) {
        settings_data::setInvert(!settings_data::getInvert());
      } else if (s_sel == 2) {
        int idx = findScreenOffIdx(settings_data::getScreenOffSec());
        if (idx > 0) settings_data::setScreenOffSec(kScreenOffVals[idx - 1]);
      } else if (s_sel == 3) {
        uint8_t sc = settings_data::getScreensaver();
        if (sc > 0) settings_data::setScreensaver(sc - 1);
        else settings_data::setScreensaver(2);
      }
    } else if (ev.key == BTN_RIGHT) {
      if (s_sel == 0) {
        uint8_t b = settings_data::getBrightness();
        if (b < 100) settings_data::setBrightness(b + 10);
      } else if (s_sel == 1) {
        settings_data::setInvert(!settings_data::getInvert());
      } else if (s_sel == 2) {
        int idx = findScreenOffIdx(settings_data::getScreenOffSec());
        if (idx < kScreenOffCount - 1) settings_data::setScreenOffSec(kScreenOffVals[idx + 1]);
      } else if (s_sel == 3) {
        uint8_t sc = settings_data::getScreensaver();
        settings_data::setScreensaver((sc + 1) % 3);
      }
    } else if (ev.key == BTN_OK && !ev.repeat) {
      if (s_sel == 4) {
        settings_data::getMenuOrder(s_order, kAppCount);
        s_layoutSel = 0;
        s_layoutBarY = (float)kStartY;
        s_layoutScroll = 0.0f;
        s_layoutTargetScroll = 0.0f;
        s_moving = false;
        s_subMode = MODE_MENU_LAYOUT;
      } else if (s_sel == 1) {
        settings_data::setInvert(!settings_data::getInvert());
      } else if (s_sel == 2) {
        int idx = (findScreenOffIdx(settings_data::getScreenOffSec()) + 1) % kScreenOffCount;
        settings_data::setScreenOffSec(kScreenOffVals[idx]);
      } else if (s_sel == 3) {
        uint8_t sc = settings_data::getScreensaver();
        settings_data::setScreensaver((sc + 1) % 3);
      }
    }
  } else if (s_subMode == MODE_MENU_LAYOUT) {
    if (ev.key == BTN_OK) {
      if (!ev.repeat) {
        s_okPressed = true;
        s_okPressTime = millis();
        s_holdTriggered = false;
      } else {
        if (!s_holdTriggered) {
          s_holdTriggered = true;
          uint8_t appId = s_order[s_layoutSel];
          if (appId != 7) {
            settings_data::toggleAppHidden(appId);
            modules::reloadMenu();
          }
          s_moving = false;
        }
      }
      return;
    }

    if (ev.key == BTN_UP) {
      if (s_moving) {
        if (s_layoutSel > 0) {
          uint8_t tmp = s_order[s_layoutSel];
          s_order[s_layoutSel] = s_order[s_layoutSel - 1];
          s_order[s_layoutSel - 1] = tmp;
          s_layoutSel--;
          settings_data::setMenuOrder(s_order, kAppCount);
          modules::reloadMenu();
        }
      } else {
        s_layoutSel = (s_layoutSel - 1 + kAppCount) % kAppCount;
      }
    } else if (ev.key == BTN_DOWN) {
      if (s_moving) {
        if (s_layoutSel < kAppCount - 1) {
          uint8_t tmp = s_order[s_layoutSel];
          s_order[s_layoutSel] = s_order[s_layoutSel + 1];
          s_order[s_layoutSel + 1] = tmp;
          s_layoutSel++;
          settings_data::setMenuOrder(s_order, kAppCount);
          modules::reloadMenu();
        }
      } else {
        s_layoutSel = (s_layoutSel + 1) % kAppCount;
      }
    }
  }
}

static void handleOkRelease() {
  if (s_okPressed) {
    if (!s_holdTriggered) {
      s_moving = !s_moving;
    }
    s_okPressed = false;
    s_holdTriggered = false;
  }
}

void draw(U8G2& u8g2, uint32_t now) {
  if (s_okPressed && digitalRead(5) == HIGH) {
    handleOkRelease();
  }

  if (s_subMode == MODE_DESKTOP_MENU) {
    u8g2.setClipWindow(0, 14, 128, 64);
    u8g2.setFont(u8g2_font_6x12_tr);
    u8g2.setDrawColor(1);

    char buf[32];

    // 0: Brightness
    snprintf(buf, sizeof(buf), "Brightness  < %d%% >", settings_data::getBrightness());
    int y0 = kStartY + (int)(0 * kRowH - s_scroll + 0.5f);
    if (y0 >= kStartY - kRowH && y0 <= 64) u8g2.drawStr(6, y0 + 10, buf);

    // 1: Invert
    snprintf(buf, sizeof(buf), "Invert      < %s >", settings_data::getInvert() ? "ON" : "OFF");
    int y1 = kStartY + (int)(1 * kRowH - s_scroll + 0.5f);
    if (y1 >= kStartY - kRowH && y1 <= 64) u8g2.drawStr(6, y1 + 10, buf);

    // 2: Screen Off Timeout
    int soIdx = findScreenOffIdx(settings_data::getScreenOffSec());
    snprintf(buf, sizeof(buf), "Screen Off  < %s >", kScreenOffNames[soIdx]);
    int y2 = kStartY + (int)(2 * kRowH - s_scroll + 0.5f);
    if (y2 >= kStartY - kRowH && y2 <= 64) u8g2.drawStr(6, y2 + 10, buf);

    // 3: Screensaver
    snprintf(buf, sizeof(buf), "Screensaver < %s >", kSaverNames[settings_data::getScreensaver() % 3]);
    int y3 = kStartY + (int)(3 * kRowH - s_scroll + 0.5f);
    if (y3 >= kStartY - kRowH && y3 <= 64) u8g2.drawStr(6, y3 + 10, buf);

    // 4: Menu Layout Order
    int y4 = kStartY + (int)(4 * kRowH - s_scroll + 0.5f);
    if (y4 >= kStartY - kRowH && y4 <= 64) u8g2.drawStr(6, y4 + 10, "Menu Layout");

    // Smooth XOR Selection Box
    u8g2.setDrawColor(2);
    int curY = (int)(s_barY + 0.5f);
    if (curY >= kStartY && curY <= 64 - kRowH) {
      u8g2.drawBox(2, curY, 124, kRowH);
    }
    u8g2.setDrawColor(1);
    u8g2.setMaxClipWindow();

    // Header
    u8g2.setDrawColor(1);
    u8g2.drawBox(0, 0, 128, 14);
    u8g2.setDrawColor(0);
    u8g2.setFont(u8g2_font_7x14B_tr);
    u8g2.drawStr(4, 11, "DISPLAY");
    u8g2.setDrawColor(1);
    return;
  }

  if (s_subMode == MODE_MENU_LAYOUT) {
    u8g2.setClipWindow(0, 14, 128, 64);
    u8g2.setFont(u8g2_font_6x12_tr);
    for (int i = 0; i < kAppCount; i++) {
      int y = kStartY + (int)(i * kRowH - s_layoutScroll + 0.5f);
      if (y < kStartY - kRowH || y > 64) continue;

      uint8_t appId = s_order[i];
      const char* name = (appId < modules::kCount) ? modules::kApps[appId]->name() : "App";
      bool hidden = settings_data::isAppHidden(appId);

      if (hidden) {
        u8g2.setDrawColor(1);
        u8g2.drawBox(4, y, 120, kRowH - 1);
        u8g2.setDrawColor(0);
        u8g2.drawStr(8, y + 10, name);
        u8g2.drawStr(120 - u8g2.getStrWidth("HIDDEN"), y + 10, "HIDDEN");
        u8g2.setDrawColor(1);
      } else {
        u8g2.setDrawColor(1);
        u8g2.drawStr(8, y + 10, name);
      }
    }

    // Selection bar
    int curY = (int)(s_layoutBarY + 0.5f);
    if (curY >= kStartY && curY <= 64 - kRowH) {
      if (s_moving) {
        u8g2.setDrawColor(1);
        u8g2.drawFrame(2, curY, 124, kRowH);
      } else {
        u8g2.setDrawColor(2);
        u8g2.drawBox(2, curY, 124, kRowH);
        u8g2.setDrawColor(1);
      }
    }
    u8g2.setMaxClipWindow();

    // Header
    u8g2.setDrawColor(1);
    u8g2.drawBox(0, 0, 128, 14);
    u8g2.setDrawColor(0);
    u8g2.setFont(u8g2_font_7x14B_tr);
    const char* title = s_moving ? "MOVE ITEM" : "MAIN MENU";
    u8g2.drawStr(4, 11, title);
    u8g2.setDrawColor(1);
  }
}

}
