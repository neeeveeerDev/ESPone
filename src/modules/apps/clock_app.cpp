#include "clock_app.h"
#include "core/sys_clock.h"
#include <time.h>

namespace clock_app {

static const char* kDayNames[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
static const char* kMonthNames[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

void getCurrentTime(int& year, int& month, int& day, int& hour, int& min, int& sec) {
  sys_clock::getTime(year, month, day, hour, min, sec);
}

void open() {
  sys_clock::init();
}

void close() {}

bool onBack() {
  return false;
}

void tick(uint32_t now) {
  (void)now;
}

void onKey(const KeyEvent& ev) {
  (void)ev;
}

void draw(U8G2& u8g2, uint32_t now) {
  (void)now;

  u8g2.setDrawColor(1);
  u8g2.drawBox(0, 0, 128, 13);
  u8g2.setDrawColor(0);
  u8g2.setFont(u8g2_font_7x14B_tr);
  u8g2.drawStr(3, 11, "CLOCK");
  u8g2.setDrawColor(1);

  time_t cur = time(nullptr);
  struct tm ti;
  localtime_r(&cur, &ti);


  char timeStr[16];
  snprintf(timeStr, sizeof(timeStr), "%02d:%02d:%02d", ti.tm_hour, ti.tm_min, ti.tm_sec);
  u8g2.setFont(u8g2_font_10x20_tr);
  int tw = u8g2.getStrWidth(timeStr);
  u8g2.drawStr((128 - tw) / 2, 36, timeStr);


  char dateStr[32];
  int wday = (ti.tm_wday >= 0 && ti.tm_wday < 7) ? ti.tm_wday : 0;
  int mon = (ti.tm_mon >= 0 && ti.tm_mon < 12) ? ti.tm_mon : 0;
  snprintf(dateStr, sizeof(dateStr), "%s, %02d %s %04d",
           kDayNames[wday], ti.tm_mday, kMonthNames[mon], ti.tm_year + 1900);

  u8g2.setFont(u8g2_font_6x12_tr);
  int dw = u8g2.getStrWidth(dateStr);
  u8g2.drawStr((128 - dw) / 2, 53, dateStr);
}

}
