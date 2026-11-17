#include "charging_view.h"
#include "charging_icon.h"
#include "core/display.h"
#include "core/battery.h"
#include "modules/settings/settings_data.h"
#include <math.h>

namespace charging_view {

static bool s_active = false;
static bool s_dismissed = false;
static bool s_lastChrg = false;
static uint32_t s_startTime = 0;

// 4x4 Bayer matrix for smooth ordered dithering (values 0..15)
static const uint8_t kBayer4[4][4] = {
  {  0,  8,  2, 10 },
  { 12,  4, 14,  6 },
  {  3, 11,  1,  9 },
  { 15,  7, 13,  5 }
};

static void restoreNormalContrast() {
  uint8_t normalBrightness = settings_data::getBrightness();
  uint8_t normalContrast = (normalBrightness * 255) / 100;
  display::setContrast(normalContrast);
}

void init() {
  s_active = false;
  s_dismissed = false;
  s_lastChrg = false;
  s_startTime = 0;
}

void start(uint32_t now) {
  s_active = true;
  s_dismissed = false;
  s_startTime = (now > 0) ? now : millis();
}

void stop() {
  if (s_active) {
    s_active = false;
    restoreNormalContrast();
  }
}

void dismiss() {
  s_dismissed = true;
  stop();
}

bool isActive() {
  return s_active && !s_dismissed;
}

bool isDismissed() {
  return s_dismissed;
}

bool isCharging() {
  return battery::isCharging();
}

bool tick(uint32_t now) {
  bool chrg = isCharging();

  if (chrg && !s_lastChrg) {
    // Charging cable plugged in: start 2-second animation
    start(now);
  } else if (!chrg && s_lastChrg) {
    // Charging cable unplugged: stop immediately
    stop();
  }
  s_lastChrg = chrg;

  if (s_active && !s_dismissed) {
    if ((now - s_startTime) >= 2000) {
      // 2 seconds elapsed: auto dismiss and return to normal screen
      stop();
      return false;
    }
    return true; // Request continuous redraws during the 2s animation
  }
  return false;
}

void draw(U8G2& u8g2, uint32_t now) {
  if (!s_active || s_dismissed) return;

  uint32_t elapsed = now - s_startTime;
  if (elapsed >= 2000) {
    stop();
    return;
  }

  // 2-second animation curve:
  // 0..400 ms: Smooth cosine fade-in
  // 400..1400 ms: Hold peak brightness (1000 ms)
  // 1400..2000 ms: Smooth cosine fade-out (600 ms)
  float t;
  if (elapsed < 400) {
    t = 0.5f * (1.0f - cosf(((float)elapsed / 400.0f) * (float)M_PI));
  } else if (elapsed < 1400) {
    t = 1.0f;
  } else {
    t = 0.5f * (1.0f + cosf(((float)(elapsed - 1400) / 600.0f) * (float)M_PI));
  }

  // Hardware contrast modulation
  uint8_t userMaxContrast = (settings_data::getBrightness() * 255) / 100;
  if (userMaxContrast < 30) userMaxContrast = 30;
  uint8_t currentContrast = (uint8_t)(t * userMaxContrast);
  display::setContrast(currentContrast);

  if (t < 0.02f) {
    return;
  }

  // Flipper Zero official lightning bolt (7x11 bitmap)
  // Scaled 2x = 14x22 pixels, centered on 128x64 display
  const int scale = 2;
  const int w = 7;
  const int h = 11;
  const int sw = w * scale; // 14
  const int sh = h * scale; // 22
  const int ox = (128 - sw) / 2; // = 57
  const int oy = (64 - sh) / 2;  // = 21

  uint8_t ditherLevel = (uint8_t)(t * 16.99f);

  u8g2.setDrawColor(1);
  for (int y = 0; y < h; y++) {
    uint8_t rowByte = pgm_read_byte(&icon_Charging_flipper_7x11_bits[y]);
    for (int x = 0; x < w; x++) {
      if ((rowByte >> x) & 1) {
        for (int dy = 0; dy < scale; dy++) {
          for (int dx = 0; dx < scale; dx++) {
            int px = ox + x * scale + dx;
            int py = oy + y * scale + dy;
            if (ditherLevel > kBayer4[py % 4][px % 4]) {
              u8g2.drawPixel(px, py);
            }
          }
        }
      }
    }
  }
}

} // namespace charging_view
