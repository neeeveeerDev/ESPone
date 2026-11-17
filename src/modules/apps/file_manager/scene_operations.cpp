#include "scene_operations.h"
#include <math.h>

namespace scene_operations {

static const char* kAllOps[] = {
  "Open",
  "Copy",
  "Move",
  "Paste",
  "Delete",
  "Info"
};
static const int kTotalOps = 6;

static fm_fs::StorageType s_st = fm_fs::STORAGE_NONE;
static char s_currentDir[128] = "/";
static char s_itemName[36] = "";
static bool s_isDir = false;
static size_t s_size = 0;


static char s_clipPath[128] = "";
static bool s_clipIsMove = false;
static bool s_clipValid = false;
static fm_fs::StorageType s_clipSt = fm_fs::STORAGE_NONE;

static int s_sel = 0;
static float s_barY = 16.0f;
static float s_scroll = 0.0f;
static float s_targetScroll = 0.0f;
static const int rowH = 12;
static const int startY = 16;
static const int visibleRows = 4;

static bool s_showInfo = false;
static const char* s_feedbackMsg = nullptr;
static uint32_t s_feedbackUntil = 0;

void open(fm_fs::StorageType st, const char* currentDir, const char* itemName, bool isDir, size_t size) {
  s_st = st;
  snprintf(s_currentDir, sizeof(s_currentDir), "%s", currentDir ? currentDir : "/");
  snprintf(s_itemName, sizeof(s_itemName), "%s", itemName ? itemName : "");
  s_isDir = isDir;
  s_size = size;

  s_sel = 0;
  s_barY = startY;
  s_scroll = 0.0f;
  s_targetScroll = 0.0f;
  s_showInfo = false;
  s_feedbackMsg = nullptr;
  s_feedbackUntil = 0;
}

void close() {}

void tick(uint32_t now) {
  if (s_showInfo) return;

  float targetY = startY + (s_sel - s_scroll) * rowH;
  float dy = targetY - s_barY;
  if (fabsf(dy) > 0.1f) s_barY += dy * 0.35f;
  else s_barY = targetY;

  float ds = s_targetScroll - s_scroll;
  if (fabsf(ds) > 0.05f) s_scroll += ds * 0.35f;
  else s_scroll = s_targetScroll;

  if (s_feedbackMsg && now > s_feedbackUntil) {
    s_feedbackMsg = nullptr;
  }
}

static String getTargetFullPath() {
  if (strcmp(s_itemName, "..") == 0 || strlen(s_itemName) == 0) return String(s_currentDir);
  if (strcmp(s_currentDir, "/") == 0) {
    return String("/") + s_itemName;
  }
  return String(s_currentDir) + "/" + s_itemName;
}

OpResult onKey(const KeyEvent& ev) {
  if (s_showInfo) {
    if (ev.key == BTN_OK || ev.key == BTN_BACK) {
      s_showInfo = false;
    }
    return OP_NONE;
  }

  if (ev.key == BTN_UP) {
    if (s_sel > 0) s_sel--;
    else s_sel = kTotalOps - 1;

    if (s_sel < s_targetScroll) s_targetScroll = s_sel;
    else if (s_sel >= s_targetScroll + visibleRows) s_targetScroll = s_sel - visibleRows + 1;
  } else if (ev.key == BTN_DOWN) {
    if (s_sel < kTotalOps - 1) s_sel++;
    else s_sel = 0;

    if (s_sel >= s_targetScroll + visibleRows) s_targetScroll = s_sel - visibleRows + 1;
    else if (s_sel < s_targetScroll) s_targetScroll = s_sel;
  } else if (ev.key == BTN_BACK) {
    return OP_CLOSE;
  } else if (ev.key == BTN_OK && !ev.repeat) {
    String fullPath = getTargetFullPath();

    if (s_sel == 0) {

      if (!s_isDir && strlen(s_itemName) > 0 && strcmp(s_itemName, "..") != 0) {
        return OP_OPEN_VIEWER;
      }
      return OP_CLOSE;
    } else if (s_sel == 1) {

      if (strlen(s_itemName) > 0 && strcmp(s_itemName, "..") != 0 && !s_isDir) {
        snprintf(s_clipPath, sizeof(s_clipPath), "%s", fullPath.c_str());
        s_clipIsMove = false;
        s_clipValid = true;
        s_clipSt = s_st;
        s_feedbackMsg = "Copied to clipboard";
        s_feedbackUntil = millis() + 1200;
      }
    } else if (s_sel == 2) {

      if (strlen(s_itemName) > 0 && strcmp(s_itemName, "..") != 0 && !s_isDir) {
        snprintf(s_clipPath, sizeof(s_clipPath), "%s", fullPath.c_str());
        s_clipIsMove = true;
        s_clipValid = true;
        s_clipSt = s_st;
        s_feedbackMsg = "Moved to clipboard";
        s_feedbackUntil = millis() + 1200;
      }
    } else if (s_sel == 3) {

      if (s_clipValid && s_clipSt == s_st) {
        const char* baseName = strrchr(s_clipPath, '/');
        if (baseName) baseName++;
        else baseName = s_clipPath;

        String dst;
        if (strcmp(s_currentDir, "/") == 0) dst = String("/") + baseName;
        else dst = String(s_currentDir) + "/" + baseName;

        if (s_clipIsMove) {
          fm_fs::moveFile(s_st, s_clipPath, dst.c_str());
          s_clipValid = false;
        } else {
          fm_fs::copyFile(s_st, s_clipPath, dst.c_str());
        }
        return OP_REFRESH;
      } else {
        s_feedbackMsg = "Clipboard empty";
        s_feedbackUntil = millis() + 1200;
      }
    } else if (s_sel == 4) {

      if (strlen(s_itemName) > 0 && strcmp(s_itemName, "..") != 0) {
        fm_fs::deleteItem(s_st, fullPath.c_str(), s_isDir);
        return OP_REFRESH;
      }
    } else if (s_sel == 5) {

      s_showInfo = true;
    }
  }
  return OP_NONE;
}

void draw(U8G2& u8g2, uint32_t now) {
  if (s_showInfo) {

    u8g2.setDrawColor(1);
    u8g2.drawBox(0, 0, 128, 14);
    u8g2.setDrawColor(0);
    u8g2.setFont(u8g2_font_7x14B_tr);
    u8g2.drawStr(4, 11, "INFO");
    u8g2.setDrawColor(1);

    u8g2.setFont(u8g2_font_6x12_tr);
    char buf[36];
    snprintf(buf, sizeof(buf), "Name: %s", s_itemName);
    u8g2.drawStr(4, 26, buf);

    snprintf(buf, sizeof(buf), "Type: %s", s_isDir ? "Folder" : "File");
    u8g2.drawStr(4, 38, buf);

    if (s_isDir) {
      u8g2.drawStr(4, 50, "Size: -");
    } else if (s_size >= 1024) {
      snprintf(buf, sizeof(buf), "Size: %u KB", (unsigned int)(s_size / 1024));
      u8g2.drawStr(4, 50, buf);
    } else {
      snprintf(buf, sizeof(buf), "Size: %u B", (unsigned int)s_size);
      u8g2.drawStr(4, 50, buf);
    }

    String fp = getTargetFullPath();
    snprintf(buf, sizeof(buf), "Path: %s", fp.c_str());
    u8g2.drawStr(4, 62, buf);
    return;
  }


  u8g2.setFont(u8g2_font_6x12_tr);
  u8g2.setDrawColor(1);

  for (int i = 0; i < kTotalOps; i++) {
    int y = startY + (int)((i - s_scroll) * rowH + 0.5f);
    if (y < startY - rowH || y > 64) continue;
    u8g2.drawStr(12, y + 9, kAllOps[i]);
  }


  int bY = (int)(s_barY + 0.5f);
  if (bY >= startY - 2 && bY <= 64 - rowH + 2) {
    u8g2.setDrawColor(2);
    u8g2.drawBox(4, bY, 120, rowH);
    u8g2.setDrawColor(1);
  }


  u8g2.setDrawColor(0);
  u8g2.drawBox(0, 0, 128, 14);
  u8g2.setDrawColor(1);
  u8g2.drawBox(0, 0, 128, 14);
  u8g2.setDrawColor(0);
  u8g2.setFont(u8g2_font_7x14B_tr);
  u8g2.drawStr(4, 11, "OPTIONS");

  if (s_feedbackMsg) {
    u8g2.setFont(u8g2_font_5x8_tr);
    int fw = u8g2.getStrWidth(s_feedbackMsg);
    u8g2.drawStr(124 - fw, 10, s_feedbackMsg);
  }
  u8g2.setDrawColor(1);
}

}
