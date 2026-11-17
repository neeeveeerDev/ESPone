#pragma once
#include "core/app.h"

class SubghzApp final : public App {
public:
  enum SubScene {
    SCENE_MENU = 0,
    SCENE_READ,
    SCENE_READ_RAW,
    SCENE_SAVED,
    SCENE_CUSTOM_SUB,
    SCENE_ADD_MANUALLY,
    SCENE_FREQ_ANALYZER,
    SCENE_SPECTRUM,
    SCENE_RADIO_SETTINGS,
    SCENE_BRUTEFORCE,
    SCENE_JS_APP
  };

  const char* name() const override { return "SubGhz"; }
  void open() override;
  void close() override;
  void draw(U8G2& u8g2, uint32_t now) override;
  void onKey(const KeyEvent& ev) override;
  bool onBack() override;
  void tick(uint32_t now) override;
  bool animates() const override { return true; }

private:
  SubScene scene_ = SCENE_MENU;
  int menuSel_ = 0;
  float barY_ = 16.0f;
  float scroll_ = 0.0f;
  float targetScroll_ = 0.0f;
};
