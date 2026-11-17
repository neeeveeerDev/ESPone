#pragma once
#include <U8g2lib.h>
#include "core/keys.h"
#include "fm_fs.h"

namespace scene_operations {

enum OpResult {
  OP_NONE = 0,
  OP_CLOSE,
  OP_REFRESH,
  OP_OPEN_VIEWER
};

void open(fm_fs::StorageType st, const char* currentDir, const char* itemName, bool isDir, size_t size);
void close();
void tick(uint32_t now);
OpResult onKey(const KeyEvent& ev);
void draw(U8G2& u8g2, uint32_t now);

}
