#pragma once
#include <U8g2lib.h>
#include "core/keys.h"

namespace disclaimer {

void init();
bool isAccepted();
void setAccepted(bool val);

void onKey(const KeyEvent& ev);
bool tick(uint32_t now);
void draw(U8G2& u8g2, uint32_t now);

}
