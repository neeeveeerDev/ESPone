#include "scene_bruteforce.h"
#include "cc1101_driver.h"
#include "subghz_protocols.h"
#include <vector>
#include <math.h>

namespace scene_bruteforce {

enum BFState : uint8_t {
  STATE_SETTINGS = 0,
  STATE_RUNNING,
  STATE_PAUSED,
  STATE_DONE
};

struct ProtocolDef {
  const char* name;
  uint8_t bits;
  uint32_t defaultTe;
  uint32_t maxCombinations;
};

static const ProtocolDef kProtocols[] = {
  {"Came", 12, 320, 4096},
  {"Nice FLO", 12, 700, 4096},
  {"Linear", 10, 500, 1024},
  {"Princeton", 12, 400, 4096}
};
static const int kProtoCount = sizeof(kProtocols) / sizeof(kProtocols[0]);

static const uint32_t kFreqs[] = {
  433920000,
  315000000,
  868350000,
  300000000
};
static const char* kFreqLabels[] = {
  "433.92M",
  "315.00M",
  "868.35M",
  "300.00M"
};
static const int kFreqCount = sizeof(kFreqs) / sizeof(kFreqs[0]);

static const int kRepeatsOptions[] = {1, 2, 3};
static const int kRepeatsCount = sizeof(kRepeatsOptions) / sizeof(kRepeatsOptions[0]);

static const uint32_t kDelayOptions[] = {0, 10, 25, 50};
static const int kDelayCount = sizeof(kDelayOptions) / sizeof(kDelayOptions[0]);

static BFState s_state = STATE_SETTINGS;
static int s_settingRow = 0;
static const int kSettingRowCount = 5;

static int s_protoIdx = 0;
static int s_freqIdx = 0;
static int s_repeatsIdx = 0;
static int s_delayIdx = 0;

static uint32_t s_currentCode = 0;
static uint32_t s_totalCodes = 4096;
static uint32_t s_lastSendMs = 0;
static uint32_t s_codesSent = 0;
static uint32_t s_startTime = 0;
static float s_codesPerSec = 0.0f;
static std::vector<int32_t> s_txBuffer;

void open() {
  s_state = STATE_SETTINGS;
  s_settingRow = 0;
  s_currentCode = 0;
  s_codesSent = 0;
  s_codesPerSec = 0.0f;
  s_totalCodes = kProtocols[s_protoIdx].maxCombinations;
}

void close() {
  s_state = STATE_SETTINGS;
}

bool onBack() {
  if (s_state == STATE_RUNNING || s_state == STATE_PAUSED || s_state == STATE_DONE) {
    s_state = STATE_SETTINGS;
    return true;
  }
  return false;
}

static void sendNextCode() {
  if (s_currentCode >= s_totalCodes) {
    s_state = STATE_DONE;
    return;
  }

  const ProtocolDef& proto = kProtocols[s_protoIdx];
  if (subghz_protocols::encode(proto.name, s_currentCode, proto.bits, proto.defaultTe, s_txBuffer)) {
    if (!s_txBuffer.empty()) {
      int reps = kRepeatsOptions[s_repeatsIdx];
      cc1101::sendRaw(s_txBuffer.data(), s_txBuffer.size(), reps);
    }
  }

  s_currentCode++;
  s_codesSent++;

  uint32_t elapsed = millis() - s_startTime;
  if (elapsed > 0) {
    s_codesPerSec = (float)s_codesSent / (elapsed / 1000.0f);
  }
}

void tick(uint32_t now) {
  if (s_state == STATE_RUNNING) {
    uint32_t stepDelay = kDelayOptions[s_delayIdx];
    if (now - s_lastSendMs >= stepDelay) {
      s_lastSendMs = now;
      sendNextCode();
    }
  }
}

void onKey(const KeyEvent& ev) {
  if (s_state == STATE_SETTINGS) {
    if (ev.key == BTN_UP) {
      if (s_settingRow > 0) s_settingRow--;
      else s_settingRow = kSettingRowCount - 1;
    } else if (ev.key == BTN_DOWN) {
      if (s_settingRow < kSettingRowCount - 1) s_settingRow++;
      else s_settingRow = 0;
    } else if (ev.key == BTN_LEFT) {
      switch (s_settingRow) {
        case 0:
          s_protoIdx = (s_protoIdx - 1 + kProtoCount) % kProtoCount;
          s_totalCodes = kProtocols[s_protoIdx].maxCombinations;
          break;
        case 1:
          s_freqIdx = (s_freqIdx - 1 + kFreqCount) % kFreqCount;
          break;
        case 2:
          s_repeatsIdx = (s_repeatsIdx - 1 + kRepeatsCount) % kRepeatsCount;
          break;
        case 3:
          s_delayIdx = (s_delayIdx - 1 + kDelayCount) % kDelayCount;
          break;
        default:
          break;
      }
    } else if (ev.key == BTN_RIGHT) {
      switch (s_settingRow) {
        case 0:
          s_protoIdx = (s_protoIdx + 1) % kProtoCount;
          s_totalCodes = kProtocols[s_protoIdx].maxCombinations;
          break;
        case 1:
          s_freqIdx = (s_freqIdx + 1) % kFreqCount;
          break;
        case 2:
          s_repeatsIdx = (s_repeatsIdx + 1) % kRepeatsCount;
          break;
        case 3:
          s_delayIdx = (s_delayIdx + 1) % kDelayCount;
          break;
        default:
          break;
      }
    } else if (ev.key == BTN_OK && !ev.repeat) {
      if (s_settingRow == 4) {
        cc1101::setFrequency(kFreqs[s_freqIdx]);
        cc1101::setPreset(cc1101::PRESET_OOK_650);
        s_currentCode = 0;
        s_codesSent = 0;
        s_startTime = millis();
        s_lastSendMs = millis();
        s_totalCodes = kProtocols[s_protoIdx].maxCombinations;
        s_state = STATE_RUNNING;
      }
    }
  } else if (s_state == STATE_RUNNING) {
    if (ev.key == BTN_OK && !ev.repeat) {
      s_state = STATE_PAUSED;
    } else if (ev.key == BTN_LEFT) {
      if (s_currentCode >= 10) s_currentCode -= 10;
      else s_currentCode = 0;
    } else if (ev.key == BTN_RIGHT) {
      if (s_currentCode + 10 < s_totalCodes) s_currentCode += 10;
      else s_currentCode = s_totalCodes - 1;
    }
  } else if (s_state == STATE_PAUSED) {
    if (ev.key == BTN_OK && !ev.repeat) {
      s_state = STATE_RUNNING;
      s_lastSendMs = millis();
    } else if (ev.key == BTN_LEFT) {
      if (s_currentCode >= 10) s_currentCode -= 10;
      else s_currentCode = 0;
    } else if (ev.key == BTN_RIGHT) {
      if (s_currentCode + 10 < s_totalCodes) s_currentCode += 10;
      else s_currentCode = s_totalCodes - 1;
    }
  } else if (s_state == STATE_DONE) {
    if (ev.key == BTN_OK && !ev.repeat) {
      s_state = STATE_SETTINGS;
    }
  }
}

void draw(U8G2& u8g2, uint32_t now) {
  (void)now;
  u8g2.setDrawColor(1);

  if (s_state == STATE_SETTINGS) {
    u8g2.setDrawColor(0);
    u8g2.drawBox(0, 0, 128, 14);
    u8g2.setDrawColor(1);
    u8g2.drawBox(0, 0, 128, 14);
    u8g2.setDrawColor(0);
    u8g2.setFont(u8g2_font_7x14B_tr);
    u8g2.drawStr(4, 11, "BRUTEFORCE");
    u8g2.setDrawColor(1);
    u8g2.setFont(u8g2_font_6x10_tr);

    int startY = 15;
    int rowH = 9;

    for (int i = 0; i < kSettingRowCount; i++) {
      bool isSelected = (i == s_settingRow);

      if (i < 4) {
        int y = startY + i * 9;
        char label[16];
        char val[24];

        switch (i) {
          case 0:
            strcpy(label, "Proto");
            snprintf(val, sizeof(val), "< %s >", kProtocols[s_protoIdx].name);
            break;
          case 1:
            strcpy(label, "Freq");
            snprintf(val, sizeof(val), "< %s >", kFreqLabels[s_freqIdx]);
            break;
          case 2:
            strcpy(label, "Repeat");
            snprintf(val, sizeof(val), "< %dx >", kRepeatsOptions[s_repeatsIdx]);
            break;
          case 3:
            strcpy(label, "Delay");
            snprintf(val, sizeof(val), "< %lums >", (unsigned long)kDelayOptions[s_delayIdx]);
            break;
        }

        if (isSelected) {
          u8g2.setDrawColor(1);
          u8g2.drawBox(2, y, 124, rowH);
          u8g2.setDrawColor(0);
          u8g2.drawStr(5, y + 7, label);
          int vw = u8g2.getStrWidth(val);
          u8g2.drawStr(123 - vw, y + 7, val);
          u8g2.setDrawColor(1);
        } else {
          u8g2.drawStr(5, y + 7, label);
          int vw = u8g2.getStrWidth(val);
          u8g2.drawStr(123 - vw, y + 7, val);
        }
      } else {
        const char* btnText = "START ATTACK";
        int bw = u8g2.getStrWidth(btnText);
        int bx = (128 - bw) / 2;
        int btnY = 52;
        int btnH = 10;

        if (isSelected) {
          u8g2.setDrawColor(1);
          u8g2.drawBox(14, btnY, 100, btnH);
          u8g2.setDrawColor(0);
          u8g2.drawStr(bx, btnY + 8, btnText);
          u8g2.setDrawColor(1);
        } else {
          u8g2.drawFrame(14, btnY, 100, btnH);
          u8g2.drawStr(bx, btnY + 8, btnText);
        }
      }
    }
  } else {
    u8g2.setDrawColor(0);
    u8g2.drawBox(0, 0, 128, 14);
    u8g2.setDrawColor(1);
    u8g2.drawBox(0, 0, 128, 14);
    u8g2.setDrawColor(0);
    u8g2.setFont(u8g2_font_7x14B_tr);
    u8g2.drawStr(4, 11, "BRUTEFORCE");

    u8g2.setFont(u8g2_font_5x8_tr);
    const char* badge = (s_state == STATE_RUNNING) ? "ATTACK" :
                        (s_state == STATE_PAUSED)  ? "PAUSED" : "DONE";
    int badgew = u8g2.getStrWidth(badge);
    u8g2.drawStr(124 - badgew, 10, badge);
    u8g2.setDrawColor(1);

    char buf[32];
    u8g2.setFont(u8g2_font_6x10_tr);
    snprintf(buf, sizeof(buf), "%s  |  %s", kProtocols[s_protoIdx].name, kFreqLabels[s_freqIdx]);
    u8g2.drawStr(4, 24, buf);

    uint8_t bits = kProtocols[s_protoIdx].bits;
    snprintf(buf, sizeof(buf), "Key: 0x%03lX (%lubit)", (unsigned long)s_currentCode, (unsigned long)bits);
    u8g2.drawStr(4, 35, buf);

    snprintf(buf, sizeof(buf), "%lu / %lu (%d%%)", 
             (unsigned long)s_currentCode, 
             (unsigned long)s_totalCodes,
             (int)(s_totalCodes ? (s_currentCode * 100 / s_totalCodes) : 0));
    u8g2.drawStr(4, 46, buf);

    u8g2.drawFrame(4, 49, 120, 6);
    int fillW = s_totalCodes ? (int)((float)s_currentCode / s_totalCodes * 116.0f) : 0;
    if (fillW > 116) fillW = 116;
    if (fillW > 0) {
      u8g2.drawBox(6, 51, fillW, 2);
    }

    if (s_state == STATE_RUNNING) {
      snprintf(buf, sizeof(buf), "Speed: %.1f c/s", s_codesPerSec);
    } else if (s_state == STATE_PAUSED) {
      snprintf(buf, sizeof(buf), "Paused");
    } else {
      snprintf(buf, sizeof(buf), "Attack Finished");
    }
    u8g2.setFont(u8g2_font_5x8_tr);
    u8g2.drawStr(4, 63, buf);
  }
}

}
