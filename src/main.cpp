#include <Arduino.h>
#include "config.h"
#include "core/display.h"
#include "core/buttons.h"
#include "ui/splash.h"
#include "ui/pairing.h"
#include "ui/menu.h"
#include "ui/disclaimer.h"
#include "ui/home_scenes.h"
#include "modules/modules.h"
#include "core/ble_service.h"
#include "core/usb_remote.h"
#include "core/battery.h"
#include "modules/settings/settings_data.h"
#include "ui/screensaver.h"
#include "ui/charging_view.h"
#include "ui/keyboard.h"
#include "core/js/app_registry.h"
#include "core/power_mgr.h"
#include "core/supervisor.h"
#include "core/screenshot.h"
#include <FFat.h>
#include <SD.h>
#include <Wire.h>








enum MainState : uint8_t { ST_SPLASH, ST_DISCLAIMER, ST_PAIRING, ST_HOME, ST_MENU, ST_APP };
static MainState state = ST_SPLASH;
static MenuView menu;
static App* activeApp = nullptr;
static uint32_t lastDraw = 0;
static bool dirty = true;
static uint32_t lastActivity = 0;

static void drawFrame(uint32_t now) {
  U8G2& u8g2 = display::get();
  u8g2.clearBuffer();
  if (charging_view::isActive()) {
    charging_view::draw(u8g2, now);
  } else if (screensaver::isActive()) {
    screensaver::draw(u8g2, now);
  } else if (ui_keyboard::isModal()) {
    ui_keyboard::draw(u8g2, now);
  } else {
    switch (state) {
      case ST_SPLASH:      splash::draw(u8g2, now); break;
      case ST_DISCLAIMER:  disclaimer::draw(u8g2, now); break;
      case ST_PAIRING:     pairing::draw(u8g2, now); break;
      case ST_HOME:        home_scenes::draw(u8g2, now); break;
      case ST_MENU:        menu.draw(u8g2, now);    break;
      case ST_APP:         activeApp->draw(u8g2, now); break;
    }
  }

  if (screenshot::isFlashing()) {
    u8g2.setDrawColor(2);
    u8g2.drawBox(0, 0, 128, 64);
    u8g2.setDrawColor(1);
  }

  u8g2.sendBuffer();


  ble_service::sendScreen(u8g2.getBufferPtr(), 1024);


  usb_remote::sendScreenFrame(u8g2.getBufferPtr(), 1024);
}

void setup() {
  Serial.begin(921600);
  delay(50);
  Serial.println("\n[ESPone] boot (Web Remote BLE enabled)");

  display::init();
  settings_data::init();
  battery::init();
  charging_view::init();
  buttons::init();
  ble_service::init();
  usb_remote::init();
  disclaimer::init();
  home_scenes::init();
  app_registry::init();
  modules::initMenu(menu);
  power_mgr::init();
  supervisor::init();
  screenshot::init();
  splash::start(millis());
}

void loop() {
  uint32_t now = millis();
  if (lastActivity == 0) lastActivity = now;

  battery::tick(now);
  supervisor::heartbeat(now);
  power_mgr::tick(now, lastActivity);

  if (supervisor::isForceKillRequested()) {
    supervisor::clearForceKill();
    if (state == ST_APP && activeApp) {
      activeApp->close();
      activeApp = nullptr;
      state = ST_MENU;
      power_mgr::setProfile(power_mgr::CpuProfile::BALANCED);
      dirty = true;
      Serial.println("[main] Emergency Force-Kill executed -> returned to menu");
    }
  }

  if (supervisor::isScreenshotRequested()) {
    supervisor::clearScreenshotRequest();
    String scPath;
    if (screenshot::capture(scPath)) {
      dirty = true;
    }
  }

  ble_service::tick();


  usb_remote::tick();


  auto handleCli = [](Stream& s) {
    if (!s.available()) return;
    int first = s.peek();

    if (first == 0x55) return;


    if (first < 0x20 && first != '\r' && first != '\n') {
      s.read();
      return;
    }

    s.setTimeout(15);
    String cmd = s.readStringUntil('\n');
    cmd.trim();
    if (cmd.length() == 0) return;

    if (cmd == "folders" || cmd == "ls") {
      s.println("\n=== [FFat Internal Storage] ===");
      File root = FFat.open("/");
      if (!root) {
        s.println("Failed to open FFat root");
      } else {
        File file = root.openNextFile();
        while (file) {
          s.printf("  %s %-20s (%u bytes)\n", file.isDirectory() ? "[DIR] " : "[FILE]", file.name(), file.size());
          if (file.isDirectory()) {
            char subPath[64];
            snprintf(subPath, sizeof(subPath), "/%s", file.name());
            File sub = FFat.open(subPath);
            if (sub) {
              File subFile = sub.openNextFile();
              while (subFile) {
                s.printf("    %s %-18s (%u bytes)\n", subFile.isDirectory() ? "[DIR] " : "[FILE]", subFile.name(), subFile.size());
                subFile = sub.openNextFile();
              }
            }
          }
          file = root.openNextFile();
        }
      }
      if (SD.exists("/")) {
        s.println("=== [SD Card Storage] ===");
        File sdRoot = SD.open("/");
        if (sdRoot) {
          File file = sdRoot.openNextFile();
          while (file) {
            s.printf("  %s %-20s (%u bytes)\n", file.isDirectory() ? "[DIR] " : "[FILE]", file.name(), file.size());
            file = sdRoot.openNextFile();
          }
        }
      }
      s.println("=== [End of List] ===\n");
    } else if (cmd.startsWith("cat ")) {
      String p = cmd.substring(4);
      p.trim();
      File f = FFat.open(p.c_str(), FILE_READ);
      if (!f) {
        s.printf("cat: '%s' cannot open\n", p.c_str());
      } else {
        s.printf("--- Content of %s ---\n", p.c_str());
        while (f.available()) {
          s.write(f.read());
        }
        f.close();
        s.println("\n--- End of file ---");
      }
    } else if (cmd.startsWith("mkdir ")) {
      String p = cmd.substring(6);
      p.trim();
      errno = 0;
      bool ok = FFat.mkdir(p.c_str());
      s.printf("mkdir '%s' -> %s (errno=%d: %s)\n", p.c_str(), ok ? "OK" : "FAIL", errno, strerror(errno));
      s.printf("FFat total=%llu, used=%llu, free=%llu\n", (uint64_t)FFat.totalBytes(), (uint64_t)FFat.usedBytes(), (uint64_t)FFat.freeBytes());
    } else if (cmd.startsWith("rm ")) {
      String p = cmd.substring(3);
      p.trim();
      bool ok = FFat.remove(p.c_str()) || FFat.rmdir(p.c_str());
      s.printf("rm '%s' -> %s\n", p.c_str(), ok ? "OK" : "FAIL");
    } else if (cmd == "format") {
      s.println("Formatting FFat...");
      FFat.end();
      bool fok = FFat.format();
      s.printf("FFat.format() -> %s\n", fok ? "OK" : "FAIL");
      FFat.begin(true, "", 10, "ffat");
      s.printf("FFat total=%llu, free=%llu\n", (uint64_t)FFat.totalBytes(), (uint64_t)FFat.freeBytes());
      s.println("Restarting in 2s...");
      delay(2000);
      ESP.restart();
    } else if (cmd == "i2c" || cmd == "i2c scan" || cmd == "i2cdetect") {
      s.println("\n=== [I2C Scanner - Main Bus (SDA:8, SCL:9)] ===");
      Wire.begin(PIN_SDA, PIN_SCL);
      Wire.setClock(100000);
      int found = 0;
      s.println("     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f");
      for (uint8_t row = 0; row < 128; row += 16) {
        s.printf("%02x: ", row);
        for (uint8_t col = 0; col < 16; col++) {
          uint8_t addr = row + col;
          if (addr < 3 || addr > 0x77) {
            s.print("   ");
            continue;
          }
          Wire.beginTransmission(addr);
          if (Wire.endTransmission() == 0) {
            s.printf("%02x ", addr);
            found++;
          } else {
            s.print("-- ");
          }
        }
        s.println();
      }
      Wire.setClock(I2C_CLOCK);
      s.printf("=== Devices found on Main bus: %d ===\n\n", found);
    } else if (cmd == "screenshot" || cmd == "sc") {
      String scPath;
      if (screenshot::capture(scPath)) {
        s.printf("Screenshot captured: %s\n", scPath.c_str());
      } else {
        s.println("Screenshot capture failed");
      }
    } else if (cmd == "freq") {
      s.printf("Current CPU Frequency: %u MHz\n", (unsigned)power_mgr::getFrequencyMhz());
    } else if (cmd == "charge auto" || cmd == "chg auto") {
      battery::setTestCharging(-1);
      s.println("Simulated charging: AUTO (hardware pin)");
    } else if (cmd.startsWith("charge ") || cmd.startsWith("chg ")) {
      String val = cmd.substring(cmd.indexOf(' ') + 1);
      val.trim();
      bool en = (val == "1" || val == "on" || val == "true");
      battery::setTestCharging(en ? 1 : 0);
      s.printf("Simulated charging: %s\n", en ? "ON" : "OFF");
    } else if (cmd.startsWith("bat ")) {
      String val = cmd.substring(4);
      val.trim();
      int p = val.toInt();
      battery::setTestPercentage((int16_t)p);
      if (p < 0) {
        s.println("Simulated battery: AUTO (hardware ADC)");
      } else {
        s.printf("Simulated battery percent: %d%%\n", p);
      }
    }
  };

  handleCli(Serial);
  handleCli(Serial0);

  if (usb_remote::isStreaming()) {
    lastActivity = now;
  }


  uint16_t scroff = settings_data::getScreenOffSec();
  if (state != ST_SPLASH && scroff > 0 && !display::isSleeping() && !screensaver::isActive() && !charging_view::isActive() && (now - lastActivity >= scroff * 1000UL)) {
    uint8_t saver = settings_data::getScreensaver();
    if (saver == 0) {
      display::setPowerSave(true);
    } else {
      screensaver::start(saver);
      dirty = true;
    }
  }

  uint16_t sleepTimeout = settings_data::getSleepSec();
  if (sleepTimeout > 0 && !charging_view::isCharging() && (now - lastActivity >= sleepTimeout * 1000UL)) {
    settings_data::powerOff();
  }

  KeyEvent ev;
  while (buttons::poll(ev)) {
    if (charging_view::isActive()) {
      charging_view::dismiss();
      lastActivity = now;
      dirty = true;
      continue;
    }
    if (display::isSleeping() || screensaver::isActive()) {
      if (display::isSleeping()) {
        display::setPowerSave(false);
      }
      if (screensaver::isActive()) {
        screensaver::stop();
      }
      lastActivity = now;
      dirty = true;
      continue;
    }
    lastActivity = now;
    power_mgr::notifyActivity();

    if (ui_keyboard::isModal()) {
      ui_keyboard::onKey(ev);
      dirty = true;
      continue;
    }

    if (state == ST_DISCLAIMER) {
      disclaimer::onKey(ev);
    } else if (state == ST_PAIRING) {
      if ((ev.key == BTN_OK || ev.key == BTN_BACK) && !ev.repeat) {
        state = ST_HOME;
        power_mgr::setProfile(power_mgr::CpuProfile::IDLE_ECO);
        Serial.println("[pairing] Skipped by user -> to scenes");
      }
    } else if (state == ST_HOME) {
      if (ev.key == BTN_OK && !ev.repeat) {
        state = ST_MENU;
        power_mgr::setProfile(power_mgr::CpuProfile::BALANCED);
        menu.reset();
        Serial.println("[home] Open menu");
      } else {
        home_scenes::onKey(ev);
      }
    } else if (state == ST_MENU) {
      if (ev.key == BTN_OK && !ev.repeat) {
        activeApp = menu.selectedApp();
        if (activeApp) {
          activeApp->open();
          state = ST_APP;
          power_mgr::setProfile(power_mgr::CpuProfile::HIGH_PERF);
          Serial.printf("[open] %s\n", activeApp->name());
        }
      } else if (ev.key == BTN_BACK) {
        state = ST_HOME;
        power_mgr::setProfile(power_mgr::CpuProfile::IDLE_ECO);
        Serial.println("[menu] Back to scenes");
      } else {
        menu.onKey(ev);
      }
    } else if (state == ST_APP && activeApp) {
      if (ev.key == BTN_BACK && !ev.repeat) {
        if (!activeApp->onBack()) {
          activeApp->close();
          activeApp = nullptr;
          state = ST_MENU;
          power_mgr::setProfile(power_mgr::CpuProfile::BALANCED);
          Serial.println("[back]");
        }
      } else {
        activeApp->onKey(ev);
      }
    }
    dirty = true;
  }


  if (state == ST_APP && activeApp) {
    activeApp->tick(now);
  }


  if (ble_service::checkAndClearNewClient()) {
    dirty = true;
  }


  bool animating = false;
  bool wasChargingActive = charging_view::isActive();
  charging_view::tick(now);
  if (charging_view::isActive()) {
    animating = true;
  } else if (wasChargingActive) {
    dirty = true;
  } else if (screensaver::isActive()) {
    animating = screensaver::tick(now);
  } else if (ui_keyboard::isModal()) {
    ui_keyboard::tick(now);
    animating = true;
  } else {
    switch (state) {
      case ST_SPLASH:
        if (splash::finished(now)) {
          if (!disclaimer::isAccepted()) {
            state = ST_DISCLAIMER;
            dirty = true;
            Serial.println("[boot] First boot disclaimer required");
          } else if (ble_service::isPaired()) {
            state = ST_HOME;
            dirty = true;
            Serial.println("[boot] Already paired! Straight to scenes.");
          } else {
            state = ST_PAIRING;
            dirty = true;
            Serial.println("[pairing] Waiting for initial phone pairing.");
          }
        } else {
          animating = true;
        }
        break;

      case ST_DISCLAIMER:
        animating = disclaimer::tick(now);
        if (disclaimer::isAccepted()) {
          if (ble_service::isPaired()) {
            state = ST_HOME;
          } else {
            state = ST_PAIRING;
          }
          dirty = true;
          Serial.println("[disclaimer] Accepted! Proceeding to next state.");
        }
        break;

      case ST_PAIRING:
        if (ble_service::isAuthenticated()) {
          state = ST_HOME;
          dirty = true;
          Serial.println("[pairing] Connected & authenticated by phone! Opening scenes");
        } else {
          animating = true;
        }
        break;

      case ST_HOME:
        home_scenes::tick(now);
        animating = true;
        break;

      case ST_MENU:
        animating = menu.tick();
        break;

      case ST_APP:
        if (activeApp && activeApp->animates()) {
          animating = true;
        }
        break;
    }
  }


  static uint32_t lastBleStream = 0;
  static uint32_t lastUsbStream = 0;
  bool bleKeepAlive = ble_service::isConnected() && ble_service::isAuthenticated() && (now - lastBleStream >= 120);
  bool usbKeepAlive = usb_remote::isStreaming() && (now - lastUsbStream >= 16);


  if (!display::isSleeping() && (dirty || animating || bleKeepAlive || usbKeepAlive) && now - lastDraw >= 16) {
    drawFrame(now);
    lastDraw = now;
    dirty = false;
    if (bleKeepAlive) lastBleStream = now;
    if (usbKeepAlive) lastUsbStream = now;
  }
}
