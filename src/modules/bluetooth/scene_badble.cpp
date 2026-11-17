#include "scene_badble.h"
#include "ble_hid.h"
#include "core/ble_service.h"
#include "core/vfs_mgr.h"
#include <vector>
#include <math.h>

namespace scene_badble {

enum BadBleState {
  STATE_LIST = 0,
  STATE_RUNNER
};

enum RunStatus {
  STATUS_IDLE = 0,
  STATUS_RUNNING,
  STATUS_PAUSED,
  STATUS_FINISHED
};

static BadBleState s_state = STATE_LIST;
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

static uint8_t parseDuckyKey(const String& k) {
  if (k.length() == 1) {
    char c = k.charAt(0);
    if (c >= 'a' && c <= 'z') return 0x04 + (c - 'a');
    if (c >= 'A' && c <= 'Z') return 0x04 + (c - 'A');
    if (c >= '1' && c <= '9') return 0x1E + (c - '1');
    if (c == '0') return 0x27;
    if (c == ' ') return ble_hid::KEY_SPACE;
  }
  String u = k;
  u.toUpperCase();
  if (u == "ENTER" || u == "RETURN") return ble_hid::KEY_ENTER;
  if (u == "ESC" || u == "ESCAPE") return ble_hid::KEY_ESC;
  if (u == "BACKSPACE") return ble_hid::KEY_BACKSPACE;
  if (u == "TAB") return ble_hid::KEY_TAB;
  if (u == "SPACE") return ble_hid::KEY_SPACE;
  if (u == "DELETE" || u == "DEL") return 0x4C;
  if (u == "UP" || u == "UPARROW") return ble_hid::KEY_UP_ARROW;
  if (u == "DOWN" || u == "DOWNARROW") return ble_hid::KEY_DOWN_ARROW;
  if (u == "LEFT" || u == "LEFTARROW") return ble_hid::KEY_LEFT_ARROW;
  if (u == "RIGHT" || u == "RIGHTARROW") return ble_hid::KEY_RIGHT_ARROW;
  if (u == "PRINTSCREEN") return ble_hid::KEY_PRINTSCREEN;
  if (u == "CAPSLOCK") return 0x39;
  if (u.startsWith("F") && u.length() <= 3) {
    int fNum = u.substring(1).toInt();
    if (fNum >= 1 && fNum <= 12) return (uint8_t)(0x3A + (fNum - 1));
  }
  return 0;
}

static void executeLine(const String& line) {
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
    for (size_t i = 0; i < arg.length(); i++) {
      ble_hid::sendChar(arg.charAt(i));
    }
    s_delayUntil = millis() + s_defaultDelay;
    return;
  }
  if (cmd == "ENTER") {
    ble_hid::sendKey(0, ble_hid::KEY_ENTER);
    s_delayUntil = millis() + s_defaultDelay;
    return;
  }
  if (cmd == "TAB") {
    ble_hid::sendKey(0, ble_hid::KEY_TAB);
    s_delayUntil = millis() + s_defaultDelay;
    return;
  }
  if (cmd == "ESC" || cmd == "ESCAPE") {
    ble_hid::sendKey(0, ble_hid::KEY_ESC);
    s_delayUntil = millis() + s_defaultDelay;
    return;
  }
  if (cmd == "SPACE") {
    ble_hid::sendKey(0, ble_hid::KEY_SPACE);
    s_delayUntil = millis() + s_defaultDelay;
    return;
  }
  if (cmd == "UP" || cmd == "UPARROW") {
    ble_hid::sendKey(0, ble_hid::KEY_UP_ARROW);
    s_delayUntil = millis() + s_defaultDelay;
    return;
  }
  if (cmd == "DOWN" || cmd == "DOWNARROW") {
    ble_hid::sendKey(0, ble_hid::KEY_DOWN_ARROW);
    s_delayUntil = millis() + s_defaultDelay;
    return;
  }
  if (cmd == "LEFT" || cmd == "LEFTARROW") {
    ble_hid::sendKey(0, ble_hid::KEY_LEFT_ARROW);
    s_delayUntil = millis() + s_defaultDelay;
    return;
  }
  if (cmd == "RIGHT" || cmd == "RIGHTARROW") {
    ble_hid::sendKey(0, ble_hid::KEY_RIGHT_ARROW);
    s_delayUntil = millis() + s_defaultDelay;
    return;
  }


  uint8_t mod = 0;
  String remaining = line;
  while (true) {
    int nextSp = remaining.indexOf(' ');
    String token = (nextSp >= 0) ? remaining.substring(0, nextSp) : remaining;
    token.toUpperCase();

    if (token == "GUI" || token == "WINDOWS") {
      mod |= 0x08;
    } else if (token == "CTRL" || token == "CONTROL") {
      mod |= 0x01;
    } else if (token == "ALT") {
      mod |= 0x04;
    } else if (token == "SHIFT") {
      mod |= 0x02;
    } else {

      uint8_t key = parseDuckyKey(token);
      if (key != 0 || mod != 0) {
        ble_hid::sendKey(mod, key);
        s_delayUntil = millis() + s_defaultDelay;
      }
      return;
    }

    if (nextSp < 0) {
      if (mod != 0) {
        ble_hid::sendKey(mod, 0);
        s_delayUntil = millis() + s_defaultDelay;
      }
      return;
    }
    remaining = remaining.substring(nextSp + 1);
  }
}

void open() {
  s_state = STATE_LIST;
  s_status = STATUS_IDLE;
  s_sel = 0;
  s_scroll = 0.0f;
  s_targetScroll = 0.0f;
  s_barY = (float)startY;
  s_lineIdx = 0;
  s_delayUntil = 0;
  s_actionSel = 0;

  ble_service::resumeAdvertising();
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
    if (s_sel < topIndex) s_targetScroll = (float)(s_sel * rowH);
    else if (s_sel >= topIndex + maxLines) s_targetScroll = (float)((s_sel - maxLines + 1) * rowH);

    if (s_targetScroll < 0.0f) s_targetScroll = 0.0f;
    if (s_targetScroll > (float)(maxScroll * rowH)) s_targetScroll = (float)(maxScroll * rowH);

    float ds = s_targetScroll - s_scroll;
    if (fabsf(ds) > 0.1f) s_scroll += ds * 0.35f;
    else s_scroll = s_targetScroll;

    float targetBarY = (float)startY + (float)(s_sel * rowH) - s_scroll;
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
    int count = (int)s_scripts.size();
    if (count == 0) return;

    if (ev.key == BTN_UP) {
      s_sel = (s_sel - 1 + count) % count;
    } else if (ev.key == BTN_DOWN) {
      s_sel = (s_sel + 1) % count;
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

  if (!ble_service::isConnected() && !ble_hid::isConnected()) {
    u8g2.setDrawColor(1);
    u8g2.drawBox(0, 0, 128, 14);
    u8g2.setDrawColor(0);
    u8g2.setFont(u8g2_font_7x14B_tr);
    u8g2.drawStr(4, 11, "BADBLE");
    u8g2.setDrawColor(1);

    u8g2.setFont(u8g2_font_6x12_tr);
    int dots = (now / 350) % 4;
    char waitBuf[24];
    snprintf(waitBuf, sizeof(waitBuf), "Waiting pairing%.*s", dots, "...");
    int tw = u8g2.getStrWidth(waitBuf);
    u8g2.drawStr((128 - tw) / 2, 36, waitBuf);

    u8g2.setFont(u8g2_font_5x8_tr);
    char nameBuf[32];
    snprintf(nameBuf, sizeof(nameBuf), "Device: %s", ble_service::getDeviceName());
    int nw = u8g2.getStrWidth(nameBuf);
    u8g2.drawStr((128 - nw) / 2, 50, nameBuf);
    return;
  }


  if (s_state == STATE_LIST) {
    if (s_scripts.empty()) {
      u8g2.setFont(u8g2_font_6x12_tr);
      u8g2.drawStr(12, 36, "No .txt scripts");
      u8g2.setFont(u8g2_font_5x8_tr);
      u8g2.drawStr(10, 48, "Put in /badusb on SD");
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
    int progW = ((int)s_lineIdx * 116) / total;
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
    u8g2.drawStr(4, 11, "BADBLE");
  } else {
    u8g2.drawStr(4, 11, s_curScriptName.c_str());
  }
  u8g2.setDrawColor(1);
}

}
