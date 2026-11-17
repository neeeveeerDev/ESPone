#include "settings_power.h"
#include "settings_data.h"
#include "core/battery.h"
#include <math.h>

namespace settings_power {

enum ConfirmAction : uint8_t {
  CONFIRM_NONE = 0,
  CONFIRM_REBOOT,
  CONFIRM_POWEROFF
};

enum SubScene : uint8_t {
  SUB_MAIN = 0,
  SUB_BATTERY
};

static SubScene s_subScene = SUB_MAIN;
static const int kCount = 4;
static const int rowH = 12;
static const int startY = 16;

static int s_sel = 0;
static float s_barY = 16.0f;
static ConfirmAction s_confirm = CONFIRM_NONE;
static int s_confirmSel = 0; // 0 = Cancel, 1 = Confirm

static const uint16_t kSleepVals[] = { 0, 60, 120, 300, 600 };
static const char* kSleepNames[] = { "OFF", "1 min", "2 min", "5 min", "10 min" };
static const int kSleepCount = sizeof(kSleepVals) / sizeof(kSleepVals[0]);

static int findSleepIdx(uint16_t sec) {
  for (int i = 0; i < kSleepCount; i++) {
    if (kSleepVals[i] == sec) return i;
  }
  return 0;
}

void open() {
  s_subScene = SUB_MAIN;
  s_sel = 0;
  s_barY = (float)startY;
  s_confirm = CONFIRM_NONE;
  s_confirmSel = 0;
}

void close() {
  settings_data::save();
  s_confirm = CONFIRM_NONE;
  s_subScene = SUB_MAIN;
}

bool onBack() {
  if (s_confirm != CONFIRM_NONE) {
    s_confirm = CONFIRM_NONE;
    return true;
  }
  if (s_subScene == SUB_BATTERY) {
    s_subScene = SUB_MAIN;
    return true;
  }
  return false;
}

void tick(uint32_t now) {
  (void)now;
  if (s_subScene == SUB_MAIN && s_confirm == CONFIRM_NONE) {
    float target = (float)(startY + s_sel * rowH);
    float dy = target - s_barY;
    if (fabsf(dy) > 0.1f) {
      s_barY += dy * 0.35f;
    } else {
      s_barY = target;
    }
  }
}

void onKey(const KeyEvent& ev) {
  if (s_confirm != CONFIRM_NONE) {
    if (ev.key == BTN_LEFT || ev.key == BTN_UP) {
      s_confirmSel = 0;
    } else if (ev.key == BTN_RIGHT || ev.key == BTN_DOWN) {
      s_confirmSel = 1;
    } else if (ev.key == BTN_BACK) {
      s_confirm = CONFIRM_NONE;
    } else if (ev.key == BTN_OK && !ev.repeat) {
      if (s_confirmSel == 1) {
        if (s_confirm == CONFIRM_REBOOT) {
          settings_data::reboot();
        } else if (s_confirm == CONFIRM_POWEROFF) {
          settings_data::powerOff();
        }
      }
      s_confirm = CONFIRM_NONE;
    }
    return;
  }

  if (s_subScene == SUB_BATTERY) {
    if (ev.key == BTN_BACK || ev.key == BTN_OK || ev.key == BTN_LEFT) {
      s_subScene = SUB_MAIN;
    }
    return;
  }

  // SUB_MAIN
  if (ev.key == BTN_UP) {
    s_sel = (s_sel - 1 + kCount) % kCount;
  } else if (ev.key == BTN_DOWN) {
    s_sel = (s_sel + 1) % kCount;
  } else if (ev.key == BTN_LEFT) {
    if (s_sel == 1) {
      int idx = findSleepIdx(settings_data::getSleepSec());
      if (idx > 0) settings_data::setSleepSec(kSleepVals[idx - 1]);
    }
  } else if (ev.key == BTN_RIGHT) {
    if (s_sel == 0) {
      s_subScene = SUB_BATTERY;
    } else if (s_sel == 1) {
      int idx = findSleepIdx(settings_data::getSleepSec());
      if (idx < kSleepCount - 1) settings_data::setSleepSec(kSleepVals[idx + 1]);
    }
  } else if (ev.key == BTN_OK && !ev.repeat) {
    if (s_sel == 0) {
      s_subScene = SUB_BATTERY;
    } else if (s_sel == 1) {
      int idx = (findSleepIdx(settings_data::getSleepSec()) + 1) % kSleepCount;
      settings_data::setSleepSec(kSleepVals[idx]);
    } else if (s_sel == 2) {
      s_confirm = CONFIRM_REBOOT;
      s_confirmSel = 0;
    } else if (s_sel == 3) {
      s_confirm = CONFIRM_POWEROFF;
      s_confirmSel = 0;
    }
  }
}

static void drawBatterySubscene(U8G2& u8g2, uint32_t now) {
  // Header
  u8g2.setDrawColor(1);
  u8g2.drawBox(0, 0, 128, 14);
  u8g2.setDrawColor(0);
  u8g2.setFont(u8g2_font_7x14B_tr);
  u8g2.drawStr(4, 11, "BATTERY INFO");
  u8g2.setDrawColor(1);

  uint16_t mv = battery::getMillivolts();
  uint8_t pct = battery::getPercentage();
  bool chg = battery::isCharging();
  bool usb = battery::isUsbPowered();
  bool low = battery::isLow();

  char buf[36];

  // Graphical Battery Icon (Large: 28x12 at x=6, y=17)
  const int bx = 6, by = 17, bw = 28, bh = 12;
  u8g2.drawFrame(bx, by, bw, bh);
  u8g2.drawBox(bx + bw, by + 3, 2, bh - 6); // tip

  int fillW = (int)((pct * (bw - 4)) / 100);
  if (fillW > bw - 4) fillW = bw - 4;
  if (fillW > 0) {
    u8g2.drawBox(bx + 2, by + 2, fillW, bh - 4);
  }

  // Large Percentage text next to battery icon
  u8g2.setFont(u8g2_font_7x14B_tr);
  snprintf(buf, sizeof(buf), "%u%%", pct);
  u8g2.drawStr(40, by + 11, buf);

  // If charging or on USB, display status badge
  u8g2.setFont(u8g2_font_6x10_tr);
  if (chg) {
    u8g2.drawStr(78, by + 10, "[CHRG]");
  } else if (usb) {
    u8g2.drawStr(78, by + 10, "[USB]");
  }

  // Divider line
  u8g2.drawHLine(4, 32, 120);

  // Details
  u8g2.setFont(u8g2_font_6x10_tr);

  // Line 1: Voltage
  float v = mv / 1000.0f;
  snprintf(buf, sizeof(buf), "Voltage: %.2fV (%umV)", v, mv);
  u8g2.drawStr(6, 42, buf);

  // Line 2: State
  const char* st = chg ? "Charging" : (usb ? "USB Powered" : (low ? "Low Battery" : "Discharging"));
  snprintf(buf, sizeof(buf), "State:   %s", st);
  u8g2.drawStr(6, 52, buf);

  // Line 3: Uptime
  uint32_t sec = now / 1000;
  uint32_t m = (sec / 60) % 60;
  uint32_t h = (sec / 3600);
  snprintf(buf, sizeof(buf), "Uptime:  %02lu:%02lu:%02lu", h, m, sec % 60);
  u8g2.drawStr(6, 62, buf);
}

void draw(U8G2& u8g2, uint32_t now) {
  if (s_subScene == SUB_BATTERY) {
    drawBatterySubscene(u8g2, now);
    return;
  }

  u8g2.setFont(u8g2_font_6x12_tr);
  u8g2.setDrawColor(1);

  char buf[36];

  // 0: Battery Info entry
  uint8_t pct = battery::getPercentage();
  uint16_t mv = battery::getMillivolts();
  float v = mv / 1000.0f;
  snprintf(buf, sizeof(buf), "Battery: %u%% (%.2fV) >", pct, v);
  u8g2.drawStr(6, startY + 10, buf);

  // 1: Auto Sleep
  int slIdx = findSleepIdx(settings_data::getSleepSec());
  snprintf(buf, sizeof(buf), "Auto-Sleep: %s", kSleepNames[slIdx]);
  u8g2.drawStr(6, startY + 10 + rowH, buf);

  // 2: Reboot Device
  u8g2.drawStr(6, startY + 10 + 2 * rowH, "Reboot");

  // 3: Power Off Now
  u8g2.drawStr(6, startY + 10 + 3 * rowH, "Power Off");

  // Selection bar
  if (s_confirm == CONFIRM_NONE) {
    u8g2.setDrawColor(2);
    int curY = (int)(s_barY + 0.5f);
    if (curY >= startY && curY <= 64 - rowH) {
      u8g2.drawBox(2, curY, 124, rowH);
    }
    u8g2.setDrawColor(1);
  }

  // Header
  u8g2.setDrawColor(1);
  u8g2.drawBox(0, 0, 128, 14);
  u8g2.setDrawColor(0);
  u8g2.setFont(u8g2_font_7x14B_tr);
  u8g2.drawStr(4, 11, "POWER");
  u8g2.setDrawColor(1);

  // Modal Confirmation Dialog
  if (s_confirm != CONFIRM_NONE) {
    u8g2.setDrawColor(0);
    u8g2.drawBox(10, 16, 108, 44);
    u8g2.setDrawColor(1);
    u8g2.drawFrame(10, 16, 108, 44);
    u8g2.drawFrame(12, 18, 104, 40);

    u8g2.setFont(u8g2_font_6x12_tr);
    const char* q = (s_confirm == CONFIRM_REBOOT) ? "Reboot Device?" : "Power Off Now?";
    int qw = u8g2.getStrWidth(q);
    u8g2.drawStr((128 - qw) / 2, 30, q);

    // Cancel Button
    if (s_confirmSel == 0) {
      u8g2.drawBox(18, 40, 42, 14);
      u8g2.setDrawColor(0);
      u8g2.drawStr(22, 51, "Cancel");
      u8g2.setDrawColor(1);
    } else {
      u8g2.drawFrame(18, 40, 42, 14);
      u8g2.drawStr(22, 51, "Cancel");
    }

    // Confirm Button
    const char* actionLabel = (s_confirm == CONFIRM_REBOOT) ? "Reboot" : "Off";
    if (s_confirmSel == 1) {
      u8g2.drawBox(68, 40, 42, 14);
      u8g2.setDrawColor(0);
      int lw = u8g2.getStrWidth(actionLabel);
      u8g2.drawStr(68 + (42 - lw) / 2, 51, actionLabel);
      u8g2.setDrawColor(1);
    } else {
      u8g2.drawFrame(68, 40, 42, 14);
      int lw = u8g2.getStrWidth(actionLabel);
      u8g2.drawStr(68 + (42 - lw) / 2, 51, actionLabel);
    }
  }
}

}
