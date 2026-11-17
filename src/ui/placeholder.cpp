#include "placeholder.h"

namespace placeholder {

void draw(U8G2& u8g2, const char* title, uint32_t now) {
  (void)now;

  u8g2.setFont(u8g2_font_7x14B_tr);
  u8g2.drawBox(0, 0, 128, 14);
  u8g2.setDrawColor(0);
  u8g2.drawStr(4, 11, title);
  u8g2.setDrawColor(1);

  u8g2.setFont(u8g2_font_6x12_tr);
  const char* m = "Coming soon";
  u8g2.drawStr((128 - u8g2.getStrWidth(m)) / 2, 38, m);
}

}
