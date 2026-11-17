#include "flipper_ir.h"
#include "ir_sender_hw.h"
#include "ir_db.h"
#include "core/vfs_mgr.h"

namespace flipper_ir {

static uint32_t parseHexBytes(const String& str) {
  uint32_t val = 0;
  char buf[48];
  snprintf(buf, sizeof(buf), "%s", str.c_str());
  char* token = strtok(buf, " \t\r\n");
  int shift = 0;
  while (token && shift < 32) {
    uint32_t b = strtoul(token, nullptr, 16) & 0xFF;
    val |= (b << shift);
    shift += 8;
    token = strtok(nullptr, " \t\r\n");
  }
  return val;
}

static void formatHexBytes(uint32_t val, char* out, size_t outSize) {
  snprintf(out, outSize, "%02X %02X %02X %02X",
           (uint8_t)(val & 0xFF),
           (uint8_t)((val >> 8) & 0xFF),
           (uint8_t)((val >> 16) & 0xFF),
           (uint8_t)((val >> 24) & 0xFF));
}

bool loadIrFile(fs::FS& fs, const char* path, std::vector<Signal>& signals) {
  signals.clear();
  File f = fs.open(path, FILE_READ);
  if (!f) return false;

  Signal current;
  bool inSignal = false;

  while (f.available()) {
    String line = f.readStringUntil('\n');
    line.trim();

    if (line.startsWith("#") || line.length() == 0) {
      if (inSignal && current.name.length() > 0) {
        signals.push_back(current);
        current = Signal();
        inSignal = false;
      }
      continue;
    }

    int colon = line.indexOf(':');
    if (colon <= 0) continue;

    String key = line.substring(0, colon);
    String val = line.substring(colon + 1);
    key.trim();
    val.trim();

    if (key == "name") {
      if (inSignal && current.name.length() > 0) {
        signals.push_back(current);
        current = Signal();
      }
      current.name = val;
      inSignal = true;
    } else if (key == "type") {
      current.isRaw = (val == "raw");
      inSignal = true;
    } else if (key == "protocol") {
      current.protocol = val;
      inSignal = true;
    } else if (key == "address") {
      current.address = parseHexBytes(val);
      inSignal = true;
    } else if (key == "command") {
      current.command = parseHexBytes(val);
      inSignal = true;
    } else if (key == "frequency") {
      current.frequency = val.toInt();
      inSignal = true;
    } else if (key == "data") {
      current.isRaw = true;
      inSignal = true;
      char buf[256];
      snprintf(buf, sizeof(buf), "%s", val.c_str());
      char* token = strtok(buf, " \t\r\n");
      while (token) {
        current.rawTimings.push_back((uint16_t)atoi(token));
        token = strtok(nullptr, " \t\r\n");
      }
    }
  }

  if (inSignal && current.name.length() > 0) {
    signals.push_back(current);
  }

  f.close();
  return !signals.empty();
}

bool saveSignalToFile(fs::FS& fs, const char* path, const Signal& sig, bool append) {
  bool exists = fs.exists(path);
  File f = fs.open(path, (append && exists) ? FILE_APPEND : FILE_WRITE);
  if (!f) return false;

  if (!exists || !append) {
    f.println("Filetype: IR signals file");
    f.println("Version: 1");
  }

  f.println("#");
  f.printf("name: %s\n", sig.name.length() > 0 ? sig.name.c_str() : "Button");

  if (!sig.isRaw) {
    f.println("type: parsed");
    f.printf("protocol: %s\n", sig.protocol.length() > 0 ? sig.protocol.c_str() : "NEC");
    char hexBuf[24];
    formatHexBytes(sig.address, hexBuf, sizeof(hexBuf));
    f.printf("address: %s\n", hexBuf);
    formatHexBytes(sig.command, hexBuf, sizeof(hexBuf));
    f.printf("command: %s\n", hexBuf);
  } else {
    f.println("type: raw");
    f.printf("frequency: %lu\n", sig.frequency ? sig.frequency : 38000UL);
    f.println("duty_cycle: 0.330000");
    f.print("data:");
    for (size_t i = 0; i < sig.rawTimings.size(); i++) {
      f.printf(" %u", sig.rawTimings[i]);
    }
    f.println();
  }

  f.close();
  return true;
}

bool loadIrFile(const char* path, std::vector<Signal>& signals) {
  String relPath;
  fs::FS* fs = vfs::resolveFs(path, relPath, false);
  if (!fs) return false;
  return loadIrFile(*fs, relPath.c_str(), signals);
}

bool saveSignalToFile(const char* path, const Signal& sig, bool append) {
  String relPath;
  fs::FS* fs = vfs::resolveFs(path, relPath, true);
  if (!fs) return false;
  return saveSignalToFile(*fs, relPath.c_str(), sig, append);
}

void transmitSignal(const Signal& sig) {
  if (!sig.rawTimings.empty()) {
    ir_sender_hw::sendRaw(sig.rawTimings.data(), sig.rawTimings.size(),
                          sig.frequency ? sig.frequency / 1000 : 38);
    return;
  }

  ir_db::IrCode code;
  code.name = sig.name.c_str();
  code.address = sig.address;
  code.command = sig.command;
  code.bits = 32;

  String p = sig.protocol;
  p.toUpperCase();

  if (p.startsWith("NEC")) {
    code.proto = ir_db::IR_PROTO_NEC;
  } else if (p.startsWith("SAMSUNG")) {
    code.proto = ir_db::IR_PROTO_SAMSUNG;
  } else if (p.startsWith("SONY") || p.startsWith("SIRC")) {
    code.proto = ir_db::IR_PROTO_SONY;
    code.bits = (p.indexOf("15") >= 0) ? 15 : ((p.indexOf("20") >= 0) ? 20 : 12);
  } else if (p.startsWith("RC5")) {
    code.proto = ir_db::IR_PROTO_RC5;
    code.bits = 12;
  } else if (p.startsWith("RC6")) {
    code.proto = ir_db::IR_PROTO_RC6;
    code.bits = 20;
  } else if (p.startsWith("PANASONIC") || p.startsWith("KASEIKYO")) {
    code.proto = ir_db::IR_PROTO_PANASONIC;
    code.bits = 48;
  } else if (p.startsWith("LG")) {
    code.proto = ir_db::IR_PROTO_LG;
  } else if (p.startsWith("SHARP")) {
    code.proto = ir_db::IR_PROTO_SHARP;
  } else if (p.startsWith("JVC")) {
    code.proto = ir_db::IR_PROTO_JVC;
  } else {
    code.proto = ir_db::IR_PROTO_NEC;
  }

  ir_sender_hw::sendCode(code);
}

}
