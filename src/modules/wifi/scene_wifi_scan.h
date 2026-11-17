#pragma once
#include <Arduino.h>
#include <U8g2lib.h>
#include "core/keys.h"

namespace scene_wifi_scan {

struct WifiApItem {
  char ssid[33];
  char bssidStr[18];
  int32_t rssi;
  uint8_t channel;
  uint8_t authMode;
};

void open();
void close();
bool onBack();
void tick(uint32_t now);
void onKey(const KeyEvent& ev);
void draw(U8G2& u8g2, uint32_t now);


bool hasSelectedTarget();
const WifiApItem& getSelectedTarget();
void clearSelectedTarget();
void setTarget(const char* ssid, const char* bssid, uint8_t channel, int32_t rssi);

}
