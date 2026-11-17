#pragma once
#include <U8g2lib.h>
#include "core/keys.h"

namespace scene_create {

enum CreateTarget {
  CREATE_NONE = 0,
  CREATE_FOLDER,
  CREATE_FILE,
  CREATE_CANCEL
};

void open();
void close();
void tick(uint32_t now);
CreateTarget onKey(const KeyEvent& ev);
void draw(U8G2& u8g2, uint32_t now);

}
