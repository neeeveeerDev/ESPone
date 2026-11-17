#include "gpio.h"
#include "gpio_pins.h"
#include "usb_uart.h"
#include "pin_config_view.h"
#include "ibutton.h"
#include "scene_i2c_scanner.h"

namespace {
static const char* kMenuItems[] = {
  "iButton",
  "USB-UART Bridge",
  "GPIO Mapping",
  "I2C Scanner"
};
static const int kMenuCount = sizeof(kMenuItems) / sizeof(kMenuItems[0]);
static const int rowH = 12;
static const int startY = 16;
}

void GpioApp::open() {
  gpio_pins::init();
  scene_ = SCENE_MENU;
  menuSel_ = 0;
  barY_ = startY;
  Serial.println("[gpio] open");
}

void GpioApp::close() {
  if (scene_ == SCENE_IBUTTON) {
    ibutton::close();
  } else if (scene_ == SCENE_USB_UART) {
    usb_uart::close();
  } else if (scene_ == SCENE_PIN_CONFIG) {
    pin_config_view::close();
  } else if (scene_ == SCENE_I2C_SCANNER) {
    scene_i2c_scanner::close();
  }
  scene_ = SCENE_MENU;
}

bool GpioApp::onBack() {
  if (scene_ != SCENE_MENU) {
    if (scene_ == SCENE_IBUTTON) {
      if (ibutton::onBack()) return true;
      ibutton::close();
    } else if (scene_ == SCENE_USB_UART) {
      usb_uart::close();
    } else if (scene_ == SCENE_PIN_CONFIG) {
      pin_config_view::close();
    } else if (scene_ == SCENE_I2C_SCANNER) {
      if (scene_i2c_scanner::onBack()) return true;
      scene_i2c_scanner::close();
    }

    scene_ = SCENE_MENU;
    barY_ = startY + menuSel_ * rowH;
    return true;
  }
  return false;
}

void GpioApp::tick(uint32_t now) {
  if (scene_ == SCENE_MENU) {
    float targetY = startY + menuSel_ * rowH;
    float dy = targetY - barY_;
    if (fabsf(dy) > 0.1f) {
      barY_ += dy * 0.35f;
    } else {
      barY_ = targetY;
    }
  } else if (scene_ == SCENE_IBUTTON) {
    ibutton::tick(now);
  } else if (scene_ == SCENE_USB_UART) {
    usb_uart::tick(now);
  } else if (scene_ == SCENE_PIN_CONFIG) {
    pin_config_view::tick(now);
  } else if (scene_ == SCENE_I2C_SCANNER) {
    scene_i2c_scanner::tick(now);
  }
}

void GpioApp::onKey(const KeyEvent& ev) {
  if (scene_ == SCENE_MENU) {
    if (ev.key == BTN_UP) {
      menuSel_ = (menuSel_ - 1 + kMenuCount) % kMenuCount;
    } else if (ev.key == BTN_DOWN) {
      menuSel_ = (menuSel_ + 1) % kMenuCount;
    } else if (ev.key == BTN_OK && !ev.repeat) {
      if (menuSel_ == 0) {
        scene_ = SCENE_IBUTTON;
        ibutton::open();
      } else if (menuSel_ == 1) {
        scene_ = SCENE_USB_UART;
        usb_uart::open();
      } else if (menuSel_ == 2) {
        scene_ = SCENE_PIN_CONFIG;
        pin_config_view::open();
      } else if (menuSel_ == 3) {
        scene_ = SCENE_I2C_SCANNER;
        scene_i2c_scanner::open();
      }
    }
  } else if (scene_ == SCENE_IBUTTON) {
    ibutton::onKey(ev);
  } else if (scene_ == SCENE_USB_UART) {
    usb_uart::onKey(ev);
  } else if (scene_ == SCENE_PIN_CONFIG) {
    pin_config_view::onKey(ev);
  } else if (scene_ == SCENE_I2C_SCANNER) {
    scene_i2c_scanner::onKey(ev);
  }
}

void GpioApp::draw(U8G2& u8g2, uint32_t now) {
  if (scene_ == SCENE_MENU) {

    u8g2.setFont(u8g2_font_6x12_tr);
    u8g2.setDrawColor(1);
    for (int i = 0; i < kMenuCount; ++i) {
      u8g2.drawStr(6, startY + i * rowH + 10, kMenuItems[i]);
    }


    u8g2.setDrawColor(2);
    u8g2.drawBox(2, (int)(barY_ + 0.5f), 124, rowH);
    u8g2.setDrawColor(1);


    u8g2.setDrawColor(0);
    u8g2.drawBox(0, 0, 128, 14);
    u8g2.setDrawColor(1);
    u8g2.drawBox(0, 0, 128, 14);
    u8g2.setDrawColor(0);
    u8g2.setFont(u8g2_font_7x14B_tr);
    u8g2.drawStr(4, 11, "GPIO");
    u8g2.setDrawColor(1);
  } else if (scene_ == SCENE_IBUTTON) {
    ibutton::draw(u8g2, now);
  } else if (scene_ == SCENE_USB_UART) {
    usb_uart::draw(u8g2, now);
  } else if (scene_ == SCENE_PIN_CONFIG) {
    pin_config_view::draw(u8g2, now);
  } else if (scene_ == SCENE_I2C_SCANNER) {
    scene_i2c_scanner::draw(u8g2, now);
  }
}

