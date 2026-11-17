#include "scene_ble_scan.h"
#include "core/ble_service.h"
#include <NimBLEDevice.h>
#include <NimBLEScan.h>
#include <NimBLEAdvertisedDevice.h>
#include <vector>
#include <math.h>

namespace scene_ble_scan {

enum ScanSubScene : uint8_t {
  SUB_LIST = 0,
  SUB_DETAILS
};

struct BleDeviceItem {
  char name[28];
  char mac[18];
  char vendor[14];
  int rssi;
  uint32_t lastSeen;
};

static const int kMaxDevices = 60;
static const int kRowH = 12;
static const int kStartY = 16;

static ScanSubScene s_subScene = SUB_LIST;
static std::vector<BleDeviceItem> s_devices;
static SemaphoreHandle_t s_mutex = nullptr;
static bool s_scanningActive = false;

static int s_sel = 0;
static float s_barY = (float)kStartY;
static float s_scroll = 0.0f;
static float s_targetScroll = 0.0f;
static int s_selectedDeviceIdx = -1;

static const char* identifyVendor(const std::string& mfgData) {
  if (mfgData.length() < 2) return "Unknown";
  uint16_t id = (uint8_t)mfgData[0] | (((uint8_t)mfgData[1]) << 8);
  switch (id) {
    case 0x004C: return "Apple";
    case 0x0075: return "Samsung";
    case 0x0006: return "Microsoft";
    case 0x00E0: return "Google";
    case 0x038F: return "Xiaomi";
    case 0x027D: return "Huawei";
    case 0x012D: return "Sony";
    case 0x02E5: return "Espressif";
    case 0x0059: return "Nordic";
    case 0x000D: return "Texas Inst";
    case 0x000A: return "CSR";
    default:     return "Other";
  }
}

class MyScanCallbacks : public NimBLEAdvertisedDeviceCallbacks {
  void onResult(NimBLEAdvertisedDevice* advertisedDevice) override {
    if (!s_scanningActive || !s_mutex) return;

    if (!advertisedDevice) return;
    std::string macStr = advertisedDevice->getAddress().toString();
    std::string nameStr = advertisedDevice->getName();
    int rssiVal = advertisedDevice->getRSSI();
    std::string mfgData = advertisedDevice->haveManufacturerData() ? advertisedDevice->getManufacturerData() : "";
    const char* vendorStr = identifyVendor(mfgData);

    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(25)) == pdTRUE) {
      bool found = false;
      for (size_t i = 0; i < s_devices.size(); i++) {
        if (strcmp(s_devices[i].mac, macStr.c_str()) == 0) {
          found = true;
          s_devices[i].rssi = rssiVal;
          s_devices[i].lastSeen = millis();
          if (nameStr.length() > 0 && strlen(s_devices[i].name) == 0) {
            strncpy(s_devices[i].name, nameStr.c_str(), sizeof(s_devices[i].name) - 1);
            s_devices[i].name[sizeof(s_devices[i].name) - 1] = '\0';
          }
          if (strcmp(s_devices[i].vendor, "Unknown") == 0 && strcmp(vendorStr, "Unknown") != 0) {
            strncpy(s_devices[i].vendor, vendorStr, sizeof(s_devices[i].vendor) - 1);
            s_devices[i].vendor[sizeof(s_devices[i].vendor) - 1] = '\0';
          }
          break;
        }
      }

      if (!found && (int)s_devices.size() < kMaxDevices) {
        BleDeviceItem item;
        memset(&item, 0, sizeof(item));
        strncpy(item.mac, macStr.c_str(), sizeof(item.mac) - 1);
        strncpy(item.name, nameStr.c_str(), sizeof(item.name) - 1);
        strncpy(item.vendor, vendorStr, sizeof(item.vendor) - 1);
        item.rssi = rssiVal;
        item.lastSeen = millis();
        s_devices.push_back(item);
      }
      xSemaphoreGive(s_mutex);
    }
  }
};

static MyScanCallbacks* s_scanCallbacks = nullptr;

static void scanCompleteCB(NimBLEScanResults results) {
  (void)results;
  if (s_scanningActive) {
    NimBLEScan* pScan = NimBLEDevice::getScan();
    if (pScan) {
      pScan->start(4, scanCompleteCB, false);
    }
  }
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

static void drawHeader(U8G2& u8g2, const char* title, int count = -1) {
  u8g2.setDrawColor(1);
  u8g2.drawBox(0, 0, 128, 14);
  u8g2.setDrawColor(0);
  u8g2.setFont(u8g2_font_7x14B_tr);
  u8g2.drawStr(4, 11, title);

  if (count >= 0) {
    char cntBuf[12];
    snprintf(cntBuf, sizeof(cntBuf), "%d", count);
    int cw = u8g2.getStrWidth(cntBuf);
    u8g2.drawStr(124 - cw, 11, cntBuf);
  }
  u8g2.setDrawColor(1);
}

void open() {
  if (!s_mutex) {
    s_mutex = xSemaphoreCreateMutex();
  }

  s_subScene = SUB_LIST;
  s_sel = 0;
  s_barY = (float)kStartY;
  s_scroll = 0.0f;
  s_targetScroll = 0.0f;
  s_selectedDeviceIdx = -1;

  if (s_mutex && xSemaphoreTake(s_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
    s_devices.clear();
    xSemaphoreGive(s_mutex);
  }

  s_scanningActive = true;
  ble_service::setSuspended(true);

  if (!s_scanCallbacks) {
    s_scanCallbacks = new MyScanCallbacks();
  }

  NimBLEScan* pScan = NimBLEDevice::getScan();
  if (pScan) {
    pScan->setAdvertisedDeviceCallbacks(s_scanCallbacks, true);
    pScan->setActiveScan(true);
    pScan->setInterval(160);
    pScan->setWindow(50);
    pScan->start(4, scanCompleteCB, false);
  }
}

void close() {
  s_scanningActive = false;
  NimBLEScan* pScan = NimBLEDevice::getScan();
  if (pScan) {
    pScan->stop();
    pScan->clearResults();
  }

  ble_service::setSuspended(false);

  if (s_mutex && xSemaphoreTake(s_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
    s_devices.clear();
    xSemaphoreGive(s_mutex);
  }
  s_subScene = SUB_LIST;
}

bool onBack() {
  if (s_subScene == SUB_DETAILS) {
    s_subScene = SUB_LIST;
    return true;
  }

  return false;
}

void tick(uint32_t now) {
  (void)now;
  if (s_subScene == SUB_LIST) {
    int devCount = 0;
    if (s_mutex && xSemaphoreTake(s_mutex, pdMS_TO_TICKS(15)) == pdTRUE) {
      devCount = (int)s_devices.size();
      xSemaphoreGive(s_mutex);
    }
    if (devCount > 0) {
      if (s_sel >= devCount) s_sel = devCount - 1;
      updateScroll(s_sel, devCount, s_scroll, s_targetScroll, s_barY);
    }
  }
}

void onKey(const KeyEvent& ev) {
  if (s_subScene == SUB_LIST) {
    int devCount = 0;
    if (s_mutex && xSemaphoreTake(s_mutex, pdMS_TO_TICKS(20)) == pdTRUE) {
      devCount = (int)s_devices.size();
      xSemaphoreGive(s_mutex);
    }

    if (devCount > 0) {
      if (ev.key == BTN_UP) {
        s_sel = (s_sel - 1 + devCount) % devCount;
      } else if (ev.key == BTN_DOWN) {
        s_sel = (s_sel + 1) % devCount;
      } else if (ev.key == BTN_OK && !ev.repeat) {
        s_selectedDeviceIdx = s_sel;
        s_subScene = SUB_DETAILS;
      }
    }
  } else if (s_subScene == SUB_DETAILS) {

    if (ev.key == BTN_OK && !ev.repeat) {
      s_subScene = SUB_LIST;
    }
  }
}

static void drawDetails(U8G2& u8g2) {
  drawHeader(u8g2, "DEVICE INFO");

  BleDeviceItem curItem;
  memset(&curItem, 0, sizeof(curItem));
  bool valid = false;

  if (s_mutex && xSemaphoreTake(s_mutex, pdMS_TO_TICKS(20)) == pdTRUE) {
    if (s_selectedDeviceIdx >= 0 && s_selectedDeviceIdx < (int)s_devices.size()) {
      curItem = s_devices[s_selectedDeviceIdx];
      valid = true;
    }
    xSemaphoreGive(s_mutex);
  }

  if (!valid) {
    u8g2.setFont(u8g2_font_6x12_tr);
    u8g2.drawStr(10, 36, "Device lost");
    return;
  }


  u8g2.setFont(u8g2_font_6x12_tr);
  const char* rawName = (strlen(curItem.name) > 0) ? curItem.name : "Unnamed";
  char nameBuf[32];
  strncpy(nameBuf, rawName, sizeof(nameBuf) - 1);
  nameBuf[sizeof(nameBuf) - 1] = '\0';
  while (strlen(nameBuf) > 0 && u8g2.getStrWidth(nameBuf) > 120) {
    nameBuf[strlen(nameBuf) - 1] = '\0';
  }
  u8g2.drawStr(4, 25, nameBuf);


  u8g2.setFont(u8g2_font_5x8_tr);
  char macBuf[28];
  snprintf(macBuf, sizeof(macBuf), "MAC: %s", curItem.mac);
  u8g2.drawStr(4, 35, macBuf);


  char vendorBuf[20];
  snprintf(vendorBuf, sizeof(vendorBuf), "%s", curItem.vendor);
  u8g2.drawStr(4, 45, vendorBuf);

  char rssiBuf[16];
  snprintf(rssiBuf, sizeof(rssiBuf), "%d dBm", curItem.rssi);
  int rw = u8g2.getStrWidth(rssiBuf);
  u8g2.drawStr(124 - rw, 45, rssiBuf);


  u8g2.drawFrame(4, 50, 120, 10);
  int clampedRssi = curItem.rssi;
  if (clampedRssi < -95) clampedRssi = -95;
  if (clampedRssi > -35) clampedRssi = -35;
  int barW = (clampedRssi - (-95)) * 114 / (-35 - (-95));
  if (barW > 0) {
    u8g2.drawBox(7, 53, barW, 4);
  }
}

void draw(U8G2& u8g2, uint32_t now) {
  if (s_subScene == SUB_DETAILS) {
    drawDetails(u8g2);
    return;
  }


  int devCount = 0;
  if (s_mutex && xSemaphoreTake(s_mutex, pdMS_TO_TICKS(15)) == pdTRUE) {
    devCount = (int)s_devices.size();
    xSemaphoreGive(s_mutex);
  }

  drawHeader(u8g2, "BLE SCAN", devCount);

  if (devCount == 0) {
    u8g2.setFont(u8g2_font_6x12_tr);
    int dots = (now / 350) % 4;
    char waitBuf[20];
    snprintf(waitBuf, sizeof(waitBuf), "Scanning%.*s", dots, "...");
    int tw = u8g2.getStrWidth(waitBuf);
    u8g2.drawStr((128 - tw) / 2, 40, waitBuf);
    return;
  }


  u8g2.setClipWindow(0, 14, 128, 64);

  if (s_mutex && xSemaphoreTake(s_mutex, pdMS_TO_TICKS(15)) == pdTRUE) {
    for (int i = 0; i < devCount; i++) {
      int y = kStartY + (int)(i * kRowH - s_scroll + 0.5f);
      if (y < kStartY - kRowH || y > 64) continue;

      const BleDeviceItem& dev = s_devices[i];
      char labelBuf[32];
      if (strlen(dev.name) > 0) {
        strncpy(labelBuf, dev.name, sizeof(labelBuf) - 1);
        labelBuf[sizeof(labelBuf) - 1] = '\0';
      } else if (strcmp(dev.vendor, "Unknown") != 0) {
        snprintf(labelBuf, sizeof(labelBuf), "[%s]", dev.vendor);
      } else {
        strncpy(labelBuf, "[BLE Device]", sizeof(labelBuf) - 1);
        labelBuf[sizeof(labelBuf) - 1] = '\0';
      }


      char rssiBuf[8];
      snprintf(rssiBuf, sizeof(rssiBuf), "%d", dev.rssi);
      u8g2.setFont(u8g2_font_5x8_tr);
      int rw = u8g2.getStrWidth(rssiBuf);
      u8g2.drawStr(124 - rw, y + 9, rssiBuf);


      u8g2.setFont(u8g2_font_6x12_tr);
      int maxW = 124 - rw - 8;
      while (strlen(labelBuf) > 0 && u8g2.getStrWidth(labelBuf) > maxW) {
        labelBuf[strlen(labelBuf) - 1] = '\0';
      }
      u8g2.drawStr(6, y + 10, labelBuf);
    }
    xSemaphoreGive(s_mutex);
  }


  u8g2.setDrawColor(2);
  int curY = (int)(s_barY + 0.5f);
  if (curY < kStartY) curY = kStartY;
  if (curY > 64 - kRowH) curY = 64 - kRowH;
  u8g2.drawBox(2, curY, 124, kRowH);
  u8g2.setDrawColor(1);

  u8g2.setMaxClipWindow();
}

}
