#pragma once
#include <U8g2lib.h>
#include "core/keys.h"
#include "fm_fs.h"

namespace scene_storage {

void open();
void close();
void tick(uint32_t now);
void onKey(const KeyEvent& ev);
void draw(U8G2& u8g2, uint32_t now);


fm_fs::StorageType getSelectedStorage();
void resetSelection();

}
