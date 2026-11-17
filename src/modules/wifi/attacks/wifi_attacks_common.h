#pragma once
#include <Arduino.h>
#include <U8g2lib.h>
#include "ui/keyboard.h"

namespace wifi_attacks_common {

static const int kStartY = 16;
static const int kRowH   = 12;

void drawHeader(U8G2& u8g2, const char* title, int count = -1);
void updateScroll(int sel, int count, float& scroll, float& targetScroll, float& barY);

}
