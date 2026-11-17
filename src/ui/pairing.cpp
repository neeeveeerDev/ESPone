#include "pairing.h"
#include "core/ble_service.h"

namespace pairing {

void draw(U8G2& u8g2, uint32_t now) {
  u8g2.setDrawColor(1);


  u8g2.setFont(u8g2_font_6x12_tr);
  const char* title = ble_service::getDeviceName();
  int tw = u8g2.getStrWidth(title);
  u8g2.drawStr((128 - tw) / 2, 10, title);


  u8g2.drawRFrame(12, 15, 104, 25, 4);


  const char* rawPin = ble_service::getPin();
  char pinFmt[16];
  snprintf(pinFmt, sizeof(pinFmt), "%c %c %c  %c %c %c",
           rawPin[0], rawPin[1], rawPin[2],
           rawPin[3], rawPin[4], rawPin[5]);

  u8g2.setFont(u8g2_font_7x14B_tr);
  int pw = u8g2.getStrWidth(pinFmt);
  u8g2.drawStr((128 - pw) / 2, 32, pinFmt);


  u8g2.setFont(u8g2_font_5x8_tr);
  const char* status = ble_service::isConnected() ? "Authenticating..." : "Waiting for phone";
  int dots = (now / 400) % 4;
  char statusBuf[32];
  snprintf(statusBuf, sizeof(statusBuf), "%s%.*s", status, dots, "...");
  int sw = u8g2.getStrWidth(statusBuf);
  u8g2.drawStr((128 - sw) / 2, 51, statusBuf);
}

}
