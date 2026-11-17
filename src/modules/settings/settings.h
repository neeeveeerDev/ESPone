#pragma once
#include "core/app.h"

class SettingsApp final : public App {
public:
  enum SettingsScene {
    SCENE_MENU = 0,
    SCENE_DISPLAY,
    SCENE_CONNECTIVITY,
    SCENE_DATETIME,
    SCENE_POWER,
    SCENE_SYSTEM
  };

  const char* name() const override { return "Settings"; }
  void open() override;
  void close() override;
  void draw(U8G2& u8g2, uint32_t now) override;
  void onKey(const KeyEvent& ev) override;
  bool onBack() override;
  void tick(uint32_t now) override;
  bool animates() const override { return true; }

private:
  SettingsScene scene_ = SCENE_MENU;
  int menuSel_ = 0;
  float barY_ = 16.0f;
  float scroll_ = 0.0f;
  float targetScroll_ = 0.0f;
};
