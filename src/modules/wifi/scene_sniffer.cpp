#include "scene_sniffer.h"
#include <WiFi.h>
#include <esp_wifi.h>

namespace scene_sniffer {

static volatile uint32_t s_totalPkts = 0;
static volatile uint32_t s_mgmtPkts = 0;
static volatile uint32_t s_ctrlPkts = 0;
static volatile uint32_t s_dataPkts = 0;
static volatile uint32_t s_beaconPkts = 0;
static volatile uint32_t s_eapolPkts = 0;
static volatile int8_t s_lastRssi = 0;
static char s_lastType[8] = "-";

static uint8_t s_channel = 1;
static bool s_autoHop = true;
static uint32_t s_lastHop = 0;

static void IRAM_ATTR wifi_sniffer_cb(void* buf, wifi_promiscuous_pkt_type_t type) {
  const wifi_promiscuous_pkt_t* pkt = (wifi_promiscuous_pkt_t*)buf;
  s_totalPkts++;
  s_lastRssi = pkt->rx_ctrl.rssi;
  if (pkt->rx_ctrl.sig_len < 2) return;

  uint8_t fc0 = pkt->payload[0];
  uint8_t frameType = (fc0 >> 2) & 0x03;
  uint8_t frameSubtype = (fc0 >> 4) & 0x0F;

  if (frameType == 0) {
    s_mgmtPkts++;
    if (frameSubtype == 8) {
      s_beaconPkts++;
      strncpy(s_lastType, "BCN", sizeof(s_lastType));
    } else if (frameSubtype == 4) {
      strncpy(s_lastType, "PROB", sizeof(s_lastType));
    } else if (frameSubtype == 12) {
      strncpy(s_lastType, "DEAU", sizeof(s_lastType));
    } else {
      strncpy(s_lastType, "MGMT", sizeof(s_lastType));
    }
  } else if (frameType == 1) {
    s_ctrlPkts++;
    strncpy(s_lastType, "CTRL", sizeof(s_lastType));
  } else if (frameType == 2) {
    s_dataPkts++;
    bool isEapol = false;
    int len = pkt->rx_ctrl.sig_len;
    if (len > 36) {
      for (int i = 24; i < len - 2 && i < 40; i++) {
        if (pkt->payload[i] == 0x88 && pkt->payload[i + 1] == 0x8E) {
          isEapol = true;
          break;
        }
      }
    }
    if (isEapol) {
      s_eapolPkts++;
      strncpy(s_lastType, "EAPL", sizeof(s_lastType));
    } else {
      strncpy(s_lastType, "DATA", sizeof(s_lastType));
    }
  }
}

static void drawHeader(U8G2& u8g2, const char* title) {
  u8g2.setDrawColor(1);
  u8g2.drawBox(0, 0, 128, 14);
  u8g2.setDrawColor(0);
  u8g2.setFont(u8g2_font_7x14B_tr);
  u8g2.drawStr(4, 11, title);
  u8g2.setDrawColor(1);
}

void open() {
  s_totalPkts = 0;
  s_mgmtPkts = 0;
  s_ctrlPkts = 0;
  s_dataPkts = 0;
  s_beaconPkts = 0;
  s_eapolPkts = 0;
  s_lastRssi = 0;
  strncpy(s_lastType, "-", sizeof(s_lastType));
  s_channel = 1;
  s_autoHop = true;
  s_lastHop = millis();

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  esp_wifi_set_promiscuous(false);
  esp_wifi_set_promiscuous_rx_cb(&wifi_sniffer_cb);
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(s_channel, WIFI_SECOND_CHAN_NONE);
}

void close() {
  esp_wifi_set_promiscuous(false);
}

void tick(uint32_t now) {
  if (s_autoHop && (now - s_lastHop >= 2000)) {
    s_lastHop = now;
    s_channel++;
    if (s_channel > 13) s_channel = 1;
    esp_wifi_set_channel(s_channel, WIFI_SECOND_CHAN_NONE);
  }
}

void onKey(const KeyEvent& ev) {
  if (ev.repeat) return;
  if (ev.key == BTN_OK) {
    s_autoHop = !s_autoHop;
    s_lastHop = millis();
  } else if (ev.key == BTN_LEFT) {
    s_autoHop = false;
    if (s_channel > 1) s_channel--;
    else s_channel = 13;
    esp_wifi_set_channel(s_channel, WIFI_SECOND_CHAN_NONE);
  } else if (ev.key == BTN_RIGHT) {
    s_autoHop = false;
    if (s_channel < 13) s_channel++;
    else s_channel = 1;
    esp_wifi_set_channel(s_channel, WIFI_SECOND_CHAN_NONE);
  }
}

bool onBack() {
  return false;
}

void draw(U8G2& u8g2, uint32_t now) {
  drawHeader(u8g2, "SNIFFER");

  u8g2.setFont(u8g2_font_6x12_tr);


  char buf[32];
  snprintf(buf, sizeof(buf), "CH: %d %s  TOT:%lu", s_channel, s_autoHop ? "AUTO" : "MAN", (unsigned long)s_totalPkts);
  u8g2.drawStr(4, 25, buf);


  snprintf(buf, sizeof(buf), "BCN: %lu   DATA:%lu", (unsigned long)s_beaconPkts, (unsigned long)s_dataPkts);
  u8g2.drawStr(4, 37, buf);


  snprintf(buf, sizeof(buf), "MGMT:%lu   CTRL:%lu", (unsigned long)s_mgmtPkts, (unsigned long)s_ctrlPkts);
  u8g2.drawStr(4, 49, buf);


  snprintf(buf, sizeof(buf), "EAPL:%lu  %s %ddBm", (unsigned long)s_eapolPkts, s_lastType, (int)s_lastRssi);
  u8g2.drawStr(4, 61, buf);
}

}
