#include "keyboard.h"
#include <string.h>

namespace ui_keyboard {

static bool s_isOpen = false;
static bool s_modal = false;
static Callback s_callback = nullptr;

static char s_title[24] = "KEYBOARD";
static bool s_isFile = false;
static char s_buffer[48] = "";
static size_t s_bufLen = 0;

static const char* kExtensions[] = {
  ".txt", ".ir", ".sub", ".nfc", ".bin", ".hex", ".py", ".js", ".json", ".csv", ".log", ""
};
static const int kExtCount = sizeof(kExtensions) / sizeof(kExtensions[0]);
static int s_extIdx = 0;

enum KbMode {
  MODE_LOWER = 0,
  MODE_UPPER,
  MODE_SYMBOLS
};
static KbMode s_mode = MODE_LOWER;


static const char* kRow0_lower = "qwertyuiop";
static const char* kRow1_lower = "asdfghjkl-";
static const char* kRow2_lower = "^zxcvbnm.";

static const char* kRow0_upper = "QWERTYUIOP";
static const char* kRow1_upper = "ASDFGHJKL_";
static const char* kRow2_upper = "^ZXCVBNM/";

static const char* kRow0_sym = "1234567890";
static const char* kRow1_sym = "!@#$%^&*()";
static const char* kRow2_sym = "~+=[{]}:;?";

static int s_curRow = 0;
static int s_curCol = 0;

void openManual(const char* title, const char* defaultText, bool isFile) {
  snprintf(s_title, sizeof(s_title), "%s", title ? title : (isFile ? "NEW FILE" : "KEYBOARD"));
  s_isFile = isFile;
  s_extIdx = 0;
  s_mode = MODE_LOWER;
  s_curRow = 0;
  s_curCol = 0;
  s_isOpen = true;
  s_modal = false;
  s_callback = nullptr;

  if (defaultText && strlen(defaultText) > 0) {
    snprintf(s_buffer, sizeof(s_buffer), "%s", defaultText);
    s_bufLen = strlen(s_buffer);
  } else {
    s_buffer[0] = '\0';
    s_bufLen = 0;
  }
}

void open(const char* title, Callback cb) {
  open(title, "", false, cb);
}

void open(const char* title, const char* defaultText, Callback cb) {
  open(title, defaultText, false, cb);
}

void open(const char* title, const char* defaultText, bool isFile, Callback cb) {
  openManual(title, defaultText, isFile);
  s_callback = cb;
  s_modal = (cb != nullptr);
}

bool isOpen() {
  return s_isOpen;
}

bool isModal() {
  return s_isOpen && s_modal;
}

void close() {
  s_isOpen = false;
  s_modal = false;
  s_callback = nullptr;
}

void tick(uint32_t now) {
  (void)now;
}

String getText() {
  String res = String(s_buffer);
  if (s_isFile && strlen(kExtensions[s_extIdx]) > 0) {
    res += kExtensions[s_extIdx];
  }
  return res;
}

static int getRowColCount(int row) {
  if (row == -1) return 1;
  if (row == 0) return 10;
  if (row == 1) return 10;
  if (row == 2) return 9;
  if (row == 3) return 4;
  return 0;
}

Result onKey(const KeyEvent& ev) {
  if (!s_isOpen) return NONE;

  if (ev.key == BTN_BACK) {
    String res = getText();
    if (s_modal) {
      Callback cb = s_callback;
      close();
      if (cb) cb(res, false);
    } else {
      close();
    }
    return CANCEL;
  }

  if (ev.key == BTN_UP) {
    if (s_curRow > 0) {
      s_curRow--;
      int maxCols = getRowColCount(s_curRow);
      if (s_curCol >= maxCols) s_curCol = maxCols - 1;
    } else if (s_curRow == 0 && s_isFile) {
      s_curRow = -1;
    }
  } else if (ev.key == BTN_DOWN) {
    if (s_curRow < 3) {
      s_curRow++;
      int maxCols = getRowColCount(s_curRow);
      if (s_curCol >= maxCols) s_curCol = maxCols - 1;
    }
  } else if (ev.key == BTN_LEFT) {
    if (s_curRow == -1) {
      if (s_extIdx > 0) s_extIdx--;
      else s_extIdx = kExtCount - 1;
    } else {
      int maxCols = getRowColCount(s_curRow);
      if (s_curCol > 0) s_curCol--;
      else s_curCol = maxCols - 1;
    }
  } else if (ev.key == BTN_RIGHT) {
    if (s_curRow == -1) {
      if (s_extIdx < kExtCount - 1) s_extIdx++;
      else s_extIdx = 0;
    } else {
      int maxCols = getRowColCount(s_curRow);
      if (s_curCol < maxCols - 1) s_curCol++;
      else s_curCol = 0;
    }
  } else if (ev.key == BTN_OK && !ev.repeat) {
    if (s_curRow == -1) {

      if (s_extIdx < kExtCount - 1) s_extIdx++;
      else s_extIdx = 0;
      return NONE;
    }

    if (s_curRow == 3) {

      if (s_curCol == 0) {

        if (s_mode == MODE_SYMBOLS) s_mode = MODE_LOWER;
        else s_mode = MODE_SYMBOLS;
      } else if (s_curCol == 1) {

        if (s_bufLen < sizeof(s_buffer) - 1) {
          s_buffer[s_bufLen++] = ' ';
          s_buffer[s_bufLen] = '\0';
        }
      } else if (s_curCol == 2) {

        if (s_bufLen > 0) {
          s_buffer[--s_bufLen] = '\0';
        }
      } else if (s_curCol == 3) {

        String res = getText();
        if (s_modal) {
          Callback cb = s_callback;
          close();
          if (cb) cb(res, true);
        } else {
          close();
        }
        return DONE;
      }
    } else {

      char ch = 0;
      if (s_mode == MODE_LOWER) {
        if (s_curRow == 0) ch = kRow0_lower[s_curCol];
        else if (s_curRow == 1) ch = kRow1_lower[s_curCol];
        else if (s_curRow == 2) ch = kRow2_lower[s_curCol];
      } else if (s_mode == MODE_UPPER) {
        if (s_curRow == 0) ch = kRow0_upper[s_curCol];
        else if (s_curRow == 1) ch = kRow1_upper[s_curCol];
        else if (s_curRow == 2) ch = kRow2_upper[s_curCol];
      } else {
        if (s_curRow == 0) ch = kRow0_sym[s_curCol];
        else if (s_curRow == 1) ch = kRow1_sym[s_curCol];
        else if (s_curRow == 2) ch = kRow2_sym[s_curCol];
      }

      if (ch == '^') {

        if (s_mode == MODE_LOWER) s_mode = MODE_UPPER;
        else if (s_mode == MODE_UPPER) s_mode = MODE_LOWER;
      } else if (ch != 0) {
        if (s_bufLen < sizeof(s_buffer) - 1) {
          s_buffer[s_bufLen++] = ch;
          s_buffer[s_bufLen] = '\0';
        }
      }
    }
  }

  return NONE;
}

void draw(U8G2& u8g2, uint32_t now) {
  if (!s_isOpen) return;


  u8g2.setDrawColor(1);
  u8g2.drawBox(0, 0, 128, 12);
  u8g2.setDrawColor(0);
  u8g2.setFont(u8g2_font_6x12_tr);
  u8g2.drawStr(4, 10, s_title);
  u8g2.setDrawColor(1);


  u8g2.setFont(u8g2_font_5x8_tr);
  char dispBuf[64];
  snprintf(dispBuf, sizeof(dispBuf), "> %s", s_buffer);

  int maxW = s_isFile ? 85 : 124;
  const char* pDisp = dispBuf;
  while (*pDisp && u8g2.getStrWidth(pDisp) > maxW) {
    pDisp++;
  }
  u8g2.drawStr(2, 21, pDisp);


  if ((now / 400) % 2 == 0) {
    int curX = 2 + u8g2.getStrWidth(pDisp);
    u8g2.drawHLine(curX, 22, 5);
  }


  if (s_isFile) {
    const char* extStr = strlen(kExtensions[s_extIdx]) > 0 ? kExtensions[s_extIdx] : ".none";
    int ew = u8g2.getStrWidth(extStr);
    int exX = 126 - ew - 4;
    u8g2.drawFrame(exX, 13, ew + 4, 10);
    u8g2.drawStr(exX + 2, 21, extStr);

    if (s_curRow == -1) {

      u8g2.setDrawColor(2);
      u8g2.drawBox(exX, 13, ew + 4, 10);
      u8g2.setDrawColor(1);
    }
  }


  const char* r0 = (s_mode == MODE_LOWER) ? kRow0_lower : ((s_mode == MODE_UPPER) ? kRow0_upper : kRow0_sym);
  const char* r1 = (s_mode == MODE_LOWER) ? kRow1_lower : ((s_mode == MODE_UPPER) ? kRow1_upper : kRow1_sym);
  const char* r2 = (s_mode == MODE_LOWER) ? kRow2_lower : ((s_mode == MODE_UPPER) ? kRow2_upper : kRow2_sym);


  for (int i = 0; i < 10; i++) {
    char c[2] = { r0[i], '\0' };
    u8g2.drawStr(4 + i * 12 + 3, 32, c);
  }

  for (int i = 0; i < 10; i++) {
    char c[2] = { r1[i], '\0' };
    u8g2.drawStr(4 + i * 12 + 3, 41, c);
  }

  for (int i = 0; i < 9; i++) {
    char c[2] = { r2[i], '\0' };
    u8g2.drawStr(5 + i * 13 + 4, 50, c);
  }


  const char* fn0 = (s_mode == MODE_SYMBOLS) ? "ABC" : "123#";
  u8g2.drawStr(4, 60, fn0);
  u8g2.drawStr(40, 60, "SPACE");
  u8g2.drawStr(74, 60, "DEL");
  u8g2.drawStr(105, 60, "OK");


  if (s_curRow == 0) {
    u8g2.setDrawColor(2);
    u8g2.drawBox(4 + s_curCol * 12, 24, 11, 9);
    u8g2.setDrawColor(1);
  } else if (s_curRow == 1) {
    u8g2.setDrawColor(2);
    u8g2.drawBox(4 + s_curCol * 12, 33, 11, 9);
    u8g2.setDrawColor(1);
  } else if (s_curRow == 2) {
    u8g2.setDrawColor(2);
    u8g2.drawBox(5 + s_curCol * 13, 42, 12, 9);
    u8g2.setDrawColor(1);
  } else if (s_curRow == 3) {
    u8g2.setDrawColor(2);
    if (s_curCol == 0) u8g2.drawBox(2, 52, 28, 10);
    else if (s_curCol == 1) u8g2.drawBox(33, 52, 36, 10);
    else if (s_curCol == 2) u8g2.drawBox(71, 52, 28, 10);
    else if (s_curCol == 3) u8g2.drawBox(101, 52, 25, 10);
    u8g2.setDrawColor(1);
  }
}

}
