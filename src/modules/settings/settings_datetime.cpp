#include "settings_datetime.h"
#include "settings_data.h"
#include "core/sys_clock.h"
#include <math.h>

namespace settings_datetime {

enum EditMode {
  EDIT_NONE = 0,
  EDIT_TIME,
  EDIT_DATE
};

static EditMode s_mode = EDIT_NONE;
static int s_sel = 0;
static const int kCount = 4;
static const int rowH = 12;
static const int startY = 16;
static float s_barY = 16.0f;


static int s_editField = 0;
static int s_editH = 12, s_editM = 0, s_editS = 0;
static int s_editY = 2026, s_editMo = 10, s_editD = 3;

void open() {
  s_mode = EDIT_NONE;
  s_sel = 0;
  s_barY = startY;
  s_editField = 0;
}

void close() {
  s_mode = EDIT_NONE;
}

bool onBack() {
  if (s_mode != EDIT_NONE) {
    s_mode = EDIT_NONE;
    return true;
  }
  return false;
}

void tick(uint32_t now) {
  (void)now;
  if (s_mode == EDIT_NONE) {
    float target = startY + s_sel * rowH;
    float dy = target - s_barY;
    if (fabsf(dy) > 0.1f) s_barY += dy * 0.35f;
    else s_barY = target;
  }
}

void onKey(const KeyEvent& ev) {
  if (ev.repeat && ev.key != BTN_UP && ev.key != BTN_DOWN) return;

  if (s_mode == EDIT_NONE) {
    if (ev.key == BTN_UP) {
      if (s_sel > 0) s_sel--;
      else s_sel = kCount - 1;
    } else if (ev.key == BTN_DOWN) {
      if (s_sel < kCount - 1) s_sel++;
      else s_sel = 0;
    } else if (ev.key == BTN_LEFT) {
      if (s_sel == 0) {
        settings_data::setBleSync(!settings_data::getBleSync());
      } else if (s_sel == 1) {
        int8_t tz = sys_clock::getTimezone();
        if (tz > -12) sys_clock::setTimezone(tz - 1);
      }
    } else if (ev.key == BTN_RIGHT) {
      if (s_sel == 0) {
        settings_data::setBleSync(!settings_data::getBleSync());
      } else if (s_sel == 1) {
        int8_t tz = sys_clock::getTimezone();
        if (tz < 14) sys_clock::setTimezone(tz + 1);
      }
    } else if (ev.key == BTN_OK && !ev.repeat) {
      if (s_sel == 0) {
        settings_data::setBleSync(!settings_data::getBleSync());
      } else if (s_sel == 1) {
        int8_t tz = sys_clock::getTimezone();
        sys_clock::setTimezone((tz >= 14) ? -12 : tz + 1);
      } else if (s_sel == 2) {

        int y, mo, d;
        sys_clock::getTime(y, mo, d, s_editH, s_editM, s_editS);
        s_editField = 0;
        s_mode = EDIT_TIME;
      } else if (s_sel == 3) {

        int h, m, s;
        sys_clock::getTime(s_editY, s_editMo, s_editD, h, m, s);
        s_editField = 0;
        s_mode = EDIT_DATE;
      }
    }
  } else if (s_mode == EDIT_TIME) {
    if (ev.key == BTN_LEFT && !ev.repeat) {
      s_editField = (s_editField > 0) ? s_editField - 1 : 2;
    } else if (ev.key == BTN_RIGHT && !ev.repeat) {
      s_editField = (s_editField < 2) ? s_editField + 1 : 0;
    } else if (ev.key == BTN_UP) {
      if (s_editField == 0) s_editH = (s_editH + 1) % 24;
      else if (s_editField == 1) s_editM = (s_editM + 1) % 60;
      else if (s_editField == 2) s_editS = (s_editS + 1) % 60;
    } else if (ev.key == BTN_DOWN) {
      if (s_editField == 0) s_editH = (s_editH > 0) ? s_editH - 1 : 23;
      else if (s_editField == 1) s_editM = (s_editM > 0) ? s_editM - 1 : 59;
      else if (s_editField == 2) s_editS = (s_editS > 0) ? s_editS - 1 : 59;
    } else if (ev.key == BTN_OK && !ev.repeat) {
      sys_clock::setTime(s_editH, s_editM, s_editS);
      s_mode = EDIT_NONE;
    }
  } else if (s_mode == EDIT_DATE) {
    if (ev.key == BTN_LEFT && !ev.repeat) {
      s_editField = (s_editField > 0) ? s_editField - 1 : 2;
    } else if (ev.key == BTN_RIGHT && !ev.repeat) {
      s_editField = (s_editField < 2) ? s_editField + 1 : 0;
    } else if (ev.key == BTN_UP) {
      if (s_editField == 0) s_editD = (s_editD >= 31) ? 1 : s_editD + 1;
      else if (s_editField == 1) s_editMo = (s_editMo >= 12) ? 1 : s_editMo + 1;
      else if (s_editField == 2) if (s_editY < 2099) s_editY++;
    } else if (ev.key == BTN_DOWN) {
      if (s_editField == 0) s_editD = (s_editD > 1) ? s_editD - 1 : 31;
      else if (s_editField == 1) s_editMo = (s_editMo > 1) ? s_editMo - 1 : 12;
      else if (s_editField == 2) if (s_editY > 2020) s_editY--;
    } else if (ev.key == BTN_OK && !ev.repeat) {
      sys_clock::setDate(s_editY, s_editMo, s_editD);
      s_mode = EDIT_NONE;
    }
  }
}

void draw(U8G2& u8g2, uint32_t now) {
  (void)now;
  if (s_mode == EDIT_NONE) {



    u8g2.setFont(u8g2_font_6x12_tr);
    u8g2.setDrawColor(1);


    int yr, mo, dy, hr, mn, sc;
    sys_clock::getTime(yr, mo, dy, hr, mn, sc);

    char buf[36];

    snprintf(buf, sizeof(buf), "BLE Sync    < %s >", settings_data::getBleSync() ? "ON" : "OFF");
    u8g2.drawStr(6, startY + 10, buf);


    int8_t tz = sys_clock::getTimezone();
    if (tz == 0) snprintf(buf, sizeof(buf), "Timezone    < UTC >");
    else snprintf(buf, sizeof(buf), "Timezone    < UTC%+d >", tz);
    u8g2.drawStr(6, startY + 10 + rowH, buf);


    snprintf(buf, sizeof(buf), "Set Time    %02d:%02d:%02d", hr, mn, sc);
    u8g2.drawStr(6, startY + 10 + 2 * rowH, buf);


    snprintf(buf, sizeof(buf), "Set Date  %02d/%02d/%04d", dy, mo, yr);
    u8g2.drawStr(6, startY + 10 + 3 * rowH, buf);


    int bY = (int)(s_barY + 0.5f);
    u8g2.setDrawColor(2);
    u8g2.drawBox(2, bY, 124, rowH);
    u8g2.setDrawColor(1);


    u8g2.setDrawColor(1);
    u8g2.drawBox(0, 0, 128, 14);
    u8g2.setDrawColor(0);
    u8g2.setFont(u8g2_font_7x14B_tr);
    u8g2.drawStr(4, 11, "DATETIME");
    u8g2.setDrawColor(1);

  } else if (s_mode == EDIT_TIME) {



    u8g2.setDrawColor(1);
    u8g2.drawBox(0, 0, 128, 14);
    u8g2.setDrawColor(0);
    u8g2.setFont(u8g2_font_7x14B_tr);
    u8g2.drawStr(4, 11, "SET TIME");
    u8g2.setDrawColor(1);

    char hBuf[4], mBuf[4], sBuf[4];
    snprintf(hBuf, sizeof(hBuf), "%02d", s_editH);
    snprintf(mBuf, sizeof(mBuf), "%02d", s_editM);
    snprintf(sBuf, sizeof(sBuf), "%02d", s_editS);

    u8g2.setFont(u8g2_font_7x14B_tr);
    u8g2.drawStr(12, 33, "Time:");


    if (s_editField == 0) u8g2.drawBox(54, 21, 20, 15);
    u8g2.setDrawColor(s_editField == 0 ? 0 : 1);
    u8g2.drawStr(56, 33, hBuf);
    u8g2.setDrawColor(1);

    u8g2.drawStr(76, 33, ":");


    if (s_editField == 1) u8g2.drawBox(82, 21, 20, 15);
    u8g2.setDrawColor(s_editField == 1 ? 0 : 1);
    u8g2.drawStr(84, 33, mBuf);
    u8g2.setDrawColor(1);

    u8g2.drawStr(104, 33, ":");


    if (s_editField == 2) u8g2.drawBox(110, 21, 16, 15);
    u8g2.setDrawColor(s_editField == 2 ? 0 : 1);
    u8g2.drawStr(111, 33, sBuf);
    u8g2.setDrawColor(1);


    u8g2.drawHLine(4, 43, 120);
    u8g2.setFont(u8g2_font_4x6_tr);
    u8g2.drawStr(4, 52, "[< >] Move field   [^ v] Adjust");
    u8g2.drawStr(4, 60, "[OK] Save          [Back] Cancel");

  } else if (s_mode == EDIT_DATE) {



    u8g2.setDrawColor(1);
    u8g2.drawBox(0, 0, 128, 14);
    u8g2.setDrawColor(0);
    u8g2.setFont(u8g2_font_7x14B_tr);
    u8g2.drawStr(4, 11, "SET DATE");
    u8g2.setDrawColor(1);

    char dBuf[4], moBuf[4], yBuf[6];
    snprintf(dBuf, sizeof(dBuf), "%02d", s_editD);
    snprintf(moBuf, sizeof(moBuf), "%02d", s_editMo);
    snprintf(yBuf, sizeof(yBuf), "%04d", s_editY);

    u8g2.setFont(u8g2_font_7x14B_tr);
    u8g2.drawStr(8, 33, "Date:");


    if (s_editField == 0) u8g2.drawBox(48, 21, 20, 15);
    u8g2.setDrawColor(s_editField == 0 ? 0 : 1);
    u8g2.drawStr(50, 33, dBuf);
    u8g2.setDrawColor(1);

    u8g2.drawStr(70, 33, "/");


    if (s_editField == 1) u8g2.drawBox(76, 21, 20, 15);
    u8g2.setDrawColor(s_editField == 1 ? 0 : 1);
    u8g2.drawStr(78, 33, moBuf);
    u8g2.setDrawColor(1);

    u8g2.drawStr(98, 33, "/");


    if (s_editField == 2) u8g2.drawBox(102, 21, 25, 15);
    u8g2.setDrawColor(s_editField == 2 ? 0 : 1);
    u8g2.drawStr(103, 33, yBuf);
    u8g2.setDrawColor(1);


    u8g2.drawHLine(4, 43, 120);
    u8g2.setFont(u8g2_font_4x6_tr);
    u8g2.drawStr(4, 52, "[< >] Move field   [^ v] Adjust");
    u8g2.drawStr(4, 60, "[OK] Save          [Back] Cancel");
  }
}

}
