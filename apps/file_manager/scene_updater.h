#pragma once
#include <U8g2lib.h>
#include "core/keys.h"
#include "fm_fs.h"

namespace scene_updater {

enum UpdaterResult {
  UPDATER_RUNNING = 0,
  UPDATER_CANCEL
};

void open(fm_fs::StorageType st, const char* fullPath, const char* fileName);
void close();
UpdaterResult onKey(const KeyEvent& ev);
void tick(uint32_t now);
void draw(U8G2& u8g2, uint32_t now);

}
