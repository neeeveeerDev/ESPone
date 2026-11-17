#include "scene_create.h"
#include <math.h>

namespace scene_create {

static const char* kItems[] = {
  "New Folder",
  "New File"
};
static const int kCount = 2;
static const int rowH = 14;
static const int startY = 22;

static int s_sel = 0;
static float s_barY = startY;

void open() {
  s_sel = 0;
  s_barY = startY;
}

void close() {}

void tick(uint32_t now) {
  float target = startY + s_sel * rowH;
  float dy = target - s_barY;
  if (fabsf(dy) > 0.1f) s_barY += dy * 0.35f;
  else s_barY = target;
}

CreateTarget onKey(const KeyEvent& ev) {
  if (ev.key == BTN_UP) {
    if (s_sel > 0) s_sel--;
    else s_sel = kCount - 1;
  } else if (ev.key == BTN_DOWN) {
    if (s_sel < kCount - 1) s_sel++;
    else s_sel = 0;
  } else if (ev.key == BTN_BACK) {
    return CREATE_CANCEL;
  } else if (ev.key == BTN_OK && !ev.repeat) {
    if (s_sel == 0) return CREATE_FOLDER;
    if (s_sel == 1) return CREATE_FILE;
  }
  return CREATE_NONE;
}

void draw(U8G2& u8g2, uint32_t now) {

  u8g2.setDrawColor(1);
  u8g2.drawBox(0, 0, 128, 14);
  u8g2.setDrawColor(0);
  u8g2.setFont(u8g2_font_7x14B_tr);
  u8g2.drawStr(4, 11, "CREATE");
  u8g2.setDrawColor(1);


  u8g2.setFont(u8g2_font_6x12_tr);
  for (int i = 0; i < kCount; i++) {
    int y = startY + i * rowH;
    u8g2.drawStr(12, y + 10, kItems[i]);
  }


  int bY = (int)(s_barY + 0.5f);
  u8g2.setDrawColor(2);
  u8g2.drawBox(4, bY, 120, rowH);
  u8g2.setDrawColor(1);
}

}
