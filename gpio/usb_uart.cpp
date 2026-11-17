#include "usb_uart.h"
#include "config.h"

namespace usb_uart {

static const uint32_t kBauds[] = {
  9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600
};
static const int kBaudCount = sizeof(kBauds) / sizeof(kBauds[0]);
static int s_baudIdx = 4;

static bool s_running = false;


static const int kLines = 6;
static const int kCols = 25;
static char s_lines[kLines][kCols + 1];
static int s_col = 0;

static void termScrollUp() {
  for (int i = 0; i < kLines - 1; i++) {
    memcpy(s_lines[i], s_lines[i + 1], kCols + 1);
  }
  memset(s_lines[kLines - 1], 0, kCols + 1);
  s_col = 0;
}

static void termPut(char c) {
  if (c == '\r') {
    return;
  }
  if (c == '\n') {
    termScrollUp();
    return;
  }
  if ((uint8_t)c < 32 || (uint8_t)c > 126) {
    return;
  }
  if (s_col >= kCols) {
    termScrollUp();
  }
  s_lines[kLines - 1][s_col++] = c;
  s_lines[kLines - 1][s_col] = '\0';
}

static void startBridge() {
  uint32_t baud = kBauds[s_baudIdx];
  Serial1.end();
  Serial1.begin(baud, SERIAL_8N1, UART_BRIDGE_PIN_RX, UART_BRIDGE_PIN_TX);
  s_running = true;
  Serial.printf("[bridge] Started on RX:IO%d TX:IO%d @ %lu baud\n",
                UART_BRIDGE_PIN_RX, UART_BRIDGE_PIN_TX, baud);
}

void open() {

  for (int i = 0; i < kLines; i++) {
    memset(s_lines[i], 0, kCols + 1);
  }
  s_col = 0;
  startBridge();
}

void close() {
  if (s_running) {
    Serial1.end();
    s_running = false;
    Serial.println("[bridge] Closed");
  }
}

void tick(uint32_t now) {
  (void)now;
  if (!s_running) return;


  while (Serial.available()) {
    uint8_t b = Serial.read();
    Serial1.write(b);
  }


  while (Serial1.available()) {
    uint8_t b = Serial1.read();
    Serial.write(b);
    termPut((char)b);
  }
}

void onKey(const KeyEvent& ev) {
  if (ev.key == BTN_LEFT && !ev.repeat) {
    if (s_baudIdx > 0) {
      s_baudIdx--;
      startBridge();
    }
  } else if (ev.key == BTN_RIGHT && !ev.repeat) {
    if (s_baudIdx < kBaudCount - 1) {
      s_baudIdx++;
      startBridge();
    }
  } else if (ev.key == BTN_OK && !ev.repeat) {

    for (int i = 0; i < kLines; i++) {
      memset(s_lines[i], 0, kCols + 1);
    }
    s_col = 0;
  }
}

void draw(U8G2& u8g2, uint32_t now) {
  (void)now;

  u8g2.setDrawColor(1);
  u8g2.setFont(u8g2_font_5x7_tr);

  for (int i = 0; i < kLines; i++) {
    if (s_lines[i][0] != '\0') {
      u8g2.drawStr(1, 8 + i * 8, s_lines[i]);
    }
  }


  u8g2.drawHLine(0, 52, 128);


  u8g2.setFont(u8g2_font_6x10_tr);
  char baudBuf[24];
  snprintf(baudBuf, sizeof(baudBuf), "< %lu >", kBauds[s_baudIdx]);
  int bw = u8g2.getStrWidth(baudBuf);
  u8g2.drawStr((128 - bw) / 2, 62, baudBuf);
}

}
