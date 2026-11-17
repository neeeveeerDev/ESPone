#include "splash.h"
#include "config.h"

namespace splash {

static uint32_t t0 = 0;

void start(uint32_t now) { t0 = now; }
bool finished(uint32_t now) { return now - t0 >= SPLASH_MS; }

void draw(U8G2& u8g2, uint32_t now) {
  u8g2.setFont(u8g2_font_10x20_tr);
  const char* t = "ESPone";
  u8g2.drawStr((128 - u8g2.getStrWidth(t)) / 2, 38, t);

  float p = (now - t0) / (float)SPLASH_MS;
  if (p > 1) p = 1;
  u8g2.drawFrame(34, 46, 60, 6);
  u8g2.drawBox(36, 48, (int)(56 * p), 2);
}

}
