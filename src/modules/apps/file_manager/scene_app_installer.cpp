#include "scene_app_installer.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include "fm_bitmaps.h"
#include "core/js/app_registry.h"

namespace scene_app_installer {

enum State {
  STATE_READING_HEADER = 0,
  STATE_CONFIRM,
  STATE_UNPACKING,
  STATE_SUCCESS,
  STATE_ERROR
};

#pragma pack(push, 1)
struct AppPackageHeader {
  char magic[8];
  uint16_t version;
  uint16_t flags;
  uint16_t fileCount;
  uint16_t reserved;
  uint32_t manifestOffset;
  uint32_t manifestLength;
  uint32_t headerCrc;
};
#pragma pack(pop)

struct AppFileRecord {
  String relPath;
  uint32_t offset;
  uint32_t size;
  uint32_t crc;
};

static State s_state = STATE_READING_HEADER;
static fm_fs::StorageType s_storage = fm_fs::STORAGE_NONE;
static String s_fullPath = "";
static String s_fileName = "";

static String s_appId = "";
static String s_appName = "";
static String s_appVersion = "";
static String s_appTargetMenu = "";
static String s_appPermissions = "";
static String s_errorMsg = "";

static int s_selectedOption = 0;
static int s_progress = 0;
static std::vector<AppFileRecord> s_files;

void open(fm_fs::StorageType st, const char* fullPath, const char* fileName) {
  s_storage = st;
  s_fullPath = String(fullPath ? fullPath : "");
  s_fileName = String(fileName ? fileName : "");
  s_selectedOption = 0;
  s_progress = 0;
  s_errorMsg = "";
  s_appId = "";
  s_appName = "";
  s_appVersion = "";
  s_appTargetMenu = "";
  s_appPermissions = "";
  s_files.clear();
  s_state = STATE_READING_HEADER;
}

void close() {
  s_files.clear();
}

const char* getInstalledAppId() {
  return s_appId.c_str();
}

InstallerResult onKey(const KeyEvent& ev) {
  if (s_state == STATE_CONFIRM) {
    if (ev.key == BTN_LEFT || ev.key == BTN_RIGHT || ev.key == BTN_UP || ev.key == BTN_DOWN) {
      s_selectedOption = (s_selectedOption == 0) ? 1 : 0;
    } else if (ev.key == BTN_BACK) {
      return INSTALLER_CLOSE;
    } else if (ev.key == BTN_OK && !ev.repeat) {
      if (s_selectedOption == 1) {
        return INSTALLER_CLOSE;
      } else {
        s_state = STATE_UNPACKING;
      }
    }
  } else if (s_state == STATE_SUCCESS) {
    if (ev.key == BTN_OK && !ev.repeat) {
      return INSTALLER_LAUNCH_APP;
    } else if (ev.key == BTN_BACK) {
      return INSTALLER_CLOSE;
    }
  } else if (s_state == STATE_ERROR) {
    if (ev.key == BTN_OK || ev.key == BTN_BACK) {
      return INSTALLER_CLOSE;
    }
  }
  return INSTALLER_RUNNING;
}

static void readHeaderAndManifest() {
  fs::FS* fs = fm_fs::getFS(s_storage);
  if (!fs) {
    s_state = STATE_ERROR;
    s_errorMsg = "Storage unavailable";
    return;
  }

  File f = fs->open(s_fullPath.c_str(), "r");
  if (!f) {
    s_state = STATE_ERROR;
    s_errorMsg = "Cannot open file";
    return;
  }

  AppPackageHeader hdr;
  if (f.read((uint8_t*)&hdr, sizeof(hdr)) != sizeof(hdr)) {
    f.close();
    s_state = STATE_ERROR;
    s_errorMsg = "Invalid .app header";
    return;
  }

  if (memcmp(hdr.magic, "ESP1APP\0", 8) != 0) {
    f.close();
    s_state = STATE_ERROR;
    s_errorMsg = "Not an ESPone app";
    return;
  }


  s_files.clear();
  for (uint16_t i = 0; i < hdr.fileCount; i++) {
    uint16_t plen = 0;
    if (f.read((uint8_t*)&plen, 2) != 2 || plen > 256) break;

    char pathBuf[258];
    if (f.read((uint8_t*)pathBuf, plen) != plen) break;
    pathBuf[plen] = '\0';

    uint32_t offset = 0, size = 0, crc = 0;
    f.read((uint8_t*)&offset, 4);
    f.read((uint8_t*)&size, 4);
    f.read((uint8_t*)&crc, 4);

    AppFileRecord rec;
    rec.relPath = String(pathBuf);
    rec.offset = offset;
    rec.size = size;
    rec.crc = crc;
    s_files.push_back(rec);
  }


  if (hdr.manifestOffset > 0 && hdr.manifestLength > 0 && hdr.manifestLength < 4096) {
    f.seek(hdr.manifestOffset);
    char* jsonBuf = (char*)malloc(hdr.manifestLength + 1);
    if (jsonBuf) {
      f.read((uint8_t*)jsonBuf, hdr.manifestLength);
      jsonBuf[hdr.manifestLength] = '\0';

      JsonDocument doc;
      DeserializationError err = deserializeJson(doc, jsonBuf);
      if (!err) {
        s_appId = doc["id"] | "app";
        s_appName = doc["name"] | s_fileName.c_str();
        s_appVersion = doc["version"] | "1.0";
        s_appTargetMenu = doc["menu"]["target"] | doc["category"] | "apps";

        JsonArray perms = doc["permissions"].as<JsonArray>();
        s_appPermissions = "";
        for (JsonVariant p : perms) {
          if (s_appPermissions.length() > 0) s_appPermissions += ", ";
          s_appPermissions += p.as<const char*>();
        }
        if (s_appPermissions.length() == 0) s_appPermissions = "None";
      }
      free(jsonBuf);
    }
  }

  f.close();

  if (s_appId.length() == 0) {
    s_appId = s_fileName;
    s_appId.replace(".app", "");
  }
  if (s_appName.length() == 0) s_appName = s_appId;

  s_state = STATE_CONFIRM;
}

static void unpackApp() {
  fs::FS* fs = fm_fs::getFS(s_storage);
  if (!fs) {
    s_state = STATE_ERROR;
    s_errorMsg = "Storage unavailable";
    return;
  }

  File src = fs->open(s_fullPath.c_str(), "r");
  if (!src) {
    s_state = STATE_ERROR;
    s_errorMsg = "Cannot read package";
    return;
  }


  String baseDir = "/apps/" + s_appId;
  if (!fs->exists("/apps")) {
    fs->mkdir("/apps");
  }
  if (!fs->exists(baseDir.c_str())) {
    fs->mkdir(baseDir.c_str());
  }

  uint8_t buf[512];
  size_t totalFiles = s_files.size();

  for (size_t i = 0; i < totalFiles; i++) {
    const auto& rec = s_files[i];
    s_progress = (int)((i * 100) / (totalFiles > 0 ? totalFiles : 1));

    String destPath = baseDir + "/" + rec.relPath;


    int lastSlash = destPath.lastIndexOf('/');
    if (lastSlash > 0) {
      String subDir = destPath.substring(0, lastSlash);
      if (!fs->exists(subDir.c_str())) {
        fs->mkdir(subDir.c_str());
      }
    }

    File dst = fs->open(destPath.c_str(), "w");
    if (!dst) {
      src.close();
      s_state = STATE_ERROR;
      s_errorMsg = "Write failed: " + rec.relPath;
      return;
    }

    src.seek(rec.offset);
    uint32_t remaining = rec.size;
    while (remaining > 0) {
      size_t toRead = remaining > sizeof(buf) ? sizeof(buf) : remaining;
      size_t bytesRead = src.read(buf, toRead);
      if (bytesRead == 0) break;
      dst.write(buf, bytesRead);
      remaining -= bytesRead;
    }
    dst.close();
  }

  src.close();
  s_progress = 100;
  s_state = STATE_SUCCESS;
  app_registry::refresh();
}

void tick(uint32_t now) {
  if (s_state == STATE_READING_HEADER) {
    readHeaderAndManifest();
  } else if (s_state == STATE_UNPACKING) {
    unpackApp();
  }
}

void draw(U8G2& u8g2, uint32_t now) {
  u8g2.setFont(u8g2_font_6x12_tr);


  u8g2.setDrawColor(1);
  u8g2.drawBox(0, 0, 128, 13);
  u8g2.setDrawColor(0);
  u8g2.drawXBMP(3, 2, kFmIconW, kFmIconH, file_icon_app_bits);
  u8g2.drawStr(16, 10, "APP INSTALLER");
  u8g2.setDrawColor(1);

  if (s_state == STATE_READING_HEADER) {
    u8g2.drawStr(12, 36, "Reading package...");
  } else if (s_state == STATE_CONFIRM) {

    char title[24];
    snprintf(title, sizeof(title), "%.15s v%.5s", s_appName.c_str(), s_appVersion.c_str());
    u8g2.drawStr(4, 25, title);


    char menuStr[28];
    snprintf(menuStr, sizeof(menuStr), "Menu: %s", s_appTargetMenu.c_str());
    u8g2.drawStr(4, 36, menuStr);


    char permStr[28];
    snprintf(permStr, sizeof(permStr), "Rights: %s", s_appPermissions.c_str());
    u8g2.drawStr(4, 47, permStr);



    int btnY = 51;
    if (s_selectedOption == 0) {
      u8g2.drawBox(4, btnY, 56, 12);
      u8g2.setDrawColor(0);
      u8g2.drawStr(10, btnY + 9, "INSTALL");
      u8g2.setDrawColor(1);
      u8g2.drawFrame(68, btnY, 56, 12);
      u8g2.drawStr(76, btnY + 9, "CANCEL");
    } else {
      u8g2.drawFrame(4, btnY, 56, 12);
      u8g2.drawStr(10, btnY + 9, "INSTALL");
      u8g2.drawBox(68, btnY, 56, 12);
      u8g2.setDrawColor(0);
      u8g2.drawStr(76, btnY + 9, "CANCEL");
      u8g2.setDrawColor(1);
    }
  } else if (s_state == STATE_UNPACKING) {
    u8g2.drawStr(16, 28, "Installing files...");

    u8g2.drawFrame(14, 36, 100, 10);
    int fillW = (s_progress * 96) / 100;
    if (fillW > 0) u8g2.drawBox(16, 38, fillW, 6);
  } else if (s_state == STATE_SUCCESS) {
    u8g2.drawStr(14, 28, "App Installed!");
    u8g2.drawStr(14, 40, "Ready in menu.");
    u8g2.drawStr(4, 60, "OK:LAUNCH  BACK:EXIT");
  } else if (s_state == STATE_ERROR) {
    u8g2.drawStr(4, 28, "Installation Failed:");
    char err[24];
    snprintf(err, sizeof(err), "%.22s", s_errorMsg.c_str());
    u8g2.drawStr(4, 42, err);
    u8g2.drawStr(4, 60, "BACK: RETURN");
  }
}

}
