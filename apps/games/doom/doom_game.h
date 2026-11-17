#pragma once
#include <Arduino.h>
#include <U8g2lib.h>
#include "core/keys.h"

namespace doom_game {

void open();
void close();
void draw(U8G2& u8g2, uint32_t now);
void onKey(const KeyEvent& ev);
bool onBack();
void tick(uint32_t now);
bool isRunning();

}
