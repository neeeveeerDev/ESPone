#pragma once
#include <U8g2lib.h>
#include "core/keys.h"

namespace scene_keyboard {

enum KeyboardResult {
  KB_NONE = 0,
  KB_DONE,
  KB_CANCEL
};

void open(const char* title, bool isFile, const char* defaultName = "");
void close();
void tick(uint32_t now);
KeyboardResult onKey(const KeyEvent& ev);
void draw(U8G2& u8g2, uint32_t now);


String getResultString();

}
