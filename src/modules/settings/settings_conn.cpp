#include "settings_conn.h"
#include "settings_data.h"
#include "ui/keyboard.h"
#include "core/ble_service.h"
#include <math.h>

namespace settings_conn {

static const int kCount = 4;
static const int rowH = 12;
static const int startY = 16;

static int s_sel = 0;
static float s_barY = 16.0f;
static float s_scroll = 0.0f;
static float s_targetScroll = 0.0f;
static uint32_t s_msgUntil = 0;
static const char* s_msgText = "";

void open() {
  s_sel = 0;
  s_barY = (float)startY;
  s_scroll = 0.0f;
  s_targetScroll = 0.0f;
  s_msgUntil = 0;
}

void close() {
  settings_data::save();
}

bool onBack() {
  return false;
}

void tick(uint32_t now) {
  (void)now;
  const int maxLines = 4;
  int maxScroll = kCount - maxLines;
  if (maxScroll < 0) maxScroll = 0;

  int topIndex = (int)(s_targetScroll / rowH + 0.5f);
  if (s_sel < topIndex) {
    s_targetScroll = (float)(s_sel * rowH);
  } else if (s_sel >= topIndex + maxLines) {
    s_targetScroll = (float)((s_sel - maxLines + 1) * rowH);
  }

  if (s_targetScroll < 0.0f) s_targetScroll = 0.0f;
  if (s_targetScroll > (float)(maxScroll * rowH)) s_targetScroll = (float)(maxScroll * rowH);

  float ds = s_targetScroll - s_scroll;
  if (fabsf(ds) > 0.1f) s_scroll += ds * 0.35f;
  else s_scroll = s_targetScroll;

  float targetBar = (float)(startY + s_sel * rowH) - s_scroll;
  float dy = targetBar - s_barY;
  if (fabsf(dy) > 0.1f) s_barY += dy * 0.4f;
  else s_barY = targetBar;
}

void onKey(const KeyEvent& ev) {
  if (ev.key == BTN_UP) {
    s_sel = (s_sel - 1 + kCount) % kCount;
  } else if (ev.key == BTN_DOWN) {
    s_sel = (s_sel + 1) % kCount;
  } else if (ev.key == BTN_LEFT || ev.key == BTN_RIGHT) {
    if (s_sel == 0) {
      settings_data::setBleEnabled(!settings_data::getBleEnabled());
    } else if (s_sel == 3) {
      settings_data::setUsbEnabled(!settings_data::getUsbEnabled());
    }
  } else if (ev.key == BTN_OK && !ev.repeat) {
    if (s_sel == 0) {
      settings_data::setBleEnabled(!settings_data::getBleEnabled());
    } else if (s_sel == 1) {
      ui_keyboard::open("BLE NAME", settings_data::getBleName().c_str(), [](const String& text, bool ok) {
        if (ok && text.length() > 0) {
          settings_data::setBleName(text);
          ble_service::setDeviceName(text.c_str());
        }
      });
    } else if (s_sel == 2) {
      settings_data::resetBlePin();
      s_msgText = "PIN NEW";
      s_msgUntil = millis() + 1500;
    } else if (s_sel == 3) {
      settings_data::setUsbEnabled(!settings_data::getUsbEnabled());
    }
  }
}

void draw(U8G2& u8g2, uint32_t now) {
  u8g2.setClipWindow(0, 14, 128, 64);
  u8g2.setFont(u8g2_font_6x12_tr);
  u8g2.setDrawColor(1);

  char buf[36];

  // 0: Bluetooth
  snprintf(buf, sizeof(buf), "Bluetooth: %s", settings_data::getBleEnabled() ? "ON" : "OFF");
  int y0 = startY + (int)(0 * rowH - s_scroll + 0.5f);
  if (y0 >= startY - rowH && y0 <= 64) u8g2.drawStr(6, y0 + 10, buf);

  // 1: Device Name
  snprintf(buf, sizeof(buf), "Name: %s", settings_data::getBleName().c_str());
  int y1 = startY + (int)(1 * rowH - s_scroll + 0.5f);
  if (y1 >= startY - rowH && y1 <= 64) u8g2.drawStr(6, y1 + 10, buf);

  // 2: BLE PIN
  snprintf(buf, sizeof(buf), "PIN: %s", ble_service::getPin());
  int y2 = startY + (int)(2 * rowH - s_scroll + 0.5f);
  if (y2 >= startY - rowH && y2 <= 64) u8g2.drawStr(6, y2 + 10, buf);

  // 3: USB Remote
  snprintf(buf, sizeof(buf), "USB Remote: %s", settings_data::getUsbEnabled() ? "ON" : "OFF");
  int y3 = startY + (int)(3 * rowH - s_scroll + 0.5f);
  if (y3 >= startY - rowH && y3 <= 64) u8g2.drawStr(6, y3 + 10, buf);

  // Smooth XOR selection bar
  int curY = (int)(s_barY + 0.5f);
  if (curY >= startY && curY <= 64 - rowH) {
    u8g2.setDrawColor(2);
    u8g2.drawBox(2, curY, 124, rowH);
    u8g2.setDrawColor(1);
  }
  u8g2.setMaxClipWindow();

  // Header
  u8g2.setDrawColor(1);
  u8g2.drawBox(0, 0, 128, 14);
  u8g2.setDrawColor(0);
  u8g2.setFont(u8g2_font_7x14B_tr);
  u8g2.drawStr(4, 11, "CONNECTION");
  if (now < s_msgUntil) {
    u8g2.setFont(u8g2_font_5x8_tr);
    u8g2.drawStr(80, 10, s_msgText);
  }
  u8g2.setDrawColor(1);
}

}
