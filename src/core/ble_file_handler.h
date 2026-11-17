#pragma once
#include <Arduino.h>
#include <NimBLECharacteristic.h>

namespace ble_file_handler {

void init(NimBLECharacteristic* pChar);
void onPacketReceived(NimBLECharacteristic* pChar, const uint8_t* data, size_t length);

}
