#include "settings_system.h"
#include "settings_data.h"
#include <FFat.h>
#include <SD.h>
#include <math.h>

namespace settings_system {

enum ConfirmType : uint8_t {
  CONFIRM_NONE = 0,
  CONFIRM_GPIO_RESET,
  CONFIRM_PIN_RESET,
  CONFIRM_FACTORY_RESET
};

static const int kCount = 9;
static const int rowH = 12;
static const int startY = 16;

static int s_sel = 0;
static float s_scroll = 0.0f;
static float s_targetScroll = 0.0f;
static float s_barY = 16.0f;

static ConfirmType s_confirm = CONFIRM_NONE;
static int s_confirmSel = 0; // 0 = Cancel, 1 = Execute
static uint32_t s_msgUntil = 0;
static const char* s_msgText = "";

void open() {
  s_sel = 0;
  s_scroll = 0.0f;
  s_targetScroll = 0.0f;
  s_barY = (float)startY;
  s_confirm = CONFIRM_NONE;
  s_confirmSel = 0;
  s_msgUntil = 0;
}

void close() {
  s_confirm = CONFIRM_NONE;
}

bool onBack() {
  if (s_confirm != CONFIRM_NONE) {
    s_confirm = CONFIRM_NONE;
    return true;
  }
  return false;
}

void tick(uint32_t now) {
  (void)now;
  if (s_confirm == CONFIRM_NONE) {
    const int maxLines = 4;
    int maxScroll = kCount - maxLines;
    if (maxScroll < 0) maxScroll = 0;

    int topIndex = (int)(s_targetScroll / rowH + 0.5f);
    if (s_sel < topIndex) {
      s_targetScroll = (float)(s_sel * rowH);
    } else if (s_sel >= topIndex + maxLines) {
      s_targetScroll = (float)((s_sel - maxLines + 1) * rowH);
    }

    if (s_targetScroll < 0.0f) s_targetScroll = 0.0f;
    if (s_targetScroll > (float)(maxScroll * rowH)) s_targetScroll = (float)(maxScroll * rowH);

    float ds = s_targetScroll - s_scroll;
    if (fabsf(ds) > 0.1f) s_scroll += ds * 0.35f;
    else s_scroll = s_targetScroll;

    float targetBar = (float)(startY + s_sel * rowH) - s_scroll;
    float dy = targetBar - s_barY;
    if (fabsf(dy) > 0.1f) s_barY += dy * 0.4f;
    else s_barY = targetBar;
  }
}

void onKey(const KeyEvent& ev) {
  if (s_confirm != CONFIRM_NONE) {
    if (ev.key == BTN_LEFT || ev.key == BTN_UP) {
      s_confirmSel = 0;
    } else if (ev.key == BTN_RIGHT || ev.key == BTN_DOWN) {
      s_confirmSel = 1;
    } else if (ev.key == BTN_BACK) {
      s_confirm = CONFIRM_NONE;
    } else if (ev.key == BTN_OK && !ev.repeat) {
      if (s_confirmSel == 1) {
        if (s_confirm == CONFIRM_GPIO_RESET) {
          settings_data::resetGpioPins();
          s_msgText = "PINS RESET";
          s_msgUntil = millis() + 1500;
        } else if (s_confirm == CONFIRM_PIN_RESET) {
          settings_data::resetBlePin();
          s_msgText = "PIN RESET";
          s_msgUntil = millis() + 1500;
        } else if (s_confirm == CONFIRM_FACTORY_RESET) {
          settings_data::factoryReset();
        }
      }
      s_confirm = CONFIRM_NONE;
    }
    return;
  }

  if (ev.key == BTN_UP) {
    s_sel = (s_sel - 1 + kCount) % kCount;
  } else if (ev.key == BTN_DOWN) {
    s_sel = (s_sel + 1) % kCount;
  } else if (ev.key == BTN_OK && !ev.repeat) {
    if (s_sel == 6) {
      s_confirm = CONFIRM_GPIO_RESET;
      s_confirmSel = 0;
    } else if (s_sel == 7) {
      s_confirm = CONFIRM_PIN_RESET;
      s_confirmSel = 0;
    } else if (s_sel == 8) {
      s_confirm = CONFIRM_FACTORY_RESET;
      s_confirmSel = 0;
    }
  }
}

void draw(U8G2& u8g2, uint32_t now) {
  u8g2.setClipWindow(0, 14, 128, 64);
  u8g2.setFont(u8g2_font_6x12_tr);
  u8g2.setDrawColor(1);

  char lines[kCount][36];

  // 0: Firmware Version
  snprintf(lines[0], sizeof(lines[0]), "FW: ESPone S3 v2.4");

  // 1: MCU
  snprintf(lines[1], sizeof(lines[1]), "MCU: ESP32-S3 240MHz");

  // 2: RAM Free
  uint32_t freeKb = ESP.getFreeHeap() / 1024;
  snprintf(lines[2], sizeof(lines[2]), "RAM: %lu KB free", freeKb);

  // 3: PSRAM
  if (psramFound()) {
    uint32_t psramMb = ESP.getPsramSize() / (1024 * 1024);
    uint32_t psramFreeKb = ESP.getFreePsram() / 1024;
    snprintf(lines[3], sizeof(lines[3]), "PSRAM: %luM (%luK)", psramMb, psramFreeKb);
  } else {
    snprintf(lines[3], sizeof(lines[3]), "PSRAM: None");
  }

  // 4: Flash
  snprintf(lines[4], sizeof(lines[4]), "Flash: 16 MB QIO");

  // 5: Storage
  size_t ffatTotal = FFat.totalBytes() / 1024;
  size_t ffatUsed = FFat.usedBytes() / 1024;
  snprintf(lines[5], sizeof(lines[5]), "FFat: %u/%uK SD:%s", (unsigned)ffatUsed, (unsigned)ffatTotal, SD.exists("/") ? "OK" : "NO");

  // 6: Reset GPIO
  snprintf(lines[6], sizeof(lines[6]), "Reset GPIO Pins");

  // 7: Reset BLE PIN
  snprintf(lines[7], sizeof(lines[7]), "Reset BLE PIN");

  // 8: Factory Reset
  snprintf(lines[8], sizeof(lines[8]), "Factory Reset");

  for (int i = 0; i < kCount; i++) {
    int y = startY + (int)(i * rowH - s_scroll + 0.5f);
    if (y < startY - rowH || y > 64) continue;
    u8g2.drawStr(6, y + 10, lines[i]);
  }

  // Selection Box
  if (s_confirm == CONFIRM_NONE) {
    u8g2.setDrawColor(2);
    int curY = (int)(s_barY + 0.5f);
    if (curY >= startY && curY <= 64 - rowH) {
      u8g2.drawBox(2, curY, 124, rowH);
    }
    u8g2.setDrawColor(1);
  }
  u8g2.setMaxClipWindow();

  // Header
  u8g2.setDrawColor(1);
  u8g2.drawBox(0, 0, 128, 14);
  u8g2.setDrawColor(0);
  u8g2.setFont(u8g2_font_7x14B_tr);
  u8g2.drawStr(4, 11, "SYSTEM");
  if (now < s_msgUntil) {
    u8g2.setFont(u8g2_font_5x8_tr);
    u8g2.drawStr(74, 10, s_msgText);
  }
  u8g2.setDrawColor(1);

  // Modal Confirmation Dialog
  if (s_confirm != CONFIRM_NONE) {
    u8g2.setDrawColor(0);
    u8g2.drawBox(10, 16, 108, 44);
    u8g2.setDrawColor(1);
    u8g2.drawFrame(10, 16, 108, 44);
    u8g2.drawFrame(12, 18, 104, 40);

    u8g2.setFont(u8g2_font_6x12_tr);
    const char* q = "Reset GPIO Pins?";
    if (s_confirm == CONFIRM_PIN_RESET) q = "Reset BLE PIN?";
    else if (s_confirm == CONFIRM_FACTORY_RESET) q = "Wipe All Data?";
    int qw = u8g2.getStrWidth(q);
    u8g2.drawStr((128 - qw) / 2, 30, q);

    // Cancel Button
    if (s_confirmSel == 0) {
      u8g2.drawBox(18, 40, 42, 14);
      u8g2.setDrawColor(0);
      u8g2.drawStr(22, 51, "Cancel");
      u8g2.setDrawColor(1);
    } else {
      u8g2.drawFrame(18, 40, 42, 14);
      u8g2.drawStr(22, 51, "Cancel");
    }

    // Confirm Button
    const char* actionLabel = (s_confirm == CONFIRM_FACTORY_RESET) ? "Wipe" : "Reset";
    if (s_confirmSel == 1) {
      u8g2.drawBox(68, 40, 42, 14);
      u8g2.setDrawColor(0);
      int lw = u8g2.getStrWidth(actionLabel);
      u8g2.drawStr(68 + (42 - lw) / 2, 51, actionLabel);
      u8g2.setDrawColor(1);
    } else {
      u8g2.drawFrame(68, 40, 42, 14);
      int lw = u8g2.getStrWidth(actionLabel);
      u8g2.drawStr(68 + (42 - lw) / 2, 51, actionLabel);
    }
  }
}

}
