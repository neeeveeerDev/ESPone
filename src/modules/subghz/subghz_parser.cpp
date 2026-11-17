#include "subghz_parser.h"
#include "core/vfs_mgr.h"

namespace subghz_parser {

static uint64_t parseHexBytes(const String& hexStr) {
  uint64_t val = 0;
  int len = hexStr.length();
  int i = 0;
  while (i < len) {
    while (i < len && (hexStr[i] == ' ' || hexStr[i] == '\t')) i++;
    if (i >= len) break;
    char byteBuf[3] = {0, 0, 0};
    byteBuf[0] = hexStr[i++];
    if (i < len && hexStr[i] != ' ' && hexStr[i] != '\t') {
      byteBuf[1] = hexStr[i++];
    }
    uint8_t b = (uint8_t)strtoul(byteBuf, nullptr, 16);
    val = (val << 8) | b;
  }
  return val;
}

static void parseRawLine(const String& line, std::vector<int32_t>& outData) {
  int start = 0;
  int len = line.length();
  while (start < len) {
    while (start < len && (line[start] == ' ' || line[start] == '\t')) start++;
    if (start >= len) break;
    int end = start;
    while (end < len && line[end] != ' ' && line[end] != '\t') end++;
    String token = line.substring(start, end);
    int32_t val = token.toInt();
    if (val != 0) {
      outData.push_back(val);
    }
    start = end;
  }
}

bool loadFile(const char* path, SubGhzItem& out) {
  out.path = path;
  out.name = "";
  int lastSlash = out.path.lastIndexOf('/');
  if (lastSlash >= 0) {
    out.name = out.path.substring(lastSlash + 1);
  } else {
    out.name = out.path;
  }
  if (out.name.endsWith(".sub")) {
    out.name = out.name.substring(0, out.name.length() - 4);
  }

  File f = vfs::open(path, FILE_READ);
  if (!f) return false;

  out.rawData.clear();
  out.isRaw = false;

  while (f.available()) {
    String line = f.readStringUntil('\n');
    line.trim();
    if (line.length() == 0 || line.startsWith("#")) continue;

    int col = line.indexOf(':');
    if (col < 0) continue;

    String key = line.substring(0, col);
    String val = line.substring(col + 1);
    key.trim();
    val.trim();

    if (key.equalsIgnoreCase("Frequency")) {
      out.frequency = val.toInt();
    } else if (key.equalsIgnoreCase("Preset")) {
      out.preset = val;
    } else if (key.equalsIgnoreCase("Protocol")) {
      out.protocol = val;
      if (out.protocol.equalsIgnoreCase("RAW") || out.protocol.equalsIgnoreCase("BinRAW")) {
        out.isRaw = true;
      }
    } else if (key.equalsIgnoreCase("Bit")) {
      out.bits = val.toInt();
    } else if (key.equalsIgnoreCase("Key")) {
      out.key = parseHexBytes(val);
    } else if (key.equalsIgnoreCase("TE")) {
      out.te = val.toInt();
    } else if (key.equalsIgnoreCase("Repeat")) {
      out.repeat = val.toInt();
    } else if (key.equalsIgnoreCase("RAW_Data")) {
      parseRawLine(val, out.rawData);
    }
  }

  f.close();
  return true;
}

bool saveFile(const char* path, const SubGhzItem& item) {
  File f = vfs::open(path, FILE_WRITE);
  if (!f) return false;

  f.println("Filetype: Flipper SubGhz Key File");
  f.println("Version: 1");
  f.printf("Frequency: %lu\n", (unsigned long)item.frequency);
  f.printf("Preset: %s\n", item.preset.c_str());
  f.printf("Protocol: %s\n", item.protocol.c_str());

  if (item.isRaw || item.protocol.equalsIgnoreCase("RAW")) {
    f.print("RAW_Data:");
    for (size_t i = 0; i < item.rawData.size(); i++) {
      f.printf(" %ld", (long)item.rawData[i]);
    }
    f.println();
  } else {
    f.printf("Bit: %d\n", item.bits);

    f.print("Key:");
    for (int i = 7; i >= 0; i--) {
      uint8_t b = (item.key >> (i * 8)) & 0xFF;
      f.printf(" %02X", b);
    }
    f.println();
    if (item.te > 0) {
      f.printf("TE: %lu\n", (unsigned long)item.te);
    }
    if (item.repeat > 0) {
      f.printf("Repeat: %d\n", item.repeat);
    }
  }

  f.close();
  return true;
}

bool listSavedFiles(std::vector<String>& outPaths) {
  outPaths.clear();
  std::vector<vfs::DirEntry> entries;
  if (vfs::listDir("/sub", entries, true)) {
    for (const auto& entry : entries) {
      if (!entry.isDir && String(entry.name).endsWith(".sub")) {
        String p = entry.isSd ? ("/sd/sub/" + String(entry.name)) : ("/sub/" + String(entry.name));
        outPaths.push_back(p);
      }
    }
  }
  return !outPaths.empty();
}

}
