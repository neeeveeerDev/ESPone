#pragma once
#include "core/keys.h"

namespace buttons {
void init();
bool poll(KeyEvent& ev);
void inject(BtnKey key, bool repeat = false);
}
