#include "scene_wifi_scan.h"
#include <WiFi.h>
#include <esp_wifi.h>
#include <vector>
#include <math.h>

namespace scene_wifi_scan {

enum ScanSubScene : uint8_t {
  SUB_LIST = 0,
  SUB_DETAILS
};

static const int kRowH = 12;
static const int kStartY = 16;
static const int kMaxNets = 60;

static ScanSubScene s_subScene = SUB_LIST;
static std::vector<WifiApItem> s_networks;
static int s_sel = 0;
static float s_barY = (float)kStartY;
static float s_scroll = 0.0f;
static float s_targetScroll = 0.0f;
static int s_selectedIdx = -1;

static bool s_hasTarget = false;
static WifiApItem s_selectedTarget;
static bool s_isScanning = false;

static const char* getAuthShort(uint8_t auth) {
  switch (auth) {
    case WIFI_AUTH_OPEN: return "OPEN";
    case WIFI_AUTH_WEP: return "WEP";
    case WIFI_AUTH_WPA_PSK: return "WPA";
    case WIFI_AUTH_WPA2_PSK: return "WPA2";
    case WIFI_AUTH_WPA_WPA2_PSK: return "WPA/2";
    case WIFI_AUTH_WPA2_ENTERPRISE: return "ENT";
    case WIFI_AUTH_WPA3_PSK: return "WPA3";
    case WIFI_AUTH_WPA2_WPA3_PSK: return "WPA2/3";
    default: return "ENC";
  }
}

static void updateScroll(int sel, int count, float& scroll, float& targetScroll, float& barY) {
  const int maxLines = 4;
  int maxScroll = count - maxLines;
  if (maxScroll < 0) maxScroll = 0;

  int topIndex = (int)(targetScroll / kRowH + 0.5f);
  if (sel < topIndex) {
    targetScroll = (float)(sel * kRowH);
  } else if (sel >= topIndex + maxLines) {
    targetScroll = (float)((sel - maxLines + 1) * kRowH);
  }

  if (targetScroll < 0.0f) targetScroll = 0.0f;
  if (targetScroll > (float)(maxScroll * kRowH)) targetScroll = (float)(maxScroll * kRowH);

  float ds = targetScroll - scroll;
  if (fabsf(ds) > 0.1f) {
    scroll += ds * 0.35f;
  } else {
    scroll = targetScroll;
  }

  float targetBarY = (float)(kStartY + sel * kRowH) - scroll;
  float dy = targetBarY - barY;
  if (fabsf(dy) > 0.1f) {
    barY += dy * 0.4f;
  } else {
    barY = targetBarY;
  }
}

static void drawHeader(U8G2& u8g2, const char* title, int count = -1) {
  u8g2.setDrawColor(1);
  u8g2.drawBox(0, 0, 128, 14);
  u8g2.setDrawColor(0);
  u8g2.setFont(u8g2_font_7x14B_tr);
  u8g2.drawStr(4, 11, title);

  if (count >= 0) {
    char cntBuf[12];
    snprintf(cntBuf, sizeof(cntBuf), "%d", count);
    u8g2.setFont(u8g2_font_5x8_tr);
    int cw = u8g2.getStrWidth(cntBuf);
    u8g2.drawStr(124 - cw, 10, cntBuf);
  }
  u8g2.setDrawColor(1);
}

static void triggerScan() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  WiFi.scanNetworks(true, true, false, 100);
  s_isScanning = true;
}

void open() {
  s_subScene = SUB_LIST;
  s_sel = 0;
  s_barY = (float)kStartY;
  s_scroll = 0.0f;
  s_targetScroll = 0.0f;
  s_selectedIdx = -1;
  s_networks.clear();
  triggerScan();
}

void close() {
  WiFi.scanDelete();
  s_isScanning = false;
  s_subScene = SUB_LIST;
}

bool onBack() {
  if (s_subScene == SUB_DETAILS) {
    s_subScene = SUB_LIST;
    return true;
  }
  return false;
}

void tick(uint32_t now) {
  (void)now;
  if (s_isScanning) {
    int16_t res = WiFi.scanComplete();
    if (res >= 0) {
      s_isScanning = false;
      for (int i = 0; i < res; i++) {
        String bssid = WiFi.BSSIDstr(i);
        bool found = false;
        for (auto& item : s_networks) {
          if (strcmp(item.bssidStr, bssid.c_str()) == 0) {
            found = true;
            item.rssi = WiFi.RSSI(i);
            break;
          }
        }
        if (!found && (int)s_networks.size() < kMaxNets) {
          WifiApItem item;
          String ssid = WiFi.SSID(i);
          snprintf(item.ssid, sizeof(item.ssid), "%s", (ssid.length() > 0) ? ssid.c_str() : "<Hidden>");
          snprintf(item.bssidStr, sizeof(item.bssidStr), "%s", bssid.c_str());
          item.rssi = WiFi.RSSI(i);
          item.channel = WiFi.channel(i);
          item.authMode = (uint8_t)WiFi.encryptionType(i);
          s_networks.push_back(item);
        }
      }
      WiFi.scanDelete();

      triggerScan();
    }
  }

  if (s_subScene == SUB_LIST) {
    int count = (int)s_networks.size();
    if (count > 0) {
      if (s_sel >= count) s_sel = count - 1;
      updateScroll(s_sel, count, s_scroll, s_targetScroll, s_barY);
    }
  }
}

void onKey(const KeyEvent& ev) {
  int count = (int)s_networks.size();

  if (s_subScene == SUB_LIST) {
    if (count > 0) {
      if (ev.key == BTN_UP) {
        s_sel = (s_sel - 1 + count) % count;
      } else if (ev.key == BTN_DOWN) {
        s_sel = (s_sel + 1) % count;
      } else if (ev.key == BTN_OK && !ev.repeat) {
        s_selectedIdx = s_sel;
        s_selectedTarget = s_networks[s_sel];
        s_hasTarget = true;
        s_subScene = SUB_DETAILS;
      }
    }
  } else if (s_subScene == SUB_DETAILS) {
    if (ev.key == BTN_OK && !ev.repeat) {

      s_hasTarget = true;
      s_subScene = SUB_LIST;
    }
  }
}

static void drawDetails(U8G2& u8g2) {
  drawHeader(u8g2, "NETWORK INFO");

  if (s_selectedIdx < 0 || s_selectedIdx >= (int)s_networks.size()) {
    u8g2.setFont(u8g2_font_6x12_tr);
    u8g2.drawStr(10, 36, "Network lost");
    return;
  }

  const WifiApItem& curItem = s_networks[s_selectedIdx];


  u8g2.setFont(u8g2_font_6x12_tr);
  char nameBuf[32];
  strncpy(nameBuf, curItem.ssid, sizeof(nameBuf) - 1);
  nameBuf[sizeof(nameBuf) - 1] = '\0';
  while (strlen(nameBuf) > 0 && u8g2.getStrWidth(nameBuf) > 120) {
    nameBuf[strlen(nameBuf) - 1] = '\0';
  }
  u8g2.drawStr(4, 25, nameBuf);


  u8g2.setFont(u8g2_font_5x8_tr);
  char macBuf[28];
  snprintf(macBuf, sizeof(macBuf), "BSSID: %s", curItem.bssidStr);
  u8g2.drawStr(4, 35, macBuf);


  char chSecBuf[24];
  snprintf(chSecBuf, sizeof(chSecBuf), "CH:%d  %s", curItem.channel, getAuthShort(curItem.authMode));
  u8g2.drawStr(4, 45, chSecBuf);

  char rssiBuf[16];
  snprintf(rssiBuf, sizeof(rssiBuf), "%d", (int)curItem.rssi);
  int rw = u8g2.getStrWidth(rssiBuf);
  u8g2.drawStr(124 - rw, 45, rssiBuf);
}

void draw(U8G2& u8g2, uint32_t now) {
  if (s_subScene == SUB_DETAILS) {
    drawDetails(u8g2);
    return;
  }

  int count = (int)s_networks.size();
  drawHeader(u8g2, "WIFI SCAN", count);

  if (count == 0) {
    u8g2.setFont(u8g2_font_6x12_tr);
    int dots = (now / 350) % 4;
    char waitBuf[20];
    snprintf(waitBuf, sizeof(waitBuf), "Scanning%.*s", dots, "...");
    int tw = u8g2.getStrWidth(waitBuf);
    u8g2.drawStr((128 - tw) / 2, 40, waitBuf);
    return;
  }


  u8g2.setClipWindow(0, 14, 128, 64);

  for (int i = 0; i < count; i++) {
    int y = kStartY + (int)(i * kRowH - s_scroll + 0.5f);
    if (y < kStartY - kRowH || y > 64) continue;

    const WifiApItem& item = s_networks[i];


    char metaBuf[16];
    snprintf(metaBuf, sizeof(metaBuf), "%s %d", getAuthShort(item.authMode), (int)item.rssi);
    u8g2.setFont(u8g2_font_5x8_tr);
    int mw = u8g2.getStrWidth(metaBuf);
    u8g2.drawStr(124 - mw, y + 9, metaBuf);


    u8g2.setFont(u8g2_font_6x12_tr);
    int maxW = 124 - mw - 8;
    char ssidLabel[32];
    strncpy(ssidLabel, item.ssid, sizeof(ssidLabel) - 1);
    ssidLabel[sizeof(ssidLabel) - 1] = '\0';
    while (strlen(ssidLabel) > 0 && u8g2.getStrWidth(ssidLabel) > maxW) {
      ssidLabel[strlen(ssidLabel) - 1] = '\0';
    }
    u8g2.drawStr(6, y + 10, ssidLabel);
  }


  u8g2.setDrawColor(2);
  int curY = (int)(s_barY + 0.5f);
  if (curY < kStartY) curY = kStartY;
  if (curY > 64 - kRowH) curY = 64 - kRowH;
  u8g2.drawBox(2, curY, 124, kRowH);
  u8g2.setDrawColor(1);

  u8g2.setMaxClipWindow();
}

bool hasSelectedTarget() {
  return s_hasTarget;
}

const WifiApItem& getSelectedTarget() {
  return s_selectedTarget;
}

void clearSelectedTarget() {
  s_hasTarget = false;
}

void setTarget(const char* ssid, const char* bssid, uint8_t channel, int32_t rssi) {
  snprintf(s_selectedTarget.ssid, sizeof(s_selectedTarget.ssid), "%s", ssid);
  snprintf(s_selectedTarget.bssidStr, sizeof(s_selectedTarget.bssidStr), "%s", bssid);
  s_selectedTarget.channel = channel;
  s_selectedTarget.rssi = rssi;
  s_hasTarget = true;
}

}
