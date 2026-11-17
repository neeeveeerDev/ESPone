#include "usb_remote.h"
#include "buttons.h"
#include "display.h"
#include "modules/apps/file_manager/fm_fs.h"
#include "modules/gpio/gpio_pins.h"
#include "config.h"
#include <Wire.h>
#include <USB.h>

namespace usb_remote {

static bool s_streamEnabled = false;
static uint32_t s_lastFrameTime = 0;

static File s_uploadFile;
static bool s_isUploading = false;
static uint32_t s_lastUploadTime = 0;


enum ParseState {
  WAIT_SYNC1 = 0,
  WAIT_SYNC2,
  WAIT_CMD,
  WAIT_LEN_LO,
  WAIT_LEN_HI,
  WAIT_PAYLOAD,
  WAIT_CHKSUM
};

struct StreamParser {
  ParseState state = WAIT_SYNC1;
  uint8_t cmd = 0;
  uint16_t len = 0;
  uint16_t pos = 0;
  uint8_t chk = 0;
  uint8_t payload[1024];
};

static StreamParser s_parserSerial;
static StreamParser s_parserSerial0;

static Stream* s_port = &Serial0;

static void sendPacket(uint8_t cmd, const uint8_t* payload, uint16_t len) {
  uint8_t hdr[5];
  hdr[0] = 0x55;
  hdr[1] = 0xAA;
  hdr[2] = cmd;
  hdr[3] = (uint8_t)(len & 0xFF);
  hdr[4] = (uint8_t)((len >> 8) & 0xFF);

  uint8_t chk = cmd ^ hdr[3] ^ hdr[4];
  if (payload && len > 0) {
    for (uint16_t i = 0; i < len; i++) {
      chk ^= payload[i];
    }
  }

  if (s_port) {
    s_port->write(hdr, 5);
    if (payload && len > 0) {
      s_port->write(payload, len);
    }
    s_port->write(&chk, 1);

    if (cmd != 0x10) {
      s_port->flush();
    }
  }
}

static void handlePacket(uint8_t cmd, const uint8_t* payload, uint16_t len) {
  display::setPowerSave(false);
  switch (cmd) {
    case 0x01: {
      uint8_t intOk = fm_fs::isStorageReady(fm_fs::STORAGE_INTERNAL) ? 1 : 0;
      uint8_t sdOk = fm_fs::isStorageReady(fm_fs::STORAGE_SD) ? 1 : 0;
      uint8_t resp[6] = { 'E', 'S', 'P', '1', intOk, sdOk };
      sendPacket(0x81, resp, 6);
      break;
    }

    case 0x02: {
      if (len >= 1) {
        uint8_t k = payload[0];
        switch (k) {
          case 1: buttons::inject(BTN_UP); break;
          case 2: buttons::inject(BTN_DOWN); break;
          case 3: buttons::inject(BTN_LEFT); break;
          case 4: buttons::inject(BTN_RIGHT); break;
          case 5: buttons::inject(BTN_OK); break;
          case 6: buttons::inject(BTN_BACK); break;
          default: break;
        }
      }
      break;
    }

    case 0x03: {
      if (len >= 1) {
        s_streamEnabled = (payload[0] == 1);
      }
      uint8_t ack[1] = { (uint8_t)(s_streamEnabled ? 1 : 0) };
      sendPacket(0x83, ack, 1);
      break;
    }

    case 0x04: {
      if (len < 1) break;
      fm_fs::StorageType st = (payload[0] == 1) ? fm_fs::STORAGE_SD : fm_fs::STORAGE_INTERNAL;
      String path = "/";
      if (len > 1) {
        path = String((const char*)(payload + 1), len - 1);
      }
      if (path.length() == 0 || path[0] != '/') path = "/" + path;

      std::vector<fm_fs::FileItem> items;
      bool ok = fm_fs::listDir(st, path.c_str(), items);
      if (ok) {
        for (const auto& it : items) {
          uint8_t itemBuf[128];
          itemBuf[0] = it.isDir ? 1 : 0;
          uint32_t sz = (uint32_t)it.size;
          memcpy(itemBuf + 1, &sz, 4);
          size_t nl = strlen(it.name);
          if (nl > 100) nl = 100;
          memcpy(itemBuf + 5, it.name, nl);
          sendPacket(0x84, itemBuf, (uint16_t)(5 + nl));
          delay(1);
        }
      }
      uint8_t endBuf[1] = { (uint8_t)(ok ? 0 : 1) };
      sendPacket(0x85, endBuf, 1);
      break;
    }

    case 0x05: {
      if (len < 2) break;
      fm_fs::StorageType st = (payload[0] == 1) ? fm_fs::STORAGE_SD : fm_fs::STORAGE_INTERNAL;
      String path = String((const char*)(payload + 1), len - 1);

      fs::FS* fs = fm_fs::getFS(st);
      if (!fs) {
        uint8_t err[1] = { 1 };
        sendPacket(0x88, err, 1);
        break;
      }

      File f = fs->open(path.c_str(), "r");
      if (!f || f.isDirectory()) {
        uint8_t err[1] = { 1 };
        sendPacket(0x88, err, 1);
        if (f) f.close();
        break;
      }

      uint32_t totalSize = f.size();
      uint8_t szBuf[4];
      memcpy(szBuf, &totalSize, 4);
      sendPacket(0x86, szBuf, 4);

      uint8_t chunk[512];
      while (f.available()) {
        size_t r = f.read(chunk, sizeof(chunk));
        if (r == 0) break;
        sendPacket(0x87, chunk, (uint16_t)r);
        delay(1);
      }
      f.close();

      uint8_t okEnd[1] = { 0 };
      sendPacket(0x88, okEnd, 1);
      break;
    }

    case 0x06: {
      if (len < 6) break;
      fm_fs::StorageType st = (payload[0] == 1) ? fm_fs::STORAGE_SD : fm_fs::STORAGE_INTERNAL;
      const char* rawPath = (const char*)(payload + 5);
      size_t maxPathLen = len - 5;
      size_t actualLen = strnlen(rawPath, maxPathLen);
      String path = String(rawPath, actualLen);
      path.trim();
      while (path.length() > 1 && path.endsWith("/")) {
        path.remove(path.length() - 1);
      }
      if (!path.startsWith("/")) {
        path = "/" + path;
      }

      if (s_isUploading && s_uploadFile) {
        s_uploadFile.flush();
        s_uploadFile.close();
      }

      fs::FS* fs = fm_fs::getFS(st);
      if (!fs) {
        uint8_t err[1] = { 1 };
        sendPacket(0x89, err, 1);
        break;
      }

      int lastSlash = path.lastIndexOf('/');
      if (lastSlash > 0) {
        String dir = path.substring(0, lastSlash);
        fm_fs::createDir(st, dir.c_str());
      }

      s_uploadFile = fs->open(path.c_str(), "w");
      if (!s_uploadFile) {
        uint8_t err[1] = { 1 };
        sendPacket(0x89, err, 1);
        break;
      }

      s_isUploading = true;
      s_lastUploadTime = millis();
      uint8_t okAck[1] = { 0 };
      sendPacket(0x89, okAck, 1);
      break;
    }

    case 0x07: {
      if (s_isUploading && s_uploadFile && len > 0) {
        size_t written = s_uploadFile.write(payload, len);
        s_uploadFile.flush();
        s_lastUploadTime = millis();

        uint8_t ack[1] = { (uint8_t)(written == len ? 0 : 1) };
        sendPacket(0x8E, ack, 1);
      } else {
        uint8_t ack[1] = { 1 };
        sendPacket(0x8E, ack, 1);
      }
      break;
    }

    case 0x08: {
      if (s_isUploading && s_uploadFile) {
        s_uploadFile.flush();
        s_uploadFile.close();
        s_isUploading = false;
        uint8_t okFin[1] = { 0 };
        sendPacket(0x8A, okFin, 1);
      } else {
        uint8_t errFin[1] = { 1 };
        sendPacket(0x8A, errFin, 1);
      }
      break;
    }

    case 0x09: {
      if (len < 3) break;
      fm_fs::StorageType st = (payload[0] == 1) ? fm_fs::STORAGE_SD : fm_fs::STORAGE_INTERNAL;
      bool isDir = (payload[1] == 1);
      const char* rawPath = (const char*)(payload + 2);
      size_t maxPathLen = len - 2;
      size_t actualLen = strnlen(rawPath, maxPathLen);
      String path = String(rawPath, actualLen);
      path.trim();
      if (!path.startsWith("/")) path = "/" + path;
      bool ok = fm_fs::deleteItem(st, path.c_str(), isDir);
      uint8_t resp[1] = { (uint8_t)(ok ? 0 : 1) };
      sendPacket(0x8B, resp, 1);
      break;
    }

    case 0x0A: {
      if (len < 2) break;
      fm_fs::StorageType st = (payload[0] == 1) ? fm_fs::STORAGE_SD : fm_fs::STORAGE_INTERNAL;
      const char* rawPath = (const char*)(payload + 1);
      size_t maxPathLen = len - 1;
      size_t actualLen = strnlen(rawPath, maxPathLen);
      String path = String(rawPath, actualLen);
      path.trim();
      while (path.length() > 1 && path.endsWith("/")) {
        path.remove(path.length() - 1);
      }
      if (!path.startsWith("/")) {
        path = "/" + path;
      }
      bool ok = fm_fs::createDir(st, path.c_str());
      uint8_t resp[1] = { (uint8_t)(ok ? 0 : 1) };
      sendPacket(0x8C, resp, 1);
      break;
    }

    case 0x0B: {
      uint8_t intOk = fm_fs::isStorageReady(fm_fs::STORAGE_INTERNAL) ? 1 : 0;
      uint8_t sdOk = fm_fs::isStorageReady(fm_fs::STORAGE_SD) ? 1 : 0;
      uint8_t resp[2] = { intOk, sdOk };
      sendPacket(0x8D, resp, 2);
      break;
    }

    case 0x0C: {
      uint8_t bus = (len > 0) ? payload[0] : 0;
      TwoWire* wire = (bus == 0) ? &Wire : &Wire1;
      if (bus == 0) {
        wire->begin(PIN_SDA, PIN_SCL);
        wire->setClock(100000);
      } else {
        int8_t pinSDA = gpio_pins::getPin(gpio_pins::PIN_IDX_F);
        int8_t pinSCL = gpio_pins::getPin(gpio_pins::PIN_IDX_G);
        wire->begin(pinSDA, pinSCL);
        wire->setClock(100000);
      }

      uint8_t foundBuf[64];
      uint8_t cnt = 0;
      for (uint8_t addr = 1; addr < 127 && cnt < 60; addr++) {
        wire->beginTransmission(addr);
        if (wire->endTransmission() == 0) {
          foundBuf[1 + cnt] = addr;
          cnt++;
        }
      }

      if (bus == 0) {
        Wire.setClock(I2C_CLOCK);
      }

      foundBuf[0] = cnt;
      sendPacket(0x8F, foundBuf, (uint16_t)(1 + cnt));
      break;
    }
  }
}

void init() {
  s_streamEnabled = false;
  s_parserSerial.state = WAIT_SYNC1;
  s_parserSerial0.state = WAIT_SYNC1;

  fm_fs::initInternal();
  fm_fs::initSD();

#if ARDUINO_USB_MODE == 0
  USB.manufacturerName("Neeeveeer Devices");
  USB.productName("ESPoneV1");
  USB.begin();
#endif

  Serial.setRxBufferSize(8192);
  Serial0.setRxBufferSize(8192);
  Serial.begin(921600);
  Serial0.begin(921600);
}

static void processStream(Stream& stream, StreamParser& p) {
  while (stream.available()) {
    if (p.state == WAIT_SYNC1 && stream.peek() != 0x55) {
      break;
    }
    uint8_t b = (uint8_t)stream.read();

    switch (p.state) {
      case WAIT_SYNC1:
        if (b == 0x55) p.state = WAIT_SYNC2;
        break;

      case WAIT_SYNC2:
        if (b == 0xAA) p.state = WAIT_CMD;
        else if (b != 0x55) p.state = WAIT_SYNC1;
        break;

      case WAIT_CMD:
        p.cmd = b;
        p.chk = b;
        p.state = WAIT_LEN_LO;
        break;

      case WAIT_LEN_LO:
        p.len = b;
        p.chk ^= b;
        p.state = WAIT_LEN_HI;
        break;

      case WAIT_LEN_HI:
        p.len |= ((uint16_t)b << 8);
        p.chk ^= b;
        p.pos = 0;
        if (p.len > sizeof(p.payload)) {
          p.state = WAIT_SYNC1;
        } else if (p.len == 0) {
          p.state = WAIT_CHKSUM;
        } else {
          p.state = WAIT_PAYLOAD;
        }
        break;

      case WAIT_PAYLOAD:
        p.payload[p.pos++] = b;
        p.chk ^= b;
        if (p.pos >= p.len) {
          p.state = WAIT_CHKSUM;
        }
        break;

      case WAIT_CHKSUM:
        if (b == p.chk) {
          s_port = &stream;
          handlePacket(p.cmd, p.payload, p.len);
          p.state = WAIT_SYNC1;
        } else {
          p.state = (b == 0x55) ? WAIT_SYNC2 : WAIT_SYNC1;
        }
        break;
    }
  }
}

static bool s_enabled = true;

void setEnabled(bool enabled) {
  s_enabled = enabled;
  if (!s_enabled) {
    s_streamEnabled = false;
  }
}

bool isEnabled() {
  return s_enabled;
}

void tick() {
  if (!s_enabled) return;


  if (s_isUploading && millis() - s_lastUploadTime > 8000) {
    if (s_uploadFile) {
      s_uploadFile.flush();
      s_uploadFile.close();
    }
    s_isUploading = false;
    Serial.println("[USB Remote] Upload watchdog timed out.");
  }

  processStream(Serial, s_parserSerial);
  processStream(Serial0, s_parserSerial0);
}

bool isStreaming() {
  return s_streamEnabled && s_enabled && !s_isUploading;
}

void sendScreenFrame(const uint8_t* buffer, size_t length) {
  if (!s_enabled || !s_streamEnabled || s_isUploading || !buffer || length < 1024) return;

  uint32_t now = millis();
  if (now - s_lastFrameTime < 16) return;
  s_lastFrameTime = now;

  sendPacket(0x10, buffer, 1024);
}

}
