#include "scene_usb_mouse.h"
#include "usb_hid.h"

namespace scene_usb_mouse {

static int s_curX = 64;
static int s_curY = 38;
static int s_clickFeedbackUntil = 0;
static const char* s_feedbackText = nullptr;

static int s_mode = 0;
static const char* kModes[] = {"L-CLICK", "R-CLICK", "SCROLL"};
static const int kModeCount = 3;

void open() {
  usb_hid::init();
  s_curX = 64;
  s_curY = 38;
  s_clickFeedbackUntil = 0;
  s_feedbackText = nullptr;
  s_mode = 0;
}

void close() {}

void tick(uint32_t now) {
  if (s_feedbackText && (int)now >= s_clickFeedbackUntil) {
    s_feedbackText = nullptr;
  }
}

void onKey(const KeyEvent& ev) {
  int step = ev.repeat ? 14 : 4;

  if (s_mode == 2) {

    if (ev.key == BTN_UP) {
      usb_hid::getMouse().move(0, 0, 1);
      s_feedbackText = "WHEEL UP";
      s_clickFeedbackUntil = millis() + 300;
    } else if (ev.key == BTN_DOWN) {
      usb_hid::getMouse().move(0, 0, -1);
      s_feedbackText = "WHEEL DN";
      s_clickFeedbackUntil = millis() + 300;
    }
  } else {

    int dx = 0, dy = 0;
    if (ev.key == BTN_UP) dy = -step;
    else if (ev.key == BTN_DOWN) dy = step;
    else if (ev.key == BTN_LEFT) dx = -step;
    else if (ev.key == BTN_RIGHT) dx = step;

    if (dx != 0 || dy != 0) {
      usb_hid::getMouse().move(dx, dy);
      s_curX += dx / 2;
      s_curY += dy / 2;
      if (s_curX < 6) s_curX = 6;
      if (s_curX > 122) s_curX = 122;
      if (s_curY < 18) s_curY = 18;
      if (s_curY > 50) s_curY = 50;
    }
  }

  if (ev.key == BTN_OK && !ev.repeat) {
    if (s_mode == 0) {
      usb_hid::getMouse().click(MOUSE_LEFT);
      s_feedbackText = "L-CLICK";
      s_clickFeedbackUntil = millis() + 400;
    } else if (s_mode == 1) {
      usb_hid::getMouse().click(MOUSE_RIGHT);
      s_feedbackText = "R-CLICK";
      s_clickFeedbackUntil = millis() + 400;
    } else if (s_mode == 2) {
      usb_hid::getMouse().click(MOUSE_MIDDLE);
      s_feedbackText = "M-CLICK";
      s_clickFeedbackUntil = millis() + 400;
    }
  }
}

void draw(U8G2& u8g2, uint32_t now) {
  (void)now;

  u8g2.setDrawColor(0);
  u8g2.drawBox(0, 0, 128, 14);
  u8g2.setDrawColor(1);
  u8g2.drawBox(0, 0, 128, 14);
  u8g2.setDrawColor(0);
  u8g2.setFont(u8g2_font_7x14B_tr);
  u8g2.drawStr(4, 11, "USB MOUSE");

  u8g2.setFont(u8g2_font_5x8_tr);
  if (s_feedbackText) {
    u8g2.drawStr(124 - u8g2.getStrWidth(s_feedbackText), 10, s_feedbackText);
  } else {
    u8g2.drawStr(124 - u8g2.getStrWidth(kModes[s_mode]), 10, kModes[s_mode]);
  }
  u8g2.setDrawColor(1);


  u8g2.drawFrame(4, 16, 120, 36);


  u8g2.drawDisc(s_curX, s_curY, 2);
  u8g2.drawHLine(s_curX - 4, s_curY, 9);
  u8g2.drawVLine(s_curX, s_curY - 4, 9);


  u8g2.setFont(u8g2_font_5x8_tr);
  u8g2.drawStr(6, 61, "D-Pad: Move | OK: Click");
}

}
