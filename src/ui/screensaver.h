#pragma once
#include <Arduino.h>
#include <U8g2lib.h>

namespace screensaver {

void start(uint8_t mode);
void stop();
bool isActive();
uint8_t getMode();
bool tick(uint32_t now);
void draw(U8G2& u8g2, uint32_t now);

}
