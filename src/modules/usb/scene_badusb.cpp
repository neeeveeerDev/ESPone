#include "scene_badusb.h"
#include "core/vfs_mgr.h"
#include <vector>
#include <math.h>
#include "usb_hid.h"

#ifndef KEY_TAB
#define KEY_TAB 0xB3
#endif

namespace scene_badusb {

enum BadUsbState {
  STATE_LIST = 0,
  STATE_RUNNER
};

enum RunStatus {
  STATUS_IDLE = 0,
  STATUS_RUNNING,
  STATUS_PAUSED,
  STATUS_FINISHED
};

static BadUsbState s_state = STATE_LIST;
static RunStatus s_status = STATUS_IDLE;

static std::vector<String> s_scripts;
static int s_sel = 0;
static float s_scroll = 0.0f;
static float s_targetScroll = 0.0f;
static float s_barY = 16.0f;

static String s_curScriptPath = "";
static String s_curScriptName = "";
static std::vector<String> s_lines;
static size_t s_lineIdx = 0;
static uint32_t s_delayUntil = 0;
static uint32_t s_defaultDelay = 50;

static int s_actionSel = 0;
static const char* kActions[] = {"RUN", "STOP"};

static const int rowH = 12;
static const int startY = 16;
static const int visibleRows = 4;

static void scanScripts() {
  s_scripts.clear();
  std::vector<vfs::DirEntry> entries;
  if (vfs::listDir("/badusb", entries, true)) {
    for (const auto& entry : entries) {
      if (!entry.isDir && String(entry.name).endsWith(".txt")) {
        s_scripts.push_back(entry.isSd ? ("/sd/badusb/" + String(entry.name)) : ("/badusb/" + String(entry.name)));
      }
    }
  }
}

static bool loadScriptLines(const String& path) {
  s_lines.clear();
  File f = vfs::open(path, FILE_READ);
  if (!f) return false;

  while (f.available()) {
    String l = f.readStringUntil('\n');
    l.trim();
    if (l.length() > 0) {
      s_lines.push_back(l);
    }
  }
  f.close();
  return !s_lines.empty();
}

static void executeLine(const String& line) {
  auto& kb = usb_hid::getKeyboard();

  int sp = line.indexOf(' ');
  String cmd = (sp >= 0) ? line.substring(0, sp) : line;
  String arg = (sp >= 0) ? line.substring(sp + 1) : "";
  cmd.toUpperCase();

  if (cmd == "REM") {

    return;
  }
  if (cmd == "DELAY") {
    uint32_t ms = arg.toInt();
    s_delayUntil = millis() + (ms > 0 ? ms : 50);
    return;
  }
  if (cmd == "DEFAULT_DELAY" || cmd == "DEFAULTDELAY") {
    s_defaultDelay = arg.toInt();
    return;
  }
  if (cmd == "STRING") {
    kb.print(arg);
    s_delayUntil = millis() + s_defaultDelay;
    return;
  }
  if (cmd == "ENTER") {
    kb.write(KEY_RETURN);
    s_delayUntil = millis() + s_defaultDelay;
    return;
  }
  if (cmd == "GUI" || cmd == "WINDOWS") {
    if (arg.length() > 0) {
      char k = arg.charAt(0);
      kb.press(KEY_LEFT_GUI);
      kb.press(k);
      delay(30);
      kb.releaseAll();
    } else {
      kb.write(KEY_LEFT_GUI);
    }
    s_delayUntil = millis() + s_defaultDelay;
    return;
  }
  if (cmd == "CTRL" || cmd == "CONTROL") {
    if (arg.length() > 0) {
      char k = arg.charAt(0);
      kb.press(KEY_LEFT_CTRL);
      kb.press(k);
      delay(30);
      kb.releaseAll();
    } else {
      kb.write(KEY_LEFT_CTRL);
    }
    s_delayUntil = millis() + s_defaultDelay;
    return;
  }
  if (cmd == "ALT") {
    if (arg.length() > 0) {
      char k = arg.charAt(0);
      kb.press(KEY_LEFT_ALT);
      kb.press(k);
      delay(30);
      kb.releaseAll();
    } else {
      kb.write(KEY_LEFT_ALT);
    }
    s_delayUntil = millis() + s_defaultDelay;
    return;
  }
  if (cmd == "TAB") {
    kb.write(KEY_TAB);
    s_delayUntil = millis() + s_defaultDelay;
    return;
  }
  if (cmd == "ESC" || cmd == "ESCAPE") {
    kb.write(KEY_ESC);
    s_delayUntil = millis() + s_defaultDelay;
    return;
  }
  if (cmd == "SPACE") {
    kb.write(' ');
    s_delayUntil = millis() + s_defaultDelay;
    return;
  }
  if (cmd == "UP" || cmd == "UPARROW") {
    kb.write(KEY_UP_ARROW);
    s_delayUntil = millis() + s_defaultDelay;
    return;
  }
  if (cmd == "DOWN" || cmd == "DOWNARROW") {
    kb.write(KEY_DOWN_ARROW);
    s_delayUntil = millis() + s_defaultDelay;
    return;
  }
  if (cmd == "LEFT" || cmd == "LEFTARROW") {
    kb.write(KEY_LEFT_ARROW);
    s_delayUntil = millis() + s_defaultDelay;
    return;
  }
  if (cmd == "RIGHT" || cmd == "RIGHTARROW") {
    kb.write(KEY_RIGHT_ARROW);
    s_delayUntil = millis() + s_defaultDelay;
    return;
  }
}

void open() {
  usb_hid::init();
  s_state = STATE_LIST;
  s_status = STATUS_IDLE;
  s_sel = 0;
  s_scroll = 0.0f;
  s_targetScroll = 0.0f;
  s_barY = startY;
  s_lineIdx = 0;
  s_delayUntil = 0;
  s_actionSel = 0;

  scanScripts();
}

void close() {
  s_status = STATUS_IDLE;
  s_lines.clear();
  s_scripts.clear();
}

bool onBack() {
  if (s_state == STATE_RUNNER) {
    if (s_status == STATUS_RUNNING) {
      s_status = STATUS_PAUSED;
      return true;
    }
    s_state = STATE_LIST;
    s_status = STATUS_IDLE;
    return true;
  }
  return false;
}

void tick(uint32_t now) {
  if (s_state == STATE_LIST) {
    int maxLines = visibleRows;
    int maxScroll = (int)s_scripts.size() - maxLines;
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
  } else if (s_state == STATE_RUNNER) {
    if (s_status == STATUS_RUNNING) {
      if (now >= s_delayUntil) {
        if (s_lineIdx < s_lines.size()) {
          executeLine(s_lines[s_lineIdx]);
          s_lineIdx++;
        } else {
          s_status = STATUS_FINISHED;
        }
      }
    }
  }
}

void onKey(const KeyEvent& ev) {
  if (s_state == STATE_LIST) {
    int count = s_scripts.size();
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
      s_curScriptPath = s_scripts[s_sel];
      int ls = s_curScriptPath.lastIndexOf('/');
      s_curScriptName = (ls >= 0) ? s_curScriptPath.substring(ls + 1) : s_curScriptPath;

      if (loadScriptLines(s_curScriptPath)) {
        s_state = STATE_RUNNER;
        s_status = STATUS_IDLE;
        s_lineIdx = 0;
        s_delayUntil = 0;
        s_actionSel = 0;
      }
    }
  } else if (s_state == STATE_RUNNER) {
    if (ev.key == BTN_LEFT && !ev.repeat) {
      s_actionSel = 0;
    } else if (ev.key == BTN_RIGHT && !ev.repeat) {
      s_actionSel = 1;
    } else if (ev.key == BTN_OK && !ev.repeat) {
      if (s_actionSel == 0) {

        if (s_status == STATUS_RUNNING) {
          s_status = STATUS_PAUSED;
        } else if (s_status == STATUS_FINISHED) {
          s_lineIdx = 0;
          s_status = STATUS_RUNNING;
          s_delayUntil = millis() + 200;
        } else {
          s_status = STATUS_RUNNING;
          s_delayUntil = millis() + 200;
        }
      } else if (s_actionSel == 1) {

        s_status = STATUS_IDLE;
        s_lineIdx = 0;
      }
    }
  }
}

void draw(U8G2& u8g2, uint32_t now) {
  (void)now;

  if (s_state == STATE_LIST) {
    if (s_scripts.empty()) {
      u8g2.setFont(u8g2_font_6x12_tr);
      u8g2.drawStr(12, 36, "No .txt scripts");
    } else {
      u8g2.setFont(u8g2_font_6x12_tr);
      for (size_t i = 0; i < s_scripts.size(); i++) {
        int y = startY + (int)(i * rowH - s_scroll + 0.5f);
        if (y < 14 || y > 64) continue;

        String name = s_scripts[i];
        int ls = name.lastIndexOf('/');
        if (ls >= 0) name = name.substring(ls + 1);
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

    u8g2.setFont(u8g2_font_5x8_tr);


    char lineInfo[32];
    snprintf(lineInfo, sizeof(lineInfo), "Line: %d / %d", (int)s_lineIdx, (int)s_lines.size());
    u8g2.drawStr(6, 26, lineInfo);

    if (s_lineIdx > 0 && s_lineIdx <= s_lines.size()) {
      String preview = s_lines[s_lineIdx - 1];
      if (preview.length() > 22) preview = preview.substring(0, 22) + "...";
      u8g2.drawStr(6, 36, preview.c_str());
    } else {
      u8g2.drawStr(6, 36, "Ready to start");
    }


    int total = s_lines.empty() ? 1 : (int)s_lines.size();
    int progW = (s_lineIdx * 116) / total;
    u8g2.drawFrame(6, 40, 116, 6);
    if (progW > 0) {
      u8g2.drawBox(8, 42, progW, 2);
    }


    int btnW = 54;
    int btnH = 12;
    int yBtn = 49;

    const char* runLabel = (s_status == STATUS_RUNNING) ? "PAUSE" : "RUN";

    u8g2.drawStr(24, yBtn + 9, runLabel);
    if (s_actionSel == 0) {
      u8g2.setDrawColor(2);
      u8g2.drawBox(8, yBtn, btnW, btnH);
      u8g2.setDrawColor(1);
    }

    u8g2.drawStr(84, yBtn + 9, "STOP");
    if (s_actionSel == 1) {
      u8g2.setDrawColor(2);
      u8g2.drawBox(68, yBtn, btnW, btnH);
      u8g2.setDrawColor(1);
    }
  }


  u8g2.setDrawColor(0);
  u8g2.drawBox(0, 0, 128, 14);
  u8g2.setDrawColor(1);
  u8g2.drawBox(0, 0, 128, 14);
  u8g2.setDrawColor(0);
  u8g2.setFont(u8g2_font_7x14B_tr);

  if (s_state == STATE_LIST) {
    u8g2.drawStr(4, 11, "BADUSB");
  } else {
    u8g2.drawStr(4, 11, s_curScriptName.c_str());
  }
  u8g2.setDrawColor(1);
}

}
