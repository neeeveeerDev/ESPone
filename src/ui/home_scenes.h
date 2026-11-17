#pragma once
#include <Arduino.h>
#include <U8g2lib.h>
#include "core/keys.h"

namespace home_scenes {

void init();
void draw(U8G2& u8g2, uint32_t now);
void onKey(const KeyEvent& ev);
void tick(uint32_t now);
void nextScene();
void prevScene();
uint8_t currentSceneIndex();

}
