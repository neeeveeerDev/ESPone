#pragma once
#include <Arduino.h>

namespace settings_data {

void init();
void save();
void apply();


uint8_t getBrightness();
void setBrightness(uint8_t val);

bool getInvert();
void setInvert(bool inv);

uint8_t getScreensaver();
void setScreensaver(uint8_t mode);


uint16_t getScreenOffSec();
void setScreenOffSec(uint16_t sec);

uint16_t getSleepSec();
void setSleepSec(uint16_t sec);

void powerOff();


bool getBleEnabled();
void setBleEnabled(bool en);

bool getUsbEnabled();
void setUsbEnabled(bool en);

bool getBleSync();
void setBleSync(bool en);

String getBleName();
void setBleName(const String& name);

void resetBlePin();
void resetGpioPins();
void factoryReset();
void reboot();


void getMenuOrder(uint8_t* outOrder, uint8_t count);
void setMenuOrder(const uint8_t* inOrder, uint8_t count);
bool isAppHidden(uint8_t appId);
void setAppHidden(uint8_t appId, bool hidden);
void toggleAppHidden(uint8_t appId);
uint16_t getMenuHiddenMask();
void setMenuHiddenMask(uint16_t mask);

}
