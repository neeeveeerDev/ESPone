#include "scene_browser.h"
#include <math.h>
#include "fm_bitmaps.h"

namespace scene_browser {

static fm_fs::StorageType s_storage = fm_fs::STORAGE_NONE;
static char s_currentPath[128] = "/";
static std::vector<fm_fs::FileItem> s_items;

static int s_sel = 0;
static float s_barY = 16.0f;
static float s_scroll = 0.0f;
static float s_targetScroll = 0.0f;

static const int rowH = 12;
static const int startY = 16;
static const int visibleRows = 4;

static bool hasParentDir() {
  return strcmp(s_currentPath, "/") != 0;
}

bool isBinFile(const char* name) {
  if (!name) return false;
  size_t len = strlen(name);
  if (len < 4) return false;
  return (strcasecmp(name + len - 4, ".bin") == 0);
}

bool isAppFile(const char* name) {
  if (!name) return false;
  size_t len = strlen(name);
  if (len < 4) return false;
  return (strcasecmp(name + len - 4, ".app") == 0);
}

bool isJsFile(const char* name) {
  if (!name) return false;
  size_t len = strlen(name);
  if (len < 3) return false;
  return (strcasecmp(name + len - 3, ".js") == 0);
}

static int getTotalCount() {
  return (int)s_items.size() + (hasParentDir() ? 1 : 0);
}

void refresh() {
  fm_fs::listDir(s_storage, s_currentPath, s_items);
  int count = getTotalCount();
  if (s_sel >= count) s_sel = count > 0 ? count - 1 : 0;
}

void open(fm_fs::StorageType st) {
  s_storage = st;
  snprintf(s_currentPath, sizeof(s_currentPath), "/");
  s_sel = 0;
  s_barY = startY;
  s_scroll = 0.0f;
  s_targetScroll = 0.0f;
  refresh();
}

void close() {
  s_items.clear();
}

const char* getCurrentPath() {
  return s_currentPath;
}

const char* getSelectedItemName() {
  int count = getTotalCount();
  if (count == 0 || s_sel >= count) return "";
  if (hasParentDir()) {
    if (s_sel == 0) return "..";
    return s_items[s_sel - 1].name;
  }
  return s_items[s_sel].name;
}

bool isSelectedItemDir() {
  int count = getTotalCount();
  if (count == 0 || s_sel >= count) return false;
  if (hasParentDir()) {
    if (s_sel == 0) return true;
    return s_items[s_sel - 1].isDir;
  }
  return s_items[s_sel].isDir;
}

size_t getSelectedItemSize() {
  int count = getTotalCount();
  if (count == 0 || s_sel >= count) return 0;
  if (hasParentDir()) {
    if (s_sel == 0) return 0;
    return s_items[s_sel - 1].size;
  }
  return s_items[s_sel].size;
}

String getSelectedFullPath() {
  const char* nm = getSelectedItemName();
  if (strcmp(nm, "..") == 0 || strlen(nm) == 0) return String(s_currentPath);
  if (strcmp(s_currentPath, "/") == 0) {
    return String("/") + nm;
  }
  return String(s_currentPath) + "/" + nm;
}

void navigateTo(const char* path) {
  snprintf(s_currentPath, sizeof(s_currentPath), "%s", path);
  s_sel = 0;
  s_barY = startY;
  s_scroll = 0.0f;
  s_targetScroll = 0.0f;
  refresh();
}

static void navigateUp() {
  if (!hasParentDir()) return;
  char* lastSlash = strrchr(s_currentPath, '/');
  if (lastSlash && lastSlash != s_currentPath) {
    *lastSlash = '\0';
  } else {
    snprintf(s_currentPath, sizeof(s_currentPath), "/");
  }
  s_sel = 0;
  s_barY = startY;
  s_scroll = 0.0f;
  s_targetScroll = 0.0f;
  refresh();
}

void tick(uint32_t now) {
  int count = getTotalCount();
  if (count == 0) return;

  float targetY = startY + (s_sel - s_scroll) * rowH;
  float dy = targetY - s_barY;
  if (fabsf(dy) > 0.1f) s_barY += dy * 0.35f;
  else s_barY = targetY;

  float ds = s_targetScroll - s_scroll;
  if (fabsf(ds) > 0.05f) s_scroll += ds * 0.35f;
  else s_scroll = s_targetScroll;
}

BrowserAction onKey(const KeyEvent& ev) {
  int count = getTotalCount();

  if (ev.key == BTN_UP) {
    if (count > 0) {
      if (s_sel > 0) s_sel--;
      else s_sel = count - 1;

      if (s_sel < s_targetScroll) s_targetScroll = s_sel;
      else if (s_sel >= s_targetScroll + visibleRows) s_targetScroll = s_sel - visibleRows + 1;
    }
  } else if (ev.key == BTN_DOWN) {
    if (count > 0) {
      if (s_sel < count - 1) s_sel++;
      else s_sel = 0;

      if (s_sel >= s_targetScroll + visibleRows) s_targetScroll = s_sel - visibleRows + 1;
      else if (s_sel < s_targetScroll) s_targetScroll = s_sel;
    }
  } else if (ev.key == BTN_LEFT) {
    return ACTION_OPEN_OPERATIONS;
  } else if (ev.key == BTN_RIGHT) {
    return ACTION_OPEN_CREATE;
  } else if (ev.key == BTN_BACK) {
    if (hasParentDir()) {
      navigateUp();
      return ACTION_NONE;
    } else {
      return ACTION_BACK_TO_STORAGE;
    }
  } else if (ev.key == BTN_OK && !ev.repeat) {
    if (count == 0) return ACTION_NONE;
    if (hasParentDir() && s_sel == 0) {
      navigateUp();
      return ACTION_NONE;
    }
    int itemIdx = hasParentDir() ? s_sel - 1 : s_sel;
    if (s_items[itemIdx].isDir) {

      if (strcmp(s_currentPath, "/") == 0) {
        snprintf(s_currentPath, sizeof(s_currentPath), "/%s", s_items[itemIdx].name);
      } else {
        size_t len = strlen(s_currentPath);
        snprintf(s_currentPath + len, sizeof(s_currentPath) - len, "/%s", s_items[itemIdx].name);
      }
      s_sel = 0;
      s_barY = startY;
      s_scroll = 0.0f;
      s_targetScroll = 0.0f;
      refresh();
      return ACTION_NONE;
    } else {

      if (isAppFile(s_items[itemIdx].name)) {
        return ACTION_INSTALL_APP;
      }
      if (isJsFile(s_items[itemIdx].name)) {
        return ACTION_RUN_JS;
      }
      if (isBinFile(s_items[itemIdx].name)) {
        return ACTION_UPDATE_FIRMWARE;
      }
      return ACTION_OPEN_FILE;
    }
  }
  return ACTION_NONE;
}

void draw(U8G2& u8g2, uint32_t now) {
  int count = getTotalCount();
  bool parent = hasParentDir();


  u8g2.setFont(u8g2_font_6x12_tr);
  u8g2.setDrawColor(1);

  if (count == 0) {
    u8g2.drawStr(16, 38, "Folder is empty");
  } else {
    for (int i = 0; i < count; i++) {
      int y = startY + (int)((i - s_scroll) * rowH + 0.5f);
      if (y < startY - rowH || y > 64) continue;

      if (parent && i == 0) {

        u8g2.drawXBMP(4, y + 1, kFmIconW, kFmIconH, file_icon_back_bits);
        u8g2.drawStr(18, y + 10, "..");
      } else {
        int itemIdx = parent ? i - 1 : i;
        const auto& it = s_items[itemIdx];
        if (it.isDir) {
          u8g2.drawXBMP(4, y + 1, kFmIconW, kFmIconH, file_icon_dir_bits);
        } else if (isAppFile(it.name) || isJsFile(it.name)) {
          u8g2.drawXBMP(4, y + 1, kFmIconW, kFmIconH, file_icon_app_bits);
        } else if (isBinFile(it.name)) {
          u8g2.drawXBMP(4, y + 1, kFmIconW, kFmIconH, file_icon_update_bits);
        } else {
          u8g2.drawXBMP(4, y + 1, kFmIconW, kFmIconH, file_icon_file_bits);
        }


        char dispName[22];
        snprintf(dispName, sizeof(dispName), "%s", it.name);
        u8g2.drawStr(18, y + 10, dispName);
      }
    }


    int bY = (int)(s_barY + 0.5f);
    if (bY >= startY - 2 && bY <= 64 - rowH + 2) {
      u8g2.setDrawColor(2);
      u8g2.drawBox(2, bY, 124, rowH);
      u8g2.setDrawColor(1);
    }
  }


  u8g2.setDrawColor(0);
  u8g2.drawBox(0, 0, 128, 14);
  u8g2.setDrawColor(1);
  u8g2.drawBox(0, 0, 128, 14);
  u8g2.setDrawColor(0);
  u8g2.setFont(u8g2_font_7x14B_tr);

  char title[32];
  const char* pfx = (s_storage == fm_fs::STORAGE_SD) ? "SD:" : "INT:";
  if (strlen(s_currentPath) > 12) {
    const char* lastSlash = strrchr(s_currentPath, '/');
    if (lastSlash && lastSlash != s_currentPath) {
      snprintf(title, sizeof(title), "%s..%s", pfx, lastSlash);
    } else {
      snprintf(title, sizeof(title), "%s%s", pfx, s_currentPath);
    }
  } else {
    snprintf(title, sizeof(title), "%s%s", pfx, s_currentPath);
  }
  u8g2.drawStr(4, 11, title);
  u8g2.setDrawColor(1);
}

}
