#include "usb_app.h"
#include "usb_hid.h"
#include "core/sys_storage.h"
#include "scene_badusb.h"
#include "scene_mouse_jiggler.h"
#include "scene_usb_mouse.h"
#include <math.h>

namespace {
static const char* kMenuItems[] = {
  "BadUSB",
  "Mouse Jiggler",
  "USB Mouse"
};
static const int kMenuCount = sizeof(kMenuItems) / sizeof(kMenuItems[0]);
static const int rowH = 12;
static const int startY = 16;
}

void UsbApp::open() {
  sys_storage::init();
  scene_ = SCENE_MENU;
  menuSel_ = 0;
  barY_ = startY;
  scroll_ = 0.0f;
  targetScroll_ = 0.0f;
  Serial.println("[usb] open");
}

void UsbApp::close() {
  if (scene_ == SCENE_BADUSB) scene_badusb::close();
  else if (scene_ == SCENE_MOUSE_JIGGLER) scene_mouse_jiggler::close();
  else if (scene_ == SCENE_USB_MOUSE) scene_usb_mouse::close();
  scene_ = SCENE_MENU;
}

bool UsbApp::onBack() {
  if (scene_ == SCENE_BADUSB) {
    if (scene_badusb::onBack()) return true;
  }

  if (scene_ != SCENE_MENU) {
    if (scene_ == SCENE_BADUSB) scene_badusb::close();
    else if (scene_ == SCENE_MOUSE_JIGGLER) scene_mouse_jiggler::close();
    else if (scene_ == SCENE_USB_MOUSE) scene_usb_mouse::close();

    scene_ = SCENE_MENU;
    return true;
  }

  return false;
}

void UsbApp::onKey(const KeyEvent& ev) {
  if (scene_ == SCENE_MENU) {
    if (ev.key == BTN_UP) {
      menuSel_ = (menuSel_ - 1 + kMenuCount) % kMenuCount;
    } else if (ev.key == BTN_DOWN) {
      menuSel_ = (menuSel_ + 1) % kMenuCount;
    } else if (ev.key == BTN_OK && !ev.repeat) {
      switch (menuSel_) {
        case 0:
          scene_ = SCENE_BADUSB;
          usb_hid::init();
          scene_badusb::open();
          break;
        case 1:
          scene_ = SCENE_MOUSE_JIGGLER;
          usb_hid::init();
          scene_mouse_jiggler::open();
          break;
        case 2:
          scene_ = SCENE_USB_MOUSE;
          usb_hid::init();
          scene_usb_mouse::open();
          break;
      }
    }
  } else {
    switch (scene_) {
      case SCENE_BADUSB:        scene_badusb::onKey(ev); break;
      case SCENE_MOUSE_JIGGLER: scene_mouse_jiggler::onKey(ev); break;
      case SCENE_USB_MOUSE:     scene_usb_mouse::onKey(ev); break;
      default: break;
    }
  }
}

void UsbApp::tick(uint32_t now) {
  if (scene_ == SCENE_MENU) {
    int maxLines = 4;
    int maxScroll = kMenuCount - maxLines;
    if (maxScroll < 0) maxScroll = 0;

    int topIndex = (int)(targetScroll_ / rowH);
    if (menuSel_ < topIndex) targetScroll_ = menuSel_ * rowH;
    else if (menuSel_ >= topIndex + maxLines) targetScroll_ = (menuSel_ - maxLines + 1) * rowH;

    if (targetScroll_ < 0) targetScroll_ = 0;
    if (targetScroll_ > maxScroll * rowH) targetScroll_ = maxScroll * rowH;

    float ds = targetScroll_ - scroll_;
    if (fabsf(ds) > 0.1f) scroll_ += ds * 0.35f;
    else scroll_ = targetScroll_;

    float targetBarY = startY + menuSel_ * rowH - scroll_;
    float dy = targetBarY - barY_;
    if (fabsf(dy) > 0.1f) barY_ += dy * 0.4f;
    else barY_ = targetBarY;
  } else {
    switch (scene_) {
      case SCENE_BADUSB:        scene_badusb::tick(now); break;
      case SCENE_MOUSE_JIGGLER: scene_mouse_jiggler::tick(now); break;
      case SCENE_USB_MOUSE:     scene_usb_mouse::tick(now); break;
      default: break;
    }
  }
}

void UsbApp::draw(U8G2& u8g2, uint32_t now) {
  if (scene_ == SCENE_MENU) {

    u8g2.setFont(u8g2_font_6x12_tr);
    for (int i = 0; i < kMenuCount; i++) {
      int y = startY + (int)(i * rowH - scroll_ + 0.5f);
      if (y < 14 || y > 64) continue;
      u8g2.drawStr(6, y + 10, kMenuItems[i]);
    }


    int bY = (int)(barY_ + 0.5f);
    if (bY >= 14 && bY <= 64 - rowH) {
      u8g2.setDrawColor(2);
      u8g2.drawBox(2, bY, 124, rowH);
      u8g2.setDrawColor(1);
    }


    u8g2.setDrawColor(0);
    u8g2.drawBox(0, 0, 128, 14);
    u8g2.setDrawColor(1);
    u8g2.drawBox(0, 0, 128, 14);
    u8g2.setDrawColor(0);
    u8g2.setFont(u8g2_font_7x14B_tr);
    u8g2.drawStr(4, 11, "USB");
    u8g2.setDrawColor(1);
  } else {
    switch (scene_) {
      case SCENE_BADUSB:        scene_badusb::draw(u8g2, now); break;
      case SCENE_MOUSE_JIGGLER: scene_mouse_jiggler::draw(u8g2, now); break;
      case SCENE_USB_MOUSE:     scene_usb_mouse::draw(u8g2, now); break;
      default: break;
    }
  }
}
