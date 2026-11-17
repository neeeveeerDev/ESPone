#pragma once
#include "core/app.h"





class MenuView {
public:
  void init(App* const* apps, int count);
  void reset();
  int selection() const { return sel_; }
  App* selectedApp() const { return (apps_ && sel_ >= 0 && sel_ < count_) ? apps_[sel_] : nullptr; }
  bool tick();
  void draw(U8G2& u8g2, uint32_t now);
  void onKey(const KeyEvent& ev);

private:
  void move(int d);

  App* const* apps_ = nullptr;
  int count_ = 0;
  int sel_ = 0;
  float scroll_ = 0;
  float target_ = 0;
};
