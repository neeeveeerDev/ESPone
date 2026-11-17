#include "scene_rssi_radar.h"
#include <WiFi.h>
#include <esp_wifi.h>
#include <vector>
#include <math.h>
#include "scene_wifi_scan.h"

namespace scene_rssi_radar {

enum RadarState : uint8_t {
  STATE_PICK_TARGET = 0,
  STATE_TRACKING
};

static const int kRowH = 12;
static const int kStartY = 16;
static const int kSpecWidth = 57;

static RadarState s_state = STATE_PICK_TARGET;


static char s_targetSsid[33] = "";
static uint8_t s_targetBssid[6] = {0};
static char s_targetBssidStr[18] = "";
static uint8_t s_targetChannel = 1;
static volatile int32_t s_liveRssi = -75;
static float s_smoothedRssi = -75.0f;
static float s_distanceMeters = 5.0f;
static uint32_t s_lastPktTime = 0;
static uint32_t s_lastScanCheck = 0;


static std::vector<scene_wifi_scan::WifiApItem> s_pickerList;
static int s_pickerSel = 0;
static float s_barY = (float)kStartY;
static float s_scroll = 0.0f;
static float s_targetScroll = 0.0f;


static uint8_t s_spectrogram[kSpecWidth];
static uint32_t s_lastSpecTick = 0;

static void drawHeader(U8G2& u8g2, const char* title, const char* badge = nullptr) {
  u8g2.setDrawColor(1);
  u8g2.drawBox(0, 0, 128, 14);
  u8g2.setDrawColor(0);
  u8g2.setFont(u8g2_font_7x14B_tr);
  u8g2.drawStr(4, 11, title);

  if (badge && strlen(badge) > 0) {
    u8g2.setFont(u8g2_font_5x8_tr);
    int bw = u8g2.getStrWidth(badge);
    u8g2.drawStr(124 - bw, 10, badge);
  }
  u8g2.setDrawColor(1);
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

static bool parseMac(const char* str, uint8_t* out) {
  int vals[6];
  if (sscanf(str, "%x:%x:%x:%x:%x:%x", &vals[0], &vals[1], &vals[2], &vals[3], &vals[4], &vals[5]) == 6) {
    for (int i = 0; i < 6; i++) out[i] = (uint8_t)vals[i];
    return true;
  }
  return false;
}

static void IRAM_ATTR wifi_radar_cb(void* buf, wifi_promiscuous_pkt_type_t type) {
  const wifi_promiscuous_pkt_t* pkt = (wifi_promiscuous_pkt_t*)buf;
  if (pkt->rx_ctrl.sig_len >= 24) {
    const uint8_t* srcMac = pkt->payload + 10;
    if (memcmp(srcMac, s_targetBssid, 6) == 0) {
      s_liveRssi = pkt->rx_ctrl.rssi;
      s_lastPktTime = millis();
      return;
    }
    const uint8_t* bssid = pkt->payload + 16;
    if (memcmp(bssid, s_targetBssid, 6) == 0) {
      s_liveRssi = pkt->rx_ctrl.rssi;
      s_lastPktTime = millis();
    }
  }
}

static void startTrackingTarget() {
  s_state = STATE_TRACKING;
  s_smoothedRssi = (float)s_liveRssi;
  s_lastPktTime = millis();
  memset(s_spectrogram, 0, sizeof(s_spectrogram));

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  esp_wifi_set_promiscuous(false);
  esp_wifi_set_promiscuous_rx_cb(&wifi_radar_cb);
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(s_targetChannel, WIFI_SECOND_CHAN_NONE);
}

static void scanForPicker() {
  s_state = STATE_PICK_TARGET;
  s_pickerList.clear();
  s_pickerSel = 0;
  s_barY = (float)kStartY;
  s_scroll = 0.0f;
  s_targetScroll = 0.0f;

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  int n = WiFi.scanNetworks(false, true, false, 120);
  for (int i = 0; i < n; i++) {
    scene_wifi_scan::WifiApItem item;
    String ssid = WiFi.SSID(i);
    snprintf(item.ssid, sizeof(item.ssid), "%s", ssid.length() > 0 ? ssid.c_str() : "<Hidden>");
    snprintf(item.bssidStr, sizeof(item.bssidStr), "%s", WiFi.BSSIDstr(i).c_str());
    item.channel = WiFi.channel(i);
    item.rssi = WiFi.RSSI(i);
    item.authMode = (uint8_t)WiFi.encryptionType(i);
    s_pickerList.push_back(item);
  }
  WiFi.scanDelete();
}

void open() {
  memset(s_spectrogram, 0, sizeof(s_spectrogram));
  s_lastSpecTick = millis();
  s_lastScanCheck = millis();

  if (scene_wifi_scan::hasSelectedTarget()) {
    const auto& target = scene_wifi_scan::getSelectedTarget();
    snprintf(s_targetSsid, sizeof(s_targetSsid), "%s", target.ssid);
    snprintf(s_targetBssidStr, sizeof(s_targetBssidStr), "%s", target.bssidStr);
    s_targetChannel = target.channel;
    s_liveRssi = target.rssi;
    parseMac(s_targetBssidStr, s_targetBssid);
    startTrackingTarget();
  } else {
    scanForPicker();
  }
}

void close() {
  esp_wifi_set_promiscuous(false);
}

bool onBack() {
  if (s_state == STATE_TRACKING) {
    esp_wifi_set_promiscuous(false);
    s_state = STATE_PICK_TARGET;
    scanForPicker();
    return true;
  }
  return false;
}

void tick(uint32_t now) {
  if (s_state == STATE_PICK_TARGET) {
    int count = (int)s_pickerList.size();
    if (count > 0) {
      if (s_pickerSel >= count) s_pickerSel = count - 1;
      updateScroll(s_pickerSel, count, s_scroll, s_targetScroll, s_barY);
    }
    return;
  }


  if (now - s_lastPktTime >= 1200 && now - s_lastScanCheck >= 1200) {
    s_lastScanCheck = now;
    if (s_liveRssi > -95) s_liveRssi--;
  }


  s_smoothedRssi = s_smoothedRssi * 0.8f + (float)s_liveRssi * 0.2f;


  float expVal = ((-42.0f) - s_smoothedRssi) / (10.0f * 2.4f);
  s_distanceMeters = powf(10.0f, expVal);
  if (s_distanceMeters < 0.2f) s_distanceMeters = 0.2f;
  if (s_distanceMeters > 99.0f) s_distanceMeters = 99.0f;


  if (now - s_lastSpecTick >= 100) {
    s_lastSpecTick = now;


    float norm = (s_smoothedRssi + 95.0f) / 65.0f;
    if (norm < 0.05f) norm = 0.05f;
    if (norm > 1.0f) norm = 1.0f;
    uint8_t barH = (uint8_t)(norm * 42.0f);

    for (int i = 0; i < kSpecWidth - 1; i++) {
      s_spectrogram[i] = s_spectrogram[i + 1];
    }
    s_spectrogram[kSpecWidth - 1] = barH;
  }
}

void onKey(const KeyEvent& ev) {
  if (s_state == STATE_PICK_TARGET) {
    int count = (int)s_pickerList.size();
    if (count == 0) {
      if (ev.key == BTN_OK && !ev.repeat) scanForPicker();
      return;
    }
    if (ev.key == BTN_UP) {
      s_pickerSel = (s_pickerSel - 1 + count) % count;
    } else if (ev.key == BTN_DOWN) {
      s_pickerSel = (s_pickerSel + 1) % count;
    } else if (ev.key == BTN_OK && !ev.repeat) {
      const auto& item = s_pickerList[s_pickerSel];
      snprintf(s_targetSsid, sizeof(s_targetSsid), "%s", item.ssid);
      snprintf(s_targetBssidStr, sizeof(s_targetBssidStr), "%s", item.bssidStr);
      s_targetChannel = item.channel;
      s_liveRssi = item.rssi;
      parseMac(s_targetBssidStr, s_targetBssid);
      startTrackingTarget();
    }
  } else if (s_state == STATE_TRACKING) {
    if (ev.key == BTN_OK && !ev.repeat) {
      esp_wifi_set_promiscuous(false);
      s_state = STATE_PICK_TARGET;
      scanForPicker();
    }
  }
}

void draw(U8G2& u8g2, uint32_t now) {
  if (s_state == STATE_PICK_TARGET) {
    int count = (int)s_pickerList.size();
    drawHeader(u8g2, "SELECT TARGET", count > 0 ? "SCAN" : "");

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

      const auto& item = s_pickerList[i];


      char rBuf[12];
      snprintf(rBuf, sizeof(rBuf), "%d dBm", (int)item.rssi);
      u8g2.setFont(u8g2_font_5x8_tr);
      int rw = u8g2.getStrWidth(rBuf);
      u8g2.drawStr(124 - rw, y + 9, rBuf);


      u8g2.setFont(u8g2_font_6x12_tr);
      int maxW = 124 - rw - 8;
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
    return;
  }




  char badge[16];
  snprintf(badge, sizeof(badge), "CH:%d", s_targetChannel);
  drawHeader(u8g2, "RADAR", badge);


  u8g2.drawVLine(61, 14, 50);



  u8g2.setFont(u8g2_font_5x8_tr);
  char shortSsid[12];
  strncpy(shortSsid, s_targetSsid, 10);
  shortSsid[10] = '\0';
  u8g2.drawStr(4, 23, shortSsid);


  char distBuf[16];
  if (s_distanceMeters < 10.0f) {
    snprintf(distBuf, sizeof(distBuf), "~%.1fm", s_distanceMeters);
  } else {
    snprintf(distBuf, sizeof(distBuf), "~%.0fm", s_distanceMeters);
  }
  u8g2.setFont(u8g2_font_7x14B_tr);
  u8g2.drawStr(4, 42, distBuf);


  u8g2.setFont(u8g2_font_6x10_tr);
  char rssiBuf[16];
  snprintf(rssiBuf, sizeof(rssiBuf), "%d dBm", (int)s_smoothedRssi);
  u8g2.drawStr(4, 58, rssiBuf);


  const int specX = 64;
  const int specBaseY = 62;


  u8g2.drawFrame(specX - 1, 16, kSpecWidth + 2, 47);


  for (int i = 0; i < kSpecWidth; i++) {
    uint8_t h = s_spectrogram[i];
    if (h > 45) h = 45;
    if (h > 0) {
      u8g2.drawVLine(specX + i, specBaseY - h, h);
    }
  }


  for (int i = 0; i < kSpecWidth - 1; i++) {
    uint8_t h1 = s_spectrogram[i];
    uint8_t h2 = s_spectrogram[i + 1];
    if (h1 > 45) h1 = 45;
    if (h2 > 45) h2 = 45;
    u8g2.drawLine(specX + i, specBaseY - h1, specX + i + 1, specBaseY - h2);
  }
}

}
