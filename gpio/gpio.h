#pragma once
#include "core/app.h"

class GpioApp final : public App {
public:
  enum GpioScene {
    SCENE_MENU = 0,
    SCENE_IBUTTON,
    SCENE_USB_UART,
    SCENE_PIN_CONFIG,
    SCENE_I2C_SCANNER
  };

  const char* name() const override { return "GPIO"; }
  void open() override;
  void close() override;
  void draw(U8G2& u8g2, uint32_t now) override;
  void onKey(const KeyEvent& ev) override;
  bool onBack() override;
  void tick(uint32_t now) override;
  bool animates() const override { return true; }

private:
  GpioScene scene_ = SCENE_MENU;
  int menuSel_ = 0;
  float barY_ = 22.0f;
};
