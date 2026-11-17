#pragma once
#include <U8g2lib.h>
#include "core/keys.h"

namespace scene_add_manually {

void open();
void close();
void onKey(const KeyEvent& ev);
void tick(uint32_t now);
void draw(U8G2& u8g2, uint32_t now);

}
