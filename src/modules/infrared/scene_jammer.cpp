#include "scene_jammer.h"
#include "ir_sender_hw.h"

namespace scene_jammer {

static bool s_active = false;

void open() {
  s_active = false;
}

void close() {
  if (s_active) {
    s_active = false;
    ir_sender_hw::stopJammer();
  }
}

void tick(uint32_t now) {
  (void)now;
  if (s_active) {
    ir_sender_hw::tickJammer();
  }
}

void onKey(const KeyEvent& ev) {
  if (ev.key == BTN_OK && !ev.repeat) {
    s_active = !s_active;
    if (s_active) ir_sender_hw::startJammer();
    else ir_sender_hw::stopJammer();
  }
}

void draw(U8G2& u8g2, uint32_t now) {

  u8g2.setDrawColor(1);
  u8g2.drawBox(0, 0, 128, 14);
  u8g2.setDrawColor(0);
  u8g2.setFont(u8g2_font_7x14B_tr);
  u8g2.drawStr(4, 11, "IR JAMMER");

  u8g2.setDrawColor(1);


  u8g2.setFont(u8g2_font_6x12_tr);
  const char* st = s_active ? "JAMMING ACTIVE (38kHz)" : "JAMMER STANDBY";
  int sw = u8g2.getStrWidth(st);
  u8g2.drawStr((128 - sw) / 2, 28, st);


  if (s_active) {
    for (int x = 4; x < 124; x += 3) {
      int h = 4 + ((now / 15 + x * 7) % 24);
      u8g2.drawVLine(x, 60 - h, h);
    }
  } else {

    u8g2.drawHLine(10, 48, 108);
  }
}

}
