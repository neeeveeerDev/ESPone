#pragma once
#include <U8g2lib.h>

namespace display {
void init();
U8G2& get();

void setContrast(uint8_t contrast);
void setInvert(bool invert);
void setPowerSave(bool sleep);
bool isSleeping();
}
