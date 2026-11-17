#include "calculator_app.h"
#include <math.h>

namespace calculator_app {

static const char* kGrid[4][5] = {
  { "7", "8", "9", "/", "DEL" },
  { "4", "5", "6", "*", "C"   },
  { "1", "2", "3", "-", "+/-" },
  { "0", ".", "=", "+", "ANS" }
};

static int s_curRow = 0;
static int s_curCol = 0;

static String s_currentStr = "0";
static String s_historyStr = "";
static double s_acc = 0.0;
static double s_lastFirst = 0.0;
static double s_lastAns = 0.0;
static char s_pendingOp = 0;
static bool s_startNewNumber = true;
static bool s_hasDecimal = false;
static bool s_isError = false;

static uint32_t s_pressTime = 0;
static int s_pressedRow = -1;
static int s_pressedCol = -1;

static String formatNumber(double val) {
  if (isnan(val) || isinf(val)) return "Error";
  if (fabs(val - round(val)) < 1e-9 && fabs(val) < 1e11) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%.0f", val);
    return String(buf);
  }
  char buf[32];
  snprintf(buf, sizeof(buf), "%.6g", val);
  return String(buf);
}

static void executePendingOp(double nextVal) {
  if (s_pendingOp == '+') s_acc += nextVal;
  else if (s_pendingOp == '-') s_acc -= nextVal;
  else if (s_pendingOp == '*') s_acc *= nextVal;
  else if (s_pendingOp == '/') {
    if (fabs(nextVal) < 1e-12) {
      s_isError = true;
      s_currentStr = "Error";
      s_historyStr = "Div by 0";
      return;
    }
    s_acc /= nextVal;
  }
}

void open() {
  s_curRow = 0;
  s_curCol = 0;
  s_currentStr = "0";
  s_historyStr = "";
  s_acc = 0.0;
  s_lastFirst = 0.0;
  s_pendingOp = 0;
  s_startNewNumber = true;
  s_hasDecimal = false;
  s_isError = false;
  s_pressedRow = -1;
  s_pressedCol = -1;
}

void close() {
}

bool onBack() {
  return false;
}

void tick(uint32_t now) {

}

static void onButtonAction(const char* label) {
  if (strcmp(label, "C") == 0) {
    s_currentStr = "0";
    s_acc = 0.0;
    s_lastFirst = 0.0;
    s_pendingOp = 0;
    s_historyStr = "";
    s_hasDecimal = false;
    s_startNewNumber = true;
    s_isError = false;
    return;
  }

  if (strcmp(label, "DEL") == 0) {
    if (s_isError) {
      s_isError = false;
      s_currentStr = "0";
      s_startNewNumber = true;
      return;
    }
    if (!s_startNewNumber && s_currentStr.length() > 0) {
      char lastChar = s_currentStr[s_currentStr.length() - 1];
      if (lastChar == '.') s_hasDecimal = false;
      s_currentStr.remove(s_currentStr.length() - 1);
      if (s_currentStr.length() == 0 || s_currentStr == "-") {
        s_currentStr = "0";
        s_startNewNumber = true;
      }
    } else {
      s_currentStr = "0";
      s_startNewNumber = true;
      s_hasDecimal = false;
    }
    return;
  }

  if (strcmp(label, "ANS") == 0) {
    if (s_isError) s_isError = false;
    s_currentStr = formatNumber(s_lastAns);
    s_startNewNumber = true;
    s_hasDecimal = (s_currentStr.indexOf('.') >= 0);
    return;
  }

  if (strcmp(label, "+/-") == 0) {
    if (s_isError) return;
    if (s_currentStr.startsWith("-")) {
      s_currentStr = s_currentStr.substring(1);
    } else if (s_currentStr != "0") {
      s_currentStr = "-" + s_currentStr;
    }
    return;
  }

  if (strcmp(label, ".") == 0) {
    if (s_isError) {
      s_isError = false;
      s_currentStr = "0";
      s_startNewNumber = true;
    }
    if (s_startNewNumber) {
      s_currentStr = "0.";
      s_startNewNumber = false;
      s_hasDecimal = true;
    } else if (!s_hasDecimal) {
      s_currentStr += ".";
      s_hasDecimal = true;
    }
    return;
  }

  if (strcmp(label, "+") == 0 || strcmp(label, "-") == 0 ||
      strcmp(label, "*") == 0 || strcmp(label, "/") == 0) {
    if (s_isError) return;
    double val = s_currentStr.toDouble();
    char op = label[0];

    if (s_pendingOp != 0 && !s_startNewNumber) {
      executePendingOp(val);
      if (s_isError) return;
      s_currentStr = formatNumber(s_acc);
    } else {
      s_acc = val;
    }
    s_lastFirst = s_acc;
    s_pendingOp = op;
    s_historyStr = formatNumber(s_acc) + " " + op;
    s_startNewNumber = true;
    s_hasDecimal = false;
    return;
  }

  if (strcmp(label, "=") == 0) {
    if (s_isError) return;
    if (s_pendingOp != 0) {
      double secondVal = s_currentStr.toDouble();
      executePendingOp(secondVal);
      if (!s_isError) {
        s_historyStr = formatNumber(s_lastFirst) + " " + s_pendingOp + " " + formatNumber(secondVal) + " =";
        s_lastAns = s_acc;
        s_currentStr = formatNumber(s_acc);
      }
      s_pendingOp = 0;
      s_startNewNumber = true;
      s_hasDecimal = (s_currentStr.indexOf('.') >= 0);
    }
    return;
  }


  if (label[0] >= '0' && label[0] <= '9') {
    if (s_isError) {
      s_isError = false;
      s_currentStr = "0";
      s_startNewNumber = true;
    }
    if (s_startNewNumber || s_currentStr == "0") {
      s_currentStr = String(label);
      s_startNewNumber = false;
      s_hasDecimal = false;
    } else {
      if (s_currentStr.length() < 12) {
        s_currentStr += label;
      }
    }
  }
}

void onKey(const KeyEvent& ev) {
  if (ev.key == BTN_UP) {
    s_curRow = (s_curRow > 0) ? s_curRow - 1 : 3;
  } else if (ev.key == BTN_DOWN) {
    s_curRow = (s_curRow < 3) ? s_curRow + 1 : 0;
  } else if (ev.key == BTN_LEFT) {
    s_curCol = (s_curCol > 0) ? s_curCol - 1 : 4;
  } else if (ev.key == BTN_RIGHT) {
    s_curCol = (s_curCol < 4) ? s_curCol + 1 : 0;
  } else if (ev.key == BTN_OK && !ev.repeat) {
    s_pressedRow = s_curRow;
    s_pressedCol = s_curCol;
    s_pressTime = millis();
    onButtonAction(kGrid[s_curRow][s_curCol]);
  }
}

void draw(U8G2& u8g2, uint32_t now) {

  u8g2.setDrawColor(1);
  u8g2.drawRFrame(2, 1, 124, 20, 2);


  u8g2.setFont(u8g2_font_5x7_tr);
  if (s_historyStr.length() > 0) {
    int hw = u8g2.getStrWidth(s_historyStr.c_str());
    int hx = 122 - hw;
    if (hx < 6) hx = 6;
    u8g2.drawStr(hx, 8, s_historyStr.c_str());
  }


  u8g2.setFont(u8g2_font_7x14B_tr);
  const char* valStr = s_currentStr.c_str();
  int vw = u8g2.getStrWidth(valStr);
  int vx = 122 - vw;
  if (vx < 6) vx = 6;
  u8g2.drawStr(vx, 18, valStr);




  static const int kColX[5] = { 2, 27, 52, 77, 102 };
  static const int kRowY[4] = { 23, 33, 43, 53 };
  const int kW = 23;
  const int kH = 9;

  u8g2.setFont(u8g2_font_5x7_tr);

  for (int r = 0; r < 4; r++) {
    for (int c = 0; c < 5; c++) {
      int x = kColX[c];
      int y = kRowY[r];
      const char* label = kGrid[r][c];

      bool isSelected = (r == s_curRow && c == s_curCol);
      bool isPressed = (r == s_pressedRow && c == s_pressedCol && (millis() - s_pressTime < 120));

      int tw = u8g2.getStrWidth(label);
      int tx = x + (kW - tw) / 2;
      int ty = y + 7;

      if (isSelected || isPressed) {

        u8g2.setDrawColor(1);
        u8g2.drawRBox(x, y, kW, kH, 1);
        u8g2.setDrawColor(0);
        u8g2.drawStr(tx, ty, label);
        u8g2.setDrawColor(1);
      } else {

        u8g2.setDrawColor(1);
        u8g2.drawRFrame(x, y, kW, kH, 1);
        u8g2.drawStr(tx, ty, label);
      }
    }
  }
}

}
