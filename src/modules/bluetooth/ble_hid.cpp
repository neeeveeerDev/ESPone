#include "ble_hid.h"
#include <NimBLEDevice.h>
#include <NimBLEServer.h>
#include <NimBLEUtils.h>
#include <NimBLEHIDDevice.h>
#include <NimBLESecurity.h>
#include <HIDTypes.h>

namespace ble_hid {

static NimBLEHIDDevice* s_hid = nullptr;
static NimBLECharacteristic* s_inputKeyboard = nullptr;
static NimBLECharacteristic* s_outputKeyboard = nullptr;
static NimBLECharacteristic* s_inputMedia = nullptr;
static NimBLECharacteristic* s_inputMouse = nullptr;

static const uint8_t hidReportMap[] = {
  USAGE_PAGE(1),      0x01,
  USAGE(1),           0x06,
  COLLECTION(1),      0x01,

  REPORT_ID(1),       0x01,
  USAGE_PAGE(1),      0x07,
  USAGE_MINIMUM(1),   0xE0,
  USAGE_MAXIMUM(1),   0xE7,
  LOGICAL_MINIMUM(1), 0x00,
  LOGICAL_MAXIMUM(1), 0x01,
  REPORT_SIZE(1),     0x01,
  REPORT_COUNT(1),    0x08,
  HIDINPUT(1),        0x02,
  REPORT_COUNT(1),    0x01,
  REPORT_SIZE(1),     0x08,
  HIDINPUT(1),        0x01,
  REPORT_COUNT(1),    0x05,
  REPORT_SIZE(1),     0x01,
  USAGE_PAGE(1),      0x08,
  USAGE_MINIMUM(1),   0x01,
  USAGE_MAXIMUM(1),   0x05,
  HIDOUTPUT(1),       0x02,
  REPORT_COUNT(1),    0x01,
  REPORT_SIZE(1),     0x03,
  HIDOUTPUT(1),       0x01,
  REPORT_COUNT(1),    0x06,
  REPORT_SIZE(1),     0x08,
  LOGICAL_MINIMUM(1), 0x00,
  LOGICAL_MAXIMUM(1), 0x65,
  USAGE_PAGE(1),      0x07,
  USAGE_MINIMUM(1),   0x00,
  USAGE_MAXIMUM(1),   0x65,
  HIDINPUT(1),        0x00,
  END_COLLECTION(0),


  USAGE_PAGE(1),      0x0C,
  USAGE(1),           0x01,
  COLLECTION(1),      0x01,
  REPORT_ID(1),       0x02,
  USAGE_PAGE(1),      0x0C,
  LOGICAL_MINIMUM(1), 0x00,
  LOGICAL_MAXIMUM(2), 0xFF, 0x03,
  USAGE_MINIMUM(1),   0x00,
  USAGE_MAXIMUM(2),   0xFF, 0x03,
  REPORT_SIZE(1),     0x10,
  REPORT_COUNT(1),    0x01,
  HIDINPUT(1),        0x00,
  END_COLLECTION(0),


  USAGE_PAGE(1),      0x01,
  USAGE(1),           0x02,
  COLLECTION(1),      0x01,
  USAGE(1),           0x01,
  COLLECTION(1),      0x00,
  REPORT_ID(1),       0x03,
  USAGE_PAGE(1),      0x09,
  USAGE_MINIMUM(1),   0x01,
  USAGE_MAXIMUM(1),   0x03,
  LOGICAL_MINIMUM(1), 0x00,
  LOGICAL_MAXIMUM(1), 0x01,
  REPORT_COUNT(1),    0x03,
  REPORT_SIZE(1),     0x01,
  HIDINPUT(1),        0x02,
  REPORT_COUNT(1),    0x01,
  REPORT_SIZE(1),     0x05,
  HIDINPUT(1),        0x01,
  USAGE_PAGE(1),      0x01,
  USAGE(1),           0x30,
  USAGE(1),           0x31,
  USAGE(1),           0x38,
  LOGICAL_MINIMUM(1), 0x81,
  LOGICAL_MAXIMUM(1), 0x7F,
  REPORT_SIZE(1),     0x08,
  REPORT_COUNT(1),    0x03,
  HIDINPUT(1),        0x06,
  END_COLLECTION(0),
  END_COLLECTION(0)
};

static NimBLEServer* s_server = nullptr;

class OutputCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* pChar) override {

    (void)pChar;
  }
};

void init(NimBLEServer* pServer) {
  if (!pServer) return;
  s_server = pServer;

  s_hid = new NimBLEHIDDevice(pServer);
  s_inputKeyboard  = s_hid->inputReport(1);
  s_outputKeyboard = s_hid->outputReport(1);
  s_inputMedia     = s_hid->inputReport(2);
  s_inputMouse     = s_hid->inputReport(3);

  s_outputKeyboard->setCallbacks(new OutputCallbacks());

  s_hid->manufacturer()->setValue(std::string("ESPone"));

  s_hid->pnp(0x02, 0x05ac, 0x022c, 0x0110);
  s_hid->hidInfo(0x00, 0x01);

  NimBLEDevice::setSecurityAuth(true, true, true);
  NimBLEDevice::setSecurityIOCap(ESP_IO_CAP_NONE);
  NimBLEDevice::setSecurityInitKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);
  NimBLEDevice::setSecurityRespKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);

  class MySecurityCallbacks : public NimBLESecurityCallbacks {
    uint32_t onPassKeyRequest() override {
      Serial.println("[BLE Security] PassKey requested -> returning 0");
      return 0;
    }
    void onPassKeyNotify(uint32_t pass_key) override {
      Serial.printf("[BLE Security] PassKey notify: %06u\n", pass_key);
    }
    bool onConfirmPIN(uint32_t pass_key) override {
      Serial.printf("[BLE Security] Confirm PIN: %06u -> Auto-accepted\n", pass_key);
      return true;
    }
    bool onSecurityRequest() override {
      Serial.println("[BLE Security] Security request received -> Auto-accepting");
      return true;
    }
    void onAuthenticationComplete(ble_gap_conn_desc* desc) override {
      if (desc && desc->sec_state.bonded) {
        Serial.printf("[BLE Security] Bonded successfully! MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
                      desc->peer_ota_addr.val[5], desc->peer_ota_addr.val[4],
                      desc->peer_ota_addr.val[3], desc->peer_ota_addr.val[2],
                      desc->peer_ota_addr.val[1], desc->peer_ota_addr.val[0]);
      } else {
        Serial.println("[BLE Security] Bonding or pairing failed/unbonded");
      }
    }
  };
  static MySecurityCallbacks s_secCallbacks;
  NimBLEDevice::setSecurityCallbacks(&s_secCallbacks);

  s_hid->reportMap((uint8_t*)hidReportMap, sizeof(hidReportMap));
  s_hid->startServices();
  s_hid->setBatteryLevel(100);

  Serial.println("[BLE HID] Service initialized successfully.");
}

bool isConnected() {
  return (s_server && s_server->getConnectedCount() > 0);
}

void sendKey(uint8_t modifier, uint8_t keycode) {
  if (!s_inputKeyboard || !isConnected()) return;
  uint8_t msg[8] = {modifier, 0, keycode, 0, 0, 0, 0, 0};
  s_inputKeyboard->setValue(msg, sizeof(msg));
  s_inputKeyboard->notify();
  delay(12);

  uint8_t rel[8] = {0};
  s_inputKeyboard->setValue(rel, sizeof(rel));
  s_inputKeyboard->notify();
  delay(8);
}

void sendConsumer(uint16_t consumerCode) {
  if (!s_inputMedia || !isConnected()) return;
  s_inputMedia->setValue((uint8_t*)&consumerCode, 2);
  s_inputMedia->notify();
  delay(15);

  uint16_t rel = 0;
  s_inputMedia->setValue((uint8_t*)&rel, 2);
  s_inputMedia->notify();
  delay(8);
}

void sendMouse(uint8_t buttons, int8_t x, int8_t y, int8_t wheel) {
  if (!s_inputMouse || !isConnected()) return;
  uint8_t msg[4] = {buttons, (uint8_t)x, (uint8_t)y, (uint8_t)wheel};
  s_inputMouse->setValue(msg, sizeof(msg));
  s_inputMouse->notify();
}

void mouseMove(int8_t x, int8_t y) {
  sendMouse(0, x, y, 0);
}

void mouseClick(uint8_t buttons) {
  sendMouse(buttons, 0, 0, 0);
  delay(15);
  sendMouse(0, 0, 0, 0);
}

void mouseScroll(int8_t wheel) {
  sendMouse(0, 0, 0, wheel);
}

void sendChar(char c) {
  uint8_t mod = 0;
  uint8_t key = 0;

  if (c >= 'a' && c <= 'z') {
    key = 0x04 + (c - 'a');
  } else if (c >= 'A' && c <= 'Z') {
    key = 0x04 + (c - 'A');
    mod = 0x02;
  } else if (c >= '1' && c <= '9') {
    key = 0x1E + (c - '1');
  } else if (c == '0') {
    key = 0x27;
  } else if (c == ' ') {
    key = KEY_SPACE;
  } else if (c == '\n' || c == '\r') {
    key = KEY_ENTER;
  } else if (c == '\b') {
    key = KEY_BACKSPACE;
  } else if (c == '\t') {
    key = KEY_TAB;
  } else if (c == '.') {
    key = 0x37;
  } else if (c == ',') {
    key = 0x36;
  } else if (c == '-') {
    key = 0x2D;
  } else if (c == '/') {
    key = 0x38;
  } else if (c == '!') {
    key = 0x1E; mod = 0x02;
  } else if (c == '@') {
    key = 0x1F; mod = 0x02;
  } else if (c == '#') {
    key = 0x20; mod = 0x02;
  } else if (c == '$') {
    key = 0x21; mod = 0x02;
  } else if (c == '%') {
    key = 0x22; mod = 0x02;
  } else if (c == '^') {
    key = 0x23; mod = 0x02;
  } else if (c == '&') {
    key = 0x24; mod = 0x02;
  } else if (c == '*') {
    key = 0x25; mod = 0x02;
  } else if (c == '(') {
    key = 0x26; mod = 0x02;
  } else if (c == ')') {
    key = 0x27; mod = 0x02;
  } else if (c == '_') {
    key = 0x2D; mod = 0x02;
  } else if (c == '=') {
    key = 0x2E;
  } else if (c == '+') {
    key = 0x2E; mod = 0x02;
  } else if (c == '[') {
    key = 0x2F;
  } else if (c == '{') {
    key = 0x2F; mod = 0x02;
  } else if (c == ']') {
    key = 0x30;
  } else if (c == '}') {
    key = 0x30; mod = 0x02;
  } else if (c == '\\') {
    key = 0x31;
  } else if (c == '|') {
    key = 0x31; mod = 0x02;
  } else if (c == ';') {
    key = 0x33;
  } else if (c == ':') {
    key = 0x33; mod = 0x02;
  } else if (c == '\'') {
    key = 0x34;
  } else if (c == '"') {
    key = 0x34; mod = 0x02;
  } else if (c == '`') {
    key = 0x35;
  } else if (c == '~') {
    key = 0x35; mod = 0x02;
  } else if (c == '<') {
    key = 0x36; mod = 0x02;
  } else if (c == '>') {
    key = 0x37; mod = 0x02;
  } else if (c == '?') {
    key = 0x38;
    mod = 0x02;
  }

  if (key != 0) {
    sendKey(mod, key);
  }
}

}
