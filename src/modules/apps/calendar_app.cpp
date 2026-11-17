#include "calendar_app.h"
#include "clock_app.h"

namespace calendar_app {

static int s_viewYear = 2026;
static int s_viewMonth = 10;

static int s_todayYear = 2026;
static int s_todayMonth = 10;
static int s_todayDay = 3;

static const char* kMonthNames[] = {
  "JANUARY", "FEBRUARY", "MARCH", "APRIL", "MAY", "JUNE",
  "JULY", "AUGUST", "SEPTEMBER", "OCTOBER", "NOVEMBER", "DECEMBER"
};


static int getDayOfWeek(int y, int m, int d) {
  static const int t[] = { 0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4 };
  if (m < 3) y -= 1;
  return (y + y/4 - y/100 + y/400 + t[m - 1] + d) % 7;
}

static int getDaysInMonth(int y, int m) {
  if (m == 2) {
    bool leap = (y % 4 == 0 && (y % 100 != 0 || y % 400 == 0));
    return leap ? 29 : 28;
  }
  if (m == 4 || m == 6 || m == 9 || m == 11) return 30;
  return 31;
}

void open() {
  int h, min, s;
  clock_app::getCurrentTime(s_todayYear, s_todayMonth, s_todayDay, h, min, s);
  s_viewYear = s_todayYear;
  s_viewMonth = s_todayMonth;
}

void close() {}

bool onBack() {
  return false;
}

void tick(uint32_t now) {}

void onKey(const KeyEvent& ev) {
  if (ev.repeat && ev.key != BTN_LEFT && ev.key != BTN_RIGHT) return;

  if (ev.key == BTN_LEFT) {

    if (s_viewMonth > 1) {
      s_viewMonth--;
    } else {
      s_viewMonth = 12;
      if (s_viewYear > 2000) s_viewYear--;
    }
  } else if (ev.key == BTN_RIGHT) {

    if (s_viewMonth < 12) {
      s_viewMonth++;
    } else {
      s_viewMonth = 1;
      if (s_viewYear < 2099) s_viewYear++;
    }
  } else if (ev.key == BTN_UP && !ev.repeat) {

    if (s_viewYear < 2099) s_viewYear++;
  } else if (ev.key == BTN_DOWN && !ev.repeat) {

    if (s_viewYear > 2000) s_viewYear--;
  } else if (ev.key == BTN_OK && !ev.repeat) {

    s_viewYear = s_todayYear;
    s_viewMonth = s_todayMonth;
  }
}

void draw(U8G2& u8g2, uint32_t now) {



  u8g2.setDrawColor(1);
  u8g2.drawBox(0, 0, 128, 12);
  u8g2.setDrawColor(0);
  u8g2.setFont(u8g2_font_6x12_tr);

  char titleBuf[32];
  snprintf(titleBuf, sizeof(titleBuf), "< %s %d >", kMonthNames[s_viewMonth - 1], s_viewYear);
  int tw = u8g2.getStrWidth(titleBuf);
  u8g2.drawStr((128 - tw) / 2, 10, titleBuf);
  u8g2.setDrawColor(1);




  static const char* kWeekdays[] = { "Mo", "Tu", "We", "Th", "Fr", "Sa", "Su" };
  static const int kColX[] = { 4, 22, 40, 58, 76, 94, 112 };

  u8g2.setFont(u8g2_font_5x7_tr);
  for (int i = 0; i < 7; i++) {

    u8g2.drawStr(kColX[i], 21, kWeekdays[i]);
  }


  u8g2.drawHLine(2, 23, 124);




  int firstDow = getDayOfWeek(s_viewYear, s_viewMonth, 1);

  int startCol = (firstDow == 0) ? 6 : (firstDow - 1);
  int totalDays = getDaysInMonth(s_viewYear, s_viewMonth);

  u8g2.setFont(u8g2_font_5x7_tr);

  int curCol = startCol;
  int curRow = 0;

  for (int day = 1; day <= totalDays; day++) {
    int x = kColX[curCol];
    int y = 31 + curRow * 8;

    bool isToday = (s_viewYear == s_todayYear && s_viewMonth == s_todayMonth && day == s_todayDay);

    char dStr[4];
    snprintf(dStr, sizeof(dStr), "%d", day);
    int dw = u8g2.getStrWidth(dStr);

    if (isToday) {

      u8g2.drawBox(x - 2, y - 7, 14, 8);
      u8g2.setDrawColor(0);
      u8g2.drawStr(x + (8 - dw) / 2, y, dStr);
      u8g2.setDrawColor(1);
    } else {
      u8g2.drawStr(x + (8 - dw) / 2, y, dStr);
    }

    curCol++;
    if (curCol > 6) {
      curCol = 0;
      curRow++;
    }
  }
}

}
