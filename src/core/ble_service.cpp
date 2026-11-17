#include "ble_service.h"
#include "buttons.h"
#include "sys_clock.h"
#include "ble_file_handler.h"
#include "modules/settings/settings_data.h"
#include "modules/bluetooth/ble_hid.h"
#include <NimBLEDevice.h>
#include <NimBLEServer.h>
#include <NimBLEUtils.h>
#include <Preferences.h>
#include <esp_bt.h>

extern "C" bool btInUse() {
  return true;
}

#define SERVICE_UUID "E5010001-0000-1000-8000-00805F9B34FB"
#define CHAR_AUTH_UUID "E5010002-0000-1000-8000-00805F9B34FB"
#define CHAR_CONTROL_UUID "E5010003-0000-1000-8000-00805F9B34FB"
#define CHAR_SCREEN_UUID "E5010004-0000-1000-8000-00805F9B34FB"
#define CHAR_FILE_UUID "E5010005-0000-1000-8000-00805F9B34FB"

namespace {

static bool s_connected = false;
static bool s_authenticated = false;
static bool s_paired = false;
static bool s_hasNewClient = false;
static bool s_hidMode = false;
static bool s_bleEnabled = true;
static String s_pin = "000000";
static char s_deviceName[32] = "ESPone";

static NimBLEServer *pServer = nullptr;
static NimBLECharacteristic *pAuthChar = nullptr;
static NimBLECharacteristic *pControlChar = nullptr;
static NimBLECharacteristic *pScreenChar = nullptr;
static NimBLECharacteristic *pFileChar = nullptr;

class ServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer *pServer, ble_gap_conn_desc *desc) override {
    s_connected = true;
    s_hasNewClient = true;
    if (desc) {
      Serial.printf("[BLE] Client connected! MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
                    desc->peer_ota_addr.val[5], desc->peer_ota_addr.val[4],
                    desc->peer_ota_addr.val[3], desc->peer_ota_addr.val[2],
                    desc->peer_ota_addr.val[1], desc->peer_ota_addr.val[0]);
    } else {
      Serial.println("[BLE] Client connected!");
    }
    if (s_paired) {
      s_authenticated = true;
      Serial.println("[BLE Remote] Auto-authenticated (already paired).");
    } else {
      s_authenticated = false;
      Serial.println("[BLE Remote] Waiting for PIN / HID bonding...");
    }
  }

  void onDisconnect(NimBLEServer *pServer) override {
    s_connected = false;
    s_authenticated = false;
    s_hasNewClient = false;
    Serial.println("[BLE Remote] Client disconnected. Restarting advertising...");
    ble_service::resumeAdvertising();
  }
};

class AuthCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic *pChar) override {
    std::string val = pChar->getValue();
    Serial.printf("[BLE Remote] Auth write received (%d bytes): %s\n",
                  (int)val.length(), val.c_str());

    if (val.find(s_pin.c_str()) != std::string::npos) {
      s_authenticated = true;
      s_paired = true;
      Preferences prefs;
      prefs.begin("espone", false);
      prefs.putBool("paired", true);
      prefs.end();
      Serial.println("[BLE Remote] Auth & Pairing SUCCESS! Saved to NVS.");
      uint8_t okResp[] = {0x01, 'O', 'K'};
      pChar->setValue(okResp, sizeof(okResp));
      pChar->notify();
    } else {
      Serial.println("[BLE Remote] Auth FAILED: invalid PIN");
      uint8_t errResp[] = {0x00, 'E', 'R', 'R'};
      pChar->setValue(errResp, sizeof(errResp));
      pChar->notify();
    }
  }
};

class ControlCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic *pChar) override {
    if (!s_authenticated) {
      Serial.println("[BLE Remote] Control rejected: not authenticated");
      return;
    }
    std::string val = pChar->getValue();
    if (val.empty())
      return;

    uint8_t cmd = (uint8_t)val[0];
    if (cmd == 0x10) {

      if (val.length() >= 6) {
        if (settings_data::getBleSync()) {
          uint32_t epoch = 0;
          memcpy(&epoch, val.data() + 1, 4);
          int8_t tz = (int8_t)val[5];
          sys_clock::syncFromBle(epoch, tz);
        } else {
          Serial.println("[BLE Remote] Time sync ignored (BLE Sync is OFF in Settings)");
        }
      }
      return;
    }

    switch (cmd) {
    case 1:
      buttons::inject(BTN_UP);
      break;
    case 2:
      buttons::inject(BTN_DOWN);
      break;
    case 3:
      buttons::inject(BTN_LEFT);
      break;
    case 4:
      buttons::inject(BTN_RIGHT);
      break;
    case 5:
      buttons::inject(BTN_OK);
      break;
    case 6:
      buttons::inject(BTN_BACK);
      break;
    default:
      break;
    }
  }
};

class CtsCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic *pChar) override {
    if (!settings_data::getBleSync()) {
      Serial.println("[CTS] Sync ignored (BLE Sync is OFF in Settings)");
      return;
    }
    std::string val = pChar->getValue();
    if (val.length() >= 7) {
      uint16_t yr = (uint8_t)val[0] | ((uint8_t)val[1] << 8);
      uint8_t mo = (uint8_t)val[2];
      uint8_t dy = (uint8_t)val[3];
      uint8_t hr = (uint8_t)val[4];
      uint8_t mn = (uint8_t)val[5];
      uint8_t sc = (uint8_t)val[6];
      sys_clock::setDate(yr, mo, dy);
      sys_clock::setTime(hr, mn, sc);
      Serial.printf("[CTS] Auto-synced from standard BLE CTS: %04d-%02d-%02d %02d:%02d:%02d\n",
                    yr, mo, dy, hr, mn, sc);
    }
  }
};

class FileCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic *pChar) override {
    if (!s_authenticated) {
      Serial.println("[BLE File] Access rejected: not authenticated");
      return;
    }
    std::string val = pChar->getValue();
    if (val.empty()) return;
    ble_file_handler::onPacketReceived(pChar, (const uint8_t*)val.data(), val.length());
  }
};

}

namespace ble_service {

void init() {
  Preferences prefs;
  prefs.begin("espone", false);
  s_paired = prefs.getBool("paired", false);
  String storedPin = prefs.getString("pin", "");

  uint64_t mac = ESP.getEfuseMac();
  uint16_t macSuffix = (uint16_t)(mac & 0xFFFF);

  Preferences setPrefs;
  setPrefs.begin("espone_set", true);
  String customName = setPrefs.getString("ble_name", "");
  s_bleEnabled = setPrefs.getBool("ble", true);
  setPrefs.end();

  if (customName.length() > 0) {
    snprintf(s_deviceName, sizeof(s_deviceName), "%s", customName.c_str());
  } else {
    snprintf(s_deviceName, sizeof(s_deviceName), "ESPone-%04X", macSuffix);
  }

  if (storedPin.length() != 6) {
    uint32_t code =
        (uint32_t)(((mac ^ (mac >> 16) ^ 0x5A3C9E21) % 900000) + 100000);
    char pinBuf[8];
    snprintf(pinBuf, sizeof(pinBuf), "%06u", code);
    s_pin = String(pinBuf);
    prefs.putString("pin", s_pin);
    Serial.printf("[BLE Remote] Generated permanent PIN: %s\n", s_pin.c_str());
  } else {
    s_pin = storedPin;
    Serial.printf("[BLE Remote] Loaded permanent PIN from NVS: %s\n", s_pin.c_str());
  }
  prefs.end();

  Serial.println("[DBG] Step 1: Checking BLE enabled...");
  if (!s_bleEnabled) {
    Serial.println("[BLE Remote] Bluetooth is disabled in settings");
    return;
  }

  Serial.printf("[DBG] Step 2: NimBLEDevice::init(%s)...\n", s_deviceName);
  Serial.flush();
  NimBLEDevice::init(s_deviceName);
  Serial.println("[DBG] Step 3: NimBLEDevice::setMTU...");
  Serial.flush();
  NimBLEDevice::setMTU(517);

  Serial.println("[DBG] Step 4: createServer...");
  Serial.flush();
  pServer = NimBLEDevice::createServer();
  static ServerCallbacks s_srvCb;
  pServer->setCallbacks(&s_srvCb);

  Serial.println("[DBG] Step 5: createService...");
  Serial.flush();
  NimBLEService *pService = pServer->createService(SERVICE_UUID);

  pAuthChar = pService->createCharacteristic(
      CHAR_AUTH_UUID, NIMBLE_PROPERTY::READ |
                          NIMBLE_PROPERTY::WRITE |
                          NIMBLE_PROPERTY::NOTIFY);
  static AuthCallbacks s_authCb;
  pAuthChar->setCallbacks(&s_authCb);

  pControlChar = pService->createCharacteristic(
      CHAR_CONTROL_UUID,
      NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
  static ControlCallbacks s_ctrlCb;
  pControlChar->setCallbacks(&s_ctrlCb);

  pScreenChar = pService->createCharacteristic(
      CHAR_SCREEN_UUID, NIMBLE_PROPERTY::NOTIFY);

  pFileChar = pService->createCharacteristic(
      CHAR_FILE_UUID,
      NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR | NIMBLE_PROPERTY::NOTIFY);
  static FileCallbacks s_fileCb;
  pFileChar->setCallbacks(&s_fileCb);
  ble_file_handler::init(pFileChar);

  pService->start();


  NimBLEService *pCts = pServer->createService(NimBLEUUID((uint16_t)0x1805));
  if (pCts) {
    NimBLECharacteristic *pCtsChar = pCts->createCharacteristic(
        NimBLEUUID((uint16_t)0x2A2B),
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::NOTIFY);
    static CtsCallbacks s_ctsCb;
    pCtsChar->setCallbacks(&s_ctsCb);
    pCts->start();
  }

  ble_hid::init(pServer);

  NimBLEAdvertising *pAdv = NimBLEDevice::getAdvertising();
  pAdv->setScanResponse(true);
  pAdv->setMinPreferred(0x06);
  pAdv->setMaxPreferred(0x12);


  NimBLEAdvertisementData advData;
  advData.setFlags(0x06);
  advData.setName(s_deviceName);
  advData.setAppearance(0x03C1);
  advData.setCompleteServices(NimBLEUUID((uint16_t)0x1812));


  NimBLEAdvertisementData scanResponseData;
  uint8_t mfgData[4] = {0xE5, 0x01, (uint8_t)(macSuffix >> 8),
                        (uint8_t)(macSuffix & 0xFF)};
  std::string mfgStr((char *)mfgData, sizeof(mfgData));
  scanResponseData.setManufacturerData(mfgStr);
  scanResponseData.setCompleteServices(NimBLEUUID(SERVICE_UUID));

  pAdv->setAdvertisementData(advData);
  pAdv->setScanResponseData(scanResponseData);

  NimBLEDevice::startAdvertising();
  Serial.printf("[BLE Remote] Started. Name: %s, PIN: %s, Paired: %d\n", s_deviceName,
                s_pin.c_str(), (int)s_paired);
}

void tick() {
}

bool isConnected() { return s_connected; }
bool isAuthenticated() { return s_authenticated; }
bool isPaired() { return s_paired; }

bool checkAndClearNewClient() {
  bool val = s_hasNewClient;
  s_hasNewClient = false;
  return val;
}

const char *getPin() { return s_pin.c_str(); }
const char *getDeviceName() { return s_deviceName; }

void setDeviceName(const char *name) {
  if (!name || strlen(name) == 0) return;
  snprintf(s_deviceName, sizeof(s_deviceName), "%s", name);
  NimBLEDevice::setDeviceName(std::string(s_deviceName));
  resumeAdvertising();
}

void resetPin() {
  Preferences prefs;
  prefs.begin("espone", false);
  uint32_t code = (uint32_t)random(100000, 999999);
  char pinBuf[8];
  snprintf(pinBuf, sizeof(pinBuf), "%06u", code);
  s_pin = String(pinBuf);
  prefs.putString("pin", s_pin);
  prefs.putBool("paired", false);
  s_paired = false;
  s_authenticated = false;
  prefs.end();
  Serial.printf("[BLE Remote] New PIN generated: %s\n", s_pin.c_str());
}

void setEnabled(bool enabled) {
  if (s_bleEnabled == enabled) return;
  s_bleEnabled = enabled;
  if (!s_bleEnabled) {
    if (s_connected && pServer) {
      pServer->disconnect(0);
    }
    NimBLEDevice::deinit(false);
    Serial.println("[BLE Remote] Radio turned OFF");
  } else {
    NimBLEDevice::init(s_deviceName);
    NimBLEDevice::startAdvertising();
    Serial.println("[BLE Remote] Radio turned ON");
  }
}

bool isEnabled() {
  return s_bleEnabled;
}

void pauseAdvertising() {
  NimBLEAdvertising *pAdv = NimBLEDevice::getAdvertising();
  if (pAdv) {
    pAdv->stop();
  }
}

void resumeAdvertising() {
  if (s_bleEnabled && !s_connected) {
    uint64_t mac = ESP.getEfuseMac();
    uint16_t macSuffix = (uint16_t)(mac & 0xFFFF);
    uint8_t mfgData[4] = {0xE5, 0x01, (uint8_t)(macSuffix >> 8), (uint8_t)(macSuffix & 0xFF)};
    std::string mfgStr((char *)mfgData, sizeof(mfgData));

    NimBLEAdvertising *pAdv = NimBLEDevice::getAdvertising();
    if (pAdv) {
      pAdv->stop();
      pAdv->setScanResponse(true);
      pAdv->setMinPreferred(0x06);
      pAdv->setMaxPreferred(0x12);

      NimBLEAdvertisementData advData;
      advData.setFlags(0x06);
      advData.setName(s_deviceName);
      advData.setAppearance(0x03C1);
      advData.setCompleteServices(NimBLEUUID((uint16_t)0x1812));

      NimBLEAdvertisementData scanResponseData;
      scanResponseData.setManufacturerData(mfgStr);
      scanResponseData.setCompleteServices(NimBLEUUID(SERVICE_UUID));

      pAdv->setAdvertisementData(advData);
      pAdv->setScanResponseData(scanResponseData);
      pAdv->start();
    }
  }
}

void setHidMode(bool active) {
  s_hidMode = active;
}

bool isHidMode() {
  return s_hidMode;
}

static bool s_suspended = false;

void setSuspended(bool suspended) {
  s_suspended = suspended;
}

bool isSuspended() {
  return s_suspended;
}

void* getServer() {
  return (void*)pServer;
}

void sendScreen(const uint8_t *buffer, size_t length) {
  if (s_suspended || !s_connected || !s_authenticated || !pScreenChar)
    return;

  static uint32_t lastSend = 0;
  uint32_t now = millis();
  if (now - lastSend < 40)
    return;
  lastSend = now;

  static uint8_t frameId = 0;
  frameId++;

  const size_t kChunkSize = 256;
  const uint8_t kTotalChunks = (length + kChunkSize - 1) / kChunkSize;

  uint8_t packet[260];
  for (uint8_t idx = 0; idx < kTotalChunks; idx++) {
    size_t offset = idx * kChunkSize;
    size_t chunkLen =
        (length - offset < kChunkSize) ? (length - offset) : kChunkSize;

    packet[0] = frameId;
    packet[1] = idx;
    packet[2] = kTotalChunks;
    memcpy(&packet[3], buffer + offset, chunkLen);

    pScreenChar->setValue(packet, chunkLen + 3);
    pScreenChar->notify();
    delay(3);
  }
}

}
