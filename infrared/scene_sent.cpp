#include "scene_sent.h"
#include "core/vfs_mgr.h"
#include <math.h>
#include "flipper_ir.h"
#include "core/sys_storage.h"

namespace scene_sent {

enum SentMode {
  MODE_FILES = 0,
  MODE_BUTTONS
};

static SentMode s_mode = MODE_FILES;
static std::vector<String> s_files;
static int s_fileSel = 0;
static float s_fileBarY = 16.0f;
static float s_fileScroll = 0.0f;
static float s_fileTargetScroll = 0.0f;

static String s_curFileName = "";
static std::vector<flipper_ir::Signal> s_signals;
static int s_btnSel = 0;
static float s_btnBarY = 16.0f;
static float s_btnScroll = 0.0f;
static float s_btnTargetScroll = 0.0f;

static const int rowH = 12;
static const int startY = 16;
static const int visibleRows = 4;

static void scanIrFiles() {
  s_files.clear();
  std::vector<vfs::DirEntry> entries;
  if (vfs::listDir("/ir", entries, true)) {
    for (const auto& entry : entries) {
      if (!entry.isDir && String(entry.name).endsWith(".ir")) {
        s_files.push_back(String(entry.name));
      }
    }
  }
  if (s_fileSel >= (int)s_files.size()) s_fileSel = s_files.empty() ? 0 : s_files.size() - 1;
}

void open() {
  s_mode = MODE_FILES;
  s_fileSel = 0;
  s_fileBarY = startY;
  s_fileScroll = 0.0f;
  s_fileTargetScroll = 0.0f;
  scanIrFiles();
}

void close() {
  s_files.clear();
  s_signals.clear();
  s_mode = MODE_FILES;
}

bool onBack() {
  if (s_mode == MODE_BUTTONS) {
    s_mode = MODE_FILES;
    s_signals.clear();
    return true;
  }
  return false;
}

void tick(uint32_t now) {
  if (s_mode == MODE_FILES) {
    int total = (int)s_files.size();
    if (total == 0) return;

    float targetY = startY + (s_fileSel - s_fileScroll) * rowH;
    float dy = targetY - s_fileBarY;
    if (fabsf(dy) > 0.1f) s_fileBarY += dy * 0.35f;
    else s_fileBarY = targetY;

    float ds = s_fileTargetScroll - s_fileScroll;
    if (fabsf(ds) > 0.05f) s_fileScroll += ds * 0.35f;
    else s_fileScroll = s_fileTargetScroll;
  } else if (s_mode == MODE_BUTTONS) {
    int total = (int)s_signals.size();
    if (total == 0) return;

    float targetY = startY + (s_btnSel - s_btnScroll) * rowH;
    float dy = targetY - s_btnBarY;
    if (fabsf(dy) > 0.1f) s_btnBarY += dy * 0.35f;
    else s_btnBarY = targetY;

    float ds = s_btnTargetScroll - s_btnScroll;
    if (fabsf(ds) > 0.05f) s_btnScroll += ds * 0.35f;
    else s_btnScroll = s_btnTargetScroll;
  }
}

void onKey(const KeyEvent& ev) {
  if (s_mode == MODE_FILES) {
    int total = (int)s_files.size();
    if (total == 0) return;

    if (ev.key == BTN_UP) {
      if (s_fileSel > 0) s_fileSel--;
      else s_fileSel = total - 1;

      if (s_fileSel < s_fileTargetScroll) s_fileTargetScroll = s_fileSel;
      else if (s_fileSel >= s_fileTargetScroll + visibleRows) s_fileTargetScroll = s_fileSel - visibleRows + 1;
    } else if (ev.key == BTN_DOWN) {
      if (s_fileSel < total - 1) s_fileSel++;
      else s_fileSel = 0;

      if (s_fileSel >= s_fileTargetScroll + visibleRows) s_fileTargetScroll = s_fileSel - visibleRows + 1;
      else if (s_fileSel < s_fileTargetScroll) s_fileTargetScroll = s_fileSel;
    } else if (ev.key == BTN_OK && !ev.repeat) {
      s_curFileName = s_files[s_fileSel];
      String fullPath = String("/ir/") + s_curFileName;


      s_signals.clear();
      flipper_ir::loadIrFile(fullPath.c_str(), s_signals);

      if (!s_signals.empty()) {
        s_mode = MODE_BUTTONS;
        s_btnSel = 0;
        s_btnBarY = startY;
        s_btnScroll = 0.0f;
        s_btnTargetScroll = 0.0f;
      }
    }
  } else if (s_mode == MODE_BUTTONS) {
    int total = (int)s_signals.size();
    if (total == 0) return;

    if (ev.key == BTN_UP) {
      if (s_btnSel > 0) s_btnSel--;
      else s_btnSel = total - 1;

      if (s_btnSel < s_btnTargetScroll) s_btnTargetScroll = s_btnSel;
      else if (s_btnSel >= s_btnTargetScroll + visibleRows) s_btnTargetScroll = s_btnSel - visibleRows + 1;
    } else if (ev.key == BTN_DOWN) {
      if (s_btnSel < total - 1) s_btnSel++;
      else s_btnSel = 0;

      if (s_btnSel >= s_btnTargetScroll + visibleRows) s_btnTargetScroll = s_btnSel - visibleRows + 1;
      else if (s_btnSel < s_btnTargetScroll) s_btnTargetScroll = s_btnSel;
    } else if (ev.key == BTN_OK && !ev.repeat) {

      flipper_ir::transmitSignal(s_signals[s_btnSel]);
    }
  }
}

void draw(U8G2& u8g2, uint32_t now) {
  u8g2.setFont(u8g2_font_6x12_tr);
  u8g2.setDrawColor(1);

  if (s_mode == MODE_FILES) {

    int total = (int)s_files.size();
    if (total == 0) {
      u8g2.drawStr(12, 38, "No .ir files in /ir");
    } else {
      for (int i = 0; i < total; i++) {
        int y = startY + (int)((i - s_fileScroll) * rowH + 0.5f);
        if (y < startY - rowH || y > 64) continue;
        u8g2.drawStr(6, y + 10, s_files[i].c_str());
      }

      int bY = (int)(s_fileBarY + 0.5f);
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
    u8g2.drawStr(4, 11, "SENT");
    u8g2.setDrawColor(1);
  } else if (s_mode == MODE_BUTTONS) {

    int total = (int)s_signals.size();
    for (int i = 0; i < total; i++) {
      int y = startY + (int)((i - s_btnScroll) * rowH + 0.5f);
      if (y < startY - rowH || y > 64) continue;
      u8g2.drawStr(6, y + 10, s_signals[i].name.c_str());
    }

    int bY = (int)(s_btnBarY + 0.5f);
    if (bY >= startY - 2 && bY <= 64 - rowH + 2) {
      u8g2.setDrawColor(2);
      u8g2.drawBox(2, bY, 124, rowH);
      u8g2.setDrawColor(1);
    }


    u8g2.setDrawColor(0);
    u8g2.drawBox(0, 0, 128, 14);
    u8g2.setDrawColor(1);
    u8g2.drawBox(0, 0, 128, 14);
    u8g2.setDrawColor(0);
    u8g2.setFont(u8g2_font_7x14B_tr);
    u8g2.drawStr(4, 11, s_curFileName.c_str());
    u8g2.setDrawColor(1);
  }
}

}
