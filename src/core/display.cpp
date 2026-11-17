#include "display.h"
#include <Arduino.h>
#include <Wire.h>
#include "config.h"



static U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);


static bool s_sleeping = false;

namespace display {

void init() {

  Wire.begin(PIN_SDA, PIN_SCL);
  Wire.setClock(I2C_CLOCK);
  uint8_t addr = 0;
  for (uint8_t a : {0x3C, 0x3D}) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) { addr = a; break; }
  }
  if (addr == 0) {
    Serial.println("[OLED] not found on I2C! Check wiring/pins in config.h");
  } else {
    Serial.printf("[OLED] found at 0x%02X\n", addr);
  }

  u8g2.setI2CAddress((addr ? addr : 0x3C) << 1);
  u8g2.begin();
  u8g2.setBusClock(I2C_CLOCK);
  u8g2.clearBuffer();
  u8g2.sendBuffer();
}

U8G2& get() { return u8g2; }

void setContrast(uint8_t contrast) {
  u8g2.setContrast(contrast);
}

void setInvert(bool invert) {
  u8g2.sendF("c", invert ? 0xA7 : 0xA6);
}

void setPowerSave(bool sleep) {
  s_sleeping = sleep;
  u8g2.setPowerSave(sleep ? 1 : 0);
}

bool isSleeping() {
  return s_sleeping;
}

}
