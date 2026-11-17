#pragma once
#include <U8g2lib.h>
#include "core/keys.h"

namespace settings_conn {
void open();
void close();
bool onBack();
void tick(uint32_t now);
void onKey(const KeyEvent& ev);
void draw(U8G2& u8g2, uint32_t now);
}
