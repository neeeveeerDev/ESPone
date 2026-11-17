#include "menu.h"

namespace {
const int kItemH = 16;
}

void MenuView::init(App* const* apps, int count) {
  apps_ = apps;
  count_ = count;
  reset();
}

void MenuView::reset() { sel_ = 0; scroll_ = target_ = 0; }


static float targetFor(int s, int count) {
  int maxScroll = count * kItemH - 64;
  float t = s * kItemH + kItemH / 2.0f - 32.0f;
  if (t < 0) t = 0;
  if (t > maxScroll) t = maxScroll;
  return t;
}

void MenuView::move(int d) {
  sel_ = (sel_ + d + count_) % count_;
  target_ = targetFor(sel_, count_);
  Serial.printf("[menu] %s\n", apps_[sel_]->name());
}

bool MenuView::tick() {
  float d = target_ - scroll_;
  if (fabsf(d) > 0.15f) {
    scroll_ += d * 0.35f;
    return true;
  }
  scroll_ = target_;
  return false;
}

void MenuView::onKey(const KeyEvent& ev) {
  switch (ev.key) {
    case BTN_UP:
    case BTN_LEFT:  move(-1); break;
    case BTN_DOWN:
    case BTN_RIGHT: move(+1); break;
    default: break;
  }
}

void MenuView::draw(U8G2& u8g2, uint32_t now) {
  (void)now;
  u8g2.setFont(u8g2_font_9x15B_tr);
  int off = (int)(scroll_ + 0.5f);
  for (int i = 0; i < count_; i++) {
    int y = i * kItemH - off;
    if (y <= -kItemH || y >= 64) continue;
    if (i == sel_) {
      u8g2.setDrawColor(1);
      u8g2.drawRFrame(1, y, 126, kItemH - 1, 3);
    }
    u8g2.setDrawColor(1);
    u8g2.drawStr(6, y + 12, apps_[i]->name());
  }
}
