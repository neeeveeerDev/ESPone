#include "scene_ibeacon.h"
#include "core/ble_service.h"
#include <NimBLEDevice.h>
#include <NimBLEAdvertising.h>
#include <NimBLEScan.h>
#include <NimBLEAdvertisedDevice.h>
#include <math.h>
#include <vector>

namespace scene_ibeacon {

enum SubScene : uint8_t {
  SUB_MAIN = 0,
  SUB_PRESETS,
  SUB_TX_RUN,
  SUB_TX_SETUP,
  SUB_SNIFFER,
  SUB_SNIFFER_DETAIL
};

struct BeaconPreset {
  const char* name;
  uint8_t uuid[16];
  uint16_t major;
  uint16_t minor;
};

static const BeaconPreset kPresets[] = {
  {
    "AirLocate (Apple)",
    { 0xE2, 0xC5, 0x6D, 0xB5, 0xDF, 0xFB, 0x48, 0xD2,
      0xB0, 0x60, 0xD0, 0xF5, 0xA7, 0x10, 0x96, 0xE0 },
    100, 1
  },
  {
    "Estimote Default",
    { 0xB9, 0x40, 0x7F, 0x30, 0xF5, 0xF8, 0x46, 0x6E,
      0xAF, 0xF9, 0x25, 0x55, 0x6B, 0x57, 0xFE, 0x6D },
    1, 1
  },
  {
    "Kontakt.io",
    { 0xF7, 0x82, 0x6D, 0xA6, 0x4F, 0xA2, 0x4E, 0x98,
      0x80, 0x24, 0xBC, 0x5B, 0x71, 0xE0, 0x89, 0x3E },
    1000, 200
  },
  {
    "RadBeacon",
    { 0x2F, 0x23, 0x44, 0x54, 0xCF, 0x6D, 0x4A, 0x0F,
      0xAD, 0xF2, 0xF4, 0x91, 0x1B, 0xA9, 0xFF, 0xA6 },
    1, 1
  },
  {
    "ESPone Beacon",
    { 0xE5, 0x90, 0x00, 0x01, 0x00, 0x00, 0x40, 0x00,
      0x80, 0x00, 0x01, 0x23, 0x45, 0x67, 0x89, 0xAB },
    42, 1
  }
};
static const int kPresetCount = sizeof(kPresets) / sizeof(kPresets[0]);

static const char* kMainMenuItems[] = {
  "Broadcaster",
  "Presets",
  "Adjust Parameters",
  "Beacon Sniffer"
};
static const int kMainMenuCount = sizeof(kMainMenuItems) / sizeof(kMainMenuItems[0]);

static const int kRowH = 12;
static const int kStartY = 16;


static uint8_t s_activeUuid[16] = {
  0xE2, 0xC5, 0x6D, 0xB5, 0xDF, 0xFB, 0x48, 0xD2,
  0xB0, 0x60, 0xD0, 0xF5, 0xA7, 0x10, 0x96, 0xE0
};
static char s_activeName[24] = "AirLocate (Apple)";
static uint16_t s_major = 100;
static uint16_t s_minor = 1;
static int8_t s_measuredPower = -59;
static bool s_isBroadcasting = false;
static uint32_t s_txStartTime = 0;
static uint32_t s_txPacketCount = 0;


struct SniffedBeacon {
  uint8_t uuid[16];
  uint16_t major;
  uint16_t minor;
  int8_t txPower;
  int rssi;
  float distance;
  char mac[18];
  uint32_t lastSeen;
};
static std::vector<SniffedBeacon> s_sniffList;
static SemaphoreHandle_t s_sniffMutex = nullptr;
static bool s_isSniffing = false;
static int s_sniffSel = 0;
static float s_sniffScroll = 0.0f;
static float s_sniffTargetScroll = 0.0f;
static float s_sniffBarY = 16.0f;


static SubScene s_subScene = SUB_MAIN;
static int s_mainSel = 0;
static float s_mainScroll = 0.0f;
static float s_mainTargetScroll = 0.0f;
static float s_mainBarY = 16.0f;

static int s_presetSel = 0;
static float s_presetScroll = 0.0f;
static float s_presetTargetScroll = 0.0f;
static float s_presetBarY = 16.0f;

static int s_configParamSel = 0;


static void formatUuid(const uint8_t* u, char* out, size_t outSize) {
  snprintf(out, outSize,
           "%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X",
           u[0], u[1], u[2], u[3], u[4], u[5], u[6], u[7],
           u[8], u[9], u[10], u[11], u[12], u[13], u[14], u[15]);
}




static void startAdvertising() {
  ble_service::pauseAdvertising();

  NimBLEAdvertising* pAdv = NimBLEDevice::getAdvertising();
  if (pAdv) {
    pAdv->stop();

    NimBLEAdvertisementData advData;
    advData.setFlags(0x06);


    std::string mfg;
    mfg.reserve(25);
    mfg.push_back((char)0x4C);
    mfg.push_back((char)0x00);
    mfg.push_back((char)0x02);
    mfg.push_back((char)0x15);
    for (int i = 0; i < 16; i++) {
      mfg.push_back((char)s_activeUuid[i]);
    }
    mfg.push_back((char)((s_major >> 8) & 0xFF));
    mfg.push_back((char)(s_major & 0xFF));
    mfg.push_back((char)((s_minor >> 8) & 0xFF));
    mfg.push_back((char)(s_minor & 0xFF));
    mfg.push_back((char)s_measuredPower);

    advData.setManufacturerData(mfg);

    pAdv->setAdvertisementData(advData);
    pAdv->setMinPreferred(0x06);
    pAdv->setMaxPreferred(0x12);
    pAdv->start();

    s_isBroadcasting = true;
    s_txStartTime = millis();
    s_txPacketCount = 0;
    Serial.printf("[iBeacon] Broadcast started: Major=%d Minor=%d\n", s_major, s_minor);
  }
}

static void stopAdvertising() {
  NimBLEAdvertising* pAdv = NimBLEDevice::getAdvertising();
  if (pAdv) {
    pAdv->stop();
  }
  s_isBroadcasting = false;
  ble_service::resumeAdvertising();
  Serial.println("[iBeacon] Broadcast stopped");
}




class BeaconScanCallbacks : public NimBLEAdvertisedDeviceCallbacks {
  void onResult(NimBLEAdvertisedDevice* dev) override {
    if (!s_isSniffing || !s_sniffMutex) return;

    if (!dev) return;
    if (dev->haveManufacturerData()) {
      std::string mfg = dev->getManufacturerData();

      if (mfg.length() >= 25 &&
          (uint8_t)mfg[0] == 0x4C && (uint8_t)mfg[1] == 0x00 &&
          (uint8_t)mfg[2] == 0x02 && (uint8_t)mfg[3] == 0x15) {

        uint8_t uuid[16];
        memcpy(uuid, mfg.data() + 4, 16);
        uint16_t major = ((uint8_t)mfg[20] << 8) | (uint8_t)mfg[21];
        uint16_t minor = ((uint8_t)mfg[22] << 8) | (uint8_t)mfg[23];
        int8_t txPwr = (int8_t)mfg[24];
        int rssi = dev->getRSSI();


        float ratio = (float)(txPwr - rssi) / 20.0f;
        float dist = powf(10.0f, ratio);

        std::string macStr = dev->getAddress().toString();

        if (xSemaphoreTake(s_sniffMutex, pdMS_TO_TICKS(30)) == pdTRUE) {
          bool found = false;
          for (size_t i = 0; i < s_sniffList.size(); i++) {
            if (s_sniffList[i].major == major && s_sniffList[i].minor == minor &&
                memcmp(s_sniffList[i].uuid, uuid, 16) == 0) {
              found = true;
              s_sniffList[i].rssi = rssi;
              s_sniffList[i].distance = dist;
              s_sniffList[i].lastSeen = millis();
              break;
            }
          }

          if (!found && s_sniffList.size() < 30) {
            SniffedBeacon item;
            memcpy(item.uuid, uuid, 16);
            item.major = major;
            item.minor = minor;
            item.txPower = txPwr;
            item.rssi = rssi;
            item.distance = dist;
            strncpy(item.mac, macStr.c_str(), sizeof(item.mac) - 1);
            item.mac[sizeof(item.mac) - 1] = '\0';
            item.lastSeen = millis();
            s_sniffList.push_back(item);
          }
          xSemaphoreGive(s_sniffMutex);
        }
      }
    }
  }
};
static BeaconScanCallbacks* s_scanCb = nullptr;

static void sniffCompleteCB(NimBLEScanResults res) {
  (void)res;
  if (s_isSniffing) {
    NimBLEScan* pScan = NimBLEDevice::getScan();
    if (pScan) {
      pScan->start(3, sniffCompleteCB, false);
    }
  }
}

static void startSniffer() {
  ble_service::pauseAdvertising();
  if (!s_sniffMutex) s_sniffMutex = xSemaphoreCreateMutex();
  s_sniffList.clear();
  s_isSniffing = true;
  s_sniffSel = 0;

  NimBLEScan* pScan = NimBLEDevice::getScan();
  if (pScan) {
    if (!s_scanCb) s_scanCb = new BeaconScanCallbacks();
    pScan->setAdvertisedDeviceCallbacks(s_scanCb, true);
    pScan->setActiveScan(true);
    pScan->setInterval(100);
    pScan->setWindow(99);
    pScan->start(3, sniffCompleteCB, false);
    Serial.println("[iBeacon] Sniffer started");
  }
}

static void stopSniffer() {
  s_isSniffing = false;
  NimBLEScan* pScan = NimBLEDevice::getScan();
  if (pScan) {
    pScan->stop();
  }
  ble_service::resumeAdvertising();
  Serial.println("[iBeacon] Sniffer stopped");
}




static void updateScroll(int sel, int count, float& scroll, float& targetScroll, float& barY) {
  const int maxLines = 4;
  int maxScroll = count - maxLines;
  if (maxScroll < 0) maxScroll = 0;

  int topIndex = (int)(targetScroll / kRowH + 0.5f);
  if (sel < topIndex) targetScroll = (float)(sel * kRowH);
  else if (sel >= topIndex + maxLines) targetScroll = (float)((sel - maxLines + 1) * kRowH);

  if (targetScroll < 0.0f) targetScroll = 0.0f;
  if (targetScroll > (float)(maxScroll * kRowH)) targetScroll = (float)(maxScroll * kRowH);

  float ds = targetScroll - scroll;
  if (fabsf(ds) > 0.02f) scroll += ds * 0.35f;
  else scroll = targetScroll;

  float targetBarY = (float)kStartY + (float)(sel * kRowH) - scroll;
  float dy = targetBarY - barY;
  if (fabsf(dy) > 0.1f) barY += dy * 0.4f;
  else barY = targetBarY;
}




void open() {
  s_subScene = SUB_MAIN;
  s_mainSel = 0;
  s_mainScroll = 0.0f;
  s_mainTargetScroll = 0.0f;
  s_mainBarY = (float)kStartY;
  s_isBroadcasting = false;
  s_isSniffing = false;
}

void close() {
  if (s_isBroadcasting) stopAdvertising();
  if (s_isSniffing) stopSniffer();
  s_subScene = SUB_MAIN;
}

bool onBack() {
  if (s_subScene == SUB_TX_RUN) {
    if (s_isBroadcasting) stopAdvertising();
    s_subScene = SUB_MAIN;
    return true;
  }
  if (s_subScene == SUB_SNIFFER) {
    if (s_isSniffing) stopSniffer();
    s_subScene = SUB_MAIN;
    return true;
  }
  if (s_subScene == SUB_SNIFFER_DETAIL) {
    s_subScene = SUB_SNIFFER;
    return true;
  }
  if (s_subScene == SUB_PRESETS || s_subScene == SUB_TX_SETUP) {
    s_subScene = SUB_MAIN;
    return true;
  }
  return false;
}

void tick(uint32_t now) {
  if (s_subScene == SUB_MAIN) {
    updateScroll(s_mainSel, kMainMenuCount, s_mainScroll, s_mainTargetScroll, s_mainBarY);
  } else if (s_subScene == SUB_PRESETS) {
    updateScroll(s_presetSel, kPresetCount, s_presetScroll, s_presetTargetScroll, s_presetBarY);
  } else if (s_subScene == SUB_TX_RUN) {
    if (s_isBroadcasting) {

      static uint32_t lastPkt = 0;
      if (now - lastPkt >= 100) {
        lastPkt = now;
        s_txPacketCount++;
      }
    }
  } else if (s_subScene == SUB_SNIFFER) {
    int count = 0;
    if (s_sniffMutex && xSemaphoreTake(s_sniffMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
      count = (int)s_sniffList.size();
      xSemaphoreGive(s_sniffMutex);
    }
    updateScroll(s_sniffSel, count > 0 ? count : 1, s_sniffScroll, s_sniffTargetScroll, s_sniffBarY);
  }
}

void onKey(const KeyEvent& ev) {
  if (s_subScene == SUB_MAIN) {
    if (ev.key == BTN_UP) {
      s_mainSel = (s_mainSel - 1 + kMainMenuCount) % kMainMenuCount;
    } else if (ev.key == BTN_DOWN) {
      s_mainSel = (s_mainSel + 1) % kMainMenuCount;
    } else if (ev.key == BTN_OK && !ev.repeat) {
      switch (s_mainSel) {
        case 0:
          s_subScene = SUB_TX_RUN;
          startAdvertising();
          break;
        case 1:
          s_subScene = SUB_PRESETS;
          s_presetSel = 0;
          break;
        case 2:
          s_subScene = SUB_TX_SETUP;
          s_configParamSel = 0;
          break;
        case 3:
          s_subScene = SUB_SNIFFER;
          startSniffer();
          break;
      }
    }
  } else if (s_subScene == SUB_PRESETS) {
    if (ev.key == BTN_UP) {
      s_presetSel = (s_presetSel - 1 + kPresetCount) % kPresetCount;
    } else if (ev.key == BTN_DOWN) {
      s_presetSel = (s_presetSel + 1) % kPresetCount;
    } else if (ev.key == BTN_OK && !ev.repeat) {

      const auto& p = kPresets[s_presetSel];
      memcpy(s_activeUuid, p.uuid, 16);
      s_major = p.major;
      s_minor = p.minor;
      strncpy(s_activeName, p.name, sizeof(s_activeName) - 1);
      s_activeName[sizeof(s_activeName) - 1] = '\0';
      s_subScene = SUB_TX_RUN;
      startAdvertising();
    }
  } else if (s_subScene == SUB_TX_RUN) {
    if (ev.key == BTN_OK && !ev.repeat) {
      if (s_isBroadcasting) stopAdvertising();
      else startAdvertising();
    }
  } else if (s_subScene == SUB_TX_SETUP) {
    if (ev.key == BTN_UP) {
      if (s_configParamSel > 0) s_configParamSel--;
      else s_configParamSel = 2;
    } else if (ev.key == BTN_DOWN) {
      if (s_configParamSel < 2) s_configParamSel++;
      else s_configParamSel = 0;
    } else if (ev.key == BTN_LEFT) {
      if (s_configParamSel == 0 && s_major > 0) s_major -= ev.repeat ? 10 : 1;
      else if (s_configParamSel == 1 && s_minor > 0) s_minor -= ev.repeat ? 10 : 1;
      else if (s_configParamSel == 2 && s_measuredPower > -100) s_measuredPower--;
    } else if (ev.key == BTN_RIGHT) {
      if (s_configParamSel == 0 && s_major < 65535) s_major += ev.repeat ? 10 : 1;
      else if (s_configParamSel == 1 && s_minor < 65535) s_minor += ev.repeat ? 10 : 1;
      else if (s_configParamSel == 2 && s_measuredPower < -30) s_measuredPower++;
    } else if (ev.key == BTN_OK && !ev.repeat) {
      s_subScene = SUB_TX_RUN;
      startAdvertising();
    }
  } else if (s_subScene == SUB_SNIFFER) {
    int count = (int)s_sniffList.size();
    if (ev.key == BTN_UP && count > 0) {
      s_sniffSel = (s_sniffSel - 1 + count) % count;
    } else if (ev.key == BTN_DOWN && count > 0) {
      s_sniffSel = (s_sniffSel + 1) % count;
    } else if (ev.key == BTN_OK && !ev.repeat && count > 0 && s_sniffSel < count) {
      s_subScene = SUB_SNIFFER_DETAIL;
    }
  } else if (s_subScene == SUB_SNIFFER_DETAIL) {
    if (ev.key == BTN_OK && !ev.repeat) {

      if (s_sniffSel < (int)s_sniffList.size()) {
        const auto& b = s_sniffList[s_sniffSel];
        memcpy(s_activeUuid, b.uuid, 16);
        s_major = b.major;
        s_minor = b.minor;
        s_measuredPower = b.txPower;
        snprintf(s_activeName, sizeof(s_activeName), "Cloned Beacon");
        stopSniffer();
        s_subScene = SUB_TX_RUN;
        startAdvertising();
      }
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

void draw(U8G2& u8g2, uint32_t now) {
  if (s_subScene == SUB_MAIN) {
    drawHeader(u8g2, "iBEACON");
    u8g2.setFont(u8g2_font_6x12_tr);
    for (int i = 0; i < kMainMenuCount; i++) {
      int y = kStartY + (int)(i * kRowH - s_mainScroll + 0.5f);
      if (y < kStartY - kRowH || y > 64) continue;
      u8g2.drawStr(6, y + 10, kMainMenuItems[i]);
    }

    u8g2.setDrawColor(2);
    int curY = (int)(s_mainBarY + 0.5f);
    if (curY < kStartY) curY = kStartY;
    if (curY > 64 - kRowH) curY = 64 - kRowH;
    u8g2.drawBox(2, curY, 124, kRowH);
    u8g2.setDrawColor(1);
  } else if (s_subScene == SUB_PRESETS) {
    drawHeader(u8g2, "BEACON PRESETS");
    u8g2.setFont(u8g2_font_6x12_tr);
    for (int i = 0; i < kPresetCount; i++) {
      int y = kStartY + (int)(i * kRowH - s_presetScroll + 0.5f);
      if (y < kStartY - kRowH || y > 64) continue;
      u8g2.drawStr(6, y + 10, kPresets[i].name);
    }

    u8g2.setDrawColor(2);
    int curY = (int)(s_presetBarY + 0.5f);
    if (curY < kStartY) curY = kStartY;
    if (curY > 64 - kRowH) curY = 64 - kRowH;
    u8g2.drawBox(2, curY, 124, kRowH);
    u8g2.setDrawColor(1);
  } else if (s_subScene == SUB_TX_RUN) {
    drawHeader(u8g2, "iBEACON TX");


    u8g2.setFont(u8g2_font_6x12_tr);
    u8g2.drawStr(6, 26, s_activeName);

    char idBuf[32];
    snprintf(idBuf, sizeof(idBuf), "Maj: %u  Min: %u", s_major, s_minor);
    u8g2.setFont(u8g2_font_5x8_tr);
    u8g2.drawStr(6, 36, idBuf);

    char pwrBuf[32];
    snprintf(pwrBuf, sizeof(pwrBuf), "Power: %d dBm", s_measuredPower);
    u8g2.setFont(u8g2_font_5x8_tr);
    u8g2.drawStr(6, 48, pwrBuf);

    if (s_isBroadcasting) {
      char pBuf[32];
      snprintf(pBuf, sizeof(pBuf), "Packets: %lu", (unsigned long)s_txPacketCount);
      u8g2.drawStr(6, 59, pBuf);
    } else {
      u8g2.drawStr(6, 59, "Paused");
    }
  } else if (s_subScene == SUB_TX_SETUP) {
    drawHeader(u8g2, "BEACON CONFIG");
    u8g2.setFont(u8g2_font_6x12_tr);

    char buf[32];
    snprintf(buf, sizeof(buf), "Major: %u", s_major);
    u8g2.drawStr(6, 28, buf);

    snprintf(buf, sizeof(buf), "Minor: %u", s_minor);
    u8g2.drawStr(6, 42, buf);

    snprintf(buf, sizeof(buf), "TxPower: %d dBm", s_measuredPower);
    u8g2.drawStr(6, 56, buf);


    u8g2.setDrawColor(2);
    int ySel = 18 + s_configParamSel * 14;
    u8g2.drawBox(2, ySel, 124, 13);
    u8g2.setDrawColor(1);
  } else if (s_subScene == SUB_SNIFFER) {
    drawHeader(u8g2, "BEACON SNIFFER");

    if (s_sniffList.empty()) {
      u8g2.setFont(u8g2_font_6x12_tr);
      u8g2.drawStr(10, 40, "Scanning beacons...");
    } else {
      u8g2.setFont(u8g2_font_5x8_tr);
      for (size_t i = 0; i < s_sniffList.size(); i++) {
        int y = kStartY + (int)(i * kRowH - s_sniffScroll + 0.5f);
        if (y < kStartY - kRowH || y > 64) continue;

        char line[36];
        const auto& b = s_sniffList[i];
        snprintf(line, sizeof(line), "M:%u m:%u %.1fm", b.major, b.minor, b.distance);
        u8g2.drawStr(6, y + 9, line);

        char rssiBuf[12];
        snprintf(rssiBuf, sizeof(rssiBuf), "%d", b.rssi);
        u8g2.drawStr(104, y + 9, rssiBuf);
      }


      u8g2.setDrawColor(2);
      int curY = (int)(s_sniffBarY + 0.5f);
      if (curY < kStartY) curY = kStartY;
      if (curY > 64 - kRowH) curY = 64 - kRowH;
      u8g2.drawBox(2, curY, 124, kRowH);
      u8g2.setDrawColor(1);
    }
  } else if (s_subScene == SUB_SNIFFER_DETAIL) {
    drawHeader(u8g2, "BEACON DETAILS");
    if (s_sniffSel < (int)s_sniffList.size()) {
      const auto& b = s_sniffList[s_sniffSel];

      char uuidStr[38];
      formatUuid(b.uuid, uuidStr, sizeof(uuidStr));

      u8g2.setFont(u8g2_font_5x8_tr);
      char part1[20], part2[20];
      strncpy(part1, uuidStr, 18); part1[18] = '\0';
      strncpy(part2, uuidStr + 18, 18); part2[18] = '\0';
      u8g2.drawStr(4, 24, part1);
      u8g2.drawStr(4, 33, part2);

      char info[36];
      snprintf(info, sizeof(info), "M:%u  m:%u  Pwr:%d", b.major, b.minor, b.txPower);
      u8g2.drawStr(4, 43, info);

      snprintf(info, sizeof(info), "RSSI: %d  Dist: ~%.2fm", b.rssi, b.distance);
      u8g2.drawStr(4, 52, info);


      u8g2.drawStr(4, 62, "Clone & Broadcast");
    }
  }
}

}
