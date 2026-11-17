#pragma once
#include <Arduino.h>
#include <vector>

struct SubGhzItem {
  String path;
  String name;
  uint32_t frequency = 433920000;
  String preset = "FuriHalSubGhzPresetOok650Async";
  String protocol = "Came";
  uint8_t bits = 12;
  uint64_t key = 0;
  uint32_t te = 320;
  int repeat = 3;
  bool isRaw = false;
  std::vector<int32_t> rawData;
};

namespace subghz_parser {

bool loadFile(const char* path, SubGhzItem& out);
bool saveFile(const char* path, const SubGhzItem& item);
bool listSavedFiles(std::vector<String>& outPaths);

}
