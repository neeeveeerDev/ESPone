#include "sys_storage.h"
#include "vfs_mgr.h"
#include <FFat.h>
#include <SD.h>
#include <SPI.h>
#include "config.h"
#include "modules/settings/settings_data.h"

namespace sys_storage {

static bool s_inited = false;

static void ensureDefaultIrFile() {
  if (!FFat.exists("/ir/tv.ir")) {
    File f = FFat.open("/ir/tv.ir", FILE_WRITE);
    if (f) {
      f.println("Filetype: IR signals file");
      f.println("Version: 1");
      f.println("#");
      f.println("name: Power");
      f.println("type: parsed");
      f.println("protocol: NEC");
      f.println("address: 04 00 00 00");
      f.println("command: 08 00 00 00");
      f.println("#");
      f.println("name: Mute");
      f.println("type: parsed");
      f.println("protocol: NEC");
      f.println("address: 04 00 00 00");
      f.println("command: 09 00 00 00");
      f.println("#");
      f.println("name: Vol_up");
      f.println("type: parsed");
      f.println("protocol: NEC");
      f.println("address: 04 00 00 00");
      f.println("command: 02 00 00 00");
      f.println("#");
      f.println("name: Vol_dn");
      f.println("type: parsed");
      f.println("protocol: NEC");
      f.println("address: 04 00 00 00");
      f.println("command: 03 00 00 00");
      f.close();
      Serial.println("[sys_storage] Created default /ir/tv.ir");
    }
  }
}

static void ensureDefaultSubFile() {
  if (!FFat.exists("/sub/gate_came.sub")) {
    File f = FFat.open("/sub/gate_came.sub", FILE_WRITE);
    if (f) {
      f.println("Filetype: Flipper SubGhz Key File");
      f.println("Version: 1");
      f.println("Frequency: 433920000");
      f.println("Preset: FuriHalSubGhzPresetOok650Async");
      f.println("Protocol: Came");
      f.println("Bit: 12");
      f.println("Key: 00 00 00 00 00 00 0E 12");
      f.println("TE: 320");
      f.println("Repeat: 3");
      f.close();
      Serial.println("[sys_storage] Created default /sub/gate_came.sub");
    }
  }
  if (!FFat.exists("/sub/barrier_nice.sub")) {
    File f = FFat.open("/sub/barrier_nice.sub", FILE_WRITE);
    if (f) {
      f.println("Filetype: Flipper SubGhz Key File");
      f.println("Version: 1");
      f.println("Frequency: 433920000");
      f.println("Preset: FuriHalSubGhzPresetOok650Async");
      f.println("Protocol: Nice FLO");
      f.println("Bit: 12");
      f.println("Key: 00 00 00 00 00 00 0A 55");
      f.println("TE: 700");
      f.println("Repeat: 3");
      f.close();
      Serial.println("[sys_storage] Created default /sub/barrier_nice.sub");
    }
  }
}

static void ensureDefaultBadUsb() {
  if (!FFat.exists("/badusb/hello_world.txt")) {
    File f = FFat.open("/badusb/hello_world.txt", FILE_WRITE);
    if (f) {
      f.println("REM Hello World Demo");
      f.println("GUI r");
      f.println("DELAY 400");
      f.println("STRING notepad.exe");
      f.println("ENTER");
      f.println("DELAY 600");
      f.println("STRING Hello from ESPone Multi-Tool!");
      f.println("ENTER");
      f.close();
      Serial.println("[sys_storage] Created default /badusb/hello_world.txt");
    }
  }
}

void init() {
  if (s_inited) return;
  s_inited = true;

  bool mounted = vfs::init();
  if (mounted) {

    Serial.println("[sys_storage] FFat ready");


    if (!FFat.exists("/system")) {
      FFat.mkdir("/system");
      Serial.println("[sys_storage] Created /system");
    }
    if (!FFat.exists("/pcaps")) {
      FFat.mkdir("/pcaps");
      Serial.println("[sys_storage] Created /pcaps");
    }
    if (!FFat.exists("/ir")) {
      FFat.mkdir("/ir");
      Serial.println("[sys_storage] Created /ir");
    }
    if (!FFat.exists("/sub")) {
      FFat.mkdir("/sub");
      Serial.println("[sys_storage] Created /sub");
    }
    if (!FFat.exists("/apps")) {
      FFat.mkdir("/apps");
      Serial.println("[sys_storage] Created /apps");
    }
    if (!FFat.exists("/bruteforce")) {
      FFat.mkdir("/bruteforce");
      Serial.println("[sys_storage] Created /bruteforce");
    }
    if (!FFat.exists("/bruteforce/common.txt")) {
      File f = FFat.open("/bruteforce/common.txt", FILE_WRITE);
      if (f) {
        f.println("12345678");
        f.println("password");
        f.println("123456789");
        f.println("admin123");
        f.println("11111111");
        f.println("00000000");
        f.println("qwertyuiop");
        f.println("1234567890");
        f.println("guest123");
        f.println("internet");
        f.close();
        Serial.println("[sys_storage] Created default /bruteforce/common.txt");
      }
    }


    if (FFat.exists("/badusb/hello_world.txt")) {
      FFat.remove("/badusb/hello_world.txt");
    }
    if (FFat.exists("/badusb")) {
      FFat.rmdir("/badusb");
    }
    if (FFat.exists("/adb")) {
      FFat.rmdir("/adb");
    }

    if (!FFat.exists("/pcaps/readme.txt")) {
      File f = FFat.open("/pcaps/readme.txt", FILE_WRITE);
      if (f) {
        f.println("Handshake captures (.pcap) saved here.");
        f.close();
      }
    }

    ensureDefaultIrFile();
    ensureDefaultSubFile();


    if (!FFat.exists("/system/sys.config")) {
      saveConfig();
    } else {
      loadConfig();
    }
  } else {
    Serial.println("[sys_storage] Failed to mount FFat");
  }
}

void loadConfig() {
  if (!FFat.exists("/system/sys.config")) return;
  File f = FFat.open("/system/sys.config", FILE_READ);
  if (!f) return;

  while (f.available()) {
    String line = f.readStringUntil('\n');
    line.trim();
    if (line.startsWith("#") || line.length() == 0) continue;

    int eq = line.indexOf('=');
    if (eq > 0) {
      String key = line.substring(0, eq);
      String val = line.substring(eq + 1);
      key.trim();
      val.trim();

      if (key == "brightness") {
        settings_data::setBrightness((uint8_t)val.toInt());
      } else if (key == "invert") {
        settings_data::setInvert(val.toInt() != 0);
      } else if (key == "screensaver") {
        settings_data::setScreensaver((uint8_t)val.toInt());
      } else if (key == "screen_off") {
        settings_data::setScreenOffSec((uint16_t)val.toInt());
      } else if (key == "sleep_timeout") {
        settings_data::setSleepSec((uint16_t)val.toInt());
      } else if (key == "ble_enabled") {
        settings_data::setBleEnabled(val.toInt() != 0);
      } else if (key == "menu_order") {
        uint8_t ord[8];
        int idx = 0;
        int start = 0;
        for (int i = 0; i <= val.length() && idx < 8; i++) {
          if (i == val.length() || val[i] == ',') {
            ord[idx++] = (uint8_t)val.substring(start, i).toInt();
            start = i + 1;
          }
        }
        if (idx == 8) {
          settings_data::setMenuOrder(ord, 8);
        }
      } else if (key == "menu_hidden") {
        settings_data::setMenuHiddenMask((uint16_t)val.toInt());
      }
    }
  }
  f.close();
  settings_data::apply();
  Serial.println("[sys_storage] Loaded config from /system/sys.config");
}

void saveConfig() {
  File f = FFat.open("/system/sys.config", FILE_WRITE);
  if (!f) return;

  f.println("# ESPone System Configuration");
  f.printf("brightness=%u\n", settings_data::getBrightness());
  f.printf("invert=%d\n", settings_data::getInvert() ? 1 : 0);
  f.printf("screensaver=%u\n", settings_data::getScreensaver());
  f.printf("screen_off=%u\n", settings_data::getScreenOffSec());
  f.printf("sleep_timeout=%u\n", settings_data::getSleepSec());
  f.printf("ble_enabled=%d\n", settings_data::getBleEnabled() ? 1 : 0);


  char orderStr[32] = "";
  uint8_t ord[8];
  settings_data::getMenuOrder(ord, 8);
  for (int i = 0; i < 8; i++) {
    char b[6];
    snprintf(b, sizeof(b), "%d%s", ord[i], (i < 7) ? "," : "");
    strcat(orderStr, b);
  }
  f.printf("menu_order=%s\n", orderStr);
  f.printf("menu_hidden=%u\n", settings_data::getMenuHiddenMask());

  f.close();
  Serial.println("[sys_storage] Saved config to /system/sys.config");
}

bool isSdMounted() {
  return vfs::isSdMounted();
}

}

