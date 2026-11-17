#pragma once
#include "core/app.h"

class WifiApp final : public App {
public:
  enum WifiScene {
    SCENE_MENU = 0,
    SCENE_SCAN,
    SCENE_ATTACKS,
    SCENE_PACKET_MONITOR,
    SCENE_RSSI_RADAR,
    SCENE_WARDRIVING,
    SCENE_JAM_DETECT,
    SCENE_SNIFFER,
    SCENE_PMKID
  };

  const char* name() const override { return "Wi-Fi"; }
  void open() override;
  void close() override;
  void draw(U8G2& u8g2, uint32_t now) override;
  void onKey(const KeyEvent& ev) override;
  bool onBack() override;
  void tick(uint32_t now) override;
  bool animates() const override { return true; }

private:
  WifiScene scene_ = SCENE_MENU;
  int menuSel_ = 0;
  float barY_ = 16.0f;
  float scroll_ = 0.0f;
  float targetScroll_ = 0.0f;
};
