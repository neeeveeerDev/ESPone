#include "scene_mouse_jiggler.h"
#include "usb_hid.h"

namespace scene_mouse_jiggler {

static const int kIntervals[] = {5, 15, 30, 60};
static const int kIntervalCount = 4;
static int s_intervalIdx = 1;

static bool s_active = false;
static uint32_t s_lastJiggle = 0;
static uint32_t s_jiggleFlashUntil = 0;
static uint32_t s_jiggleCount = 0;

void open() {
  usb_hid::init();
  s_active = false;
  s_lastJiggle = millis();
  s_jiggleFlashUntil = 0;
  s_jiggleCount = 0;
}

void close() {
  s_active = false;
}

void tick(uint32_t now) {
  if (s_jiggleFlashUntil > 0 && now >= s_jiggleFlashUntil) {
    s_jiggleFlashUntil = 0;
  }

  if (s_active) {
    uint32_t intervalMs = kIntervals[s_intervalIdx] * 1000UL;
    if (now - s_lastJiggle >= intervalMs) {
      s_lastJiggle = now;
      s_jiggleCount++;
      s_jiggleFlashUntil = now + 400;


      int dir = (s_jiggleCount % 2 != 0) ? 2 : -2;
      usb_hid::getMouse().move(dir, 0);
    }
  }
}

void onKey(const KeyEvent& ev) {
  if (ev.key == BTN_UP || ev.key == BTN_LEFT) {
    if (!s_active) {
      s_intervalIdx = (s_intervalIdx - 1 + kIntervalCount) % kIntervalCount;
    }
  } else if (ev.key == BTN_DOWN || ev.key == BTN_RIGHT) {
    if (!s_active) {
      s_intervalIdx = (s_intervalIdx + 1) % kIntervalCount;
    }
  } else if (ev.key == BTN_OK && !ev.repeat) {
    s_active = !s_active;
    if (s_active) {
      s_lastJiggle = millis();
    }
  }
}

void draw(U8G2& u8g2, uint32_t now) {

  u8g2.setDrawColor(0);
  u8g2.drawBox(0, 0, 128, 14);
  u8g2.setDrawColor(1);
  u8g2.drawBox(0, 0, 128, 14);
  u8g2.setDrawColor(0);
  u8g2.setFont(u8g2_font_7x14B_tr);
  u8g2.drawStr(4, 11, "MOUSE JIGGLER");

  u8g2.setFont(u8g2_font_5x8_tr);
  if (s_jiggleFlashUntil > 0) {
    u8g2.drawStr(124 - u8g2.getStrWidth("JIGGLED!"), 10, "JIGGLED!");
  } else {
    u8g2.drawStr(124 - u8g2.getStrWidth(s_active ? "ACTIVE" : "STOPPED"), 10, s_active ? "ACTIVE" : "STOPPED");
  }
  u8g2.setDrawColor(1);


  u8g2.setFont(u8g2_font_6x12_tr);
  char intStr[32];
  snprintf(intStr, sizeof(intStr), "Interval: %d sec", kIntervals[s_intervalIdx]);
  u8g2.drawStr(6, 28, intStr);


  u8g2.setFont(u8g2_font_5x8_tr);
  if (s_active) {
    uint32_t intervalMs = kIntervals[s_intervalIdx] * 1000UL;
    uint32_t elapsed = now - s_lastJiggle;
    uint32_t remainMs = (elapsed < intervalMs) ? (intervalMs - elapsed) : 0;
    int remainSec = (remainMs + 999) / 1000;

    char cntStr[32];
    snprintf(cntStr, sizeof(cntStr), "Next nudge in: %ds  (#%lu)", remainSec, (unsigned long)s_jiggleCount);
    u8g2.drawStr(6, 40, cntStr);


    int progW = (elapsed * 116) / intervalMs;
    if (progW > 116) progW = 116;
    u8g2.drawFrame(6, 44, 116, 5);
    if (progW > 0) {
      u8g2.drawBox(8, 45, progW, 3);
    }
  } else {
    u8g2.drawStr(6, 40, "Press OK to activate");
  }


  int btnW = 64;
  int btnH = 11;
  int xBtn = (128 - btnW) / 2;
  int yBtn = 52;

  const char* btnText = s_active ? "STOP" : "START";
  int tw = u8g2.getStrWidth(btnText);
  u8g2.drawStr(xBtn + (btnW - tw) / 2, yBtn + 8, btnText);

  u8g2.setDrawColor(2);
  u8g2.drawBox(xBtn, yBtn, btnW, btnH);
  u8g2.setDrawColor(1);
}

}
