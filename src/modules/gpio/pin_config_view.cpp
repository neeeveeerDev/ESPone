#include "pin_config_view.h"
#include "gpio_pins.h"

namespace pin_config_view {

static const int kItemCount = gpio_pins::PIN_COUNT + 2;
static const int rowH = 12;
static const int startY = 15;

static int s_selected = 0;
static float s_scroll = 0.0f;
static float s_targetScroll = 0.0f;
static float s_barY = 15.0f;
static uint32_t s_savedUntil = 0;

void open() {
  s_selected = 0;
  s_scroll = 0.0f;
  s_targetScroll = 0.0f;
  s_barY = startY;
  s_savedUntil = 0;
}

void close() {
  gpio_pins::save();
}

static void changePin(int pinIdx, int delta) {
  int8_t cur = gpio_pins::getPin((gpio_pins::CasePinIndex)pinIdx);
  int next = cur + delta;
  if (next < 0) next = 48;
  if (next > 48) next = 0;
  gpio_pins::setPin((gpio_pins::CasePinIndex)pinIdx, (int8_t)next);
}

void tick(uint32_t now) {
  (void)now;

  if (s_selected < s_targetScroll) {
    s_targetScroll = s_selected;
  } else if (s_selected >= s_targetScroll + 4) {
    s_targetScroll = s_selected - 3;
  }

  float dScroll = s_targetScroll - s_scroll;
  if (fabsf(dScroll) > 0.02f) {
    s_scroll += dScroll * 0.35f;
  } else {
    s_scroll = s_targetScroll;
  }


  float targetBar = startY + (s_selected - s_scroll) * rowH;
  float dBar = targetBar - s_barY;
  if (fabsf(dBar) > 0.1f) {
    s_barY += dBar * 0.35f;
  } else {
    s_barY = targetBar;
  }
}

void onKey(const KeyEvent& ev) {
  if (ev.key == BTN_UP) {
    if (s_selected > 0) s_selected--;
    else s_selected = kItemCount - 1;
  } else if (ev.key == BTN_DOWN) {
    if (s_selected < kItemCount - 1) s_selected++;
    else s_selected = 0;
  } else if (ev.key == BTN_LEFT) {
    if (s_selected < gpio_pins::PIN_COUNT) {
      changePin(s_selected, -1);
    }
  } else if (ev.key == BTN_RIGHT) {
    if (s_selected < gpio_pins::PIN_COUNT) {
      changePin(s_selected, +1);
    }
  } else if (ev.key == BTN_OK && !ev.repeat) {
    if (s_selected < gpio_pins::PIN_COUNT) {
      changePin(s_selected, +1);
    } else if (s_selected == gpio_pins::PIN_COUNT) {
      gpio_pins::save();
      s_savedUntil = millis() + 1500;
    } else if (s_selected == gpio_pins::PIN_COUNT + 1) {
      gpio_pins::resetDefaults();
      s_savedUntil = millis() + 1500;
    }
  }
}

void draw(U8G2& u8g2, uint32_t now) {

  u8g2.setFont(u8g2_font_6x12_tr);
  u8g2.setDrawColor(1);

  for (int i = 0; i < kItemCount; i++) {
    int y = startY + (int)((i - s_scroll) * rowH + 0.5f);
    if (y < startY - rowH || y > 64) continue;

    char buf[32];
    if (i < gpio_pins::PIN_COUNT) {
      const char* label = gpio_pins::getLabel((gpio_pins::CasePinIndex)i);
      int8_t gpio = gpio_pins::getPin((gpio_pins::CasePinIndex)i);
      snprintf(buf, sizeof(buf), "Pin %-1s : < GPIO %02d >", label, gpio);
    } else if (i == gpio_pins::PIN_COUNT) {
      snprintf(buf, sizeof(buf), "Save to NVS");
    } else {
      snprintf(buf, sizeof(buf), "Reset Defaults");
    }

    u8g2.drawStr(6, y + 10, buf);
  }


  int bY = (int)(s_barY + 0.5f);
  if (bY >= startY - 2 && bY <= 64 - rowH + 2) {
    u8g2.setDrawColor(2);
    u8g2.drawBox(2, bY, 124, rowH);
    u8g2.setDrawColor(1);
  }


  u8g2.setDrawColor(0);
  u8g2.drawBox(0, 0, 128, 14);
  u8g2.setDrawColor(1);
  u8g2.drawBox(0, 0, 128, 14);
  u8g2.setDrawColor(0);
  u8g2.setFont(u8g2_font_7x14B_tr);
  u8g2.drawStr(4, 11, "GPIO");
  if (now < s_savedUntil) {
    u8g2.drawStr(88, 11, "SAVED");
  }
  u8g2.setDrawColor(1);
}

}
