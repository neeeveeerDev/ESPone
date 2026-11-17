#pragma once
#include <Arduino.h>
#include <NimBLEServer.h>

namespace ble_hid {

void init(NimBLEServer* pServer);
bool isConnected();


void sendKey(uint8_t modifier, uint8_t keycode);
void sendChar(char c);


void sendConsumer(uint16_t consumerCode);


void sendMouse(uint8_t buttons, int8_t x, int8_t y, int8_t wheel = 0);
void mouseMove(int8_t x, int8_t y);
void mouseClick(uint8_t buttons);
void mouseScroll(int8_t wheel);

static const uint8_t MOUSE_BUTTON_LEFT   = 0x01;
static const uint8_t MOUSE_BUTTON_RIGHT  = 0x02;
static const uint8_t MOUSE_BUTTON_MIDDLE = 0x04;


static const uint16_t MEDIA_VOL_UP    = 0x00E9;
static const uint16_t MEDIA_VOL_DOWN  = 0x00EA;
static const uint16_t MEDIA_MUTE      = 0x00E2;
static const uint16_t MEDIA_PLAY_PAUSE= 0x00CD;
static const uint16_t MEDIA_STOP      = 0x00B7;
static const uint16_t MEDIA_NEXT      = 0x00B5;
static const uint16_t MEDIA_PREV      = 0x00B6;
static const uint16_t MEDIA_BRIGHT_UP = 0x006F;
static const uint16_t MEDIA_BRIGHT_DOWN = 0x0070;
static const uint16_t MEDIA_SNAPSHOT  = 0x00B8;
static const uint16_t MEDIA_HOME      = 0x0223;
static const uint16_t MEDIA_BACK      = 0x0224;
static const uint16_t MEDIA_VOICE_ASST= 0x0221;
static const uint16_t MEDIA_POWER     = 0x0030;


static const uint8_t KEY_ENTER        = 0x28;
static const uint8_t KEY_ESC          = 0x29;
static const uint8_t KEY_BACKSPACE    = 0x2A;
static const uint8_t KEY_TAB          = 0x2B;
static const uint8_t KEY_SPACE        = 0x2C;
static const uint8_t KEY_RIGHT_ARROW  = 0x4F;
static const uint8_t KEY_LEFT_ARROW   = 0x50;
static const uint8_t KEY_DOWN_ARROW   = 0x51;
static const uint8_t KEY_UP_ARROW     = 0x52;
static const uint8_t KEY_F5           = 0x3E;
static const uint8_t KEY_PRINTSCREEN  = 0x46;

}
