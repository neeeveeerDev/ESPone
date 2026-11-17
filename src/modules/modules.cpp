#include "modules.h"
#include "wifi/wifi.h"
#include "bluetooth/bluetooth.h"
#include "infrared/infrared.h"
#include "subghz/subghz.h"
#include "usb/usb.h"
#include "gpio/gpio.h"
#include "apps/apps.h"
#include "settings/settings.h"
#include "settings/settings_data.h"
#include "ui/menu.h"

namespace {
WifiApp wifiApp;
BluetoothApp bluetoothApp;
InfraredApp infraredApp;
SubghzApp subghzApp;
UsbApp usbApp;
GpioApp gpioApp;
AppsApp appsApp;
SettingsApp settingsApp;
}

namespace modules {


App* const kApps[] = {
    &wifiApp,
    &bluetoothApp,
    &infraredApp,
    &subghzApp,
    &usbApp,
    &gpioApp,
    &appsApp,
    &settingsApp,
};
const int kCount = sizeof(kApps) / sizeof(kApps[0]);

static MenuView* s_menuPtr = nullptr;
static App* s_activeApps[8];

void initMenu(MenuView& menu) {
  s_menuPtr = &menu;
  reloadMenu();
}

void reloadMenu() {
  if (!s_menuPtr) return;
  uint8_t order[8];
  settings_data::getMenuOrder(order, kCount);
  int activeCount = 0;
  for (int i = 0; i < kCount; i++) {
    uint8_t id = order[i];
    if (id < kCount && !settings_data::isAppHidden(id)) {
      s_activeApps[activeCount++] = kApps[id];
    }
  }

  if (activeCount == 0) {
    s_activeApps[0] = kApps[7];
    activeCount = 1;
  }
  s_menuPtr->init(s_activeApps, activeCount);
}

}
