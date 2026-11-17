#pragma once
#include <U8g2lib.h>
#include "core/keys.h"
#include "fm_fs.h"

namespace scene_browser {

enum BrowserAction {
  ACTION_NONE = 0,
  ACTION_BACK_TO_STORAGE,
  ACTION_OPEN_FILE,
  ACTION_OPEN_OPERATIONS,
  ACTION_OPEN_CREATE,
  ACTION_UPDATE_FIRMWARE,
  ACTION_INSTALL_APP,
  ACTION_RUN_JS
};

void open(fm_fs::StorageType st);
void close();
void refresh();
void tick(uint32_t now);
BrowserAction onKey(const KeyEvent& ev);
void draw(U8G2& u8g2, uint32_t now);

const char* getCurrentPath();
const char* getSelectedItemName();
bool isSelectedItemDir();
size_t getSelectedItemSize();
String getSelectedFullPath();
void navigateTo(const char* path);

bool isBinFile(const char* name);
bool isAppFile(const char* name);
bool isJsFile(const char* name);

}
