#include "status_bar.h"
#include "charging_icon.h"
#include "core/sys_clock.h"
#include "core/sys_storage.h"
#include "core/battery.h"
#include "core/ble_service.h"
#include "modules/bluetooth/ble_hid.h"
#include "config.h"

namespace status_bar {

// ============================================================
// Authentic Flipper Zero StatusBar assets from flipperzero-firmware-1.5.1
// assets/icons/StatusBar/
// ============================================================

// Battery_26x8.png (26x8)
static const uint8_t icon_Battery_26x8_bits[] PROGMEM = {
  0xfe, 0xff, 0x7f, 0x00, 0x01, 0x00, 0x80, 0x00, 0x01, 0x00, 0x80, 0x03, 0x01, 0x00, 0x80, 0x02,
  0x01, 0x00, 0x80, 0x02, 0x01, 0x00, 0x80, 0x03, 0x01, 0x00, 0x80, 0x00, 0xfe, 0xff, 0x7f, 0x00
};

// SDcardMounted_11x8.png (11x8 - "норм")
static const uint8_t icon_SDcardMounted_11x8_bits[] PROGMEM = {
  0xff, 0x07, 0xff, 0x04, 0xff, 0x07, 0xff, 0x04, 0xff, 0x07, 0xff, 0x04, 0xff, 0x07, 0x67, 0x00
};

// SDcardFail_11x8.png (11x8 - "не норм")
static const uint8_t icon_SDcardFail_11x8_bits[] PROGMEM = {
  0xff, 0x07, 0xb7, 0x07, 0xff, 0x07, 0x87, 0x07, 0x7b, 0x07, 0xff, 0x07, 0xff, 0x07, 0x67, 0x00
};

// Bluetooth_Idle_5x8.png (5x8 - "значок бт")
static const uint8_t icon_Bluetooth_Idle_5x8_bits[] PROGMEM = {
  0x04, 0x0d, 0x16, 0x0c, 0x0c, 0x16, 0x0d, 0x04
};

void init() {
}

void draw(U8G2& u8g2, uint32_t now) {
  // ============================================================
  // 1. Left Plaque: Current Time (в левый край)
  // ============================================================
  int yr, mo, dy, hr, mn, sc;
  sys_clock::getTime(yr, mo, dy, hr, mn, sc);
  char timeBuf[8];
  snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", hr, mn);

  u8g2.setFont(u8g2_font_helvB08_tr); // Flipper FontPrimary
  uint8_t w_clk = u8g2.getStrWidth(timeBuf);
  if (mn % 10 == 1) {
    w_clk++;
  }

  const uint8_t lw = w_clk + 6;
  const uint8_t lh = 13; // GUI_STATUS_BAR_HEIGHT

  // Solid white work area
  u8g2.setDrawColor(1);
  u8g2.drawBox(0, 0, lw, lh);

  // Flipper Zero rounded frame (radius 1) & double-shadow border lines
  u8g2.setDrawColor(0);
  u8g2.drawRFrame(0, 0, lw, lh, 1);
  u8g2.drawLine(lw - 2, 1, lw - 2, lh - 2);
  u8g2.drawLine(1, lh - 2, lw - 2, lh - 2);

  // Draw Time string
  u8g2.drawStr(3, 10, timeBuf);

  // ============================================================
  // 2. Right Plaque: BT + SD ("норм / не норм") + Battery
  // ============================================================
  const bool btConn = ble_service::isConnected() || ble_hid::isConnected();
  const bool sdMounted = sys_storage::isSdMounted();

  const uint8_t w_bat = 26;
  const uint8_t w_sd = 11;
  const uint8_t w_bt = btConn ? 5 : 0;

  uint8_t right_used = (w_bat + 2) + (w_sd + 2);
  if (btConn) {
    right_used += (w_bt + 2);
  }

  const uint8_t rw = right_used + 3;
  const uint8_t rx = 128 - 3 - right_used;
  const uint8_t rh = 13;

  // Solid white work area
  u8g2.setDrawColor(1);
  u8g2.drawBox(rx, 0, rw, rh);

  // Flipper Zero rounded frame (radius 1) & double-shadow border lines
  u8g2.setDrawColor(0);
  u8g2.drawRFrame(rx, 0, rw, rh, 1);
  u8g2.drawLine(rx + rw - 2, 1, rx + rw - 2, rh - 2);
  u8g2.drawLine(rx + 1, rh - 2, rx + rw - 2, rh - 2);

  // Item 1 (Far right): Battery icon at (bx, 2)
  const uint8_t bx = 128 - 1 - (w_bat + 2);
  u8g2.drawXBM(bx, 2, 26, 8, icon_Battery_26x8_bits);

  // Draw Battery percentage inside Battery_26x8 instead of division bars
  uint8_t pct = battery::getPercentage();
  if (pct > 100) pct = 100;
  char pctBuf[8];
  snprintf(pctBuf, sizeof(pctBuf), "%u%%", pct);

  u8g2.setFont(u8g2_font_4x6_tr);
  uint8_t tw = u8g2.getStrWidth(pctBuf);
  uint8_t tx = bx + 1 + (22 > tw ? (22 - tw) / 2 : 0);
  u8g2.setDrawColor(0);
  u8g2.drawStr(tx, 8, pctBuf);

  // Item 2 (In front of battery): SD card icon ("норм" или "не норм")
  const uint8_t sdx = bx - (w_sd + 2);
  if (sdMounted) {
    u8g2.drawXBM(sdx, 2, 11, 8, icon_SDcardMounted_11x8_bits);
  } else {
    u8g2.drawXBM(sdx, 2, 11, 8, icon_SDcardFail_11x8_bits);
  }

  // Item 3 (In front of SD): Bluetooth icon (if connected)
  if (btConn) {
    const uint8_t btx = sdx - (w_bt + 2);
    u8g2.drawXBM(btx, 2, 5, 8, icon_Bluetooth_Idle_5x8_bits);
  }
}

} // namespace status_bar
