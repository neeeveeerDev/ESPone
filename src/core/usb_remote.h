#pragma once
#include <Arduino.h>

namespace usb_remote {

void init();
void tick();
void setEnabled(bool enabled);
bool isEnabled();
bool isStreaming();
void sendScreenFrame(const uint8_t* buffer, size_t length);

}
