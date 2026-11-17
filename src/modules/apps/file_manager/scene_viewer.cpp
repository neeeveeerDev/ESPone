#include "scene_viewer.h"
#include <math.h>

namespace scene_viewer {

static char s_fileName[32] = "";
static std::vector<String> s_lines;
static int s_scroll = 0;
static float s_scrollSmooth = 0.0f;
static const int lineH = 10;
static const int startY = 16;
static const int visibleLines = 5;

void open(fm_fs::StorageType st, const char* path, const char* fileName) {
  snprintf(s_fileName, sizeof(s_fileName), "%s", fileName ? fileName : "Viewer");
  s_scroll = 0;
  s_scrollSmooth = 0.0f;
  s_lines.clear();
  fm_fs::readFileContent(st, path, s_lines, 200);
}

void close() {
  s_lines.clear();
}

void tick(uint32_t now) {
  float ds = s_scroll - s_scrollSmooth;
  if (fabsf(ds) > 0.05f) s_scrollSmooth += ds * 0.35f;
  else s_scrollSmooth = s_scroll;
}

bool onKey(const KeyEvent& ev) {
  int total = (int)s_lines.size();

  if (ev.key == BTN_UP) {
    if (s_scroll > 0) s_scroll--;
  } else if (ev.key == BTN_DOWN) {
    if (s_scroll + visibleLines < total) s_scroll++;
  } else if (ev.key == BTN_BACK || (ev.key == BTN_OK && !ev.repeat)) {
    return true;
  }
  return false;
}

void draw(U8G2& u8g2, uint32_t now) {

  u8g2.setFont(u8g2_font_5x8_tr);
  u8g2.setDrawColor(1);

  if (s_lines.empty()) {
    u8g2.drawStr(12, 38, "File is empty or binary");
  } else {
    for (int i = 0; i < (int)s_lines.size(); i++) {
      int y = startY + (int)((i - s_scrollSmooth) * lineH + 0.5f);
      if (y < startY - lineH || y > 64) continue;
      u8g2.drawStr(4, y + 8, s_lines[i].c_str());
    }
  }


  u8g2.setDrawColor(0);
  u8g2.drawBox(0, 0, 128, 14);
  u8g2.setDrawColor(1);
  u8g2.drawBox(0, 0, 128, 14);
  u8g2.setDrawColor(0);
  u8g2.setFont(u8g2_font_7x14B_tr);
  u8g2.drawStr(4, 11, s_fileName);
  u8g2.setDrawColor(1);
}

}
