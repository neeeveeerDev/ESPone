#include "scene_saved.h"
#include "subghz_parser.h"
#include "subghz_protocols.h"
#include "cc1101_driver.h"
#include <FFat.h>
#include <SD.h>
#include <math.h>

namespace scene_saved {

enum SavedMode {
  MODE_LIST = 0,
  MODE_DETAILS
};

static SavedMode s_mode = MODE_LIST;
static std::vector<String> s_files;
static int s_sel = 0;
static float s_scroll = 0.0f;
static float s_targetScroll = 0.0f;
static float s_barY = 16.0f;

static SubGhzItem s_currentItem;
static int s_actionSel = 0;
static const char* kActions[] = {"SEND", "DELETE"};

static uint32_t s_txUntil = 0;
static const char* s_toast = nullptr;
static uint32_t s_toastUntil = 0;

static const int rowH = 12;
static const int startY = 16;

void open() {
  s_mode = MODE_LIST;
  s_sel = 0;
  s_scroll = 0.0f;
  s_targetScroll = 0.0f;
  s_barY = startY;
  s_txUntil = 0;
  s_toast = nullptr;

  subghz_parser::listSavedFiles(s_files);
}

void close() {
  s_files.clear();
}

bool onBack() {
  if (s_mode == MODE_DETAILS) {
    s_mode = MODE_LIST;
    return true;
  }
  return false;
}

void onKey(const KeyEvent& ev) {
  if (s_mode == MODE_LIST) {
    int count = s_files.size();
    if (count == 0) return;

    if (ev.key == BTN_UP) {
      if (s_sel == 0) {
        s_sel = count - 1;
      } else {
        s_sel--;
      }
    } else if (ev.key == BTN_DOWN) {
      if (s_sel == count - 1) {
        s_sel = 0;
      } else {
        s_sel++;
      }
    } else if (ev.key == BTN_OK && !ev.repeat) {
      if (subghz_parser::loadFile(s_files[s_sel].c_str(), s_currentItem)) {
        s_mode = MODE_DETAILS;
        s_actionSel = 0;
      }
    }
  } else if (s_mode == MODE_DETAILS) {
    if (ev.key == BTN_LEFT && !ev.repeat) {
      s_actionSel = 0;
    } else if (ev.key == BTN_RIGHT && !ev.repeat) {
      s_actionSel = 1;
    } else if (ev.key == BTN_OK && !ev.repeat) {
      if (s_actionSel == 0) {

        s_txUntil = millis() + 450;
        if (s_currentItem.isRaw) {
          cc1101::sendRaw(s_currentItem.rawData.data(), s_currentItem.rawData.size(), s_currentItem.repeat > 0 ? s_currentItem.repeat : 2);
        } else {
          std::vector<int32_t> timings;
          if (subghz_protocols::encode(s_currentItem.protocol, s_currentItem.key, s_currentItem.bits, s_currentItem.te, timings)) {
            cc1101::sendRaw(timings.data(), timings.size(), s_currentItem.repeat > 0 ? s_currentItem.repeat : 3);
          }
        }
      } else if (s_actionSel == 1) {

        if (s_currentItem.path.startsWith("/sd")) {
          const char* sp = s_currentItem.path.c_str() + 3;
          SD.remove(sp);
        } else {
          FFat.remove(s_currentItem.path.c_str());
        }
        subghz_parser::listSavedFiles(s_files);
        if (s_sel >= (int)s_files.size() && s_sel > 0) s_sel--;
        s_mode = MODE_LIST;
      }
    }
  }
}

void tick(uint32_t now) {
  if (s_txUntil > 0 && now >= s_txUntil) s_txUntil = 0;
  if (s_toast && now >= s_toastUntil) s_toast = nullptr;

  if (s_mode == MODE_LIST) {
    int maxLines = 4;
    int maxScroll = (int)s_files.size() - maxLines;
    if (maxScroll < 0) maxScroll = 0;

    int topIndex = (int)(s_targetScroll / rowH);
    if (s_sel < topIndex) s_targetScroll = s_sel * rowH;
    else if (s_sel >= topIndex + maxLines) s_targetScroll = (s_sel - maxLines + 1) * rowH;

    if (s_targetScroll < 0) s_targetScroll = 0;
    if (s_targetScroll > maxScroll * rowH) s_targetScroll = maxScroll * rowH;

    float ds = s_targetScroll - s_scroll;
    if (fabsf(ds) > 0.1f) s_scroll += ds * 0.35f;
    else s_scroll = s_targetScroll;

    float targetBarY = startY + s_sel * rowH - s_scroll;
    float dy = targetBarY - s_barY;
    if (fabsf(dy) > 0.1f) s_barY += dy * 0.4f;
    else s_barY = targetBarY;
  }
}

void draw(U8G2& u8g2, uint32_t now) {

  if (s_mode == MODE_LIST) {
    if (s_files.empty()) {
      u8g2.setFont(u8g2_font_6x12_tr);
      u8g2.drawStr(12, 36, "No .sub files");
    } else {
      u8g2.setFont(u8g2_font_6x12_tr);
      for (size_t i = 0; i < s_files.size(); i++) {
        int y = startY + (int)(i * rowH - s_scroll + 0.5f);
        if (y < 14 || y > 64) continue;

        String name = s_files[i];
        int lastSlash = name.lastIndexOf('/');
        if (lastSlash >= 0) name = name.substring(lastSlash + 1);
        if (name.endsWith(".sub")) name = name.substring(0, name.length() - 4);

        u8g2.drawStr(6, y + 10, name.c_str());
      }


      int bY = (int)(s_barY + 0.5f);
      if (bY >= 14 && bY <= 64 - rowH) {
        u8g2.setDrawColor(2);
        u8g2.drawBox(2, bY, 124, rowH);
        u8g2.setDrawColor(1);
      }
    }
  } else {

    u8g2.setFont(u8g2_font_6x12_tr);
    char fStr[32];
    snprintf(fStr, sizeof(fStr), "Freq: %.2f MHz", (float)s_currentItem.frequency / 1000000.0f);
    u8g2.drawStr(6, 27, fStr);

    char pStr[32];
    snprintf(pStr, sizeof(pStr), "Prot: %s (%db)", s_currentItem.protocol.c_str(), s_currentItem.bits);
    u8g2.drawStr(6, 39, pStr);


    int btnW = 54;
    int btnH = 12;
    int yBtn = 49;

    for (int i = 0; i < 2; i++) {
      int xBtn = 8 + i * 60;
      u8g2.setFont(u8g2_font_5x8_tr);
      int tw = u8g2.getStrWidth(kActions[i]);
      u8g2.drawStr(xBtn + (btnW - tw) / 2, yBtn + 9, kActions[i]);

      if (i == s_actionSel) {
        u8g2.setDrawColor(2);
        u8g2.drawBox(xBtn, yBtn, btnW, btnH);
        u8g2.setDrawColor(1);
      }
    }
  }


  u8g2.setDrawColor(0);
  u8g2.drawBox(0, 0, 128, 14);
  u8g2.setDrawColor(1);
  u8g2.drawBox(0, 0, 128, 14);
  u8g2.setDrawColor(0);
  u8g2.setFont(u8g2_font_7x14B_tr);

  if (s_txUntil > 0) {
    u8g2.drawStr(4, 11, "TX >>>");
  } else if (s_mode == MODE_DETAILS) {
    u8g2.drawStr(4, 11, s_currentItem.name.c_str());
  } else {
    u8g2.drawStr(4, 11, "SAVED SUB");
  }

  if (s_mode == MODE_LIST) {
    u8g2.setFont(u8g2_font_5x8_tr);
    char cnt[16];
    snprintf(cnt, sizeof(cnt), "%d files", (int)s_files.size());
    u8g2.drawStr(124 - u8g2.getStrWidth(cnt), 10, cnt);
  }
  u8g2.setDrawColor(1);
}

}
