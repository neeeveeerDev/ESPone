#include "scene_i2c_scanner.h"
#include "config.h"
#include "gpio_pins.h"
#include <Wire.h>
#include <vector>

namespace scene_i2c_scanner {

struct I2CDeviceInfo {
  uint8_t addr;
  const char* name;
};

struct FoundDevice {
  uint8_t addr;
  String name;
};

static const I2CDeviceInfo kKnownList[] = {
  {0x18, "LIS3DH / MCP9808"},
  {0x1C, "LIS3MDL / MMA8452Q"},
  {0x1D, "ADXL345 / MMA8451Q"},
  {0x1E, "HMC5883L Compass"},
  {0x20, "PCF8574 IO Expander"},
  {0x21, "PCF8574 IO Expander"},
  {0x22, "PCF8574 IO Expander"},
  {0x23, "BH1750 Light Sensor"},
  {0x24, "PCF8574 IO Expander"},
  {0x25, "PCF8574 IO Expander"},
  {0x26, "PCF8574 IO Expander"},
  {0x27, "PCF8574 / 1602 LCD"},
  {0x38, "AHT10 / AHT20 Sensor"},
  {0x39, "APDS9960 / TSL2561"},
  {0x3C, "SSD1306 / SH1106 OLED"},
  {0x3D, "SSD1306 OLED (Alt)"},
  {0x40, "INA219 / PCA9685 / SI7021"},
  {0x44, "SHT30 / SHT31 Temp"},
  {0x48, "ADS1115 / TMP102 / PCF8591"},
  {0x49, "ADS1115 (Alt) / TMP102"},
  {0x4A, "ADS1115 ADC (Alt)"},
  {0x50, "AT24C32/64 EEPROM"},
  {0x51, "AT24Cxx / PCF8563 RTC"},
  {0x52, "AT24Cxx / Nunchuk"},
  {0x53, "ADXL345 / AT24Cxx"},
  {0x57, "AT24C32 / MAX30100"},
  {0x5A, "MLX90614 IR Temp"},
  {0x60, "TEA5767 / MCP4725"},
  {0x68, "MPU6050 / DS3231 RTC"},
  {0x69, "MPU6050 (Alt)"},
  {0x76, "BMP280 / BME280 / MS5611"},
  {0x77, "BMP280 / BME280 (Alt)"}
};
static const size_t kKnownCount = sizeof(kKnownList) / sizeof(kKnownList[0]);

static const char* lookupName(uint8_t addr) {
  for (size_t i = 0; i < kKnownCount; i++) {
    if (kKnownList[i].addr == addr) {
      return kKnownList[i].name;
    }
  }
  return "Unknown Device";
}

static int s_busIndex = 0;
static std::vector<FoundDevice> s_devices;
static int s_sel = 0;
static float s_scroll = 0.0f;
static float s_targetScroll = 0.0f;
static bool s_scanning = false;

static void performScan() {
  s_scanning = true;
  s_devices.clear();

  TwoWire* wire = (s_busIndex == 0) ? &Wire : &Wire1;

  if (s_busIndex == 0) {
    wire->begin(PIN_SDA, PIN_SCL);
    wire->setClock(100000);
  } else {
    int8_t pinSDA = gpio_pins::getPin(gpio_pins::PIN_IDX_F);
    int8_t pinSCL = gpio_pins::getPin(gpio_pins::PIN_IDX_G);
    wire->begin(pinSDA, pinSCL);
    wire->setClock(100000);
  }

  Serial.printf("[i2c] Starting scan on Bus %d...\n", s_busIndex);

  for (uint8_t addr = 1; addr < 127; addr++) {
    wire->beginTransmission(addr);
    uint8_t err = wire->endTransmission();

    if (err == 0) {
      FoundDevice dev;
      dev.addr = addr;
      dev.name = lookupName(addr);
      s_devices.push_back(dev);
      Serial.printf("[i2c] Found device at 0x%02X: %s\n", addr, dev.name.c_str());
    }
  }

  if (s_busIndex == 0) {
    Wire.setClock(I2C_CLOCK);
  }

  Serial.printf("[i2c] Scan finished: %d devices found\n", (int)s_devices.size());

  s_sel = 0;
  s_scroll = 0.0f;
  s_targetScroll = 0.0f;
  s_scanning = false;
}

void open() {
  s_busIndex = 0;
  performScan();
}

void close() {
  s_devices.clear();
}

bool onBack() {
  return false;
}

void onKey(const KeyEvent& ev) {
  if (ev.repeat && ev.key != BTN_UP && ev.key != BTN_DOWN) return;

  if (ev.key == BTN_LEFT || ev.key == BTN_RIGHT) {
    s_busIndex = (s_busIndex == 0) ? 1 : 0;
    performScan();
  } else if (ev.key == BTN_OK && !ev.repeat) {
    performScan();
  } else if (ev.key == BTN_UP) {
    if (s_sel > 0) s_sel--;
    else if (!s_devices.empty()) s_sel = (int)s_devices.size() - 1;
  } else if (ev.key == BTN_DOWN) {
    if (!s_devices.empty()) {
      if (s_sel < (int)s_devices.size() - 1) s_sel++;
      else s_sel = 0;
    }
  }
}

void tick(uint32_t now) {
  (void)now;
  const int rowH = 12;
  const int maxVisible = 3;

  if (!s_devices.empty()) {
    int maxScroll = (int)s_devices.size() - maxVisible;
    if (maxScroll < 0) maxScroll = 0;

    int topIdx = (int)(s_targetScroll / rowH);
    if (s_sel < topIdx) {
      s_targetScroll = s_sel * rowH;
    } else if (s_sel >= topIdx + maxVisible) {
      s_targetScroll = (s_sel - maxVisible + 1) * rowH;
    }

    if (s_targetScroll < 0) s_targetScroll = 0;
    if (s_targetScroll > maxScroll * rowH) s_targetScroll = maxScroll * rowH;

    float ds = s_targetScroll - s_scroll;
    if (fabsf(ds) > 0.1f) s_scroll += ds * 0.35f;
    else s_scroll = s_targetScroll;
  }
}

void draw(U8G2& u8g2, uint32_t now) {
  (void)now;


  u8g2.setDrawColor(0);
  u8g2.drawBox(0, 0, 128, 14);
  u8g2.setDrawColor(1);
  u8g2.drawBox(0, 0, 128, 14);
  u8g2.setDrawColor(0);
  u8g2.setFont(u8g2_font_7x14B_tr);
  u8g2.drawStr(4, 11, "I2C SCANNER");
  u8g2.setDrawColor(1);


  u8g2.setFont(u8g2_font_5x8_tr);
  char busStr[32];
  if (s_busIndex == 0) {
    snprintf(busStr, sizeof(busStr), "< Bus: Main (8/9) >");
  } else {
    int8_t pinSDA = gpio_pins::getPin(gpio_pins::PIN_IDX_F);
    int8_t pinSCL = gpio_pins::getPin(gpio_pins::PIN_IDX_G);
    snprintf(busStr, sizeof(busStr), "< Bus: Ext (%d/%d) >", pinSDA, pinSCL);
  }
  int bw = u8g2.getStrWidth(busStr);
  u8g2.drawStr((128 - bw) / 2, 23, busStr);


  u8g2.drawHLine(4, 25, 120);


  const int startY = 27;
  const int rowH = 12;

  if (s_devices.empty()) {
    u8g2.setFont(u8g2_font_6x12_tr);
    const char* m1 = "No devices found";
    u8g2.drawStr((128 - u8g2.getStrWidth(m1)) / 2, 45, m1);
  } else {
    u8g2.setFont(u8g2_font_5x8_tr);
    for (size_t i = 0; i < s_devices.size(); i++) {
      int y = startY + (int)(i * rowH - s_scroll + 0.5f);
      if (y < 25 || y > 62) continue;

      char itemBuf[40];
      snprintf(itemBuf, sizeof(itemBuf), "0x%02X %s", s_devices[i].addr, s_devices[i].name.c_str());
      u8g2.drawStr(6, y + 9, itemBuf);

      if ((int)i == s_sel) {
        u8g2.setDrawColor(2);
        u8g2.drawBox(4, y, 120, rowH);
        u8g2.setDrawColor(1);
      }
    }
  }
}

}
