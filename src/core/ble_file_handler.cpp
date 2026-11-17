#include "ble_file_handler.h"
#include "modules/apps/file_manager/fm_fs.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <vector>

namespace ble_file_handler {

struct BleFilePacket {
  uint16_t length;
  uint8_t data[516];
};

static NimBLECharacteristic* s_fileChar = nullptr;
static QueueHandle_t s_cmdQueue = nullptr;
static TaskHandle_t s_taskHandle = nullptr;

static File s_uploadFile;
static bool s_isUploading = false;
static uint32_t s_uploadExpectedBytes = 0;
static uint32_t s_uploadReceivedBytes = 0;
static uint32_t s_lastUploadTime = 0;

static void sendNotify(const uint8_t* data, size_t len) {
  if (!s_fileChar) return;
  s_fileChar->setValue((uint8_t*)data, len);
  s_fileChar->notify();
}

static void sendStatus(uint8_t cmd, uint8_t status) {
  uint8_t resp[3] = { 0x03, cmd, status };
  sendNotify(resp, 3);
}

static void processPacket(const uint8_t* data, size_t length) {
  if (!data || length == 0) return;
  uint8_t cmd = data[0];

  switch (cmd) {
    case 0x01: {
      if (length < 2) return;
      fm_fs::StorageType st = (data[1] == 1) ? fm_fs::STORAGE_SD : fm_fs::STORAGE_INTERNAL;
      String path = "/";
      if (length > 2) {
        path = String((const char*)(data + 2), length - 2);
      }
      if (path.length() == 0 || path[0] != '/') path = "/" + path;


      if (!fm_fs::isStorageReady(st)) {
        if (st == fm_fs::STORAGE_SD) fm_fs::initSD();
        else fm_fs::initInternal();
      }

      std::vector<fm_fs::FileItem> items;
      bool ok = fm_fs::listDir(st, path.c_str(), items);

      if (!ok) {
        uint8_t endResp[2] = { 0x02, 0x01 };
        sendNotify(endResp, 2);
        return;
      }


      for (const auto& it : items) {
        uint8_t itemBuf[128];
        itemBuf[0] = 0x01;
        itemBuf[1] = it.isDir ? 1 : 0;
        uint32_t sz = (uint32_t)it.size;
        memcpy(itemBuf + 2, &sz, 4);
        size_t nameLen = strlen(it.name);
        if (nameLen > 100) nameLen = 100;
        memcpy(itemBuf + 6, it.name, nameLen);
        sendNotify(itemBuf, 6 + nameLen);
        vTaskDelay(pdMS_TO_TICKS(5));
      }

      uint8_t endResp[2] = { 0x02, 0x00 };
      sendNotify(endResp, 2);
      break;
    }

    case 0x03: {
      if (length < 3) return;
      fm_fs::StorageType st = (data[1] == 1) ? fm_fs::STORAGE_SD : fm_fs::STORAGE_INTERNAL;
      const char* rawPath = (const char*)(data + 2);
      size_t maxPathLen = length - 2;
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
      sendStatus(cmd, ok ? 0 : 1);
      break;
    }

    case 0x04: {
      if (length < 3) return;
      fm_fs::StorageType st = (data[1] == 1) ? fm_fs::STORAGE_SD : fm_fs::STORAGE_INTERNAL;
      const char* rawPath = (const char*)(data + 2);
      size_t maxPathLen = length - 2;
      size_t actualLen = strnlen(rawPath, maxPathLen);
      String path = String(rawPath, actualLen);
      path.trim();
      if (!path.startsWith("/")) path = "/" + path;
      bool ok = fm_fs::createFile(st, path.c_str(), "");
      sendStatus(cmd, ok ? 0 : 1);
      break;
    }

    case 0x05: {
      if (length < 4) return;
      fm_fs::StorageType st = (data[1] == 1) ? fm_fs::STORAGE_SD : fm_fs::STORAGE_INTERNAL;
      bool isDir = (data[2] == 1);
      const char* rawPath = (const char*)(data + 3);
      size_t maxPathLen = length - 3;
      size_t actualLen = strnlen(rawPath, maxPathLen);
      String path = String(rawPath, actualLen);
      path.trim();
      if (!path.startsWith("/")) path = "/" + path;
      bool ok = fm_fs::deleteItem(st, path.c_str(), isDir);
      sendStatus(cmd, ok ? 0 : 1);
      break;
    }

    case 0x06: {
      if (length < 3) return;
      fm_fs::StorageType st = (data[1] == 1) ? fm_fs::STORAGE_SD : fm_fs::STORAGE_INTERNAL;
      String path = String((const char*)(data + 2), length - 2);

      fs::FS* fs = fm_fs::getFS(st);
      if (!fs) {
        uint8_t endResp[2] = { 0x08, 0x01 };
        sendNotify(endResp, 2);
        return;
      }

      File f = fs->open(path.c_str(), "r");
      if (!f || f.isDirectory()) {
        uint8_t endResp[2] = { 0x08, 0x01 };
        sendNotify(endResp, 2);
        if (f) f.close();
        return;
      }

      uint32_t totalSize = f.size();
      uint8_t startResp[5];
      startResp[0] = 0x06;
      memcpy(startResp + 1, &totalSize, 4);
      sendNotify(startResp, 5);
      vTaskDelay(pdMS_TO_TICKS(12));


      uint8_t chunkBuf[256];
      chunkBuf[0] = 0x07;
      uint16_t seq = 0;

      while (f.available()) {
        size_t n = f.read(chunkBuf + 3, sizeof(chunkBuf) - 3);
        if (n == 0) break;
        memcpy(chunkBuf + 1, &seq, 2);
        sendNotify(chunkBuf, 3 + n);
        seq++;
        vTaskDelay(pdMS_TO_TICKS(8));
      }
      f.close();

      uint8_t endResp[2] = { 0x08, 0x00 };
      sendNotify(endResp, 2);
      break;
    }

    case 0x07: {
      if (length < 7) return;
      fm_fs::StorageType st = (data[1] == 1) ? fm_fs::STORAGE_SD : fm_fs::STORAGE_INTERNAL;
      memcpy(&s_uploadExpectedBytes, data + 2, 4);
      const char* rawPath = (const char*)(data + 6);
      size_t maxPathLen = length - 6;
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
        sendStatus(cmd, 1);
        return;
      }

      int lastSlash = path.lastIndexOf('/');
      if (lastSlash > 0) {
        String dir = path.substring(0, lastSlash);
        fm_fs::createDir(st, dir.c_str());
      }

      s_uploadFile = fs->open(path.c_str(), "w");
      if (!s_uploadFile) {
        sendStatus(cmd, 1);
        return;
      }

      s_isUploading = true;
      s_uploadReceivedBytes = 0;
      s_lastUploadTime = millis();
      sendStatus(cmd, 0);
      break;
    }

    case 0x08: {
      if (!s_isUploading || !s_uploadFile || length < 3) return;
      size_t chunkLen = length - 3;
      s_uploadFile.write(data + 3, chunkLen);
      s_uploadFile.flush();
      s_uploadReceivedBytes += chunkLen;
      s_lastUploadTime = millis();
      break;
    }

    case 0x09: {
      if (s_isUploading && s_uploadFile) {
        s_uploadFile.flush();
        s_uploadFile.close();
        s_isUploading = false;
        sendStatus(cmd, 0);
      } else {
        sendStatus(cmd, 1);
      }
      break;
    }

    case 0x0A: {
      uint8_t intReady = fm_fs::isStorageReady(fm_fs::STORAGE_INTERNAL) ? 1 : 0;
      uint8_t sdReady = fm_fs::isStorageReady(fm_fs::STORAGE_SD) ? 1 : 0;

      if (!sdReady) {
        fm_fs::initSD();
        sdReady = fm_fs::isStorageReady(fm_fs::STORAGE_SD) ? 1 : 0;
      }
      uint8_t resp[3] = { 0x0A, intReady, sdReady };
      sendNotify(resp, 3);
      break;
    }

    default:
      break;
  }
}

static void fileWorkerTask(void* param) {
  BleFilePacket pkt;
  while (true) {
    if (xQueueReceive(s_cmdQueue, &pkt, pdMS_TO_TICKS(500)) == pdTRUE) {
      processPacket(pkt.data, pkt.length);
    }
    if (s_isUploading && millis() - s_lastUploadTime > 8000) {
      if (s_uploadFile) {
        s_uploadFile.flush();
        s_uploadFile.close();
      }
      s_isUploading = false;
    }
  }
}

void init(NimBLECharacteristic* pChar) {
  s_fileChar = pChar;
  fm_fs::initInternal();
  fm_fs::initSD();

  if (!s_cmdQueue) {
    s_cmdQueue = xQueueCreate(32, sizeof(BleFilePacket));
  }
  if (!s_taskHandle) {
    xTaskCreatePinnedToCore(
      fileWorkerTask,
      "bleFileTask",
      8192,
      nullptr,
      2,
      &s_taskHandle,
      1
    );
  }
}

void onPacketReceived(NimBLECharacteristic* pChar, const uint8_t* data, size_t length) {
  if (!data || length == 0 || length > 516) return;
  s_fileChar = pChar;

  if (s_cmdQueue) {
    BleFilePacket pkt;
    pkt.length = (uint16_t)length;
    memcpy(pkt.data, data, length);

    xQueueSend(s_cmdQueue, &pkt, pdMS_TO_TICKS(50));
  }
}

}
