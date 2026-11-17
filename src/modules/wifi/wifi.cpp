#include "wifi.h"
#include "scene_wifi_attacks.h"
#include "scene_wifi_scan.h"
#include "scene_packet_monitor.h"
#include "scene_rssi_radar.h"
#include "scene_wardriving.h"
#include "scene_jam_detect.h"
#include "scene_sniffer.h"
#include "scene_pmkid.h"
#include <WiFi.h>
#include <math.h>

namespace {
static const char* kMenuItems[] = {
  "Scan",
  "Wi-Fi Attacks",
  "Capture PMKID",
  "Packet Monitor",
  "RSSID Radar",
  "Wardriving",
  "Jam Detect",
  "Sniffer"
};
static const int kMenuCount = sizeof(kMenuItems) / sizeof(kMenuItems[0]);
static const int kRowH = 12;
static const int kStartY = 16;

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
}

void WifiApp::open() {
  scene_ = SCENE_MENU;
  menuSel_ = 0;
  barY_ = (float)kStartY;
  scroll_ = 0.0f;
  targetScroll_ = 0.0f;
  WiFi.mode(WIFI_STA);
  Serial.println("[wifi] module opened");
}

void WifiApp::close() {
  if (scene_ == SCENE_ATTACKS) scene_wifi_attacks::close();
  else if (scene_ == SCENE_SCAN) scene_wifi_scan::close();
  else if (scene_ == SCENE_PMKID) scene_pmkid::close();
  else if (scene_ == SCENE_PACKET_MONITOR) scene_packet_monitor::close();
  else if (scene_ == SCENE_RSSI_RADAR) scene_rssi_radar::close();
  else if (scene_ == SCENE_WARDRIVING) scene_wardriving::close();
  else if (scene_ == SCENE_JAM_DETECT) scene_jam_detect::close();
  else if (scene_ == SCENE_SNIFFER) scene_sniffer::close();

  WiFi.scanDelete();
  WiFi.mode(WIFI_OFF);
  scene_ = SCENE_MENU;
  Serial.println("[wifi] module closed");
}

bool WifiApp::onBack() {
  if (scene_ == SCENE_ATTACKS) {
    if (scene_wifi_attacks::onBack()) return true;
    scene_wifi_attacks::close();
    scene_ = SCENE_MENU;
    return true;
  } else if (scene_ == SCENE_SCAN) {
    if (scene_wifi_scan::onBack()) return true;
    scene_wifi_scan::close();
    scene_ = SCENE_MENU;
    return true;
  } else if (scene_ == SCENE_PMKID) {
    if (scene_pmkid::onBack()) return true;
    scene_pmkid::close();
    scene_ = SCENE_MENU;
    return true;
  } else if (scene_ == SCENE_PACKET_MONITOR) {
    if (scene_packet_monitor::onBack()) return true;
    scene_packet_monitor::close();
    scene_ = SCENE_MENU;
    return true;
  } else if (scene_ == SCENE_RSSI_RADAR) {
    if (scene_rssi_radar::onBack()) return true;
    scene_rssi_radar::close();
    scene_ = SCENE_MENU;
    return true;
  } else if (scene_ == SCENE_WARDRIVING) {
    if (scene_wardriving::onBack()) return true;
    scene_wardriving::close();
    scene_ = SCENE_MENU;
    return true;
  } else if (scene_ == SCENE_JAM_DETECT) {
    if (scene_jam_detect::onBack()) return true;
    scene_jam_detect::close();
    scene_ = SCENE_MENU;
    return true;
  } else if (scene_ == SCENE_SNIFFER) {
    if (scene_sniffer::onBack()) return true;
    scene_sniffer::close();
    scene_ = SCENE_MENU;
    return true;
  }
  return false;
}

void WifiApp::tick(uint32_t now) {
  if (scene_ == SCENE_MENU) {
    updateScroll(menuSel_, kMenuCount, scroll_, targetScroll_, barY_);
  } else if (scene_ == SCENE_ATTACKS) {
    scene_wifi_attacks::tick(now);
  } else if (scene_ == SCENE_SCAN) {
    scene_wifi_scan::tick(now);
  } else if (scene_ == SCENE_PMKID) {
    scene_pmkid::tick(now);
  } else if (scene_ == SCENE_PACKET_MONITOR) {
    scene_packet_monitor::tick(now);
  } else if (scene_ == SCENE_RSSI_RADAR) {
    scene_rssi_radar::tick(now);
  } else if (scene_ == SCENE_WARDRIVING) {
    scene_wardriving::tick(now);
  } else if (scene_ == SCENE_JAM_DETECT) {
    scene_jam_detect::tick(now);
  } else if (scene_ == SCENE_SNIFFER) {
    scene_sniffer::tick(now);
  }
}

void WifiApp::onKey(const KeyEvent& ev) {
  if (scene_ == SCENE_MENU) {
    if (ev.repeat) return;
    if (ev.key == BTN_UP) {
      menuSel_ = (menuSel_ - 1 + kMenuCount) % kMenuCount;
    } else if (ev.key == BTN_DOWN) {
      menuSel_ = (menuSel_ + 1) % kMenuCount;
    } else if (ev.key == BTN_OK) {
      if (menuSel_ == 0) {
        scene_ = SCENE_SCAN;
        scene_wifi_scan::open();
      } else if (menuSel_ == 1) {
        scene_ = SCENE_ATTACKS;
        scene_wifi_attacks::open();
      } else if (menuSel_ == 2) {
        scene_ = SCENE_PMKID;
        scene_pmkid::open();
      } else if (menuSel_ == 3) {
        scene_ = SCENE_PACKET_MONITOR;
        scene_packet_monitor::open();
      } else if (menuSel_ == 4) {
        scene_ = SCENE_RSSI_RADAR;
        scene_rssi_radar::open();
      } else if (menuSel_ == 5) {
        scene_ = SCENE_WARDRIVING;
        scene_wardriving::open();
      } else if (menuSel_ == 6) {
        scene_ = SCENE_JAM_DETECT;
        scene_jam_detect::open();
      } else if (menuSel_ == 7) {
        scene_ = SCENE_SNIFFER;
        scene_sniffer::open();
      }
    }
  } else if (scene_ == SCENE_ATTACKS) {
    scene_wifi_attacks::onKey(ev);
  } else if (scene_ == SCENE_SCAN) {
    scene_wifi_scan::onKey(ev);
  } else if (scene_ == SCENE_PMKID) {
    scene_pmkid::onKey(ev);
  } else if (scene_ == SCENE_PACKET_MONITOR) {
    scene_packet_monitor::onKey(ev);
  } else if (scene_ == SCENE_RSSI_RADAR) {
    scene_rssi_radar::onKey(ev);
  } else if (scene_ == SCENE_WARDRIVING) {
    scene_wardriving::onKey(ev);
  } else if (scene_ == SCENE_JAM_DETECT) {
    scene_jam_detect::onKey(ev);
  } else if (scene_ == SCENE_SNIFFER) {
    scene_sniffer::onKey(ev);
  }
}

void WifiApp::draw(U8G2& u8g2, uint32_t now) {
  if (scene_ == SCENE_MENU) {

    u8g2.setClipWindow(0, 14, 128, 64);


    u8g2.setFont(u8g2_font_6x12_tr);
    u8g2.setDrawColor(1);
    for (int i = 0; i < kMenuCount; i++) {
      int y = kStartY + (int)(i * kRowH - scroll_ + 0.5f);
      if (y < kStartY - kRowH || y > 64) continue;
      u8g2.drawStr(6, y + 10, kMenuItems[i]);
    }


    u8g2.setDrawColor(2);
    int curY = (int)(barY_ + 0.5f);
    if (curY < kStartY) curY = kStartY;
    if (curY > 64 - kRowH) curY = 64 - kRowH;
    u8g2.drawBox(2, curY, 124, kRowH);
    u8g2.setDrawColor(1);

    u8g2.setMaxClipWindow();


    drawHeader(u8g2, "WI-FI");
  } else if (scene_ == SCENE_ATTACKS) {
    scene_wifi_attacks::draw(u8g2, now);
  } else if (scene_ == SCENE_SCAN) {
    scene_wifi_scan::draw(u8g2, now);
  } else if (scene_ == SCENE_PMKID) {
    scene_pmkid::draw(u8g2, now);
  } else if (scene_ == SCENE_PACKET_MONITOR) {
    scene_packet_monitor::draw(u8g2, now);
  } else if (scene_ == SCENE_RSSI_RADAR) {
    scene_rssi_radar::draw(u8g2, now);
  } else if (scene_ == SCENE_WARDRIVING) {
    scene_wardriving::draw(u8g2, now);
  } else if (scene_ == SCENE_JAM_DETECT) {
    scene_jam_detect::draw(u8g2, now);
  } else if (scene_ == SCENE_SNIFFER) {
    scene_sniffer::draw(u8g2, now);
  }
}
