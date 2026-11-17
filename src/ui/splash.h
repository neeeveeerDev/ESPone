#pragma once
#include <Arduino.h>
#include <U8g2lib.h>

namespace splash {
void start(uint32_t now);
bool finished(uint32_t now);
void draw(U8G2& u8g2, uint32_t now);
}
