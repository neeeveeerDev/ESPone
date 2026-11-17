#pragma once
#include "core/app.h"

class InfraredApp final : public App {
public:
  enum IrScene {
    SCENE_MENU = 0,
    SCENE_UNIVERSAL,
    SCENE_TV_B_GONE,
    SCENE_COPY,
    SCENE_CUSTOM_IR,
    SCENE_SENT,
    SCENE_JAMMER,
    SCENE_JS_APP
  };

  const char* name() const override { return "Infrared"; }
  void open() override;
  void close() override;
  void draw(U8G2& u8g2, uint32_t now) override;
  void onKey(const KeyEvent& ev) override;
  bool onBack() override;
  void tick(uint32_t now) override;
  bool animates() const override { return true; }

private:
  IrScene scene_ = SCENE_MENU;
  int menuSel_ = 0;
  float barY_ = 15.0f;
  float scroll_ = 0.0f;
  float targetScroll_ = 0.0f;
};
