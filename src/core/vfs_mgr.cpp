#include "vfs_mgr.h"
#include "config.h"
#include <FFat.h>
#include <SD.h>
#include <SPI.h>
#include <set>

namespace vfs {

static bool s_ffatMounted = false;
static bool s_sdMounted = false;
static uint32_t s_lastSdCheck = 0;

static void ensureSpiBusSafe() {
  pinMode(PIN_CC1101_CS, OUTPUT);
  digitalWrite(PIN_CC1101_CS, HIGH);
  pinMode(PIN_SD_CS, OUTPUT);
  digitalWrite(PIN_SD_CS, HIGH);
}

bool init() {
  if (!s_ffatMounted) {
    s_ffatMounted = FFat.begin(false, "", 10, "ffat");
    if (!s_ffatMounted || (s_ffatMounted && FFat.totalBytes() > 0 && FFat.freeBytes() == 0)) {
      Serial.println("[vfs] FFat mount failed or 0 free bytes (corrupted FAT), formatting partition...");
      FFat.end();
      FFat.format();
      s_ffatMounted = FFat.begin(true, "", 10, "ffat");
    }
    if (s_ffatMounted) {
      Serial.printf("[vfs] Internal FFat storage mounted successfully (Total: %llu KB, Free: %llu KB)\n",
                    (uint64_t)(FFat.totalBytes() / 1024), (uint64_t)(FFat.freeBytes() / 1024));
    } else {
      Serial.println("[vfs] CRITICAL: FFat mount/format failed");
    }
  }

  isSdMounted(true);
  return s_ffatMounted;
}

bool isInternalMounted() {
  return s_ffatMounted;
}

bool isSdMounted(bool forceProbe) {
  uint32_t now = millis();
  if (forceProbe || (now - s_lastSdCheck >= 2500) || s_lastSdCheck == 0) {
    s_lastSdCheck = now;
    if (s_sdMounted) {
      if (SD.cardType() == CARD_NONE) {
        s_sdMounted = false;
        Serial.println("[vfs] SD Card removed");
      }
    } else {
      ensureSpiBusSafe();
      SPI.begin(PIN_SD_SCK, PIN_SD_MISO, PIN_SD_MOSI, PIN_SD_CS);
      if (SD.begin(PIN_SD_CS, SPI, 4000000)) {
        s_sdMounted = (SD.cardType() != CARD_NONE);
        if (s_sdMounted) {
          Serial.printf("[vfs] SD Card mounted (Type: %d, Size: %llu MB)\n",
                        SD.cardType(), SD.totalBytes() / (1024 * 1024));
        }
      }
    }
  }
  return s_sdMounted;
}

uint64_t totalBytes(StorageTarget target) {
  if (target == StorageTarget::SD) {
    return isSdMounted() ? SD.totalBytes() : 0;
  }
  return s_ffatMounted ? FFat.totalBytes() : 0;
}

uint64_t usedBytes(StorageTarget target) {
  if (target == StorageTarget::SD) {
    return isSdMounted() ? SD.usedBytes() : 0;
  }
  return s_ffatMounted ? FFat.usedBytes() : 0;
}

uint64_t freeBytes(StorageTarget target) {
  uint64_t tot = totalBytes(target);
  uint64_t usd = usedBytes(target);
  return (tot >= usd) ? (tot - usd) : 0;
}

bool isSdPath(const char* path) {
  if (!path) return false;
  if (strncmp(path, "SD:", 3) == 0 || strncmp(path, "sd:", 3) == 0) return true;
  if (strncmp(path, "EXT:", 4) == 0 || strncmp(path, "ext:", 4) == 0) return true;
  if (strncmp(path, "/sd/", 4) == 0 || strncmp(path, "/SD/", 4) == 0) return true;
  if (strncmp(path, "/ext/", 5) == 0 || strncmp(path, "/EXT/", 5) == 0) return true;
  return false;
}

String sanitizePath(const char* path) {
  if (!path || strlen(path) == 0) return "/";
  String p = path;
  p.trim();
  if (!p.startsWith("/")) {
    p = "/" + p;
  }
  while (p.indexOf("//") >= 0) {
    p.replace("//", "/");
  }
  return p;
}

fs::FS* resolveFs(const char* path, String& outRelPath, bool forWrite, StorageTarget defaultTarget) {
  if (!path || strlen(path) == 0) {
    outRelPath = "/";
    return s_ffatMounted ? &FFat : nullptr;
  }

  // 1. Explicit SD prefixes
  if (strncmp(path, "SD:", 3) == 0 || strncmp(path, "sd:", 3) == 0) {
    outRelPath = sanitizePath(path + 3);
    return isSdMounted() ? &SD : nullptr;
  }
  if (strncmp(path, "EXT:", 4) == 0 || strncmp(path, "ext:", 4) == 0) {
    outRelPath = sanitizePath(path + 4);
    return isSdMounted() ? &SD : nullptr;
  }
  if (strncmp(path, "/sd/", 4) == 0 || strncmp(path, "/SD/", 4) == 0) {
    outRelPath = sanitizePath(path + 3); // keeps leading slash
    return isSdMounted() ? &SD : nullptr;
  }
  if (strncmp(path, "/ext/", 5) == 0 || strncmp(path, "/EXT/", 5) == 0) {
    outRelPath = sanitizePath(path + 4); // keeps leading slash
    return isSdMounted() ? &SD : nullptr;
  }

  // 2. Explicit Internal (FFat) prefixes
  if (strncmp(path, "INT:", 4) == 0 || strncmp(path, "int:", 4) == 0) {
    outRelPath = sanitizePath(path + 4);
    return s_ffatMounted ? &FFat : nullptr;
  }
  if (strncmp(path, "/int/", 5) == 0 || strncmp(path, "/INT/", 5) == 0) {
    outRelPath = sanitizePath(path + 4);
    return s_ffatMounted ? &FFat : nullptr;
  }

  // 3. Neutral standard paths (e.g. /sub/gate.sub, /pcaps/dump.pcap)
  outRelPath = sanitizePath(path);

  if (forWrite) {
    if (defaultTarget == StorageTarget::INTERNAL) {
      return s_ffatMounted ? &FFat : nullptr;
    }
    if (defaultTarget == StorageTarget::SD) {
      return isSdMounted() ? &SD : nullptr;
    }
    // AUTO mode for write: prefer SD card if mounted to save internal Flash wear
    if (isSdMounted()) {
      return &SD;
    }
    return s_ffatMounted ? &FFat : nullptr;
  }

  // Read mode: check SD card first (if mounted and file exists), then fallback to FFat
  if (isSdMounted() && SD.exists(outRelPath.c_str())) {
    return &SD;
  }
  return s_ffatMounted ? &FFat : nullptr;
}

static bool mkdirRecursiveImpl(fs::FS* fs, const String& fullPath) {
  if (!fs) return false;
  String p = fullPath;
  p.trim();
  while (p.length() > 1 && p.endsWith("/")) {
    p.remove(p.length() - 1);
  }
  if (!p.startsWith("/")) {
    p = "/" + p;
  }
  if (p == "/" || p.length() <= 1) return true;
  if (fs->exists(p.c_str())) return true;

  int idx = 1;
  while ((idx = p.indexOf('/', idx)) != -1) {
    String sub = p.substring(0, idx);
    if (sub.length() > 1 && !fs->exists(sub.c_str())) {
      fs->mkdir(sub.c_str());
    }
    idx++;
  }

  if (!fs->exists(p.c_str())) {
    return fs->mkdir(p.c_str());
  }
  return true;
}

File open(const char* path, const char* mode, StorageTarget target) {
  if (!path) return File();
  bool isWrite = (mode && (mode[0] == 'w' || mode[0] == 'a' || strchr(mode, '+') != nullptr));
  String relPath;
  fs::FS* fs = resolveFs(path, relPath, isWrite, target);
  if (!fs) return File();
  if (isWrite) {
    int lastSlash = relPath.lastIndexOf('/');
    if (lastSlash > 0) {
      mkdirRecursiveImpl(fs, relPath.substring(0, lastSlash));
    }
  }
  return fs->open(relPath.c_str(), mode);
}

File open(const String& path, const char* mode, StorageTarget target) {
  return open(path.c_str(), mode, target);
}

bool exists(const char* path) {
  if (!path) return false;
  String relPath;
  fs::FS* fs = resolveFs(path, relPath, false, StorageTarget::AUTO);
  if (fs && fs->exists(relPath.c_str())) {
    return true;
  }
  // If not found in primary resolved FS and it was a neutral path, check other FS
  if (!isSdPath(path) && isSdMounted() && fs != &SD) {
    if (SD.exists(relPath.c_str())) return true;
  }
  return false;
}

bool exists(const String& path) {
  return exists(path.c_str());
}

bool remove(const char* path) {
  if (!path) return false;
  String relPath;
  fs::FS* fs = resolveFs(path, relPath, true, StorageTarget::AUTO);
  if (fs) {
    return fs->remove(relPath.c_str());
  }
  return false;
}

bool remove(const String& path) {
  return remove(path.c_str());
}

bool rmdir(const char* path) {
  if (!path) return false;
  String relPath;
  fs::FS* fs = resolveFs(path, relPath, true, StorageTarget::AUTO);
  if (fs) {
    return fs->rmdir(relPath.c_str());
  }
  return false;
}

bool rmdir(const String& path) {
  return rmdir(path.c_str());
}

bool mkdir(const char* path, StorageTarget target) {
  if (!path) return false;
  String relPath;
  fs::FS* fs = resolveFs(path, relPath, true, target);
  if (fs) {
    return mkdirRecursiveImpl(fs, relPath);
  }
  return false;
}

bool mkdir(const String& path, StorageTarget target) {
  return mkdir(path.c_str(), target);
}

bool listDir(const char* dirPath, std::vector<DirEntry>& outEntries, bool merge) {
  outEntries.clear();
  if (!dirPath) return false;

  String relPath;
  bool isExplicit = isSdPath(dirPath) || (strncmp(dirPath, "INT:", 4) == 0) || (strncmp(dirPath, "/int/", 5) == 0);

  auto scanFs = [&](fs::FS& fs, bool isSd) {
    File root = fs.open(relPath.c_str());
    if (!root || !root.isDirectory()) return false;

    File file = root.openNextFile();
    while (file) {
      DirEntry entry;
      const char* rawName = file.name();
      // Remove leading directory path if returned by esp32 FS
      const char* slash = strrchr(rawName, '/');
      const char* baseName = slash ? (slash + 1) : rawName;

      strncpy(entry.name, baseName, sizeof(entry.name) - 1);
      entry.name[sizeof(entry.name) - 1] = '\0';
      entry.size = file.size();
      entry.isDir = file.isDirectory();
      entry.isSd = isSd;
      outEntries.push_back(entry);

      file = root.openNextFile();
    }
    return true;
  };

  if (isExplicit || !merge) {
    fs::FS* fs = resolveFs(dirPath, relPath, false, StorageTarget::AUTO);
    if (!fs) return false;
    return scanFs(*fs, (fs == &SD));
  }

  // Merged listing of both /int and /ext
  relPath = sanitizePath(dirPath);
  std::set<String> seen;

  if (s_ffatMounted) {
    File root = FFat.open(relPath.c_str());
    if (root && root.isDirectory()) {
      File file = root.openNextFile();
      while (file) {
        DirEntry entry;
        const char* slash = strrchr(file.name(), '/');
        const char* baseName = slash ? (slash + 1) : file.name();
        strncpy(entry.name, baseName, sizeof(entry.name) - 1);
        entry.name[sizeof(entry.name) - 1] = '\0';
        entry.size = file.size();
        entry.isDir = file.isDirectory();
        entry.isSd = false;

        seen.insert(String(baseName));
        outEntries.push_back(entry);
        file = root.openNextFile();
      }
    }
  }

  if (isSdMounted()) {
    File root = SD.open(relPath.c_str());
    if (root && root.isDirectory()) {
      File file = root.openNextFile();
      while (file) {
        const char* slash = strrchr(file.name(), '/');
        const char* baseName = slash ? (slash + 1) : file.name();

        if (seen.find(String(baseName)) == seen.end()) {
          DirEntry entry;
          strncpy(entry.name, baseName, sizeof(entry.name) - 1);
          entry.name[sizeof(entry.name) - 1] = '\0';
          entry.size = file.size();
          entry.isDir = file.isDirectory();
          entry.isSd = true;

          outEntries.push_back(entry);
        }
        file = root.openNextFile();
      }
    }
  }

  return true;
}

bool listDir(const String& dirPath, std::vector<DirEntry>& outEntries, bool merge) {
  return listDir(dirPath.c_str(), outEntries, merge);
}

fs::FS& getInternalFs() {
  return FFat;
}

fs::FS& getSdFs() {
  return SD;
}

} // namespace vfs
