#include "screensaver.h"
#include "core/sys_clock.h"

namespace screensaver {

static bool s_active = false;
static uint8_t s_mode = 0;
static int s_lastSec = -1;

void start(uint8_t mode) {
  s_active = true;
  s_mode = mode;
  s_lastSec = -1;
}

void stop() {
  s_active = false;
}

bool isActive() {
  return s_active;
}

uint8_t getMode() {
  return s_mode;
}

bool tick(uint32_t now) {
  (void)now;
  if (!s_active) return false;

  if (s_mode == 1) {
    int yr, mo, d, h, m, s;
    sys_clock::getTime(yr, mo, d, h, m, s);
    if (s != s_lastSec) {
      s_lastSec = s;
      return true;
    }
  }
  return false;
}

void draw(U8G2& u8g2, uint32_t now) {
  (void)now;
  if (!s_active) return;

  u8g2.setDrawColor(1);

  if (s_mode == 1) {



    int yr, mo, d, h, m, s;
    sys_clock::getTime(yr, mo, d, h, m, s);

    char timeStr[16];
    snprintf(timeStr, sizeof(timeStr), "%02d:%02d:%02d", h, m, s);

    u8g2.setFont(u8g2_font_10x20_tr);
    int w = u8g2.getStrWidth(timeStr);
    u8g2.drawStr((128 - w) / 2, 40, timeStr);

  } else if (s_mode == 2) {



    u8g2.setFont(u8g2_font_10x20_tr);
    const char* logo = "ESPone";
    int w = u8g2.getStrWidth(logo);
    u8g2.drawStr((128 - w) / 2, 40, logo);
  }
}

}
