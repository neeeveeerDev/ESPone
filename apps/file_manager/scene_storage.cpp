#include "scene_storage.h"
#include <math.h>

namespace scene_storage {

static const char* kItems[] = {
  "SD Card",
  "Internal Storage"
};
static const int kCount = 2;
static const int rowH = 14;
static const int startY = 22;

static int s_sel = 0;
static float s_barY = startY;
static fm_fs::StorageType s_selected = fm_fs::STORAGE_NONE;
static const char* s_errorMsg = nullptr;
static uint32_t s_errorUntil = 0;

void open() {
  s_sel = 0;
  s_barY = startY;
  s_selected = fm_fs::STORAGE_NONE;
  s_errorMsg = nullptr;
  s_errorUntil = 0;
}

void close() {}

fm_fs::StorageType getSelectedStorage() {
  return s_selected;
}

void resetSelection() {
  s_selected = fm_fs::STORAGE_NONE;
}

void tick(uint32_t now) {
  float target = startY + s_sel * rowH;
  float dy = target - s_barY;
  if (fabsf(dy) > 0.1f) s_barY += dy * 0.35f;
  else s_barY = target;

  if (s_errorMsg && now > s_errorUntil) {
    s_errorMsg = nullptr;
  }
}

void onKey(const KeyEvent& ev) {
  if (ev.key == BTN_UP) {
    if (s_sel > 0) s_sel--;
    else s_sel = kCount - 1;
  } else if (ev.key == BTN_DOWN) {
    if (s_sel < kCount - 1) s_sel++;
    else s_sel = 0;
  } else if (ev.key == BTN_OK && !ev.repeat) {
    if (s_sel == 0) {

      if (fm_fs::initSD()) {
        s_selected = fm_fs::STORAGE_SD;
      } else {
        s_errorMsg = "SD Card not mounted";
        s_errorUntil = millis() + 2000;
      }
    } else if (s_sel == 1) {

      if (fm_fs::initInternal()) {
        s_selected = fm_fs::STORAGE_INTERNAL;
      } else {
        s_errorMsg = "Internal Flash error";
        s_errorUntil = millis() + 2000;
      }
    }
  }
}

void draw(U8G2& u8g2, uint32_t now) {

  u8g2.setDrawColor(1);
  u8g2.drawBox(0, 0, 128, 14);
  u8g2.setDrawColor(0);
  u8g2.setFont(u8g2_font_7x14B_tr);
  u8g2.drawStr(4, 11, "STORAGE");
  u8g2.setDrawColor(1);

  if (s_errorMsg) {
    u8g2.setFont(u8g2_font_6x12_tr);
    int w = u8g2.getStrWidth(s_errorMsg);
    u8g2.drawStr((128 - w) / 2, 42, s_errorMsg);
    return;
  }


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
