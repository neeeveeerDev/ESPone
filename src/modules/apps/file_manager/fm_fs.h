#pragma once
#include <Arduino.h>
#include <FS.h>
#include <vector>

namespace fm_fs {

enum StorageType {
  STORAGE_NONE = 0,
  STORAGE_SD,
  STORAGE_INTERNAL
};

struct FileItem {
  char name[36];
  bool isDir;
  size_t size;
};

bool initInternal();
bool initSD();
bool isStorageReady(StorageType st);
const char* getStorageName(StorageType st);

fs::FS* getFS(StorageType st);

bool listDir(StorageType st, const char* path, std::vector<FileItem>& items);
bool mkdirRecursive(fs::FS* fs, const String& fullPath);
bool createDir(StorageType st, const char* path);
bool createFile(StorageType st, const char* path, const char* initialContent = "");
bool deleteItem(StorageType st, const char* path, bool isDir);
bool copyFile(StorageType st, const char* srcPath, const char* dstPath);
bool moveFile(StorageType st, const char* srcPath, const char* dstPath);
bool readFileContent(StorageType st, const char* path, std::vector<String>& lines, size_t maxLines = 100);
size_t getFileSize(StorageType st, const char* path);

}
