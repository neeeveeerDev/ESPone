#include "scene_packet_monitor.h"
#include <WiFi.h>
#include <esp_wifi.h>

namespace scene_packet_monitor {

static volatile uint32_t s_rawPackets = 0;
static volatile uint64_t s_rawBytes = 0;
static uint32_t s_totalPackets = 0;
static uint64_t s_totalBytes = 0;
static uint32_t s_pktsPerSec = 0;
static uint32_t s_lastCalc = 0;

static uint8_t s_channel = 1;
static bool s_autoMode = false;
static uint32_t s_lastChannelSwitch = 0;


static const int kGraphW = 74;
static const int kGraphH = 34;
static uint8_t s_history[kGraphW];

static void IRAM_ATTR wifi_promiscuous_cb(void* buf, wifi_promiscuous_pkt_type_t type) {
  const wifi_promiscuous_pkt_t* pkt = (wifi_promiscuous_pkt_t*)buf;
  s_rawPackets++;
  s_rawBytes += pkt->rx_ctrl.sig_len;
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
  s_rawPackets = 0;
  s_rawBytes = 0;
  s_totalPackets = 0;
  s_totalBytes = 0;
  s_pktsPerSec = 0;
  s_channel = 1;
  s_autoMode = false;
  s_lastCalc = millis();
  s_lastChannelSwitch = millis();
  memset(s_history, 0, sizeof(s_history));

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  esp_wifi_set_promiscuous(false);
  esp_wifi_set_promiscuous_rx_cb(&wifi_promiscuous_cb);
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(s_channel, WIFI_SECOND_CHAN_NONE);
}

void close() {
  esp_wifi_set_promiscuous(false);
}

bool onBack() {
  close();
  return false;
}

void tick(uint32_t now) {

  if (s_autoMode && (now - s_lastChannelSwitch >= 10000)) {
    s_lastChannelSwitch = now;
    s_channel++;
    if (s_channel > 13) s_channel = 1;
    esp_wifi_set_channel(s_channel, WIFI_SECOND_CHAN_NONE);
  }


  if (now - s_lastCalc >= 150) {
    uint32_t dt = now - s_lastCalc;
    s_lastCalc = now;

    uint32_t pkts = s_rawPackets;
    s_rawPackets = 0;
    uint64_t bytes = s_rawBytes;
    s_rawBytes = 0;

    s_totalPackets += pkts;
    s_totalBytes += bytes;

    s_pktsPerSec = (pkts * 1000UL) / dt;


    uint8_t barH = (uint8_t)(s_pktsPerSec / 10);
    if (barH > kGraphH - 2) barH = kGraphH - 2;


    for (int i = 0; i < kGraphW - 1; i++) {
      s_history[i] = s_history[i + 1];
    }
    s_history[kGraphW - 1] = barH;
  }
}

void onKey(const KeyEvent& ev) {
  if (ev.repeat) return;

  if (ev.key == BTN_OK) {

    s_autoMode = !s_autoMode;
    if (s_autoMode) {
      s_lastChannelSwitch = millis();
    }
  } else if (!s_autoMode) {

    if (ev.key == BTN_LEFT) {
      s_channel = (s_channel > 1) ? s_channel - 1 : 13;
      esp_wifi_set_channel(s_channel, WIFI_SECOND_CHAN_NONE);
    } else if (ev.key == BTN_RIGHT) {
      s_channel = (s_channel < 13) ? s_channel + 1 : 1;
      esp_wifi_set_channel(s_channel, WIFI_SECOND_CHAN_NONE);
    }
  }
}

void draw(U8G2& u8g2, uint32_t now) {

  drawHeader(u8g2, "PKT MONITOR");


  u8g2.setFont(u8g2_font_6x12_tr);


  char sizeBuf[16];
  if (s_totalBytes < 1024) {
    snprintf(sizeBuf, sizeof(sizeBuf), "%d B", (int)s_totalBytes);
  } else if (s_totalBytes < 1024 * 1024) {
    snprintf(sizeBuf, sizeof(sizeBuf), "%.1f K", (float)s_totalBytes / 1024.0f);
  } else {
    snprintf(sizeBuf, sizeof(sizeBuf), "%.2f M", (float)s_totalBytes / (1024.0f * 1024.0f));
  }
  u8g2.drawStr(4, 26, sizeBuf);


  char pktBuf[16];
  if (s_totalPackets < 10000) {
    snprintf(pktBuf, sizeof(pktBuf), "%d p", (int)s_totalPackets);
  } else {
    snprintf(pktBuf, sizeof(pktBuf), "%d k", (int)(s_totalPackets / 1000));
  }
  u8g2.setFont(u8g2_font_5x8_tr);
  u8g2.drawStr(4, 37, pktBuf);


  char spdBuf[16];
  snprintf(spdBuf, sizeof(spdBuf), "%d/s", (int)s_pktsPerSec);
  u8g2.drawStr(4, 47, spdBuf);


  const int graphX = 49;
  const int graphY = 16;
  const int baseY = graphY + kGraphH;


  u8g2.drawFrame(graphX - 1, graphY, kGraphW + 2, kGraphH);


  for (int i = 0; i < kGraphW; i++) {
    uint8_t h = s_history[i];
    if (h > 0) {
      u8g2.drawVLine(graphX + i, baseY - h - 1, h);
    }
  }


  char botBuf[32];
  if (s_autoMode) {
    int secLeft = 10 - (int)((now - s_lastChannelSwitch) / 1000);
    if (secLeft < 0) secLeft = 0;
    snprintf(botBuf, sizeof(botBuf), "AUTO (%ds)  CH:%d", secLeft, s_channel);
  } else {
    snprintf(botBuf, sizeof(botBuf), "MANUAL  CH:%d", s_channel);
  }
  u8g2.setFont(u8g2_font_5x8_tr);
  u8g2.drawStr(4, 62, botBuf);
}

}
