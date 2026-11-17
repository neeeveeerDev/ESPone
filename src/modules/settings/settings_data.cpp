#include "settings_data.h"
#include <Preferences.h>
#include "core/display.h"
#include "core/ble_service.h"
#include "core/usb_remote.h"
#include "config.h"
#include "core/sys_storage.h"
#include "modules/gpio/gpio_pins.h"
#include <esp_sleep.h>

#include "core/sys_clock.h"

namespace settings_data {

static uint8_t s_brightness = 100;
static bool s_invert = false;
static uint8_t s_screensaver = 0;
static uint16_t s_screenOffSec = 30;
static uint16_t s_sleepSec = 0;
static bool s_bleEnabled = true;
static bool s_usbEnabled = true;
static bool s_bleSync = true;
static String s_bleName = "ESPone";
static uint8_t s_menuOrder[8] = { 0, 1, 2, 3, 4, 5, 6, 7 };
static uint16_t s_menuHiddenMask = 0;
static bool s_inited = false;

void init() {
  if (s_inited) return;

  sys_clock::init();


  Preferences prefs;
  prefs.begin("espone_set", true);
  s_brightness = prefs.getUChar("bright", 100);
  s_invert = prefs.getBool("invert", false);
  s_screensaver = prefs.getUChar("scrsave", 0);
  s_screenOffSec = prefs.getUShort("scroff", 30);
  s_sleepSec = prefs.getUShort("sleep", 0);
  s_bleEnabled = prefs.getBool("ble", true);
  s_usbEnabled = prefs.getBool("usb_en", true);
  s_bleSync = prefs.getBool("ble_sync", true);
  s_bleName = prefs.getString("ble_name", "ESPone");

  size_t rLen = prefs.getBytes("m_order", s_menuOrder, sizeof(s_menuOrder));
  if (rLen != sizeof(s_menuOrder)) {
    for (uint8_t i = 0; i < 8; i++) s_menuOrder[i] = i;
  }
  s_menuHiddenMask = prefs.getUShort("m_hidden", 0);
  s_menuHiddenMask &= ~(1 << 7);

  prefs.end();


  s_inited = true;


  sys_storage::init();

  apply();
}

void apply() {
  display::setContrast((s_brightness * 255) / 100);
  display::setInvert(s_invert);
  ble_service::setEnabled(s_bleEnabled);
  usb_remote::setEnabled(s_usbEnabled);
}

void save() {
  if (!s_inited) return;

  Preferences prefs;
  prefs.begin("espone_set", false);
  prefs.putUChar("bright", s_brightness);
  prefs.putBool("invert", s_invert);
  prefs.putUChar("scrsave", s_screensaver);
  prefs.putUShort("scroff", s_screenOffSec);
  prefs.putUShort("sleep", s_sleepSec);
  prefs.putBool("ble", s_bleEnabled);
  prefs.putBool("usb_en", s_usbEnabled);
  prefs.putBool("ble_sync", s_bleSync);
  prefs.putString("ble_name", s_bleName);
  prefs.putBytes("m_order", s_menuOrder, sizeof(s_menuOrder));
  prefs.putUShort("m_hidden", s_menuHiddenMask);
  prefs.end();
  apply();
  sys_storage::saveConfig();
  Serial.println("[settings] Saved to NVS and /system/sys.config");
}

uint8_t getBrightness() { return s_brightness; }
void setBrightness(uint8_t val) {
  s_brightness = val;
  display::setContrast((s_brightness * 255) / 100);
}

bool getInvert() { return s_invert; }
void setInvert(bool inv) {
  s_invert = inv;
  display::setInvert(s_invert);
}

uint8_t getScreensaver() { return s_screensaver; }
void setScreensaver(uint8_t mode) { s_screensaver = mode; }

uint16_t getScreenOffSec() { return s_screenOffSec; }
void setScreenOffSec(uint16_t sec) { s_screenOffSec = sec; }

uint16_t getSleepSec() { return s_sleepSec; }
void setSleepSec(uint16_t sec) { s_sleepSec = sec; }

void powerOff() {
  save();
  display::setPowerSave(true);
  delay(100);


  uint64_t mask = (1ULL << PIN_BTN_OK) | (1ULL << PIN_BTN_BACK) |
                  (1ULL << PIN_BTN_UP) | (1ULL << PIN_BTN_DOWN);
  esp_sleep_enable_ext1_wakeup(mask, ESP_EXT1_WAKEUP_ANY_LOW);
  Serial.println("[power] Entering deep sleep. Wake up on button press.");
  esp_deep_sleep_start();
}

bool getBleEnabled() { return s_bleEnabled; }
void setBleEnabled(bool en) {
  s_bleEnabled = en;
  ble_service::setEnabled(en);
  save();
}

bool getUsbEnabled() { return s_usbEnabled; }
void setUsbEnabled(bool en) {
  s_usbEnabled = en;
  usb_remote::setEnabled(en);
  save();
}

bool getBleSync() { return s_bleSync; }
void setBleSync(bool en) {
  s_bleSync = en;
  save();
}

String getBleName() { return s_bleName; }
void setBleName(const String& name) {
  if (name.length() > 0) {
    s_bleName = name;
    save();
  }
}

void resetBlePin() {
  ble_service::resetPin();
}

void resetGpioPins() {
  gpio_pins::resetDefaults();
}

void factoryReset() {
  Preferences p;
  p.begin("espone_set", false); p.clear(); p.end();
  p.begin("espone_gpio", false); p.clear(); p.end();
  p.begin("espone", false); p.clear(); p.end();
  Serial.println("[system] Factory reset complete. Restarting...");
  delay(200);
  ESP.restart();
}

void reboot() {
  Serial.println("[system] Rebooting...");
  delay(100);
  ESP.restart();
}

void getMenuOrder(uint8_t* outOrder, uint8_t count) {
  if (!outOrder) return;
  for (uint8_t i = 0; i < count; i++) {
    outOrder[i] = (i < 8) ? s_menuOrder[i] : i;
  }
}

void setMenuOrder(const uint8_t* inOrder, uint8_t count) {
  if (!inOrder) return;
  for (uint8_t i = 0; i < 8 && i < count; i++) {
    s_menuOrder[i] = inOrder[i];
  }
  save();
}

bool isAppHidden(uint8_t appId) {
  if (appId == 7) return false;
  return (s_menuHiddenMask & (1 << appId)) != 0;
}

void setAppHidden(uint8_t appId, bool hidden) {
  if (appId == 7) return;
  if (hidden) s_menuHiddenMask |= (1 << appId);
  else s_menuHiddenMask &= ~(1 << appId);
  save();
}

void toggleAppHidden(uint8_t appId) {
  if (appId == 7) return;
  s_menuHiddenMask ^= (1 << appId);
  save();
}

uint16_t getMenuHiddenMask() {
  return s_menuHiddenMask;
}

void setMenuHiddenMask(uint16_t mask) {
  s_menuHiddenMask = mask & ~(1 << 7);
  save();
}

}
