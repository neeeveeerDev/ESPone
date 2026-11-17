#pragma once
#include "core/app.h"

class AppsApp final : public App {
public:
  const char* name() const override { return "Apps"; }
  void open() override;
  void close() override;
  void draw(U8G2& u8g2, uint32_t now) override;
  void onKey(const KeyEvent& ev) override;
  bool onBack() override;
  void tick(uint32_t now) override;
  bool animates() const override { return true; }

private:
  enum AppScene {
    SCENE_MENU = 0,
    SCENE_FILE_MANAGER,
    SCENE_APP_MANAGER,
    SCENE_GAMES,
    SCENE_CLOCK,
    SCENE_CALENDAR,
    SCENE_CALCULATOR,
    SCENE_JS_APP
  };

  AppScene scene_ = SCENE_MENU;
  int menuSel_ = 0;
  float barY_ = 18.0f;
  float scroll_ = 0.0f;
  float targetScroll_ = 0.0f;
};
