#pragma once
#include <Arduino.h>
#include <U8g2lib.h>

namespace status_bar {

void init();
void draw(U8G2& u8g2, uint32_t now);

}
