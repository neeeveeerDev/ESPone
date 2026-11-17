#pragma once
#include <U8g2lib.h>
#include "core/keys.h"
#include "fm_fs.h"

namespace scene_viewer {

void open(fm_fs::StorageType st, const char* path, const char* fileName);
void close();
void tick(uint32_t now);
bool onKey(const KeyEvent& ev);
void draw(U8G2& u8g2, uint32_t now);

}
