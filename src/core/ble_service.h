#pragma once
#include <Arduino.h>
#include <stdint.h>

namespace ble_service {


void init();


void tick();


bool isConnected();
bool isAuthenticated();
bool isPaired();
bool checkAndClearNewClient();


const char* getPin();


const char* getDeviceName();
void setDeviceName(const char* name);


void resetPin();
void setEnabled(bool enabled);
bool isEnabled();


void pauseAdvertising();
void resumeAdvertising();
void setHidMode(bool active);
bool isHidMode();
void setSuspended(bool suspended);
bool isSuspended();
void* getServer();


void sendScreen(const uint8_t* buffer, size_t length);

}
