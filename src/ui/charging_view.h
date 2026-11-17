#pragma once
#include <Arduino.h>
#include <U8g2lib.h>

namespace charging_view {

void init();
void start(uint32_t now = 0);
void stop();
void dismiss();
bool isActive();
bool isDismissed();
bool isCharging();
bool tick(uint32_t now);
void draw(U8G2& u8g2, uint32_t now);

} // namespace charging_view
