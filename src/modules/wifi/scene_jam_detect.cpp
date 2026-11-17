#include "scene_jam_detect.h"
#include <WiFi.h>
#include <esp_wifi.h>

namespace scene_jam_detect {


static const uint8_t JD_CHANNELS[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13};
static const int JD_NCH = sizeof(JD_CHANNELS) / sizeof(JD_CHANNELS[0]);
static const uint16_t JD_DWELL = 100;

static volatile uint32_t jd_deauth = 0;
static volatile uint32_t jd_total = 0;

static uint16_t s_dps[14] = {0};
static uint16_t s_peak[14] = {0};
static uint32_t s_threshold = 10;
static int s_chIdx = 0;
static uint8_t s_curCh = 1;
static uint32_t s_dwellStart = 0;

static int s_attackCh = -1;
static uint32_t s_attackExpiry = 0;
static uint32_t s_lastDecay = 0;


static void IRAM_ATTR jd_rx_cb(void *buf, wifi_promiscuous_pkt_type_t type) {
  const wifi_promiscuous_pkt_t *pkt = (const wifi_promiscuous_pkt_t *)buf;
  if (!pkt) return;
  jd_total++;
  if (pkt->rx_ctrl.sig_len < 2) return;
  const uint8_t *f = pkt->payload;
  uint16_t fc = (uint16_t)f[0] | ((uint16_t)f[1] << 8);
  uint8_t ftype = (fc & 0x0C) >> 2;
  uint8_t fsub = (fc & 0xF0) >> 4;
  if (ftype == 0x00 && (fsub == 0x0C || fsub == 0x0A)) {
    jd_deauth++;
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
  memset(s_dps, 0, sizeof(s_dps));
  memset(s_peak, 0, sizeof(s_peak));
  s_threshold = 10;
  s_chIdx = 0;
  s_curCh = JD_CHANNELS[0];
  s_attackCh = -1;
  s_attackExpiry = 0;
  jd_deauth = 0;
  jd_total = 0;
  s_dwellStart = millis();
  s_lastDecay = millis();

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  esp_wifi_set_promiscuous(false);


  wifi_promiscuous_filter_t filt = {};
  filt.filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT | WIFI_PROMIS_FILTER_MASK_DATA;
  esp_wifi_set_promiscuous_filter(&filt);
  esp_wifi_set_promiscuous_rx_cb(jd_rx_cb);
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(s_curCh, WIFI_SECOND_CHAN_NONE);
}

void close() {
  esp_wifi_set_promiscuous(false);
  esp_wifi_set_promiscuous_rx_cb(NULL);
}

void tick(uint32_t now) {
  uint32_t elapsed = now - s_dwellStart;
  uint32_t live = (elapsed > 0) ? ((uint32_t)jd_deauth * 1000UL / JD_DWELL) : 0;


  if (elapsed >= JD_DWELL || live >= s_threshold) {
    uint32_t d = (uint32_t)jd_deauth * 1000UL / JD_DWELL;
    if (d > 65535) d = 65535;
    s_dps[s_curCh] = (uint16_t)d;
    if (s_dps[s_curCh] > s_peak[s_curCh]) s_peak[s_curCh] = s_dps[s_curCh];

    if (s_dps[s_curCh] >= s_threshold) {
      s_attackCh = s_curCh;
      s_attackExpiry = now + 2500;
    }


    s_chIdx = (s_chIdx + 1) % JD_NCH;
    s_curCh = JD_CHANNELS[s_chIdx];
    esp_wifi_set_channel(s_curCh, WIFI_SECOND_CHAN_NONE);
    jd_deauth = 0;
    jd_total = 0;
    s_dwellStart = now;
  }


  if (s_attackCh >= 0 && now >= s_attackExpiry) {
    s_attackCh = -1;
  }


  if (now - s_lastDecay >= 800) {
    s_lastDecay = now;
    for (int i = 1; i <= 13; i++) {
      if (s_dps[i] > 0) s_dps[i] = (s_dps[i] * 3) / 4;
    }
  }
}

void onKey(const KeyEvent& ev) {

}

bool onBack() {
  return false;
}

void draw(U8G2& u8g2, uint32_t now) {
  drawHeader(u8g2, "JAM DETECT");


  u8g2.setFont(u8g2_font_6x12_tr);
  if (s_attackCh >= 0) {
    u8g2.drawBox(2, 16, 124, 10);
    u8g2.setDrawColor(0);
    char atkBuf[32];
    snprintf(atkBuf, sizeof(atkBuf), "ATTACK CH %d  %u/s", s_attackCh, (unsigned int)s_dps[s_attackCh]);
    u8g2.drawStr(4, 25, atkBuf);
    u8g2.setDrawColor(1);
  } else {
    u8g2.drawStr(4, 25, "STATUS: NO JAMMING");
  }


  const int kBaseY = 52;
  const int kMaxH = 26;
  const int kBarW = 7;
  const int kGap = 2;
  const int kStartX = 6;

  for (int i = 0; i < JD_NCH; i++) {
    uint8_t ch = JD_CHANNELS[i];
    int x = kStartX + (ch - 1) * (kBarW + kGap);
    int h = 0;

    if (s_attackCh >= 0) {

      h = kMaxH;
    } else if (s_dps[ch] > 0) {

      h = (int)((uint32_t)s_dps[ch] * kMaxH / (s_threshold * 2));
      if (h < 2) h = 2;
      if (h > kMaxH) h = kMaxH;
    }

    if (h > 0) {
      u8g2.drawBox(x, kBaseY - h, kBarW, h);
    } else {

      u8g2.drawHLine(x, kBaseY, kBarW);
    }
  }


  u8g2.setFont(u8g2_font_4x6_tr);
  for (int i = 0; i < JD_NCH; i++) {
    uint8_t ch = JD_CHANNELS[i];
    int x = kStartX + (ch - 1) * (kBarW + kGap);
    char numBuf[4];
    itoa(ch, numBuf, 10);
    int nx = (ch < 10) ? (x + 2) : x;
    u8g2.drawStr(nx, 61, numBuf);


    if (ch == s_curCh && s_attackCh < 0) {
      u8g2.drawHLine(x, 63, kBarW);
    }
  }
}

}
