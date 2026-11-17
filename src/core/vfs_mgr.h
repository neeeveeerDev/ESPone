#pragma once
#include <Arduino.h>
#include <FS.h>
#include <vector>

namespace vfs {

enum class StorageTarget : uint8_t {
  AUTO = 0,     // Read: SD first, fallback to FFat. Write: SD if mounted, else FFat
  INTERNAL,     // FFat only (/int or flash)
  SD            // MicroSD only (/ext or /sd)
};

struct DirEntry {
  char name[48];
  size_t size;
  bool isDir;
  bool isSd;
};

// Lifecycle
bool init();
bool isInternalMounted();
bool isSdMounted(bool forceProbe = false);

// Memory / Volume statistics (in bytes)
uint64_t totalBytes(StorageTarget target = StorageTarget::INTERNAL);
uint64_t usedBytes(StorageTarget target = StorageTarget::INTERNAL);
uint64_t freeBytes(StorageTarget target = StorageTarget::INTERNAL);

// Unified File Operations
File open(const char* path, const char* mode = FILE_READ, StorageTarget target = StorageTarget::AUTO);
File open(const String& path, const char* mode = FILE_READ, StorageTarget target = StorageTarget::AUTO);

bool exists(const char* path);
bool exists(const String& path);

bool remove(const char* path);
bool remove(const String& path);

bool rmdir(const char* path);
bool rmdir(const String& path);

bool mkdir(const char* path, StorageTarget target = StorageTarget::AUTO);
bool mkdir(const String& path, StorageTarget target = StorageTarget::AUTO);

// Directory enumeration (merges /int and /ext if merge = true)
bool listDir(const char* dirPath, std::vector<DirEntry>& outEntries, bool merge = true);
bool listDir(const String& dirPath, std::vector<DirEntry>& outEntries, bool merge = true);

// Path resolvers & helpers
bool isSdPath(const char* path);
String sanitizePath(const char* path);
fs::FS* resolveFs(const char* path, String& outRelPath, bool forWrite = false, StorageTarget defaultTarget = StorageTarget::AUTO);

fs::FS& getInternalFs();
fs::FS& getSdFs();

} // namespace vfs
