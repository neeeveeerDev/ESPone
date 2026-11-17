#pragma once

// Дисплей
#define OLED_ADDR       0x3C
#define PIN_SDA         8
#define PIN_SCL         9
#define I2C_CLOCK       800000

// Кнопки
#define PIN_BTN_UP      1
#define PIN_BTN_DOWN    2
#define PIN_BTN_LEFT    3
#define PIN_BTN_RIGHT   4
#define PIN_BTN_OK      5
#define PIN_BTN_BACK    6

#define BTN_DEBOUNCE_MS     25
#define BTN_REPEAT_DELAY_MS 400
#define BTN_REPEAT_MS       140

#define SPLASH_MS       1000

// Внешний разъём GPIO
#define DEFAULT_PIN_A    10
#define DEFAULT_PIN_B    11
#define DEFAULT_PIN_C    12
#define DEFAULT_PIN_D    13
#define DEFAULT_PIN_E    14
#define DEFAULT_PIN_F    15
#define DEFAULT_PIN_G    16

#define PIN_IBUTTON      15

// UART
#define UART_BRIDGE_PIN_TX   43
#define UART_BRIDGE_PIN_RX   44

// IR
#define PIN_IR_TX            17
#define PIN_IR_RX            41

// SD (SPI)
#define PIN_SD_CS            10
#define PIN_SD_MOSI          11
#define PIN_SD_SCK           12
#define PIN_SD_MISO          13

// CC1101 (SPI)
#define PIN_CC1101_CS        7
#define PIN_CC1101_MOSI      11
#define PIN_CC1101_SCK       12
#define PIN_CC1101_MISO      13
#define PIN_CC1101_GDO0      14
#define PIN_CC1101_GDO2      16

// Battery
#define PIN_BATTERY_ADC      18
#define PIN_BATTERY_CHRG     21
