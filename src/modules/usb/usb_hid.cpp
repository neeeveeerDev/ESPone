#include "usb_hid.h"

namespace usb_hid {

static USBHIDKeyboard s_keyboard;
static USBHIDMouse s_mouse;
static bool s_inited = false;

void init() {
  if (s_inited) return;
  s_inited = true;

  s_mouse.begin();
  s_keyboard.begin();
  USB.begin();
  Serial.println("[usb_hid] Native USB HID initialized");
}

USBHIDKeyboard& getKeyboard() {
  if (!s_inited) init();
  return s_keyboard;
}

USBHIDMouse& getMouse() {
  if (!s_inited) init();
  return s_mouse;
}

bool isConnected() {
  return true;
}

}
