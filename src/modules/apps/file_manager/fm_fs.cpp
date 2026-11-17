#include "fm_fs.h"
#include "core/vfs_mgr.h"
#include <FFat.h>
#include <SD.h>
#include <SPI.h>
#include <algorithm>
#include "config.h"

namespace fm_fs {

bool initInternal() {
  return vfs::init();
}

bool initSD() {
  return vfs::isSdMounted(true);
}

bool isStorageReady(StorageType st) {
  if (st == STORAGE_INTERNAL) return vfs::isInternalMounted();
  if (st == STORAGE_SD) return vfs::isSdMounted();
  return false;
}

const char* getStorageName(StorageType st) {
  if (st == STORAGE_SD) return "SD Card";
  if (st == STORAGE_INTERNAL) return "Internal";
  return "None";
}

fs::FS* getFS(StorageType st) {
  if (st == STORAGE_INTERNAL) {
    if (!vfs::isInternalMounted()) vfs::init();
    if (vfs::isInternalMounted()) return &vfs::getInternalFs();
  }
  if (st == STORAGE_SD) {
    if (!vfs::isSdMounted()) vfs::isSdMounted(true);
    if (vfs::isSdMounted()) return &vfs::getSdFs();
  }
  return nullptr;
}

bool mkdirRecursive(fs::FS* fs, const String& fullPath) {
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
      bool ok = fs->mkdir(sub.c_str());
      if (!ok) return false;
    }
    idx++;
  }

  if (!fs->exists(p.c_str())) {
    return fs->mkdir(p.c_str());
  }
  return true;
}

bool listDir(StorageType st, const char* path, std::vector<FileItem>& items) {
  items.clear();
  fs::FS* fs = getFS(st);
  if (!fs) return false;

  String p = path ? path : "/";
  p.trim();
  if (!p.startsWith("/")) p = "/" + p;

  File root = fs->open(p.c_str());
  if (!root || !root.isDirectory()) return false;

  std::vector<FileItem> dirs;
  std::vector<FileItem> files;

  File file = root.openNextFile();
  while (file) {
    FileItem it;
    const char* fn = file.name();

    const char* baseName = strrchr(fn, '/');
    if (baseName) baseName++;
    else baseName = fn;

    snprintf(it.name, sizeof(it.name), "%s", baseName);
    it.isDir = file.isDirectory();
    it.size = file.size();

    if (it.name[0] != '.') {
      if (it.isDir) dirs.push_back(it);
      else files.push_back(it);
    }

    file = root.openNextFile();
  }

  auto sortFunc = [](const FileItem& a, const FileItem& b) {
    return strcasecmp(a.name, b.name) < 0;
  };
  std::sort(dirs.begin(), dirs.end(), sortFunc);
  std::sort(files.begin(), files.end(), sortFunc);

  items.insert(items.end(), dirs.begin(), dirs.end());
  items.insert(items.end(), files.begin(), files.end());
  return true;
}

bool createDir(StorageType st, const char* path) {
  fs::FS* fs = getFS(st);
  if (!fs || !path) return false;
  return mkdirRecursive(fs, String(path));
}

bool createFile(StorageType st, const char* path, const char* initialContent) {
  fs::FS* fs = getFS(st);
  if (!fs || !path) return false;

  String p = path;
  p.trim();
  if (!p.startsWith("/")) p = "/" + p;

  int lastSlash = p.lastIndexOf('/');
  if (lastSlash > 0) {
    String parentDir = p.substring(0, lastSlash);
    mkdirRecursive(fs, parentDir);
  }

  File f = fs->open(p.c_str(), FILE_WRITE);
  if (!f) return false;
  if (initialContent && strlen(initialContent) > 0) {
    f.print(initialContent);
  }
  f.flush();
  f.close();
  return true;
}

static bool rmdirRecursive(fs::FS* fs, const String& path) {
  if (!fs) return false;
  File dir = fs->open(path.c_str());
  if (!dir || !dir.isDirectory()) return false;
  File file = dir.openNextFile();
  while (file) {
    String sub = path;
    if (!sub.endsWith("/")) sub += "/";
    const char* fn = file.name();
    const char* baseName = strrchr(fn, '/');
    if (baseName) baseName++;
    else baseName = fn;
    sub += baseName;

    if (file.isDirectory()) {
      rmdirRecursive(fs, sub);
    } else {
      fs->remove(sub.c_str());
    }
    file = dir.openNextFile();
  }
  dir.close();
  return fs->rmdir(path.c_str());
}

bool deleteItem(StorageType st, const char* path, bool isDir) {
  fs::FS* fs = getFS(st);
  if (!fs || !path) return false;
  if (isDir) {
    if (fs->rmdir(path)) return true;
    return rmdirRecursive(fs, String(path));
  } else {
    return fs->remove(path);
  }
}

bool copyFile(StorageType st, const char* srcPath, const char* dstPath) {
  fs::FS* fs = getFS(st);
  if (!fs) return false;
  File src = fs->open(srcPath, FILE_READ);
  if (!src) return false;
  File dst = fs->open(dstPath, FILE_WRITE);
  if (!dst) {
    src.close();
    return false;
  }

  uint8_t buf[256];
  while (src.available()) {
    size_t r = src.read(buf, sizeof(buf));
    if (r > 0) dst.write(buf, r);
  }
  src.close();
  dst.close();
  return true;
}

bool moveFile(StorageType st, const char* srcPath, const char* dstPath) {
  fs::FS* fs = getFS(st);
  if (!fs) return false;
  if (fs->rename(srcPath, dstPath)) return true;

  if (copyFile(st, srcPath, dstPath)) {
    fs->remove(srcPath);
    return true;
  }
  return false;
}

bool readFileContent(StorageType st, const char* path, std::vector<String>& lines, size_t maxLines) {
  lines.clear();
  fs::FS* fs = getFS(st);
  if (!fs) return false;
  File f = fs->open(path, FILE_READ);
  if (!f) return false;

  while (f.available() && lines.size() < maxLines) {
    String line = f.readStringUntil('\n');
    line.trim();
    lines.push_back(line);
  }
  f.close();
  return true;
}

size_t getFileSize(StorageType st, const char* path) {
  fs::FS* fs = getFS(st);
  if (!fs) return 0;
  File f = fs->open(path, FILE_READ);
  if (!f) return 0;
  size_t s = f.size();
  f.close();
  return s;
}

}
