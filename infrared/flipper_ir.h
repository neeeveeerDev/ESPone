#pragma once
#include <Arduino.h>
#include <FS.h>
#include <vector>

namespace flipper_ir {

struct Signal {
  String name;
  bool isRaw = false;
  String protocol;
  uint32_t address = 0;
  uint32_t command = 0;
  uint32_t frequency = 38000;
  std::vector<uint16_t> rawTimings;
};


bool loadIrFile(fs::FS& fs, const char* path, std::vector<Signal>& signals);
bool loadIrFile(const char* path, std::vector<Signal>& signals);

bool saveSignalToFile(fs::FS& fs, const char* path, const Signal& sig, bool append = true);
bool saveSignalToFile(const char* path, const Signal& sig, bool append = true);


void transmitSignal(const Signal& sig);

}
