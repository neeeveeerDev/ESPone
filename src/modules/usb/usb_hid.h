#pragma once
#include <Arduino.h>
#include <USB.h>
#include <USBHIDKeyboard.h>
#include <USBHIDMouse.h>

namespace usb_hid {

void init();
USBHIDKeyboard& getKeyboard();
USBHIDMouse& getMouse();
bool isConnected();

}
